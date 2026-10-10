#include "order/OrderManager.h"
#include <cmath>
#include <limits>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

OrderManager::OrderManager(InventoryManager& inventoryManager)
    : inventoryManager(inventoryManager)
    , orders()
    , urgentQueue()
    , highQueue()
    , normalQueue()
    , lowQueue()
    , dispatchHeap()
    , nextSequenceNumber(1)
    , preMutationHook(nullptr)
{
}

// ---------------------------------------------------------------------------
// Priority Lane Resolver
// ---------------------------------------------------------------------------

Queue<std::string>& OrderManager::getQueueForPriority(OrderPriority priority)
{
    switch (priority) {
    case OrderPriority::Urgent:
        return urgentQueue;
    case OrderPriority::High:
        return highQueue;
    case OrderPriority::Normal:
        return normalQueue;
    case OrderPriority::Low:
        return lowQueue;
    }
    return normalQueue;
}

// ---------------------------------------------------------------------------
// Scheduler: Select next pending order ID in FIFO priority order
// ---------------------------------------------------------------------------

bool OrderManager::selectNextPendingOrderId(std::string& outOrderId)
{
    if (urgentQueue.dequeue(outOrderId)) return true;
    if (highQueue.dequeue(outOrderId))   return true;
    if (normalQueue.dequeue(outOrderId)) return true;
    if (lowQueue.dequeue(outOrderId))    return true;
    return false;
}

// ---------------------------------------------------------------------------
// Catalog Price Snapshot & Arithmetic Bounds Checking
// ---------------------------------------------------------------------------

bool OrderManager::snapshotCatalogPrices(Order& order)
{
    double calculatedTotal = 0.0;
    const auto& items = order.getItems();

    for (std::size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        if (item.productId.empty() || item.quantity <= 0) {
            return false;
        }

        Product* p = inventoryManager.findProduct(item.productId);
        if (p == nullptr) {
            return false; // SKU not found in inventory catalog
        }

        double price = p->getPrice();
        if (!std::isfinite(price) || price < 0.0) {
            return false; // Reject non-finite or negative catalog prices
        }

        order.setItemPriceSnapshot(i, price);

        double lineTotal = static_cast<double>(item.quantity) * price;
        if (!std::isfinite(lineTotal) || lineTotal < 0.0) {
            return false;
        }

        if (std::numeric_limits<double>::max() - calculatedTotal < lineTotal) {
            return false; // Arithmetic overflow in total price
        }

        calculatedTotal += lineTotal;
        if (!std::isfinite(calculatedTotal)) {
            return false;
        }
    }

    order.setTotalPrice(calculatedTotal);
    return true;
}

// ---------------------------------------------------------------------------
// Order Submission (Multi-structure atomic registration)
// ---------------------------------------------------------------------------

