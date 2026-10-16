#include "test_utils.h"
#include "task/FibonacciHeap.h"

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

void testEmptyAndSingleton()
{
    FibonacciHeap<int> heap;
    TEST_CHECK(heap.isEmpty());
    TEST_CHECK(heap.size() == 0);
    TEST_CHECK(heap.getSize() == 0);
    TEST_CHECK(heap.verifyHeapInvariants());

    int val = -1;
    TEST_CHECK(!heap.peek(val));
    TEST_CHECK(!heap.extract(val));

    bool threw = false;
    try {
        [[maybe_unused]] const int& top = heap.peek();
    } catch (const std::logic_error&) {
        threw = true;
    }
    TEST_CHECK(threw);

    auto handle = heap.insert(42);
    TEST_CHECK(!heap.isEmpty());
    TEST_CHECK(heap.size() == 1);
    TEST_CHECK(heap.verifyHeapInvariants());
    TEST_CHECK(handle.isValid());
    TEST_CHECK(*handle == 42);
    TEST_CHECK(heap.isHandleValid(handle));

    TEST_CHECK(heap.peek(val));
    TEST_CHECK(val == 42);
    TEST_CHECK(heap.peek() == 42);

    TEST_CHECK(heap.extract(val));
    TEST_CHECK(val == 42);
    TEST_CHECK(heap.isEmpty());
    TEST_CHECK(heap.size() == 0);
    TEST_CHECK(!handle.isValid());
    TEST_CHECK(!heap.isHandleValid(handle));
    TEST_CHECK(heap.verifyHeapInvariants());
}

void testExtractionOrderAndDuplicates()
{
    // Max-Heap tests (std::less<int>, higher value has higher priority)
    {
        FibonacciHeap<int> heap;
        std::vector<int> inputs = {15, 3, 20, 8, 12, 1, 25, 18, 5, 30, 2, 7, 20, 15, 30, 1};
        for (int x : inputs) {
            heap.insert(x);
            TEST_CHECK(heap.verifyHeapInvariants());
        }

        TEST_CHECK(heap.size() == inputs.size());

        std::vector<int> extracted;
        int val = 0;
        while (heap.extract(val)) {
            extracted.push_back(val);
            TEST_CHECK(heap.verifyHeapInvariants());
        }

        TEST_CHECK(extracted.size() == inputs.size());
        TEST_CHECK(std::is_sorted(extracted.begin(), extracted.end(), std::greater<int>()));
        TEST_CHECK(heap.isEmpty());
    }

    // Min-Heap tests (std::greater<int>, smaller value has higher priority)
    {
        FibonacciHeap<int, std::greater<int>> minHeap;
        std::vector<int> inputs = {40, 10, 50, 20, 30, 10, 60, 5, 25, 5};
        for (int x : inputs) {
            minHeap.insert(x);
            TEST_CHECK(minHeap.verifyHeapInvariants());
        }

        TEST_CHECK(minHeap.size() == inputs.size());

        std::vector<int> extracted;
        int val = 0;
        while (minHeap.extract(val)) {
            extracted.push_back(val);
            TEST_CHECK(minHeap.verifyHeapInvariants());
        }

        TEST_CHECK(extracted.size() == inputs.size());
        TEST_CHECK(std::is_sorted(extracted.begin(), extracted.end(), std::less<int>()));
        TEST_CHECK(minHeap.isEmpty());
    }
}

