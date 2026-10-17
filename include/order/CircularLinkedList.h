#pragma once

#include <cstddef>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

/// Custom singly-linked circular list template with exclusive node ownership.
///
/// Topologically, when the list is non-empty, the last node's next pointer
/// links back to the first node (tail->next == head). A single node points to itself.
///
/// Exception safety guarantees:
/// - pushFront, pushBack: Strong exception guarantee. If dynamic allocation or element
///   copy/move construction throws, internal state remains completely unmodified.
/// - popFront: Unconditionally noexcept if discarding or if T's move assignment is noexcept.
/// - front, back: Strong exception guarantee; throws std::underflow_error if empty.
/// - rotateForward: Strictly noexcept; shifts head and tail pointers cyclically.
/// - clear: Strictly noexcept; breaks the cycle before destroying all nodes in O(N).
/// - executeJosephus: Validates arguments upfront (throws std::invalid_argument if step == 0).
///   Pre-allocates result vector capacity to prevent mid-operation allocation failure.
///   Survivors of partial elimination remain in a valid, intact circular list.
/// - Move construction / Move assignment: Strictly noexcept pointer and count transfers.
/// - Copy construction / Copy assignment: Explicitly deleted to guarantee sole ownership.
template <typename T>
class CircularLinkedList
{
private:
    struct Node
    {
        T data;
        Node* next{nullptr};

        explicit Node(const T& val)
            : data(val)
            , next(nullptr)
        {
        }

        explicit Node(T&& val)
            : data(std::move(val))
            , next(nullptr)
        {
        }
    };

    Node* head{nullptr};
    Node* tail{nullptr};
    std::size_t count{0};

public:
    CircularLinkedList() noexcept = default;

    ~CircularLinkedList() noexcept
    {
        clear();
    }

    CircularLinkedList(const CircularLinkedList&) = delete;
    CircularLinkedList& operator=(const CircularLinkedList&) = delete;

    CircularLinkedList(CircularLinkedList&& other) noexcept
        : head(other.head)
        , tail(other.tail)
        , count(other.count)
    {
        other.head = nullptr;
        other.tail = nullptr;
        other.count = 0;
    }

    CircularLinkedList& operator=(CircularLinkedList&& other) noexcept
    {
        if (this != &other) {
            clear();
            head = other.head;
            tail = other.tail;
            count = other.count;
            other.head = nullptr;
            other.tail = nullptr;
            other.count = 0;
        }
        return *this;
    }

    /// Inserts an element at the front of the circular list.
    /// Strong exception guarantee: list is unaltered if allocation/construction throws.
    void pushFront(const T& value)
    {
        Node* newNode = new Node(value);
        if (isEmpty()) {
            head = newNode;
            tail = newNode;
            newNode->next = newNode;
        } else {
            newNode->next = head;
            head = newNode;
            tail->next = head;
        }
        ++count;
    }

    /// Inserts an element at the front by rvalue move.
    /// Strong exception guarantee: list is unaltered if allocation/construction throws.
    void pushFront(T&& value)
    {
        Node* newNode = new Node(std::move(value));
        if (isEmpty()) {
            head = newNode;
            tail = newNode;
            newNode->next = newNode;
        } else {
            newNode->next = head;
            head = newNode;
            tail->next = head;
        }
        ++count;
    }

    /// Appends an element to the back of the circular list.
    /// Strong exception guarantee: list is unaltered if allocation/construction throws.
    void pushBack(const T& value)
    {
        Node* newNode = new Node(value);
        if (isEmpty()) {
            head = newNode;
            tail = newNode;
            newNode->next = newNode;
        } else {
            tail->next = newNode;
            newNode->next = head;
            tail = newNode;
        }
        ++count;
    }

    /// Appends an element to the back by rvalue move.
    /// Strong exception guarantee: list is unaltered if allocation/construction throws.
    void pushBack(T&& value)
    {
        Node* newNode = new Node(std::move(value));
        if (isEmpty()) {
            head = newNode;
            tail = newNode;
            newNode->next = newNode;
        } else {
            tail->next = newNode;
            newNode->next = head;
            tail = newNode;
        }
        ++count;
    }