bool OrderManager::submitOrder(const Order& order)
{
    // 1. Structural validation
    if (order.getOrderId().empty() || order.getCustomerId().empty()) {
        return false;
    }
    if (order.getItems().empty()) {
        return false;
    }
    if (order.getStatus() != OrderStatus::Pending) {
        return false;
    }

    const std::string id = order.getOrderId();
    if (orders.find(id) != orders.end()) {
        return false; // Duplicate orderId rejected
    }

    // 2. Prepare working copy and snapshot catalog prices
    Order workingOrder = order;
    if (!snapshotCatalogPrices(workingOrder)) {
        return false;
    }

    // 3. Checked arithmetic for repeated SKU quantities
    std::unordered_map<std::string, std::int64_t> demandCheck;
    for (const auto& item : workingOrder.getItems()) {
        if (item.quantity <= 0) {
            return false;
        }
        if (demandCheck[item.productId] >
            static_cast<std::int64_t>(std::numeric_limits<int>::max()) - item.quantity) {
            return false; // Cumulative quantity exceeds 32-bit signed int max
        }
        demandCheck[item.productId] += item.quantity;
        if (demandCheck[item.productId] > std::numeric_limits<int>::max()) {
            return false;
        }
    }

    // 3b. Validate shipping inputs and calculate dynamic freight pricing
    const double dist = workingOrder.getShippingDistance();
    const double wt = workingOrder.getShippingWeight();
    const auto freightResult = pricingEngine.calculateFreight(dist, wt, workingOrder.getPriority());
    if (!freightResult.success) {
        return false; // Reject order if freight pricing fails
    }
    workingOrder.setFreightCost(freightResult.cost);

    // 4. Freeze order immutability
    workingOrder.freeze();

    // Check sequence counter boundary before assignment to prevent wrapping
    if (nextSequenceNumber == std::numeric_limits<std::uint64_t>::max()) {
        bool hasLiveOrders = false;
        for (const auto& [existingId, existingOrder] : orders) {
            const OrderStatus st = existingOrder.getStatus();
            if (st == OrderStatus::Pending || st == OrderStatus::Processing) {
                hasLiveOrders = true;
                break;
            }
        }
        if (hasLiveOrders) {
            // Refuse intake to protect deterministic priority ordering while live entries exist
            return false;
        }
        // No live pending orders remain: safely reset counter and clear stale dispatch entries
        nextSequenceNumber = 1;
        dispatchHeap.clear();
    }

    // Prepare DEPQ entry with discrete priority rank and monotonic sequence number
    const DispatchEntry dispatchEntry{
        id,
        getPriorityRank(workingOrder.getPriority()),
        nextSequenceNumber++
    };

    // 5. Multi-step atomic registration:
    // Step A: Insert into master orders registry
    auto [it, inserted] = orders.emplace(id, std::move(workingOrder));
    if (!inserted) {
        return false;
    }

    // Step B: Insert into DEPQ dispatch heap
    try {
        dispatchHeap.insert(dispatchEntry);
    } catch (...) {
        orders.erase(it);
        return false;
    }

    // Step C: Enqueue into target FIFO priority lane
    try {
        Queue<std::string>& lane = getQueueForPriority(it->second.getPriority());
        lane.enqueue(id);
    } catch (...) {
        // Rollback earlier registrations: remove from DEPQ and master registry
        dispatchHeap.removeIf([&id](const DispatchEntry& e) {
            return e.orderId == id;
        });
        orders.erase(it);
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Shared Order Fulfillment Engine (Two-Phase Commit with Compensating Rollback)
// ---------------------------------------------------------------------------

bool OrderManager::executeOrderFulfillment(Order& order)
{
    // Transition from Pending to Processing
    if (!order.setStatus(OrderStatus::Processing)) {
        return false;
    }

    // -----------------------------------------------------------------
    // Phase 1: Cumulative Demand Aggregation & Preflight Stock Check
    // Retain first-seen unique SKU order
    // -----------------------------------------------------------------
    std::vector<std::string> uniqueSkus;
    std::unordered_map<std::string, std::int64_t> cumulativeDemand;
    bool overflowDetected = false;

    for (const auto& item : order.getItems()) {
        if (cumulativeDemand.find(item.productId) == cumulativeDemand.end()) {
            uniqueSkus.push_back(item.productId);
            cumulativeDemand[item.productId] = 0;
        }

        if (cumulativeDemand[item.productId] >
            static_cast<std::int64_t>(std::numeric_limits<int>::max()) - item.quantity) {
            overflowDetected = true;
            break;
        }

        cumulativeDemand[item.productId] += item.quantity;
        if (cumulativeDemand[item.productId] > std::numeric_limits<int>::max()) {
            overflowDetected = true;
            break;
        }
    }

    if (overflowDetected) {
        order.setStatus(OrderStatus::Failed);
        order.setFailureReason("Quantity overflow in cumulative demand calculation");
        return true;
    }

    // Validate stock availability for all unique SKUs
    bool preflightPassed = true;
    std::string failureDetail;

    for (const auto& sku : uniqueSkus) {
        Product* p = inventoryManager.findProduct(sku);
        if (p == nullptr) {
            preflightPassed = false;
            failureDetail = "Product not found in catalog: " + sku;
            break;
        }

        std::int64_t required = cumulativeDemand[sku];
        int available = p->getQuantity();
        if (static_cast<std::int64_t>(available) < required) {
            preflightPassed = false;
            failureDetail = "Insufficient stock for SKU " + sku +
                            ": required " + std::to_string(required) +
                            ", available " + std::to_string(available);
            break;
        }
    }

    if (!preflightPassed) {
        order.setStatus(OrderStatus::Failed);
        order.setFailureReason(failureDetail);
        return true; // Preflight failed: zero inventory touched
    }

    // -----------------------------------------------------------------
    // Phase 2: Mutation Planning & Execution in First-Seen Order
    // -----------------------------------------------------------------
    struct StockMutation
    {
        std::string sku;
        int originalQty;
        int targetQty;
    };

    std::vector<StockMutation> plan;
    plan.reserve(uniqueSkus.size());

    for (const auto& sku : uniqueSkus) {
        Product* p = inventoryManager.findProduct(sku);
        int orig = p->getQuantity();
        int target = orig - static_cast<int>(cumulativeDemand[sku]);
        plan.push_back({sku, orig, target});
    }

    std::size_t appliedCount = 0;
    bool mutationFailed = false;
    std::string failedSku;

    for (const auto& step : plan) {
        if (preMutationHook) {
            preMutationHook(step.sku); // Test seam allows deterministic sabotage
        }

        if (!inventoryManager.updateProductQuantity(step.sku, step.targetQty)) {
            mutationFailed = true;
            failedSku = step.sku;
            break;
        }
        ++appliedCount;
    }

    if (!mutationFailed) {
        order.setStatus(OrderStatus::Completed);
        return true;
    }

    // -----------------------------------------------------------------
    // Phase 3: Compensating Rollback of Applied Deductions
    // -----------------------------------------------------------------
    bool rollbackFailed = false;
    std::string rollbackFailedSku;

    for (std::size_t i = appliedCount; i > 0; --i) {
        const auto& step = plan[i - 1];
        if (!inventoryManager.updateProductQuantity(step.sku, step.originalQty)) {
            rollbackFailed = true;
            rollbackFailedSku = step.sku;
        }
    }

    order.setStatus(OrderStatus::Failed);
    if (rollbackFailed) {
        order.setFailureReason(
            "CRITICAL: Partial rollback failure on SKU " + rollbackFailedSku +
            "; inventory inconsistent");
    } else {
        order.setFailureReason(
            "Inventory update mutation failed on SKU " + failedSku +
            "; prior deductions successfully rolled back");
    }

    return true;
}

// ---------------------------------------------------------------------------
// Standard FIFO Order Processing Loop
// ---------------------------------------------------------------------------

bool OrderManager::processNextOrder()
{
    std::string orderId;
    while (selectNextPendingOrderId(orderId)) {
        auto it = orders.find(orderId);
        if (it == orders.end()) {
            continue; // Stale or unregistered entry, safely discard
        }

        Order& order = it->second;
        if (order.getStatus() != OrderStatus::Pending) {
            continue; // Lazily skip already-processed, failed, or cancelled orders
        }

        return executeOrderFulfillment(order);
    }

    return false; // No pending orders were available
}

std::size_t OrderManager::processAllPendingOrders()
{
    std::size_t processed = 0;
    while (processNextOrder()) {
        ++processed;
    }
    return processed;
}

// ---------------------------------------------------------------------------
// DEPQ-based Dynamic Dispatch Operations
// ---------------------------------------------------------------------------

bool OrderManager::peekHighestPriorityOrder(std::string& outOrderId)
{
    DispatchEntry entry;
    while (dispatchHeap.peekMax(entry)) {
        auto it = orders.find(entry.orderId);
        if (it == orders.end() || it->second.getStatus() != OrderStatus::Pending) {
            // Lazily clean stale or non-pending entry at top of heap
            dispatchHeap.extractMax(entry);
            continue;
        }
        outOrderId = entry.orderId;
        return true;
    }
    return false;
}

bool OrderManager::peekLowestPriorityOrder(std::string& outOrderId)
{
    DispatchEntry entry;
    while (dispatchHeap.peekMin(entry)) {
        auto it = orders.find(entry.orderId);
        if (it == orders.end() || it->second.getStatus() != OrderStatus::Pending) {
            // Lazily clean stale or non-pending entry at bottom of heap
            dispatchHeap.extractMin(entry);
            continue;
        }
        outOrderId = entry.orderId;
        return true;
    }
    return false;
}

bool OrderManager::dispatchHighestPriorityOrder()
{
    DispatchEntry entry;
    while (dispatchHeap.extractMax(entry)) {
        auto it = orders.find(entry.orderId);
        if (it == orders.end()) {
            continue; // Safely discard missing registry reference
        }

        Order& order = it->second;
        if (order.getStatus() != OrderStatus::Pending) {
            continue; // Lazily skip already-fulfilled, failed, or cancelled orders
        }

        return executeOrderFulfillment(order);
    }
    return false;
}

bool OrderManager::dispatchLowestPriorityOrder()
{
    DispatchEntry entry;
    while (dispatchHeap.extractMin(entry)) {
        auto it = orders.find(entry.orderId);
        if (it == orders.end()) {
            continue; // Safely discard missing registry reference
        }

        Order& order = it->second;
        if (order.getStatus() != OrderStatus::Pending) {
            continue; // Lazily skip already-fulfilled, failed, or cancelled orders
        }

        return executeOrderFulfillment(order);
    }
    return false;
}

// ---------------------------------------------------------------------------
// Cancellation
// ---------------------------------------------------------------------------

bool OrderManager::cancelOrder(const std::string& orderId)
{
    auto it = orders.find(orderId);
    if (it == orders.end()) {
        return false;
    }

    Order& order = it->second;
    if (order.getStatus() == OrderStatus::Pending) {
        order.setStatus(OrderStatus::Cancelled);
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Queries & Inspection
// ---------------------------------------------------------------------------

const Order* OrderManager::getOrder(const std::string& orderId) const
{
    auto it = orders.find(orderId);
    if (it != orders.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::size_t OrderManager::getPendingCount() const noexcept
{
    std::size_t count = 0;
    for (const auto& [id, order] : orders) {
        if (order.getStatus() == OrderStatus::Pending) {
            ++count;
        }
    }
    return count;
}

std::size_t OrderManager::getCompletedCount() const noexcept
{
    std::size_t count = 0;
    for (const auto& [id, order] : orders) {
        if (order.getStatus() == OrderStatus::Completed) {
            ++count;
        }
    }
    return count;
}

std::size_t OrderManager::getFailedCount() const noexcept
{
    std::size_t count = 0;
    for (const auto& [id, order] : orders) {
        if (order.getStatus() == OrderStatus::Failed) {
            ++count;
        }
    }
    return count;
}

std::size_t OrderManager::getCancelledCount() const noexcept
{
    std::size_t count = 0;
    for (const auto& [id, order] : orders) {
        if (order.getStatus() == OrderStatus::Cancelled) {
            ++count;
        }
    }
    return count;
}

std::size_t OrderManager::getTotalOrderCount() const noexcept
{
    return orders.size();
}

bool OrderManager::hasPendingOrders() const noexcept
{
    return getPendingCount() > 0;
}

FreightPricingEngine& OrderManager::getPricingEngine() noexcept
{
    return pricingEngine;
}

const FreightPricingEngine& OrderManager::getPricingEngine() const noexcept
{
    return pricingEngine;
}
