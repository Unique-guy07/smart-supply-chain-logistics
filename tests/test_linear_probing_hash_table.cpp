#include "test_utils.h"
#include "inventory/LinearProbingHashTable.h"
#include "inventory/Product.h"
#include <climits>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Test Accessor: Narrowly scoped friend accessor to test exact internal hash
// ---------------------------------------------------------------------------
class LinearProbingHashTableTestAccessor
{
public:
    static int hash(const LinearProbingHashTable& table, const std::string& key)
    {
        return table.hash(key);
    }
};

// ---------------------------------------------------------------------------
// Helper: Deterministically find keys that produce an exact target bucket index
// ---------------------------------------------------------------------------
static std::vector<std::string> findCollidingKeys(
    const LinearProbingHashTable& table,
    int targetBucket,
    int count)
{
    std::vector<std::string> keys;
    int counter = 0;
    while (keys.size() < static_cast<size_t>(count)) {
        std::string candidate = "COLLIDE_KEY_" + std::to_string(counter++);
        if (LinearProbingHashTableTestAccessor::hash(table, candidate) == targetBucket) {
            keys.push_back(candidate);
        }
    }
    return keys;
}

// ---------------------------------------------------------------------------
// T1: Empty table state
// ---------------------------------------------------------------------------
void testEmptyTable()
{
    LinearProbingHashTable table(16);
    TEST_CHECK(table.isEmpty());
    TEST_CHECK(table.getCount() == 0);
    TEST_CHECK(table.getCapacity() == 16);
    TEST_CHECK(table.getDeletedCount() == 0);
    TEST_CHECK(table.getLoadFactor() == 0.0);
    TEST_CHECK(table.search("SKU-001") == nullptr);
    TEST_CHECK(!table.contains("SKU-001"));
}

// ---------------------------------------------------------------------------
// T2: Invalid constructor arguments
// ---------------------------------------------------------------------------
void testInvalidArguments()
{
    bool caughtZeroCap = false;
    try {
        LinearProbingHashTable table(0);
    } catch (const std::invalid_argument&) {
        caughtZeroCap = true;
    }
    TEST_CHECK(caughtZeroCap);

    bool caughtNegCap = false;
    try {
        LinearProbingHashTable table(-5);
    } catch (const std::invalid_argument&) {
        caughtNegCap = true;
    }
    TEST_CHECK(caughtNegCap);

    bool caughtZeroLf = false;
    try {
        LinearProbingHashTable table(16, 0.0);
    } catch (const std::invalid_argument&) {
        caughtZeroLf = true;
    }
    TEST_CHECK(caughtZeroLf);

    bool caughtHighLf = false;
    try {
        LinearProbingHashTable table(16, 1.0);
    } catch (const std::invalid_argument&) {
        caughtHighLf = true;
    }
    TEST_CHECK(caughtHighLf);

    bool caughtNegMaxCap = false;
    try {
        LinearProbingHashTable table(16, 0.70, -1);
    } catch (const std::invalid_argument&) {
        caughtNegMaxCap = true;
    }
    TEST_CHECK(caughtNegMaxCap);

    bool caughtInitExceedMax = false;
    try {
        LinearProbingHashTable table(32, 0.70, 16);
    } catch (const std::invalid_argument&) {
        caughtInitExceedMax = true;
    }
    TEST_CHECK(caughtInitExceedMax);

    bool caughtHugeInitCap = false;
    try {
        LinearProbingHashTable table(LinearProbingHashTable::MAX_ALLOWABLE_CAPACITY + 1);
    } catch (const std::invalid_argument&) {
        caughtHugeInitCap = true;
    }
    TEST_CHECK(caughtHugeInitCap);

    bool caughtHugeMaxCap = false;
    try {
        LinearProbingHashTable table(16, 0.70, LinearProbingHashTable::MAX_ALLOWABLE_CAPACITY + 1);
    } catch (const std::invalid_argument&) {
        caughtHugeMaxCap = true;
    }
    TEST_CHECK(caughtHugeMaxCap);
}

