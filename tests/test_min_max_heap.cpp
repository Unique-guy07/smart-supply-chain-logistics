#include "test_utils.h"
#include "order/MinMaxHeap.h"
#include <algorithm>
#include <random>
#include <set>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// T1: Empty Heap Behavior
// ---------------------------------------------------------------------------
void testEmptyHeap()
{
    MinMaxHeap<int> heap;
    TEST_CHECK(heap.isEmpty());
    TEST_CHECK(heap.getSize() == 0);
    TEST_CHECK(heap.verifyHeapInvariants());

    int val = -999;
    TEST_CHECK(!heap.peekMin(val));
    TEST_CHECK(val == -999); // Untouched

    TEST_CHECK(!heap.peekMax(val));
    TEST_CHECK(val == -999);

    TEST_CHECK(!heap.extractMin(val));
    TEST_CHECK(val == -999);

    TEST_CHECK(!heap.extractMax(val));
    TEST_CHECK(val == -999);
}

// ---------------------------------------------------------------------------
// T2: Single Element Heap
// ---------------------------------------------------------------------------
void testSingleElement()
{
    // Test removal via extractMin
    {
        MinMaxHeap<int> heap;
        heap.insert(42);
        TEST_CHECK(!heap.isEmpty());
        TEST_CHECK(heap.getSize() == 1);
        TEST_CHECK(heap.verifyHeapInvariants());

        int minVal = 0;
        int maxVal = 0;
        TEST_CHECK(heap.peekMin(minVal));
        TEST_CHECK(heap.peekMax(maxVal));
        TEST_CHECK(minVal == 42);
        TEST_CHECK(maxVal == 42);

        int out = 0;
        TEST_CHECK(heap.extractMin(out));
        TEST_CHECK(out == 42);
        TEST_CHECK(heap.isEmpty());
        TEST_CHECK(heap.getSize() == 0);
        TEST_CHECK(heap.verifyHeapInvariants());
    }

    // Test removal via extractMax
    {
        MinMaxHeap<int> heap;
        heap.insert(99);
        TEST_CHECK(heap.verifyHeapInvariants());

        int out = 0;
        TEST_CHECK(heap.extractMax(out));
        TEST_CHECK(out == 99);
        TEST_CHECK(heap.isEmpty());
        TEST_CHECK(heap.getSize() == 0);
        TEST_CHECK(heap.verifyHeapInvariants());
    }
}

// ---------------------------------------------------------------------------
// T3: Increasing and Decreasing Insertion Sequences
// ---------------------------------------------------------------------------
void testIncreasingAndDecreasingSequences()
{
    // Increasing sequence: 1 to 50
    {
        MinMaxHeap<int> heap;
        for (int i = 1; i <= 50; ++i) {
            heap.insert(i);
            TEST_CHECK(heap.verifyHeapInvariants());
        }
        TEST_CHECK(heap.getSize() == 50);

        int minVal = 0;
        int maxVal = 0;
        TEST_CHECK(heap.peekMin(minVal));
        TEST_CHECK(heap.peekMax(maxVal));
        TEST_CHECK(minVal == 1);
        TEST_CHECK(maxVal == 50);

        // Extract all min -> strictly ascending
        for (int i = 1; i <= 50; ++i) {
            int out = 0;
            TEST_CHECK(heap.extractMin(out));
            TEST_CHECK(out == i);
            TEST_CHECK(heap.verifyHeapInvariants());
        }
        TEST_CHECK(heap.isEmpty());
    }

    // Decreasing sequence: 50 down to 1
    {
        MinMaxHeap<int> heap;
        for (int i = 50; i >= 1; --i) {
            heap.insert(i);
            TEST_CHECK(heap.verifyHeapInvariants());
        }
        TEST_CHECK(heap.getSize() == 50);

        // Extract all max -> strictly descending
        for (int i = 50; i >= 1; --i) {
            int out = 0;
            TEST_CHECK(heap.extractMax(out));
            TEST_CHECK(out == i);
            TEST_CHECK(heap.verifyHeapInvariants());
        }
        TEST_CHECK(heap.isEmpty());
    }
}

