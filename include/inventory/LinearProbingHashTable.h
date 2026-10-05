#pragma once

#include "inventory/Product.h"
#include <cstdint>
#include <string>

/// Slot occupancy state for open addressing with linear probing.
enum class SlotState : uint8_t
{
    Empty,      ///< Never occupied slot (terminates probe sequence)
    Occupied,   ///< Holds an active key and non-owning product reference
    Deleted     ///< Tombstone slot (continues search probe, reusable on insert)
};

/// Entry in the linear probing hash table.
/// Owns the productId key and slot state, but DOES NOT own the Product object.
struct Entry
{
    std::string productId;
    Product* product{nullptr};          ///< Non-owning pointer to canonical Product in master HashTable
    SlotState state{SlotState::Empty};
};

/// Linear-Probing Hash Table — embedded/device-side inventory lookup index.
///
/// Implements open addressing with linear probing.
/// This table does NOT own Product objects; canonical ownership resides in
/// HashTable (Tier 1: Separate Chaining).
///
/// Complexity:
///   Search:  Average O(1), Worst O(n)
///   Insert:  Average O(1), Worst O(n), Amortized O(1)
///   Remove:  Average O(1), Worst O(n)
///   Rehash:  O(n)
///   Space:   O(M) where M is table capacity
class LinearProbingHashTable
{
private:
    Entry* table;
    int capacity;
    int maxCapacity;    ///< Maximum capacity limit (0 = unlimited growth)
    int count;          ///< Number of currently Occupied slots
    int deletedCount;   ///< Number of Deleted tombstone slots
    double maxLoadFactor;

    /// Hash function (djb2) — maps a product ID to an initial bucket index.
    int hash(const std::string& key) const;

    /// Rehash all Occupied entries into a table of newCapacity.
    /// Purges all Deleted tombstones and preserves non-owning Product* pointers.
    void rehash(int newCapacity);

    friend class LinearProbingHashTableTestAccessor;

public:
    static constexpr int DEFAULT_INITIAL_CAPACITY = 16;
    static constexpr double DEFAULT_MAX_LOAD_FACTOR = 0.70;
    static constexpr int MAX_ALLOWABLE_CAPACITY = 1 << 30;

    explicit LinearProbingHashTable(
        int initialCapacity = DEFAULT_INITIAL_CAPACITY,
        double maxLoadFactor = DEFAULT_MAX_LOAD_FACTOR,
        int maxCapacity = 0);

    ~LinearProbingHashTable();

    // Non-copyable, non-movable (owns dynamic array of Entry)
    LinearProbingHashTable(const LinearProbingHashTable&) = delete;
    LinearProbingHashTable& operator=(const LinearProbingHashTable&) = delete;
    LinearProbingHashTable(LinearProbingHashTable&&) = delete;
    LinearProbingHashTable& operator=(LinearProbingHashTable&&) = delete;

    /// Insert a product reference.
    /// Key is derived from product->getProductId().
    /// @return true if inserted, false if product is nullptr or productId already exists.
    bool insert(Product* product);

    /// Search for a product by ID.
    /// @return Pointer to stored canonical Product, or nullptr if not found.
    Product* search(const std::string& productId) const;

    /// Check if key exists.
    bool contains(const std::string& productId) const;

    /// Remove a product reference by marking its slot Deleted (tombstone).
    /// @return true if found and removed, false if not found.
    bool remove(const std::string& productId);

    int getCount() const;
    int getCapacity() const;
    int getDeletedCount() const;
    double getLoadFactor() const;
    bool isEmpty() const;

    /// Display all slots and their states.
    void display() const;

    /// Clear all entries and reset counts.
    void clear();
};
