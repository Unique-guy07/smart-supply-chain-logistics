#include "inventory/Product.h"
#include <iostream>

Product::Product()
    : productId("")
    , name("")
    , category("")
    , quantity(0)
    , price(0.0)
    , warehouseId("")
    , binLocation("")
{
}

Product::Product(const std::string& id,
                 const std::string& name,
                 const std::string& category,
                 int quantity,
                 double price,
                 const std::string& warehouseId,
                 const std::string& binLocation)
    : productId(id)
    , name(name)
    , category(category)
    , quantity(quantity)
    , price(price)
    , warehouseId(warehouseId)
    , binLocation(binLocation)
{
}

// --- Read access ---

const std::string& Product::getProductId() const { return productId; }
const std::string& Product::getName() const { return name; }
const std::string& Product::getCategory() const { return category; }
int Product::getQuantity() const { return quantity; }
double Product::getPrice() const { return price; }
const std::string& Product::getWarehouseId() const { return warehouseId; }
const std::string& Product::getBinLocation() const { return binLocation; }

// --- Display ---

void Product::display() const
{
    std::cout << "Product [" << productId << "]\n"
              << "  Name:        " << name << "\n"
              << "  Category:    " << category << "\n"
              << "  Quantity:    " << quantity << "\n"
              << "  Price:       " << price << "\n"
              << "  Warehouse:   " << warehouseId << "\n"
              << "  Bin:         " << binLocation << "\n";
}

// --- Equality ---

bool Product::operator==(const Product& other) const
{
    return productId == other.productId
        && name == other.name
        && category == other.category
        && quantity == other.quantity
        && price == other.price
        && warehouseId == other.warehouseId
        && binLocation == other.binLocation;
}

bool Product::operator!=(const Product& other) const
{
    return !(*this == other);
}

// --- Quantity mutation (private, friend-accessible to HashTable) ---

void Product::setQuantity(int newQuantity)
{
    quantity = newQuantity;
}
