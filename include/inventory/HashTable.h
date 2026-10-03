#pragma once

#include "inventory/Product.h"
#include <string>

/// Separate-Chaining Hash Table — central/master Product-ID index.
///
/// Maps std::string product IDs to Product objects using linked-list
/// chains for collision resolution. This is a custom DSA implementation,
/// not std::unordered_map.
///
/// Capacity is fixed at construction. No dynamic resizing — the research
/// architecture reserves load-factor-based resizing for Linear Probing
/// (embedded/memory-constrained tier). Separate Chaining is chosen for
/// its tolerance of higher load factors.
///
/// Complexity:
///   Insert:  average O(1), worst O(n)
///   Search:  average O(1), worst O(n)
///   Delete:  average O(1), worst O(n)
class HashTable
{
private:
    struct Node
    {
        Product product;
        Node* next;
        Node(const Product& p, Node* n = nullptr);
    };

    Node** table;      ///< Array of chain head pointers
    int capacity;      ///< Fixed bucket count (validated > 0)
    int count;         ///< Current number of stored products

    /// Hash function (djb2) — maps a product ID to a bucket index.
    int hash(const std::string& key) const;

public:
    /// Constructs a hash table with the given bucket count.
    /// @throws std::invalid_argument if capacity <= 0
    explicit HashTable(int capacity);

    /// Destructor — deallocates all chains and the bucket array.
    ~HashTable();

    // Non-copyable, non-movable (owns raw pointers via Node chains)
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;
    HashTable(HashTable&&) = delete;
    HashTable& operator=(HashTable&&) = delete;

    /// Insert a product. Keyed by product.getProductId().
    /// @return true if inserted, false if a product with the same ID already exists.
    bool insert(const Product& product);

    /// Search for a product by ID.
    /// @return Pointer to the stored Product, or nullptr if not found.
    Product* search(const std::string& productId);

    /// Remove a product by ID.
    /// @return true if found and removed, false if not found.
    bool remove(const std::string& productId);

    /// Update the quantity of a stored product.
    /// Uses the single encapsulated mutation pathway via Product::setQuantity.
    /// @return true if found and updated, false if not found.
    bool updateQuantity(const std::string& productId, int newQuantity);

    int getCount() const;
    int getCapacity() const;
    bool isEmpty() const;

    /// Display all buckets and their contents.
    void display() const;
};