// ---------------------------------------------------------------------------
// T3: Single insert and search
// ---------------------------------------------------------------------------
void testInsertAndSearch()
{
    LinearProbingHashTable table(16);
    Product p("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");

    TEST_CHECK(table.insert(&p));
    TEST_CHECK(!table.isEmpty());
    TEST_CHECK(table.getCount() == 1);
    TEST_CHECK(table.contains("SKU-001"));

    Product* found = table.search("SKU-001");
    TEST_CHECK(found != nullptr);
    TEST_CHECK(found == &p);  // Exact non-owning pointer preserved
    TEST_CHECK(found->getProductId() == "SKU-001");
    TEST_CHECK(found->getName() == "Widget");
    TEST_CHECK(found->getQuantity() == 50);
}

// ---------------------------------------------------------------------------
// T4: Multiple entries without collision
// ---------------------------------------------------------------------------
void testMultipleEntries()
{
    LinearProbingHashTable table(32);
    Product p1("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    Product p2("SKU-002", "Gadget", "Tools", 20, 14.50, "WH-B", "R1-B2");
    Product p3("SKU-003", "Gizmo", "Hardware", 100, 5.00, "WH-A", "R5-B1");

    TEST_CHECK(table.insert(&p1));
    TEST_CHECK(table.insert(&p2));
    TEST_CHECK(table.insert(&p3));
    TEST_CHECK(table.getCount() == 3);

    TEST_CHECK(table.search("SKU-001") == &p1);
    TEST_CHECK(table.search("SKU-002") == &p2);
    TEST_CHECK(table.search("SKU-003") == &p3);
}

// ---------------------------------------------------------------------------
// T5 (Fix 1): Real collision test — programmatically forced same initial bucket
// ---------------------------------------------------------------------------
void testCollisionHandling()
{
    LinearProbingHashTable table(16, 0.90);
    const int targetBucket = 5;

    // Find 3 distinct keys that all produce targetBucket
    std::vector<std::string> keys = findCollidingKeys(table, targetBucket, 3);
    TEST_CHECK(keys.size() == 3);
    for (const std::string& k : keys) {
        TEST_CHECK(LinearProbingHashTableTestAccessor::hash(table, k) == targetBucket);
    }

    Product p1(keys[0], "Colliding A", "Cat", 10, 1.0, "WH-1", "B-1");
    Product p2(keys[1], "Colliding B", "Cat", 20, 2.0, "WH-1", "B-2");
    Product p3(keys[2], "Colliding C", "Cat", 30, 3.0, "WH-1", "B-3");

    TEST_CHECK(table.insert(&p1));
    TEST_CHECK(table.insert(&p2));
    TEST_CHECK(table.insert(&p3));
    TEST_CHECK(table.getCount() == 3);

    // All must be found via linear probing across sequential slots
    TEST_CHECK(table.search(keys[0]) == &p1);
    TEST_CHECK(table.search(keys[1]) == &p2);
    TEST_CHECK(table.search(keys[2]) == &p3);
}

// ---------------------------------------------------------------------------
// T6 (Fix 2): Real wrap-around test — probing wraps past capacity - 1 to 0
// ---------------------------------------------------------------------------
void testWrapAroundProbing()
{
    const int cap = 8;
    LinearProbingHashTable table(cap, 0.90);
    const int targetBucket = cap - 1;  // Index 7

    // Find 2 keys that both hash to the very last bucket (index 7)
    std::vector<std::string> keys = findCollidingKeys(table, targetBucket, 2);
    TEST_CHECK(LinearProbingHashTableTestAccessor::hash(table, keys[0]) == targetBucket);
    TEST_CHECK(LinearProbingHashTableTestAccessor::hash(table, keys[1]) == targetBucket);

    Product pLast(keys[0], "Last Slot Item", "Cat", 10, 1.0, "W", "B");
    Product pWrap(keys[1], "Wrapped Item", "Cat", 20, 2.0, "W", "B");

    // Insert pLast at index 7
    TEST_CHECK(table.insert(&pLast));
    // Insert pWrap: collides at index 7, probes and wraps around to index 0!
    TEST_CHECK(table.insert(&pWrap));
    TEST_CHECK(table.getCount() == 2);

    // Search must find both items across the wrap boundary
    TEST_CHECK(table.search(keys[0]) == &pLast);
    TEST_CHECK(table.search(keys[1]) == &pWrap);

    // Remove the item at the boundary (index 7 becomes tombstone)
    TEST_CHECK(table.remove(keys[0]));
    TEST_CHECK(table.search(keys[0]) == nullptr);

    // Searching for pWrap must probe past the tombstone at index 7 and find it at index 0
    TEST_CHECK(table.search(keys[1]) == &pWrap);

    // Remove wrapped item
    TEST_CHECK(table.remove(keys[1]));
    TEST_CHECK(table.search(keys[1]) == nullptr);
    TEST_CHECK(table.isEmpty());
}

// ---------------------------------------------------------------------------
// T7: Duplicate insertion rejection
// ---------------------------------------------------------------------------
void testDuplicateRejection()
{
    LinearProbingHashTable table(16);
    Product p1("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    Product p1_dup("SKU-001", "Duplicate", "Electronics", 99, 9.99, "WH-X", "R0");

    TEST_CHECK(table.insert(&p1));
    TEST_CHECK(!table.insert(&p1_dup));  // Must reject duplicate key
    TEST_CHECK(table.getCount() == 1);

    Product* found = table.search("SKU-001");
    TEST_CHECK(found == &p1);
    TEST_CHECK(found->getName() == "Widget");
}

// ---------------------------------------------------------------------------
// T8 (Fix 3): Real tombstone cluster test
// ---------------------------------------------------------------------------
void testTombstoneClusterPreservation()
{
    const int cap = 8;
    LinearProbingHashTable table(cap, 0.90);
    const int targetBucket = 2;

    // Generate 4 keys that all hash to bucket 2
    std::vector<std::string> keys = findCollidingKeys(table, targetBucket, 4);
    for (const std::string& k : keys) {
        TEST_CHECK(LinearProbingHashTableTestAccessor::hash(table, k) == targetBucket);
    }

    Product pA(keys[0], "A", "Cat", 1, 10.0, "WH", "B1");
    Product pB(keys[1], "B", "Cat", 2, 20.0, "WH", "B2");
    Product pC(keys[2], "C", "Cat", 3, 30.0, "WH", "B3");
    Product pD(keys[3], "D", "Cat", 4, 40.0, "WH", "B4");

    // Form cluster at indices 2, 3, 4
    TEST_CHECK(table.insert(&pA));
    TEST_CHECK(table.insert(&pB));
    TEST_CHECK(table.insert(&pC));
    TEST_CHECK(table.getCount() == 3);

    // Remove middle element B (index 3 becomes tombstone)
    TEST_CHECK(table.remove(keys[1]));
    TEST_CHECK(table.getCount() == 2);
    TEST_CHECK(table.getDeletedCount() == 1);
    TEST_CHECK(table.search(keys[1]) == nullptr);

    // 1. Probing past tombstone: C must still be found at index 4
    TEST_CHECK(table.search(keys[2]) == &pC);

    // 2. Duplicate detection beyond tombstone: inserting duplicate C must fail
    Product pC_dup(keys[2], "C Duplicate", "Cat", 99, 99.0, "WH", "B3");
    TEST_CHECK(!table.insert(&pC_dup));
    TEST_CHECK(table.getCount() == 2);
    TEST_CHECK(table.getDeletedCount() == 1);  // Tombstone was NOT overwritten with duplicate

    // 3. Tombstone recycling: inserting new colliding key D must reuse tombstone at index 3
    TEST_CHECK(table.insert(&pD));
    TEST_CHECK(table.getCount() == 3);
    TEST_CHECK(table.getDeletedCount() == 0);  // Tombstone recycled

    // Verify all active entries in the cluster
    TEST_CHECK(table.search(keys[0]) == &pA);
    TEST_CHECK(table.search(keys[3]) == &pD);
    TEST_CHECK(table.search(keys[2]) == &pC);
}

// ---------------------------------------------------------------------------
// T9 (Fix 4): Tombstone rehash / compaction preserving original Product pointers
// ---------------------------------------------------------------------------
void testTombstoneRehashPointerPreservation()
{
    LinearProbingHashTable table(8, 0.70);

    Product p1("SKU-1", "Prod1", "Cat", 10, 1.0, "WH", "B1");
    Product p2("SKU-2", "Prod2", "Cat", 20, 2.0, "WH", "B2");
    Product p3("SKU-3", "Prod3", "Cat", 30, 3.0, "WH", "B3");
    Product p4("SKU-4", "Prod4", "Cat", 40, 4.0, "WH", "B4");
    Product p5("SKU-5", "Prod5", "Cat", 50, 5.0, "WH", "B5");

    table.insert(&p1);
    table.insert(&p2);
    table.insert(&p3);
    table.insert(&p4);
    table.insert(&p5);

    // Delete 2 items to generate tombstones
    TEST_CHECK(table.remove("SKU-2"));
    TEST_CHECK(table.remove("SKU-4"));
    TEST_CHECK(table.getDeletedCount() == 2);
    TEST_CHECK(table.getCount() == 3);

    // Insert products to recycle tombstones and exceed load factor (count + 1 > 8 * 0.70 = 5.6)
    Product p6("SKU-6", "Prod6", "Cat", 60, 6.0, "WH", "B6");
    Product p7("SKU-7", "Prod7", "Cat", 70, 7.0, "WH", "B7");
    Product p8("SKU-8", "Prod8", "Cat", 80, 8.0, "WH", "B8");
    TEST_CHECK(table.insert(&p6));
    TEST_CHECK(table.insert(&p7));
    TEST_CHECK(table.insert(&p8));

    // Verify dynamic rehash occurred and tombstones were purged
    TEST_CHECK(table.getCapacity() >= 16);
    TEST_CHECK(table.getDeletedCount() == 0);
    TEST_CHECK(table.getCount() == 6);

    // CRITICAL: verify exact pointer equality and data integrity
    TEST_CHECK(table.search("SKU-1") == &p1);
    TEST_CHECK(table.search("SKU-3") == &p3);
    TEST_CHECK(table.search("SKU-5") == &p5);
    TEST_CHECK(table.search("SKU-6") == &p6);
    TEST_CHECK(table.search("SKU-7") == &p7);
    TEST_CHECK(table.search("SKU-8") == &p8);

    TEST_CHECK(table.search("SKU-1")->getQuantity() == 10);
    TEST_CHECK(table.search("SKU-3")->getQuantity() == 30);
    TEST_CHECK(table.search("SKU-5")->getQuantity() == 50);

    TEST_CHECK(table.search("SKU-2") == nullptr);
    TEST_CHECK(table.search("SKU-4") == nullptr);
}

// ---------------------------------------------------------------------------
// T10: Remove missing key
// ---------------------------------------------------------------------------
void testRemoveMissing()
{
    LinearProbingHashTable table(16);
    TEST_CHECK(!table.remove("SKU-NONE"));

    Product p1("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    table.insert(&p1);

    TEST_CHECK(!table.remove("SKU-NONE"));
    TEST_CHECK(table.getCount() == 1);
    TEST_CHECK(table.search("SKU-001") == &p1);
}

// ---------------------------------------------------------------------------
// T11: Dynamic resizing on load factor exceedance
// ---------------------------------------------------------------------------
void testDynamicResizing()
{
    // Initial capacity 4, maxLoadFactor 0.70
    // Threshold: (count + 1) > 4 * 0.70 = 2.8 -> inserting 3rd item triggers resize to 8!
    LinearProbingHashTable table(4, 0.70);
    TEST_CHECK(table.getCapacity() == 4);

    Product p1("SKU-1", "Item1", "Cat", 1, 1.0, "W", "B");
    Product p2("SKU-2", "Item2", "Cat", 2, 2.0, "W", "B");
    Product p3("SKU-3", "Item3", "Cat", 3, 3.0, "W", "B");

    TEST_CHECK(table.insert(&p1));
    TEST_CHECK(table.insert(&p2));
    TEST_CHECK(table.getCapacity() == 4);

    // Inserting 3rd element triggers dynamic resizing
    TEST_CHECK(table.insert(&p3));
    TEST_CHECK(table.getCapacity() == 8);
    TEST_CHECK(table.getCount() == 3);

    // All elements remain searchable after resize
    TEST_CHECK(table.search("SKU-1") == &p1);
    TEST_CHECK(table.search("SKU-2") == &p2);
    TEST_CHECK(table.search("SKU-3") == &p3);
}

// ---------------------------------------------------------------------------
// T12: nullptr insertion handling
// ---------------------------------------------------------------------------
void testNullptrInsertion()
{
    LinearProbingHashTable table(16);
    TEST_CHECK(!table.insert(nullptr));
    TEST_CHECK(table.isEmpty());
    TEST_CHECK(table.getCount() == 0);
}

// ---------------------------------------------------------------------------
// T13: Clear and destruction must never delete non-owning Product objects
// ---------------------------------------------------------------------------
void testClearAndDestructionWithoutProductDeletion()
{
    Product p1("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    Product p2("SKU-002", "Gadget", "Tools", 20, 14.50, "WH-B", "R1-B2");

    {
        LinearProbingHashTable table(16);
        table.insert(&p1);
        table.insert(&p2);
        table.remove("SKU-001");

        TEST_CHECK(table.getCount() == 1);
        TEST_CHECK(table.getDeletedCount() == 1);

        table.clear();
        TEST_CHECK(table.isEmpty());
        TEST_CHECK(table.getCount() == 0);
        TEST_CHECK(table.getDeletedCount() == 0);
        TEST_CHECK(table.search("SKU-002") == nullptr);

        // Re-insert into cleared table
        TEST_CHECK(table.insert(&p1));
        TEST_CHECK(table.getCount() == 1);
        TEST_CHECK(table.search("SKU-001") == &p1);
        // table is destroyed at end of scope
    }

    // CRITICAL: Product objects on stack must remain fully valid and accessible
    TEST_CHECK(p1.getProductId() == "SKU-001");
    TEST_CHECK(p1.getName() == "Widget");
    TEST_CHECK(p1.getQuantity() == 50);
    TEST_CHECK(p2.getProductId() == "SKU-002");
    TEST_CHECK(p2.getName() == "Gadget");
    TEST_CHECK(p2.getQuantity() == 20);
}

// ---------------------------------------------------------------------------
// T14 (Fix 8): Capacity overflow protection & maxCapacity enforcement
// ---------------------------------------------------------------------------
void testCapacityOverflowAndLimits()
{
    // Test capped maxCapacity
    LinearProbingHashTable cappedTable(2, 0.70, 2);
    Product p1("K1", "Item1", "Cat", 1, 1.0, "W", "B");
    Product p2("K2", "Item2", "Cat", 2, 2.0, "W", "B");
    Product p3("K3", "Item3", "Cat", 3, 3.0, "W", "B");

    TEST_CHECK(cappedTable.insert(&p1));
    TEST_CHECK(cappedTable.insert(&p2));
    // Third insert definitely exceeds capacity 2 -> returns false cleanly
    TEST_CHECK(!cappedTable.insert(&p3));
    TEST_CHECK(cappedTable.getCount() == 2);

    // Deleting an item creates a tombstone; subsequent insert should succeed via tombstone recycling or in-place compaction
    TEST_CHECK(cappedTable.remove("K1"));
    TEST_CHECK(cappedTable.getCount() == 1);
    TEST_CHECK(cappedTable.getDeletedCount() == 1);
    TEST_CHECK(cappedTable.insert(&p3));
    TEST_CHECK(cappedTable.getCount() == 2);
    TEST_CHECK(cappedTable.search("K3") == &p3);
    TEST_CHECK(cappedTable.search("K1") == nullptr);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    TEST_RUN(testEmptyTable);
    TEST_RUN(testInvalidArguments);
    TEST_RUN(testInsertAndSearch);
    TEST_RUN(testMultipleEntries);
    TEST_RUN(testCollisionHandling);
    TEST_RUN(testWrapAroundProbing);
    TEST_RUN(testDuplicateRejection);
    TEST_RUN(testTombstoneClusterPreservation);
    TEST_RUN(testTombstoneRehashPointerPreservation);
    TEST_RUN(testRemoveMissing);
    TEST_RUN(testDynamicResizing);
    TEST_RUN(testNullptrInsertion);
    TEST_RUN(testClearAndDestructionWithoutProductDeletion);
    TEST_RUN(testCapacityOverflowAndLimits);
    TEST_REPORT();
}
