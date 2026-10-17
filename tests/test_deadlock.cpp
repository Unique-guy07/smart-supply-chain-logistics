#include "test_utils.h"
#include "graph/WaitForGraph.h"
#include "task/PriorityTask.h"

#include <stdexcept>
#include <string>
#include <vector>

void testEmptyGraphNoDeadlock()
{
    WaitForGraph wfg;
    TEST_CHECK(wfg.getAgvCount() == 0);
    TEST_CHECK(wfg.getDependencyCount() == 0);
    TEST_CHECK(wfg.getAllAgvIds().empty());

    DeadlockCycle deadlock = wfg.detectDeadlock();
    TEST_CHECK(!deadlock.hasDeadlock);
    TEST_CHECK(deadlock.cycle.empty());
    TEST_CHECK(deadlock.cycleIndices.empty());
}

void testSingleAgvNoDeadlock()
{
    WaitForGraph wfg;
    const std::size_t idx = wfg.addAgv("AGV_01", TaskPriority::Normal);
    TEST_CHECK(idx == 0);
    TEST_CHECK(wfg.getAgvCount() == 1);
    TEST_CHECK(wfg.hasAgv("AGV_01"));
    TEST_CHECK(wfg.hasAgv(0));
    TEST_CHECK(!wfg.hasAgv(1));
    TEST_CHECK(wfg.getAgvId(0) == "AGV_01");
    TEST_CHECK(wfg.getAgvIndex("AGV_01") == 0);
    TEST_CHECK(wfg.getAgvPriority("AGV_01") == TaskPriority::Normal);
    TEST_CHECK(wfg.getDependencyCount() == 0);

    DeadlockCycle deadlock = wfg.detectDeadlock();
    TEST_CHECK(!deadlock.hasDeadlock);
    TEST_CHECK(deadlock.cycle.empty());
    TEST_CHECK(deadlock.cycleIndices.empty());
}

void testAcyclicChainNoDeadlock()
{
    WaitForGraph wfg;
    wfg.addAgv("AGV_A", TaskPriority::Low);
    wfg.addAgv("AGV_B", TaskPriority::Normal);
    wfg.addAgv("AGV_C", TaskPriority::High);
    wfg.addAgv("AGV_D", TaskPriority::Urgent);

    // Chain: A -> B -> C -> D (A waits for B, B waits for C, C waits for D)
    wfg.addDependency("AGV_A", "AGV_B");
    wfg.addDependency("AGV_B", "AGV_C");
    wfg.addDependency("AGV_C", "AGV_D");

    TEST_CHECK(wfg.getDependencyCount() == 3);
    TEST_CHECK(wfg.hasDependency("AGV_A", "AGV_B"));
    TEST_CHECK(wfg.hasDependency("AGV_B", "AGV_C"));
    TEST_CHECK(wfg.hasDependency("AGV_C", "AGV_D"));
    TEST_CHECK(!wfg.hasDependency("AGV_D", "AGV_A"));

    DeadlockCycle deadlock = wfg.detectDeadlock();
    TEST_CHECK(!deadlock.hasDeadlock);
    TEST_CHECK(deadlock.cycle.empty());
    TEST_CHECK(deadlock.cycleIndices.empty());
}

void testBranchingAcyclicGraphNoDeadlock()
{
    WaitForGraph wfg;
    wfg.addAgv("A");
    wfg.addAgv("B");
    wfg.addAgv("C");
    wfg.addAgv("D");
    wfg.addAgv("E");

    // Branching DAG: A -> B, A -> C, B -> D, C -> D, D -> E
    wfg.addDependency("A", "B");
    wfg.addDependency("A", "C");
    wfg.addDependency("B", "D");
    wfg.addDependency("C", "D");
    wfg.addDependency("D", "E");

    TEST_CHECK(wfg.getDependencyCount() == 5);
    auto aDeps = wfg.getDependencies("A");
    TEST_CHECK(aDeps.size() == 2);

    DeadlockCycle deadlock = wfg.detectDeadlock();
    TEST_CHECK(!deadlock.hasDeadlock);
    TEST_CHECK(deadlock.cycle.empty());
    TEST_CHECK(deadlock.cycleIndices.empty());
}

