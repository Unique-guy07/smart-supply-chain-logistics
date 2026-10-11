#include "test_utils.h"
#include "order/MinMaxHeap.h"
#include "task/PriorityTask.h"
#include <string>
#include <type_traits>
#include <vector>

// ---------------------------------------------------------------------------
// T1: Construction and Accessors
// ---------------------------------------------------------------------------
void testConstructionAndAccessors()
{
    PriorityTask task1("TASK-001", TaskCategory::Pick, TaskPriority::High, "DC-CHI", 100);
    TEST_CHECK(task1.getTaskId() == "TASK-001");
    TEST_CHECK(task1.getCategory() == TaskCategory::Pick);
    TEST_CHECK(task1.getPriority() == TaskPriority::High);
    TEST_CHECK(task1.getDistributionCenterId() == "DC-CHI");
    TEST_CHECK(task1.getSequenceNumber() == 100);

    PriorityTask task2("TASK-002", TaskCategory::Emergency, TaskPriority::Critical, "DC-NYC", 1);
    TEST_CHECK(task2.getTaskId() == "TASK-002");
    TEST_CHECK(task2.getCategory() == TaskCategory::Emergency);
    TEST_CHECK(task2.getPriority() == TaskPriority::Critical);
    TEST_CHECK(task2.getDistributionCenterId() == "DC-NYC");
    TEST_CHECK(task2.getSequenceNumber() == 1);

    PriorityTask task3("TASK-003", TaskCategory::Restock, TaskPriority::Low, "DC-DAL", 42);
    TEST_CHECK(task3.getCategory() == TaskCategory::Restock);
    TEST_CHECK(task3.getPriority() == TaskPriority::Low);

    PriorityTask task4("TASK-004", TaskCategory::Pack, TaskPriority::Normal, "DC-SEA", 55);
    TEST_CHECK(task4.getCategory() == TaskCategory::Pack);
    TEST_CHECK(task4.getPriority() == TaskPriority::Normal);

    PriorityTask task5("TASK-005", TaskCategory::Maintenance, TaskPriority::Urgent, "DC-LAX", 99);
    TEST_CHECK(task5.getCategory() == TaskCategory::Maintenance);
    TEST_CHECK(task5.getPriority() == TaskPriority::Urgent);

    PriorityTask task6("TASK-006", TaskCategory::Audit, TaskPriority::Normal, "DC-MIA", 7);
    TEST_CHECK(task6.getCategory() == TaskCategory::Audit);
}

