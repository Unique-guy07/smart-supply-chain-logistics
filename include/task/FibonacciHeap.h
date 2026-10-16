#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

// Forward declarations
template <typename T, typename Compare>
class FibonacciHeap;

template <typename T>
struct FibNode;

/// Disjoint-set context used to track heap ownership and validate NodeHandles across merges.
struct HeapContext
{
    std::shared_ptr<HeapContext> parentContext{nullptr};

    [[nodiscard]] HeapContext* find() noexcept
    {
        HeapContext* curr = this;
        while (curr->parentContext) {
            curr = curr->parentContext.get();
        }
        return curr;
    }

    [[nodiscard]] const HeapContext* find() const noexcept
    {
        const HeapContext* curr = this;
        while (curr->parentContext) {
            curr = curr->parentContext.get();
        }
        return curr;
    }
};

/// Shared payload mediating handle validity and direct node access.
template <typename T>
struct NodePayload
{
    FibNode<T>* node{nullptr};
    std::shared_ptr<HeapContext> context{nullptr};

    NodePayload(FibNode<T>* n, std::shared_ptr<HeapContext> ctx) noexcept
        : node(n)
        , context(std::move(ctx))
    {
    }
};

/// Internal node of a Fibonacci Heap.
template <typename T>
struct FibNode
{
    T value;
    std::size_t degree{0};
    bool marked{false};
    FibNode* parent{nullptr};
    FibNode* child{nullptr};
    FibNode* left{this};
    FibNode* right{this};
    std::shared_ptr<NodePayload<T>> payload{nullptr};

    explicit FibNode(const T& val, std::shared_ptr<NodePayload<T>> p = nullptr)
        : value(val)
        , payload(std::move(p))
    {
    }

    explicit FibNode(T&& val, std::shared_ptr<NodePayload<T>> p = nullptr)
        : value(std::move(val))
        , payload(std::move(p))
    {
    }
};

/// Custom templated Fibonacci Heap.
///
/// Theoretical foundation:
/// Fredman & Tarjan (1987), Cormen et al. (CLRS Chapter 19).
/// A Fibonacci Heap is a forest of heap-ordered trees represented using circular doubly linked lists.
/// Marked nodes track whether a node has lost a child since being linked to its current parent,
/// enabling cascading cuts that guarantee an O(log N) maximum degree bound and O(1) amortized decrease-key.
///
/// Asymptotic complexity:
///   - insert: O(1) worst case
///   - peek: O(1) worst case
///   - extract: O(log N) amortized
///   - decreaseKey: O(1) amortized
///   - merge: O(1) worst case, zero dynamic allocations
///   - deleteNode: O(log N) amortized
///   - clear / verifyHeapInvariants: O(N) worst case
///   - size / isEmpty: O(1) worst case
///
/// Comparator convention:
///   Matches BinomialHeap and MaxHeap throughout this project:
///   comp(a, b) == true indicates b has strictly higher priority than a.
///   For Compare = std::less<T>, higher values have higher priority (Max-Heap).
///   For Compare = std::greater<T>, smaller values have higher priority (Min-Heap, e.g. Dijkstra distances).
///
/// Memory safety:
///   - Non-recursive iterative clear ensures O(1) stack memory.
///   - All node handles are type-safe and validated against stale, invalid, and foreign access in O(1).
///   - Move semantics transfer node ownership and context safely.
///   - Copy semantics are explicitly deleted.
template <typename T, typename Compare = std::less<T>>
class FibonacciHeap
{
    static_assert(std::is_nothrow_invocable_r_v<bool, const Compare&, const T&, const T&>,
                  "FibonacciHeap requires Compare to be non-throwing invocable (noexcept): comp(a, b) must not throw");

public:
    /// Type-safe handle to an active node in a FibonacciHeap.
    class NodeHandle
    {
    public:
        NodeHandle() noexcept = default;

        [[nodiscard]] bool isValid() const noexcept
        {
            return payload != nullptr && payload->node != nullptr;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return isValid();
        }

        [[nodiscard]] const T& value() const
        {
            if (!isValid()) {
                throw std::logic_error("Attempted to access value of invalid or stale NodeHandle");
            }
            return payload->node->value;
        }

