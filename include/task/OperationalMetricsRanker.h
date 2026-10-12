#pragma once

#include "task/HeapSort.h"
#include "task/MaxHeap.h"
#include "task/PriorityTask.h"
#include "task/WorkerPerformance.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

/// Operational analytics and performance ranking engine for warehouse logistics (Module 6).
///
/// Responsibilities:
/// - Ingests and maintains authoritative operational performance records for warehouse workers.
/// - Validates worker and distribution-center identifiers, rejecting blank or invalid inputs.
/// - Provides safe, overflow-guarded metric accumulation.
/// - Produces full leaderboard rankings using the custom in-place Heap Sort algorithm (O(N log N)).
/// - Executes fast top-K queries using the custom binary Max-Heap with Floyd's build-heap (O(N + K log N)).
/// - Filters and ranks workers within individual distribution centers.
/// - Provides deterministic tie-breaking on equivalent scores via worker ID.
class OperationalMetricsRanker
{
private:
    std::unordered_map<std::string, WorkerPerformance> registry;

public:
    OperationalMetricsRanker() = default;
    ~OperationalMetricsRanker() = default;

    // Rule of 5: value semantics
    OperationalMetricsRanker(const OperationalMetricsRanker&) = default;
    OperationalMetricsRanker& operator=(const OperationalMetricsRanker&) = default;
    OperationalMetricsRanker(OperationalMetricsRanker&&) noexcept = default;
    OperationalMetricsRanker& operator=(OperationalMetricsRanker&&) noexcept = default;

    // --- Intake & Mutation Operations ---

    /// Registers a new worker performance record.
    /// Returns false if workerId already exists in the registry.
    bool registerWorker(const WorkerPerformance& worker);

    /// Convenience intake overload constructing and registering a new worker record.
    /// Returns false on duplicate workerId or blank identifiers.
    bool registerWorker(std::string workerId,
                        std::string distributionCenterId,
                        std::uint32_t tasksCompleted = 0,
                        std::uint32_t unitsHandled = 0,
                        std::uint32_t activeMinutes = 0,
                        std::uint32_t errorCount = 0);

    /// Replaces the statistics of an existing registered worker.
    /// Returns false if workerId is not found.
    bool updateWorker(const WorkerPerformance& worker);

    /// Safely accumulates incremental performance statistics into an existing worker record.
    /// Guarantees that arithmetic overflow will reject the entire update, leaving the
    /// existing record unmodified.
    /// Returns false if workerId is not found or if any field would overflow uint32_t.
    bool accumulateMetrics(const std::string& workerId,
                           std::uint32_t additionalTasks,
                           std::uint32_t additionalUnits,
                           std::uint32_t additionalMinutes,
                           std::uint32_t additionalErrors);

    /// Convenience integration recording fulfillment of a warehouse task.
    /// Increments tasks completed by 1, units handled by unitsHandled, active minutes by durationMinutes,
    /// and errors by 1 if errorOccurred is true.
    /// Returns false if worker is not registered or if an arithmetic overflow would occur.
    bool recordTaskFulfillment(const std::string& workerId,
                               const PriorityTask& task,
                               std::uint32_t unitsHandled,
                               std::uint32_t durationMinutes,
                               bool errorOccurred = false);

    // --- Ranking & Analytics Operations ---

    /// Produces a complete ranked list of all registered workers using in-place Heap Sort in O(N log N).
    /// By default (ascending = false), orders workers from highest performance to lowest performance.
    [[nodiscard]] std::vector<WorkerPerformance> rankAllWorkers(bool ascending = false) const;

    /// Extracts the top-K highest-performing workers using the custom MaxHeap with Floyd's
    /// bottom-up build-heap in O(N + K log N) time.
    /// Returns up to K workers ordered from highest to lowest.
    [[nodiscard]] std::vector<WorkerPerformance> getTopKWorkers(std::size_t k) const;

    /// Filters registered workers by distribution center and returns them ranked using Heap Sort.
    [[nodiscard]] std::vector<WorkerPerformance> rankWorkersByDistributionCenter(
        const std::string& dcId,
        bool ascending = false) const;

    // --- State Queries ---

    [[nodiscard]] bool hasWorker(const std::string& workerId) const noexcept;
    [[nodiscard]] const WorkerPerformance* getWorker(const std::string& workerId) const noexcept;
    [[nodiscard]] std::size_t getWorkerCount() const noexcept;
    [[nodiscard]] bool isEmpty() const noexcept;
    void clear() noexcept;
};
