#include "test_utils.h"
#include "task/MaxHeap.h"
#include <algorithm>
#include <string>
#include <vector>

void testEmptyAndSingletonHeap()
{
    MaxHeap<int> heap;
    TEST_CHECK(heap.isEmpty());
    TEST_CHECK(heap.getSize() == 0);
    TEST_CHECK(heap.verifyHeapInvariants());

    int val = -1;
    TEST_CHECK(!heap.peekMax(val));
    TEST_CHECK(!heap.extractMax(val));

    heap.insert(42);
    TEST_CHECK(!heap.isEmpty());
    TEST_CHECK(heap.getSize() == 1);
    TEST_CHECK(heap.verifyHeapInvariants());

    TEST_CHECK(heap.peekMax(val));
    TEST_CHECK(val == 42);

    TEST_CHECK(heap.extractMax(val));
    TEST_CHECK(val == 42);
    TEST_CHECK(heap.isEmpty());
    TEST_CHECK(heap.getSize() == 0);
    TEST_CHECK(heap.verifyHeapInvariants());
}

void testInsertAndExtractOrdering()
{
    MaxHeap<int> heap;
    std::vector<int> inputs = {15, 3, 20, 8, 12, 1, 25, 18};
    for (int x : inputs) {
        heap.insert(x);
        TEST_CHECK(heap.verifyHeapInvariants());
    }

    TEST_CHECK(heap.getSize() == inputs.size());

    std::vector<int> extracted;
    int val = 0;
    while (heap.extractMax(val)) {
        extracted.push_back(val);
        TEST_CHECK(heap.verifyHeapInvariants());
    }

    TEST_CHECK(heap.isEmpty());
    std::vector<int> expected = {25, 20, 18, 15, 12, 8, 3, 1};
    TEST_CHECK(extracted == expected);
}

void testDuplicateKeys()
{
    MaxHeap<int> heap;
    std::vector<int> inputs = {10, 20, 10, 30, 20, 10, 30};
    for (int x : inputs) {
        heap.insert(x);
    }
    TEST_CHECK(heap.verifyHeapInvariants());
    TEST_CHECK(heap.getSize() == 7);

    std::vector<int> extracted;
    int val = 0;
    while (heap.extractMax(val)) {
        extracted.push_back(val);
    }
    std::vector<int> expected = {30, 30, 20, 20, 10, 10, 10};
    TEST_CHECK(extracted == expected);
}

void testFloydBuildHeap()
{
    std::vector<int> data = {9, 2, 7, 1, 5, 6, 8, 3, 4, 0};
    MaxHeap<int> heap(data);

    TEST_CHECK(heap.getSize() == data.size());
    TEST_CHECK(heap.verifyHeapInvariants());

    int top = 0;
    TEST_CHECK(heap.peekMax(top));
    TEST_CHECK(top == 9);

    std::vector<int> extracted;
    int val = 0;
    while (heap.extractMax(val)) {
        extracted.push_back(val);
    }

    std::vector<int> expected = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    TEST_CHECK(extracted == expected);
}

void testCustomComparator()
{
    // std::greater<int> causes MaxHeap to act as a Min-Heap
    MaxHeap<int, std::greater<int>> minHeap;
    minHeap.insert(50);
    minHeap.insert(10);
    minHeap.insert(30);
    minHeap.insert(5);
    minHeap.insert(20);

    TEST_CHECK(minHeap.verifyHeapInvariants());

    int top = 0;
    TEST_CHECK(minHeap.peekMax(top));
    TEST_CHECK(top == 5); // Minimum element at root

    std::vector<int> extracted;
    int val = 0;
    while (minHeap.extractMax(val)) {
        extracted.push_back(val);
    }
    std::vector<int> expected = {5, 10, 20, 30, 50};
    TEST_CHECK(extracted == expected);
}

struct Item
{
    int priority;
    std::string id;

    bool operator<(const Item& other) const noexcept
    {
        if (priority != other.priority) {
            return priority < other.priority;
        }
        return id > other.id; // Deterministic tie-breaker
    }
};

void testStructWithTieBreaker()
{
    MaxHeap<Item> heap;
    heap.insert(Item{10, "B"});
    heap.insert(Item{20, "A"});
    heap.insert(Item{10, "A"});
    heap.insert(Item{20, "B"});

    TEST_CHECK(heap.verifyHeapInvariants());

    std::vector<std::string> order;
    Item it{0, ""};
    while (heap.extractMax(it)) {
        order.push_back(it.id + std::to_string(it.priority));
    }

    // Expected: priority 20 ties resolved: "A" before "B"; priority 10 ties resolved: "A" before "B"
    std::vector<std::string> expected = {"A20", "B20", "A10", "B10"};
    TEST_CHECK(order == expected);
}

void testMixedOperationsAndClear()
{
    MaxHeap<int> heap;
    for (int i = 0; i < 50; ++i) {
        heap.insert((i * 37) % 100);
        TEST_CHECK(heap.verifyHeapInvariants());
    }

    TEST_CHECK(heap.getSize() == 50);

    int val = 0;
    for (int i = 0; i < 20; ++i) {
        TEST_CHECK(heap.extractMax(val));
        TEST_CHECK(heap.verifyHeapInvariants());
    }

    TEST_CHECK(heap.getSize() == 30);

    heap.clear();
    TEST_CHECK(heap.isEmpty());
    TEST_CHECK(heap.getSize() == 0);
    TEST_CHECK(!heap.extractMax(val));
}

int main()
{
    TEST_RUN(testEmptyAndSingletonHeap);
    TEST_RUN(testInsertAndExtractOrdering);
    TEST_RUN(testDuplicateKeys);
    TEST_RUN(testFloydBuildHeap);
    TEST_RUN(testCustomComparator);
    TEST_RUN(testStructWithTieBreaker);
    TEST_RUN(testMixedOperationsAndClear);
    TEST_REPORT();
}
