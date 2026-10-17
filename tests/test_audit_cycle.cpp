#include "test_utils.h"
#include "order/AuditCycle.h"
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// T1: Target Registration and Validation
// ---------------------------------------------------------------------------
void testTargetRegistration()
{
    AuditCycle cycle(3);
    TEST_CHECK(cycle.isEmpty());
    TEST_CHECK(cycle.getTargetCount() == 0);
    TEST_CHECK(cycle.verifyInvariants());

    // Valid registration
    TEST_CHECK(cycle.addTarget("ORD-101"));
    TEST_CHECK(cycle.addTarget("BIN-A01"));
    TEST_CHECK(cycle.addTarget("ORD-102"));
    TEST_CHECK(cycle.getTargetCount() == 3);
    TEST_CHECK(!cycle.isEmpty());

    // Membership check
    TEST_CHECK(cycle.containsTarget("ORD-101"));
    TEST_CHECK(cycle.containsTarget("BIN-A01"));
    TEST_CHECK(!cycle.containsTarget("ORD-999"));

    // Rejection of blank / whitespace IDs
    TEST_CHECK(!cycle.addTarget(""));
    TEST_CHECK(!cycle.addTarget("   "));
    TEST_CHECK(!cycle.addTarget("\t\n"));
    TEST_CHECK(cycle.getTargetCount() == 3);

    // Rejection of duplicates
    TEST_CHECK(!cycle.addTarget("ORD-101"));
    TEST_CHECK(!cycle.addTarget("BIN-A01"));
    TEST_CHECK(cycle.getTargetCount() == 3);

    // Active target retrieval
    std::vector<std::string> expected = {"ORD-101", "BIN-A01", "ORD-102"};
    TEST_CHECK(cycle.getActiveTargets() == expected);
    TEST_CHECK(cycle.verifyInvariants());
}

// ---------------------------------------------------------------------------
// T2: Target Removal
// ---------------------------------------------------------------------------
void testTargetRemoval()
{
    AuditCycle cycle;
    cycle.addTarget("T1");
    cycle.addTarget("T2");
    cycle.addTarget("T3");

    // Remove existing target
    TEST_CHECK(cycle.removeTarget("T2"));
    TEST_CHECK(cycle.getTargetCount() == 2);
    TEST_CHECK(!cycle.containsTarget("T2"));
    TEST_CHECK(cycle.verifyInvariants());
    TEST_CHECK(cycle.getActiveTargets() == (std::vector<std::string>{"T1", "T3"}));

    // Remove non-existent target
    TEST_CHECK(!cycle.removeTarget("T999"));
    TEST_CHECK(!cycle.removeTarget(""));
    TEST_CHECK(cycle.getTargetCount() == 2);

    // Clear targets
    cycle.clearTargets();
    TEST_CHECK(cycle.isEmpty());
    TEST_CHECK(cycle.getTargetCount() == 0);
    TEST_CHECK(cycle.verifyInvariants());
}

// ---------------------------------------------------------------------------
// T3: Step Interval Configuration and Validation
// ---------------------------------------------------------------------------
void testStepIntervalConfig()
{
    AuditCycle cycle(4);
    TEST_CHECK(cycle.getStepInterval() == 4);

    cycle.setStepInterval(7);
    TEST_CHECK(cycle.getStepInterval() == 7);

    // Invalid interval 0 in constructor throws
    bool ctorCaught = false;
    try {
        AuditCycle invalidCycle(0);
    } catch (const std::invalid_argument&) {
        ctorCaught = true;
    }
    TEST_CHECK(ctorCaught);

    // Invalid interval 0 in setter throws
    bool setterCaught = false;
    try {
        cycle.setStepInterval(0);
    } catch (const std::invalid_argument&) {
        setterCaught = true;
    }
    TEST_CHECK(setterCaught);
    TEST_CHECK(cycle.getStepInterval() == 7);
}

