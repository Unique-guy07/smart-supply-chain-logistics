#include "inventory/LinearProbingHashTable.h"
#include <climits>
#include <iostream>
#include <stdexcept>
#include <utility>

// ---------------------------------------------------------------------------
// Hash function — djb2
// ---------------------------------------------------------------------------

int LinearProbingHashTable::hash(const std::string& key) const
{
    unsigned long hashValue = 5381;
    for (char c : key) {
        hashValue = ((hashValue << 5) + hashValue) + static_cast<unsigned char>(c);
    }
    return static_cast<int>(hashValue % static_cast<unsigned long>(capacity));
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

LinearProbingHashTable::LinearProbingHashTable(
    int initialCapacity,
    double maxLoadFactor,
    int maxCapacity)
    : table(nullptr)
    , capacity(initialCapacity)
    , maxCapacity(maxCapacity)
    , count(0)
    , deletedCount(0)
    , maxLoadFactor(maxLoadFactor)
{
    if (initialCapacity <= 0 || initialCapacity > MAX_ALLOWABLE_CAPACITY) {
        throw std::invalid_argument(
            "LinearProbingHashTable capacity must be positive and within allowable limits");
    }
    if (maxLoadFactor <= 0.0 || maxLoadFactor >= 1.0) {
        throw std::invalid_argument(
            "LinearProbingHashTable maxLoadFactor must be strictly between 0.0 and 1.0");
    }
    if (maxCapacity < 0 || maxCapacity > MAX_ALLOWABLE_CAPACITY) {
        throw std::invalid_argument(
            "LinearProbingHashTable maxCapacity must be non-negative and within allowable limits");
    }
    if (maxCapacity > 0 && initialCapacity > maxCapacity) {
        throw std::invalid_argument(
            "LinearProbingHashTable initialCapacity cannot exceed maxCapacity");
    }
    table = new Entry[capacity]();
}

// ---------------------------------------------------------------------------
// Destructor — releases slot array; does NOT destroy non-owning Product*
// ---------------------------------------------------------------------------

LinearProbingHashTable::~LinearProbingHashTable()
{
    delete[] table;
}

// ---------------------------------------------------------------------------
// Rehash — transfers active entries, purges Deleted tombstones
// ---------------------------------------------------------------------------

void LinearProbingHashTable::rehash(int newCapacity)
{
    if (newCapacity <= 0) {
        newCapacity = 1;
    }
    if (newCapacity > MAX_ALLOWABLE_CAPACITY) {
        newCapacity = MAX_ALLOWABLE_CAPACITY;
    }
    if (maxCapacity > 0 && newCapacity > maxCapacity) {
        newCapacity = maxCapacity;
    }

    Entry* newTable = new Entry[newCapacity]();
    Entry* oldTable = table;
    int oldCapacity = capacity;

    table = newTable;
    capacity = newCapacity;
    count = 0;
    deletedCount = 0;

    for (int i = 0; i < oldCapacity; ++i) {
        if (oldTable[i].state == SlotState::Occupied) {
            int baseIndex = hash(oldTable[i].productId);
            for (int j = 0; j < capacity; ++j) {
                size_t idx = (static_cast<size_t>(baseIndex) + static_cast<size_t>(j)) % static_cast<size_t>(capacity);
                if (table[idx].state == SlotState::Empty) {
                    table[idx].productId = std::move(oldTable[i].productId);
                    table[idx].product = oldTable[i].product;
                    table[idx].state = SlotState::Occupied;
                    ++count;
                    break;
                }
            }
        }
    }

    delete[] oldTable;
}

// ---------------------------------------------------------------------------
// Insert — open addressing with linear probing and tombstone recycling
// ---------------------------------------------------------------------------

bool LinearProbingHashTable::insert(Product* product)
{
    if (product == nullptr) {
        return false;
    }

    const std::string& key = product->getProductId();

    // Safe arithmetic: evaluate thresholds without addition to count that could overflow.
    int resizeThreshold = static_cast<int>(static_cast<double>(capacity) * maxLoadFactor);
    int tombstoneThreshold = static_cast<int>(static_cast<double>(capacity) * 0.75);

    if (count >= resizeThreshold) {
        if (maxCapacity > 0 && capacity >= maxCapacity) {
            // Embedded memory budget capped: cannot grow further.
            // Check if physically full using 64-bit arithmetic to prevent any signed overflow.
            if (static_cast<int64_t>(count) + static_cast<int64_t>(deletedCount) >= capacity) {
                if (deletedCount > 0) {
                    rehash(capacity);  // Compact tombstones in-place to reclaim slots
                } else {
                    return false;      // Physically full of active entries
                }
            }
        } else {
            // Double capacity with overflow guard against MAX_ALLOWABLE_CAPACITY
            int newCapacity = (capacity > MAX_ALLOWABLE_CAPACITY / 2)
                                  ? MAX_ALLOWABLE_CAPACITY
                                  : capacity * 2;
            if (maxCapacity > 0 && newCapacity > maxCapacity) {
                newCapacity = maxCapacity;
            }
            if (newCapacity > capacity) {
                rehash(newCapacity);
            } else if (static_cast<int64_t>(count) + static_cast<int64_t>(deletedCount) >= capacity) {
                if (deletedCount > 0) {
                    rehash(capacity);
                } else {
                    return false;  // At capacity boundary and physically full
                }
            }
        }
    } else if (static_cast<int64_t>(count) + static_cast<int64_t>(deletedCount) >= tombstoneThreshold) {
        // High tombstone density: compact in-place to reclaim deleted slots
        rehash(capacity);
    }

    int baseIndex = hash(key);
    int firstDeletedIndex = -1;

    for (int i = 0; i < capacity; ++i) {
        size_t idx = (static_cast<size_t>(baseIndex) + static_cast<size_t>(i)) % static_cast<size_t>(capacity);

        if (table[idx].state == SlotState::Occupied) {
            if (table[idx].productId == key) {
                return false;  // Duplicate productId rejected
            }
        } else if (table[idx].state == SlotState::Deleted) {
            if (firstDeletedIndex == -1) {
                firstDeletedIndex = static_cast<int>(idx);
            }
        } else if (table[idx].state == SlotState::Empty) {
            // End of probe sequence — key not present in table
            int targetIndex = (firstDeletedIndex != -1) ? firstDeletedIndex : static_cast<int>(idx);
            if (firstDeletedIndex != -1) {
                --deletedCount;
            }
            table[targetIndex].productId = key;
            table[targetIndex].product = product;
            table[targetIndex].state = SlotState::Occupied;
            ++count;
            return true;
        }
    }

    // Traversed entire capacity without hitting Empty
    if (firstDeletedIndex != -1) {
        --deletedCount;
        table[firstDeletedIndex].productId = key;
        table[firstDeletedIndex].product = product;
        table[firstDeletedIndex].state = SlotState::Occupied;
        ++count;
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Search — probes through Deleted slots until Empty or key is found
// ---------------------------------------------------------------------------

Product* LinearProbingHashTable::search(const std::string& productId) const
{
    if (isEmpty()) {
        return nullptr;
    }

    int baseIndex = hash(productId);
    for (int i = 0; i < capacity; ++i) {
        size_t idx = (static_cast<size_t>(baseIndex) + static_cast<size_t>(i)) % static_cast<size_t>(capacity);

        if (table[idx].state == SlotState::Empty) {
            return nullptr;  // Probe sequence terminated
        }
        if (table[idx].state == SlotState::Occupied && table[idx].productId == productId) {
            return table[idx].product;
        }
        // If SlotState::Deleted, continue probing
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// Contains
// ---------------------------------------------------------------------------

bool LinearProbingHashTable::contains(const std::string& productId) const
{
    return search(productId) != nullptr;
}

// ---------------------------------------------------------------------------
// Remove — tombstone deletion (non-allocating, non-throwing for exception safety)
// ---------------------------------------------------------------------------

bool LinearProbingHashTable::remove(const std::string& productId)
{
    if (isEmpty()) {
        return false;
    }

    int baseIndex = hash(productId);
    for (int i = 0; i < capacity; ++i) {
        size_t idx = (static_cast<size_t>(baseIndex) + static_cast<size_t>(i)) % static_cast<size_t>(capacity);

        if (table[idx].state == SlotState::Empty) {
            return false;
        }
        if (table[idx].state == SlotState::Occupied && table[idx].productId == productId) {
            table[idx].product = nullptr;
            table[idx].productId.clear();
            table[idx].state = SlotState::Deleted;
            --count;
            ++deletedCount;
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

int LinearProbingHashTable::getCount() const
{
    return count;
}

int LinearProbingHashTable::getCapacity() const
{
    return capacity;
}

int LinearProbingHashTable::getDeletedCount() const
{
    return deletedCount;
}

double LinearProbingHashTable::getLoadFactor() const
{
    if (capacity <= 0) {
        return 0.0;
    }
    return static_cast<double>(count) / static_cast<double>(capacity);
}

bool LinearProbingHashTable::isEmpty() const
{
    return count == 0;
}

// ---------------------------------------------------------------------------
// Clear
// ---------------------------------------------------------------------------

void LinearProbingHashTable::clear()
{
    for (int i = 0; i < capacity; ++i) {
        table[i].productId.clear();
        table[i].product = nullptr;
        table[i].state = SlotState::Empty;
    }
    count = 0;
    deletedCount = 0;
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

void LinearProbingHashTable::display() const
{
    std::cout << "LinearProbingHashTable (" << count << " active, "
              << deletedCount << " deleted, " << capacity << " capacity, "
              << "load factor: " << getLoadFactor() << "):\n";
    for (int i = 0; i < capacity; ++i) {
        std::cout << "  Slot " << i << ": ";
        if (table[i].state == SlotState::Empty) {
            std::cout << "[empty]\n";
        } else if (table[i].state == SlotState::Deleted) {
            std::cout << "[deleted (tombstone)]\n";
        } else {
            std::cout << "[" << table[i].productId << " -> "
                      << (table[i].product ? table[i].product->getName() : "null")
                      << " | qty:" << (table[i].product ? table[i].product->getQuantity() : 0)
                      << "]\n";
        }
    }
}