    /// Accesses the front element by const reference.
    /// @throws std::underflow_error if empty.
    [[nodiscard]] const T& front() const
    {
        if (isEmpty()) {
            throw std::underflow_error("CircularLinkedList is empty");
        }
        return head->data;
    }

    /// Accesses the front element by mutable reference.
    /// @throws std::underflow_error if empty.
    [[nodiscard]] T& front()
    {
        if (isEmpty()) {
            throw std::underflow_error("CircularLinkedList is empty");
        }
        return head->data;
    }

    /// Accesses the back element by const reference.
    /// @throws std::underflow_error if empty.
    [[nodiscard]] const T& back() const
    {
        if (isEmpty()) {
            throw std::underflow_error("CircularLinkedList is empty");
        }
        return tail->data;
    }

    /// Accesses the back element by mutable reference.
    /// @throws std::underflow_error if empty.
    [[nodiscard]] T& back()
    {
        if (isEmpty()) {
            throw std::underflow_error("CircularLinkedList is empty");
        }
        return tail->data;
    }

    /// Removes and discards the front element.
    /// Safely handles empty, single-node, and multi-node circular lists.
    /// @return true if an element was removed, false if the list was empty.
    bool popFront() noexcept
    {
        if (isEmpty()) {
            return false;
        }

        Node* oldHead = head;
        if (head == tail) {
            head = nullptr;
            tail = nullptr;
        } else {
            head = head->next;
            tail->next = head;
        }
        --count;
        delete oldHead;
        return true;
    }

    /// Removes the front element and moves its data into outValue.
    /// Safely handles empty, single-node, and multi-node circular lists.
    /// @return true if an element was removed, false if the list was empty.
    bool popFront(T& outValue) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        if (isEmpty()) {
            return false;
        }

