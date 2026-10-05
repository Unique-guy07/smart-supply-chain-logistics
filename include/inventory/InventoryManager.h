#pragma once

#include "inventory/BST.h"
#include "inventory/HashTable.h"
#include "inventory/LinearProbingHashTable.h"
#include "inventory/Product.h"
#include <string>
#include <vector>

/// InventoryManager — sole business-level gateway for inventory operations.
///
/// Orchestrates Tier 1 (Separate Chaining), Tier 2 (Linear Probing), and
/// Tier 3 (BST ordered index).
///
/// Architecture:
///   InventoryManager
///       ├── HashTable              ← canonical Product ownership and master lookup
///       ├── LinearProbingHashTable ← non-owning secondary embedded/device lookup index
///       └── BST                    ← ordered Product-ID index
class InventoryManager
{
private:
    HashTable hashTable;
    LinearProbingHashTable deviceIndex;
    BST bstIndex;

    friend class InventoryManagerTestAccessor;

public:
    /// Constructs an InventoryManager with the given master and device capacities.
    /// @param hashTableCapacity Capacity for canonical Separate-Chaining HashTable.
    /// @param initialDeviceCapacity Initial capacity for embedded LinearProbingHashTable.
    /// @param maxDeviceCapacity Upper limit on device capacity (0 = unbounded growth).
    /// @throws std::invalid_argument if hashTableCapacity <= 0 or initialDeviceCapacity <= 0
    explicit InventoryManager(
        int hashTableCapacity,
        int initialDeviceCapacity = LinearProbingHashTable::DEFAULT_INITIAL_CAPACITY,
        int maxDeviceCapacity = 0);

    // No explicit destructor — RAII handles cleanup through owned member indexes.
    // Non-copyable, non-movable (inherited from HashTable member).

    /// Add a product to inventory across all three index tiers.
    /// @return true if added, false if a product with the same ID already exists.
    bool addProduct(const Product& product);

    /// Find a product by ID via canonical master index (HashTable).
    /// @return Pointer to stored canonical Product, or nullptr if not found.
    Product* findProduct(const std::string& productId);

    /// Find a product by ID via secondary embedded index (LinearProbingHashTable).
    /// @return Pointer to stored canonical Product, or nullptr if not found.
    Product* findProductOnDevice(const std::string& productId) const;

    /// Remove a product from inventory across all three index tiers.
    /// @return true if found and removed, false if not found.
    bool removeProduct(const std::string& productId);

    /// Update the quantity of a product in inventory.
    /// This is the business-level mutation gateway on the canonical product.
    /// @return true if found and updated, false if not found.
    bool updateProductQuantity(const std::string& productId, int newQuantity);

    int getProductCount() const;
    int getDeviceProductCount() const;
    int getDeviceCapacity() const;
    bool isEmpty() const;

    /// Return product IDs in the BST's lexicographic order.
    std::vector<std::string> getProductIdsInOrder() const;

    /// Display the full inventory across both index tiers.
    void displayInventory() const;
};
