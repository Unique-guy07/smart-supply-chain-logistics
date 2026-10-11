#pragma once

#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

/// Operational category of a warehouse task or event in Module 6.
enum class TaskCategory
{
    Pick,        // Order line fulfillment / stock picking
    Restock,     // Inbound replenishment / bin shelving
    Pack,        // Packaging / consolidation for outbound shipment
    Maintenance, // Warehouse equipment, conveyor, or AGV maintenance
    Audit,       // Cycle counting and physical inventory audit
    Emergency    // Critical warehouse incident (e.g. hazard, breakdown, safety halt)
};

/// 5-tier priority hierarchy for operational task scheduling.
enum class TaskPriority
{
    Low = 0,      // Routine background tasks (e.g. general housekeeping)
    Normal = 1,   // Standard warehouse operations
    High = 2,     // Time-sensitive fulfillment / prioritized picks
    Urgent = 3,   // Expedited orders / imminent dispatch deadlines
    Critical = 4  // Safety halts, emergency events, severe equipment alarms
};

/// String conversion for TaskCategory.
/// Safely returns "Unknown" for unmapped or invalid enum values.
constexpr const char* toString(TaskCategory category) noexcept
{
    switch (category) {
    case TaskCategory::Pick:        return "Pick";
    case TaskCategory::Restock:     return "Restock";
    case TaskCategory::Pack:        return "Pack";
    case TaskCategory::Maintenance: return "Maintenance";
    case TaskCategory::Audit:       return "Audit";
    case TaskCategory::Emergency:   return "Emergency";
    default:                        return "Unknown";
    }
}

/// String conversion for TaskPriority.
/// Safely returns "Unknown" for unmapped or invalid enum values.
constexpr const char* toString(TaskPriority priority) noexcept
{
    switch (priority) {
    case TaskPriority::Low:      return "Low";
    case TaskPriority::Normal:   return "Normal";
    case TaskPriority::High:     return "High";
    case TaskPriority::Urgent:   return "Urgent";
    case TaskPriority::Critical: return "Critical";
    default:                     return "Unknown";
    }
}

/// Helper overloads providing std::string representations.
inline std::string to_string(TaskCategory category)
{
    return toString(category);
}

inline std::string to_string(TaskPriority priority)
{
    return toString(priority);
}

/// Validation helper for TaskCategory enumerators.
constexpr bool isValidTaskCategory(TaskCategory category) noexcept
{
    switch (category) {
    case TaskCategory::Pick:
    case TaskCategory::Restock:
    case TaskCategory::Pack:
    case TaskCategory::Maintenance:
    case TaskCategory::Audit:
    case TaskCategory::Emergency:
        return true;
    default:
        return false;
    }
}

/// Validation helper for TaskPriority enumerators.
constexpr bool isValidTaskPriority(TaskPriority priority) noexcept
{
    switch (priority) {
    case TaskPriority::Low:
    case TaskPriority::Normal:
    case TaskPriority::High:
    case TaskPriority::Urgent:
    case TaskPriority::Critical:
        return true;
    default:
        return false;
    }
}

/// Represents a prioritized warehouse operational task or event in Module 6.
///
/// Value Semantics & Ownership:
/// - PriorityTask is an autonomous, value-semantic object with self-contained ownership.
/// - Copy and move operations are compiler-generated; move operations are noexcept.
/// - Default construction is explicitly deleted to guarantee every instance is a valid,
///   non-blank task.
///
/// Ordering & MinMaxHeap Compatibility:
/// - Designed directly for use in MinMaxHeap<PriorityTask, std::less<PriorityTask>>.
/// - Operator< implements strict weak ordering:
///     1. Priority ascending: Higher priority ranks higher (extracted first by extractMax()).
///     2. Sequence number descending: Earlier intake sequence number is treated as greater,
///        preserving strict FIFO scheduling under extractMax().
///     3. Distribution Center ID ascending: Deterministic tie-breaking on sequence collision.
///     4. Task ID ascending: Final deterministic tie-breaker.
///
/// Identity & Equality:
/// - Equality (operator==) follows entity identity: two tasks are equal if and only if
///   their taskId matches.
/// - In the Stage 1 domain model, task IDs are assumed to be globally unique.
class PriorityTask
{
private:
    std::string taskId;
    TaskCategory category;
    TaskPriority priority;
    std::string distributionCenterId;
    std::uint64_t sequenceNumber{0};

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
    PriorityTask() = delete;

