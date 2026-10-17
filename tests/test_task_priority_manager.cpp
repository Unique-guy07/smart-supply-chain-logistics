#include "test_utils.h"
#include "task/TaskPriorityManager.h"
#include <new>
#include <string>
#include <vector>

// ===========================================================================
// Deterministic Failure-Injection Infrastructure (MSVC / C++20 targeted)
//
// Environmental & Implementation Assumptions:
// 1. Under MSVC C++20 standard library (<string>), strings with length <= 15
//    characters utilize the Small String Optimization (SSO) and perform 0 dynamic
//    heap allocations. Strings with length >= 16 characters allocate memory via
//    global operator new with a capacity request >= 32 bytes (typically 48 bytes).
// 2. PriorityTask is a concrete, non-virtual type with compiler-generated
//    value-copy semantics. The only throwing operation during output assignment
//    (outTask = record.task) is dynamic memory allocation for non-SSO string members.
// 3. During dispatchHighestPriority / dispatchLowestPriority on an active task
//    with a 41-char taskId:
//      - Allocation #1 (size >= 32): peekMax(entry) / peekMin(entry) copies the
//        task into the local 'entry' variable.
//      - Allocation #2 (size >= 32): outTask = record.task copies the task into
//        the caller's output parameter.
//      - Commit phase: record.state transition, activeCount decrement, and
//        taskHeap.extractMax(entry) execute via move assignment and are strictly noexcept.
// 4. ScopedAllocationFailureGuard provides RAII cleanup: on any exit path (normal,
//    expected bad_alloc, or unexpected exception), the failure countdown is restored
//    to -1 so no failure state leaks into other tests.
// 5. g_largeAllocAttempts tracks the exact count of large allocation attempts.
//    Asserting g_largeAllocAttempts == 2 guarantees that Allocation #1 (in peekMax)
//    succeeded and Allocation #2 (in outTask = record.task) was the exact failure point,
//    preventing false-positive test passes if an exception occurs at the wrong stage.
// ===========================================================================

static int g_largeAllocAttempts = 0;
static int g_largeAllocCountdown = -1;

