#pragma once

#include "task/MaxHeap.h"

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

/// Custom templated binary Min-Heap adapter.
///
/// Theoretical foundation & design:
/// Implemented as a zero-overhead adapter over the existing binary MaxHeap<T, Compare>.
/// In the repository's heap comparator convention:
///   - MaxHeap satisfies for every node i > 0: !comp(data[parent(i)], data[i]).
///   - Sifting up/down swaps when comp(parent, child) is true.
///   - Under Compare = std::greater<T>, comp(a, b) evaluates (a > b).
///   - If parent > child, elements are swapped, bubbling smaller values to the top.
///   - Hence, with default Compare = std::greater<T>, the root at index 0 is the minimum element,
///     and parent <= child holds throughout the complete binary tree.
///
/// Complexity guarantees (delegated to MaxHeap):
///   - peekMin: O(1)
///   - insert: O(log N)
///   - extractMin: O(log N)
///   - buildHeap (Floyd's bottom-up construction): O(N)
///   - verifyHeapInvariants: O(N)
///   - getSize / isEmpty / clear: O(1)
///
/// Exception safety:
///   - Inherited directly from underlying MaxHeap:
///     * Strong guarantee for heap state on peekMin: does not mutate the heap; returns false
///       if empty leaving outValue untouched. If assignment to outValue throws, the heap is unmodified,
///       though outValue's state depends on T's assignment operator exception guarantees.
///     * Strong guarantee on insert when T's move/swap operations are noexcept and Compare is non-throwing.
///     * Basic guarantee on extractMin against throwing moves.
template <typename T, typename Compare = std::greater<T>>
class MinHeap
{
private:
    MaxHeap<T, Compare> maxHeap;

public:
    MinHeap() = default;

    explicit MinHeap(const Compare& comparator)
        : maxHeap(comparator)
    {
    }

    /// Constructs a Min-Heap from an existing vector of elements using Floyd's bottom-up
    /// build-heap algorithm in O(N) time.
    explicit MinHeap(std::vector<T> elements, const Compare& comparator = {})
        : maxHeap(std::move(elements), comparator)
    {
    }

    ~MinHeap() = default;

    // Rule of 5: value semantics preserving underlying MaxHeap copy/move guarantees
    MinHeap(const MinHeap&) = default;
    MinHeap& operator=(const MinHeap&) = default;

    MinHeap(MinHeap&& other) noexcept(
        std::is_nothrow_move_constructible_v<MaxHeap<T, Compare>>) = default;

    MinHeap& operator=(MinHeap&& other) noexcept(
        std::is_nothrow_move_assignable_v<MaxHeap<T, Compare>>) = default;

    /// Inserts a new element into the heap in O(log N).
    void insert(const T& value)
    {
        maxHeap.insert(value);
    }

    /// Inserts a new element into the heap via move in O(log N).
    void insert(T&& value)
    {
        maxHeap.insert(std::move(value));
    }

    /// Inspects the minimum element without removing it in O(1).
    /// Returns false if the heap is empty (leaving outValue untouched), true otherwise.
    /// Provides the strong exception guarantee for the heap (the heap is never modified).
    /// Note: if outValue assignment throws, the heap remains intact, but outValue is not
    /// guaranteed to remain unchanged unless T's assignment operator provides that guarantee.
    bool peekMin(T& outValue) const
    {
        return maxHeap.peekMax(outValue);
    }

    /// Extracts and removes the minimum element in O(log N).
    /// Returns false if the heap is empty (leaving outValue untouched), true otherwise.
    bool extractMin(T& outValue)
    {
        return maxHeap.extractMax(outValue);
    }

    /// Returns the number of elements in the heap in O(1).
    [[nodiscard]] std::size_t getSize() const noexcept
    {
        return maxHeap.getSize();
    }

    /// Returns true if the heap contains zero elements in O(1).
    [[nodiscard]] bool isEmpty() const noexcept
    {
        return maxHeap.isEmpty();
    }

    /// Empties the heap in O(1).
    void clear() noexcept
    {
        maxHeap.clear();
    }

    /// Rebuilds the heap using Floyd's algorithm on a newly provided vector in O(N).
    void buildFrom(std::vector<T> elements)
    {
        maxHeap.buildFrom(std::move(elements));
    }

    /// Access to internal underlying container for testing/inspection.
    [[nodiscard]] const std::vector<T>& getElements() const noexcept
    {
        return maxHeap.getElements();
    }

    /// Diagnostic verification that all nodes satisfy min-heap property.
    [[nodiscard]] bool verifyHeapInvariants() const noexcept(
        std::is_nothrow_invocable_v<const Compare&, const T&, const T&>)
    {
        return maxHeap.verifyHeapInvariants();
    }
};