        outValue = std::move(head->data);
        Node* oldHead = head;
        if (head == tail) {
            head = nullptr;
            tail = nullptr;
        } else {
            head = head->next;
            tail->next = head;
        }
        --count;
        delete oldHead;
        return true;
    }

    /// Rotates the list forward cyclically by the given number of steps.
    /// Rotating forward advances the head pointer to the subsequent element,
    /// so the previous head becomes the new tail (preserving tail->next == head).
    void rotateForward(std::size_t steps = 1) noexcept
    {
        if (count <= 1 || steps == 0) {
            return;
        }

        const std::size_t effectiveSteps = steps % count;
        if (effectiveSteps == 0) {
            return;
        }

        for (std::size_t i = 0; i < effectiveSteps; ++i) {
            tail = head;
            head = head->next;
        }
    }

    /// Removes the first occurrence of the specified value.
    /// @return true if an element was found and removed; false otherwise.
    bool remove(const T& value)
    {
        if (isEmpty()) {
            return false;
        }

        if (count == 1) {
            if (head->data == value) {
                Node* oldHead = head;
                head = nullptr;
                tail = nullptr;
                count = 0;
                delete oldHead;
                return true;
            }
            return false;
        }

        if (head->data == value) {
            return popFront();
        }

        Node* prev = head;
        Node* curr = head->next;

        while (curr != head) {
            if (curr->data == value) {
                prev->next = curr->next;
                if (curr == tail) {
                    tail = prev;
                }
                --count;
                delete curr;
                return true;
            }
            prev = curr;
            curr = curr->next;
        }

        return false;
    }

    /// Checks if the circular list contains the specified value.
    [[nodiscard]] bool contains(const T& value) const noexcept
    {
        if (isEmpty()) {
            return false;
        }

        Node* curr = head;
        for (std::size_t i = 0; i < count; ++i) {
            if (curr->data == value) {
                return true;
            }
            curr = curr->next;
        }
        return false;
    }

    /// Deallocates all nodes and resets the list to an empty state.
    /// Strictly O(N) termination without infinite cycles or dangling pointers.
    void clear() noexcept
    {
        if (isEmpty()) {
            return;
        }

        // Break cycle to ensure standard linear traversal during deletion
        tail->next = nullptr;
        Node* current = head;
        while (current != nullptr) {
            Node* nextNode = current->next;
            delete current;
            current = nextNode;
        }

        head = nullptr;
        tail = nullptr;
        count = 0;
    }

    /// Returns the number of elements currently stored.
    [[nodiscard]] std::size_t getSize() const noexcept
    {
        return count;
    }

    /// Checks whether the circular list is empty.
    [[nodiscard]] bool isEmpty() const noexcept
    {
        return count == 0;
    }

    /// Alias for isEmpty() conforming to STL container conventions.
    [[nodiscard]] bool empty() const noexcept
    {
        return isEmpty();
    }

    /// Exports all elements into a vector starting from head to tail.
    [[nodiscard]] std::vector<T> toVector() const
    {
        std::vector<T> result;
        if (isEmpty()) {
            return result;
        }

        result.reserve(count);
        Node* current = head;
        for (std::size_t i = 0; i < count; ++i) {
            result.push_back(current->data);
            current = current->next;
        }
        return result;
    }

    /// Executes Josephus elimination on the circular list.
    ///
    /// Counting Convention:
    /// - 1-based forward counting starting from current head.
    /// - stepInterval (k) must be >= 1; throws std::invalid_argument if 0.
    /// - At each step, advances (k - 1) % currentSize links, removes and returns the selected node.
    /// - For k == 1, sequentially removes the current starting node.
    /// - Next starting position advances to the node immediately following the removed node.
    /// - quota == 0 or quota >= count eliminates all nodes until the list is empty.
    /// - quota < count eliminates exactly quota elements; remaining survivors are preserved
    ///   in a valid circular list with head positioned at the next candidate.
    ///
    /// Exception Guarantee:
    /// - Vector capacity is pre-reserved before any nodes are unlinked.
    /// - If element move construction is noexcept, guarantees strong exception safety.
    ///
    /// @param stepInterval Number of steps per elimination count (k >= 1).
    /// @param quota Maximum number of elements to eliminate (0 = eliminate all).
    /// @return Elements in their exact order of elimination.
    std::vector<T> executeJosephus(std::size_t stepInterval, std::size_t quota = 0)
    {
        if (stepInterval == 0) {
            throw std::invalid_argument("Josephus step interval must be >= 1");
        }

        std::vector<T> eliminated;
        if (isEmpty()) {
            return eliminated;
        }

        const std::size_t effectiveQuota = (quota == 0 || quota > count) ? count : quota;
        eliminated.reserve(effectiveQuota);

        Node* curr = head;
        Node* prev = tail;

        for (std::size_t q = 0; q < effectiveQuota; ++q) {
            const std::size_t steps = (stepInterval - 1) % count;
            for (std::size_t s = 0; s < steps; ++s) {
                prev = curr;
                curr = curr->next;
            }

            // curr is now the node to eliminate, prev is its predecessor
            eliminated.push_back(std::move(curr->data));

            Node* toDelete = curr;
            if (count == 1) {
                head = nullptr;
                tail = nullptr;
                count = 0;
                delete toDelete;
                break;
            }

            Node* nextStart = curr->next;
            prev->next = nextStart;

            if (toDelete == head) {
                head = nextStart;
            }
            if (toDelete == tail) {
                tail = prev;
            }

            delete toDelete;
            --count;
            curr = nextStart;
        }

        // If partial elimination occurred and survivors remain, maintain circular invariant
        if (count > 0) {
            head = curr;
            tail = prev;
            tail->next = head;
        }

        return eliminated;
    }

    /// Verifies all structural invariants of the circular list.
    /// @return true if invariants are strictly maintained; false otherwise.
    [[nodiscard]] bool verifyInvariants() const noexcept
    {
        if (count == 0) {
            return head == nullptr && tail == nullptr;
        }

        if (count == 1) {
            return head != nullptr && tail == head && head->next == head;
        }

        if (head == nullptr || tail == nullptr || tail->next != head) {
            return false;
        }

        // Verify cycle length exactly matches count and reaches tail at count - 1
        Node* current = head;
        for (std::size_t i = 0; i < count; ++i) {
            if (current == nullptr) {
                return false;
            }
            if (i == count - 1) {
                if (current != tail) {
                    return false;
                }
            }
            current = current->next;
        }

        return current == head;
    }
};