// ---------------------------------------------------------------------------
// T4: Alternating Min and Max Extraction
// ---------------------------------------------------------------------------
void testAlternatingMinMaxExtraction()
{
    MinMaxHeap<int> heap;
    const std::vector<int> values = {15, 3, 20, 1, 8, 25, 12, 30, 2, 18};
    for (int v : values) {
        heap.insert(v);
        TEST_CHECK(heap.verifyHeapInvariants());
    }
    TEST_CHECK(heap.getSize() == 10);

    // Sorted values: 1, 2, 3, 8, 12, 15, 18, 20, 25, 30
    int min1 = 0, max1 = 0;
    TEST_CHECK(heap.extractMin(min1)); // 1
    TEST_CHECK(min1 == 1);
    TEST_CHECK(heap.verifyHeapInvariants());

    TEST_CHECK(heap.extractMax(max1)); // 30
    TEST_CHECK(max1 == 30);
    TEST_CHECK(heap.verifyHeapInvariants());

    int min2 = 0, max2 = 0;
    TEST_CHECK(heap.extractMin(min2)); // 2
    TEST_CHECK(min2 == 2);
    TEST_CHECK(heap.verifyHeapInvariants());

    TEST_CHECK(heap.extractMax(max2)); // 25
    TEST_CHECK(max2 == 25);
    TEST_CHECK(heap.verifyHeapInvariants());

    int min3 = 0, max3 = 0;
    TEST_CHECK(heap.extractMin(min3)); // 3
    TEST_CHECK(min3 == 3);
    TEST_CHECK(heap.verifyHeapInvariants());

    TEST_CHECK(heap.extractMax(max3)); // 20
    TEST_CHECK(max3 == 20);
    TEST_CHECK(heap.verifyHeapInvariants());

    int min4 = 0, max4 = 0;
    TEST_CHECK(heap.extractMin(min4)); // 8
    TEST_CHECK(min4 == 8);
    TEST_CHECK(heap.verifyHeapInvariants());

    TEST_CHECK(heap.extractMax(max4)); // 18
    TEST_CHECK(max4 == 18);
    TEST_CHECK(heap.verifyHeapInvariants());

    int min5 = 0, max5 = 0;
    TEST_CHECK(heap.extractMin(min5)); // 12
    TEST_CHECK(min5 == 12);
    TEST_CHECK(heap.verifyHeapInvariants());

    TEST_CHECK(heap.extractMax(max5)); // 15
    TEST_CHECK(max5 == 15);
    TEST_CHECK(heap.verifyHeapInvariants());

    TEST_CHECK(heap.isEmpty());
}

// ---------------------------------------------------------------------------
// T5: Duplicate Keys and Custom Struct Tie-Breaking
// ---------------------------------------------------------------------------
struct TestItem
{
    int priority;
    int seq;

    bool operator<(const TestItem& other) const noexcept
    {
        if (priority != other.priority) {
            return priority < other.priority;
        }
        // Deterministic tie-breaker: smaller seq comes first in max-extraction
        return seq > other.seq;
    }
};

void testDuplicateKeysAndTieBreaking()
{
    MinMaxHeap<TestItem> heap;
    heap.insert({10, 1});
    heap.insert({10, 2});
    heap.insert({10, 3});
    heap.insert({20, 4});
    heap.insert({5, 5});

    TEST_CHECK(heap.verifyHeapInvariants());
    TEST_CHECK(heap.getSize() == 5);

    TestItem maxItem;
    TEST_CHECK(heap.extractMax(maxItem));
    TEST_CHECK(maxItem.priority == 20 && maxItem.seq == 4);

    // Among tied priority 10, item with seq 1 is greater than seq 2 and seq 3
    TEST_CHECK(heap.extractMax(maxItem));
    TEST_CHECK(maxItem.priority == 10 && maxItem.seq == 1);

    TEST_CHECK(heap.extractMax(maxItem));
    TEST_CHECK(maxItem.priority == 10 && maxItem.seq == 2);

    TEST_CHECK(heap.extractMax(maxItem));
    TEST_CHECK(maxItem.priority == 10 && maxItem.seq == 3);

    TestItem minItem;
    TEST_CHECK(heap.extractMin(minItem));
    TEST_CHECK(minItem.priority == 5 && minItem.seq == 5);

    TEST_CHECK(heap.isEmpty());
}