void testNodeHandleValidityAndSafety()
{
    FibonacciHeap<int>::NodeHandle defaultHandle;
    TEST_CHECK(!defaultHandle.isValid());
    TEST_CHECK(!defaultHandle);

    bool threw = false;
    try {
        [[maybe_unused]] int v = *defaultHandle;
    } catch (const std::logic_error&) {
        threw = true;
    }
    TEST_CHECK(threw);

    FibonacciHeap<int> heap1;
    TEST_CHECK(!heap1.isHandleValid(defaultHandle));
    TEST_CHECK(!heap1.decreaseKey(defaultHandle, 100));
    TEST_CHECK(!heap1.deleteNode(defaultHandle));

    auto h1 = heap1.insert(50);
    auto h2 = heap1.insert(30);
    auto h3 = heap1.insert(70);

    TEST_CHECK(h1.isValid() && *h1 == 50);
    TEST_CHECK(h2.isValid() && *h2 == 30);
    TEST_CHECK(h3.isValid() && *h3 == 70);
    TEST_CHECK(heap1.isHandleValid(h1));
    TEST_CHECK(heap1.isHandleValid(h2));
    TEST_CHECK(heap1.isHandleValid(h3));

    // Foreign handle test
    FibonacciHeap<int> heap2;
    auto foreignHandle = heap2.insert(99);
    TEST_CHECK(foreignHandle.isValid());
    TEST_CHECK(heap2.isHandleValid(foreignHandle));
    TEST_CHECK(!heap1.isHandleValid(foreignHandle));
    TEST_CHECK(!heap1.decreaseKey(foreignHandle, 150));
    TEST_CHECK(!heap1.deleteNode(foreignHandle));

    // Priority downgrade rejection test (Max-Heap: 50 -> 40 is a downgrade)
    TEST_CHECK(!heap1.decreaseKey(h1, 40));
    TEST_CHECK(*h1 == 50); // Value must not change on rejection

    // Extract max (70) -> h3 becomes stale
    int val = 0;
    TEST_CHECK(heap1.extract(val) && val == 70);
    TEST_CHECK(!h3.isValid());
    TEST_CHECK(!heap1.isHandleValid(h3));

    threw = false;
    try {
        [[maybe_unused]] int v = *h3;
    } catch (const std::logic_error&) {
        threw = true;
    }
    TEST_CHECK(threw);

    TEST_CHECK(!heap1.decreaseKey(h3, 100));
    TEST_CHECK(!heap1.deleteNode(h3));
}

void testSingleAndCascadingCuts()
{
    FibonacciHeap<int> heap;
    std::vector<FibonacciHeap<int>::NodeHandle> handles;

    // Insert 16 numbers and extract one to trigger consolidation
    for (int i = 0; i < 16; ++i) {
        handles.push_back(heap.insert(i * 10));
    }
    TEST_CHECK(heap.size() == 16);
    TEST_CHECK(heap.verifyHeapInvariants());

    // Extract maximum (150) -> triggers consolidation of remaining 15 nodes
    int maxVal = 0;
    TEST_CHECK(heap.extract(maxVal));
    TEST_CHECK(maxVal == 150);
    TEST_CHECK(heap.verifyHeapInvariants());

    // DecreaseKey on a node to exceed parent and trigger cut
    // Node with value 10 increased to 160 (new maximum)
    TEST_CHECK(heap.decreaseKey(handles[1], 160));
    TEST_CHECK(*handles[1] == 160);
    TEST_CHECK(heap.verifyHeapInvariants());
    TEST_CHECK(heap.peek() == 160);

    // Another cut on a sibling node to trigger cascading cut
    TEST_CHECK(heap.decreaseKey(handles[2], 170));
    TEST_CHECK(*handles[2] == 170);
    TEST_CHECK(heap.verifyHeapInvariants());
    TEST_CHECK(heap.peek() == 170);

    // Verify all remaining elements are extracted in strictly descending order
    std::vector<int> extracted;
    int val = 0;
    while (heap.extract(val)) {
        extracted.push_back(val);
        TEST_CHECK(heap.verifyHeapInvariants());
    }

    TEST_CHECK(extracted.size() == 15);
    TEST_CHECK(std::is_sorted(extracted.begin(), extracted.end(), std::greater<int>()));
    TEST_CHECK(extracted.front() == 170);
    TEST_CHECK(extracted[1] == 160);
    TEST_CHECK(heap.isEmpty());
}

void testMergeAndHandleTransfer()
{
    FibonacciHeap<int> heapA;
    FibonacciHeap<int> heapB;

    auto hA1 = heapA.insert(10);
    auto hA2 = heapA.insert(30);
    auto hA3 = heapA.insert(50);

    auto hB1 = heapB.insert(20);
    auto hB2 = heapB.insert(40);
    auto hB3 = heapB.insert(60);

    TEST_CHECK(heapA.size() == 3);
    TEST_CHECK(heapB.size() == 3);

    // Merge heapB into heapA
    heapA.merge(std::move(heapB));

    TEST_CHECK(heapB.isEmpty());
    TEST_CHECK(heapB.size() == 0);
    TEST_CHECK(heapA.size() == 6);
    TEST_CHECK(heapA.verifyHeapInvariants());

    // Handles originally created in heapB must now be valid in heapA
    TEST_CHECK(heapA.isHandleValid(hB1));
    TEST_CHECK(heapA.isHandleValid(hB2));
    TEST_CHECK(heapA.isHandleValid(hB3));
    TEST_CHECK(!heapB.isHandleValid(hB1));

    // Update key on a transferred handle
    TEST_CHECK(heapA.decreaseKey(hB2, 80));
    TEST_CHECK(*hB2 == 80);
    TEST_CHECK(heapA.peek() == 80);

    // Extract all from heapA
    std::vector<int> expected = {80, 60, 50, 30, 20, 10};
    std::vector<int> actual;
    int val = 0;
    while (heapA.extract(val)) {
        actual.push_back(val);
    }
    TEST_CHECK(actual == expected);
    TEST_CHECK(heapA.isEmpty());

    // Test merging empty heaps
    FibonacciHeap<int> empty1;
    FibonacciHeap<int> empty2;
    empty1.merge(std::move(empty2));
    TEST_CHECK(empty1.isEmpty());

    FibonacciHeap<int> nonEmpty;
    nonEmpty.insert(100);
    empty1.merge(std::move(nonEmpty));
    TEST_CHECK(empty1.size() == 1);
    TEST_CHECK(empty1.peek() == 100);
}

