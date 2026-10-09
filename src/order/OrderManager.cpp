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
// Scheduler: Select next pending order ID in priority order
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
// Order Submission (Dual-storage atomic commit)
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

    // 4. Freeze order immutability
    workingOrder.freeze();

    // 5. Dual-commit: insert into registry, then enqueue in lane
    auto [it, inserted] = orders.emplace(id, std::move(workingOrder));
    if (!inserted) {
        return false;
    }

    try {
        Queue<std::string>& lane = getQueueForPriority(it->second.getPriority());
        lane.enqueue(id);
    } catch (...) {
        // Rollback registry insertion on queue allocation failure
        orders.erase(it);
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Order Processing Loop (Two-Phase Commit with Compensating Rollback)
// ---------------------------------------------------------------------------

bool OrderManager::processNextOrder()
{
    std::string orderId;
    while (selectNextPendingOrderId(orderId)) {
        auto it = orders.find(orderId);
        if (it == orders.end()) {
            continue;
        }

        Order& order = it->second;

        // Lazy cancellation: skipped during dequeue
        if (order.getStatus() == OrderStatus::Cancelled) {
            continue;
        }

        // Transition from Pending to Processing
        if (!order.setStatus(OrderStatus::Processing)) {
            continue;
        }

        // -------------------------------------------------------------
        // Phase 1: Cumulative Demand Aggregation & Preflight Stock Check
        // Retain first-seen unique SKU order!
        // -------------------------------------------------------------
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

        // -------------------------------------------------------------
        // Phase 2: Mutation Planning & Execution in First-Seen Order
        // -------------------------------------------------------------
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

        // -------------------------------------------------------------
        // Phase 3: Compensating Rollback of Applied Deductions
        // -------------------------------------------------------------
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
