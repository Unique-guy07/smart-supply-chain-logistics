#include "inventory/InventoryManager.h"
#include <iostream>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

InventoryManager::InventoryManager(
    int hashTableCapacity,
    int initialDeviceCapacity,
    int maxDeviceCapacity)
    : hashTable(hashTableCapacity)
    , deviceIndex(
          initialDeviceCapacity,
          LinearProbingHashTable::DEFAULT_MAX_LOAD_FACTOR,
          maxDeviceCapacity)
    , bstIndex()
{
}

// ---------------------------------------------------------------------------
// Add Product — 3-way transactional synchronization with rollback
// Note: Rollback operations (deviceIndex.remove and hashTable.remove) do not
// allocate dynamic memory and are non-throwing, providing strong exception safety.
// ---------------------------------------------------------------------------

bool InventoryManager::addProduct(const Product& product)
{
    const std::string productId = product.getProductId();

    // 1. Insert into canonical HashTable (Tier 1: Master)
    if (!hashTable.insert(product)) {
        return false;
    }

    // 2. Obtain canonical Product* from HashTable
    Product* canonicalProduct = hashTable.search(productId);
    if (canonicalProduct == nullptr) {
        hashTable.remove(productId);
        return false;
    }

    // 3. Insert non-owning pointer into deviceIndex (Tier 2: Embedded)
    try {
        if (!deviceIndex.insert(canonicalProduct)) {
            hashTable.remove(productId);
            return false;
        }
    } catch (...) {
        hashTable.remove(productId);
        throw;
    }

    // 4. Insert productId into bstIndex (Tier 3: Ordered)
    try {
        if (bstIndex.insert(productId)) {
            return true;
        }
    } catch (...) {
        deviceIndex.remove(productId);
        hashTable.remove(productId);
        throw;
    }

    // Index mismatch rollback
    deviceIndex.remove(productId);
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

Product* InventoryManager::findProductOnDevice(const std::string& productId) const
{
    return deviceIndex.search(productId);
}

// ---------------------------------------------------------------------------
// Remove Product — 3-way synchronization, unlinking device reference first
//
// Strong invariant design:
// 1. Pre-flight verification confirms presence across all three tiers before any mutation.
// 2. Unlinking operations (deviceIndex.remove, bstIndex.remove, hashTable.remove)
//    are purely pointer/slot state manipulations; they never allocate dynamic memory
//    and cannot throw exceptions.
// 3. The canonical Product remains alive in HashTable while secondary indexes are
//    unlinked, guaranteeing valid memory during dereferences.
// 4. deviceIndex (holding a non-owning raw pointer) is unlinked first so no dangling
//    reference ever exists.
// 5. Eliminates dangerous insert() restoration attempts that could allocate and throw.
// ---------------------------------------------------------------------------

bool InventoryManager::removeProduct(const std::string& productId)
{
    // Pre-flight check: verify the product exists in canonical HashTable
    Product* canonical = hashTable.search(productId);
    if (canonical == nullptr) {
        return false;
    }

    // Invariant check: all three tiers must contain the key
    if (!deviceIndex.contains(productId) || !bstIndex.contains(productId)) {
        return false;
    }

    // 1. Remove from deviceIndex first so no dangling pointer remains.
    //    LinearProbingHashTable::remove is non-allocating and non-throwing.
    if (!deviceIndex.remove(productId)) {
        return false;
    }

    // 2. Remove from BST.
    //    BST::remove transplants nodes and deletes without memory allocation; non-throwing.
    if (!bstIndex.remove(productId)) {
        return false;
    }

    // 3. Remove from canonical HashTable (destroys canonical Product).
    //    HashTable::remove unlinks and deletes node without dynamic allocation; non-throwing.
    if (!hashTable.remove(productId)) {
        return false;
    }

    return true;
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

int InventoryManager::getDeviceProductCount() const
{
    return deviceIndex.getCount();
}

int InventoryManager::getDeviceCapacity() const
{
    return deviceIndex.getCapacity();
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
    std::cout << "=== Inventory (Master Index: HashTable) ===\n";
    hashTable.display();
    std::cout << "=== Inventory (Embedded Index: LinearProbing) ===\n";
    deviceIndex.display();
    std::cout << "=================================================\n";
}