void testTwoAgvDirectedCycle()
{
    WaitForGraph wfg;
    wfg.addAgv("AGV_A", TaskPriority::Normal);
    wfg.addAgv("AGV_B", TaskPriority::High);

    // Mutual wait: A -> B and B -> A
    wfg.addDependency("AGV_A", "AGV_B");
    wfg.addDependency("AGV_B", "AGV_A");

    DeadlockCycle deadlock = wfg.detectDeadlock();
    TEST_CHECK(deadlock.hasDeadlock);
    TEST_CHECK(deadlock.cycle.size() == 3);
    TEST_CHECK(deadlock.cycle.front() == deadlock.cycle.back());
    TEST_CHECK(deadlock.cycle[0] == "AGV_A");
    TEST_CHECK(deadlock.cycle[1] == "AGV_B");
    TEST_CHECK(deadlock.cycle[2] == "AGV_A");

    // Verify all steps are valid directed dependencies
    TEST_CHECK(wfg.hasDependency(deadlock.cycle[0], deadlock.cycle[1]));
    TEST_CHECK(wfg.hasDependency(deadlock.cycle[1], deadlock.cycle[2]));
}

void testThreeOrMoreAgvDirectedCycle()
{
    WaitForGraph wfg;
    wfg.addAgv("AGV_1");
    wfg.addAgv("AGV_2");
    wfg.addAgv("AGV_3");
    wfg.addAgv("AGV_4");

    // Directed cycle of length 4: 1 -> 2 -> 3 -> 4 -> 1
    wfg.addDependency("AGV_1", "AGV_2");
    wfg.addDependency("AGV_2", "AGV_3");
    wfg.addDependency("AGV_3", "AGV_4");
    wfg.addDependency("AGV_4", "AGV_1");

    DeadlockCycle deadlock = wfg.detectDeadlock();
    TEST_CHECK(deadlock.hasDeadlock);
    TEST_CHECK(deadlock.cycle.size() == 5);
    TEST_CHECK(deadlock.cycle.front() == deadlock.cycle.back());

    for (std::size_t i = 0; i + 1 < deadlock.cycle.size(); ++i) {
        TEST_CHECK(wfg.hasDependency(deadlock.cycle[i], deadlock.cycle[i + 1]));
    }
}

void testDisconnectedComponentsCycleInOnlyOne()
{
    WaitForGraph wfg;
    // Component 1: Acyclic (A -> B -> C)
    wfg.addAgv("A");
    wfg.addAgv("B");
    wfg.addAgv("C");
    wfg.addDependency("A", "B");
    wfg.addDependency("B", "C");

    // Component 2: Cyclic (D -> E -> D)
    wfg.addAgv("D");
    wfg.addAgv("E");
    wfg.addDependency("D", "E");
    wfg.addDependency("E", "D");

    // Component 3: Isolated (F)
    wfg.addAgv("F");

    DeadlockCycle deadlock = wfg.detectDeadlock();
    TEST_CHECK(deadlock.hasDeadlock);
    TEST_CHECK(deadlock.cycle.size() == 3);
    TEST_CHECK(deadlock.cycle.front() == deadlock.cycle.back());
    TEST_CHECK(deadlock.cycle[0] == "D");
    TEST_CHECK(deadlock.cycle[1] == "E");
    TEST_CHECK(deadlock.cycle[2] == "D");

    for (std::size_t i = 0; i + 1 < deadlock.cycle.size(); ++i) {
        TEST_CHECK(wfg.hasDependency(deadlock.cycle[i], deadlock.cycle[i + 1]));
    }
}

void testMultipleCyclesDeterministicSelection()
{
    WaitForGraph wfg;
    // Component 1: Cycle on AGV_01 and AGV_02
    wfg.addAgv("AGV_01");
    wfg.addAgv("AGV_02");
    wfg.addDependency("AGV_01", "AGV_02");
    wfg.addDependency("AGV_02", "AGV_01");

    // Component 2: Cycle on AGV_03 and AGV_04
    wfg.addAgv("AGV_03");
    wfg.addAgv("AGV_04");
    wfg.addDependency("AGV_03", "AGV_04");
    wfg.addDependency("AGV_04", "AGV_03");

    // Documented deterministic order: visits outer vertices by ascending index (0, 1, ...).
    // Vertex 0 (AGV_01) is explored first and its cycle must be returned.
    DeadlockCycle deadlock = wfg.detectDeadlock();
    TEST_CHECK(deadlock.hasDeadlock);
    TEST_CHECK(deadlock.cycle.size() == 3);
    TEST_CHECK(deadlock.cycle[0] == "AGV_01");
    TEST_CHECK(deadlock.cycle[1] == "AGV_02");
    TEST_CHECK(deadlock.cycle[2] == "AGV_01");
}