    PriorityTask(std::string taskId,
                 TaskCategory category,
                 TaskPriority priority,
                 std::string distributionCenterId,
                 std::uint64_t sequenceNumber)
        : taskId(std::move(taskId))
        , category(category)
        , priority(priority)
        , distributionCenterId(std::move(distributionCenterId))
        , sequenceNumber(sequenceNumber)
    {
        if (isBlank(this->taskId)) {
            throw std::invalid_argument("PriorityTask: taskId must not be empty or blank");
        }
        if (!isValidTaskCategory(this->category)) {
            throw std::invalid_argument("PriorityTask: invalid TaskCategory enum value");
        }
        if (!isValidTaskPriority(this->priority)) {
            throw std::invalid_argument("PriorityTask: invalid TaskPriority enum value");
        }
        if (isBlank(this->distributionCenterId)) {
            throw std::invalid_argument("PriorityTask: distributionCenterId must not be empty or blank");
        }
    }

    // Defaulted Rule-of-5 operations
    ~PriorityTask() = default;
    PriorityTask(const PriorityTask&) = default;
    PriorityTask& operator=(const PriorityTask&) = default;
    PriorityTask(PriorityTask&&) noexcept = default;
    PriorityTask& operator=(PriorityTask&&) noexcept = default;

    // --- Accessors ---
    [[nodiscard]] const std::string& getTaskId() const noexcept { return taskId; }
    [[nodiscard]] TaskCategory getCategory() const noexcept { return category; }
    [[nodiscard]] TaskPriority getPriority() const noexcept { return priority; }
    [[nodiscard]] const std::string& getDistributionCenterId() const noexcept { return distributionCenterId; }
    [[nodiscard]] std::uint64_t getSequenceNumber() const noexcept { return sequenceNumber; }

    // --- Comparison Operators (MinMaxHeap Strict Weak Ordering) ---

    /// Deterministic strict weak ordering comparison.
    /// Compatible with MinMaxHeap (default std::less):
    /// - extractMax() yields the "greatest" element:
    ///     Higher priority is greater.
    ///     Earlier sequenceNumber is greater (FIFO order).
    ///     distributionCenterId and taskId tie-break deterministically.
    bool operator<(const PriorityTask& other) const noexcept
    {
        if (priority != other.priority) {
            return static_cast<int>(priority) < static_cast<int>(other.priority);
        }
        if (sequenceNumber != other.sequenceNumber) {
            // Earlier sequence number has higher dispatch precedence, so it is "greater"
            return sequenceNumber > other.sequenceNumber;
        }
        if (distributionCenterId != other.distributionCenterId) {
            return distributionCenterId < other.distributionCenterId;
        }
        return taskId < other.taskId;
    }

    bool operator>(const PriorityTask& other) const noexcept
    {
        return other < *this;
    }

    bool operator<=(const PriorityTask& other) const noexcept
    {
        return !(other < *this);
    }

    bool operator>=(const PriorityTask& other) const noexcept
    {
        return !(*this < other);
    }

    // --- Identity Equality ---

    /// Equality is based strictly on task identity (taskId).
    /// Task IDs are assumed to be globally unique in the logistics system.
    bool operator==(const PriorityTask& other) const noexcept
    {
        return taskId == other.taskId;
    }

    bool operator!=(const PriorityTask& other) const noexcept
    {
        return taskId != other.taskId;
    }

    /// Helper for testing full value-attribute equality.
    bool exactEquals(const PriorityTask& other) const noexcept
    {
        return taskId == other.taskId
            && category == other.category
            && priority == other.priority
            && distributionCenterId == other.distributionCenterId
            && sequenceNumber == other.sequenceNumber;
    }
};
