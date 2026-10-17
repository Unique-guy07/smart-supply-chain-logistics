#pragma once

#include "order/CircularLinkedList.h"
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

/// Represents a historical audit selection record.
struct AuditRecord
{
    std::size_t sequenceNumber{0};    // Strictly monotonically increasing global sequence identifier
    std::size_t roundNumber{0};       // 1-based audit round counter (shared across all selections in a batch)
    std::string targetId;             // Identifier of the audited item (e.g. order ID, bin ID)
    std::size_t stepIntervalUsed{0};  // Josephus step interval (k) applied during this selection
};

/// Continuous warehouse QA and cyclic audit cycle manager.
///
/// Implements cyclic audit candidate selection using a circular linked list
/// and the Josephus elimination algorithm. Supports continuous replenishment
/// and partial batch selections for ongoing warehouse cycle counting.
///
/// Exception safety guarantees:
/// - Target registration/removal: Strong exception guarantee.
/// - performAuditSelection: Transactional two-phase commit. All prospective selections
///   and history records are staged and reserved upfront. If memory allocation fails,
///   no active targets are lost, and counters/history remain untouched.
class AuditCycle
{
private:
    CircularLinkedList<std::string> m_targets;
    std::vector<AuditRecord> m_history;
    std::size_t m_stepInterval{3};
    std::size_t m_currentRoundNumber{0};
    std::size_t m_nextSequenceNumber{1};

    static bool isBlank(std::string_view str) noexcept;

    // Test accessor seam for boundary condition verification
    friend struct AuditCycleTestAccess;

public:
    /// Constructs an AuditCycle with an optional default step interval (k).
    /// @throws std::invalid_argument if defaultStepInterval == 0.
    explicit AuditCycle(std::size_t defaultStepInterval = 3);

    /// Registers a new audit target (e.g. order ID or bin ID).
    /// Rejects blank/whitespace-only IDs and duplicate active targets.
    /// @return true if added successfully; false if invalid or duplicate.
    bool addTarget(const std::string& targetId);

    /// Removes an active target from the audit cycle.
    /// @return true if removed; false if target was not found.
    bool removeTarget(const std::string& targetId);

    /// Checks if a target is currently in the active audit cycle.
    [[nodiscard]] bool containsTarget(const std::string& targetId) const noexcept;

    /// Returns the number of currently active audit targets.
    [[nodiscard]] std::size_t getTargetCount() const noexcept;

    /// Checks whether the active audit cycle is empty.
    [[nodiscard]] bool isEmpty() const noexcept;

    /// Returns a vector of active targets in current circular cycle traversal order.
    [[nodiscard]] std::vector<std::string> getActiveTargets() const;

    /// Updates the default Josephus step interval (k).
    /// @throws std::invalid_argument if stepInterval == 0.
    void setStepInterval(std::size_t stepInterval);

    /// Returns the current default Josephus step interval.
    [[nodiscard]] std::size_t getStepInterval() const noexcept;

    /// Non-mutating prospective schedule preview using the configured step interval.
    /// @param quota Maximum number of targets to preview (0 = preview all).
    /// @return Target IDs in their projected elimination order.
    [[nodiscard]] std::vector<std::string> previewAuditSchedule(std::size_t quota = 0) const;

    /// Non-mutating prospective schedule preview with a custom step interval.
    /// @throws std::invalid_argument if stepInterval == 0.
    [[nodiscard]] std::vector<std::string> previewAuditSchedule(std::size_t quota,
                                                                std::size_t stepInterval) const;

    /// Performs audit selection using the configured step interval.
    /// Removes selected targets from the active cycle and records audit history.
    /// @param quota Number of targets to select (0 = select all).
    /// @return Selected target IDs in their actual elimination order.
    /// @throws std::invalid_argument if stepInterval == 0.
    /// @throws std::overflow_error if round or sequence counter overflows.
    std::vector<std::string> performAuditSelection(std::size_t quota = 0);

    /// Performs audit selection using an explicit step interval.
    /// @throws std::invalid_argument if stepInterval == 0.
    /// @throws std::overflow_error if round or sequence counter overflows.
    std::vector<std::string> performAuditSelection(std::size_t quota, std::size_t stepInterval);

    /// Returns read-only access to complete historical audit records.
    [[nodiscard]] const std::vector<AuditRecord>& getAuditHistory() const noexcept;

    /// Returns the total number of audit rounds completed.
    [[nodiscard]] std::size_t getCurrentRoundNumber() const noexcept;

    /// Returns the next global sequence number to be assigned.
    [[nodiscard]] std::size_t getNextSequenceNumber() const noexcept;

    /// Clears active targets while preserving historical records and counters.
    void clearTargets() noexcept;

    /// Clears historical records while preserving active targets and round/sequence counters.
    void clearHistory() noexcept;

    /// Resets both active targets and history, as well as round/sequence counters.
    void resetAll() noexcept;

    /// Structural verification of the internal circular list.
    [[nodiscard]] bool verifyInvariants() const noexcept;
};
