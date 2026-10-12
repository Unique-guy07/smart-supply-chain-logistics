#pragma once

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

/// Custom templated binary Max-Heap.
///
/// Theoretical foundation:
/// Complete binary tree mapped into a contiguous std::vector<T> with zero-based indexing:
///   Parent of node i:     (i - 1) / 2
///   Left child of node i:  2 * i + 1
///   Right child of node i: 2 * i + 2
///
/// Heap invariant:
///   For every node i > 0, !comp(data[parent(i)], data[i]).
///   With default Compare = std::less<T>, parent >= child for all nodes.
///   The maximum element resides at index 0 (root).
///
/// Complexity guarantees:
///   - peekMax: O(1)
///   - insert: O(log N)
///   - extractMax: O(log N)
///   - buildHeap (Floyd's bottom-up construction): O(N)
///   - verifyHeapInvariants: O(N)
///   - size / isEmpty / clear: O(1)
///
/// Exception safety:
///   - Strong guarantee on peekMax (does not mutate heap).
///   - Strong guarantee on insert when T's move/swap operations are noexcept and Compare is non-throwing.
///   - Basic guarantee on extractMax against throwing moves.
template <typename T, typename Compare = std::less<T>>
class MaxHeap
{
private:
    std::vector<T> data;
    Compare comp;

    static constexpr std::size_t parent(std::size_t index) noexcept
    {
        return (index - 1) / 2;
    }

    static constexpr std::size_t leftChild(std::size_t index) noexcept
    {
        return 2 * index + 1;
    }

    static constexpr std::size_t rightChild(std::size_t index) noexcept
    {
        return 2 * index + 2;
    }

    void siftUp(std::size_t index)
    {
        while (index > 0) {
            std::size_t p = parent(index);
            // If parent is less than child according to comp, violate max-heap invariant -> swap
            if (comp(data[p], data[index])) {
                std::swap(data[p], data[index]);
                index = p;
            } else {
                break;
            }
        }
    }

    void siftDown(std::size_t index)
    {
        const std::size_t n = data.size();
        while (true) {
            std::size_t largest = index;
            const std::size_t left = leftChild(index);
            const std::size_t right = rightChild(index);

            if (left < n && comp(data[largest], data[left])) {
                largest = left;
            }
            if (right < n && comp(data[largest], data[right])) {
                largest = right;
            }

            if (largest != index) {
                std::swap(data[index], data[largest]);
                index = largest;
            } else {
                break;
            }
        }
    }

    /// Bottom-up heap construction using Floyd's algorithm in O(N).
    void buildHeap()
    {
        if (data.size() <= 1) {
            return;
        }
        // Last non-leaf node index: (data.size() - 2) / 2
        for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>((data.size() - 2) / 2); i >= 0; --i) {
            siftDown(static_cast<std::size_t>(i));
        }
    }

public:
    MaxHeap() = default;

    explicit MaxHeap(const Compare& comparator)
        : data()
        , comp(comparator)
    {
    }

    /// Constructs a Max-Heap from an existing vector of elements using Floyd's bottom-up
    /// build-heap algorithm in O(N) time.
    explicit MaxHeap(std::vector<T> elements, const Compare& comparator = {})
        : data(std::move(elements))
        , comp(comparator)
    {
        buildHeap();
    }

    ~MaxHeap() = default;

    // Rule of 5: value semantics with conditional noexcept moves
    MaxHeap(const MaxHeap&) = default;
    MaxHeap& operator=(const MaxHeap&) = default;

    MaxHeap(MaxHeap&& other) noexcept(
        std::is_nothrow_move_constructible_v<std::vector<T>> &&
        std::is_nothrow_move_constructible_v<Compare>)
        : data(std::move(other.data))
        , comp(std::move(other.comp))
    {
    }

    MaxHeap& operator=(MaxHeap&& other) noexcept(
        std::is_nothrow_move_assignable_v<std::vector<T>> &&
        std::is_nothrow_move_assignable_v<Compare>)
    {
        if (this != &other) {
            data = std::move(other.data);
            comp = std::move(other.comp);
        }
        return *this;
    }

    /// Inserts a new element into the heap in O(log N).
    void insert(const T& value)
    {
        data.push_back(value);
        siftUp(data.size() - 1);
    }

    /// Inserts a new element into the heap via move in O(log N).
    void insert(T&& value)
    {
        data.push_back(std::move(value));
        siftUp(data.size() - 1);
    }

    /// Inspects the maximum element without removing it in O(1).
    /// Returns false if the heap is empty, true otherwise.
    bool peekMax(T& outValue) const
    {
        if (data.empty()) {
            return false;
        }
        outValue = data[0];
        return true;
    }

    /// Extracts and removes the maximum element in O(log N).
    /// Returns false if the heap is empty, true otherwise.
    bool extractMax(T& outValue)
    {
        if (data.empty()) {
            return false;
        }
        outValue = std::move(data[0]);
        if (data.size() == 1) {
            data.pop_back();
            return true;
        }

        data[0] = std::move(data.back());
        data.pop_back();
        siftDown(0);
        return true;
    }

    /// Returns the number of elements in the heap.
    [[nodiscard]] std::size_t getSize() const noexcept
    {
        return data.size();
    }

    /// Returns true if the heap contains zero elements.
    [[nodiscard]] bool isEmpty() const noexcept
    {
        return data.empty();
    }

    /// Empties the heap in O(1).
    void clear() noexcept
    {
        data.clear();
    }

    /// Rebuilds the heap using Floyd's algorithm on a newly provided vector in O(N).
    void buildFrom(std::vector<T> elements)
    {
        data = std::move(elements);
        buildHeap();
    }

    /// Access to internal underlying container for testing/inspection.
    [[nodiscard]] const std::vector<T>& getElements() const noexcept
    {
        return data;
    }

    /// Diagnostic verification that all nodes satisfy max-heap property:
    /// For every node, neither left nor right child is greater than the node.
    [[nodiscard]] bool verifyHeapInvariants() const noexcept(
        std::is_nothrow_invocable_v<const Compare&, const T&, const T&>)
    {
        const std::size_t n = data.size();
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t left = leftChild(i);
            const std::size_t right = rightChild(i);

            if (left < n && comp(data[i], data[left])) {
                return false;
            }
            if (right < n && comp(data[i], data[right])) {
                return false;
            }
        }
        return true;
    }
};
