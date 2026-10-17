#include "test_utils.h"
#include "order/CircularLinkedList.h"
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// T1: Empty List State and Invariants
// ---------------------------------------------------------------------------
void testEmptyList()
{
    CircularLinkedList<int> list;
    TEST_CHECK(list.isEmpty());
    TEST_CHECK(list.empty());
    TEST_CHECK(list.getSize() == 0);
    TEST_CHECK(list.verifyInvariants());
    TEST_CHECK(list.toVector().empty());

    // Pop on empty
    int outVal = 42;
    TEST_CHECK(!list.popFront());
    TEST_CHECK(!list.popFront(outVal));
    TEST_CHECK(outVal == 42); // Untouched

    // Accessors throw underflow_error
    bool frontUnderflow = false;
    try {
        [[maybe_unused]] int val = list.front();
    } catch (const std::underflow_error&) {
        frontUnderflow = true;
    }
    TEST_CHECK(frontUnderflow);

    bool backUnderflow = false;
    try {
        [[maybe_unused]] int val = list.back();
    } catch (const std::underflow_error&) {
        backUnderflow = true;
    }
    TEST_CHECK(backUnderflow);

    // Rotate and clear on empty
    list.rotateForward(5);
    TEST_CHECK(list.verifyInvariants());
    list.clear();
    TEST_CHECK(list.verifyInvariants());
}

// ---------------------------------------------------------------------------
// T2: Single-Node Operations
// ---------------------------------------------------------------------------
void testSingleNode()
{
    CircularLinkedList<std::string> list;
    list.pushBack("Alpha");
    TEST_CHECK(!list.isEmpty());
    TEST_CHECK(list.getSize() == 1);
    TEST_CHECK(list.verifyInvariants());
    TEST_CHECK(list.front() == "Alpha");
    TEST_CHECK(list.back() == "Alpha");

    // Mutation through reference
    list.front() = "AlphaPrime";
    TEST_CHECK(list.back() == "AlphaPrime");

    // Rotate single-node
    list.rotateForward(1);
    TEST_CHECK(list.verifyInvariants());
    TEST_CHECK(list.front() == "AlphaPrime");

    std::vector<std::string> v = list.toVector();
    TEST_CHECK(v.size() == 1);
    TEST_CHECK(v[0] == "AlphaPrime");

    // Pop single-node
    std::string popped;
    TEST_CHECK(list.popFront(popped));
    TEST_CHECK(popped == "AlphaPrime");
    TEST_CHECK(list.isEmpty());
    TEST_CHECK(list.getSize() == 0);
    TEST_CHECK(list.verifyInvariants());
}

// ---------------------------------------------------------------------------
// T3: PushFront, PushBack, and Traversals
// ---------------------------------------------------------------------------
void testPushFrontAndBack()
{
    CircularLinkedList<int> list;
    // PushBack 10, 20
    list.pushBack(10);
    TEST_CHECK(list.verifyInvariants());
    list.pushBack(20);
    TEST_CHECK(list.verifyInvariants());
    // PushFront 5
    list.pushFront(5);
    TEST_CHECK(list.verifyInvariants());
    // PushBack 30
    list.pushBack(30);
    TEST_CHECK(list.verifyInvariants());

    // Expected order: 5, 10, 20, 30
    TEST_CHECK(list.getSize() == 4);
    TEST_CHECK(list.front() == 5);
    TEST_CHECK(list.back() == 30);

    std::vector<int> expected = {5, 10, 20, 30};
    TEST_CHECK(list.toVector() == expected);

    // PopFront 5
    TEST_CHECK(list.popFront());
    TEST_CHECK(list.verifyInvariants());
    TEST_CHECK(list.front() == 10);
    TEST_CHECK(list.getSize() == 3);

    expected = {10, 20, 30};
    TEST_CHECK(list.toVector() == expected);
}

// ---------------------------------------------------------------------------
// T4: RotateForward
// ---------------------------------------------------------------------------
void testRotateForward()
{
    CircularLinkedList<std::string> list;
    list.pushBack("A");
    list.pushBack("B");
    list.pushBack("C");
    TEST_CHECK(list.verifyInvariants());

    // Initial: A, B, C
    // Rotate 1 step -> B, C, A
    list.rotateForward(1);
    TEST_CHECK(list.verifyInvariants());
    TEST_CHECK(list.front() == "B");
    TEST_CHECK(list.back() == "A");
    std::vector<std::string> expected = {"B", "C", "A"};
    TEST_CHECK(list.toVector() == expected);

    // Rotate 2 steps -> A, B, C
    list.rotateForward(2);
    TEST_CHECK(list.verifyInvariants());
    expected = {"A", "B", "C"};
    TEST_CHECK(list.toVector() == expected);

    // Rotate full cycle (3 steps) -> no change
    list.rotateForward(3);
    TEST_CHECK(list.verifyInvariants());
    TEST_CHECK(list.toVector() == expected);

    // Rotate large steps (10 steps % 3 = 1 step) -> B, C, A
    list.rotateForward(10);
    TEST_CHECK(list.verifyInvariants());
    expected = {"B", "C", "A"};
    TEST_CHECK(list.toVector() == expected);

    // Rotate 0 steps -> no change
    list.rotateForward(0);
    TEST_CHECK(list.verifyInvariants());
    TEST_CHECK(list.toVector() == expected);
}

