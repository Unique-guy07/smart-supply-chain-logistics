#include "test_utils.h"
#include "inventory/HashTable.h"
#include <stdexcept>

// ---------------------------------------------------------------------------
// Test: Empty table state
// ---------------------------------------------------------------------------
void testEmptyTable()
{
    HashTable ht(10);
    TEST_CHECK(ht.isEmpty());
    TEST_CHECK(ht.getCount() == 0);
    TEST_CHECK(ht.getCapacity() == 10);
    TEST_CHECK(ht.search("nonexistent") == nullptr);
}

// ---------------------------------------------------------------------------
// Test: Invalid capacity throws std::invalid_argument
// ---------------------------------------------------------------------------
void testInvalidCapacity()
{
    bool caughtZero = false;
    try {
        HashTable ht(0);
    } catch (const std::invalid_argument&) {
        caughtZero = true;
    }
    TEST_CHECK(caughtZero);

    bool caughtNegative = false;
    try {
        HashTable ht(-5);
    } catch (const std::invalid_argument&) {
        caughtNegative = true;
    }
    TEST_CHECK(caughtNegative);
}

// ---------------------------------------------------------------------------
// Test: Single insert and search
// ---------------------------------------------------------------------------
void testSingleInsertSearch()
{
    HashTable ht(10);
    Product p("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");

    TEST_CHECK(ht.insert(p));
    TEST_CHECK(ht.getCount() == 1);
    TEST_CHECK(!ht.isEmpty());

    Product* found = ht.search("SKU-001");
    TEST_CHECK(found != nullptr);
    TEST_CHECK(found->getProductId() == "SKU-001");
    TEST_CHECK(found->getName() == "Widget");
    TEST_CHECK(found->getQuantity() == 50);
}