void* operator new(std::size_t size)
{
    if (size >= 32) {
        ++g_largeAllocAttempts;
        if (g_largeAllocCountdown > 0) {
            --g_largeAllocCountdown;
            if (g_largeAllocCountdown == 0) {
                throw std::bad_alloc();
            }
        }
    }
    void* ptr = std::malloc(size);
    if (!ptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

void* operator new[](std::size_t size)
{
    return operator new(size);
}

void operator delete(void* ptr) noexcept
{
    std::free(ptr);
}

void operator delete[](void* ptr) noexcept
{
    std::free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept
{
    std::free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept
{
    std::free(ptr);
}

/// Scoped RAII guard managing deterministic allocation failure injection.
/// Guarantees that failure injection is automatically disarmed on every exit path
/// (normal return, expected exception, or unexpected exception) with zero allocations
/// and a noexcept destructor.
class ScopedAllocationFailureGuard
{
public:
    explicit ScopedAllocationFailureGuard(int countdown) noexcept
    {
        g_largeAllocAttempts = 0;
        g_largeAllocCountdown = countdown;
    }

    ~ScopedAllocationFailureGuard() noexcept
    {
        g_largeAllocCountdown = -1;
    }

    ScopedAllocationFailureGuard(const ScopedAllocationFailureGuard&) = delete;
    ScopedAllocationFailureGuard& operator=(const ScopedAllocationFailureGuard&) = delete;
    ScopedAllocationFailureGuard(ScopedAllocationFailureGuard&&) = delete;
    ScopedAllocationFailureGuard& operator=(ScopedAllocationFailureGuard&&) = delete;
};

// ---------------------------------------------------------------------------
// T1: Empty Manager Operations
// ---------------------------------------------------------------------------
void testEmptyManager()
{
    TaskPriorityManager mgr;
    TEST_CHECK(mgr.isEmpty());
    TEST_CHECK(mgr.getActiveCount() == 0);
    TEST_CHECK(mgr.getTotalRegisteredCount() == 0);
    TEST_CHECK(mgr.verifyHeapInvariants());

    PriorityTask out("placeholder", TaskCategory::Pick, TaskPriority::Low, "DC_0", 0);
    TEST_CHECK(!mgr.peekHighestPriority(out));
    TEST_CHECK(!mgr.peekLowestPriority(out));
    TEST_CHECK(!mgr.dispatchHighestPriority(out));
    TEST_CHECK(!mgr.dispatchLowestPriority(out));

    TEST_CHECK(!mgr.hasTask("NONEXISTENT"));
    TEST_CHECK(mgr.getTask("NONEXISTENT") == nullptr);
    TEST_CHECK(!mgr.getTaskState("NONEXISTENT").has_value());
    TEST_CHECK(!mgr.cancelTask("NONEXISTENT"));
    TEST_CHECK(!mgr.updatePriority("NONEXISTENT", TaskPriority::High));
}

// ---------------------------------------------------------------------------
// T2: Registration, Unique IDs, and Auto-assigned Sequences
// ---------------------------------------------------------------------------
void testRegistrationAndUniqueIds()
{
    TaskPriorityManager mgr;

    // Successful registrations
    TEST_CHECK(mgr.registerTask("T1", TaskCategory::Pick, TaskPriority::Normal, "DC1"));
    TEST_CHECK(mgr.registerTask("T2", TaskCategory::Restock, TaskPriority::High, "DC2"));
    TEST_CHECK(mgr.registerTask("T3", TaskCategory::Maintenance, TaskPriority::Urgent, "DC1"));

    TEST_CHECK(!mgr.isEmpty());
    TEST_CHECK(mgr.getActiveCount() == 3);
    TEST_CHECK(mgr.getTotalRegisteredCount() == 3);
    TEST_CHECK(mgr.hasTask("T1"));
    TEST_CHECK(mgr.hasTask("T2"));
    TEST_CHECK(mgr.hasTask("T3"));

    // Verify sequential sequence numbers
    const PriorityTask* p1 = mgr.getTask("T1");
    const PriorityTask* p2 = mgr.getTask("T2");
    const PriorityTask* p3 = mgr.getTask("T3");
    TEST_CHECK(p1 != nullptr && p2 != nullptr && p3 != nullptr);
    TEST_CHECK(p1->getSequenceNumber() == 1);
    TEST_CHECK(p2->getSequenceNumber() == 2);
    TEST_CHECK(p3->getSequenceNumber() == 3);

    // Duplicate ID rejection
    TEST_CHECK(!mgr.registerTask("T1", TaskCategory::Audit, TaskPriority::Low, "DC3"));
    TEST_CHECK(!mgr.registerTask("T2", TaskCategory::Pack, TaskPriority::Critical, "DC1"));
    TEST_CHECK(mgr.getActiveCount() == 3);
    TEST_CHECK(mgr.getTotalRegisteredCount() == 3);

    // Invalid parameters rejection
    TEST_CHECK(!mgr.registerTask("", TaskCategory::Pick, TaskPriority::Normal, "DC1"));
    TEST_CHECK(!mgr.registerTask("   ", TaskCategory::Pick, TaskPriority::Normal, "DC1"));
    TEST_CHECK(!mgr.registerTask("T4", TaskCategory::Pick, TaskPriority::Normal, ""));
    TEST_CHECK(!mgr.registerTask("T4", TaskCategory::Pick, TaskPriority::Normal, "  "));

    // Invalid enum values rejection
    TEST_CHECK(!mgr.registerTask("T4", static_cast<TaskCategory>(99), TaskPriority::Normal, "DC1"));
    TEST_CHECK(!mgr.registerTask("T4", TaskCategory::Pick, static_cast<TaskPriority>(99), "DC1"));
    TEST_CHECK(!mgr.registerTask("T4", TaskCategory::Pick, static_cast<TaskPriority>(-1), "DC1"));

    TEST_CHECK(mgr.getActiveCount() == 3);
    TEST_CHECK(mgr.verifyHeapInvariants());
}

// ---------------------------------------------------------------------------
// T3: Dual-Ended Priority Dispatch (DEPQ)
// ---------------------------------------------------------------------------
void testDualEndedPriorityDispatch()
{
    TaskPriorityManager mgr;

    // Register tasks across all priority tiers
    TEST_CHECK(mgr.registerTask("T_NORM", TaskCategory::Pick, TaskPriority::Normal, "DC1"));
    TEST_CHECK(mgr.registerTask("T_LOW", TaskCategory::Restock, TaskPriority::Low, "DC1"));
    TEST_CHECK(mgr.registerTask("T_CRIT", TaskCategory::Emergency, TaskPriority::Critical, "DC1"));
    TEST_CHECK(mgr.registerTask("T_URG", TaskCategory::Maintenance, TaskPriority::Urgent, "DC1"));
    TEST_CHECK(mgr.registerTask("T_HIGH", TaskCategory::Pack, TaskPriority::High, "DC1"));

    PriorityTask out("placeholder", TaskCategory::Pick, TaskPriority::Low, "DC_0", 0);

    // Peeks
    TEST_CHECK(mgr.peekHighestPriority(out));
    TEST_CHECK(out.getTaskId() == "T_CRIT");
    TEST_CHECK(out.getPriority() == TaskPriority::Critical);

    TEST_CHECK(mgr.peekLowestPriority(out));
    TEST_CHECK(out.getTaskId() == "T_LOW");
    TEST_CHECK(out.getPriority() == TaskPriority::Low);

    // Dispatch highest first: Critical -> Urgent -> High -> Normal -> Low
    std::vector<std::string> expectedOrder = {"T_CRIT", "T_URG", "T_HIGH", "T_NORM", "T_LOW"};
    for (std::size_t i = 0; i < expectedOrder.size(); ++i) {
        TEST_CHECK(mgr.dispatchHighestPriority(out));
        TEST_CHECK(out.getTaskId() == expectedOrder[i]);
        TEST_CHECK(mgr.getTaskState(expectedOrder[i]) == TaskState::Dispatched);
        TEST_CHECK(mgr.getActiveCount() == expectedOrder.size() - 1 - i);
        TEST_CHECK(mgr.getTotalRegisteredCount() == expectedOrder.size());
    }

    TEST_CHECK(mgr.isEmpty());
    TEST_CHECK(!mgr.dispatchHighestPriority(out));
    TEST_CHECK(!mgr.dispatchLowestPriority(out));
}

// ---------------------------------------------------------------------------
// T4: Cancellation and Repeated Cancellation
// ---------------------------------------------------------------------------
void testCancellationAndRepeatedCancellation()
{
    TaskPriorityManager mgr;
    mgr.registerTask("A", TaskCategory::Pick, TaskPriority::High, "DC1");
    mgr.registerTask("B", TaskCategory::Pick, TaskPriority::Critical, "DC1");
    mgr.registerTask("C", TaskCategory::Pick, TaskPriority::Normal, "DC1");

    TEST_CHECK(mgr.getActiveCount() == 3);

    // Cancel B (which is currently the highest priority)
    TEST_CHECK(mgr.cancelTask("B"));
    TEST_CHECK(mgr.getActiveCount() == 2);
    TEST_CHECK(mgr.getTaskState("B") == TaskState::Cancelled);

    // Repeated cancellation fails
    TEST_CHECK(!mgr.cancelTask("B"));

    // Attempt to update priority of cancelled task fails
    TEST_CHECK(!mgr.updatePriority("B", TaskPriority::Low));

    // Attempt to re-register cancelled task ID fails (lifetime uniqueness)
    TEST_CHECK(!mgr.registerTask("B", TaskCategory::Pick, TaskPriority::High, "DC1"));

    // Highest active priority should now be A (High), lazily discarding B
    PriorityTask out("placeholder", TaskCategory::Pick, TaskPriority::Low, "DC_0", 0);
    TEST_CHECK(mgr.peekHighestPriority(out));
    TEST_CHECK(out.getTaskId() == "A");

    TEST_CHECK(mgr.dispatchHighestPriority(out));
    TEST_CHECK(out.getTaskId() == "A");

    TEST_CHECK(mgr.dispatchHighestPriority(out));
    TEST_CHECK(out.getTaskId() == "C");

    TEST_CHECK(mgr.isEmpty());
    TEST_CHECK(!mgr.dispatchHighestPriority(out));
}

// ---------------------------------------------------------------------------
// T5: Priority Promotion, Demotion, and Validation
// ---------------------------------------------------------------------------
void testPriorityPromotionAndDemotion()
{
    TaskPriorityManager mgr;
    mgr.registerTask("T1", TaskCategory::Pick, TaskPriority::Normal, "DC1");
    mgr.registerTask("T2", TaskCategory::Pick, TaskPriority::Urgent, "DC1");

    PriorityTask out("placeholder", TaskCategory::Pick, TaskPriority::Low, "DC_0", 0);

    // Initially T2 is highest
    TEST_CHECK(mgr.peekHighestPriority(out));
    TEST_CHECK(out.getTaskId() == "T2");

    // Promote T1 from Normal to Critical
    TEST_CHECK(mgr.updatePriority("T1", TaskPriority::Critical));
    TEST_CHECK(mgr.getTask("T1")->getPriority() == TaskPriority::Critical);

    // T1 should now be highest
    TEST_CHECK(mgr.peekHighestPriority(out));
    TEST_CHECK(out.getTaskId() == "T1");

    // Demote T2 from Urgent to Low
    TEST_CHECK(mgr.updatePriority("T2", TaskPriority::Low));
    TEST_CHECK(mgr.getTask("T2")->getPriority() == TaskPriority::Low);

    // T2 should now be lowest
    TEST_CHECK(mgr.peekLowestPriority(out));
    TEST_CHECK(out.getTaskId() == "T2");

    // Identical priority update rejected as no-op
    TEST_CHECK(!mgr.updatePriority("T1", TaskPriority::Critical));

    // Invalid priority rejected
    TEST_CHECK(!mgr.updatePriority("T1", static_cast<TaskPriority>(99)));
    TEST_CHECK(!mgr.updatePriority("T1", static_cast<TaskPriority>(-1)));

    TEST_CHECK(mgr.verifyHeapInvariants());
}

// ---------------------------------------------------------------------------
// T6: Cyclic Priority Updates (Stale Version Invalidation)
// ---------------------------------------------------------------------------
void testCyclicPriorityUpdates()
{
    TaskPriorityManager mgr;
    mgr.registerTask("T1", TaskCategory::Pick, TaskPriority::Normal, "DC1"); // v1
    mgr.registerTask("T2", TaskCategory::Pick, TaskPriority::Normal, "DC1"); // v1

    // Cyclic update: T1 Normal -> Critical (v2) -> Normal (v3)
    TEST_CHECK(mgr.updatePriority("T1", TaskPriority::Critical));
    TEST_CHECK(mgr.updatePriority("T1", TaskPriority::Normal));

    // Register T3 at Critical
    mgr.registerTask("T3", TaskCategory::Pick, TaskPriority::Critical, "DC1");

    PriorityTask out("placeholder", TaskCategory::Pick, TaskPriority::Low, "DC_0", 0);

    // 1st dispatch: T3 (Critical)
    TEST_CHECK(mgr.dispatchHighestPriority(out));
    TEST_CHECK(out.getTaskId() == "T3");

    // 2nd dispatch: T1 (Normal, seq 1) should be dispatched, NOT duplicate
    TEST_CHECK(mgr.dispatchHighestPriority(out));
    TEST_CHECK(out.getTaskId() == "T1");

    // 3rd dispatch: T2 (Normal, seq 2)
    TEST_CHECK(mgr.dispatchHighestPriority(out));
    TEST_CHECK(out.getTaskId() == "T2");

    // Heap must now be completely empty: T1's stale v1 and v2 entries were purged!
    TEST_CHECK(mgr.isEmpty());
    TEST_CHECK(!mgr.dispatchHighestPriority(out));
}

// ---------------------------------------------------------------------------
// T7: FIFO Seniority Preservation After Priority Updates
// ---------------------------------------------------------------------------
void testFifoSeniorityPreservation()
{
    TaskPriorityManager mgr;

    // T1 registered first (seq 1, Normal)
    // T2 registered second (seq 2, Normal)
    // T3 registered third (seq 3, Low)
    mgr.registerTask("T1", TaskCategory::Pick, TaskPriority::Normal, "DC1");
    mgr.registerTask("T2", TaskCategory::Pick, TaskPriority::Normal, "DC1");
    mgr.registerTask("T3", TaskCategory::Pick, TaskPriority::Low, "DC1");

    // Promote T3 from Low to Normal
    // T3 keeps its sequence number 3
    TEST_CHECK(mgr.updatePriority("T3", TaskPriority::Normal));

    // Register T4 directly as Normal (seq 4)
    mgr.registerTask("T4", TaskCategory::Pick, TaskPriority::Normal, "DC1");

    PriorityTask out("placeholder", TaskCategory::Pick, TaskPriority::Low, "DC_0", 0);

    // All are Normal; FIFO order must strictly follow sequence numbers: 1, 2, 3, 4
    std::vector<std::string> expectedFifo = {"T1", "T2", "T3", "T4"};
    for (const auto& expectedId : expectedFifo) {
        TEST_CHECK(mgr.dispatchHighestPriority(out));
        TEST_CHECK(out.getTaskId() == expectedId);
    }

    TEST_CHECK(mgr.isEmpty());
}

// ---------------------------------------------------------------------------
// T8: Dispatching at Most Once & Post-Dispatch Restrictions
// ---------------------------------------------------------------------------
void testDispatchAtMostOnce()
{
    TaskPriorityManager mgr;
    mgr.registerTask("T1", TaskCategory::Pick, TaskPriority::High, "DC1");

    PriorityTask out("placeholder", TaskCategory::Pick, TaskPriority::Low, "DC_0", 0);
    TEST_CHECK(mgr.dispatchHighestPriority(out));
    TEST_CHECK(out.getTaskId() == "T1");
    TEST_CHECK(mgr.getTaskState("T1") == TaskState::Dispatched);

    // Cannot dispatch again
    TEST_CHECK(!mgr.dispatchHighestPriority(out));
    TEST_CHECK(!mgr.dispatchLowestPriority(out));

    // Cannot cancel dispatched task
    TEST_CHECK(!mgr.cancelTask("T1"));

    // Cannot update priority of dispatched task
    TEST_CHECK(!mgr.updatePriority("T1", TaskPriority::Critical));

    // Cannot re-register dispatched task ID
    TEST_CHECK(!mgr.registerTask("T1", TaskCategory::Pick, TaskPriority::Low, "DC1"));
}

// ---------------------------------------------------------------------------
// T9: Mixed Interleaved Operations and Invariant Checks
// ---------------------------------------------------------------------------
void testMixedOperationsAndHeapInvariants()
{
    TaskPriorityManager mgr;

    for (int i = 0; i < 20; ++i) {
        const std::string id = "TASK_" + std::to_string(i);
        const TaskPriority pri = static_cast<TaskPriority>(i % 5);
        TEST_CHECK(mgr.registerTask(id, TaskCategory::Pick, pri, "DC1"));
        TEST_CHECK(mgr.verifyHeapInvariants());
    }

    TEST_CHECK(mgr.getActiveCount() == 20);

    // Cancel every 4th task
    for (int i = 0; i < 20; i += 4) {
        const std::string id = "TASK_" + std::to_string(i);
        TEST_CHECK(mgr.cancelTask(id));
    }
    TEST_CHECK(mgr.getActiveCount() == 15);

    // Update priorities
    TEST_CHECK(mgr.updatePriority("TASK_1", TaskPriority::Critical));
    TEST_CHECK(mgr.updatePriority("TASK_3", TaskPriority::Low));
    TEST_CHECK(mgr.verifyHeapInvariants());

    // Alternate dispatch highest and lowest
    PriorityTask out("placeholder", TaskCategory::Pick, TaskPriority::Low, "DC_0", 0);
    std::size_t dispatchedCount = 0;
    while (!mgr.isEmpty()) {
        if (dispatchedCount % 2 == 0) {
            TEST_CHECK(mgr.dispatchHighestPriority(out));
        } else {
            TEST_CHECK(mgr.dispatchLowestPriority(out));
        }
        ++dispatchedCount;
        TEST_CHECK(mgr.verifyHeapInvariants());
    }

    TEST_CHECK(dispatchedCount == 15);
    TEST_CHECK(mgr.isEmpty());
    TEST_CHECK(mgr.getActiveCount() == 0);
    TEST_CHECK(mgr.getTotalRegisteredCount() == 20);

    // Clear manager
    mgr.clear();
    TEST_CHECK(mgr.isEmpty());
    TEST_CHECK(mgr.getTotalRegisteredCount() == 0);
    TEST_CHECK(mgr.verifyHeapInvariants());
}

// ---------------------------------------------------------------------------
// T10: Clear Semantics and Task ID Reuse
// ---------------------------------------------------------------------------
void testClearSemanticsAndIdReuse()
{
    TaskPriorityManager mgr;

    // Register initial task
    TEST_CHECK(mgr.registerTask("REUSED_ID", TaskCategory::Pick, TaskPriority::High, "DC1"));
    TEST_CHECK(mgr.hasTask("REUSED_ID"));
    TEST_CHECK(mgr.getActiveCount() == 1);
    TEST_CHECK(mgr.getTotalRegisteredCount() == 1);

    // Duplicate registration while active must fail
    TEST_CHECK(!mgr.registerTask("REUSED_ID", TaskCategory::Pack, TaskPriority::Low, "DC2"));

    // Clear manager
    mgr.clear();

    // Verify cleared state
    TEST_CHECK(mgr.isEmpty());
    TEST_CHECK(mgr.getActiveCount() == 0);
    TEST_CHECK(mgr.getTotalRegisteredCount() == 0);
    TEST_CHECK(!mgr.hasTask("REUSED_ID"));
    TEST_CHECK(mgr.getTask("REUSED_ID") == nullptr);
    TEST_CHECK(!mgr.getTaskState("REUSED_ID").has_value());
    TEST_CHECK(mgr.verifyHeapInvariants());

    // Register same ID again after clear() -> must succeed
    TEST_CHECK(mgr.registerTask("REUSED_ID", TaskCategory::Pack, TaskPriority::Urgent, "DC2"));
    TEST_CHECK(mgr.hasTask("REUSED_ID"));
    TEST_CHECK(mgr.getActiveCount() == 1);
    TEST_CHECK(mgr.getTotalRegisteredCount() == 1);

    const PriorityTask* task = mgr.getTask("REUSED_ID");
    TEST_CHECK(task != nullptr);
    TEST_CHECK(task->getCategory() == TaskCategory::Pack);
    TEST_CHECK(task->getPriority() == TaskPriority::Urgent);
    TEST_CHECK(task->getDistributionCenterId() == "DC2");

    // Dispatch task
    PriorityTask out("placeholder", TaskCategory::Pick, TaskPriority::Low, "DC_0", 0);
    TEST_CHECK(mgr.dispatchHighestPriority(out));
    TEST_CHECK(out.getTaskId() == "REUSED_ID");
    TEST_CHECK(out.getPriority() == TaskPriority::Urgent);
    TEST_CHECK(mgr.isEmpty());
}

// ---------------------------------------------------------------------------
// T11: Exception-Safe Task Dispatch (Strong Exception Guarantee)
// ---------------------------------------------------------------------------
void testDispatchExceptionSafety()
{
    // Part 1: Highest-priority dispatch exception safety during output assignment
    {
        TaskPriorityManager mgr;
        const std::string longId = "TASK_LONG_ID_HIGHEST_EXCEPTION_SAFETY_001";
        TEST_CHECK(mgr.registerTask(longId, TaskCategory::Emergency, TaskPriority::Critical, "DC1"));
        TEST_CHECK(mgr.getActiveCount() == 1);

        PriorityTask outTask("placeholder", TaskCategory::Pick, TaskPriority::Low, "DC_0", 0);

        bool badAllocThrown = false;
        bool unexpectedThrown = false;
        {
            // Large alloc 1 is peekMax internal entry; large alloc 2 is outTask assignment
            ScopedAllocationFailureGuard guard(2);
            try {
                mgr.dispatchHighestPriority(outTask);
            } catch (const std::bad_alloc&) {
                badAllocThrown = true;
            } catch (...) {
                unexpectedThrown = true;
            }
        }

        // Verify that the intended exception was triggered at the exact target stage
        TEST_CHECK(badAllocThrown);
        TEST_CHECK(!unexpectedThrown);
        TEST_CHECK(g_largeAllocAttempts == 2); // Confirms failure occurred on outTask assignment, not peekMax

        // Verify Strong Exception Guarantee:
        // 1. Task remains registered and Active in registry
        TEST_CHECK(mgr.hasTask(longId));
        TEST_CHECK(mgr.getActiveCount() == 1);
        std::optional<TaskState> state = mgr.getTaskState(longId);
        TEST_CHECK(state.has_value() && *state == TaskState::Active);

        // 2. MinMaxHeap structure and invariants remain valid
        TEST_CHECK(mgr.verifyHeapInvariants());

        // 3. Subsequent dispatch succeeds and returns the exact same task
        TEST_CHECK(mgr.dispatchHighestPriority(outTask));
        TEST_CHECK(outTask.getTaskId() == longId);
        TEST_CHECK(outTask.getPriority() == TaskPriority::Critical);
        TEST_CHECK(mgr.getActiveCount() == 0);
        state = mgr.getTaskState(longId);
        TEST_CHECK(state.has_value() && *state == TaskState::Dispatched);
    }

    // Part 2: Lowest-priority dispatch exception safety during output assignment
    {
        TaskPriorityManager mgr;
        const std::string longId = "TASK_LONG_ID_LOWEST_EXCEPTION_SAFETY_0002";
        TEST_CHECK(mgr.registerTask(longId, TaskCategory::Audit, TaskPriority::Low, "DC2"));
        TEST_CHECK(mgr.getActiveCount() == 1);

        PriorityTask outTask("placeholder", TaskCategory::Pick, TaskPriority::Low, "DC_0", 0);

        bool badAllocThrown = false;
        bool unexpectedThrown = false;
        {
            // Large alloc 1 is peekMin internal entry; large alloc 2 is outTask assignment
            ScopedAllocationFailureGuard guard(2);
            try {
                mgr.dispatchLowestPriority(outTask);
            } catch (const std::bad_alloc&) {
                badAllocThrown = true;
            } catch (...) {
                unexpectedThrown = true;
            }
        }

        // Verify that the intended exception was triggered at the exact target stage
        TEST_CHECK(badAllocThrown);
        TEST_CHECK(!unexpectedThrown);
        TEST_CHECK(g_largeAllocAttempts == 2); // Confirms failure occurred on outTask assignment, not peekMin

        // Verify Strong Exception Guarantee:
        TEST_CHECK(mgr.hasTask(longId));
        TEST_CHECK(mgr.getActiveCount() == 1);
        std::optional<TaskState> state = mgr.getTaskState(longId);
        TEST_CHECK(state.has_value() && *state == TaskState::Active);
        TEST_CHECK(mgr.verifyHeapInvariants());

        // Subsequent dispatch succeeds and returns the exact same task
        TEST_CHECK(mgr.dispatchLowestPriority(outTask));
        TEST_CHECK(outTask.getTaskId() == longId);
        TEST_CHECK(outTask.getPriority() == TaskPriority::Low);
        TEST_CHECK(mgr.getActiveCount() == 0);
        state = mgr.getTaskState(longId);
        TEST_CHECK(state.has_value() && *state == TaskState::Dispatched);
    }

    // Part 3: Stale tombstone purging interplay with throwing dispatch
    {
        TaskPriorityManager mgr;

        // Register and cancel a high-priority task with a short ID (SSO, 0 dynamic allocations)
        TEST_CHECK(mgr.registerTask("STALE_TASK", TaskCategory::Emergency, TaskPriority::Critical, "DC1"));
        TEST_CHECK(mgr.cancelTask("STALE_TASK"));
        TEST_CHECK(mgr.getActiveCount() == 0);

        // Register active normal-priority task with long ID (> 15 chars)
        const std::string activeId = "TASK_LONG_ID_WITH_STALE_TOMBSTONE_TEST_003";
        TEST_CHECK(mgr.registerTask(activeId, TaskCategory::Pick, TaskPriority::Normal, "DC1"));
        TEST_CHECK(mgr.getActiveCount() == 1);

        PriorityTask outTask("placeholder", TaskCategory::Pick, TaskPriority::Low, "DC_0", 0);

        bool badAllocThrown = false;
        bool unexpectedThrown = false;
        {
            // Iteration 1: peekMax peeks STALE_TASK (SSO -> 0 large allocs), detects stale, extracts it
            // Iteration 2: peekMax peeks activeId (large alloc #1), outTask = record.task (large alloc #2)
            ScopedAllocationFailureGuard guard(2);
            try {
                mgr.dispatchHighestPriority(outTask);
            } catch (const std::bad_alloc&) {
                badAllocThrown = true;
            } catch (...) {
                unexpectedThrown = true;
            }
        }

        // Verify that the intended exception was triggered at the exact target stage
        TEST_CHECK(badAllocThrown);
        TEST_CHECK(!unexpectedThrown);
        TEST_CHECK(g_largeAllocAttempts == 2); // Confirms failure occurred on outTask assignment

        // Verify active task was NOT lost and remains Active
        TEST_CHECK(mgr.hasTask(activeId));
        TEST_CHECK(mgr.getActiveCount() == 1);
        std::optional<TaskState> state = mgr.getTaskState(activeId);
        TEST_CHECK(state.has_value() && *state == TaskState::Active);
        TEST_CHECK(mgr.verifyHeapInvariants());

        // Subsequent dispatch succeeds
        TEST_CHECK(mgr.dispatchHighestPriority(outTask));
        TEST_CHECK(outTask.getTaskId() == activeId);
        TEST_CHECK(outTask.getPriority() == TaskPriority::Normal);
        TEST_CHECK(mgr.getActiveCount() == 0);
        state = mgr.getTaskState(activeId);
        TEST_CHECK(state.has_value() && *state == TaskState::Dispatched);
    }
}

// ---------------------------------------------------------------------------
// Main Test Runner
// ---------------------------------------------------------------------------
int main()
{
    TEST_RUN(testEmptyManager);
    TEST_RUN(testRegistrationAndUniqueIds);
    TEST_RUN(testDualEndedPriorityDispatch);
    TEST_RUN(testCancellationAndRepeatedCancellation);
    TEST_RUN(testPriorityPromotionAndDemotion);
    TEST_RUN(testCyclicPriorityUpdates);
    TEST_RUN(testFifoSeniorityPreservation);
    TEST_RUN(testDispatchAtMostOnce);
    TEST_RUN(testMixedOperationsAndHeapInvariants);
    TEST_RUN(testClearSemanticsAndIdReuse);
    TEST_RUN(testDispatchExceptionSafety);
    TEST_REPORT();
}

