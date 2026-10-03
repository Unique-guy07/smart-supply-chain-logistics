#include "inventory/InventoryManager.h"
#include <iostream>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

InventoryManager::InventoryManager(int hashTableCapacity)
    : hashTable(hashTableCapacity)
    , bstIndex()
{
}

// ---------------------------------------------------------------------------
// Add Product
// ---------------------------------------------------------------------------

bool InventoryManager::addProduct(const Product& product)
{
    const std::string productId = product.getProductId();

    if (!hashTable.insert(product)) {
        return false;
    }

    try {
        if (bstIndex.insert(productId)) {
            return true;
        }
    } catch (...) {
        // HashTable owns the canonical Product record, so undo its insertion
        // before propagating an allocation or string-copy failure from BST.
        hashTable.remove(productId);
        throw;
    }

    // A duplicate BST key after a successful HashTable insertion indicates an
    // unexpected index mismatch. Roll back rather than leave indexes diverged.
    hashTable.remove(productId);
    return false;
}

// ---------------------------------------------------------------------------
// Find Product
// ---------------------------------------------------------------------------

Product* InventoryManager::findProduct(const std::string& productId)
{
    return hashTable.search(productId);
}

// ---------------------------------------------------------------------------
// Remove Product
// ---------------------------------------------------------------------------

bool InventoryManager::removeProduct(const std::string& productId)
{
    if (hashTable.search(productId) == nullptr) {
        return false;
    }

    // The indexes are changed only through InventoryManager, so a missing BST
    // key here would be an internal invariant violation. Leave HashTable
    // untouched rather than deleting the canonical Product without its index.
    if (!bstIndex.contains(productId)) {
        return false;
    }

    if (!bstIndex.remove(productId)) {
        return false;
    }

    // This must succeed after the successful HashTable search above in the
    // single-threaded inventory model; no allocation occurs during removal.
    return hashTable.remove(productId);
}

// ---------------------------------------------------------------------------
// Update Product Quantity
// ---------------------------------------------------------------------------

bool InventoryManager::updateProductQuantity(const std::string& productId,
                                             int newQuantity)
{
    return hashTable.updateQuantity(productId, newQuantity);
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

int InventoryManager::getProductCount() const
{
    return hashTable.getCount();
}

bool InventoryManager::isEmpty() const
{
    return hashTable.isEmpty();
}

std::vector<std::string> InventoryManager::getProductIdsInOrder() const
{
    return bstIndex.inorder();
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

void InventoryManager::displayInventory() const
{
    std::cout << "=== Inventory ===\n";
    hashTable.display();
    std::cout << "=================\n";
}
