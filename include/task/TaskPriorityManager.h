#pragma once

#include "order/MinMaxHeap.h"
#include "task/PriorityTask.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>

/// Operational lifecycle state of a managed warehouse task.
enum class TaskState
{
    Active,     // In queue, eligible for dispatch
    Dispatched, // Dispatched/extracted from queue
    Cancelled   // Cancelled prior to dispatch
};

/// String conversion for TaskState.
constexpr const char* toString(TaskState state) noexcept
{
    switch (state) {
    case TaskState::Active:     return "Active";
    case TaskState::Dispatched: return "Dispatched";
    case TaskState::Cancelled:  return "Cancelled";
    default:                    return "Unknown";
    }
}

inline std::string to_string(TaskState state)
{
    return toString(state);
}

/// Internal value-semantic queue entry stored in MinMaxHeap.
/// Binds a PriorityTask snapshot to its registration/update version counter.
struct TaskHeapEntry
{
    PriorityTask task;
    std::uint64_t entryVersion{1};

    // Forward ordering exclusively to PriorityTask::operator<
    bool operator<(const TaskHeapEntry& other) const noexcept
    {
        return task < other.task;
    }
    bool operator>(const TaskHeapEntry& other) const noexcept  { return other < *this; }
    bool operator<=(const TaskHeapEntry& other) const noexcept { return !(other < *this); }
    bool operator>=(const TaskHeapEntry& other) const noexcept { return !(*this < other); }

    bool operator==(const TaskHeapEntry& other) const noexcept
    {
        return task == other.task && entryVersion == other.entryVersion;
    }
    bool operator!=(const TaskHeapEntry& other) const noexcept { return !(*this == other); }
};

/// Internal record stored in master task registry.
struct TaskRecord
{
    PriorityTask task;
    TaskState state{TaskState::Active};
    std::uint64_t currentVersion{1};
};

/// Standalone operational task and event coordinator for Module 6.
///
/// Architecture & Guarantees:
/// - Maintains a master registry keyed by unique taskId, enforcing task ID uniqueness
///   across active, cancelled, and dispatched tasks while registered.
/// - When clear() is invoked, all registered tasks and queued entries are removed;
///   previously used task IDs may be reused after a clear().
/// - Manages a Double-Ended Priority Queue (MinMaxHeap<TaskHeapEntry>) for dual-ended dispatch.
/// - Uses versioned lazy invalidation for task cancellation and priority updates.
///   Stale heap entries from cancelled tasks or superseded priority updates are discarded upon
///   peeking or extraction; tasks cannot be dispatched more than once.
/// - Assigns strictly monotonic intake sequence numbers, preserving arrival seniority across
///   subsequent priority promotions and demotions.
/// - Strong exception safety is maintained across mutating operations.
class TaskPriorityManager
{
private:
    std::unordered_map<std::string, TaskRecord> registry;
    MinMaxHeap<TaskHeapEntry> taskHeap;
    std::uint64_t nextSequenceNumber{1};
    std::size_t activeCount{0};

    bool isEntryStale(const TaskHeapEntry& entry) const noexcept;

public:
    TaskPriorityManager() = default;
    ~TaskPriorityManager() = default;

    TaskPriorityManager(const TaskPriorityManager&) = delete;
    TaskPriorityManager& operator=(const TaskPriorityManager&) = delete;
    TaskPriorityManager(TaskPriorityManager&&) noexcept = default;
    TaskPriorityManager& operator=(TaskPriorityManager&&) noexcept = default;

    // --- Task Lifecycle Operations ---

    /// Registers a new task into the manager, assigning an internal monotonic sequence number.
    /// Returns false if taskId already exists in the registry (even if cancelled or dispatched),
    /// if input parameters are invalid/blank, if enum values are invalid, or if the sequence counter
    /// cannot be safely advanced.
    bool registerTask(const PriorityTask& task);

    /// Convenience intake overload constructing and validating a new task.
    /// Returns false on duplicate taskId, blank identifiers, invalid enums, or sequence overflow.
    bool registerTask(std::string taskId,
                      TaskCategory category,
                      TaskPriority priority,
                      std::string distributionCenterId);

    /// Cancels an active task by ID.
    /// Returns false if taskId is not found or is not in Active state (e.g. already cancelled or dispatched).
    bool cancelTask(const std::string& taskId);

    /// Updates the priority of an active task, preserving its taskId, category, distributionCenterId,
    /// and original arrival sequence number.
    /// Returns false if taskId is not found, task is not Active, newPriority is invalid,
    /// newPriority is identical to the currently assigned priority, or currentVersion has reached
    /// its maximum representation limit.
    bool updatePriority(const std::string& taskId, TaskPriority newPriority);

    // --- Dual-Ended Dispatch (DEPQ) ---

    /// Inspects the highest-urgency active task without removing it.
    /// Purges stale tombstones at the root until an active task is found.
    /// Returns false if no active tasks exist.
    bool peekHighestPriority(PriorityTask& outTask);

    /// Inspects the lowest-urgency active task without removing it.
    /// Purges stale tombstones at the root until an active task is found.
    /// Returns false if no active tasks exist.
    bool peekLowestPriority(PriorityTask& outTask);

    /// Extracts and dispatches the highest-urgency active task.
    /// Purges stale tombstones until an active task is found.
    /// Transitions task state to Dispatched and decrements active count.
    /// Returns false if no active tasks exist.
    bool dispatchHighestPriority(PriorityTask& outTask);

    /// Extracts and dispatches the lowest-urgency active task.
    /// Purges stale tombstones until an active task is found.
    /// Transitions task state to Dispatched and decrements active count.
    /// Returns false if no active tasks exist.
    bool dispatchLowestPriority(PriorityTask& outTask);

    // --- State Queries & Inspection ---

    /// Returns true if the task exists in the registry in any state (Active, Dispatched, Cancelled).
    [[nodiscard]] bool hasTask(const std::string& taskId) const noexcept;

    /// Returns a pointer to the current authoritative task snapshot, or nullptr if not found.
    [[nodiscard]] const PriorityTask* getTask(const std::string& taskId) const noexcept;

    /// Returns the lifecycle state of the task, or std::nullopt if not found.
    [[nodiscard]] std::optional<TaskState> getTaskState(const std::string& taskId) const noexcept;

    /// Returns the total number of active tasks currently eligible for dispatch.
    [[nodiscard]] std::size_t getActiveCount() const noexcept;

    /// Returns the total number of tasks registered in any state (Active, Dispatched, Cancelled).
    [[nodiscard]] std::size_t getTotalRegisteredCount() const noexcept;

    /// Returns true if there are zero active tasks eligible for dispatch.
    [[nodiscard]] bool isEmpty() const noexcept;

    /// Clears all registered tasks and queued entries from the manager, resetting active count.
    /// After clear(), the registry is emptied so previously used task IDs may be reused.
    /// The monotonic sequence counter is not reset, maintaining temporal order consistency
    /// for subsequently registered tasks.
    void clear() noexcept;

    /// Diagnostic verification of MinMaxHeap structural and min/max invariants.
    [[nodiscard]] bool verifyHeapInvariants() const noexcept;
};
