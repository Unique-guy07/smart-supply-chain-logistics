#include "task/DistributedTaskQueue.h"
#include <cctype>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

bool DistributedTaskQueue::isBlank(std::string_view s) noexcept
{
    if (s.empty()) {
        return true;
    }
    for (char c : s) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

DistributedTaskQueue::DistributedTaskQueue(std::string distributionCenterId)
    : distributionCenterId(std::move(distributionCenterId))
    , heap()
    , nextSequenceNumber(1)
{
    if (isBlank(this->distributionCenterId)) {
        throw std::invalid_argument("DistributedTaskQueue: distributionCenterId must not be empty or blank");
    }
}

const std::string& DistributedTaskQueue::getDistributionCenterId() const noexcept
{
    return distributionCenterId;
}

bool DistributedTaskQueue::enqueueTask(std::string taskId,
                                       TaskCategory category,
                                       TaskPriority priority)
{
    if (isBlank(taskId) || !isValidTaskCategory(category) || !isValidTaskPriority(priority)) {
        return false;
    }
    if (nextSequenceNumber == std::numeric_limits<std::uint64_t>::max()) {
        return false; // Guard against sequence overflow
    }

    try {
        PriorityTask task(
            std::move(taskId),
            category,
            priority,
            distributionCenterId,
            nextSequenceNumber
        );
        if (simulateInsertionFailure) {
            throw std::bad_alloc();
        }
        heap.insert(std::move(task));
        ++nextSequenceNumber;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool DistributedTaskQueue::pushTask(PriorityTask task)
{
    // 1. Calculate prospective sequence-counter update without mutating current counter
    std::uint64_t prospectiveNextSeq = nextSequenceNumber;
    if (task.getDistributionCenterId() == distributionCenterId) {
        if (task.getSequenceNumber() >= nextSequenceNumber) {
            if (task.getSequenceNumber() < std::numeric_limits<std::uint64_t>::max()) {
                prospectiveNextSeq = task.getSequenceNumber() + 1;
            } else {
                prospectiveNextSeq = std::numeric_limits<std::uint64_t>::max();
            }
        }
    }

    // 2. Perform heap insertion first
    try {
        if (simulateInsertionFailure) {
            throw std::bad_alloc();
        }
        heap.insert(std::move(task));
    } catch (const std::exception&) {
        // On insertion failure, task is not queued and counter remains unchanged
        return false;
    }

    // 3. Commit counter update only after insertion succeeds
    nextSequenceNumber = prospectiveNextSeq;
    return true;
}

bool DistributedTaskQueue::peekTask(PriorityTask& outTask) const
{
    return heap.peek(outTask);
}

bool DistributedTaskQueue::popTask(PriorityTask& outTask)
{
    return heap.extract(outTask);
}

void DistributedTaskQueue::mergeFrom(DistributedTaskQueue&& other) noexcept
{
    if (this == &other) {
        return;
    }
    heap.merge(std::move(other.heap));
}

std::size_t DistributedTaskQueue::getTaskCount() const noexcept
{
    return heap.getSize();
}

bool DistributedTaskQueue::isEmpty() const noexcept
{
    return heap.isEmpty();
}

void DistributedTaskQueue::clear() noexcept
{
    heap.clear();
}

bool DistributedTaskQueue::verifyHeapInvariants() const noexcept
{
    return heap.verifyHeapInvariants();
}


