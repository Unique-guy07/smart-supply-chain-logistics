#pragma once

#include <bit>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

/// Custom templated Min-Max Heap implementing a Double-Ended Priority Queue (DEPQ).
///
/// Theoretical foundation: Atkinson, Sack, Santoro, Strothotte (1986).
/// Complete binary tree mapped into a contiguous std::vector<T>.
/// Alternating levels:
///   Level 0 (root): Min-level
///   Level 1: Max-level
///   Level 2: Min-level
///   Level 3: Max-level, ...
///
/// Level calculation:
///   For node at index i (0-indexed), depth is floor(log2(i + 1)).
///   Calculated in O(1) via std::bit_width(i + 1) - 1.
///   Even depth -> Min-level
///   Odd depth  -> Max-level
///
/// Invariants:
///   - Node on Min-level: Key <= all descendant keys in its subtree.
///   - Node on Max-level: Key >= all descendant keys in its subtree.
///
/// Complexity guarantees:
///   - peekMin / peekMax: O(1)
///   - insert: O(log N)
///   - extractMin / extractMax: O(log N)
///   - removeAt / removeIf: O(log N) after locate
///   - verifyHeapInvariants: O(N)
///
/// Exception safety guarantees:
///   - insert:
///       * For types where Compare is non-throwing (is_nothrow_invocable) and T's move/swap
///         operations are noexcept, insert provides the STRONG exception guarantee:
///         the only potential exception is std::bad_alloc from vector growth, which leaves
///         the heap unmodified.
///       * For arbitrary throwing Compare or throwing T move/swap operations, insert provides
///         the BASIC exception guarantee: no memory is leaked and the container remains
///         valid and destructible, but heap invariants may not be preserved if an exception
///         escapes during trickle-up comparisons or swaps.
///   - peekMin / peekMax: Strong guarantee; does not modify the heap. Populates outValue via copy.
///   - extractMin / extractMax: Basic guarantee for arbitrary throwing types; if T's move or
///     Compare throws, the container remains valid and destructible with no memory leaks.
///   - removeAt / removeIf: Basic guarantee for arbitrary throwing types; strong/no-throw after
///     element extraction when Compare and T move/swap operations are noexcept.
///   - clear / isEmpty / getSize: Unconditionally noexcept.
///   - move construction / move assignment / getMaxIndex / verifyHeapInvariants:
///     Conditionally noexcept based on std::vector and Compare type traits.
template <typename T, typename Compare = std::less<T>>
class MinMaxHeap
{
private:
    std::vector<T> data;
    Compare comp;

    static constexpr bool isMinLevel(std::size_t index) noexcept
    {
        // Level is floor(log2(index + 1))
        const unsigned long long val = static_cast<unsigned long long>(index + 1);
        const int level = std::bit_width(val) - 1;
        return (level % 2 == 0);
    }

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

    void trickleUpMin(std::size_t i)
    {
        // Grandparent exists if parent(i) > 0 <=> (i - 1)/2 >= 1 <=> i >= 3
        while (i >= 3) {
            std::size_t gp = parent(parent(i));
            if (comp(data[i], data[gp])) {
                std::swap(data[i], data[gp]);
                i = gp;
            } else {
                break;
            }
        }
    }

    void trickleUpMax(std::size_t i)
    {
        while (i >= 3) {
            std::size_t gp = parent(parent(i));
            if (comp(data[gp], data[i])) {
                std::swap(data[gp], data[i]);
                i = gp;
            } else {
                break;
            }
        }
    }

    void trickleUp(std::size_t i)
    {
        if (i == 0) {
            return;
        }

        std::size_t p = parent(i);
        if (isMinLevel(i)) {
            if (comp(data[p], data[i])) {
                std::swap(data[i], data[p]);
                trickleUpMax(p);
            } else {
                trickleUpMin(i);
            }
        } else {
            if (comp(data[i], data[p])) {
                std::swap(data[i], data[p]);
                trickleUpMin(p);
            } else {
                trickleUpMax(i);
            }
        }
    }

