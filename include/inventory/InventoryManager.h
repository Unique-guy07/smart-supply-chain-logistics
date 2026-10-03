#pragma once

#include "inventory/BST.h"
#include "inventory/HashTable.h"
#include "inventory/Product.h"
#include <string>
#include <vector>

/// InventoryManager — sole business-level gateway for inventory operations.
///
/// Owns a HashTable as the primary Product-ID index and a BST ordered index.
///
/// Architecture:
///   InventoryManager
///       ├── HashTable   ← canonical Product ownership and fast lookup
///       └── BST         ← ordered Product-ID index
class InventoryManager
{
private:
    HashTable hashTable;
    BST bstIndex;

public:
    /// Constructs an InventoryManager with the given hash table capacity.
    /// @throws std::invalid_argument if hashTableCapacity <= 0
    explicit InventoryManager(int hashTableCapacity);

    // No explicit destructor — RAII handles cleanup through both owned indexes.
    // Non-copyable, non-movable (inherited from HashTable member).

    /// Add a product to inventory.
    /// @return true if added, false if a product with the same ID already exists.
    bool addProduct(const Product& product);

    /// Find a product by ID.
    /// @return Pointer to the stored Product, or nullptr if not found.
    Product* findProduct(const std::string& productId);

    /// Remove a product from inventory.
    /// @return true if found and removed, false if not found.
    bool removeProduct(const std::string& productId);

    /// Update the quantity of a product in inventory.
    /// This is the business-level mutation gateway.
    /// @return true if found and updated, false if not found.
    bool updateProductQuantity(const std::string& productId, int newQuantity);

    int getProductCount() const;
    bool isEmpty() const;

    /// Return product IDs in the BST's lexicographic order.
    std::vector<std::string> getProductIdsInOrder() const;

    /// Display the full inventory.
    void displayInventory() const;
};