void testArbitraryDeleteNode()
{
    FibonacciHeap<int> heap;
    std::vector<FibonacciHeap<int>::NodeHandle> handles;
    for (int i = 1; i <= 8; ++i) {
        handles.push_back(heap.insert(i * 10));
    }
    TEST_CHECK(heap.size() == 8);

    // Delete current max (80)
    TEST_CHECK(heap.deleteNode(handles[7]));
    TEST_CHECK(!handles[7].isValid());
    TEST_CHECK(heap.size() == 7);
    TEST_CHECK(heap.peek() == 70);
    TEST_CHECK(heap.verifyHeapInvariants());

    // Delete internal / leaf nodes
    TEST_CHECK(heap.deleteNode(handles[2])); // 30
    TEST_CHECK(!handles[2].isValid());
    TEST_CHECK(heap.size() == 6);
    TEST_CHECK(heap.verifyHeapInvariants());

    TEST_CHECK(heap.deleteNode(handles[0])); // 10
    TEST_CHECK(!handles[0].isValid());
    TEST_CHECK(heap.size() == 5);
    TEST_CHECK(heap.verifyHeapInvariants());

    // Remaining: 70, 60, 50, 40, 20
    std::vector<int> expected = {70, 60, 50, 40, 20};
    std::vector<int> actual;
    int val = 0;
    while (heap.extract(val)) {
        actual.push_back(val);
    }
    TEST_CHECK(actual == expected);
    TEST_CHECK(heap.isEmpty());
}

void testClearAndMoveSemantics()
{
    FibonacciHeap<int> heap1;
    std::vector<FibonacciHeap<int>::NodeHandle> handles;
    for (int i = 0; i < 30; ++i) {
        handles.push_back(heap1.insert(i));
    }
    TEST_CHECK(heap1.size() == 30);

    // Move construction
    FibonacciHeap<int> heap2(std::move(heap1));
    TEST_CHECK(heap1.isEmpty());
    TEST_CHECK(heap2.size() == 30);
    TEST_CHECK(heap2.verifyHeapInvariants());
    TEST_CHECK(heap2.isHandleValid(handles[0]));
    TEST_CHECK(!heap1.isHandleValid(handles[0]));

    // Move assignment
    FibonacciHeap<int> heap3;
    heap3.insert(999);
    heap3 = std::move(heap2);
    TEST_CHECK(heap2.isEmpty());
    TEST_CHECK(heap3.size() == 30);
    TEST_CHECK(heap3.verifyHeapInvariants());
    TEST_CHECK(heap3.isHandleValid(handles[0]));

    // Clear
    heap3.clear();
    TEST_CHECK(heap3.isEmpty());
    TEST_CHECK(heap3.size() == 0);
    TEST_CHECK(heap3.verifyHeapInvariants());
    for (const auto& h : handles) {
        TEST_CHECK(!h.isValid());
        TEST_CHECK(!heap3.isHandleValid(h));
    }

    // Reuse after clear
    auto newHandle = heap3.insert(500);
    TEST_CHECK(heap3.size() == 1);
    TEST_CHECK(heap3.peek() == 500);
    TEST_CHECK(newHandle.isValid());
    TEST_CHECK(heap3.isHandleValid(newHandle));
}

namespace
{
struct ThrowOnAssignment
{
    int id{0};
    bool shouldThrow{false};

    ThrowOnAssignment() = default;
    ThrowOnAssignment(int i, bool t = false)
        : id(i)
        , shouldThrow(t)
    {
    }