    void trickleDownMin(std::size_t i)
    {
        while (leftChild(i) < data.size()) {
            std::size_t m = leftChild(i);
            // Check right child
            if (rightChild(i) < data.size() && comp(data[rightChild(i)], data[m])) {
                m = rightChild(i);
            }

            // Check 4 grandchildren: 4i+3, 4i+4, 4i+5, 4i+6
            const std::size_t gcStart = 4 * i + 3;
            for (std::size_t k = 0; k < 4; ++k) {
                const std::size_t gc = gcStart + k;
                if (gc < data.size()) {
                    if (comp(data[gc], data[m])) {
                        m = gc;
                    }
                } else {
                    break;
                }
            }

            // Check if m is a grandchild of i (gcStart <= m)
            if (m >= gcStart) {
                if (comp(data[m], data[i])) {
                    std::swap(data[m], data[i]);
                    std::size_t p = parent(m);
                    if (comp(data[p], data[m])) {
                        std::swap(data[p], data[m]);
                    }
                    i = m; // Continue trickling down from m
                } else {
                    break;
                }
            } else {
                // m is a direct child of i
                if (comp(data[m], data[i])) {
                    std::swap(data[m], data[i]);
                }
                break;
            }
        }
    }

    void trickleDownMax(std::size_t i)
    {
        while (leftChild(i) < data.size()) {
            std::size_t m = leftChild(i);
            // Check right child
            if (rightChild(i) < data.size() && comp(data[m], data[rightChild(i)])) {
                m = rightChild(i);
            }

            // Check 4 grandchildren
            const std::size_t gcStart = 4 * i + 3;
            for (std::size_t k = 0; k < 4; ++k) {
                const std::size_t gc = gcStart + k;
                if (gc < data.size()) {
                    if (comp(data[m], data[gc])) {
                        m = gc;
                    }
                } else {
                    break;
                }
            }

            if (m >= gcStart) {
                if (comp(data[i], data[m])) {
                    std::swap(data[i], data[m]);
                    std::size_t p = parent(m);
                    if (comp(data[m], data[p])) {
                        std::swap(data[m], data[p]);
                    }
                    i = m;
                } else {
                    break;
                }
            } else {
                if (comp(data[i], data[m])) {
                    std::swap(data[i], data[m]);
                }
                break;
            }
        }
    }

    void trickleDown(std::size_t i)
    {
        if (isMinLevel(i)) {
            trickleDownMin(i);
        } else {
            trickleDownMax(i);
        }
    }

    std::size_t getMaxIndex() const noexcept(std::is_nothrow_invocable_v<const Compare&, const T&, const T&>)
    {
        if (data.size() <= 1) {
            return 0;
        }
        if (data.size() == 2) {
            return 1;
        }
        return comp(data[1], data[2]) ? 2 : 1;
    }

public:
    MinMaxHeap() = default;

    explicit MinMaxHeap(const Compare& comparator)
        : data()
        , comp(comparator)
    {
    }

    ~MinMaxHeap() = default;

    MinMaxHeap(const MinMaxHeap&) = delete;
    MinMaxHeap& operator=(const MinMaxHeap&) = delete;

    MinMaxHeap(MinMaxHeap&& other) noexcept(
        std::is_nothrow_move_constructible_v<std::vector<T>> &&
        std::is_nothrow_move_constructible_v<Compare>)
        : data(std::move(other.data))
        , comp(std::move(other.comp))
    {
    }

    MinMaxHeap& operator=(MinMaxHeap&& other) noexcept(
        std::is_nothrow_move_assignable_v<std::vector<T>> &&
        std::is_nothrow_move_assignable_v<Compare>)
    {
        if (this != &other) {
            data = std::move(other.data);
            comp = std::move(other.comp);
        }
        return *this;
    }

    bool isEmpty() const noexcept
    {
        return data.empty();
    }

    std::size_t getSize() const noexcept
    {
        return data.size();
    }

    void clear() noexcept
    {
        data.clear();
    }