void testVictimSelectionLowestPriority()
{
    WaitForGraph wfg;
    wfg.addAgv("AGV_A", TaskPriority::High);
    wfg.addAgv("AGV_B", TaskPriority::Low);
    wfg.addAgv("AGV_C", TaskPriority::Normal);

    wfg.addDependency("AGV_A", "AGV_B");
    wfg.addDependency("AGV_B", "AGV_C");
    wfg.addDependency("AGV_C", "AGV_A");

    // Lowest priority in the cycle is TaskPriority::Low (AGV_B)
    std::string victim = wfg.selectVictim();
    TEST_CHECK(victim == "AGV_B");

    DeadlockCycle deadlock = wfg.detectDeadlock();
    TEST_CHECK(wfg.selectVictim(deadlock) == "AGV_B");
    TEST_CHECK(wfg.selectVictim(deadlock.cycle) == "AGV_B");
}

void testVictimSelectionEqualPriorityTieBreaker()
{
    WaitForGraph wfg;
    // Three AGVs with identical TaskPriority::Normal
    wfg.addAgv("AGV_Gamma", TaskPriority::Normal);
    wfg.addAgv("AGV_Alpha", TaskPriority::Normal);
    wfg.addAgv("AGV_Beta", TaskPriority::Normal);

    wfg.addDependency("AGV_Gamma", "AGV_Alpha");
    wfg.addDependency("AGV_Alpha", "AGV_Beta");
    wfg.addDependency("AGV_Beta", "AGV_Gamma");

    // Lexicographical tie-breaker: "AGV_Alpha" < "AGV_Beta" < "AGV_Gamma"
    std::string victim = wfg.selectVictim();
    TEST_CHECK(victim == "AGV_Alpha");
}

void testVictimSelectionAllCriticalCycle()
{
    WaitForGraph wfg;
    // Every AGV has TaskPriority::Critical
    wfg.addAgv("AGV_Z", TaskPriority::Critical);
    wfg.addAgv("AGV_M", TaskPriority::Critical);

    wfg.addDependency("AGV_Z", "AGV_M");
    wfg.addDependency("AGV_M", "AGV_Z");

    // All-Critical cycle: tie-breaker selects lexicographically smallest ID ("AGV_M")
    std::string victim = wfg.selectVictim();
    TEST_CHECK(victim == "AGV_M");
}

void testVictimSelectionIgnoresOutsideAgvWithLowerPriority()
{
    WaitForGraph wfg;
    // Outside AGV has Low priority (lowest in entire graph)
    wfg.addAgv("AGV_Outside", TaskPriority::Low);

    // Cycle AGVs have High and Normal priorities
    wfg.addAgv("AGV_Inside1", TaskPriority::High);
    wfg.addAgv("AGV_Inside2", TaskPriority::Normal);

    wfg.addDependency("AGV_Outside", "AGV_Inside1"); // Outside AGV waiting on inside AGV
    wfg.addDependency("AGV_Inside1", "AGV_Inside2");
    wfg.addDependency("AGV_Inside2", "AGV_Inside1"); // Deadlock between Inside1 and Inside2

    // Victim must be selected from the detected cycle (AGV_Inside2 has lowest priority in cycle)
    std::string victim = wfg.selectVictim();
    TEST_CHECK(victim == "AGV_Inside2");
    TEST_CHECK(victim != "AGV_Outside");
}

