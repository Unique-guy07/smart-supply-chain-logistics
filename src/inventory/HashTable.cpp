#include "inventory/HashTable.h"
#include <iostream>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Node
// ---------------------------------------------------------------------------

HashTable::Node::Node(const Product& p, Node* n)
    : product(p)
    , next(n)
{
}

// ---------------------------------------------------------------------------
// Hash function — djb2
// ---------------------------------------------------------------------------

int HashTable::hash(const std::string& key) const
{
    unsigned long hashValue = 5381;
    for (char c : key) {
        hashValue = hashValue * 33 + static_cast<unsigned char>(c);
    }
    return static_cast<int>(hashValue % static_cast<unsigned long>(capacity));
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

HashTable::HashTable(int capacity)
    : table(nullptr)
    , capacity(capacity)
    , count(0)
{
    if (capacity <= 0) {
        throw std::invalid_argument("HashTable capacity must be positive");
    }
    table = new Node*[capacity]();  // value-initializes all pointers to nullptr
}

// ---------------------------------------------------------------------------
// Destructor — walks every chain and deallocates every node
// ---------------------------------------------------------------------------

HashTable::~HashTable()
{
    for (int i = 0; i < capacity; ++i) {
        Node* current = table[i];
        while (current != nullptr) {
            Node* toDelete = current;
            current = current->next;
            delete toDelete;
        }
    }
    delete[] table;
}

// ---------------------------------------------------------------------------
// Insert — keyed by product.getProductId()
// ---------------------------------------------------------------------------

bool HashTable::insert(const Product& product)
{
    const std::string& id = product.getProductId();
    int index = hash(id);

    // Check for duplicate ID in this chain
    Node* current = table[index];
    while (current != nullptr) {
        if (current->product.getProductId() == id) {
            return false;  // duplicate — reject
        }
        current = current->next;
    }

    // Prepend new node to the chain
    table[index] = new Node(product, table[index]);
    ++count;
    return true;
}

// ---------------------------------------------------------------------------
// Search — returns pointer to stored Product, or nullptr
// ---------------------------------------------------------------------------

Product* HashTable::search(const std::string& productId)
{
    return const_cast<Product*>(
        static_cast<const HashTable*>(this)->search(productId));
}

const Product* HashTable::search(const std::string& productId) const
{
    int index = hash(productId);
    Node* current = table[index];
    while (current != nullptr) {
        if (current->product.getProductId() == productId) {
            return &(current->product);
        }
        current = current->next;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Remove — unlinks and deletes the node
// ---------------------------------------------------------------------------

bool HashTable::remove(const std::string& productId)
{
    int index = hash(productId);
    Node* current = table[index];
    Node* prev = nullptr;

    while (current != nullptr) {
        if (current->product.getProductId() == productId) {
            if (prev == nullptr) {
                table[index] = current->next;
            } else {
                prev->next = current->next;
            }
            delete current;
            --count;
            return true;
        }
        prev = current;
        current = current->next;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Update Quantity — single mutation pathway via Product::setQuantity
// ---------------------------------------------------------------------------

bool HashTable::updateQuantity(const std::string& productId, int newQuantity)
{
    int index = hash(productId);
    Node* current = table[index];
    while (current != nullptr) {
        if (current->product.getProductId() == productId) {
            current->product.setQuantity(newQuantity);
            return true;
        }
        current = current->next;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

int HashTable::getCount() const { return count; }
int HashTable::getCapacity() const { return capacity; }
bool HashTable::isEmpty() const { return count == 0; }

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

void HashTable::display() const
{
    std::cout << "HashTable (" << count << " products, "
              << capacity << " buckets):\n";
    for (int i = 0; i < capacity; ++i) {
        std::cout << "  Bucket " << i << ": ";
        Node* current = table[i];
        if (current == nullptr) {
            std::cout << "[empty]";
        }
        while (current != nullptr) {
            std::cout << "[" << current->product.getProductId()
                      << " | " << current->product.getName()
                      << " | qty:" << current->product.getQuantity() << "] ";
            current = current->next;
        }
        std::cout << "\n";
    }
}
