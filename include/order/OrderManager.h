#pragma once

#include "inventory/InventoryManager.h"
#include "order/MinMaxHeap.h"
#include "order/Order.h"
#include "order/Queue.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class OrderManagerTestAccessor;

/// Lightweight dispatch metadata stored in the DEPQ.
/// Encapsulates order ID and discrete, deterministic priority scheduling attributes.
struct DispatchEntry
{
    std::string orderId;
    int priorityRank{0};           // Low=0, Normal=1, High=2, Urgent=3
    std::uint64_t sequenceNumber{0}; // Monotonic intake sequence

    /// Deterministic strict-weak-ordering comparison for MinMaxHeap.
    /// Primary: priorityRank ascending.
    /// Tie-breaker: sequenceNumber descending (earlier submission has smaller sequence number
    ///              and is therefore ordered as greater, preserving FIFO for max-extraction).
    bool operator<(const DispatchEntry& other) const noexcept
    {
        if (priorityRank != other.priorityRank) {
            return priorityRank < other.priorityRank;
        }
        return sequenceNumber > other.sequenceNumber;
    }
};

/// Orchestrates order submission, 4-tier FIFO scheduling, DEPQ-based dynamic dispatch,
/// and inventory fulfillment.
///
/// Architecture:
/// - Order Intake: Validates order structure, snapshots catalog prices, checks arithmetic limits.
/// - Coarse Scheduling: Maintains four discrete FIFO priority lanes (Urgent, High, Normal, Low)
///   for standard fair customer intake.
/// - Dynamic Dispatch (DEPQ): Maintains a Min-Max Heap storing DispatchEntry metadata for
///   dual-ended (highest-urgency express vs lowest-urgency economy backfill) task dispatching.
/// - Single Source of Truth: Order records, line items, and lifecycle states reside strictly
///   in the master orders registry.
/// - State Consistency: An order is fulfilled at most once. FIFO and DEPQ schedulers lazily
///   skip entries whose status is no longer Pending.
/// - Atomic Rollback: Two-phase commit with compensating rollback if an update fails.
class OrderManager
{
private:
    InventoryManager& inventoryManager;

    // Master order registry (orderId -> Order) — Single source of truth
    std::unordered_map<std::string, Order> orders;

    // 4 Priority Lanes (FIFO queues storing order IDs)
    Queue<std::string> urgentQueue;
    Queue<std::string> highQueue;
    Queue<std::string> normalQueue;
    Queue<std::string> lowQueue;

    // DEPQ for dual-ended priority dispatch
    MinMaxHeap<DispatchEntry> dispatchHeap;
    std::uint64_t nextSequenceNumber{1};

    // Test seam for failure injection during mutation
    std::function<void(const std::string& sku)> preMutationHook;
    friend class OrderManagerTestAccessor;

    Queue<std::string>& getQueueForPriority(OrderPriority priority);
    bool selectNextPendingOrderId(std::string& outOrderId);
    bool snapshotCatalogPrices(Order& order);
    bool executeOrderFulfillment(Order& order);

    static constexpr int getPriorityRank(OrderPriority priority) noexcept
    {
        switch (priority) {
        case OrderPriority::Urgent: return 3;
        case OrderPriority::High:   return 2;
        case OrderPriority::Normal: return 1;
        case OrderPriority::Low:    return 0;
        }
        return 1;
    }

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
    /// - Multi-step atomic registration: registry emplace -> DEPQ insert -> FIFO enqueue.
    /// - If any step fails, all earlier registrations are rolled back. Zero orphaned state remains.
    /// - Rejects empty orders, invalid line items, duplicate order IDs, and missing/invalid catalog prices.
    /// @return true if accepted and queued, false if rejected.
    bool submitOrder(const Order& order);

    /// Dequeue and process the highest-priority pending order via standard FIFO lanes.
    /// Processes Urgent -> High -> Normal -> Low, preserving FIFO order within each lane.
    /// Skipped cancelled and already-fulfilled entries are cleaned lazily.
    /// @return true if an order was dequeued and processed, false if no pending orders exist.
    bool processNextOrder();

    /// Process all pending orders via FIFO lanes until all queues are empty.
    /// @return Number of orders processed.
    std::size_t processAllPendingOrders();

    /// Inspect the highest-priority eligible pending dispatch order in the DEPQ.
    /// Lazily cleans any stale or cancelled entries encountered at the top of the heap.
    /// @return true if an eligible pending order exists and outOrderId is populated, false otherwise.
    bool peekHighestPriorityOrder(std::string& outOrderId);

    /// Inspect the lowest-priority eligible pending dispatch order in the DEPQ.
    /// Lazily cleans any stale or cancelled entries encountered at the bottom of the heap.
    /// @return true if an eligible pending order exists and outOrderId is populated, false otherwise.
    bool peekLowestPriorityOrder(std::string& outOrderId);

    /// Extract and dispatch the highest-priority eligible pending order via DEPQ.
    /// Used for express picking, rush delivery, or emergency carrier allocation.
    /// Reuses the standard two-phase inventory fulfillment engine.
    /// @return true if an eligible order was dispatched and fulfilled, false if no pending orders exist.
    bool dispatchHighestPriorityOrder();

    /// Extract and dispatch the lowest-priority eligible pending order via DEPQ.
    /// Used for economy batch consolidation, bulk freight backfill, or load shedding.
    /// Reuses the standard two-phase inventory fulfillment engine.
    /// @return true if an eligible order was dispatched and fulfilled, false if no pending orders exist.
    bool dispatchLowestPriorityOrder();

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