        [[nodiscard]] const T& operator*() const
        {
            return value();
        }

        [[nodiscard]] const T* operator->() const
        {
            if (!isValid()) {
                throw std::logic_error("Attempted to access member of invalid or stale NodeHandle");
            }
            return &payload->node->value;
        }

        bool operator==(const NodeHandle& other) const noexcept
        {
            return payload == other.payload;
        }

        bool operator!=(const NodeHandle& other) const noexcept
        {
            return payload != other.payload;
        }

    private:
        template <typename U, typename C>
        friend class FibonacciHeap;

        explicit NodeHandle(std::shared_ptr<NodePayload<T>> p) noexcept
            : payload(std::move(p))
        {
        }

        std::shared_ptr<NodePayload<T>> payload{nullptr};
    };

    FibonacciHeap()
        : context(std::make_shared<HeapContext>())
    {
    }

    explicit FibonacciHeap(const Compare& comparator)
        : comp(comparator)
        , context(std::make_shared<HeapContext>())
    {
    }

    ~FibonacciHeap() noexcept
    {
        clear();
    }

    // Move-only container semantics
    FibonacciHeap(const FibonacciHeap&) = delete;
    FibonacciHeap& operator=(const FibonacciHeap&) = delete;

    FibonacciHeap(FibonacciHeap&& other) noexcept(std::is_nothrow_move_constructible_v<Compare>)
        : comp(std::move(other.comp))
        , bestNode(other.bestNode)
        , nodeCount(other.nodeCount)
        , context(std::move(other.context))
    {
        other.bestNode = nullptr;
        other.nodeCount = 0;
    }

    FibonacciHeap& operator=(FibonacciHeap&& other) noexcept(std::is_nothrow_move_assignable_v<Compare>)
    {
        if (this != &other) {
            clear();
            comp = std::move(other.comp);
            bestNode = other.bestNode;
            nodeCount = other.nodeCount;
            context = std::move(other.context);
            other.bestNode = nullptr;
            other.nodeCount = 0;
        }
        return *this;
    }

    /// Inserts a value in O(1) worst-case time, returning a type-safe NodeHandle.
    NodeHandle insert(const T& value)
    {
        ensureContext();
        auto payload = std::make_shared<NodePayload<T>>(nullptr, context);
        auto* node = new FibNode<T>(value, payload);
        payload->node = node;
        insertNode(node);
        return NodeHandle(std::move(payload));
    }

    /// Inserts a value via move in O(1) worst-case time, returning a type-safe NodeHandle.
    NodeHandle insert(T&& value)
    {
        ensureContext();
        auto payload = std::make_shared<NodePayload<T>>(nullptr, context);
        auto* node = new FibNode<T>(std::move(value), payload);
        payload->node = node;
        insertNode(node);
        return NodeHandle(std::move(payload));
    }

    /// Inspects the highest-priority element in O(1) worst-case time.
    /// Returns false if the heap is empty, true otherwise.
    bool peek(T& outValue) const
    {
        if (bestNode == nullptr) {
            return false;
        }
        outValue = bestNode->value;
        return true;
    }

    /// Returns a const reference to the highest-priority element in O(1) worst-case time.
    /// Throws std::logic_error if the heap is empty.
    [[nodiscard]] const T& peek() const
    {
        if (bestNode == nullptr) {
            throw std::logic_error("Attempted to peek an empty FibonacciHeap");
        }
        return bestNode->value;
    }

    /// Extracts the highest-priority element in O(log N) amortized time.
    /// Returns false if the heap is empty, true otherwise.
    /// Provides Basic Exception Guarantee if outValue assignment throws.
    bool extract(T& outValue)
    {
        FibNode<T>* z = detachBestNode();
        if (z == nullptr) {
            return false;
        }

        // Basic Exception Guarantee: guard owns z and deletes it even if outValue assignment throws
        std::unique_ptr<FibNode<T>> guard(z);
        outValue = std::move(guard->value);
        return true;
    }