// ---------------------------------------------------------------------------
// T2: Invalid Identifier Validation
// ---------------------------------------------------------------------------
void testInvalidIdentifiers()
{
    // Empty taskId
    {
        bool caught = false;
        try {
            PriorityTask t("", TaskCategory::Pick, TaskPriority::Normal, "DC-1", 1);
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        TEST_CHECK(caught);
    }

    // Blank / whitespace-only taskId
    {
        bool caught = false;
        try {
            PriorityTask t("   \t \n", TaskCategory::Pick, TaskPriority::Normal, "DC-1", 1);
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        TEST_CHECK(caught);
    }

    // Empty distributionCenterId
    {
        bool caught = false;
        try {
            PriorityTask t("TASK-1", TaskCategory::Pick, TaskPriority::Normal, "", 1);
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        TEST_CHECK(caught);
    }

    // Blank / whitespace-only distributionCenterId
    {
        bool caught = false;
        try {
            PriorityTask t("TASK-1", TaskCategory::Pick, TaskPriority::Normal, " \t ", 1);
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        TEST_CHECK(caught);
    }

    // Both empty
    {
        bool caught = false;
        try {
            PriorityTask t("", TaskCategory::Pick, TaskPriority::Normal, "", 1);
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        TEST_CHECK(caught);
    }
}

// ---------------------------------------------------------------------------
// T2B: Invalid Enum Value Validation
// ---------------------------------------------------------------------------
void testInvalidEnumValues()
{
    // Invalid TaskCategory values
    {
        bool caught = false;
        try {
            PriorityTask t("TASK-1", static_cast<TaskCategory>(99), TaskPriority::Normal, "DC-1", 1);
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        TEST_CHECK(caught);
    }
    {
        bool caught = false;
        try {
            PriorityTask t("TASK-1", static_cast<TaskCategory>(-1), TaskPriority::Normal, "DC-1", 1);
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        TEST_CHECK(caught);
    }
    {
        bool caught = false;
        try {
            PriorityTask t("TASK-1", static_cast<TaskCategory>(6), TaskPriority::Normal, "DC-1", 1);
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        TEST_CHECK(caught);
    }

    // Invalid TaskPriority values
    {
        bool caught = false;
        try {
            PriorityTask t("TASK-1", TaskCategory::Pick, static_cast<TaskPriority>(99), "DC-1", 1);
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        TEST_CHECK(caught);
    }
    {
        bool caught = false;
        try {
            PriorityTask t("TASK-1", TaskCategory::Pick, static_cast<TaskPriority>(-1), "DC-1", 1);
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        TEST_CHECK(caught);
    }
    {
        bool caught = false;
        try {
            PriorityTask t("TASK-1", TaskCategory::Pick, static_cast<TaskPriority>(5), "DC-1", 1);
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        TEST_CHECK(caught);
    }

    // Valid boundary enumerators should construct successfully
    {
        bool thrown = false;
        try {
            PriorityTask tLow("TASK-BOUND", TaskCategory::Pick, TaskPriority::Low, "DC-1", 1);
            PriorityTask tCrit("TASK-BOUND", TaskCategory::Emergency, TaskPriority::Critical, "DC-1", 1);
        } catch (const std::exception&) {
            thrown = true;
        }
        TEST_CHECK(!thrown);
    }
}


// ---------------------------------------------------------------------------
// T3: Enum Conversions
// ---------------------------------------------------------------------------
void testEnumConversions()
{
    // TaskCategory toString
    TEST_CHECK(std::string(toString(TaskCategory::Pick)) == "Pick");
    TEST_CHECK(std::string(toString(TaskCategory::Restock)) == "Restock");
    TEST_CHECK(std::string(toString(TaskCategory::Pack)) == "Pack");
    TEST_CHECK(std::string(toString(TaskCategory::Maintenance)) == "Maintenance");
    TEST_CHECK(std::string(toString(TaskCategory::Audit)) == "Audit");
    TEST_CHECK(std::string(toString(TaskCategory::Emergency)) == "Emergency");
    TEST_CHECK(std::string(toString(static_cast<TaskCategory>(999))) == "Unknown");

    // TaskCategory to_string
    TEST_CHECK(to_string(TaskCategory::Pick) == "Pick");
    TEST_CHECK(to_string(static_cast<TaskCategory>(123)) == "Unknown");

    // TaskPriority toString
    TEST_CHECK(std::string(toString(TaskPriority::Low)) == "Low");
    TEST_CHECK(std::string(toString(TaskPriority::Normal)) == "Normal");
    TEST_CHECK(std::string(toString(TaskPriority::High)) == "High");
    TEST_CHECK(std::string(toString(TaskPriority::Urgent)) == "Urgent");
    TEST_CHECK(std::string(toString(TaskPriority::Critical)) == "Critical");
    TEST_CHECK(std::string(toString(static_cast<TaskPriority>(-1))) == "Unknown");
    TEST_CHECK(std::string(toString(static_cast<TaskPriority>(10))) == "Unknown");

    // TaskPriority to_string
    TEST_CHECK(to_string(TaskPriority::Critical) == "Critical");
    TEST_CHECK(to_string(static_cast<TaskPriority>(42)) == "Unknown");
}

// ---------------------------------------------------------------------------
// T4: Priority Ordering (Strict Weak Ordering)
// ---------------------------------------------------------------------------
void testPriorityOrdering()
{
    PriorityTask tLow("T_LOW", TaskCategory::Pick, TaskPriority::Low, "DC-1", 10);
    PriorityTask tNorm("T_NORM", TaskCategory::Pick, TaskPriority::Normal, "DC-1", 10);
    PriorityTask tHigh("T_HIGH", TaskCategory::Pick, TaskPriority::High, "DC-1", 10);
    PriorityTask tUrg("T_URG", TaskCategory::Pick, TaskPriority::Urgent, "DC-1", 10);
    PriorityTask tCrit("T_CRIT", TaskCategory::Pick, TaskPriority::Critical, "DC-1", 10);

    // Operator< : higher priority is "greater"
    TEST_CHECK(tLow < tNorm);
    TEST_CHECK(tNorm < tHigh);
    TEST_CHECK(tHigh < tUrg);
    TEST_CHECK(tUrg < tCrit);

    // Transitivity of <
    TEST_CHECK(tLow < tCrit);
    TEST_CHECK(tLow < tHigh);

    // Operator>
    TEST_CHECK(tCrit > tUrg);
    TEST_CHECK(tUrg > tHigh);
    TEST_CHECK(tHigh > tNorm);
    TEST_CHECK(tNorm > tLow);

    // Operator<= and >=
    TEST_CHECK(tLow <= tNorm);
    TEST_CHECK(tCrit >= tUrg);
    TEST_CHECK(tNorm <= tNorm);
    TEST_CHECK(tNorm >= tNorm);

    // Irreflexivity
    TEST_CHECK(!(tHigh < tHigh));
    TEST_CHECK(!(tHigh > tHigh));
}

// ---------------------------------------------------------------------------
// T5: FIFO Tie-Breaking
// ---------------------------------------------------------------------------
void testFifoTieBreaking()
{
    // Same priority, different sequence numbers
    // Earlier sequence number arrived earlier -> must have higher dispatch urgency (greater in heap)
    PriorityTask tSeq1("T1", TaskCategory::Pick, TaskPriority::Normal, "DC-1", 1);
    PriorityTask tSeq2("T2", TaskCategory::Pick, TaskPriority::Normal, "DC-1", 2);
    PriorityTask tSeq10("T10", TaskCategory::Pick, TaskPriority::Normal, "DC-1", 10);

    // Earlier sequence number is greater, so tSeq2 is LESS than tSeq1
    TEST_CHECK(tSeq2 < tSeq1);
    TEST_CHECK(tSeq1 > tSeq2);
    TEST_CHECK(!(tSeq1 < tSeq2));

    TEST_CHECK(tSeq10 < tSeq2);
    TEST_CHECK(tSeq10 < tSeq1);
    TEST_CHECK(tSeq1 > tSeq10);
}

// ---------------------------------------------------------------------------
// T6: Cross-Center Tie-Breaking
// ---------------------------------------------------------------------------
void testCrossCenterTieBreaking()
{
    // Same priority AND same sequence number, different distribution centers
    PriorityTask tEast("T_A", TaskCategory::Pack, TaskPriority::High, "DC-EAST", 50);
    PriorityTask tWest("T_B", TaskCategory::Pack, TaskPriority::High, "DC-WEST", 50);

    // Lexicographical ordering on distributionCenterId: "DC-EAST" < "DC-WEST"
    TEST_CHECK(tEast < tWest);
    TEST_CHECK(tWest > tEast);
    TEST_CHECK(!(tWest < tEast));
}

// ---------------------------------------------------------------------------
// T7: Task ID Tie-Breaking
// ---------------------------------------------------------------------------
void testTaskIdTieBreaking()
{
    // Same priority, sequence number, and DC ID; different taskId
    PriorityTask t01("TASK-01", TaskCategory::Audit, TaskPriority::Urgent, "DC-1", 10);
    PriorityTask t02("TASK-02", TaskCategory::Audit, TaskPriority::Urgent, "DC-1", 10);

    TEST_CHECK(t01 < t02);
    TEST_CHECK(t02 > t01);
    TEST_CHECK(!(t02 < t01));
}

// ---------------------------------------------------------------------------
// T8: Equality Semantics (Task Identity)
// ---------------------------------------------------------------------------
void testEqualitySemantics()
{
    // Tasks with same taskId are equal under entity-identity semantics,
    // even if priority or category differ across temporal snapshots.
    PriorityTask tOrig("TASK-X", TaskCategory::Pick, TaskPriority::Low, "DC-1", 5);
    PriorityTask tUpdated("TASK-X", TaskCategory::Pick, TaskPriority::Critical, "DC-1", 5);

    TEST_CHECK(tOrig == tUpdated);
    TEST_CHECK(!(tOrig != tUpdated));
    TEST_CHECK(!tOrig.exactEquals(tUpdated)); // Attributes differ

    PriorityTask tOther("TASK-Y", TaskCategory::Pick, TaskPriority::Low, "DC-1", 5);
    TEST_CHECK(tOrig != tOther);
    TEST_CHECK(!(tOrig == tOther));
    TEST_CHECK(!tOrig.exactEquals(tOther));

    PriorityTask tClone("TASK-X", TaskCategory::Pick, TaskPriority::Low, "DC-1", 5);
    TEST_CHECK(tOrig == tClone);
    TEST_CHECK(tOrig.exactEquals(tClone));
}

// ---------------------------------------------------------------------------
// T9: Copy and Move Semantics
// ---------------------------------------------------------------------------
void testCopyAndMoveSemantics()
{
    static_assert(std::is_nothrow_move_constructible_v<PriorityTask>, "Move constructor must be noexcept");
    static_assert(std::is_nothrow_move_assignable_v<PriorityTask>, "Move assignment must be noexcept");
    static_assert(!std::is_default_constructible_v<PriorityTask>, "PriorityTask must not be default constructible");

    // Copy construction
    PriorityTask original("TASK-100", TaskCategory::Maintenance, TaskPriority::High, "DC-ALPHA", 77);
    PriorityTask copied(original);
    TEST_CHECK(copied == original);
    TEST_CHECK(copied.exactEquals(original));
    TEST_CHECK(copied.getTaskId() == "TASK-100");

    // Copy assignment
    PriorityTask assigned("PLACEHOLDER", TaskCategory::Pick, TaskPriority::Low, "DC-BETA", 1);
    assigned = original;
    TEST_CHECK(assigned == original);
    TEST_CHECK(assigned.exactEquals(original));

    // Move construction
    PriorityTask moved(std::move(copied));
    TEST_CHECK(moved.getTaskId() == "TASK-100");
    TEST_CHECK(moved.getCategory() == TaskCategory::Maintenance);
    TEST_CHECK(moved.getPriority() == TaskPriority::High);
    TEST_CHECK(moved.getDistributionCenterId() == "DC-ALPHA");
    TEST_CHECK(moved.getSequenceNumber() == 77);

    // Move assignment
    PriorityTask moveAssigned("TEMP", TaskCategory::Audit, TaskPriority::Low, "DC-GAMMA", 0);
    moveAssigned = std::move(moved);
    TEST_CHECK(moveAssigned.getTaskId() == "TASK-100");
    TEST_CHECK(moveAssigned.getPriority() == TaskPriority::High);
}

// ---------------------------------------------------------------------------
// T10: MinMaxHeap Integration — Priority Hierarchy
// ---------------------------------------------------------------------------
void testMinMaxHeapPriorityIntegration()
{
    MinMaxHeap<PriorityTask> heap;
    TEST_CHECK(heap.isEmpty());
    TEST_CHECK(heap.getSize() == 0);
    TEST_CHECK(heap.verifyHeapInvariants());

    // Insert 5 tasks with distinct priorities in non-sorted order
    heap.insert(PriorityTask("T_NORM", TaskCategory::Pick, TaskPriority::Normal, "DC-1", 10));
    heap.insert(PriorityTask("T_LOW", TaskCategory::Restock, TaskPriority::Low, "DC-1", 10));
    heap.insert(PriorityTask("T_CRIT", TaskCategory::Emergency, TaskPriority::Critical, "DC-1", 10));
    heap.insert(PriorityTask("T_URG", TaskCategory::Maintenance, TaskPriority::Urgent, "DC-1", 10));
    heap.insert(PriorityTask("T_HIGH", TaskCategory::Pack, TaskPriority::High, "DC-1", 10));

    TEST_CHECK(heap.getSize() == 5);
    TEST_CHECK(heap.verifyHeapInvariants());

    // Check peekMax and peekMin
    PriorityTask maxPeek("TMP", TaskCategory::Pick, TaskPriority::Low, "DC-0", 0);
    PriorityTask minPeek("TMP", TaskCategory::Pick, TaskPriority::Low, "DC-0", 0);

    TEST_CHECK(heap.peekMax(maxPeek));
    TEST_CHECK(maxPeek.getTaskId() == "T_CRIT");
    TEST_CHECK(maxPeek.getPriority() == TaskPriority::Critical);

    TEST_CHECK(heap.peekMin(minPeek));
    TEST_CHECK(minPeek.getTaskId() == "T_LOW");
    TEST_CHECK(minPeek.getPriority() == TaskPriority::Low);

    // Extract max one by one: Critical -> Urgent -> High -> Normal -> Low
    std::vector<std::string> expectedOrder = {
        "T_CRIT", "T_URG", "T_HIGH", "T_NORM", "T_LOW"
    };

    for (const auto& expectedId : expectedOrder) {
        PriorityTask out("TMP", TaskCategory::Pick, TaskPriority::Low, "DC-0", 0);
        TEST_CHECK(heap.extractMax(out));
        TEST_CHECK(out.getTaskId() == expectedId);
        TEST_CHECK(heap.verifyHeapInvariants());
    }

    TEST_CHECK(heap.isEmpty());
    TEST_CHECK(heap.getSize() == 0);
}

// ---------------------------------------------------------------------------
// T11: MinMaxHeap Integration — FIFO Order for Equal Priority
// ---------------------------------------------------------------------------
void testMinMaxHeapFifoIntegration()
{
    MinMaxHeap<PriorityTask> heap;

    // Insert 4 tasks with the SAME priority (Urgent) but sequence numbers 1, 2, 3, 4
    // Insert in scrambled sequence: 3, 1, 4, 2
    heap.insert(PriorityTask("T_SEQ3", TaskCategory::Pick, TaskPriority::Urgent, "DC-1", 3));
    heap.insert(PriorityTask("T_SEQ1", TaskCategory::Pick, TaskPriority::Urgent, "DC-1", 1));
    heap.insert(PriorityTask("T_SEQ4", TaskCategory::Pick, TaskPriority::Urgent, "DC-1", 4));
    heap.insert(PriorityTask("T_SEQ2", TaskCategory::Pick, TaskPriority::Urgent, "DC-1", 2));

    TEST_CHECK(heap.getSize() == 4);
    TEST_CHECK(heap.verifyHeapInvariants());

    // Strict FIFO extraction on max: seq 1, then 2, then 3, then 4
    std::vector<std::string> expectedFifo = {
        "T_SEQ1", "T_SEQ2", "T_SEQ3", "T_SEQ4"
    };

    for (const auto& expectedId : expectedFifo) {
        PriorityTask out("TMP", TaskCategory::Pick, TaskPriority::Low, "DC-0", 0);
        TEST_CHECK(heap.extractMax(out));
        TEST_CHECK(out.getTaskId() == expectedId);
        TEST_CHECK(heap.verifyHeapInvariants());
    }

    TEST_CHECK(heap.isEmpty());
}

// ---------------------------------------------------------------------------
// T12: MinMaxHeap Integration — Mixed Dual-Ended Operations
// ---------------------------------------------------------------------------
void testMinMaxHeapMixedOperations()
{
    MinMaxHeap<PriorityTask> heap;

    // Mixed insertions across multiple priorities and centers
    heap.insert(PriorityTask("M1", TaskCategory::Pick, TaskPriority::Low, "DC-A", 10));
    TEST_CHECK(heap.verifyHeapInvariants());

    heap.insert(PriorityTask("M2", TaskCategory::Pack, TaskPriority::High, "DC-B", 20));
    TEST_CHECK(heap.verifyHeapInvariants());

    heap.insert(PriorityTask("M3", TaskCategory::Emergency, TaskPriority::Critical, "DC-A", 5));
    TEST_CHECK(heap.verifyHeapInvariants());

    heap.insert(PriorityTask("M4", TaskCategory::Maintenance, TaskPriority::Normal, "DC-C", 15));
    TEST_CHECK(heap.verifyHeapInvariants());

    heap.insert(PriorityTask("M5", TaskCategory::Restock, TaskPriority::Urgent, "DC-B", 2));
    TEST_CHECK(heap.verifyHeapInvariants());

    PriorityTask out("TMP", TaskCategory::Pick, TaskPriority::Low, "DC-0", 0);

    // Extract highest urgency (Critical: M3)
    TEST_CHECK(heap.extractMax(out));
    TEST_CHECK(out.getTaskId() == "M3");
    TEST_CHECK(heap.verifyHeapInvariants());

    // Extract lowest urgency (Low: M1)
    TEST_CHECK(heap.extractMin(out));
    TEST_CHECK(out.getTaskId() == "M1");
    TEST_CHECK(heap.verifyHeapInvariants());

    // Insert two more
    heap.insert(PriorityTask("M6", TaskCategory::Audit, TaskPriority::Critical, "DC-B", 30));
    TEST_CHECK(heap.verifyHeapInvariants());

    heap.insert(PriorityTask("M7", TaskCategory::Pick, TaskPriority::Low, "DC-C", 100));
    TEST_CHECK(heap.verifyHeapInvariants());

    // Highest should now be M6 (Critical)
    TEST_CHECK(heap.extractMax(out));
    TEST_CHECK(out.getTaskId() == "M6");
    TEST_CHECK(heap.verifyHeapInvariants());

    // Lowest should now be M7 (Low, seq 100)
    TEST_CHECK(heap.extractMin(out));
    TEST_CHECK(out.getTaskId() == "M7");
    TEST_CHECK(heap.verifyHeapInvariants());

    // Remaining in heap: M2 (High, 20), M4 (Normal, 15), M5 (Urgent, 2)
    // Extract remaining in max order: M5 (Urgent), M2 (High), M4 (Normal)
    TEST_CHECK(heap.extractMax(out));
    TEST_CHECK(out.getTaskId() == "M5");
    TEST_CHECK(heap.verifyHeapInvariants());

    TEST_CHECK(heap.extractMax(out));
    TEST_CHECK(out.getTaskId() == "M2");
    TEST_CHECK(heap.verifyHeapInvariants());

    TEST_CHECK(heap.extractMax(out));
    TEST_CHECK(out.getTaskId() == "M4");
    TEST_CHECK(heap.verifyHeapInvariants());

    TEST_CHECK(heap.isEmpty());
    TEST_CHECK(!heap.extractMax(out));
    TEST_CHECK(!heap.extractMin(out));
}

// ---------------------------------------------------------------------------
// Main runner
// ---------------------------------------------------------------------------
int main()
{
    TEST_RUN(testConstructionAndAccessors);
    TEST_RUN(testInvalidIdentifiers);
    TEST_RUN(testInvalidEnumValues);
    TEST_RUN(testEnumConversions);
    TEST_RUN(testPriorityOrdering);
    TEST_RUN(testFifoTieBreaking);
    TEST_RUN(testCrossCenterTieBreaking);
    TEST_RUN(testTaskIdTieBreaking);
    TEST_RUN(testEqualitySemantics);
    TEST_RUN(testCopyAndMoveSemantics);
    TEST_RUN(testMinMaxHeapPriorityIntegration);
    TEST_RUN(testMinMaxHeapFifoIntegration);
    TEST_RUN(testMinMaxHeapMixedOperations);
    TEST_REPORT();
}
