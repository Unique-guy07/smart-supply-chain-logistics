#include "test_utils.h"
#include "task/BinomialHeap.h"
#include <algorithm>
#include <functional>
#include <vector>

void testEmptyAndSingleton()
{
    BinomialHeap<int> heap;
    TEST_CHECK(heap.isEmpty());
    TEST_CHECK(heap.getSize() == 0);
    TEST_CHECK(heap.verifyHeapInvariants());

    int val = -1;
    TEST_CHECK(!heap.peek(val));
    TEST_CHECK(!heap.extract(val));

    heap.insert(42);
    TEST_CHECK(!heap.isEmpty());
    TEST_CHECK(heap.getSize() == 1);
    TEST_CHECK(heap.verifyHeapInvariants());

    TEST_CHECK(heap.peek(val));
    TEST_CHECK(val == 42);

    TEST_CHECK(heap.extract(val));
    TEST_CHECK(val == 42);
    TEST_CHECK(heap.isEmpty());
    TEST_CHECK(heap.getSize() == 0);
    TEST_CHECK(heap.verifyHeapInvariants());
}

void testDegreeOrderingAndBinaryRepresentation()
{
    BinomialHeap<int> heap;

    // Test across various binary expansions of N
    for (int i = 1; i <= 31; ++i) {
        heap.insert(i * 3);
        TEST_CHECK(heap.getSize() == static_cast<std::size_t>(i));
        TEST_CHECK(heap.verifyHeapInvariants());
    }

    TEST_CHECK(heap.getSize() == 31);
}

void testInsertAndExtractOrdering()
{
    BinomialHeap<int> heap;
    std::vector<int> inputs = {15, 3, 20, 8, 12, 1, 25, 18, 5, 30, 2, 7};
    for (int x : inputs) {
        heap.insert(x);
        TEST_CHECK(heap.verifyHeapInvariants());
    }

    TEST_CHECK(heap.getSize() == inputs.size());

    std::vector<int> extracted;
    int val = 0;
    while (heap.extract(val)) {
        extracted.push_back(val);
        TEST_CHECK(heap.verifyHeapInvariants());
    }

    TEST_CHECK(heap.isEmpty());

    // With std::less<int>, maximum elements are extracted first
    std::vector<int> expected = inputs;
    std::sort(expected.begin(), expected.end(), std::greater<int>());
    TEST_CHECK(extracted == expected);
}

void testCascadingMergeCarryPropagation()
{
    // Heap A with 7 elements -> binary 111_2 (degrees 0, 1, 2)
    BinomialHeap<int> heapA;
    for (int i = 1; i <= 7; ++i) {
        heapA.insert(i * 10);
    }
    TEST_CHECK(heapA.getSize() == 7);
    TEST_CHECK(heapA.verifyHeapInvariants());

    // Heap B with 1 element -> binary 001_2 (degree 0)
    BinomialHeap<int> heapB;
    heapB.insert(80);
    TEST_CHECK(heapB.getSize() == 1);
    TEST_CHECK(heapB.verifyHeapInvariants());

    // Merge B into A: triggers B0 + B0 -> B1; B1 + B1 -> B2; B2 + B2 -> B3
    // Resulting heap has 8 elements -> binary 1000_2 (single tree B_3 of degree 3)
    heapA.merge(std::move(heapB));

    TEST_CHECK(heapA.getSize() == 8);
    TEST_CHECK(heapA.verifyHeapInvariants());
    TEST_CHECK(heapB.isEmpty());
    TEST_CHECK(heapB.getSize() == 0);

    // Verify extraction order
    std::vector<int> extracted;
    int val = 0;
    while (heapA.extract(val)) {
        extracted.push_back(val);
    }
    std::vector<int> expected = {80, 70, 60, 50, 40, 30, 20, 10};
    TEST_CHECK(extracted == expected);
}

void testSourceEmptinessPostMerge()
{
    BinomialHeap<int> heapA;
    for (int i = 0; i < 5; ++i) {
        heapA.insert(i);
    }

    BinomialHeap<int> heapB;
    for (int i = 10; i < 20; ++i) {
        heapB.insert(i);
    }

    heapA.merge(std::move(heapB));

    // Verify source is empty and safe
    TEST_CHECK(heapB.isEmpty());
    TEST_CHECK(heapB.getSize() == 0);
    int dummy = 0;
    TEST_CHECK(!heapB.peek(dummy));
    TEST_CHECK(!heapB.extract(dummy));
    TEST_CHECK(heapB.verifyHeapInvariants());

    TEST_CHECK(heapA.getSize() == 15);
    TEST_CHECK(heapA.verifyHeapInvariants());
}

void testMoveSemantics()
{
    BinomialHeap<int> heapA;
    for (int i = 1; i <= 6; ++i) {
        heapA.insert(i * 5);
    }

    // Move construction
    BinomialHeap<int> heapB(std::move(heapA));
    TEST_CHECK(heapA.isEmpty());
    TEST_CHECK(heapA.getSize() == 0);
    TEST_CHECK(heapB.getSize() == 6);
    TEST_CHECK(heapB.verifyHeapInvariants());

    // Move assignment
    BinomialHeap<int> heapC;
    heapC.insert(999);
    heapC = std::move(heapB);
    TEST_CHECK(heapB.isEmpty());
    TEST_CHECK(heapC.getSize() == 6);
    TEST_CHECK(heapC.verifyHeapInvariants());

    int top = 0;
    TEST_CHECK(heapC.peek(top));
    TEST_CHECK(top == 30);
}

