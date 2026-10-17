#include "graph/DisjointSet.h"

#include <stdexcept>
#include <string>

DisjointSet::DisjointSet(std::size_t numElements)
    : parent(numElements)
    , rank(numElements, 0)
    , setCount(numElements)
{
    for (std::size_t i = 0; i < numElements; ++i) {
        parent[i] = i;
    }
}

void DisjointSet::validateIndex(std::size_t i) const
{
    if (i >= parent.size()) {
        throw std::out_of_range("DisjointSet: element index out of range: " + std::to_string(i)
                                + " (size is " + std::to_string(parent.size()) + ")");
    }
}

std::size_t DisjointSet::find(std::size_t i)
{
    validateIndex(i);

    // Pass 1: Find the canonical root of the set
    std::size_t root = i;
    while (root != parent[root]) {
        root = parent[root];
    }

    // Pass 2: Path compression — point every node along the path directly to the root
    std::size_t curr = i;
    while (curr != root) {
        std::size_t next = parent[curr];
        parent[curr] = root;
        curr = next;
    }

    return root;
}

bool DisjointSet::unionSets(std::size_t i, std::size_t j)
{
    validateIndex(i);
    validateIndex(j);

    std::size_t rootI = find(i);
    std::size_t rootJ = find(j);

    if (rootI == rootJ) {
        return false;
    }

    // Union by rank: attach tree with smaller rank under root of tree with larger rank
    if (rank[rootI] < rank[rootJ]) {
        parent[rootI] = rootJ;
    } else if (rank[rootI] > rank[rootJ]) {
        parent[rootJ] = rootI;
    } else {
        parent[rootJ] = rootI;
        ++rank[rootI];
    }

    --setCount;
    return true;
}

bool DisjointSet::isConnected(std::size_t i, std::size_t j)
{
    validateIndex(i);
    validateIndex(j);
    return find(i) == find(j);
}

std::size_t DisjointSet::getSetCount() const noexcept
{
    return setCount;
}

std::size_t DisjointSet::size() const noexcept
{
    return parent.size();
}