    ThrowOnAssignment(const ThrowOnAssignment&) = default;
    ThrowOnAssignment(ThrowOnAssignment&&) = default;

    ThrowOnAssignment& operator=(const ThrowOnAssignment& other) = default;

    ThrowOnAssignment& operator=(ThrowOnAssignment&& other)
    {
        if (other.shouldThrow) {
            throw std::runtime_error("Simulated move assignment failure");
        }
        id = other.id;
        shouldThrow = other.shouldThrow;
        return *this;
    }

    bool operator<(const ThrowOnAssignment& other) const noexcept
    {
        return id < other.id;
    }
};
} // namespace

void testExtractionExceptionSafety()
{
    FibonacciHeap<ThrowOnAssignment> heap;
    heap.insert(ThrowOnAssignment{10, false});
    heap.insert(ThrowOnAssignment{20, false});
    heap.insert(ThrowOnAssignment{50, true}); // Will throw upon move-assignment in extract
    heap.insert(ThrowOnAssignment{30, false});

    TEST_CHECK(heap.size() == 4);

    ThrowOnAssignment target;
    bool caughtException = false;
    try {
        heap.extract(target);
    } catch (const std::runtime_error&) {
        caughtException = true;
    }
    TEST_CHECK(caughtException);

    // Basic Exception Guarantee:
    // Size is decremented, extracted node memory is safely reclaimed, heap remains valid.
    TEST_CHECK(heap.size() == 3);
    TEST_CHECK(heap.verifyHeapInvariants());

    // Remaining nodes (30, 20, 10) can still be extracted successfully
    std::vector<int> extractedIds;
    while (heap.extract(target)) {
        extractedIds.push_back(target.id);
        TEST_CHECK(heap.verifyHeapInvariants());
    }

    std::vector<int> expectedIds = {30, 20, 10};
    TEST_CHECK(extractedIds == expectedIds);
    TEST_CHECK(heap.isEmpty());
}

namespace
{
struct VertexDistance
{
    int vertex{0};
    int dist{0};

    bool operator==(const VertexDistance& other) const noexcept
    {
        return vertex == other.vertex && dist == other.dist;
    }
};

struct MinVertexDistanceCompare
{
    bool operator()(const VertexDistance& a, const VertexDistance& b) const noexcept
    {
        return a.dist > b.dist; // Smaller distance has higher priority
    }
};

struct Edge
{
    int to{0};
    int weight{0};
};
} // namespace

void testSimulatedDijkstraRelaxation()
{
    // Directed graph with 6 vertices: 0 to 5
    // 0 -> (1, 7), (2, 9), (5, 14)
    // 1 -> (2, 10), (3, 15)
    // 2 -> (3, 11), (5, 2)
    // 3 -> (4, 6)
    // 4 -> (5, 9)
    // 5 -> none
    const int numVertices = 6;
    std::vector<std::vector<Edge>> adj(numVertices);
    adj[0] = {{1, 7}, {2, 9}, {5, 14}};
    adj[1] = {{2, 10}, {3, 15}};
    adj[2] = {{3, 11}, {5, 2}};
    adj[3] = {{4, 6}};
    adj[4] = {{5, 9}};

    constexpr int INF = 1000000;
    std::vector<int> dist(numVertices, INF);
    dist[0] = 0;

    FibonacciHeap<VertexDistance, MinVertexDistanceCompare> heap;
    std::vector<FibonacciHeap<VertexDistance, MinVertexDistanceCompare>::NodeHandle> handles(numVertices);

    for (int v = 0; v < numVertices; ++v) {
        handles[v] = heap.insert(VertexDistance{v, dist[v]});
    }

    TEST_CHECK(heap.size() == static_cast<std::size_t>(numVertices));
    TEST_CHECK(heap.verifyHeapInvariants());

    std::vector<bool> visited(numVertices, false);
    VertexDistance curr;

    while (heap.extract(curr)) {
        int u = curr.vertex;
        visited[u] = true;
        TEST_CHECK(heap.verifyHeapInvariants());

        for (const auto& edge : adj[u]) {
            int v = edge.to;
            int weight = edge.weight;

            if (!visited[v] && dist[u] + weight < dist[v]) {
                dist[v] = dist[u] + weight;
                bool updated = heap.decreaseKey(handles[v], VertexDistance{v, dist[v]});
                TEST_CHECK(updated);
                TEST_CHECK(heap.verifyHeapInvariants());
            }
        }
    }

    TEST_CHECK(heap.isEmpty());

    // Expected shortest paths from 0:
    // dist[0] = 0
    // dist[1] = 7
    // dist[2] = 9
    // dist[3] = 0 -> 2 -> 3 = 9 + 11 = 20
    // dist[4] = 0 -> 2 -> 3 -> 4 = 20 + 6 = 26
    // dist[5] = 0 -> 2 -> 5 = 9 + 2 = 11
    std::vector<int> expectedDist = {0, 7, 9, 20, 26, 11};
    TEST_CHECK(dist == expectedDist);

    for (int v = 0; v < numVertices; ++v) {
        TEST_CHECK(!handles[v].isValid());
    }
}

