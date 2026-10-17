#include "task/TaskPriorityManager.h"
#include <limits>
#include <utility>

bool TaskPriorityManager::isEntryStale(const TaskHeapEntry& entry) const noexcept
{
    auto it = registry.find(entry.task.getTaskId());
    if (it == registry.end()) {
        return true;
    }
    const TaskRecord& record = it->second;
    if (record.state != TaskState::Active) {
        return true;
    }
    if (entry.entryVersion != record.currentVersion) {
        return true;
    }
    return false;
}

bool TaskPriorityManager::registerTask(const PriorityTask& task)
{
    return registerTask(task.getTaskId(),
                        task.getCategory(),
                        task.getPriority(),
                        task.getDistributionCenterId());
}

bool TaskPriorityManager::registerTask(std::string taskId,
                                       TaskCategory category,
                                       TaskPriority priority,
                                       std::string distributionCenterId)
{
    if (registry.find(taskId) != registry.end()) {
        return false; // Reject duplicate task ID across entire lifetime
    }
    if (!isValidTaskCategory(category) || !isValidTaskPriority(priority)) {
        return false;
    }
    if (nextSequenceNumber == std::numeric_limits<std::uint64_t>::max()) {
        return false; // Reject registration: sequence counter cannot safely advance
    }

    // Explicitly copy key before moving taskId
    const std::string key = taskId;
    const std::uint64_t seq = nextSequenceNumber;

    try {
        PriorityTask managedTask(
            std::move(taskId),
            category,
            priority,
            std::move(distributionCenterId),
            seq
        );

        TaskHeapEntry heapEntry{managedTask, 1};

        auto [it, inserted] = registry.emplace(
            key,
            TaskRecord{std::move(managedTask), TaskState::Active, 1}
        );
        if (!inserted) {
            return false;
        }

        try {
            taskHeap.insert(std::move(heapEntry));
        } catch (...) {
            registry.erase(it); // Compensating rollback on heap insertion failure
            throw;
        }
    } catch (const std::invalid_argument&) {
        return false; // Blank taskId or distributionCenterId rejected
    }

    ++nextSequenceNumber;
    ++activeCount;
    return true;
}

bool TaskPriorityManager::cancelTask(const std::string& taskId)
{
    auto it = registry.find(taskId);
    if (it == registry.end()) {
        return false;
    }
    TaskRecord& record = it->second;
    if (record.state != TaskState::Active) {
        return false;
    }

    record.state = TaskState::Cancelled;
    --activeCount;
    return true;
}

bool TaskPriorityManager::updatePriority(const std::string& taskId, TaskPriority newPriority)
{
    auto it = registry.find(taskId);
    if (it == registry.end()) {
        return false;
    }
    TaskRecord& record = it->second;
    if (record.state != TaskState::Active) {
        return false;
    }
    if (!isValidTaskPriority(newPriority)) {
        return false;
    }
    if (record.task.getPriority() == newPriority) {
        return false; // No-op: priority is identical
    }
    if (record.currentVersion == std::numeric_limits<std::uint64_t>::max()) {
        return false; // Reject update: version counter cannot safely advance
    }

    const std::uint64_t nextVersion = record.currentVersion + 1;

    PriorityTask updatedTask(
        record.task.getTaskId(),
        record.task.getCategory(),
        newPriority,
        record.task.getDistributionCenterId(),
        record.task.getSequenceNumber() // Original arrival seniority preserved
    );

    TaskHeapEntry newEntry{updatedTask, nextVersion};

    // Insert into heap first: if taskHeap.insert throws bad_alloc, registry is untouched
    taskHeap.insert(std::move(newEntry));

    // Commit to registry (move assignment is noexcept)
    record.task = std::move(updatedTask);
    record.currentVersion = nextVersion;
    return true;
}

