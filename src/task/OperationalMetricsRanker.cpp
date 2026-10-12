#include "task/OperationalMetricsRanker.h"
#include <algorithm>
#include <limits>
#include <utility>

bool OperationalMetricsRanker::registerWorker(const WorkerPerformance& worker)
{
    if (registry.find(worker.getWorkerId()) != registry.end()) {
        return false; // Reject duplicate worker ID
    }
    registry.emplace(worker.getWorkerId(), worker);
    return true;
}

bool OperationalMetricsRanker::registerWorker(std::string workerId,
                                              std::string distributionCenterId,
                                              std::uint32_t tasksCompleted,
                                              std::uint32_t unitsHandled,
                                              std::uint32_t activeMinutes,
                                              std::uint32_t errorCount)
{
    try {
        WorkerPerformance worker(
            std::move(workerId),
            std::move(distributionCenterId),
            tasksCompleted,
            unitsHandled,
            activeMinutes,
            errorCount
        );
        return registerWorker(worker);
    } catch (const std::invalid_argument&) {
        return false; // Blank identifiers rejected
    }
}

bool OperationalMetricsRanker::updateWorker(const WorkerPerformance& worker)
{
    auto it = registry.find(worker.getWorkerId());
    if (it == registry.end()) {
        return false;
    }
    it->second = worker;
    return true;
}

bool OperationalMetricsRanker::accumulateMetrics(const std::string& workerId,
                                                 std::uint32_t additionalTasks,
                                                 std::uint32_t additionalUnits,
                                                 std::uint32_t additionalMinutes,
                                                 std::uint32_t additionalErrors)
{
    auto it = registry.find(workerId);
    if (it == registry.end()) {
        return false;
    }

    const WorkerPerformance& current = it->second;

    // Check for uint32 overflow across all fields before mutating
    if (current.getTasksCompleted() > std::numeric_limits<std::uint32_t>::max() - additionalTasks) {
        return false;
    }
    if (current.getUnitsHandled() > std::numeric_limits<std::uint32_t>::max() - additionalUnits) {
        return false;
    }
    if (current.getActiveMinutes() > std::numeric_limits<std::uint32_t>::max() - additionalMinutes) {
        return false;
    }
    if (current.getErrorCount() > std::numeric_limits<std::uint32_t>::max() - additionalErrors) {
        return false;
    }

    WorkerPerformance updated(
        current.getWorkerId(),
        current.getDistributionCenterId(),
        current.getTasksCompleted() + additionalTasks,
        current.getUnitsHandled() + additionalUnits,
        current.getActiveMinutes() + additionalMinutes,
        current.getErrorCount() + additionalErrors
    );

    it->second = std::move(updated);
    return true;
}

bool OperationalMetricsRanker::recordTaskFulfillment(const std::string& workerId,
                                                     const PriorityTask& /* task */,
                                                     std::uint32_t unitsHandled,
                                                     std::uint32_t durationMinutes,
                                                     bool errorOccurred)
{
    return accumulateMetrics(
        workerId,
        1,
        unitsHandled,
        durationMinutes,
        errorOccurred ? 1 : 0
    );
}

std::vector<WorkerPerformance> OperationalMetricsRanker::rankAllWorkers(bool ascending) const
{
    std::vector<WorkerPerformance> result;
    result.reserve(registry.size());
    for (const auto& [id, record] : registry) {
        result.push_back(record);
    }

    if (ascending) {
        logistics::algo::heapSort(result);
    } else {
        logistics::algo::heapSort(result, std::greater<WorkerPerformance>());
    }
    return result;
}

std::vector<WorkerPerformance> OperationalMetricsRanker::getTopKWorkers(std::size_t k) const
{
    if (k == 0 || registry.empty()) {
        return {};
    }

    std::vector<WorkerPerformance> elements;
    elements.reserve(registry.size());
    for (const auto& [id, record] : registry) {
        elements.push_back(record);
    }

    // Build MaxHeap using Floyd's bottom-up construction in O(N)
    MaxHeap<WorkerPerformance> heap(std::move(elements));

    const std::size_t count = std::min(k, heap.getSize());
    std::vector<WorkerPerformance> topK;
    topK.reserve(count);

    WorkerPerformance out("placeholder", "DC_0");
    for (std::size_t i = 0; i < count; ++i) {
        if (heap.extractMax(out)) {
            topK.push_back(std::move(out));
        }
    }
    return topK;
}

std::vector<WorkerPerformance> OperationalMetricsRanker::rankWorkersByDistributionCenter(
    const std::string& dcId,
    bool ascending) const
{
    std::vector<WorkerPerformance> result;
    for (const auto& [id, record] : registry) {
        if (record.getDistributionCenterId() == dcId) {
            result.push_back(record);
        }
    }

    if (ascending) {
        logistics::algo::heapSort(result);
    } else {
        logistics::algo::heapSort(result, std::greater<WorkerPerformance>());
    }
    return result;
}

bool OperationalMetricsRanker::hasWorker(const std::string& workerId) const noexcept
{
    return registry.find(workerId) != registry.end();
}

const WorkerPerformance* OperationalMetricsRanker::getWorker(const std::string& workerId) const noexcept
{
    auto it = registry.find(workerId);
    if (it != registry.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::size_t OperationalMetricsRanker::getWorkerCount() const noexcept
{
    return registry.size();
}

bool OperationalMetricsRanker::isEmpty() const noexcept
{
    return registry.empty();
}

void OperationalMetricsRanker::clear() noexcept
{
    registry.clear();
}
