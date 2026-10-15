#include "test_utils.h"
#include "task/DistributedTaskQueue.h"
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

void testQueueConstructionAndValidation()
{
    // Valid DC ID
    {
        DistributedTaskQueue queue("DC-CHI");
        TEST_CHECK(queue.getDistributionCenterId() == "DC-CHI");
        TEST_CHECK(queue.isEmpty());
        TEST_CHECK(queue.getTaskCount() == 0);
        TEST_CHECK(queue.verifyHeapInvariants());
    }

    // Blank / empty DC ID must throw std::invalid_argument
    {
        bool caught = false;
        try {
            DistributedTaskQueue queue("");
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        TEST_CHECK(caught);
    }
    {
        bool caught = false;
        try {
            DistributedTaskQueue queue("   ");
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        TEST_CHECK(caught);
    }
    {
        bool caught = false;
        try {
            DistributedTaskQueue queue("\t\r\n");
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        TEST_CHECK(caught);
    }
}

void testEnqueueAndInputValidation()
{
    DistributedTaskQueue queue("DC-DAL");

    // Valid enqueue
    TEST_CHECK(queue.enqueueTask("TASK-001", TaskCategory::Pick, TaskPriority::High));
    TEST_CHECK(queue.getTaskCount() == 1);
    TEST_CHECK(!queue.isEmpty());
    TEST_CHECK(queue.verifyHeapInvariants());

    // Blank taskId rejected
    TEST_CHECK(!queue.enqueueTask("", TaskCategory::Pick, TaskPriority::High));
    TEST_CHECK(!queue.enqueueTask("   ", TaskCategory::Pick, TaskPriority::High));
    TEST_CHECK(queue.getTaskCount() == 1);

    // Invalid enum values rejected
    TEST_CHECK(!queue.enqueueTask("TASK-002", static_cast<TaskCategory>(999), TaskPriority::High));
    TEST_CHECK(!queue.enqueueTask("TASK-003", TaskCategory::Pack, static_cast<TaskPriority>(-1)));
    TEST_CHECK(queue.getTaskCount() == 1);

    // Sequence counter ordering: FIFO within same priority
    TEST_CHECK(queue.enqueueTask("TASK-004", TaskCategory::Pack, TaskPriority::High));
    TEST_CHECK(queue.getTaskCount() == 2);

    PriorityTask firstOut("DUMMY", TaskCategory::Pick, TaskPriority::Low, "DC-DUMMY", 0);
    PriorityTask secondOut("DUMMY", TaskCategory::Pick, TaskPriority::Low, "DC-DUMMY", 0);

    TEST_CHECK(queue.popTask(firstOut));
    TEST_CHECK(queue.popTask(secondOut));

    // Earlier sequence number (TASK-001) must dispatch before later sequence (TASK-004)
    TEST_CHECK(firstOut.getTaskId() == "TASK-001");
    TEST_CHECK(firstOut.getSequenceNumber() == 1);
    TEST_CHECK(secondOut.getTaskId() == "TASK-004");
    TEST_CHECK(secondOut.getSequenceNumber() == 2);
    TEST_CHECK(queue.isEmpty());
}

void testSequenceCounterOverflowGuard()
{
    DistributedTaskQueue queue("DC-OVERFLOW");

    // Push a task originating at DC-OVERFLOW with sequence near max
    const std::uint64_t nearMax = std::numeric_limits<std::uint64_t>::max() - 1;
    PriorityTask taskNearMax("TASK-MAX-1", TaskCategory::Emergency, TaskPriority::Critical, "DC-OVERFLOW", nearMax);
    TEST_CHECK(queue.pushTask(taskNearMax));

    // The local nextSequenceNumber should now be max()
    // Enqueuing a new task must be guarded and rejected safely without wrapping
    TEST_CHECK(!queue.enqueueTask("TASK-REJECTED", TaskCategory::Audit, TaskPriority::Normal));
    TEST_CHECK(queue.getTaskCount() == 1);

    // Pushing a task with sequence == max()
    PriorityTask taskAtMax("TASK-MAX-2", TaskCategory::Emergency, TaskPriority::Critical, "DC-OVERFLOW", std::numeric_limits<std::uint64_t>::max());
    TEST_CHECK(queue.pushTask(taskAtMax));
    TEST_CHECK(queue.getTaskCount() == 2);

    // Enqueue must still be safely rejected
    TEST_CHECK(!queue.enqueueTask("TASK-REJECTED-2", TaskCategory::Audit, TaskPriority::Normal));
}

void testPushTaskPreservesOriginAndSequence()
{
    // Queue managed by DC-ATL
    DistributedTaskQueue queue("DC-ATL");

    // Task originating at DC-BOS with sequence 777
    PriorityTask remoteTask("REMOTE-01", TaskCategory::Restock, TaskPriority::Urgent, "DC-BOS", 777);
    TEST_CHECK(queue.pushTask(remoteTask));

    TEST_CHECK(queue.getTaskCount() == 1);
    TEST_CHECK(queue.getDistributionCenterId() == "DC-ATL");

    PriorityTask dispatched("DUMMY", TaskCategory::Pick, TaskPriority::Low, "DC-DUMMY", 0);
    TEST_CHECK(queue.peekTask(dispatched));
    TEST_CHECK(dispatched.getTaskId() == "REMOTE-01");
    TEST_CHECK(dispatched.getCategory() == TaskCategory::Restock);
    TEST_CHECK(dispatched.getPriority() == TaskPriority::Urgent);
    TEST_CHECK(dispatched.getDistributionCenterId() == "DC-BOS"); // Preserved origin
    TEST_CHECK(dispatched.getSequenceNumber() == 777);           // Preserved sequence

    TEST_CHECK(queue.popTask(dispatched));
    TEST_CHECK(dispatched.getTaskId() == "REMOTE-01");
    TEST_CHECK(dispatched.getDistributionCenterId() == "DC-BOS");
    TEST_CHECK(queue.isEmpty());
}

void testPeekAndPopOperations()
{
    DistributedTaskQueue queue("DC-PHX");

    PriorityTask out("DUMMY", TaskCategory::Pick, TaskPriority::Low, "DC-DUMMY", 0);
    // Peek and pop on empty queue return false
    TEST_CHECK(!queue.peekTask(out));
    TEST_CHECK(!queue.popTask(out));

    queue.enqueueTask("TASK-A", TaskCategory::Pick, TaskPriority::Normal);
    queue.enqueueTask("TASK-B", TaskCategory::Emergency, TaskPriority::Critical);
    queue.enqueueTask("TASK-C", TaskCategory::Maintenance, TaskPriority::Urgent);

    TEST_CHECK(queue.getTaskCount() == 3);

    // Peek inspects without removing
    TEST_CHECK(queue.peekTask(out));
    TEST_CHECK(out.getTaskId() == "TASK-B");
    TEST_CHECK(queue.getTaskCount() == 3);

    // Pop in priority order: Critical (TASK-B) -> Urgent (TASK-C) -> Normal (TASK-A)
    TEST_CHECK(queue.popTask(out));
    TEST_CHECK(out.getTaskId() == "TASK-B");
    TEST_CHECK(queue.getTaskCount() == 2);

    TEST_CHECK(queue.popTask(out));
    TEST_CHECK(out.getTaskId() == "TASK-C");
    TEST_CHECK(queue.getTaskCount() == 1);

    TEST_CHECK(queue.popTask(out));
    TEST_CHECK(out.getTaskId() == "TASK-A");
    TEST_CHECK(queue.getTaskCount() == 0);
    TEST_CHECK(queue.isEmpty());

    // Post-empty
    TEST_CHECK(!queue.peekTask(out));
    TEST_CHECK(!queue.popTask(out));
}

void testCrossDistributionCenterMerge()
{
    DistributedTaskQueue queueEast("DC-EAST");
    DistributedTaskQueue queueWest("DC-WEST");
    DistributedTaskQueue queueNorth("DC-NORTH");

    // Queue East tasks
    queueEast.enqueueTask("TASK-E1", TaskCategory::Pick, TaskPriority::Low);     // Low, seq 1, DC-EAST
    queueEast.enqueueTask("TASK-E2", TaskCategory::Pack, TaskPriority::Urgent);  // Urgent, seq 2, DC-EAST

    // Queue West tasks
    queueWest.enqueueTask("TASK-W1", TaskCategory::Audit, TaskPriority::Normal);   // Normal, seq 1, DC-WEST
    queueWest.enqueueTask("TASK-W2", TaskCategory::Emergency, TaskPriority::Critical); // Critical, seq 2, DC-WEST

    // Queue North tasks
    queueNorth.enqueueTask("TASK-N1", TaskCategory::Restock, TaskPriority::High); // High, seq 1, DC-NORTH

    TEST_CHECK(queueEast.getTaskCount() == 2);
    TEST_CHECK(queueWest.getTaskCount() == 2);
    TEST_CHECK(queueNorth.getTaskCount() == 1);

    // Merge West into East
    queueEast.mergeFrom(std::move(queueWest));

    TEST_CHECK(queueWest.isEmpty());
    TEST_CHECK(queueWest.getTaskCount() == 0);
    TEST_CHECK(queueEast.getTaskCount() == 4);
    TEST_CHECK(queueEast.verifyHeapInvariants());

    // Merge North into East
    queueEast.mergeFrom(std::move(queueNorth));

    TEST_CHECK(queueNorth.isEmpty());
    TEST_CHECK(queueNorth.getTaskCount() == 0);
    TEST_CHECK(queueEast.getTaskCount() == 5);
    TEST_CHECK(queueEast.verifyHeapInvariants());

    // Self-merge no-op
    queueEast.mergeFrom(std::move(queueEast));
    TEST_CHECK(queueEast.getTaskCount() == 5);
    TEST_CHECK(queueEast.verifyHeapInvariants());

    // Merge with empty queue no-op
    DistributedTaskQueue emptyQueue("DC-EMPTY");
    queueEast.mergeFrom(std::move(emptyQueue));
    TEST_CHECK(queueEast.getTaskCount() == 5);

    // Verify consolidated dispatch order:
    // 1. Critical: TASK-W2 (origin: DC-WEST, seq: 2)
    // 2. Urgent:   TASK-E2 (origin: DC-EAST, seq: 2)
    // 3. High:     TASK-N1 (origin: DC-NORTH, seq: 1)
    // 4. Normal:   TASK-W1 (origin: DC-WEST, seq: 1)
    // 5. Low:      TASK-E1 (origin: DC-EAST, seq: 1)
    PriorityTask task("DUMMY", TaskCategory::Pick, TaskPriority::Low, "DC-DUMMY", 0);

    TEST_CHECK(queueEast.popTask(task));
    TEST_CHECK(task.getTaskId() == "TASK-W2");
    TEST_CHECK(task.getPriority() == TaskPriority::Critical);
    TEST_CHECK(task.getDistributionCenterId() == "DC-WEST");
    TEST_CHECK(task.getSequenceNumber() == 2);

    TEST_CHECK(queueEast.popTask(task));
    TEST_CHECK(task.getTaskId() == "TASK-E2");
    TEST_CHECK(task.getPriority() == TaskPriority::Urgent);
    TEST_CHECK(task.getDistributionCenterId() == "DC-EAST");
    TEST_CHECK(task.getSequenceNumber() == 2);

    TEST_CHECK(queueEast.popTask(task));
    TEST_CHECK(task.getTaskId() == "TASK-N1");
    TEST_CHECK(task.getPriority() == TaskPriority::High);
    TEST_CHECK(task.getDistributionCenterId() == "DC-NORTH");
    TEST_CHECK(task.getSequenceNumber() == 1);

    TEST_CHECK(queueEast.popTask(task));
    TEST_CHECK(task.getTaskId() == "TASK-W1");
    TEST_CHECK(task.getPriority() == TaskPriority::Normal);
    TEST_CHECK(task.getDistributionCenterId() == "DC-WEST");
    TEST_CHECK(task.getSequenceNumber() == 1);

    TEST_CHECK(queueEast.popTask(task));
    TEST_CHECK(task.getTaskId() == "TASK-E1");
    TEST_CHECK(task.getPriority() == TaskPriority::Low);
    TEST_CHECK(task.getDistributionCenterId() == "DC-EAST");
    TEST_CHECK(task.getSequenceNumber() == 1);

    TEST_CHECK(queueEast.isEmpty());
    TEST_CHECK(queueEast.verifyHeapInvariants());
}

void testDeterministicTieBreaking()
{
    DistributedTaskQueue queue("DC-HUB");

    // Same priority (High), same sequenceNumber (10), but different DC IDs
    // Under PriorityTask::operator<, "DC-ALPHA" < "DC-BETA" is true.
    // In a Max-Heap where extract() returns the greatest element, DC-BETA ranks higher than DC-ALPHA.
    PriorityTask taskAlpha("TASK-ALPHA", TaskCategory::Pick, TaskPriority::High, "DC-ALPHA", 10);
    PriorityTask taskBeta("TASK-BETA", TaskCategory::Pick, TaskPriority::High, "DC-BETA", 10);

    queue.pushTask(taskAlpha);
    queue.pushTask(taskBeta);

    PriorityTask t1("DUMMY", TaskCategory::Pick, TaskPriority::Low, "DC-DUMMY", 0);
    PriorityTask t2("DUMMY", TaskCategory::Pick, TaskPriority::Low, "DC-DUMMY", 0);

    TEST_CHECK(queue.popTask(t1));
    TEST_CHECK(queue.popTask(t2));

    TEST_CHECK(t1.getDistributionCenterId() == "DC-BETA");
    TEST_CHECK(t2.getDistributionCenterId() == "DC-ALPHA");

    // Same priority (Normal), same sequence (5), same DC ("DC-SAME"), different task IDs
    // "TASK-01" < "TASK-02" is true, so TASK-02 is greater and dispatches first
    PriorityTask task01("TASK-01", TaskCategory::Pack, TaskPriority::Normal, "DC-SAME", 5);
    PriorityTask task02("TASK-02", TaskCategory::Pack, TaskPriority::Normal, "DC-SAME", 5);

    queue.pushTask(task01);
    queue.pushTask(task02);

    TEST_CHECK(queue.popTask(t1));
    TEST_CHECK(queue.popTask(t2));

    TEST_CHECK(t1.getTaskId() == "TASK-02");
    TEST_CHECK(t2.getTaskId() == "TASK-01");
}

void testDuplicateTaskIdPolicy()
{
    // The domain specification documents that DistributedTaskQueue does not maintain a second
    // registry to detect duplicate task IDs across independent queues.
    // Both tasks must be accepted and extracted deterministically according to PriorityTask ordering.
    DistributedTaskQueue queue("DC-CENTRAL");

    PriorityTask task1("TASK-DUP", TaskCategory::Pick, TaskPriority::High, "DC-EAST", 1);
    PriorityTask task2("TASK-DUP", TaskCategory::Pack, TaskPriority::Critical, "DC-WEST", 2);

    TEST_CHECK(queue.pushTask(task1));
    TEST_CHECK(queue.pushTask(task2));
    TEST_CHECK(queue.getTaskCount() == 2);
    TEST_CHECK(queue.verifyHeapInvariants());

    PriorityTask out("DUMMY", TaskCategory::Pick, TaskPriority::Low, "DC-DUMMY", 0);
    TEST_CHECK(queue.popTask(out));
    TEST_CHECK(out.getTaskId() == "TASK-DUP");
    TEST_CHECK(out.getPriority() == TaskPriority::Critical);
    TEST_CHECK(out.getDistributionCenterId() == "DC-WEST");

    TEST_CHECK(queue.popTask(out));
    TEST_CHECK(out.getTaskId() == "TASK-DUP");
    TEST_CHECK(out.getPriority() == TaskPriority::High);
    TEST_CHECK(out.getDistributionCenterId() == "DC-EAST");

    TEST_CHECK(queue.isEmpty());
}

void testMoveSemanticsAndClear()
{
    DistributedTaskQueue q1("DC-M1");
    q1.enqueueTask("TASK-1", TaskCategory::Pick, TaskPriority::Normal);
    q1.enqueueTask("TASK-2", TaskCategory::Emergency, TaskPriority::Critical);

    // Move construction
    DistributedTaskQueue q2(std::move(q1));
    TEST_CHECK(q2.getTaskCount() == 2);
    TEST_CHECK(q2.getDistributionCenterId() == "DC-M1");
    TEST_CHECK(q2.verifyHeapInvariants());

    // Move assignment
    DistributedTaskQueue q3("DC-M3");
    q3 = std::move(q2);
    TEST_CHECK(q3.getTaskCount() == 2);
    TEST_CHECK(q3.getDistributionCenterId() == "DC-M1");
    TEST_CHECK(q3.verifyHeapInvariants());

    // Clear
    q3.clear();
    TEST_CHECK(q3.isEmpty());
    TEST_CHECK(q3.getTaskCount() == 0);
    TEST_CHECK(q3.verifyHeapInvariants());

    // Can enqueue again after clear
    TEST_CHECK(q3.enqueueTask("TASK-NEW", TaskCategory::Audit, TaskPriority::High));
    TEST_CHECK(q3.getTaskCount() == 1);
}

struct DistributedTaskQueueTestAccess
{
    static void setSimulateInsertionFailure(DistributedTaskQueue& queue, bool simulate) noexcept
    {
        queue.simulateInsertionFailure = simulate;
    }
};

void testFailedTaskInsertionPreservesSequenceCounter()
{
    DistributedTaskQueue queue("DC-FAILTEST");
    TEST_CHECK(queue.isEmpty());
    TEST_CHECK(queue.getTaskCount() == 0);

    // Create a task originating at DC-FAILTEST with sequence 100
    // If successfully pushed, this would advance prospective nextSequenceNumber to 101.
    PriorityTask task("TASK-FAIL-1", TaskCategory::Pick, TaskPriority::High, "DC-FAILTEST", 100);

    // Inject simulated allocation failure via the private friend test accessor
    DistributedTaskQueueTestAccess::setSimulateInsertionFailure(queue, true);
    bool pushed = queue.pushTask(task);
    DistributedTaskQueueTestAccess::setSimulateInsertionFailure(queue, false);

    // Insertion must report failure, task must not be queued, and heap invariants must hold
    TEST_CHECK(!pushed);
    TEST_CHECK(queue.isEmpty());
    TEST_CHECK(queue.getTaskCount() == 0);
    TEST_CHECK(queue.verifyHeapInvariants());

    // Verify sequence counter was NOT advanced:
    // A locally enqueued task should receive sequence 1 (not 101!)
    TEST_CHECK(queue.enqueueTask("TASK-LOCAL-1", TaskCategory::Pick, TaskPriority::High));
    TEST_CHECK(queue.getTaskCount() == 1);

    PriorityTask dispatched("DUMMY", TaskCategory::Pick, TaskPriority::Low, "DC-DUMMY", 0);
    TEST_CHECK(queue.popTask(dispatched));
    TEST_CHECK(dispatched.getTaskId() == "TASK-LOCAL-1");
    TEST_CHECK(dispatched.getSequenceNumber() == 1);

    // Also test failed enqueueTask:
    DistributedTaskQueueTestAccess::setSimulateInsertionFailure(queue, true);
    bool enqueued = queue.enqueueTask("TASK-FAIL-2", TaskCategory::Pack, TaskPriority::Normal);
    DistributedTaskQueueTestAccess::setSimulateInsertionFailure(queue, false);

    TEST_CHECK(!enqueued);
    TEST_CHECK(queue.isEmpty());

    // Sequence counter was not advanced; next successful enqueue gets sequence 2
    TEST_CHECK(queue.enqueueTask("TASK-LOCAL-2", TaskCategory::Pack, TaskPriority::Normal));
    TEST_CHECK(queue.popTask(dispatched));
    TEST_CHECK(dispatched.getTaskId() == "TASK-LOCAL-2");
    TEST_CHECK(dispatched.getSequenceNumber() == 2);
}

void testMoveTraitsConsistency()
{
    static_assert(std::is_nothrow_move_constructible_v<DistributedTaskQueue>);
    static_assert(std::is_nothrow_move_assignable_v<DistributedTaskQueue>);
    TEST_CHECK(std::is_nothrow_move_constructible_v<DistributedTaskQueue>);
    TEST_CHECK(std::is_nothrow_move_assignable_v<DistributedTaskQueue>);
}

int main()
{
    TEST_RUN(testQueueConstructionAndValidation);
    TEST_RUN(testEnqueueAndInputValidation);
    TEST_RUN(testSequenceCounterOverflowGuard);
    TEST_RUN(testPushTaskPreservesOriginAndSequence);
    TEST_RUN(testPeekAndPopOperations);
    TEST_RUN(testCrossDistributionCenterMerge);
    TEST_RUN(testDeterministicTieBreaking);
    TEST_RUN(testDuplicateTaskIdPolicy);
    TEST_RUN(testMoveSemanticsAndClear);
    TEST_RUN(testFailedTaskInsertionPreservesSequenceCounter);
    TEST_RUN(testMoveTraitsConsistency);

    TEST_REPORT();
}