// ---------------------------------------------------------------------------
// Test: Multiple inserts
// ---------------------------------------------------------------------------
void testMultipleInserts()
{
    HashTable ht(10);
    Product p1("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    Product p2("SKU-002", "Gadget", "Tools", 20, 14.50, "WH-B", "R1-B2");
    Product p3("SKU-003", "Gizmo", "Hardware", 100, 5.00, "WH-A", "R5-B1");

    TEST_CHECK(ht.insert(p1));
    TEST_CHECK(ht.insert(p2));
    TEST_CHECK(ht.insert(p3));
    TEST_CHECK(ht.getCount() == 3);

    TEST_CHECK(ht.search("SKU-001") != nullptr);
    TEST_CHECK(ht.search("SKU-002") != nullptr);
    TEST_CHECK(ht.search("SKU-003") != nullptr);

    TEST_CHECK(ht.search("SKU-001")->getName() == "Widget");
    TEST_CHECK(ht.search("SKU-002")->getName() == "Gadget");
    TEST_CHECK(ht.search("SKU-003")->getName() == "Gizmo");
}

// ---------------------------------------------------------------------------
// Test: Collision handling - small capacity forces collisions
// ---------------------------------------------------------------------------
void testCollisionHandling()
{
    HashTable ht(2);

    Product p1("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    Product p2("SKU-002", "Gadget", "Tools", 20, 14.50, "WH-B", "R1-B2");
    Product p3("SKU-003", "Gizmo", "Hardware", 100, 5.00, "WH-A", "R5-B1");

    TEST_CHECK(ht.insert(p1));
    TEST_CHECK(ht.insert(p2));
    TEST_CHECK(ht.insert(p3));
    TEST_CHECK(ht.getCount() == 3);

    // With 2 buckets and 3 products, at least two products must
    // map to the same bucket.
    int bucket1 = ht.hashFunction(p1.getProductId());
    int bucket2 = ht.hashFunction(p2.getProductId());
    int bucket3 = ht.hashFunction(p3.getProductId());

    bool collisionExists =
        (bucket1 == bucket2) ||
        (bucket1 == bucket3) ||
        (bucket2 == bucket3);

    TEST_CHECK(collisionExists);

    // All products must remain searchable despite the collision.
    Product* f1 = ht.search("SKU-001");
    Product* f2 = ht.search("SKU-002");
    Product* f3 = ht.search("SKU-003");

    TEST_CHECK(f1 != nullptr);
    TEST_CHECK(f2 != nullptr);
    TEST_CHECK(f3 != nullptr);
    TEST_CHECK(f1->getProductId() == "SKU-001");
    TEST_CHECK(f2->getProductId() == "SKU-002");
    TEST_CHECK(f3->getProductId() == "SKU-003");
}

// ---------------------------------------------------------------------------
// Test: Duplicate product ID is rejected
// ---------------------------------------------------------------------------
void testDuplicateInsert()
{
    HashTable ht(10);
    Product p1("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    Product p2("SKU-001", "Different", "Other", 10, 1.00, "WH-C", "R9-B9");

    TEST_CHECK(ht.insert(p1));
    TEST_CHECK(!ht.insert(p2));  // same ID - must be rejected
    TEST_CHECK(ht.getCount() == 1);

    // Original product remains unchanged
    Product* found = ht.search("SKU-001");
    TEST_CHECK(found != nullptr);
    TEST_CHECK(found->getName() == "Widget");
}

// ---------------------------------------------------------------------------
// Test: Search for missing ID
// ---------------------------------------------------------------------------
void testSearchMissing()
{
    HashTable ht(10);
    Product p("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    ht.insert(p);

    TEST_CHECK(ht.search("SKU-999") == nullptr);
    TEST_CHECK(ht.search("") == nullptr);
    TEST_CHECK(ht.search("sku-001") == nullptr);  // case-sensitive
}

// ---------------------------------------------------------------------------
// Test: Delete existing product
// ---------------------------------------------------------------------------
void testDeleteExisting()
{
    HashTable ht(10);
    Product p1("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    Product p2("SKU-002", "Gadget", "Tools", 20, 14.50, "WH-B", "R1-B2");
    ht.insert(p1);
    ht.insert(p2);

    TEST_CHECK(ht.remove("SKU-001"));
    TEST_CHECK(ht.getCount() == 1);
    TEST_CHECK(ht.search("SKU-001") == nullptr);

    // Other product unaffected
    TEST_CHECK(ht.search("SKU-002") != nullptr);
    TEST_CHECK(ht.search("SKU-002")->getName() == "Gadget");
}

// ---------------------------------------------------------------------------
// Test: Delete from a chain (collision scenario)
// ---------------------------------------------------------------------------
void testDeleteFromChain()
{
    HashTable ht(1);  // single bucket - everything chains
    Product p1("A", "Alpha", "Cat", 1, 1.0, "W", "B1");
    Product p2("B", "Beta", "Cat", 2, 2.0, "W", "B2");
    Product p3("C", "Gamma", "Cat", 3, 3.0, "W", "B3");
    ht.insert(p1);
    ht.insert(p2);
    ht.insert(p3);

    // Delete middle of chain
    TEST_CHECK(ht.remove("B"));
    TEST_CHECK(ht.getCount() == 2);
    TEST_CHECK(ht.search("B") == nullptr);
    TEST_CHECK(ht.search("A") != nullptr);
    TEST_CHECK(ht.search("C") != nullptr);

    // Delete head of chain
    TEST_CHECK(ht.remove("C"));
    TEST_CHECK(ht.getCount() == 1);
    TEST_CHECK(ht.search("C") == nullptr);
    TEST_CHECK(ht.search("A") != nullptr);

    // Delete last remaining
    TEST_CHECK(ht.remove("A"));
    TEST_CHECK(ht.getCount() == 0);
    TEST_CHECK(ht.isEmpty());
    TEST_CHECK(ht.search("A") == nullptr);
}

// ---------------------------------------------------------------------------
// Test: Delete missing product
// ---------------------------------------------------------------------------
void testDeleteMissing()
{
    HashTable ht(10);
    Product p("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    ht.insert(p);

    TEST_CHECK(!ht.remove("SKU-999"));
    TEST_CHECK(ht.getCount() == 1);  // unchanged
}

// ---------------------------------------------------------------------------
// Test: Quantity update
// ---------------------------------------------------------------------------
void testUpdateQuantity()
{
    HashTable ht(10);
    Product p("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    ht.insert(p);

    TEST_CHECK(ht.updateQuantity("SKU-001", 75));

    Product* found = ht.search("SKU-001");
    TEST_CHECK(found != nullptr);
    TEST_CHECK(found->getQuantity() == 75);

    // Update to zero
    TEST_CHECK(ht.updateQuantity("SKU-001", 0));
    TEST_CHECK(ht.search("SKU-001")->getQuantity() == 0);
}

// ---------------------------------------------------------------------------
// Test: Update missing product
// ---------------------------------------------------------------------------
void testUpdateMissing()
{
    HashTable ht(10);
    TEST_CHECK(!ht.updateQuantity("SKU-999", 100));
}

// ---------------------------------------------------------------------------
// Test: Display runs without crash
// ---------------------------------------------------------------------------
void testDisplay()
{
    HashTable ht(5);
    ht.display();  // empty table
    TEST_CHECK(true);

    Product p("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    ht.insert(p);
    ht.display();  // non-empty table
    TEST_CHECK(true);
}

// ---------------------------------------------------------------------------
// Test: Destructor safely cleans up a populated table
// ---------------------------------------------------------------------------
void testDestructorCleanup()
{
    // Create a table, populate it, and let it go out of scope.
    // The test verifies that destruction completes safely.
    {
        HashTable ht(3);
        ht.insert(Product("A", "Alpha", "C", 1, 1.0, "W", "B1"));
        ht.insert(Product("B", "Beta",  "C", 2, 2.0, "W", "B2"));
        ht.insert(Product("C", "Gamma", "C", 3, 3.0, "W", "B3"));
        ht.insert(Product("D", "Delta", "C", 4, 4.0, "W", "B4"));
    }
    TEST_CHECK(true);  // reached here without crash
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    TEST_RUN(testEmptyTable);
    TEST_RUN(testInvalidCapacity);
    TEST_RUN(testSingleInsertSearch);
    TEST_RUN(testMultipleInserts);
    TEST_RUN(testCollisionHandling);
    TEST_RUN(testDuplicateInsert);
    TEST_RUN(testSearchMissing);
    TEST_RUN(testDeleteExisting);
    TEST_RUN(testDeleteFromChain);
    TEST_RUN(testDeleteMissing);
    TEST_RUN(testUpdateQuantity);
    TEST_RUN(testUpdateMissing);
    TEST_RUN(testDisplay);
    TEST_RUN(testDestructorCleanup);
    TEST_REPORT();
}