// ---------------------------------------------------------------------------
// T6: Differential Testing against Reference Container (std::multiset)
// ---------------------------------------------------------------------------
void testDifferentialAgainstMultiset()
{
    MinMaxHeap<int> heap;
    std::multiset<int> ref;

    std::mt19937 rng(1337);
    std::uniform_int_distribution<int> valDist(-500, 500);
    std::uniform_int_distribution<int> opDist(0, 3); // 0,1: insert, 2: extractMin, 3: extractMax

    for (int step = 0; step < 1000; ++step) {
        int op = opDist(rng);
        if (heap.isEmpty()) {
            op = 0; // Force insert when empty
        }

        if (op == 0 || op == 1) {
            int val = valDist(rng);
            heap.insert(val);
            ref.insert(val);
        } else if (op == 2) { // extractMin
            int hMin = 0;
            TEST_CHECK(heap.extractMin(hMin));
            int rMin = *ref.begin();
            ref.erase(ref.begin());
            TEST_CHECK(hMin == rMin);
        } else { // extractMax
            int hMax = 0;
            TEST_CHECK(heap.extractMax(hMax));
            auto it = std::prev(ref.end());
            int rMax = *it;
            ref.erase(it);
            TEST_CHECK(hMax == rMax);
        }

        TEST_CHECK(heap.getSize() == ref.size());
        if (!heap.isEmpty()) {
            int hMin = 0, hMax = 0;
            TEST_CHECK(heap.peekMin(hMin));
            TEST_CHECK(heap.peekMax(hMax));
            TEST_CHECK(hMin == *ref.begin());
            TEST_CHECK(hMax == *std::prev(ref.end()));
        }
        TEST_CHECK(heap.verifyHeapInvariants());
    }
}

// ---------------------------------------------------------------------------
// T7: Arbitrary Item Removal (removeIf)
// ---------------------------------------------------------------------------
void testRemoval()
{
    MinMaxHeap<int> heap;
    for (int i = 1; i <= 20; ++i) {
        heap.insert(i);
    }
    TEST_CHECK(heap.getSize() == 20);
    TEST_CHECK(heap.verifyHeapInvariants());

    // Remove element 10
    TEST_CHECK(heap.removeIf([](int v) { return v == 10; }));
    TEST_CHECK(heap.getSize() == 19);
    TEST_CHECK(heap.verifyHeapInvariants());

    // Remove non-existent element
    TEST_CHECK(!heap.removeIf([](int v) { return v == 999; }));
    TEST_CHECK(heap.getSize() == 19);

    // Remove minimum (1) and maximum (20) via removeIf
    TEST_CHECK(heap.removeIf([](int v) { return v == 1; }));
    TEST_CHECK(heap.verifyHeapInvariants());
    TEST_CHECK(heap.removeIf([](int v) { return v == 20; }));
    TEST_CHECK(heap.verifyHeapInvariants());
    TEST_CHECK(heap.getSize() == 17);

    int minVal = 0, maxVal = 0;
    TEST_CHECK(heap.peekMin(minVal));
    TEST_CHECK(heap.peekMax(maxVal));
    TEST_CHECK(minVal == 2);
    TEST_CHECK(maxVal == 19);
}

