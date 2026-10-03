#pragma once

#include <string>

/// Represents an inventory item in the supply chain system.
/// Product is a focused domain entity — mutations are controlled
/// through InventoryManager, not through unrestricted setters.
class Product
{
private:
    std::string productId;
    std::string name;
    std::string category;
    int quantity;
    double price;
    std::string warehouseId;
    std::string binLocation;

    /// Single encapsulated quantity mutation pathway.
    /// Accessible only to HashTable via the friend declaration below.
    void setQuantity(int newQuantity);
    friend class HashTable;

public:
    Product();

    Product(const std::string& id,
            const std::string& name,
            const std::string& category,
            int quantity,
            double price,
            const std::string& warehouseId,
            const std::string& binLocation);

    // --- Read access ---
    const std::string& getProductId() const;
    const std::string& getName() const;
    const std::string& getCategory() const;
    int getQuantity() const;
    double getPrice() const;
    const std::string& getWarehouseId() const;
    const std::string& getBinLocation() const;

    // --- Display ---
    void display() const;

    // --- Equality (concrete benefit: test assertions) ---
    bool operator==(const Product& other) const;
    bool operator!=(const Product& other) const;
};