// ---------------------------------------------------------------------------
// T5: Move Semantics and Reusability
// ---------------------------------------------------------------------------
void testMoveSemantics()
{
    CircularLinkedList<int> src;
    src.pushBack(1);
    src.pushBack(2);
    src.pushBack(3);

    // Move construct
    CircularLinkedList<int> dst(std::move(src));
    TEST_CHECK(dst.getSize() == 3);
    TEST_CHECK(dst.verifyInvariants());
    TEST_CHECK(dst.front() == 1);
    TEST_CHECK(dst.back() == 3);

    // src must be valid empty and reusable
    TEST_CHECK(src.isEmpty());
    TEST_CHECK(src.verifyInvariants());
    src.pushBack(99);
    TEST_CHECK(src.getSize() == 1);
    TEST_CHECK(src.front() == 99);
    TEST_CHECK(src.verifyInvariants());

    // Move assign
    CircularLinkedList<int> target;
    target.pushBack(777);
    target = std::move(dst);

    TEST_CHECK(target.getSize() == 3);
    TEST_CHECK(target.verifyInvariants());
    TEST_CHECK(target.toVector() == (std::vector<int>{1, 2, 3}));

    TEST_CHECK(dst.isEmpty());
    TEST_CHECK(dst.verifyInvariants());
}

// ---------------------------------------------------------------------------
// T6: Value Removal and Search
// ---------------------------------------------------------------------------
void testRemovalAndContains()
{
    CircularLinkedList<std::string> list;
    list.pushBack("A");
    list.pushBack("B");
    list.pushBack("C");
    list.pushBack("D");

    TEST_CHECK(list.contains("B"));
    TEST_CHECK(list.contains("D"));
    TEST_CHECK(!list.contains("Z"));

    // Remove middle element
    TEST_CHECK(list.remove("B"));
    TEST_CHECK(list.verifyInvariants());
    TEST_CHECK(list.getSize() == 3);
    TEST_CHECK(list.toVector() == (std::vector<std::string>{"A", "C", "D"}));

    // Remove head
    TEST_CHECK(list.remove("A"));
    TEST_CHECK(list.verifyInvariants());
    TEST_CHECK(list.front() == "C");
    TEST_CHECK(list.toVector() == (std::vector<std::string>{"C", "D"}));

    // Remove tail
    TEST_CHECK(list.remove("D"));
    TEST_CHECK(list.verifyInvariants());
    TEST_CHECK(list.back() == "C");
    TEST_CHECK(list.toVector() == (std::vector<std::string>{"C"}));

    // Remove last element
    TEST_CHECK(list.remove("C"));
    TEST_CHECK(list.verifyInvariants());
    TEST_CHECK(list.isEmpty());

    // Remove from empty
    TEST_CHECK(!list.remove("C"));
}