// ---------------------------------------------------------------------------
// T8: Move Semantics and Clear
// ---------------------------------------------------------------------------
void testMoveAndClear()
{
    MinMaxHeap<int> h1;
    h1.insert(10);
    h1.insert(20);
    h1.insert(30);

    // Move construct
    MinMaxHeap<int> h2(std::move(h1));
    TEST_CHECK(h1.isEmpty());
    TEST_CHECK(h2.getSize() == 3);
    TEST_CHECK(h2.verifyHeapInvariants());

    int minVal = 0, maxVal = 0;
    TEST_CHECK(h2.peekMin(minVal));
    TEST_CHECK(h2.peekMax(maxVal));
    TEST_CHECK(minVal == 10);
    TEST_CHECK(maxVal == 30);

    // Move assign
    MinMaxHeap<int> h3;
    h3.insert(999);
    h3 = std::move(h2);
    TEST_CHECK(h2.isEmpty());
    TEST_CHECK(h3.getSize() == 3);
    TEST_CHECK(h3.verifyHeapInvariants());

    // Clear
    h3.clear();
    TEST_CHECK(h3.isEmpty());
    TEST_CHECK(h3.getSize() == 0);
    TEST_CHECK(h3.verifyHeapInvariants());
}

// ---------------------------------------------------------------------------
// T9: Direct Arbitrary-Index Removal (removeAt) Across Multiple Heap Shapes
// ---------------------------------------------------------------------------
void testRemoveAtArbitraryPositions()
{
    // 1. Invalid index behavior on empty and populated heaps
    {
        MinMaxHeap<int> heap;
        int removedVal = 0;
        TEST_CHECK(!heap.removeAt(0, &removedVal));
        TEST_CHECK(removedVal == 0); // Untouched

        heap.insert(10);
        heap.insert(20);
        heap.insert(30);
        TEST_CHECK(heap.getSize() == 3);

        // Out of bounds indices
        TEST_CHECK(!heap.removeAt(3, &removedVal));
        TEST_CHECK(!heap.removeAt(999, &removedVal));
        TEST_CHECK(heap.getSize() == 3);
        TEST_CHECK(heap.verifyHeapInvariants());
    }

    // 2. Removal near root, internal nodes, and near last element on a multi-level heap
    {
        // Insert 15 elements to form a complete 4-level binary tree (levels 0, 1, 2, 3)
        // Values: 10, 20, ..., 150
        MinMaxHeap<int> heap;
        for (int v = 10; v <= 150; v += 10) {
            heap.insert(v);
        }
        TEST_CHECK(heap.getSize() == 15);
        TEST_CHECK(heap.verifyHeapInvariants());

        // Remove root (index 0 - min level)
        int removed = 0;
        TEST_CHECK(heap.removeAt(0, &removed));
        TEST_CHECK(removed == 10);
        TEST_CHECK(heap.getSize() == 14);
        TEST_CHECK(heap.verifyHeapInvariants());

        // Remove node at max-level near root (index 1)
        TEST_CHECK(heap.removeAt(1, &removed));
        TEST_CHECK(heap.getSize() == 13);
        TEST_CHECK(heap.verifyHeapInvariants());

        // Remove internal node on min-level (index 3)
        TEST_CHECK(heap.removeAt(3, &removed));
        TEST_CHECK(heap.getSize() == 12);
        TEST_CHECK(heap.verifyHeapInvariants());

        // Remove near the last element (index = size - 2)
        std::size_t nearLastIdx = heap.getSize() - 2;
        TEST_CHECK(heap.removeAt(nearLastIdx, &removed));
        TEST_CHECK(heap.getSize() == 11);
        TEST_CHECK(heap.verifyHeapInvariants());

        // Remove the exact last element (index = size - 1)
        std::size_t lastIdx = heap.getSize() - 1;
        TEST_CHECK(heap.removeAt(lastIdx, &removed));
        TEST_CHECK(heap.getSize() == 10);
        TEST_CHECK(heap.verifyHeapInvariants());

        // Drain remaining elements via extractMin, verify strictly non-decreasing order
        int prev = -1;
        while (!heap.isEmpty()) {
            int val = 0;
            TEST_CHECK(heap.extractMin(val));
            TEST_CHECK(heap.verifyHeapInvariants());
            if (prev != -1) {
                TEST_CHECK(val >= prev);
            }
            prev = val;
        }
        TEST_CHECK(heap.isEmpty());
    }

    // 3. Differential testing of removeAt against reference multiset
    {
        std::vector<int> initialValues = {
            45, 12, 89, 34, 67, 23, 90, 11, 78, 56,
            99, 10, 33, 77, 88, 22, 44, 66, 55, 31,
            72, 18, 93, 27, 61, 84, 39, 50, 71, 15
        };

        MinMaxHeap<int> heap;
        for (int v : initialValues) {
            heap.insert(v);
        }
        TEST_CHECK(heap.getSize() == 30);
        TEST_CHECK(heap.verifyHeapInvariants());

        // Track elements in reference multiset
        std::multiset<int> ref(initialValues.begin(), initialValues.end());

        // Repeatedly remove arbitrary positions across different shapes
        while (heap.getSize() > 3) {
            std::size_t idx = 0;
            switch (heap.getSize() % 5) {
            case 0: idx = 0; break;                  // root
            case 1: idx = 1; break;                  // near root
            case 2: idx = heap.getSize() / 2; break; // internal node
            case 3: idx = heap.getSize() - 2; break; // near last element
            case 4: idx = heap.getSize() - 1; break; // last element
            }

            int valRemoved = 0;
            TEST_CHECK(heap.removeAt(idx, &valRemoved));
            TEST_CHECK(heap.verifyHeapInvariants());

            auto it = ref.find(valRemoved);
            TEST_CHECK(it != ref.end());
            ref.erase(it);

            int curMin = 0, curMax = 0;
            TEST_CHECK(heap.peekMin(curMin));
            TEST_CHECK(heap.peekMax(curMax));
            TEST_CHECK(curMin == *ref.begin());
            TEST_CHECK(curMax == *ref.rbegin());
        }

        // Drain remaining elements comparing with reference
        while (!heap.isEmpty()) {
            int minVal = 0;
            TEST_CHECK(heap.extractMin(minVal));
            TEST_CHECK(minVal == *ref.begin());
            ref.erase(ref.begin());
        }
        TEST_CHECK(ref.empty());
    }
}