    /// Updates the key associated with handle to newKey in O(1) amortized time.
    /// Validates handle and verifies that newKey does not degrade priority.
    /// If handle is invalid, stale, foreign, or if newKey degrades priority, returns false.
    bool decreaseKey(const NodeHandle& handle, const T& newKey)
    {
        if (!isHandleValid(handle)) {
            return false;
        }

        FibNode<T>* x = handle.payload->node;
        // In our comparator convention, comp(a, b) == true means b has strictly higher priority than a.
        // If comp(newKey, x->value) is true, then x->value has strictly higher priority than newKey,
        // which represents an invalid priority downgrade.
        if (comp(newKey, x->value)) {
            return false;
        }

        x->value = newKey;
        FibNode<T>* y = x->parent;
        if (y != nullptr && comp(y->value, x->value)) {
            cut(x, y);
            cascadingCut(y);
        }

        if (comp(bestNode->value, x->value)) {
            bestNode = x;
        }
        return true;
    }

    /// Updates the key associated with handle via move in O(1) amortized time.
    bool decreaseKey(const NodeHandle& handle, T&& newKey)
    {
        if (!isHandleValid(handle)) {
            return false;
        }

        FibNode<T>* x = handle.payload->node;
        if (comp(newKey, x->value)) {
            return false;
        }

        x->value = std::move(newKey);
        FibNode<T>* y = x->parent;
        if (y != nullptr && comp(y->value, x->value)) {
            cut(x, y);
            cascadingCut(y);
        }

        if (comp(bestNode->value, x->value)) {
            bestNode = x;
        }
        return true;
    }

    /// Deletes an arbitrary node referenced by handle in O(log N) amortized time.
    /// Returns false if handle is invalid, stale, or foreign.
    bool deleteNode(const NodeHandle& handle)
    {
        if (!isHandleValid(handle)) {
            return false;
        }

        FibNode<T>* x = handle.payload->node;
        if (x == bestNode) {
            FibNode<T>* z = detachBestNode();
            delete z;
            return true;
        }

        FibNode<T>* p = x->parent;
        if (p != nullptr) {
            cut(x, p);
            cascadingCut(p);
        }

        // Now x is a root. Promote its children to the root list.
        if (x->child != nullptr) {
            FibNode<T>* c = x->child;
            FibNode<T>* curr = c;
            do {
                curr->parent = nullptr;
                curr = curr->right;
            } while (curr != c);

            spliceLists(bestNode, c);
            x->child = nullptr;
        }

        isolateNode(x);
        --nodeCount;

        if (x->payload) {
            x->payload->node = nullptr;
        }
        delete x;
        return true;
    }

    /// Destructive union: merges other into *this in O(1) worst-case time with zero dynamic allocations.
    /// Leaves other empty. All valid handles from other remain valid in *this.
    void merge(FibonacciHeap&& other) noexcept
    {
        if (this == &other || other.bestNode == nullptr) {
            return;
        }

        if (bestNode == nullptr) {
            bestNode = other.bestNode;
            nodeCount = other.nodeCount;
            if (!context) {
                context = std::move(other.context);
            } else {
                other.context->parentContext = context;
            }
            other.bestNode = nullptr;
            other.nodeCount = 0;
            other.context = nullptr;
            return;
        }

        // Both heaps are non-empty: splice root lists in O(1)
        spliceLists(bestNode, other.bestNode);
        if (comp(bestNode->value, other.bestNode->value)) {
            bestNode = other.bestNode;
        }

        nodeCount += other.nodeCount;
        other.context->parentContext = context;

        other.bestNode = nullptr;
        other.nodeCount = 0;
        other.context = nullptr;
    }

    /// Empties the heap in O(N), deleting all nodes with zero stack recursion and O(1) auxiliary memory.
    /// All active handles associated with this heap become stale.
    void clear() noexcept
    {
        if (bestNode == nullptr) {
            nodeCount = 0;
            return;
        }

        // Break circular root list into a linear chain via `right`
        bestNode->left->right = nullptr;
        FibNode<T>* curr = bestNode;
        bestNode = nullptr;
        nodeCount = 0;

        // Iterative flattening: detach children and append to curr->right
        while (curr != nullptr) {
            if (curr->child != nullptr) {
                FibNode<T>* c = curr->child;
                if (c->right == c) {
                    curr->child = nullptr;
                } else {
                    curr->child = c->right;
                    c->left->right = c->right;
                    c->right->left = c->left;
                }
                c->left = nullptr;
                c->right = curr->right;
                curr->right = c;
            } else {
                FibNode<T>* next = curr->right;
                if (curr->payload) {
                    curr->payload->node = nullptr;
                }
                delete curr;
                curr = next;
            }
        }
    }

