#include "test_utils.h"
#include "order/Queue.h"
#include <string>

// ---------------------------------------------------------------------------
// T1: Empty queue state
// ---------------------------------------------------------------------------
void testEmptyQueue()
{
    Queue<int> q;
    TEST_CHECK(q.isEmpty());
    TEST_CHECK(q.getSize() == 0);

    int val = -1;
    TEST_CHECK(!q.dequeue(val));
    TEST_CHECK(val == -1);

    TEST_CHECK(!q.peek(val));
    TEST_CHECK(val == -1);
}

// ---------------------------------------------------------------------------
// T2: Enqueue and Dequeue in strict FIFO order
// ---------------------------------------------------------------------------
void testEnqueueDequeueFIFO()
{
    Queue<std::string> q;
    q.enqueue("ORD-001");
    q.enqueue("ORD-002");
    q.enqueue("ORD-003");

    TEST_CHECK(!q.isEmpty());
    TEST_CHECK(q.getSize() == 3);

    std::string out;
    TEST_CHECK(q.peek(out));
    TEST_CHECK(out == "ORD-001");
    TEST_CHECK(q.getSize() == 3); // peek does not remove

    TEST_CHECK(q.dequeue(out));
    TEST_CHECK(out == "ORD-001");
    TEST_CHECK(q.getSize() == 2);

    TEST_CHECK(q.dequeue(out));
    TEST_CHECK(out == "ORD-002");
    TEST_CHECK(q.getSize() == 1);

    TEST_CHECK(q.dequeue(out));
    TEST_CHECK(out == "ORD-003");
    TEST_CHECK(q.isEmpty());
    TEST_CHECK(q.getSize() == 0);

    // Further dequeue fails gracefully
    TEST_CHECK(!q.dequeue(out));
}

// ---------------------------------------------------------------------------
// T3: Repeated Enqueue/Dequeue cycles
// ---------------------------------------------------------------------------
void testRepeatedEnqueueDequeue()
{
    Queue<int> q;
    for (int i = 0; i < 100; ++i) {
        q.enqueue(i);
    }
    TEST_CHECK(q.getSize() == 100);

    for (int i = 0; i < 50; ++i) {
        int val = -1;
        TEST_CHECK(q.dequeue(val));
        TEST_CHECK(val == i);
    }
    TEST_CHECK(q.getSize() == 50);

    for (int i = 100; i < 150; ++i) {
        q.enqueue(i);
    }
    TEST_CHECK(q.getSize() == 100);

    for (int i = 50; i < 150; ++i) {
        int val = -1;
        TEST_CHECK(q.dequeue(val));
        TEST_CHECK(val == i);
    }
    TEST_CHECK(q.isEmpty());
}

// ---------------------------------------------------------------------------
// T4: Move Construction preserves elements and resets source
// ---------------------------------------------------------------------------
void testMoveConstruction()
{
    Queue<std::string> q1;
    q1.enqueue("A");
    q1.enqueue("B");
    q1.enqueue("C");

    Queue<std::string> q2(std::move(q1));
    TEST_CHECK(q1.isEmpty());
    TEST_CHECK(q1.getSize() == 0);

    TEST_CHECK(q2.getSize() == 3);
    std::string val;
    TEST_CHECK(q2.dequeue(val));
    TEST_CHECK(val == "A");
    TEST_CHECK(q2.dequeue(val));
    TEST_CHECK(val == "B");
    TEST_CHECK(q2.dequeue(val));
    TEST_CHECK(val == "C");
    TEST_CHECK(q2.isEmpty());
}

// ---------------------------------------------------------------------------
// T5: Move Assignment cleans target and transfers source
// ---------------------------------------------------------------------------
void testMoveAssignment()
{
    Queue<int> q1;
    q1.enqueue(10);
    q1.enqueue(20);

    Queue<int> q2;
    q2.enqueue(999);

    q2 = std::move(q1);
    TEST_CHECK(q1.isEmpty());
    TEST_CHECK(q1.getSize() == 0);

    TEST_CHECK(q2.getSize() == 2);
    int val = 0;
    TEST_CHECK(q2.dequeue(val));
    TEST_CHECK(val == 10);
    TEST_CHECK(q2.dequeue(val));
    TEST_CHECK(val == 20);
    TEST_CHECK(q2.isEmpty());
}

// ---------------------------------------------------------------------------
// T6: Clear resets queue to empty and permits re-enqueue
// ---------------------------------------------------------------------------
void testClear()
{
    Queue<int> q;
    q.enqueue(1);
    q.enqueue(2);
    q.enqueue(3);
    TEST_CHECK(q.getSize() == 3);

    q.clear();
    TEST_CHECK(q.isEmpty());
    TEST_CHECK(q.getSize() == 0);

    int val = -1;
    TEST_CHECK(!q.dequeue(val));

    q.enqueue(42);
    TEST_CHECK(q.getSize() == 1);
    TEST_CHECK(q.dequeue(val));
    TEST_CHECK(val == 42);
    TEST_CHECK(q.isEmpty());
}

// ---------------------------------------------------------------------------
// T7: Destruction of non-empty queue (RAII node cleanup)
// ---------------------------------------------------------------------------
void testDestructionNonEmpty()
{
    {
        Queue<std::string> scoped;
        scoped.enqueue("one");
        scoped.enqueue("two");
        scoped.enqueue("three");
        // Leaves scope: destructor executes clear()
    }
    TEST_CHECK(true);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    TEST_RUN(testEmptyQueue);
    TEST_RUN(testEnqueueDequeueFIFO);
    TEST_RUN(testRepeatedEnqueueDequeue);
    TEST_RUN(testMoveConstruction);
    TEST_RUN(testMoveAssignment);
    TEST_RUN(testClear);
    TEST_RUN(testDestructionNonEmpty);
    TEST_REPORT();
}