namespace
{
struct NonDefaultConstructible
{
    int id;
    std::string name;

    NonDefaultConstructible() = delete;

    explicit NonDefaultConstructible(int i, std::string n)
        : id(i)
        , name(std::move(n))
    {
    }

    NonDefaultConstructible(const NonDefaultConstructible&) = default;
    NonDefaultConstructible(NonDefaultConstructible&&) = default;
    NonDefaultConstructible& operator=(const NonDefaultConstructible&) = default;
    NonDefaultConstructible& operator=(NonDefaultConstructible&&) = default;

    bool operator<(const NonDefaultConstructible& other) const noexcept
    {
        return id < other.id;
    }
};

static_assert(!std::is_default_constructible_v<NonDefaultConstructible>,
              "NonDefaultConstructible must not be default-constructible");
} // namespace

void testNonDefaultConstructibleDeleteNode()
{
    FibonacciHeap<NonDefaultConstructible> heap;
    TEST_CHECK(heap.isEmpty());

    auto h1 = heap.insert(NonDefaultConstructible{10, "item10"});
    auto h2 = heap.insert(NonDefaultConstructible{50, "item50"}); // Maximum / best node
    auto h3 = heap.insert(NonDefaultConstructible{30, "item30"});
    auto h4 = heap.insert(NonDefaultConstructible{40, "item40"});

    TEST_CHECK(heap.size() == 4);
    TEST_CHECK(heap.verifyHeapInvariants());
    TEST_CHECK(heap.peek().id == 50);

    // Delete the best/root node directly (previously failed compilation with T dummy;)
    TEST_CHECK(heap.deleteNode(h2));
    TEST_CHECK(!h2.isValid());
    TEST_CHECK(!heap.isHandleValid(h2));

    bool threw = false;
    try {
        [[maybe_unused]] const auto& val = *h2;
    } catch (const std::logic_error&) {
        threw = true;
    }
    TEST_CHECK(threw);

    TEST_CHECK(heap.size() == 3);
    TEST_CHECK(heap.verifyHeapInvariants());
    TEST_CHECK(heap.peek().id == 40);

    // Delete a non-best node
    TEST_CHECK(heap.deleteNode(h1));
    TEST_CHECK(!h1.isValid());
    TEST_CHECK(heap.size() == 2);
    TEST_CHECK(heap.verifyHeapInvariants());

    // Extract remaining elements in order (40, then 30)
    NonDefaultConstructible target{0, ""};
    TEST_CHECK(heap.extract(target));
    TEST_CHECK(target.id == 40);
    TEST_CHECK(target.name == "item40");
    TEST_CHECK(heap.size() == 1);

    TEST_CHECK(heap.extract(target));
    TEST_CHECK(target.id == 30);
    TEST_CHECK(target.name == "item30");
    TEST_CHECK(heap.isEmpty());

    // Test deleteNode on singleton heap of non-default-constructible type
    auto hSingle = heap.insert(NonDefaultConstructible{99, "singleton"});
    TEST_CHECK(heap.size() == 1);
    TEST_CHECK(heap.deleteNode(hSingle));
    TEST_CHECK(heap.isEmpty());
    TEST_CHECK(!hSingle.isValid());
    TEST_CHECK(heap.verifyHeapInvariants());
}

int main()
{
    TEST_RUN(testEmptyAndSingleton);
    TEST_RUN(testExtractionOrderAndDuplicates);
    TEST_RUN(testNodeHandleValidityAndSafety);
    TEST_RUN(testSingleAndCascadingCuts);
    TEST_RUN(testMergeAndHandleTransfer);
    TEST_RUN(testArbitraryDeleteNode);
    TEST_RUN(testClearAndMoveSemantics);
    TEST_RUN(testExtractionExceptionSafety);
    TEST_RUN(testSimulatedDijkstraRelaxation);
    TEST_RUN(testNonDefaultConstructibleDeleteNode);

    TEST_REPORT();
}