// ---------------------------------------------------------------------------
// T10: Exception Contracts and Conditional noexcept Verification
// ---------------------------------------------------------------------------
void testExceptionContractsAndTraits()
{
    struct NoThrowComp {
        bool operator()(int a, int b) const noexcept { return a < b; }
    };
    struct ThrowingComp {
        bool operator()(int a, int b) const { return a < b; } // non-noexcept
    };

    static_assert(noexcept(std::declval<MinMaxHeap<int, NoThrowComp>>().verifyHeapInvariants()));
    static_assert(!noexcept(std::declval<MinMaxHeap<int, ThrowingComp>>().verifyHeapInvariants()));

    static_assert(noexcept(MinMaxHeap<int, NoThrowComp>(std::declval<MinMaxHeap<int, NoThrowComp>&&>())));
    static_assert(noexcept(std::declval<MinMaxHeap<int, NoThrowComp>&>() = std::declval<MinMaxHeap<int, NoThrowComp>&&>()));

    MinMaxHeap<int, NoThrowComp> ntHeap;
    ntHeap.insert(5);
    ntHeap.insert(10);
    ntHeap.insert(1);
    TEST_CHECK(ntHeap.verifyHeapInvariants());

    MinMaxHeap<int, ThrowingComp> tHeap;
    tHeap.insert(5);
    tHeap.insert(10);
    tHeap.insert(1);
    TEST_CHECK(tHeap.verifyHeapInvariants());
}

int main()
{
    TEST_RUN(testEmptyHeap);
    TEST_RUN(testSingleElement);
    TEST_RUN(testIncreasingAndDecreasingSequences);
    TEST_RUN(testAlternatingMinMaxExtraction);
    TEST_RUN(testDuplicateKeysAndTieBreaking);
    TEST_RUN(testDifferentialAgainstMultiset);
    TEST_RUN(testRemoval);
    TEST_RUN(testMoveAndClear);
    TEST_RUN(testRemoveAtArbitraryPositions);
    TEST_RUN(testExceptionContractsAndTraits);
    TEST_REPORT();
}
