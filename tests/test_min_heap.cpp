#include "test_utils.h"
#include "task/MinHeap.h"

#include <algorithm>
#include <string>
#include <vector>

void testEmptyAndSingletonHeap()
{
    MinHeap<int> heap;
    TEST_CHECK(heap.isEmpty());
    TEST_CHECK(heap.getSize() == 0);
    TEST_CHECK(heap.verifyHeapInvariants());

    // Verify output arguments remain unchanged on failure
    int val = -999;
    TEST_CHECK(!heap.peekMin(val));
    TEST_CHECK(val == -999);

    TEST_CHECK(!heap.extractMin(val));
    TEST_CHECK(val == -999);

    // Single-element insert
    heap.insert(42);
    TEST_CHECK(!heap.isEmpty());
    TEST_CHECK(heap.getSize() == 1);
    TEST_CHECK(heap.verifyHeapInvariants());

    // Single-element peek
    TEST_CHECK(heap.peekMin(val));
    TEST_CHECK(val == 42);

    // Single-element extract
    val = 0;
    TEST_CHECK(heap.extractMin(val));
    TEST_CHECK(val == 42);
    TEST_CHECK(heap.isEmpty());
    TEST_CHECK(heap.getSize() == 0);
    TEST_CHECK(heap.verifyHeapInvariants());

    // Verify empty again
    val = -1;
    TEST_CHECK(!heap.peekMin(val));
    TEST_CHECK(val == -1);
    TEST_CHECK(!heap.extractMin(val));
    TEST_CHECK(val == -1);
}

void testInsertAndExtractOrdering()
{
    MinHeap<int> heap;
    std::vector<int> inputs = {35, 12, 48, 5, 23, 1, 19, 50, 8};
    for (int x : inputs) {
        heap.insert(x);
        TEST_CHECK(heap.verifyHeapInvariants());
    }

    TEST_CHECK(heap.getSize() == inputs.size());

    std::vector<int> extracted;
    int val = 0;
    while (heap.extractMin(val)) {
        extracted.push_back(val);
        TEST_CHECK(heap.verifyHeapInvariants());
    }

    TEST_CHECK(heap.isEmpty());
    std::vector<int> expected = {1, 5, 8, 12, 19, 23, 35, 48, 50};
    TEST_CHECK(extracted == expected);
}

void testDuplicateKeys()
{
    MinHeap<int> heap;
    std::vector<int> inputs = {20, 10, 20, 5, 10, 5, 30, 20};
    for (int x : inputs) {
        heap.insert(x);
        TEST_CHECK(heap.verifyHeapInvariants());
    }
    TEST_CHECK(heap.getSize() == 8);

    std::vector<int> extracted;
    int val = 0;
    while (heap.extractMin(val)) {
        extracted.push_back(val);
        TEST_CHECK(heap.verifyHeapInvariants());
    }

    TEST_CHECK(heap.isEmpty());
    std::vector<int> expected = {5, 5, 10, 10, 20, 20, 20, 30};
    TEST_CHECK(extracted == expected);
}

void testSortedAndReverseSortedInput()
{
    // Already ascending sorted input
    {
        MinHeap<int> heap;
        for (int i = 1; i <= 10; ++i) {
            heap.insert(i);
            TEST_CHECK(heap.verifyHeapInvariants());
        }
        TEST_CHECK(heap.getSize() == 10);

        std::vector<int> extracted;
        int val = 0;
        while (heap.extractMin(val)) {
            extracted.push_back(val);
            TEST_CHECK(heap.verifyHeapInvariants());
        }
        std::vector<int> expected = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        TEST_CHECK(extracted == expected);
    }

    // Reverse sorted (descending) input
    {
        MinHeap<int> heap;
        for (int i = 10; i >= 1; --i) {
            heap.insert(i);
            TEST_CHECK(heap.verifyHeapInvariants());
        }
        TEST_CHECK(heap.getSize() == 10);

        std::vector<int> extracted;
        int val = 0;
        while (heap.extractMin(val)) {
            extracted.push_back(val);
            TEST_CHECK(heap.verifyHeapInvariants());
        }
        std::vector<int> expected = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        TEST_CHECK(extracted == expected);
    }
}

