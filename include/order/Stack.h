#pragma once

#include <cstddef>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

/// Custom templated linked-node LIFO Stack.
///
/// Implements strict LIFO (Last-In, First-Out) semantics using exclusively owned,
/// singly-linked heap nodes.
///
/// Exception safety guarantees:
/// - push(const T&), push(T&&):
///     Strong exception guarantee. A new node is allocated dynamically. If allocation
///     or element construction throws, the stack remains completely unmodified.
/// - pop(T& outValue):
///     If outValue assignment throws, the stack's internal state remains untouched, but
///     outValue may be left in an intermediate state. Conditionally noexcept if T's move
///     assignment is noexcept.
/// - pop():
///     Unconditionally noexcept. Unlinks and deletes the top node without moving data.
/// - peek(T& outValue):
///     Strong exception guarantee. Does not mutate the stack.
/// - top():
///     Returns a reference to the top element. Throws std::underflow_error if empty.
/// - clear():
///     Strictly noexcept. Deallocates all nodes.
/// - Move constructor / Move assignment:
///     Strictly noexcept pointer and count transfers.
/// - Copy constructor / Copy assignment:
///     Explicitly deleted to avoid accidental deep copies.
template <typename T>
class Stack
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

    Node* topNode{nullptr};
    std::size_t count{0};

public:
    Stack() noexcept = default;

    ~Stack() noexcept
    {
        clear();
    }

    Stack(const Stack&) = delete;
    Stack& operator=(const Stack&) = delete;

    Stack(Stack&& other) noexcept
        : topNode(other.topNode)
        , count(other.count)
    {
        other.topNode = nullptr;
        other.count = 0;
    }

    Stack& operator=(Stack&& other) noexcept
    {
        if (this != &other) {
            clear();
            topNode = other.topNode;
            count = other.count;
            other.topNode = nullptr;
            other.count = 0;
        }
        return *this;
    }

    /// Pushes an element by const reference.
    /// Strong exception guarantee: stack is unaltered if allocation/construction throws.
    void push(const T& value)
    {
        Node* newNode = new Node(value);
        newNode->next = topNode;
        topNode = newNode;
        ++count;
    }

    /// Pushes an element by rvalue reference.
    /// Strong exception guarantee: stack is unaltered if allocation/construction throws.
    void push(T&& value)
    {
        Node* newNode = new Node(std::move(value));
        newNode->next = topNode;
        topNode = newNode;
        ++count;
    }

    /// Pops the top element into outValue.
    /// If outValue move assignment throws, the stack remains intact.
    /// @return true if popped successfully, false if stack was empty.
    bool pop(T& outValue) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        if (isEmpty()) {
            return false;
        }

        outValue = std::move(topNode->data);
        Node* oldTop = topNode;
        topNode = topNode->next;
        delete oldTop;
        --count;
        return true;
    }

    /// Pops and discards the top element.
    /// @return true if popped successfully, false if stack was empty.
    bool pop() noexcept
    {
        if (isEmpty()) {
            return false;
        }

        Node* oldTop = topNode;
        topNode = topNode->next;
        delete oldTop;
        --count;
        return true;
    }

    /// Inspects the top element without popping.
    /// @return true if an element was copied to outValue, false if stack was empty.
    bool peek(T& outValue) const
    {
        if (isEmpty()) {
            return false;
        }
        outValue = topNode->data;
        return true;
    }

    /// Direct const reference access to the top element.
    /// @throws std::underflow_error if empty.
    const T& top() const
    {
        if (isEmpty()) {
            throw std::underflow_error("Stack is empty");
        }
        return topNode->data;
    }

    /// Direct mutable reference access to the top element.
    /// @throws std::underflow_error if empty.
    T& top()
    {
        if (isEmpty()) {
            throw std::underflow_error("Stack is empty");
        }
        return topNode->data;
    }

    [[nodiscard]] bool isEmpty() const noexcept
    {
        return topNode == nullptr;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return isEmpty();
    }

    [[nodiscard]] std::size_t getSize() const noexcept
    {
        return count;
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return getSize();
    }

    void clear() noexcept
    {
        while (topNode != nullptr) {
            Node* next = topNode->next;
            delete topNode;
            topNode = next;
        }
        count = 0;
    }
};
