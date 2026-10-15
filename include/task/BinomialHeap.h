#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

/// Internal node representing a binomial tree node.
template <typename T>
struct BinomialNode
{
    T value;
    std::size_t degree{0};
    BinomialNode* parent{nullptr};
    BinomialNode* child{nullptr};   // Leftmost child (head of children linked list)
    BinomialNode* sibling{nullptr}; // Next sibling in root list or children list

    explicit BinomialNode(const T& val)
        : value(val)
    {
    }

    explicit BinomialNode(T&& val)
        : value(std::move(val))
    {
    }
};

/// Custom templated Binomial Heap.
///
/// Theoretical foundation:
/// Vuillemin (1978), Brown (1978), Cormen et al. (CLRS).
/// A Binomial Heap is a collection (forest) of binomial trees satisfying:
///   1. Binomial Tree Structure: Each tree B_k has 2^k nodes, height k, and root degree k.
///   2. Heap Invariant: For every node p and child c, !comp(p->value, c->value).
///      Under default Compare = std::less<T>, parent >= child (Max-Heap property),
///      matching PriorityTask::operator< where higher urgency ranks greater.
///   3. Root List Degree Ordering: Roots are maintained in a singly-linked list ordered
///      by strictly increasing degrees (degree(T_1) < degree(T_2) < ... < degree(T_m)).
///      No two trees in the root list share the same degree.
///
/// Complexity guarantees:
///   - insert: O(log N) worst case, O(1) amortized
///   - peek: O(log N) worst case (scanning at most floor(log2 N) + 1 roots)
///   - extract: O(log N) worst case
///   - merge: O(log(N + M)) worst case, zero dynamic memory allocations
///   - size / isEmpty: O(1)
///   - clear / verifyHeapInvariants: O(N)
///
/// Memory safety:
///   - BinomialHeap owns all allocated BinomialNode<T> instances.
///   - Destructor uses non-recursive pointer flattening to ensure O(1) stack memory.
///   - Copy construction and copy assignment are deleted to prevent accidental O(N) deep copies.
///   - Move construction and move assignment are noexcept pointer transfers.
template <typename T, typename Compare = std::less<T>>
class BinomialHeap
{
    static_assert(std::is_nothrow_invocable_r_v<bool, const Compare&, const T&, const T&>,
                  "BinomialHeap requires Compare to be non-throwing invocable (noexcept): comp(a, b) must not throw");

private:
    BinomialNode<T>* head{nullptr};
    std::size_t nodeCount{0};
    Compare comp;

    /// Attaches tree rooted at child as the leftmost child of parent.
    /// Both roots must have the same degree k. Parent degree becomes k + 1.
    static void linkTrees(BinomialNode<T>* child, BinomialNode<T>* parent) noexcept
    {
        child->parent = parent;
        child->sibling = parent->child;
        parent->child = child;
        parent->degree++;
    }

    /// Merges two sorted root lists (ordered by non-decreasing degrees) into one singly-linked list.
    /// Operates in O(log(N + M)) pointer rewires with zero dynamic allocations.
    static BinomialNode<T>* mergeRootLists(BinomialNode<T>* h1, BinomialNode<T>* h2) noexcept
    {
        BinomialNode<T>* dummyHead = nullptr;
        BinomialNode<T>** tail = &dummyHead;

        while (h1 != nullptr && h2 != nullptr) {
            if (h1->degree <= h2->degree) {
                *tail = h1;
                h1 = h1->sibling;
            } else {
                *tail = h2;
                h2 = h2->sibling;
            }
            tail = &((*tail)->sibling);
        }
        *tail = (h1 != nullptr) ? h1 : h2;
        return dummyHead;
    }

    /// Recursively counts nodes and verifies binomial tree invariants for a subtree.
    /// Recursion depth is strictly bounded by the height of the binomial tree (degree k <= log2 N).
    /// For N = 10^9, degree k <= 30, so stack depth is strictly <= 30 frames.
    bool verifySubtree(const BinomialNode<T>* node,
                       const BinomialNode<T>* expectedParent,
                       std::size_t expectedDegree,
                       std::size_t& countedNodes) const
    {
        if (node == nullptr) {
            return false;
        }
        if (node->parent != expectedParent) {
            return false;
        }
        if (node->degree != expectedDegree) {
            return false;
        }

        countedNodes = 1;

        // Verify children of node B_k: children must have degrees k-1, k-2, ..., 0
        const BinomialNode<T>* child = node->child;
        std::size_t childIndex = 0;
        while (child != nullptr) {
            if (childIndex >= expectedDegree) {
                return false; // Too many children
            }
            const std::size_t childExpectedDegree = expectedDegree - 1 - childIndex;

            // Heap order invariant: parent must not be less than child
            if (comp(node->value, child->value)) {
                return false;
            }

            std::size_t subtreeCount = 0;
            if (!verifySubtree(child, node, childExpectedDegree, subtreeCount)) {
                return false;
            }
            countedNodes += subtreeCount;
            child = child->sibling;
            ++childIndex;
        }

        if (childIndex != expectedDegree) {
            return false; // Too few children
        }

        // A binomial tree B_k must have exactly 2^k nodes
        const std::size_t expectedNodes = static_cast<std::size_t>(1) << expectedDegree;
        return countedNodes == expectedNodes;
    }

public:
    BinomialHeap() noexcept = default;