// ---------------------------------------------------------------------------
// T7: Classical Josephus Sequences
// ---------------------------------------------------------------------------
void testClassicalJosephus()
{
    // N=7, k=3 -> 3, 6, 2, 7, 5, 1, 4
    {
        CircularLinkedList<int> list;
        for (int i = 1; i <= 7; ++i) {
            list.pushBack(i);
        }
        std::vector<int> res = list.executeJosephus(3);
        std::vector<int> expected = {3, 6, 2, 7, 5, 1, 4};
        TEST_CHECK(res == expected);
        TEST_CHECK(list.isEmpty());
        TEST_CHECK(list.verifyInvariants());
    }

    // N=5, k=2 -> 2, 4, 1, 5, 3
    {
        CircularLinkedList<int> list;
        for (int i = 1; i <= 5; ++i) {
            list.pushBack(i);
        }
        std::vector<int> res = list.executeJosephus(2);
        std::vector<int> expected = {2, 4, 1, 5, 3};
        TEST_CHECK(res == expected);
        TEST_CHECK(list.isEmpty());
        TEST_CHECK(list.verifyInvariants());
    }

    // N=6, k=2 -> 2, 4, 6, 3, 1, 5
    {
        CircularLinkedList<int> list;
        for (int i = 1; i <= 6; ++i) {
            list.pushBack(i);
        }
        std::vector<int> res = list.executeJosephus(2);
        std::vector<int> expected = {2, 4, 6, 3, 1, 5};
        TEST_CHECK(res == expected);
        TEST_CHECK(list.isEmpty());
        TEST_CHECK(list.verifyInvariants());
    }

    // N=10, k=1 -> sequential elimination 1..10
    {
        CircularLinkedList<int> list;
        for (int i = 1; i <= 10; ++i) {
            list.pushBack(i);
        }
        std::vector<int> res = list.executeJosephus(1);
        std::vector<int> expected = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        TEST_CHECK(res == expected);
        TEST_CHECK(list.isEmpty());
        TEST_CHECK(list.verifyInvariants());
    }

    // N=3, k=10 -> 1, 3, 2
    {
        CircularLinkedList<int> list;
        for (int i = 1; i <= 3; ++i) {
            list.pushBack(i);
        }
        std::vector<int> res = list.executeJosephus(10);
        std::vector<int> expected = {1, 3, 2};
        TEST_CHECK(res == expected);
        TEST_CHECK(list.isEmpty());
        TEST_CHECK(list.verifyInvariants());
    }

    // Invalid k == 0
    {
        CircularLinkedList<int> list;
        list.pushBack(1);
        bool caught = false;
        try {
            list.executeJosephus(0);
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        TEST_CHECK(caught);
        TEST_CHECK(list.getSize() == 1);
        TEST_CHECK(list.verifyInvariants());
    }
}

// ---------------------------------------------------------------------------
// T8: Partial Josephus Elimination and Survivor Preservation
// ---------------------------------------------------------------------------
void testPartialJosephus()
{
    // N=7, k=3, quota=2
    // Elements: 1, 2, 3, 4, 5, 6, 7
    // First elimination: 3
    // Second elimination: 6
    // Survivors: 7, 1, 2, 4, 5 (starting with 7)
    CircularLinkedList<int> list;
    for (int i = 1; i <= 7; ++i) {
        list.pushBack(i);
    }

    std::vector<int> partialRes = list.executeJosephus(3, 2);
    std::vector<int> expectedEliminated = {3, 6};
    TEST_CHECK(partialRes == expectedEliminated);
    TEST_CHECK(list.getSize() == 5);
    TEST_CHECK(list.verifyInvariants());

    std::vector<int> expectedSurvivors = {7, 1, 2, 4, 5};
    TEST_CHECK(list.toVector() == expectedSurvivors);

    // Continue next batch on surviving list with quota 2
    // Start at 7. Advance 2: 7->1->2. Eliminated: 2.
    // Next start: 4. Advance 2: 4->5->7. Eliminated: 7.
    // Survivors: 1, 4, 5 (starting with 1)
    std::vector<int> nextBatch = list.executeJosephus(3, 2);
    std::vector<int> expectedBatch2 = {2, 7};
    TEST_CHECK(nextBatch == expectedBatch2);
    TEST_CHECK(list.getSize() == 3);
    TEST_CHECK(list.verifyInvariants());

    std::vector<int> expectedSurvivors2 = {1, 4, 5};
    TEST_CHECK(list.toVector() == expectedSurvivors2);

    // Finish remaining elements with quota 0 (eliminate all)
    std::vector<int> finalBatch = list.executeJosephus(3, 0);
    // Start at 1. Advance 2: 1->4->5. Eliminated: 5.
    // Start at 1. Advance 2: (3-1)%2=0 -> 1. Eliminated: 1.
    // Last: 4. Eliminated: 4.
    std::vector<int> expectedFinal = {5, 1, 4};
    TEST_CHECK(finalBatch == expectedFinal);
    TEST_CHECK(list.isEmpty());
    TEST_CHECK(list.verifyInvariants());
}

// ---------------------------------------------------------------------------
// T9: Large List Stress and Destruction
// ---------------------------------------------------------------------------
void testLargeListStress()
{
    CircularLinkedList<int> list;
    const int N = 1000;
    for (int i = 0; i < N; ++i) {
        list.pushBack(i);
    }
    TEST_CHECK(list.getSize() == N);
    TEST_CHECK(list.verifyInvariants());

    // Rotate 250 steps
    list.rotateForward(250);
    TEST_CHECK(list.verifyInvariants());
    TEST_CHECK(list.front() == 250);

    // Partial eliminate 300 elements with step 7
    std::vector<int> eliminated = list.executeJosephus(7, 300);
    TEST_CHECK(eliminated.size() == 300);
    TEST_CHECK(list.getSize() == 700);
    TEST_CHECK(list.verifyInvariants());

    // Clear and verify clean state
    list.clear();
    TEST_CHECK(list.isEmpty());
    TEST_CHECK(list.verifyInvariants());
}

int main()
{
    TEST_RUN(testEmptyList);
    TEST_RUN(testSingleNode);
    TEST_RUN(testPushFrontAndBack);
    TEST_RUN(testRotateForward);
    TEST_RUN(testMoveSemantics);
    TEST_RUN(testRemovalAndContains);
    TEST_RUN(testClassicalJosephus);
    TEST_RUN(testPartialJosephus);
    TEST_RUN(testLargeListStress);
    TEST_REPORT();
}