void testVictimSelectionNoDeadlockThrows()
{
    WaitForGraph wfg;
    wfg.addAgv("A");
    wfg.addAgv("B");
    wfg.addDependency("A", "B");

    bool threw = false;
    try {
        [[maybe_unused]] auto v = wfg.selectVictim();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Test with empty cycle vector
    threw = false;
    try {
        std::vector<std::string> emptyCycle;
        [[maybe_unused]] auto v = wfg.selectVictim(emptyCycle);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Test with non-deadlock DeadlockCycle struct
    threw = false;
    try {
        DeadlockCycle noDeadlock;
        [[maybe_unused]] auto v = wfg.selectVictim(noDeadlock);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    TEST_CHECK(threw);
}

void testDuplicateDependencyThrows()
{
    WaitForGraph wfg;
    wfg.addAgv("A");
    wfg.addAgv("B");

    wfg.addDependency("A", "B");
    TEST_CHECK(wfg.getDependencyCount() == 1);

    bool threw = false;
    try {
        wfg.addDependency("A", "B");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);
    TEST_CHECK(wfg.getDependencyCount() == 1);
}

void testRemovingDependencyBreaksCycle()
{
    WaitForGraph wfg;
    wfg.addAgv("A");
    wfg.addAgv("B");
    wfg.addAgv("C");

    wfg.addDependency("A", "B");
    wfg.addDependency("B", "C");
    wfg.addDependency("C", "A");

    TEST_CHECK(wfg.detectDeadlock().hasDeadlock);

    // Break the cycle
    bool removed = wfg.removeDependency("C", "A");
    TEST_CHECK(removed);
    TEST_CHECK(!wfg.hasDependency("C", "A"));
    TEST_CHECK(wfg.getDependencyCount() == 2);

    DeadlockCycle deadlock = wfg.detectDeadlock();
    TEST_CHECK(!deadlock.hasDeadlock);
    TEST_CHECK(deadlock.cycle.empty());
}

void testRemovingNonexistentDependency()
{
    WaitForGraph wfg;
    wfg.addAgv("A");
    wfg.addAgv("B");

    // Dependency A -> B does not exist
    bool removed = wfg.removeDependency("A", "B");
    TEST_CHECK(!removed);
    TEST_CHECK(wfg.getDependencyCount() == 0);
}

void testInvalidOrEmptyAgvIdentifiers()
{
    WaitForGraph wfg;

    // Empty identifier
    bool threw = false;
    try {
        wfg.addAgv("");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Whitespace-only identifier
    threw = false;
    try {
        wfg.addAgv("   \t  \n ");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Duplicate registration
    wfg.addAgv("A");
    threw = false;
    try {
        wfg.addAgv("A");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);
}

void testUnregisteredAgvDependencies()
{
    WaitForGraph wfg;
    wfg.addAgv("A");

    // From unknown
    bool threw = false;
    try {
        wfg.addDependency("UNKNOWN", "A");
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // To unknown
    threw = false;
    try {
        wfg.addDependency("A", "UNKNOWN");
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // removeDependency on unknown
    threw = false;
    try {
        wfg.removeDependency("A", "UNKNOWN");
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // hasDependency on unknown
    threw = false;
    try {
        [[maybe_unused]] auto h = wfg.hasDependency("A", "UNKNOWN");
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // getDependencies on unknown
    threw = false;
    try {
        [[maybe_unused]] auto d = wfg.getDependencies("UNKNOWN");
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // getAgvIndex on unknown
    threw = false;
    try {
        [[maybe_unused]] auto i = wfg.getAgvIndex("UNKNOWN");
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // getAgvId on out-of-bounds index
    threw = false;
    try {
        [[maybe_unused]] auto& id = wfg.getAgvId(99);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Priority operations on unknown
    threw = false;
    try {
        wfg.setAgvPriority("UNKNOWN", TaskPriority::Urgent);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    threw = false;
    try {
        [[maybe_unused]] auto p = wfg.getAgvPriority("UNKNOWN");
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Victim selection with unregistered AGV in supplied cycle
    threw = false;
    try {
        std::vector<std::string> invalidCycle = {"A", "NONEXISTENT", "A"};
        [[maybe_unused]] auto v = wfg.selectVictim(invalidCycle);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);
}

void testSelfDependencyBehavior()
{
    WaitForGraph wfg;
    wfg.addAgv("A");

    // Self dependency by ID throws invalid_argument
    bool threw = false;
    try {
        wfg.addDependency("A", "A");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Self dependency by index throws invalid_argument
    threw = false;
    try {
        wfg.addDependency(0, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    TEST_CHECK(wfg.getDependencyCount() == 0);
}

void testRepeatedDetectionDoesNotMutate()
{
    WaitForGraph wfg;
    wfg.addAgv("A", TaskPriority::Normal);
    wfg.addAgv("B", TaskPriority::High);
    wfg.addAgv("C", TaskPriority::Low);

    wfg.addDependency("A", "B");
    wfg.addDependency("B", "C");
    wfg.addDependency("C", "A");

    // Call detectDeadlock multiple times
    for (int i = 0; i < 5; ++i) {
        DeadlockCycle d = wfg.detectDeadlock();
        TEST_CHECK(d.hasDeadlock);
        TEST_CHECK(d.cycle.size() == 4);
        TEST_CHECK(d.cycle[0] == "A");
        TEST_CHECK(d.cycle[1] == "B");
        TEST_CHECK(d.cycle[2] == "C");
        TEST_CHECK(d.cycle[3] == "A");
    }

    // Graph state must remain intact
    TEST_CHECK(wfg.getAgvCount() == 3);
    TEST_CHECK(wfg.getDependencyCount() == 3);
    TEST_CHECK(wfg.hasDependency("A", "B"));
    TEST_CHECK(wfg.hasDependency("B", "C"));
    TEST_CHECK(wfg.hasDependency("C", "A"));
}

void testVictimSelectionDoesNotMutateGraph()
{
    WaitForGraph wfg;
    wfg.addAgv("A", TaskPriority::High);
    wfg.addAgv("B", TaskPriority::Low);
    wfg.addAgv("C", TaskPriority::Normal);

    wfg.addDependency("A", "B");
    wfg.addDependency("B", "C");
    wfg.addDependency("C", "A");

    const std::size_t agvCountBefore = wfg.getAgvCount();
    const std::size_t depCountBefore = wfg.getDependencyCount();

    std::string victim = wfg.selectVictim();
    TEST_CHECK(victim == "B");

    // Victim selection must NOT remove dependencies, delete AGVs, or resolve deadlock
    TEST_CHECK(wfg.getAgvCount() == agvCountBefore);
    TEST_CHECK(wfg.getDependencyCount() == depCountBefore);
    TEST_CHECK(wfg.hasDependency("A", "B"));
    TEST_CHECK(wfg.hasDependency("B", "C"));
    TEST_CHECK(wfg.hasDependency("C", "A"));
    TEST_CHECK(wfg.detectDeadlock().hasDeadlock);
}

void testMoveSemanticsAndClear()
{
    WaitForGraph wfg;
    wfg.addAgv("A", TaskPriority::High);
    wfg.addAgv("B", TaskPriority::Low);
    wfg.addDependency("A", "B");

    // Test clearDependencies
    wfg.clearDependencies();
    TEST_CHECK(wfg.getDependencyCount() == 0);
    TEST_CHECK(wfg.getAgvCount() == 2);
    TEST_CHECK(!wfg.hasDependency("A", "B"));

    // Re-add dependency and test move constructor
    wfg.addDependency("A", "B");
    WaitForGraph moved(std::move(wfg));
    TEST_CHECK(moved.getAgvCount() == 2);
    TEST_CHECK(moved.getDependencyCount() == 1);
    TEST_CHECK(moved.hasDependency("A", "B"));
    TEST_CHECK(moved.getAgvPriority("A") == TaskPriority::High);

    // Test move assignment
    WaitForGraph assigned;
    assigned = std::move(moved);
    TEST_CHECK(assigned.getAgvCount() == 2);
    TEST_CHECK(assigned.getDependencyCount() == 1);
    TEST_CHECK(assigned.hasDependency("A", "B"));

    // Test clear
    assigned.clear();
    TEST_CHECK(assigned.getAgvCount() == 0);
    TEST_CHECK(assigned.getDependencyCount() == 0);
    TEST_CHECK(!assigned.hasAgv("A"));
}

void testPriorityUpdate()
{
    WaitForGraph wfg;
    wfg.addAgv("AGV_1", TaskPriority::Normal);
    TEST_CHECK(wfg.getAgvPriority("AGV_1") == TaskPriority::Normal);

    wfg.setAgvPriority("AGV_1", TaskPriority::Critical);
    TEST_CHECK(wfg.getAgvPriority("AGV_1") == TaskPriority::Critical);
    TEST_CHECK(wfg.getAgvPriority(0) == TaskPriority::Critical);

    wfg.setAgvPriority(0, TaskPriority::Low);
    TEST_CHECK(wfg.getAgvPriority("AGV_1") == TaskPriority::Low);
}

void testInvalidTaskPriorityValidation()
{
    WaitForGraph wfg;

    // 1. addAgv() rejects an invalid priority such as static_cast<TaskPriority>(99) with std::invalid_argument
    const auto invalidPriority = static_cast<TaskPriority>(99);
    bool threw = false;
    try {
        wfg.addAgv("AGV_BAD", invalidPriority);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Also test negative invalid priority value
    const auto invalidNegPriority = static_cast<TaskPriority>(-1);
    threw = false;
    try {
        wfg.addAgv("AGV_BAD_NEG", invalidNegPriority);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // 2. After failed registration, the AGV count remains unchanged and the rejected ID is not registered
    TEST_CHECK(wfg.getAgvCount() == 0);
    TEST_CHECK(!wfg.hasAgv("AGV_BAD"));
    TEST_CHECK(!wfg.hasAgv("AGV_BAD_NEG"));

    // Register a valid AGV with TaskPriority::Normal
    wfg.addAgv("AGV_VALID", TaskPriority::Normal);
    TEST_CHECK(wfg.getAgvCount() == 1);
    TEST_CHECK(wfg.hasAgv("AGV_VALID"));
    TEST_CHECK(wfg.getAgvPriority("AGV_VALID") == TaskPriority::Normal);

    // 3. setAgvPriority(const std::string&, TaskPriority) rejects an invalid priority and preserves the existing priority
    threw = false;
    try {
        wfg.setAgvPriority("AGV_VALID", invalidPriority);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);
    TEST_CHECK(wfg.getAgvPriority("AGV_VALID") == TaskPriority::Normal);

    // 4. setAgvPriority(std::size_t, TaskPriority) rejects an invalid priority and preserves the existing priority
    threw = false;
    try {
        wfg.setAgvPriority(0, invalidPriority);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);
    TEST_CHECK(wfg.getAgvPriority(0) == TaskPriority::Normal);

    // 5. Valid priorities from Low through Critical continue to work
    const TaskPriority validPriorities[] = {
        TaskPriority::Low,
        TaskPriority::Normal,
        TaskPriority::High,
        TaskPriority::Urgent,
        TaskPriority::Critical
    };

    for (TaskPriority validPrio : validPriorities) {
        // Via index
        wfg.setAgvPriority(0, validPrio);
        TEST_CHECK(wfg.getAgvPriority(0) == validPrio);
        TEST_CHECK(wfg.getAgvPriority("AGV_VALID") == validPrio);

        // Via string ID
        wfg.setAgvPriority("AGV_VALID", validPrio);
        TEST_CHECK(wfg.getAgvPriority("AGV_VALID") == validPrio);
        TEST_CHECK(wfg.getAgvPriority(0) == validPrio);
    }

    // Registering new AGVs with each valid priority works
    wfg.addAgv("AGV_LOW", TaskPriority::Low);
    wfg.addAgv("AGV_NORMAL", TaskPriority::Normal);
    wfg.addAgv("AGV_HIGH", TaskPriority::High);
    wfg.addAgv("AGV_URGENT", TaskPriority::Urgent);
    wfg.addAgv("AGV_CRITICAL", TaskPriority::Critical);

    TEST_CHECK(wfg.getAgvCount() == 6);
    TEST_CHECK(wfg.getAgvPriority("AGV_LOW") == TaskPriority::Low);
    TEST_CHECK(wfg.getAgvPriority("AGV_NORMAL") == TaskPriority::Normal);
    TEST_CHECK(wfg.getAgvPriority("AGV_HIGH") == TaskPriority::High);
    TEST_CHECK(wfg.getAgvPriority("AGV_URGENT") == TaskPriority::Urgent);
    TEST_CHECK(wfg.getAgvPriority("AGV_CRITICAL") == TaskPriority::Critical);
}

int main()
{
    TEST_RUN(testEmptyGraphNoDeadlock);
    TEST_RUN(testSingleAgvNoDeadlock);
    TEST_RUN(testAcyclicChainNoDeadlock);
    TEST_RUN(testBranchingAcyclicGraphNoDeadlock);
    TEST_RUN(testTwoAgvDirectedCycle);
    TEST_RUN(testThreeOrMoreAgvDirectedCycle);
    TEST_RUN(testDisconnectedComponentsCycleInOnlyOne);
    TEST_RUN(testMultipleCyclesDeterministicSelection);
    TEST_RUN(testVictimSelectionLowestPriority);
    TEST_RUN(testVictimSelectionEqualPriorityTieBreaker);
    TEST_RUN(testVictimSelectionAllCriticalCycle);
    TEST_RUN(testVictimSelectionIgnoresOutsideAgvWithLowerPriority);
    TEST_RUN(testVictimSelectionNoDeadlockThrows);
    TEST_RUN(testDuplicateDependencyThrows);
    TEST_RUN(testRemovingDependencyBreaksCycle);
    TEST_RUN(testRemovingNonexistentDependency);
    TEST_RUN(testInvalidOrEmptyAgvIdentifiers);
    TEST_RUN(testUnregisteredAgvDependencies);
    TEST_RUN(testSelfDependencyBehavior);
    TEST_RUN(testRepeatedDetectionDoesNotMutate);
    TEST_RUN(testVictimSelectionDoesNotMutateGraph);
    TEST_RUN(testMoveSemanticsAndClear);
    TEST_RUN(testPriorityUpdate);
    TEST_RUN(testInvalidTaskPriorityValidation);

    TEST_REPORT();
}
