#pragma once

#include "inventory/InventoryManager.h"
#include "order/Order.h"
#include "order/Queue.h"
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class OrderManagerTestAccessor;

/// Orchestrates order submission, 4-tier FIFO scheduling, and inventory fulfillment.
///
/// Architecture:
/// - Order Intake: Validates order structure, snapshots catalog prices, checks arithmetic limits.
/// - Scheduling: Maintains four discrete FIFO priority lanes (Urgent, High, Normal, Low) storing order IDs.
/// - Inventory Integration: Two-phase commit. Aggregates cumulative demand per unique SKU in first-seen order.
/// - Atomic Rollback: In the event of an ordinary mutation failure, rolls back previously applied deductions.
/// - Cancellation: Lazy cancellation tombstone approach; cancelled orders are skipped during dequeue.
class OrderManager
{
private:
    InventoryManager& inventoryManager;

    // Master order registry (orderId -> Order)
    std::unordered_map<std::string, Order> orders;

    // 4 Priority Lanes (FIFO queues storing order IDs)
    Queue<std::string> urgentQueue;
    Queue<std::string> highQueue;
    Queue<std::string> normalQueue;
    Queue<std::string> lowQueue;

    // Test seam for failure injection during mutation
    std::function<void(const std::string& sku)> preMutationHook;
    friend class OrderManagerTestAccessor;

    Queue<std::string>& getQueueForPriority(OrderPriority priority);
    bool selectNextPendingOrderId(std::string& outOrderId);
    bool snapshotCatalogPrices(Order& order);

public:
    explicit OrderManager(InventoryManager& inventoryManager);

    ~OrderManager() = default;
    OrderManager(const OrderManager&) = delete;
    OrderManager& operator=(const OrderManager&) = delete;
    OrderManager(OrderManager&&) = delete;
    OrderManager& operator=(OrderManager&&) = delete;

    /// Submit a customer order for scheduling and processing.
    ///
    /// Submission contract:
    /// - Order is accepted only after registry insertion AND queue insertion both succeed.
    /// - If queue insertion fails, registry insertion is rolled back. Zero orphaned state remains.
    /// - Rejects empty orders, invalid line items, duplicate order IDs, and missing/invalid catalog prices.
    /// @return true if accepted and queued, false if rejected.
    bool submitOrder(const Order& order);

    /// Dequeue and process the highest-priority pending order.
    /// Processes Urgent -> High -> Normal -> Low, preserving FIFO order within each lane.
    /// Skipped cancelled entries are cleaned lazily.
    /// @return true if an order was dequeued and processed, false if no pending orders exist.
    bool processNextOrder();

    /// Process all pending orders until all queues are empty.
    /// @return Number of orders processed.
    std::size_t processAllPendingOrders();

    /// Cancel a pending order via lazy cancellation tombstone.
    /// @return true if order existed and was in Pending state, false otherwise.
    bool cancelOrder(const std::string& orderId);

    /// Read-only inspection of stored orders.
    const Order* getOrder(const std::string& orderId) const;

    std::size_t getPendingCount() const noexcept;
    std::size_t getCompletedCount() const noexcept;
    std::size_t getFailedCount() const noexcept;
    std::size_t getCancelledCount() const noexcept;
    std::size_t getTotalOrderCount() const noexcept;
    bool hasPendingOrders() const noexcept;
};
