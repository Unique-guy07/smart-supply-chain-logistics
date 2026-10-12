#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <utility>
#include <vector>

namespace logistics::algo {

namespace detail {

/// Sifts down element at index i within a 0-indexed range of length n [first, first + n)
/// using Floyd's sift-down logic and comparator comp.
template <typename RandomIt, typename Compare>
void siftDown(RandomIt first, std::ptrdiff_t n, std::ptrdiff_t i, Compare& comp)
{
    while (true) {
        std::ptrdiff_t largest = i;
        const std::ptrdiff_t left = 2 * i + 1;
        const std::ptrdiff_t right = 2 * i + 2;

        if (left < n && comp(*(first + largest), *(first + left))) {
            largest = left;
        }
        if (right < n && comp(*(first + largest), *(first + right))) {
            largest = right;
        }

        if (largest != i) {
            std::iter_swap(first + i, first + largest);
            i = largest;
        } else {
            break;
        }
    }
}

} // namespace detail

/// In-place Heap Sort algorithm over a random-access iterator range [first, last).
///
/// Theoretical complexity:
///   Time: O(N log N) worst-case, O(N log N) average-case, O(N log N) best-case.
///   Auxiliary Space: O(1) in-place memory overhead.
///
/// Ordering convention:
///   With default Compare = std::less<>, sorts elements in non-decreasing (ascending) order.
///   With Compare = std::greater<>, sorts elements in non-increasing (descending) order.
///
/// Note: Heap Sort is not a stable sort. For deterministic ordering of equivalent items,
/// the element type or comparator should incorporate a secondary tie-breaker.
template <typename RandomIt, typename Compare = std::less<>>
void heapSort(RandomIt first, RandomIt last, Compare comp = {})
{
    const std::ptrdiff_t n = last - first;
    if (n <= 1) {
        return; // Empty or singleton range is trivially sorted
    }

    // Phase 1: Floyd's bottom-up Max-Heap construction in O(N).
    // The last non-leaf node index in a 0-indexed complete binary tree of size n is (n - 2) / 2.
    for (std::ptrdiff_t i = (n - 2) / 2; i >= 0; --i) {
        detail::siftDown(first, n, i, comp);
    }

    // Phase 2: In-place extraction phase in O(N log N).
    // Repeatedly swap the root (current maximum) to the end of the unsorted subrange,
    // and restore the heap invariant on the prefix [0, i).
    for (std::ptrdiff_t i = n - 1; i > 0; --i) {
        std::iter_swap(first, first + i);
        detail::siftDown(first, i, 0, comp);
    }
}

/// Convenience overload for std::vector<T>.
template <typename T, typename Compare = std::less<T>>
void heapSort(std::vector<T>& vec, Compare comp = {})
{
    heapSort(vec.begin(), vec.end(), comp);
}

} // namespace logistics::algo
