#pragma once

#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

/// Value-semantic model representing warehouse worker operational performance in Module 6.
///
/// Encapsulates productivity, throughput, fulfillment volume, and accuracy metrics.
///
/// Scoring formula (Net Accurate Units Handled Per Active Hour):
///   - If activeMinutes == 0:
///       score = 0.0 (no active work logged; avoids division by zero).
///   - If unitsHandled <= errorCount:
///       score = 0.0 (defect count equals or exceeds units handled; net output is non-positive).
///   - Otherwise:
///       netUnits = unitsHandled - errorCount;
///       score = (netUnits * 60.0) / activeMinutes;
///   This produces a deterministic, finite, interpretable rate (units/hour).
///
/// Deterministic Ordering:
///   operator< establishes strict weak ordering:
///     1. Primary: Efficiency score ascending (higher score ranks greater).
///     2. Secondary: Tasks completed ascending (more tasks ranks greater).
///     3. Tertiary: Units handled ascending (more units ranks greater).
///     4. Deterministic tie-breaker: workerId descending (lexicographically earlier ID ranks greater).
class WorkerPerformance
{
private:
    std::string workerId;
    std::string distributionCenterId;
    std::uint32_t tasksCompleted{0};
    std::uint32_t unitsHandled{0};
    std::uint32_t activeMinutes{0};
    std::uint32_t errorCount{0};

    static bool isBlank(std::string_view s) noexcept
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

public:
    WorkerPerformance() = delete;

    WorkerPerformance(std::string workerId,
                      std::string distributionCenterId,
                      std::uint32_t tasksCompleted = 0,
                      std::uint32_t unitsHandled = 0,
                      std::uint32_t activeMinutes = 0,
                      std::uint32_t errorCount = 0)
        : workerId(std::move(workerId))
        , distributionCenterId(std::move(distributionCenterId))
        , tasksCompleted(tasksCompleted)
        , unitsHandled(unitsHandled)
        , activeMinutes(activeMinutes)
        , errorCount(errorCount)
    {
        if (isBlank(this->workerId)) {
            throw std::invalid_argument("WorkerPerformance: workerId must not be empty or blank");
        }
        if (isBlank(this->distributionCenterId)) {
            throw std::invalid_argument("WorkerPerformance: distributionCenterId must not be empty or blank");
        }
    }

    // Defaulted Rule-of-5 operations
    ~WorkerPerformance() = default;
    WorkerPerformance(const WorkerPerformance&) = default;
    WorkerPerformance& operator=(const WorkerPerformance&) = default;
    WorkerPerformance(WorkerPerformance&&) noexcept = default;
    WorkerPerformance& operator=(WorkerPerformance&&) noexcept = default;

    // --- Accessors ---
    [[nodiscard]] const std::string& getWorkerId() const noexcept { return workerId; }
    [[nodiscard]] const std::string& getDistributionCenterId() const noexcept { return distributionCenterId; }
    [[nodiscard]] std::uint32_t getTasksCompleted() const noexcept { return tasksCompleted; }
    [[nodiscard]] std::uint32_t getUnitsHandled() const noexcept { return unitsHandled; }
    [[nodiscard]] std::uint32_t getActiveMinutes() const noexcept { return activeMinutes; }
    [[nodiscard]] std::uint32_t getErrorCount() const noexcept { return errorCount; }

    /// Computes the net accurate units handled per active hour.
    /// Guaranteed to be non-negative and finite.
    [[nodiscard]] double calculateEfficiencyScore() const noexcept
    {
        if (activeMinutes == 0) {
            return 0.0;
        }
        if (unitsHandled <= errorCount) {
            return 0.0;
        }
        const double netUnits = static_cast<double>(unitsHandled - errorCount);
        return (netUnits * 60.0) / static_cast<double>(activeMinutes);
    }

    // --- Strict Weak Ordering Comparison ---
    bool operator<(const WorkerPerformance& other) const noexcept
    {
        const double scoreA = calculateEfficiencyScore();
        const double scoreB = other.calculateEfficiencyScore();
        if (scoreA != scoreB) {
            return scoreA < scoreB;
        }
        if (tasksCompleted != other.tasksCompleted) {
            return tasksCompleted < other.tasksCompleted;
        }
        if (unitsHandled != other.unitsHandled) {
            return unitsHandled < other.unitsHandled;
        }
        // Earlier worker ID (e.g. "W1" < "W2") is considered greater for dispatch/ranking precedence
        return workerId > other.workerId;
    }

    bool operator>(const WorkerPerformance& other) const noexcept
    {
        return other < *this;
    }

    bool operator<=(const WorkerPerformance& other) const noexcept
    {
        return !(other < *this);
    }

    bool operator>=(const WorkerPerformance& other) const noexcept
    {
        return !(*this < other);
    }

    // --- Identity Equality ---
    bool operator==(const WorkerPerformance& other) const noexcept
    {
        return workerId == other.workerId;
    }

    bool operator!=(const WorkerPerformance& other) const noexcept
    {
        return workerId != other.workerId;
    }

    /// Verifies equality of all metrics and identity fields.
    [[nodiscard]] bool exactEquals(const WorkerPerformance& other) const noexcept
    {
        return workerId == other.workerId
            && distributionCenterId == other.distributionCenterId
            && tasksCompleted == other.tasksCompleted
            && unitsHandled == other.unitsHandled
            && activeMinutes == other.activeMinutes
            && errorCount == other.errorCount;
    }
};