    [[nodiscard]] std::size_t getSize() const noexcept
    {
        return nodeCount;
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return nodeCount;
    }

    [[nodiscard]] bool isEmpty() const noexcept
    {
        return nodeCount == 0;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return nodeCount == 0;
    }

    /// Validates whether a handle points to an active node belonging to this heap.
    [[nodiscard]] bool isHandleValid(const NodeHandle& handle) const noexcept
    {
        if (!handle.payload || handle.payload->node == nullptr) {
            return false;
        }
        if (!context || !handle.payload->context) {
            return false;
        }
        return handle.payload->context->find() == context->find();
    }

    /// Diagnostic verification of Fibonacci Heap structural, degree, and ordering invariants.
    [[nodiscard]] bool verifyHeapInvariants() const noexcept
    {
        if (bestNode == nullptr) {
            return nodeCount == 0;
        }

        // Verify root list circular pointers and heap priority
        std::size_t countedRoots = 0;
        const FibNode<T>* currRoot = bestNode;
        do {
            if (currRoot == nullptr) {
                return false;
            }
            if (currRoot->parent != nullptr) {
                return false;
            }
            if (currRoot->right == nullptr || currRoot->left == nullptr) {
                return false;
            }
            if (currRoot->right->left != currRoot || currRoot->left->right != currRoot) {
                return false;
            }
            // Root priority must not be strictly greater than bestNode
            if (comp(bestNode->value, currRoot->value)) {
                return false;
            }

            ++countedRoots;
            currRoot = currRoot->right;
            if (countedRoots > nodeCount) {
                return false;
            }
        } while (currRoot != bestNode);

        // Verify subtrees
        std::size_t totalCountedNodes = 0;
        currRoot = bestNode;
        for (std::size_t i = 0; i < countedRoots; ++i) {
            std::size_t treeCount = 0;
            if (!verifySubtree(currRoot, nullptr, treeCount)) {
                return false;
            }
            totalCountedNodes += treeCount;
            currRoot = currRoot->right;
        }

        return totalCountedNodes == nodeCount;
    }

private:
    FibNode<T>* detachBestNode() noexcept
    {
        if (bestNode == nullptr) {
            return nullptr;
        }

        FibNode<T>* z = bestNode;

        // Step 1: Promote all children of z to the root list
        if (z->child != nullptr) {
            FibNode<T>* c = z->child;
            FibNode<T>* curr = c;
            do {
                curr->parent = nullptr;
                curr = curr->right;
            } while (curr != c);

            spliceLists(bestNode, c);
            z->child = nullptr;
        }

        // Step 2: Remove z from the root list
        if (z->right == z) {
            bestNode = nullptr;
        } else {
            bestNode = z->right;
            isolateNode(z);
        }

        // Step 3: Consolidate trees of identical degree in the root list
        if (bestNode != nullptr) {
            consolidate();
        }

        --nodeCount;

        // Invalidate handle so any reference to this node becomes stale
        if (z->payload) {
            z->payload->node = nullptr;
        }

        return z;
    }

    void ensureContext()
    {
        if (!context) {
            context = std::make_shared<HeapContext>();
        }
    }

    void insertNode(FibNode<T>* node) noexcept
    {
        node->degree = 0;
        node->marked = false;
        node->parent = nullptr;
        node->child = nullptr;
        node->left = node;
        node->right = node;

        if (bestNode == nullptr) {
            bestNode = node;
        } else {
            spliceLists(bestNode, node);
            if (comp(bestNode->value, node->value)) {
                bestNode = node;
            }
        }
        ++nodeCount;
    }

    static void spliceLists(FibNode<T>* a, FibNode<T>* b) noexcept
    {
        if (a == nullptr || b == nullptr) {
            return;
        }
        FibNode<T>* a_left = a->left;
        FibNode<T>* b_left = b->left;

        a->left = b_left;
        b_left->right = a;
        b->left = a_left;
        a_left->right = b;
    }

