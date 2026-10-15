#pragma once

#include "task/BinomialHeap.h"
#include "task/PriorityTask.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

struct DistributedTaskQueueTestAccess;

/// Operational priority queue managing warehouse tasks for a distribution center in Module 6.
///
/// Theoretical & Architectural Role:
/// - Provides cross-distribution-center task queue buffering and consolidation using a custom
///   Binomial Heap (O(log N) merge, zero dynamic memory allocations).
/// - Represents a facility's queue identified by distributionCenterId.
/// - Stores autonomous, value-semantic PriorityTask objects.
///
/// Identity & Uniqueness Contract:
/// - In the supply-chain platform domain model, task IDs are assumed globally unique across
///   distribution centers. DistributedTaskQueue deliberately does NOT maintain a secondary
///   hash-table registry or lifecycle engine, avoiding duplicate state and lifecycle drift.
/// - Priority-based extraction order is deterministic via PriorityTask::operator<:
///     1. Priority tier (Critical > Urgent > High > Normal > Low)
///     2. Intake sequence number (earlier sequence has FIFO precedence)
///     3. Origin distribution-center ID (deterministic cross-DC tie-breaker)
///     4. Task ID (final deterministic tie-breaker)
///
/// Origin & Sequencing Semantics:
/// - Each task retains its immutable origin distributionCenterId.
/// - Tasks originated locally via enqueueTask() receive a monotonic sequence number from this queue.
/// - Tasks pushed via pushTask() retain their origin distributionCenterId and arrival sequence number.
/// - Cross-center consolidation via mergeFrom() merges the underlying Binomial Heaps in O(log(N + M))
///   with no dynamic memory allocation and No-Throw Guarantee, leaving the source queue empty.
class DistributedTaskQueue
{
private:
    std::string distributionCenterId;
    BinomialHeap<PriorityTask, std::less<PriorityTask>> heap;
    std::uint64_t nextSequenceNumber{1};
    bool simulateInsertionFailure{false};

    friend struct DistributedTaskQueueTestAccess;

    static bool isBlank(std::string_view s) noexcept;

public:
    DistributedTaskQueue() = delete;

    /// Constructs a task queue for a specific distribution center.
    /// Throws std::invalid_argument if distributionCenterId is empty or whitespace-only.
    explicit DistributedTaskQueue(std::string distributionCenterId);

    ~DistributedTaskQueue() = default;

    // Rule of 5: move-only container
    DistributedTaskQueue(const DistributedTaskQueue&) = delete;
    DistributedTaskQueue& operator=(const DistributedTaskQueue&) = delete;

    DistributedTaskQueue(DistributedTaskQueue&& other) noexcept = default;
    DistributedTaskQueue& operator=(DistributedTaskQueue&& other) noexcept = default;

    /// Returns the identity of the distribution center managing this queue.
    [[nodiscard]] const std::string& getDistributionCenterId() const noexcept;

    /// Direct intake: constructs and sequences a new task originating at this distribution center.
    /// Assigns a strictly monotonic sequence number from the local counter.
    /// Returns false on blank taskId, invalid enum values, or sequence counter overflow.
    bool enqueueTask(std::string taskId,
                     TaskCategory category,
                     TaskPriority priority);

    /// Ingests an existing, already-sequenced task (e.g. transferred from another facility).
    /// Strictly preserves the task's originating distributionCenterId and sequence number.
    /// Performs heap insertion first; commits sequence counter update only after successful insertion.
    /// Returns false on insertion failure, leaving queue contents and sequence counter unchanged.
    bool pushTask(PriorityTask task);

    /// Inspects the highest-urgency task without removing it in O(log N).
    /// Returns false if the queue is empty, true otherwise.
    bool peekTask(PriorityTask& outTask) const;

    /// Extracts and dispatches the highest-urgency task in O(log N).
    /// Returns false if the queue is empty, true otherwise.
    bool popTask(PriorityTask& outTask);

    /// Consolidates another distribution center's task queue into this queue in O(log(N + M)) time.
    /// Merges the underlying Binomial Heaps with zero dynamic memory allocations and No-Throw Guarantee.
    /// Leaves other empty (other.isEmpty() == true).
    void mergeFrom(DistributedTaskQueue&& other) noexcept;

    /// Returns the total number of tasks currently queued.
    [[nodiscard]] std::size_t getTaskCount() const noexcept;

    /// Returns true if there are zero tasks in the queue.
    [[nodiscard]] bool isEmpty() const noexcept;

    /// Removes all tasks from the queue in O(N).
    void clear() noexcept;

    /// Verifies Binomial Heap structural and degree invariants.
    [[nodiscard]] bool verifyHeapInvariants() const noexcept;
};