void testFloydBuildHeapConstructor()
{
    std::vector<int> data = {9, 2, 7, 1, 5, 6, 8, 3, 4, 0};
    MinHeap<int> heap(data);

    TEST_CHECK(heap.getSize() == data.size());
    TEST_CHECK(heap.verifyHeapInvariants());

    int top = -1;
    TEST_CHECK(heap.peekMin(top));
    TEST_CHECK(top == 0);

    std::vector<int> extracted;
    int val = 0;
    while (heap.extractMin(val)) {
        extracted.push_back(val);
        TEST_CHECK(heap.verifyHeapInvariants());
    }

    std::vector<int> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    TEST_CHECK(extracted == expected);
}

struct WarehousePackage
{
    int weightKg;
    std::string binLocation;

    // operator> provides default std::greater<WarehousePackage> support
    bool operator>(const WarehousePackage& other) const noexcept
    {
        if (weightKg != other.weightKg) {
            return weightKg > other.weightKg;
        }
        return binLocation > other.binLocation;
    }

    bool operator==(const WarehousePackage& other) const noexcept
    {
        return weightKg == other.weightKg && binLocation == other.binLocation;
    }
};

struct CustomBinDescComparator
{
    // Custom comparator that prioritizes lexicographically smaller binLocation
    bool operator()(const WarehousePackage& a, const WarehousePackage& b) const noexcept
    {
        return a.binLocation > b.binLocation;
    }
};

void testCustomTypeAndComparator()
{
    // Default comparator via operator>
    {
        MinHeap<WarehousePackage> heap;
        heap.insert(WarehousePackage{25, "A-03"});
        heap.insert(WarehousePackage{5, "B-01"});
        heap.insert(WarehousePackage{15, "A-01"});
        heap.insert(WarehousePackage{5, "A-02"});

        TEST_CHECK(heap.verifyHeapInvariants());

        WarehousePackage top{0, ""};
        TEST_CHECK(heap.peekMin(top));
        TEST_CHECK(top.weightKg == 5 && top.binLocation == "A-02");

        std::vector<WarehousePackage> extracted;
        while (heap.extractMin(top)) {
            extracted.push_back(top);
        }

        std::vector<WarehousePackage> expected = {
            WarehousePackage{5, "A-02"},
            WarehousePackage{5, "B-01"},
            WarehousePackage{15, "A-01"},
            WarehousePackage{25, "A-03"}
        };
        TEST_CHECK(extracted == expected);
    }

    // Custom comparator prioritizing binLocation
    {
        MinHeap<WarehousePackage, CustomBinDescComparator> heap;
        heap.insert(WarehousePackage{100, "Zone-C"});
        heap.insert(WarehousePackage{50, "Zone-A"});
        heap.insert(WarehousePackage{75, "Zone-B"});

        TEST_CHECK(heap.verifyHeapInvariants());

        WarehousePackage top{0, ""};
        TEST_CHECK(heap.peekMin(top));
        TEST_CHECK(top.binLocation == "Zone-A");

        std::vector<std::string> binOrder;
        while (heap.extractMin(top)) {
            binOrder.push_back(top.binLocation);
        }

        std::vector<std::string> expected = {"Zone-A", "Zone-B", "Zone-C"};
        TEST_CHECK(binOrder == expected);
    }
}