// ---------------------------------------------------------------------------
// T4: Non-Mutating Schedule Preview
// ---------------------------------------------------------------------------
void testSchedulePreview()
{
    AuditCycle cycle(3);
    for (int i = 1; i <= 7; ++i) {
        cycle.addTarget("ORD-" + std::to_string(i));
    }

    // Preview all 7 targets with default step interval k=3
    // Order: 3, 6, 2, 7, 5, 1, 4
    std::vector<std::string> preview = cycle.previewAuditSchedule();
    std::vector<std::string> expected = {
        "ORD-3", "ORD-6", "ORD-2", "ORD-7", "ORD-5", "ORD-1", "ORD-4"
    };
    TEST_CHECK(preview == expected);

    // Verify non-mutating property
    TEST_CHECK(cycle.getTargetCount() == 7);
    TEST_CHECK(cycle.getCurrentRoundNumber() == 0);
    TEST_CHECK(cycle.getNextSequenceNumber() == 1);
    TEST_CHECK(cycle.getAuditHistory().empty());
    TEST_CHECK(cycle.verifyInvariants());

    // Partial preview (quota = 2)
    std::vector<std::string> partialPreview = cycle.previewAuditSchedule(2);
    std::vector<std::string> expectedPartial = {"ORD-3", "ORD-6"};
    TEST_CHECK(partialPreview == expectedPartial);
    TEST_CHECK(cycle.getTargetCount() == 7);

    // Preview with invalid step throws
    bool caught = false;
    try {
        [[maybe_unused]] auto res = cycle.previewAuditSchedule(2, 0);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    TEST_CHECK(caught);
}

// ---------------------------------------------------------------------------
// T5: Full Audit Selection and History Tracking
// ---------------------------------------------------------------------------
void testFullAuditSelection()
{
    AuditCycle cycle(3);
    for (int i = 1; i <= 7; ++i) {
        cycle.addTarget("ORD-" + std::to_string(i));
    }

    // Execute full selection (quota = 0)
    std::vector<std::string> selected = cycle.performAuditSelection(0);
    std::vector<std::string> expected = {
        "ORD-3", "ORD-6", "ORD-2", "ORD-7", "ORD-5", "ORD-1", "ORD-4"
    };
    TEST_CHECK(selected == expected);

    // Active targets now empty
    TEST_CHECK(cycle.isEmpty());
    TEST_CHECK(cycle.getTargetCount() == 0);
    TEST_CHECK(cycle.verifyInvariants());

    // Verify round and sequence numbers
    TEST_CHECK(cycle.getCurrentRoundNumber() == 1);
    TEST_CHECK(cycle.getNextSequenceNumber() == 8);

    // Verify history records
    const auto& history = cycle.getAuditHistory();
    TEST_CHECK(history.size() == 7);

    for (std::size_t i = 0; i < 7; ++i) {
        TEST_CHECK(history[i].sequenceNumber == i + 1);
        TEST_CHECK(history[i].roundNumber == 1);
        TEST_CHECK(history[i].targetId == expected[i]);
        TEST_CHECK(history[i].stepIntervalUsed == 3);
    }
}

// ---------------------------------------------------------------------------
// T6: Partial Selection, Continuous Replenishment, and Multi-Round History
// ---------------------------------------------------------------------------
void testPartialSelectionAndReplenishment()
{
    AuditCycle cycle(3);
    for (int i = 1; i <= 5; ++i) {
        cycle.addTarget("ITEM-" + std::to_string(i));
    }

    // Round 1: Select 2 targets (step = 2)
    // N=5, k=2: first two eliminations are ITEM-2, ITEM-4
    std::vector<std::string> round1 = cycle.performAuditSelection(2, 2);
    std::vector<std::string> expectedR1 = {"ITEM-2", "ITEM-4"};
    TEST_CHECK(round1 == expectedR1);
    TEST_CHECK(cycle.getTargetCount() == 3);
    TEST_CHECK(cycle.getCurrentRoundNumber() == 1);
    TEST_CHECK(cycle.getNextSequenceNumber() == 3);
    TEST_CHECK(cycle.verifyInvariants());

    // Survivors after Round 1: ITEM-5, ITEM-1, ITEM-3
    TEST_CHECK(cycle.getActiveTargets() == (std::vector<std::string>{"ITEM-5", "ITEM-1", "ITEM-3"}));

    // Replenishment: Add 2 new inbound items
    TEST_CHECK(cycle.addTarget("ITEM-6"));
    TEST_CHECK(cycle.addTarget("ITEM-7"));
    TEST_CHECK(cycle.getTargetCount() == 5);
    // Active targets: ITEM-5, ITEM-1, ITEM-3, ITEM-6, ITEM-7
    TEST_CHECK(cycle.getActiveTargets() ==
               (std::vector<std::string>{"ITEM-5", "ITEM-1", "ITEM-3", "ITEM-6", "ITEM-7"}));

    // Round 2: Select 3 targets with step = 3
    // Pool: [ITEM-5, ITEM-1, ITEM-3, ITEM-6, ITEM-7]
    // Simulation:
    // (0 + 3 - 1) % 5 = 2 -> ITEM-3
    // Pool: [ITEM-5, ITEM-1, ITEM-6, ITEM-7] (next start is ITEM-6 at idx 2)
    // (2 + 3 - 1) % 4 = 4 % 4 = 0 -> ITEM-5
    // Pool: [ITEM-1, ITEM-6, ITEM-7] (next start is ITEM-1 at idx 0)
    // (0 + 3 - 1) % 3 = 2 -> ITEM-7
    // Eliminated: ITEM-3, ITEM-5, ITEM-7
    std::vector<std::string> round2 = cycle.performAuditSelection(3, 3);
    std::vector<std::string> expectedR2 = {"ITEM-3", "ITEM-5", "ITEM-7"};
    TEST_CHECK(round2 == expectedR2);
    TEST_CHECK(cycle.getTargetCount() == 2);
    TEST_CHECK(cycle.getCurrentRoundNumber() == 2);
    TEST_CHECK(cycle.getNextSequenceNumber() == 6);
    TEST_CHECK(cycle.verifyInvariants());

    // Verify entire cumulative history
    const auto& history = cycle.getAuditHistory();
    TEST_CHECK(history.size() == 5);

    // Check round 1 records
    TEST_CHECK(history[0].sequenceNumber == 1 && history[0].roundNumber == 1 && history[0].targetId == "ITEM-2");
    TEST_CHECK(history[1].sequenceNumber == 2 && history[1].roundNumber == 1 && history[1].targetId == "ITEM-4");

    // Check round 2 records
    TEST_CHECK(history[2].sequenceNumber == 3 && history[2].roundNumber == 2 && history[2].targetId == "ITEM-3");
    TEST_CHECK(history[3].sequenceNumber == 4 && history[3].roundNumber == 2 && history[3].targetId == "ITEM-5");
    TEST_CHECK(history[4].sequenceNumber == 5 && history[4].roundNumber == 2 && history[4].targetId == "ITEM-7");
}

// ---------------------------------------------------------------------------
// T7: Empty Cycle and Quota Overshoot
// ---------------------------------------------------------------------------
void testEmptyCycleAndOvershoot()
{
    AuditCycle cycle(3);

    // Selection on empty returns empty and does NOT advance round or sequence
    std::vector<std::string> emptyRes = cycle.performAuditSelection(5);
    TEST_CHECK(emptyRes.empty());
    TEST_CHECK(cycle.getCurrentRoundNumber() == 0);
    TEST_CHECK(cycle.getNextSequenceNumber() == 1);
    TEST_CHECK(cycle.getAuditHistory().empty());

    // Add 2 targets
    cycle.addTarget("A");
    cycle.addTarget("B");

    // Quota overshoot (quota = 10 on a 2-element cycle) -> eliminates all 2
    std::vector<std::string> res = cycle.performAuditSelection(10, 1);
    TEST_CHECK(res == (std::vector<std::string>{"A", "B"}));
    TEST_CHECK(cycle.isEmpty());
    TEST_CHECK(cycle.getCurrentRoundNumber() == 1);
    TEST_CHECK(cycle.getNextSequenceNumber() == 3);
}

// ---------------------------------------------------------------------------
// T8: History Reset and State Preservation
// ---------------------------------------------------------------------------
void testResetAndClear()
{
    AuditCycle cycle;
    cycle.addTarget("X");
    cycle.addTarget("Y");
    cycle.performAuditSelection(1);

    TEST_CHECK(cycle.getAuditHistory().size() == 1);
    TEST_CHECK(cycle.getTargetCount() == 1);

    // clearHistory clears records but preserves round/seq and active targets
    cycle.clearHistory();
    TEST_CHECK(cycle.getAuditHistory().empty());
    TEST_CHECK(cycle.getTargetCount() == 1);
    TEST_CHECK(cycle.getCurrentRoundNumber() == 1);
    TEST_CHECK(cycle.getNextSequenceNumber() == 2);

    // resetAll clears everything and resets counters
    cycle.resetAll();
    TEST_CHECK(cycle.isEmpty());
    TEST_CHECK(cycle.getAuditHistory().empty());
    TEST_CHECK(cycle.getCurrentRoundNumber() == 0);
    TEST_CHECK(cycle.getNextSequenceNumber() == 1);
}

// ---------------------------------------------------------------------------
// Narrowly Scoped Test Accessor Seam for Boundary Verification
// ---------------------------------------------------------------------------
struct AuditCycleTestAccess
{
    static void setNextSequenceNumber(AuditCycle& cycle, std::size_t seq) noexcept
    {
        cycle.m_nextSequenceNumber = seq;
    }

    static void setCurrentRoundNumber(AuditCycle& cycle, std::size_t round) noexcept
    {
        cycle.m_currentRoundNumber = round;
    }
};

// ---------------------------------------------------------------------------
// T9: Round-Number Overflow Rejection and State Invariance
// ---------------------------------------------------------------------------
void testRoundNumberOverflowRejection()
{
    AuditCycle cycle;
    cycle.addTarget("T1");
    cycle.addTarget("T2");
    TEST_CHECK(cycle.getTargetCount() == 2);

    constexpr std::size_t maxLimit = std::numeric_limits<std::size_t>::max();
    AuditCycleTestAccess::setCurrentRoundNumber(cycle, maxLimit);
    TEST_CHECK(cycle.getCurrentRoundNumber() == maxLimit);

    // Non-empty selection when round counter is at maxLimit must throw std::overflow_error
    bool caught = false;
    try {
        [[maybe_unused]] auto res = cycle.performAuditSelection(1);
    } catch (const std::overflow_error&) {
        caught = true;
    }
    TEST_CHECK(caught);

    // Verify strong exception safety: active targets, history, round, and sequence remain intact
    TEST_CHECK(cycle.getTargetCount() == 2);
    TEST_CHECK(cycle.containsTarget("T1"));
    TEST_CHECK(cycle.containsTarget("T2"));
    TEST_CHECK(cycle.getAuditHistory().empty());
    TEST_CHECK(cycle.getCurrentRoundNumber() == maxLimit);
    TEST_CHECK(cycle.getNextSequenceNumber() == 1);
    TEST_CHECK(cycle.verifyInvariants());

    // Empty/no-op cycle does not throw even when round counter is at maxLimit
    AuditCycle emptyCycle;
    AuditCycleTestAccess::setCurrentRoundNumber(emptyCycle, maxLimit);
    std::vector<std::string> emptyRes = emptyCycle.performAuditSelection(1);
    TEST_CHECK(emptyRes.empty());
    TEST_CHECK(emptyCycle.getCurrentRoundNumber() == maxLimit);
}

// ---------------------------------------------------------------------------
// T10: Sequence-Number Overflow Rejection and State Invariance
// ---------------------------------------------------------------------------
void testSequenceNumberOverflowRejection()
{
    AuditCycle cycle;
    cycle.addTarget("A");
    cycle.addTarget("B");
    TEST_CHECK(cycle.getTargetCount() == 2);

    constexpr std::size_t maxLimit = std::numeric_limits<std::size_t>::max();

    // Case A: Sequence counter at maxLimit and selecting 2 targets (would exceed maxLimit)
    AuditCycleTestAccess::setNextSequenceNumber(cycle, maxLimit);
    TEST_CHECK(cycle.getNextSequenceNumber() == maxLimit);

    bool caught = false;
    try {
        [[maybe_unused]] auto res = cycle.performAuditSelection(2);
    } catch (const std::overflow_error&) {
        caught = true;
    }
    TEST_CHECK(caught);

    // Verify all state is preserved unaltered
    TEST_CHECK(cycle.getTargetCount() == 2);
    TEST_CHECK(cycle.containsTarget("A"));
    TEST_CHECK(cycle.containsTarget("B"));
    TEST_CHECK(cycle.getAuditHistory().empty());
    TEST_CHECK(cycle.getNextSequenceNumber() == maxLimit);
    TEST_CHECK(cycle.getCurrentRoundNumber() == 0);
    TEST_CHECK(cycle.verifyInvariants());

    // Case B: Sequence counter exhausted or invalid (0)
    AuditCycleTestAccess::setNextSequenceNumber(cycle, 0);
    caught = false;
    try {
        [[maybe_unused]] auto res = cycle.performAuditSelection(1);
    } catch (const std::overflow_error&) {
        caught = true;
    }
    TEST_CHECK(caught);

    // State still intact
    TEST_CHECK(cycle.getTargetCount() == 2);
    TEST_CHECK(cycle.getAuditHistory().empty());

    // Case C: Empty cycle does not throw even if next sequence is invalid or maxLimit
    AuditCycle emptyCycle;
    AuditCycleTestAccess::setNextSequenceNumber(emptyCycle, maxLimit);
    std::vector<std::string> emptyRes = emptyCycle.performAuditSelection(5);
    TEST_CHECK(emptyRes.empty());
    TEST_CHECK(emptyCycle.getNextSequenceNumber() == maxLimit);

    AuditCycleTestAccess::setNextSequenceNumber(emptyCycle, 0);
    emptyRes = emptyCycle.performAuditSelection(5);
    TEST_CHECK(emptyRes.empty());
    TEST_CHECK(emptyCycle.getNextSequenceNumber() == 0);
}

// ---------------------------------------------------------------------------
// T11: Boundary Transition to Limit and Reset Recovery
// ---------------------------------------------------------------------------
void testSequenceBoundaryTransitionAndRecovery()
{
    AuditCycle cycle;
    cycle.addTarget("LIMIT_ITEM");
    TEST_CHECK(cycle.getTargetCount() == 1);

    constexpr std::size_t maxLimit = std::numeric_limits<std::size_t>::max();
    AuditCycleTestAccess::setNextSequenceNumber(cycle, maxLimit);

    // Selecting 1 target when nextSequence == maxLimit must succeed:
    // selectionCount - 1 = 0 <= maxLimit - maxLimit = 0
    std::vector<std::string> res = cycle.performAuditSelection(1);
    TEST_CHECK(res == (std::vector<std::string>{"LIMIT_ITEM"}));
    TEST_CHECK(cycle.isEmpty());
    TEST_CHECK(cycle.getCurrentRoundNumber() == 1);

    // History contains record with exactly maxLimit as sequence number
    const auto& history = cycle.getAuditHistory();
    TEST_CHECK(history.size() == 1);
    TEST_CHECK(history[0].sequenceNumber == maxLimit);
    TEST_CHECK(history[0].roundNumber == 1);
    TEST_CHECK(history[0].targetId == "LIMIT_ITEM");

    // Sequence counter transitioned to 0 (exhausted indicator)
    TEST_CHECK(cycle.getNextSequenceNumber() == 0);

    // Next selection attempt fails safely because counter is exhausted
    cycle.addTarget("AFTER_LIMIT");
    bool caught = false;
    try {
        [[maybe_unused]] auto failRes = cycle.performAuditSelection(1);
    } catch (const std::overflow_error&) {
        caught = true;
    }
    TEST_CHECK(caught);
    TEST_CHECK(cycle.containsTarget("AFTER_LIMIT"));

    // Resetting cycle via resetAll() restores clean state and counters
    cycle.resetAll();
    TEST_CHECK(cycle.isEmpty());
    TEST_CHECK(cycle.getAuditHistory().empty());
    TEST_CHECK(cycle.getCurrentRoundNumber() == 0);
    TEST_CHECK(cycle.getNextSequenceNumber() == 1);

    // Normal selection works correctly post-reset
    cycle.addTarget("NORMAL_ITEM");
    std::vector<std::string> normalRes = cycle.performAuditSelection(1);
    TEST_CHECK(normalRes == (std::vector<std::string>{"NORMAL_ITEM"}));
    TEST_CHECK(cycle.getCurrentRoundNumber() == 1);
    TEST_CHECK(cycle.getNextSequenceNumber() == 2);
    TEST_CHECK(cycle.getAuditHistory().size() == 1);
    TEST_CHECK(cycle.getAuditHistory()[0].sequenceNumber == 1);
}

// ---------------------------------------------------------------------------
// T12: Maximum Step Interval Preview Agreement and Invariants
// ---------------------------------------------------------------------------
void testMaximumStepIntervalPreviewAgreement()
{
    AuditCycle cycle;
    cycle.addTarget("A");
    cycle.addTarget("B");
    cycle.addTarget("C");
    cycle.addTarget("D");
    cycle.addTarget("E");
    TEST_CHECK(cycle.getTargetCount() == 5);

    constexpr std::size_t maxStep = std::numeric_limits<std::size_t>::max();
    cycle.setStepInterval(maxStep);
    TEST_CHECK(cycle.getStepInterval() == maxStep);

    // 1. Preview complete schedule with maxStep without arithmetic overflow
    std::vector<std::string> preview = cycle.previewAuditSchedule(0, maxStep);
    // Mathematical elimination order for N=5, k=maxStep:
    // (maxStep - 1) % 5 = 4 -> E
    // (maxStep - 1) % 4 = 2 -> C
    // (maxStep - 1) % 3 = 2 -> B
    // (maxStep - 1) % 2 = 0 -> D
    // (maxStep - 1) % 1 = 0 -> A
    std::vector<std::string> expectedOrder = {"E", "C", "B", "D", "A"};
    TEST_CHECK(preview == expectedOrder);

    // Verify non-mutating preview invariants
    TEST_CHECK(cycle.getTargetCount() == 5);
    TEST_CHECK(cycle.verifyInvariants());
    TEST_CHECK(cycle.getAuditHistory().empty());

    // 2. Actual selection using maxStep
    std::vector<std::string> actual = cycle.performAuditSelection(0, maxStep);

    // 3. Preview order must match actual selected order exactly
    TEST_CHECK(actual == preview);
    TEST_CHECK(actual == expectedOrder);

    // 4. Audit history must match selected order exactly
    const auto& history = cycle.getAuditHistory();
    TEST_CHECK(history.size() == 5);
    for (std::size_t i = 0; i < 5; ++i) {
        TEST_CHECK(history[i].sequenceNumber == i + 1);
        TEST_CHECK(history[i].roundNumber == 1);
        TEST_CHECK(history[i].targetId == expectedOrder[i]);
        TEST_CHECK(history[i].stepIntervalUsed == maxStep);
    }

    // 5. Active-target and circular-list invariants remain correct
    TEST_CHECK(cycle.isEmpty());
    TEST_CHECK(cycle.getTargetCount() == 0);
    TEST_CHECK(cycle.verifyInvariants());

    // 6. Verify partial selection with maxStep on a replenished cycle
    cycle.addTarget("X1");
    cycle.addTarget("X2");
    cycle.addTarget("X3");
    cycle.addTarget("X4");

    // Preview quota = 2
    std::vector<std::string> partialPreview = cycle.previewAuditSchedule(2, maxStep);
    std::vector<std::string> partialActual = cycle.performAuditSelection(2, maxStep);
    TEST_CHECK(partialActual == partialPreview);
    TEST_CHECK(partialActual.size() == 2);
    TEST_CHECK(cycle.getTargetCount() == 2);
    TEST_CHECK(cycle.verifyInvariants());
}

int main()
{
    TEST_RUN(testTargetRegistration);
    TEST_RUN(testTargetRemoval);
    TEST_RUN(testStepIntervalConfig);
    TEST_RUN(testSchedulePreview);
    TEST_RUN(testFullAuditSelection);
    TEST_RUN(testPartialSelectionAndReplenishment);
    TEST_RUN(testEmptyCycleAndOvershoot);
    TEST_RUN(testResetAndClear);
    TEST_RUN(testRoundNumberOverflowRejection);
    TEST_RUN(testSequenceNumberOverflowRejection);
    TEST_RUN(testSequenceBoundaryTransitionAndRecovery);
    TEST_RUN(testMaximumStepIntervalPreviewAgreement);
    TEST_REPORT();
}