void testCustomComparatorMinHeap()
{
    // std::greater<int> configures BinomialHeap as a Min-Heap
    BinomialHeap<int, std::greater<int>> minHeap;
    std::vector<int> values = {50, 10, 30, 5, 20, 40, 15};
    for (int v : values) {
        minHeap.insert(v);
        TEST_CHECK(minHeap.verifyHeapInvariants());
    }

    int top = 0;
    TEST_CHECK(minHeap.peek(top));
    TEST_CHECK(top == 5); // Minimum element at top

    std::vector<int> extracted;
    int val = 0;
    while (minHeap.extract(val)) {
        extracted.push_back(val);
    }

    std::vector<int> expected = {5, 10, 15, 20, 30, 40, 50};
    TEST_CHECK(extracted == expected);
}

void testClearAndReinsertion()
{
    BinomialHeap<int> heap;
    for (int i = 0; i < 20; ++i) {
        heap.insert(i);
    }
    TEST_CHECK(heap.getSize() == 20);

    heap.clear();
    TEST_CHECK(heap.isEmpty());
    TEST_CHECK(heap.getSize() == 0);
    TEST_CHECK(heap.verifyHeapInvariants());

    int dummy = 0;
    TEST_CHECK(!heap.peek(dummy));
    TEST_CHECK(!heap.extract(dummy));

    // Re-insert after clear
    for (int i = 100; i < 105; ++i) {
        heap.insert(i);
    }
    TEST_CHECK(heap.getSize() == 5);
    TEST_CHECK(heap.verifyHeapInvariants());

    int top = 0;
    TEST_CHECK(heap.peek(top));
    TEST_CHECK(top == 104);
}

struct ThrowOnAssignment
{
    int value{0};
    bool shouldThrow{false};

    ThrowOnAssignment() = default;
    explicit ThrowOnAssignment(int v, bool throwOnAssign = false)
        : value(v)
        , shouldThrow(throwOnAssign)
    {
    }

    ThrowOnAssignment(const ThrowOnAssignment&) = default;
    ThrowOnAssignment(ThrowOnAssignment&&) noexcept = default;

    ThrowOnAssignment& operator=(const ThrowOnAssignment&) = default;
    ThrowOnAssignment& operator=(ThrowOnAssignment&& other)
    {
        if (other.shouldThrow) {
            throw std::runtime_error("Simulated output assignment failure in extract()");
        }
        value = other.value;
        shouldThrow = other.shouldThrow;
        return *this;
    }

    bool operator<(const ThrowOnAssignment& other) const noexcept
    {
        return value < other.value;
    }
};

void testExtractOutputAssignmentFailure()
{
    BinomialHeap<ThrowOnAssignment> heap;
    heap.insert(ThrowOnAssignment(10));
    heap.insert(ThrowOnAssignment(20));
    heap.insert(ThrowOnAssignment(30, true)); // Max element will throw on move-assignment during extract
    heap.insert(ThrowOnAssignment(5));

    TEST_CHECK(heap.getSize() == 4);
    TEST_CHECK(heap.verifyHeapInvariants());

    ThrowOnAssignment target;
    bool caught = false;
    try {
        heap.extract(target);
    } catch (const std::runtime_error&) {
        caught = true;
    }
    TEST_CHECK(caught);

    // Documented contract verification:
    // - Node was reclaimed (no memory leak)
    // - Heap remains structurally valid
    // - Size decremented by 1 (the extracted node was unlinked and its children merged back)
    TEST_CHECK(heap.getSize() == 3);
    TEST_CHECK(heap.verifyHeapInvariants());

    // Subsequent extractions must succeed in proper priority order (20, 10, 5)
    std::vector<int> extracted;
    while (heap.extract(target)) {
        extracted.push_back(target.value);
        TEST_CHECK(heap.verifyHeapInvariants());
    }
    std::vector<int> expected = {20, 10, 5};
    TEST_CHECK(extracted == expected);
    TEST_CHECK(heap.isEmpty());
}

struct ThrowingMoveComparator
{
    ThrowingMoveComparator() = default;
    ThrowingMoveComparator(const ThrowingMoveComparator&) = default;
    ThrowingMoveComparator(ThrowingMoveComparator&&) noexcept(false) {}
    ThrowingMoveComparator& operator=(ThrowingMoveComparator&&) noexcept(false) { return *this; }
    bool operator()(int a, int b) const noexcept { return a < b; }
};

void testConditionalMoveTraits()
{
    // A comparator with a throwing move constructor makes BinomialHeap's move operations conditionally noexcept(false)
    static_assert(!std::is_nothrow_move_constructible_v<BinomialHeap<int, ThrowingMoveComparator>>);
    static_assert(!std::is_nothrow_move_assignable_v<BinomialHeap<int, ThrowingMoveComparator>>);

    // Standard noexcept comparators keep BinomialHeap's move operations strictly noexcept
    static_assert(std::is_nothrow_move_constructible_v<BinomialHeap<int, std::less<int>>>);
    static_assert(std::is_nothrow_move_assignable_v<BinomialHeap<int, std::less<int>>>);

    TEST_CHECK((!std::is_nothrow_move_constructible_v<BinomialHeap<int, ThrowingMoveComparator>>));
    TEST_CHECK((std::is_nothrow_move_constructible_v<BinomialHeap<int, std::less<int>>>));
}

int main()
{
    TEST_RUN(testEmptyAndSingleton);
    TEST_RUN(testDegreeOrderingAndBinaryRepresentation);
    TEST_RUN(testInsertAndExtractOrdering);
    TEST_RUN(testCascadingMergeCarryPropagation);
    TEST_RUN(testSourceEmptinessPostMerge);
    TEST_RUN(testMoveSemantics);
    TEST_RUN(testCustomComparatorMinHeap);
    TEST_RUN(testClearAndReinsertion);
    TEST_RUN(testExtractOutputAssignmentFailure);
    TEST_RUN(testConditionalMoveTraits);
    TEST_REPORT();
}

