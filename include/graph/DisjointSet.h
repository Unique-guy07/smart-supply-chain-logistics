#pragma once

#include <cstddef>
#include <vector>

/// Custom Disjoint-Set / Union-Find data structure with union-by-rank and path compression.
///
/// Theoretical foundation:
/// Tarjan (1975), Galler & Fischer (1964), Cormen et al. (CLRS Chapter 21).
/// Disjoint-Set maintains a collection of disjoint dynamic sets over a universe {0, 1, ..., N - 1}.
/// With union-by-rank and two-pass path compression in find(), any sequence of m operations
/// on n elements runs in O(m * alpha(n)) time, where alpha is the inverse Ackermann function
/// (effectively O(1) in practical computing, alpha(10^80) <= 4).
///
/// Complexity guarantees:
///   - makeSet (constructor): O(N)
///   - find: O(alpha(N)) amortized
///   - unionSets: O(alpha(N)) amortized
///   - isConnected: O(alpha(N)) amortized
///   - getSetCount / size: O(1) worst-case
///
/// Exception safety:
///   - find, unionSets, isConnected: Strong exception guarantee; throws std::out_of_range
///     if an element index is >= size().
///   - Move constructor / assignment: noexcept.
///   - Copy operations: explicitly deleted to prevent accidental deep copies.
class DisjointSet
{
private:
    std::vector<std::size_t> parent;
    std::vector<std::size_t> rank;
    std::size_t setCount{0};

    void validateIndex(std::size_t i) const;

public:
    explicit DisjointSet(std::size_t numElements);
    ~DisjointSet() = default;

    // Move-only semantics (following repository convention)
    DisjointSet(const DisjointSet&) = delete;
    DisjointSet& operator=(const DisjointSet&) = delete;

    DisjointSet(DisjointSet&& other) noexcept = default;
    DisjointSet& operator=(DisjointSet&& other) noexcept = default;

    /// Finds the canonical representative of element i with two-pass path compression.
    /// @throws std::out_of_range if i >= size().
    [[nodiscard]] std::size_t find(std::size_t i);

    /// Unites the sets containing element i and element j using union-by-rank.
    /// @return true if the two elements were in different sets and merged; false if they were already in the same set.
    /// @throws std::out_of_range if i >= size() or j >= size().
    bool unionSets(std::size_t i, std::size_t j);

    /// Checks whether element i and element j belong to the same set.
    /// @throws std::out_of_range if i >= size() or j >= size().
    [[nodiscard]] bool isConnected(std::size_t i, std::size_t j);

    /// Returns the current number of disjoint sets.
    [[nodiscard]] std::size_t getSetCount() const noexcept;

    /// Returns the total number of elements managed by this DisjointSet.
    [[nodiscard]] std::size_t size() const noexcept;
};