    static void isolateNode(FibNode<T>* node) noexcept
    {
        node->left->right = node->right;
        node->right->left = node->left;
        node->left = node;
        node->right = node;
    }

    void linkNodes(FibNode<T>* y, FibNode<T>* x) noexcept
    {
        y->parent = x;
        y->marked = false;
        if (x->child == nullptr) {
            x->child = y;
            y->left = y;
            y->right = y;
        } else {
            spliceLists(x->child, y);
        }
        ++x->degree;
    }

    void cut(FibNode<T>* x, FibNode<T>* y) noexcept
    {
        if (x->right == x) {
            y->child = nullptr;
        } else {
            if (y->child == x) {
                y->child = x->right;
            }
            isolateNode(x);
        }
        --y->degree;

        spliceLists(bestNode, x);
        x->parent = nullptr;
        x->marked = false;
    }

    void cascadingCut(FibNode<T>* y) noexcept
    {
        FibNode<T>* z = y->parent;
        while (z != nullptr) {
            if (!y->marked) {
                y->marked = true;
                break;
            }
            cut(y, z);
            if (comp(bestNode->value, y->value)) {
                bestNode = y;
            }
            y = z;
            z = y->parent;
        }
    }

    void consolidate() noexcept
    {
        // Maximum degree bound D(N) <= floor(log_phi(N)) + 2.
        // For N <= 2^64, maximum degree is <= 92. A fixed 128-pointer stack array guarantees
        // zero dynamic memory allocations during consolidation.
        constexpr std::size_t MAX_DEGREE = 128;
        FibNode<T>* A[MAX_DEGREE] = {nullptr};

        // Break the circular root list into a linear singly-linked chain
        FibNode<T>* rootListHead = bestNode;
        bestNode->left->right = nullptr;
        bestNode->left = nullptr;
        bestNode = nullptr;

        FibNode<T>* curr = rootListHead;
        while (curr != nullptr) {
            FibNode<T>* x = curr;
            curr = curr->right;

            x->left = x;
            x->right = x;
            x->parent = nullptr;

            std::size_t d = x->degree;
            while (d < MAX_DEGREE && A[d] != nullptr) {
                FibNode<T>* y = A[d];
                if (comp(x->value, y->value)) {
                    FibNode<T>* tmp = x;
                    x = y;
                    y = tmp;
                }
                linkNodes(y, x);
                A[d] = nullptr;
                ++d;
            }
            if (d < MAX_DEGREE) {
                A[d] = x;
            }
        }

        // Reconstruct circular doubly linked root list from consolidated roots
        bestNode = nullptr;
        for (std::size_t i = 0; i < MAX_DEGREE; ++i) {
            if (A[i] != nullptr) {
                if (bestNode == nullptr) {
                    bestNode = A[i];
                    bestNode->left = bestNode;
                    bestNode->right = bestNode;
                } else {
                    spliceLists(bestNode, A[i]);
                    if (comp(bestNode->value, A[i]->value)) {
                        bestNode = A[i];
                    }
                }
            }
        }
    }

    bool verifySubtree(const FibNode<T>* node,
                       const FibNode<T>* expectedParent,
                       std::size_t& countedNodes) const noexcept
    {
        if (node == nullptr) {
            return false;
        }
        if (node->parent != expectedParent) {
            return false;
        }

        countedNodes = 1;

        if (node->child == nullptr) {
            return node->degree == 0;
        }

        std::size_t childCount = 0;
        const FibNode<T>* child = node->child;
        do {
            if (child == nullptr) {
                return false;
            }
            if (child->right == nullptr || child->left == nullptr) {
                return false;
            }
            if (child->right->left != child || child->left->right != child) {
                return false;
            }
            // Heap ordering: parent must have >= priority than child
            if (comp(node->value, child->value)) {
                return false;
            }

            std::size_t subCount = 0;
            if (!verifySubtree(child, node, subCount)) {
                return false;
            }
            countedNodes += subCount;
            ++childCount;
            child = child->right;
            if (childCount > nodeCount) {
                return false;
            }
        } while (child != node->child);

        return childCount == node->degree;
    }

    Compare comp{};
    FibNode<T>* bestNode{nullptr};
    std::size_t nodeCount{0};
    std::shared_ptr<HeapContext> context{nullptr};
};
