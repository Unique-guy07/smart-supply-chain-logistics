#include "order/AuditCycle.h"
#include <cctype>
#include <limits>
#include <stdexcept>
#include <utility>

bool AuditCycle::isBlank(std::string_view str) noexcept
{
    if (str.empty()) {
        return true;
    }
    for (char ch : str) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    return true;
}

AuditCycle::AuditCycle(std::size_t defaultStepInterval)
    : m_stepInterval(defaultStepInterval)
{
    if (defaultStepInterval == 0) {
        throw std::invalid_argument("AuditCycle default step interval must be >= 1");
    }
}

bool AuditCycle::addTarget(const std::string& targetId)
{
    if (isBlank(targetId)) {
        return false;
    }
    if (containsTarget(targetId)) {
        return false;
    }
    m_targets.pushBack(targetId);
    return true;
}

bool AuditCycle::removeTarget(const std::string& targetId)
{
    if (isBlank(targetId)) {
        return false;
    }
    return m_targets.remove(targetId);
}

bool AuditCycle::containsTarget(const std::string& targetId) const noexcept
{
    return m_targets.contains(targetId);
}

std::size_t AuditCycle::getTargetCount() const noexcept
{
    return m_targets.getSize();
}

bool AuditCycle::isEmpty() const noexcept
{
    return m_targets.isEmpty();
}

std::vector<std::string> AuditCycle::getActiveTargets() const
{
    return m_targets.toVector();
}

void AuditCycle::setStepInterval(std::size_t stepInterval)
{
    if (stepInterval == 0) {
        throw std::invalid_argument("AuditCycle step interval must be >= 1");
    }
    m_stepInterval = stepInterval;
}

std::size_t AuditCycle::getStepInterval() const noexcept
{
    return m_stepInterval;
}

std::vector<std::string> AuditCycle::previewAuditSchedule(std::size_t quota) const
{
    return previewAuditSchedule(quota, m_stepInterval);
}

std::vector<std::string> AuditCycle::previewAuditSchedule(std::size_t quota,
                                                          std::size_t stepInterval) const
{
    if (stepInterval == 0) {
        throw std::invalid_argument("AuditCycle step interval must be >= 1");
    }

    if (m_targets.isEmpty()) {
        return {};
    }

    std::vector<std::string> pool = m_targets.toVector();
    const std::size_t effectiveQuota = (quota == 0 || quota > pool.size()) ? pool.size() : quota;

    std::vector<std::string> schedule;
    schedule.reserve(effectiveQuota);

    std::size_t currIdx = 0;
    for (std::size_t q = 0; q < effectiveQuota; ++q) {
        if (currIdx >= pool.size()) {
            currIdx = 0;
        }

        const std::size_t offset = (stepInterval - 1) % pool.size();
        const std::size_t remainingInCycle = pool.size() - currIdx;

        if (offset < remainingInCycle) {
            currIdx += offset;
        } else {
            currIdx = offset - remainingInCycle;
        }

        schedule.push_back(pool[currIdx]);
        pool.erase(pool.begin() + static_cast<std::ptrdiff_t>(currIdx));
    }

    return schedule;
}

std::vector<std::string> AuditCycle::performAuditSelection(std::size_t quota)
{
    return performAuditSelection(quota, m_stepInterval);
}

std::vector<std::string> AuditCycle::performAuditSelection(std::size_t quota,
                                                          std::size_t stepInterval)
{
    if (stepInterval == 0) {
        throw std::invalid_argument("AuditCycle step interval must be >= 1");
    }

    if (m_targets.isEmpty()) {
        return {};
    }

    const std::size_t effectiveQuota = (quota == 0 || quota > m_targets.getSize())
                                           ? m_targets.getSize()
                                           : quota;

    if (effectiveQuota == 0) {
        return {};
    }

    constexpr std::size_t maxLimit = std::numeric_limits<std::size_t>::max();

    // Round-number overflow check: reject if current round counter is already at max limit
    if (m_currentRoundNumber == maxLimit) {
        throw std::overflow_error("AuditCycle round number overflow: maximum representable round reached");
    }

    // Phase 1: Preparation (Two-Phase Commit)
    // Compute preview to stage historical records without mutating internal state
    std::vector<std::string> stagedSelection = previewAuditSchedule(effectiveQuota, stepInterval);
    if (stagedSelection.empty()) {
        return {};
    }

    const std::size_t selectionCount = stagedSelection.size();

    // Sequence-number overflow check: reject invalid zero next-sequence or selection exceeding maxLimit
    if (m_nextSequenceNumber == 0) {
        throw std::overflow_error("AuditCycle sequence counter is invalid or exhausted (0)");
    }

    if (selectionCount - 1 > maxLimit - m_nextSequenceNumber) {
        throw std::overflow_error("AuditCycle sequence number overflow: selection exceeds maximum representable sequence number");
    }

    std::vector<AuditRecord> stagedRecords;
    stagedRecords.reserve(selectionCount);
    const std::size_t nextRound = m_currentRoundNumber + 1;
    std::size_t seq = m_nextSequenceNumber;

    for (const auto& targetId : stagedSelection) {
        stagedRecords.push_back(AuditRecord{
            .sequenceNumber = seq,
            .roundNumber = nextRound,
            .targetId = targetId,
            .stepIntervalUsed = stepInterval
        });
        if (seq == maxLimit) {
            seq = 0; // Exhausted marker for subsequent selection attempts
        } else {
            ++seq;
        }
    }

    // Pre-reserve history capacity so appending cannot throw std::bad_alloc
    m_history.reserve(m_history.size() + stagedRecords.size());

    // Phase 2: Commit (No-fail mutation phase)
    std::vector<std::string> actualEliminated = m_targets.executeJosephus(stepInterval, effectiveQuota);

    for (auto& record : stagedRecords) {
        m_history.push_back(std::move(record));
    }

    m_nextSequenceNumber = seq;
    m_currentRoundNumber = nextRound;

    return actualEliminated;
}

const std::vector<AuditRecord>& AuditCycle::getAuditHistory() const noexcept
{
    return m_history;
}

std::size_t AuditCycle::getCurrentRoundNumber() const noexcept
{
    return m_currentRoundNumber;
}

std::size_t AuditCycle::getNextSequenceNumber() const noexcept
{
    return m_nextSequenceNumber;
}

void AuditCycle::clearTargets() noexcept
{
    m_targets.clear();
}

void AuditCycle::clearHistory() noexcept
{
    m_history.clear();
}

void AuditCycle::resetAll() noexcept
{
    m_targets.clear();
    m_history.clear();
    m_currentRoundNumber = 0;
    m_nextSequenceNumber = 1;
}

bool AuditCycle::verifyInvariants() const noexcept
{
    return m_targets.verifyInvariants();
}