    explicit BinomialHeap(const Compare& comparator)
        : head(nullptr)
        , nodeCount(0)
        , comp(comparator)
    {
    }

    ~BinomialHeap() noexcept
    {
        clear();
    }

    // Move-only container semantics
    BinomialHeap(const BinomialHeap&) = delete;
    BinomialHeap& operator=(const BinomialHeap&) = delete;

    BinomialHeap(BinomialHeap&& other) noexcept(std::is_nothrow_move_constructible_v<Compare>)
        : head(other.head)
        , nodeCount(other.nodeCount)
        , comp(std::move(other.comp))
    {
        other.head = nullptr;
        other.nodeCount = 0;
    }

    BinomialHeap& operator=(BinomialHeap&& other) noexcept(std::is_nothrow_move_assignable_v<Compare>)
    {
        if (this != &other) {
            clear();
            head = other.head;
            nodeCount = other.nodeCount;
            comp = std::move(other.comp);
            other.head = nullptr;
            other.nodeCount = 0;
        }
        return *this;
    }

    /// Inserts a value in O(log N) worst case, O(1) amortized.
    void insert(const T& value)
    {
        BinomialHeap temp(comp);
        temp.head = new BinomialNode<T>(value);
        temp.nodeCount = 1;
        merge(std::move(temp));
    }

    /// Inserts a value via move in O(log N) worst case, O(1) amortized.
    void insert(T&& value)
    {
        BinomialHeap temp(comp);
        temp.head = new BinomialNode<T>(std::move(value));
        temp.nodeCount = 1;
        merge(std::move(temp));
    }

    /// Inspects the maximum element without modifying the heap in O(log N).
    /// Returns false if the heap is empty, true otherwise.
    bool peek(T& outValue) const
    {
        if (head == nullptr) {
            return false;
        }

        const BinomialNode<T>* best = head;
        const BinomialNode<T>* curr = head->sibling;

        while (curr != nullptr) {
            if (comp(best->value, curr->value)) {
                best = curr;
            }
            curr = curr->sibling;
        }

        outValue = best->value;
        return true;
    }

    /// Extracts and removes the maximum element in O(log N).
    /// Returns false if the heap is empty, true otherwise.
    bool extract(T& outValue)
    {
        if (head == nullptr) {
            return false;
        }

        // Step 1: Locate the root with the top priority in the root list
        BinomialNode<T>* bestPrev = nullptr;
        BinomialNode<T>* best = head;
        BinomialNode<T>* prev = head;
        BinomialNode<T>* curr = head->sibling;

        while (curr != nullptr) {
            if (comp(best->value, curr->value)) {
                best = curr;
                bestPrev = prev;
            }
            prev = curr;
            curr = curr->sibling;
        }

        // Step 2: Unlink best from the root list
        if (bestPrev == nullptr) {
            head = best->sibling;
        } else {
            bestPrev->sibling = best->sibling;
        }

        // Step 3: Reverse the singly-linked list of best's children.
        // The children of B_k have degrees k-1, k-2, ..., 0 in decreasing order.
        // Reversing them yields a valid root list ordered by strictly increasing degrees (0, ..., k-1).
        BinomialNode<T>* childHead = nullptr;
        BinomialNode<T>* childCurr = best->child;
        while (childCurr != nullptr) {
            BinomialNode<T>* next = childCurr->sibling;
            childCurr->sibling = childHead;
            childCurr->parent = nullptr; // Detached children become tree roots
            childHead = childCurr;
            childCurr = next;
        }

        // Total nodes in the children subtree of B_k is 2^k - 1
        const std::size_t childNodeCount = (static_cast<std::size_t>(1) << best->degree) - 1;

        // Construct temporary heap from the children root list
        BinomialHeap temp(comp);
        temp.head = childHead;
        temp.nodeCount = childNodeCount;

        // Decrement nodeCount for the extracted node (and temp's nodeCount is merged back)
        nodeCount -= (childNodeCount + 1);

        // Step 4: Merge children back into the remaining root list in O(log N)
        merge(std::move(temp));

        // Step 5: Transfer extracted value to outValue.
        // Node best is wrapped in a std::unique_ptr guard so it is reclaimed even if outValue assignment throws.
        // Exception Safety Contract:
        //   - Basic Exception Guarantee: If assignment throws, best is deleted (no memory leak),
        //     the remaining heap remains structurally valid with all children promoted, and the
        //     extracted element itself is discarded.
        std::unique_ptr<BinomialNode<T>> nodeGuard(best);
        outValue = std::move(nodeGuard->value);
        return true;
    }