void testCopyAndMoveSemantics()
{
    MinHeap<int> heap1;
    heap1.insert(30);
    heap1.insert(10);
    heap1.insert(20);

    // Copy constructor
    MinHeap<int> heap2(heap1);
    TEST_CHECK(heap2.getSize() == 3);
    TEST_CHECK(heap1.getSize() == 3);
    TEST_CHECK(heap2.verifyHeapInvariants());

    // Copy assignment
    MinHeap<int> heap3;
    heap3 = heap2;
    TEST_CHECK(heap3.getSize() == 3);
    TEST_CHECK(heap3.verifyHeapInvariants());

    // Move constructor
    MinHeap<int> heap4(std::move(heap1));
    TEST_CHECK(heap4.getSize() == 3);
    TEST_CHECK(heap4.verifyHeapInvariants());

    // Moved-from heap1 remains in a valid, reusable state without assuming empty/specific size
    heap1.clear();
    TEST_CHECK(heap1.isEmpty());
    TEST_CHECK(heap1.getSize() == 0);
    heap1.insert(99);
    TEST_CHECK(heap1.getSize() == 1);
    TEST_CHECK(heap1.verifyHeapInvariants());
    int reval = 0;
    TEST_CHECK(heap1.peekMin(reval) && reval == 99);
    TEST_CHECK(heap1.extractMin(reval) && reval == 99);
    TEST_CHECK(heap1.isEmpty());

    // Move assignment
    MinHeap<int> heap5;
    heap5 = std::move(heap2);
    TEST_CHECK(heap5.getSize() == 3);
    TEST_CHECK(heap5.verifyHeapInvariants());

    // Moved-from heap2 remains in a valid, reusable state without assuming empty/specific size
    heap2.clear();
    TEST_CHECK(heap2.isEmpty());
    TEST_CHECK(heap2.getSize() == 0);
    heap2.insert(77);
    TEST_CHECK(heap2.getSize() == 1);
    TEST_CHECK(heap2.verifyHeapInvariants());
    TEST_CHECK(heap2.extractMin(reval) && reval == 77);
    TEST_CHECK(heap2.isEmpty());

    // Verify extracted contents from moved-into heap
    std::vector<int> extracted;
    int val = 0;
    while (heap5.extractMin(val)) {
        extracted.push_back(val);
    }
    std::vector<int> expected = {10, 20, 30};
    TEST_CHECK(extracted == expected);
}

void testClearAndReuse()
{
    MinHeap<int> heap;
    for (int i = 0; i < 50; ++i) {
        heap.insert((i * 47) % 100);
        TEST_CHECK(heap.verifyHeapInvariants());
    }
    TEST_CHECK(heap.getSize() == 50);

    // Partial extraction
    int val = 0;
    for (int i = 0; i < 15; ++i) {
        TEST_CHECK(heap.extractMin(val));
        TEST_CHECK(heap.verifyHeapInvariants());
    }
    TEST_CHECK(heap.getSize() == 35);

    // Clear
    heap.clear();
    TEST_CHECK(heap.isEmpty());
    TEST_CHECK(heap.getSize() == 0);
    TEST_CHECK(heap.verifyHeapInvariants());

    val = -42;
    TEST_CHECK(!heap.peekMin(val));
    TEST_CHECK(val == -42);
    TEST_CHECK(!heap.extractMin(val));
    TEST_CHECK(val == -42);

    // Reuse after clear
    heap.insert(90);
    heap.insert(10);
    heap.insert(50);
    TEST_CHECK(heap.getSize() == 3);
    TEST_CHECK(heap.verifyHeapInvariants());

    std::vector<int> extracted;
    while (heap.extractMin(val)) {
        extracted.push_back(val);
    }
    std::vector<int> expected = {10, 50, 90};
    TEST_CHECK(extracted == expected);
}

int main()
{
    TEST_RUN(testEmptyAndSingletonHeap);
    TEST_RUN(testInsertAndExtractOrdering);
    TEST_RUN(testDuplicateKeys);
    TEST_RUN(testSortedAndReverseSortedInput);
    TEST_RUN(testFloydBuildHeapConstructor);
    TEST_RUN(testCustomTypeAndComparator);
    TEST_RUN(testCopyAndMoveSemantics);
    TEST_RUN(testClearAndReuse);

    TEST_REPORT();
}