    void insert(const T& value)
    {
        data.push_back(value);
        trickleUp(data.size() - 1);
    }

    void insert(T&& value)
    {
        data.push_back(std::move(value));
        trickleUp(data.size() - 1);
    }

    bool peekMin(T& outValue) const
    {
        if (data.empty()) {
            return false;
        }
        outValue = data[0];
        return true;
    }

    bool peekMax(T& outValue) const
    {
        if (data.empty()) {
            return false;
        }
        outValue = data[getMaxIndex()];
        return true;
    }

    bool extractMin(T& outValue)
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
        trickleDown(0);
        return true;
    }

    bool extractMax(T& outValue)
    {
        if (data.empty()) {
            return false;
        }

        const std::size_t maxIdx = getMaxIndex();
        outValue = std::move(data[maxIdx]);

        if (maxIdx == data.size() - 1) {
            data.pop_back();
            return true;
        }

        data[maxIdx] = std::move(data.back());
        data.pop_back();
        trickleDown(maxIdx);
        return true;
    }

    /// Removes the element at the specified index, restoring heap invariants.
    /// Optionally outputs the removed element via outRemoved.
    /// Time complexity: O(log N).
    bool removeAt(std::size_t index, T* outRemoved = nullptr)
    {
        if (index >= data.size()) {
            return false;
        }

        if (outRemoved != nullptr) {
            *outRemoved = std::move(data[index]);
        }

        if (index == data.size() - 1) {
            data.pop_back();
            return true;
        }

        data[index] = std::move(data.back());
        data.pop_back();

        if (index < data.size()) {
            // Replaced element could need to trickle down or trickle up
            if (index > 0) {
                std::size_t p = parent(index);
                if (isMinLevel(index)) {
                    if (comp(data[p], data[index])) {
                        std::swap(data[index], data[p]);
                        trickleUpMax(p);
                        trickleDownMin(index);
                    } else {
                        trickleUpMin(index);
                        trickleDownMin(index);
                    }
                } else {
                    if (comp(data[index], data[p])) {
                        std::swap(data[index], data[p]);
                        trickleUpMin(p);
                        trickleDownMax(index);
                    } else {
                        trickleUpMax(index);
                        trickleDownMax(index);
                    }
                }
            } else {
                trickleDown(0);
            }
        }

        return true;
    }

    /// Removes the first element satisfying the predicate.
    /// Returns true if an element was found and removed, false otherwise.
    template <typename Predicate>
    bool removeIf(Predicate pred)
    {
        for (std::size_t i = 0; i < data.size(); ++i) {
            if (pred(data[i])) {
                return removeAt(i);
            }
        }
        return false;
    }

    /// Diagnostic verification of Min-Max Heap invariants.
    /// Returns true if all nodes satisfy min-level and max-level rules.
    bool verifyHeapInvariants() const noexcept(std::is_nothrow_invocable_v<const Compare&, const T&, const T&>)
    {
        for (std::size_t i = 0; i < data.size(); ++i) {
            const bool isMin = isMinLevel(i);

            // Check children
            const std::size_t left = leftChild(i);
            const std::size_t right = rightChild(i);

            if (left < data.size()) {
                if (isMin && comp(data[left], data[i])) {
                    return false;
                }
                if (!isMin && comp(data[i], data[left])) {
                    return false;
                }
            }
            if (right < data.size()) {
                if (isMin && comp(data[right], data[i])) {
                    return false;
                }
                if (!isMin && comp(data[i], data[right])) {
                    return false;
                }
            }

            // Check grandchildren
            const std::size_t gcStart = 4 * i + 3;
            for (std::size_t k = 0; k < 4; ++k) {
                const std::size_t gc = gcStart + k;
                if (gc < data.size()) {
                    if (isMin && comp(data[gc], data[i])) {
                        return false;
                    }
                    if (!isMin && comp(data[i], data[gc])) {
                        return false;
                    }
                } else {
                    break;
                }
            }
        }
        return true;
    }
};