    /// Destructive union: merges other into *this in O(log(N + M)) time.
    /// Guarantees zero dynamic memory allocations. Leaves other empty.
    /// No-Throw Guarantee (noexcept): Compare is statically enforced to be noexcept invocable.
    void merge(BinomialHeap&& other) noexcept
    {
        if (this == &other || other.head == nullptr) {
            return;
        }
        if (head == nullptr) {
            head = other.head;
            nodeCount = other.nodeCount;
            other.head = nullptr;
            other.nodeCount = 0;
            return;
        }

        // Phase 1: Merge sorted root lists by non-decreasing degree in O(log N)
        BinomialNode<T>* newHead = mergeRootLists(head, other.head);
        nodeCount += other.nodeCount;
        other.head = nullptr;
        other.nodeCount = 0;

        if (newHead == nullptr) {
            head = nullptr;
            return;
        }

        // Phase 2: Traverse root list and link trees of identical degree in O(log N)
        BinomialNode<T>* prev = nullptr;
        BinomialNode<T>* curr = newHead;
        BinomialNode<T>* next = curr->sibling;

        while (next != nullptr) {
            // Case 1 & 2: Degrees differ, or 3 consecutive trees share the same degree
            // (curr->degree == next->degree == next->sibling->degree) -> propagate carry
            if (curr->degree != next->degree ||
                (next->sibling != nullptr && next->sibling->degree == curr->degree))
            {
                prev = curr;
                curr = next;
            }
            // Case 3: curr has higher or equal priority than next -> next becomes child of curr
            else if (!comp(curr->value, next->value)) {
                curr->sibling = next->sibling;
                linkTrees(next, curr);
            }
            // Case 4: next has higher priority than curr -> curr becomes child of next
            else {
                if (prev == nullptr) {
                    newHead = next;
                } else {
                    prev->sibling = next;
                }
                linkTrees(curr, next);
                curr = next;
            }
            next = curr->sibling;
        }

        head = newHead;
    }

    /// Returns the total number of elements stored in the heap.
    [[nodiscard]] std::size_t getSize() const noexcept
    {
        return nodeCount;
    }

    /// Returns true if the heap contains zero elements.
    [[nodiscard]] bool isEmpty() const noexcept
    {
        return nodeCount == 0;
    }

    /// Empties the heap in O(N), deleting all nodes with zero stack recursion and O(1) auxiliary memory.
    void clear() noexcept
    {
        while (head != nullptr) {
            BinomialNode<T>* curr = head;
            head = head->sibling;
            curr->sibling = nullptr;

            // Non-recursive flattening: while curr has a child, move it to curr's sibling list
            while (curr != nullptr) {
                if (curr->child != nullptr) {
                    BinomialNode<T>* c = curr->child;
                    curr->child = c->sibling; // detach first child
                    c->sibling = curr->sibling;
                    curr->sibling = c;
                } else {
                    BinomialNode<T>* next = curr->sibling;
                    delete curr;
                    curr = next;
                }
            }
        }
        nodeCount = 0;
    }

    /// Diagnostic verification of Binomial Heap structural and heap invariants.
    /// Verifies strictly increasing root degrees, unique degrees, 2^k node counts,
    /// and parent-child heap ordering.
    [[nodiscard]] bool verifyHeapInvariants() const noexcept
    {
        if (head == nullptr) {
            return nodeCount == 0;
        }

        std::size_t totalCounted = 0;
        const BinomialNode<T>* curr = head;
        std::size_t prevDegree = 0;
        bool isFirst = true;

        while (curr != nullptr) {
            // Invariant: root nodes have no parent
            if (curr->parent != nullptr) {
                return false;
            }

            // Invariant: strictly increasing root degrees
            if (!isFirst && curr->degree <= prevDegree) {
                return false; // Degree not strictly increasing or duplicate degree
            }
            prevDegree = curr->degree;
            isFirst = false;

            std::size_t treeNodeCount = 0;
            if (!verifySubtree(curr, nullptr, curr->degree, treeNodeCount)) {
                return false;
            }
            totalCounted += treeNodeCount;
            curr = curr->sibling;
        }

        return totalCounted == nodeCount;
    }
};
