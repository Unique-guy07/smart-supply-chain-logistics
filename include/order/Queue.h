#pragma once

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

/// Custom templated linked-node FIFO Queue.
///
/// Implements strict FIFO (First-In, First-Out) scheduling.
///
/// Exception safety guarantees:
/// - enqueue: Allocates a new node dynamically. If allocation fails, throws
///   std::bad_alloc with the Strong Exception Guarantee (queue state is untouched).
///   Must NOT be noexcept.
/// - dequeue: Conditionally noexcept depending on whether T's move assignment is noexcept.
///   Unlinks head node and moves data.
/// - peek: Read-only check; strong exception safety.
/// - clear: Destroys all nodes; strictly noexcept.
/// - Move construction / move assignment: Pointer swaps; strictly noexcept.
/// - Destructor: Calls clear(); strictly noexcept.
/// - Copy construction / assignment: Deleted to prevent unintentional deep copies.
template <typename T>
class Queue
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
    Queue() noexcept = default;

    ~Queue() noexcept
    {
        clear();
    }

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    Queue(Queue&& other) noexcept
        : head(other.head)
        , tail(other.tail)
        , count(other.count)
    {
        other.head = nullptr;
        other.tail = nullptr;
        other.count = 0;
    }

    Queue& operator=(Queue&& other) noexcept
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

    void enqueue(const T& value)
    {
        Node* newNode = new Node(value);
        if (tail == nullptr) {
            head = newNode;
            tail = newNode;
        } else {
            tail->next = newNode;
            tail = newNode;
        }
        ++count;
    }

    void enqueue(T&& value)
    {
        Node* newNode = new Node(std::move(value));
        if (tail == nullptr) {
            head = newNode;
            tail = newNode;
        } else {
            tail->next = newNode;
            tail = newNode;
        }
        ++count;
    }

    bool dequeue(T& outValue) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        if (isEmpty()) {
            return false;
        }

        outValue = std::move(head->data);
        Node* oldHead = head;
        head = head->next;
        if (head == nullptr) {
            tail = nullptr;
        }
        --count;
        delete oldHead;
        return true;
    }

    bool peek(T& outValue) const
    {
        if (isEmpty()) {
            return false;
        }
        outValue = head->data;
        return true;
    }

    bool isEmpty() const noexcept
    {
        return count == 0;
    }

    std::size_t getSize() const noexcept
    {
        return count;
    }

    void clear() noexcept
    {
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
};