bool TaskPriorityManager::dispatchHighestPriority(PriorityTask& outTask)
{
    while (!taskHeap.isEmpty()) {
        TaskHeapEntry entry(
            PriorityTask("placeholder", TaskCategory::Pick, TaskPriority::Low, "DC_0", 0),
            0
        );
        if (!taskHeap.peekMax(entry)) {
            break;
        }

        if (isEntryStale(entry)) {
            taskHeap.extractMax(entry); // Discard stale tombstone
            continue;
        }

        auto it = registry.find(entry.task.getTaskId());
        TaskRecord& record = it->second;

        // Step 1: Assign to caller's output parameter first.
        // If copying the task throws (e.g. std::bad_alloc on string allocation),
        // the task remains untouched in taskHeap and in the registry as Active.
        outTask = record.task;

        // Step 2: Commit state changes once assignment has succeeded.
        record.state = TaskState::Dispatched;
        --activeCount;
        taskHeap.extractMax(entry);
        return true;
    }
    return false;
}

bool TaskPriorityManager::dispatchLowestPriority(PriorityTask& outTask)
{
    while (!taskHeap.isEmpty()) {
        TaskHeapEntry entry(
            PriorityTask("placeholder", TaskCategory::Pick, TaskPriority::Low, "DC_0", 0),
            0
        );
        if (!taskHeap.peekMin(entry)) {
            break;
        }

        if (isEntryStale(entry)) {
            taskHeap.extractMin(entry); // Discard stale tombstone
            continue;
        }

        auto it = registry.find(entry.task.getTaskId());
        TaskRecord& record = it->second;

        // Step 1: Assign to caller's output parameter first.
        // If copying the task throws (e.g. std::bad_alloc on string allocation),
        // the task remains untouched in taskHeap and in the registry as Active.
        outTask = record.task;

        // Step 2: Commit state changes once assignment has succeeded.
        record.state = TaskState::Dispatched;
        --activeCount;
        taskHeap.extractMin(entry);
        return true;
    }
    return false;
}

bool TaskPriorityManager::peekHighestPriority(PriorityTask& outTask)
{
    while (!taskHeap.isEmpty()) {
        TaskHeapEntry entry(
            PriorityTask("placeholder", TaskCategory::Pick, TaskPriority::Low, "DC_0", 0),
            0
        );
        if (!taskHeap.peekMax(entry)) {
            break;
        }

        if (isEntryStale(entry)) {
            // Discard stale root entry
            TaskHeapEntry dummy(
                PriorityTask("placeholder", TaskCategory::Pick, TaskPriority::Low, "DC_0", 0),
                0
            );
            taskHeap.extractMax(dummy);
            continue;
        }

        auto it = registry.find(entry.task.getTaskId());
        outTask = it->second.task;
        return true;
    }
    return false;
}

bool TaskPriorityManager::peekLowestPriority(PriorityTask& outTask)
{
    while (!taskHeap.isEmpty()) {
        TaskHeapEntry entry(
            PriorityTask("placeholder", TaskCategory::Pick, TaskPriority::Low, "DC_0", 0),
            0
        );
        if (!taskHeap.peekMin(entry)) {
            break;
        }

        if (isEntryStale(entry)) {
            // Discard stale root entry
            TaskHeapEntry dummy(
                PriorityTask("placeholder", TaskCategory::Pick, TaskPriority::Low, "DC_0", 0),
                0
            );
            taskHeap.extractMin(dummy);
            continue;
        }

        auto it = registry.find(entry.task.getTaskId());
        outTask = it->second.task;
        return true;
    }
    return false;
}

bool TaskPriorityManager::hasTask(const std::string& taskId) const noexcept
{
    return registry.find(taskId) != registry.end();
}

const PriorityTask* TaskPriorityManager::getTask(const std::string& taskId) const noexcept
{
    auto it = registry.find(taskId);
    if (it != registry.end()) {
        return &(it->second.task);
    }
    return nullptr;
}

std::optional<TaskState> TaskPriorityManager::getTaskState(const std::string& taskId) const noexcept
{
    auto it = registry.find(taskId);
    if (it != registry.end()) {
        return it->second.state;
    }
    return std::nullopt;
}

std::size_t TaskPriorityManager::getActiveCount() const noexcept
{
    return activeCount;
}

std::size_t TaskPriorityManager::getTotalRegisteredCount() const noexcept
{
    return registry.size();
}

bool TaskPriorityManager::isEmpty() const noexcept
{
    return activeCount == 0;
}

void TaskPriorityManager::clear() noexcept
{
    registry.clear();
    taskHeap.clear();
    activeCount = 0;
}

bool TaskPriorityManager::verifyHeapInvariants() const noexcept
{
    return taskHeap.verifyHeapInvariants();
}
