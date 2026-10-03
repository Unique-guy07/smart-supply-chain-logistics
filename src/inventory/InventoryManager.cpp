#include "inventory/InventoryManager.h"
#include <iostream>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

InventoryManager::InventoryManager(int hashTableCapacity)
    : hashTable(hashTableCapacity)
{
}

// ---------------------------------------------------------------------------
// Add Product
// ---------------------------------------------------------------------------

bool InventoryManager::addProduct(const Product& product)
{
    return hashTable.insert(product);
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

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

void InventoryManager::displayInventory() const
{
    std::cout << "=== Inventory ===\n";
    hashTable.display();
    std::cout << "=================\n";
}
