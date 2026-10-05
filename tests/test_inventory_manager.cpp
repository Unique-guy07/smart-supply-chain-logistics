#include "test_utils.h"
#include "inventory/InventoryManager.h"
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Test: Empty inventory
// ---------------------------------------------------------------------------
void testEmptyInventory()
{
    InventoryManager mgr(10);
    TEST_CHECK(mgr.isEmpty());
    TEST_CHECK(mgr.getProductCount() == 0);
    TEST_CHECK(mgr.findProduct("SKU-001") == nullptr);
}

// ---------------------------------------------------------------------------
// Test: Add and find product
// ---------------------------------------------------------------------------
void testAddAndFind()
{
    InventoryManager mgr(10);
    Product p("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");

    TEST_CHECK(mgr.addProduct(p));
    TEST_CHECK(mgr.getProductCount() == 1);
    TEST_CHECK(!mgr.isEmpty());

    Product* found = mgr.findProduct("SKU-001");
    TEST_CHECK(found != nullptr);
    TEST_CHECK(found->getProductId() == "SKU-001");
    TEST_CHECK(found->getName() == "Widget");
    TEST_CHECK(found->getQuantity() == 50);
}

// ---------------------------------------------------------------------------
// Test: Unordered insertion produces a synchronized ordered index
// ---------------------------------------------------------------------------
void testOrderedIndexAndSynchronization()
{
    InventoryManager mgr(10);
    Product p3("SKU-003", "Gizmo", "Hardware", 100, 5.00, "WH-A", "R5-B1");
    Product p1("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    Product p2("SKU-002", "Gadget", "Tools", 20, 14.50, "WH-B", "R1-B2");

    TEST_CHECK(mgr.addProduct(p3));
    TEST_CHECK(mgr.addProduct(p1));
    TEST_CHECK(mgr.addProduct(p2));

    const std::vector<std::string> expectedInitial{
        "SKU-001", "SKU-002", "SKU-003"};
    const std::vector<std::string> initialIds = mgr.getProductIdsInOrder();
    TEST_CHECK(initialIds == expectedInitial);
    TEST_CHECK(mgr.getProductCount() == static_cast<int>(initialIds.size()));

    for (const std::string& productId : initialIds) {
        TEST_CHECK(mgr.findProduct(productId) != nullptr);
    }

    TEST_CHECK(mgr.removeProduct("SKU-002"));
    TEST_CHECK(mgr.findProduct("SKU-002") == nullptr);

    const std::vector<std::string> expectedAfterRemoval{
        "SKU-001", "SKU-003"};
    const std::vector<std::string> remainingIds = mgr.getProductIdsInOrder();
    TEST_CHECK(remainingIds == expectedAfterRemoval);
    TEST_CHECK(mgr.getProductCount() == static_cast<int>(remainingIds.size()));

    for (const std::string& productId : remainingIds) {
        TEST_CHECK(mgr.findProduct(productId) != nullptr);
    }
}

// ---------------------------------------------------------------------------
// Test: Add duplicate product ID
// ---------------------------------------------------------------------------
void testAddDuplicate()
{
    InventoryManager mgr(10);
    Product p1("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    Product p2("SKU-001", "Different", "Other", 10, 1.00, "WH-C", "R9-B9");

    TEST_CHECK(mgr.addProduct(p1));
    TEST_CHECK(!mgr.addProduct(p2));
    TEST_CHECK(mgr.getProductCount() == 1);

    // Original remains unchanged
    Product* found = mgr.findProduct("SKU-001");
    TEST_CHECK(found != nullptr);
    TEST_CHECK(found->getName() == "Widget");

    const std::vector<std::string> expected{"SKU-001"};
    TEST_CHECK(mgr.getProductIdsInOrder() == expected);
    TEST_CHECK(mgr.getProductCount()
               == static_cast<int>(mgr.getProductIdsInOrder().size()));
}

// ---------------------------------------------------------------------------
// Test: Remove product
// ---------------------------------------------------------------------------
void testRemoveProduct()
{
    InventoryManager mgr(10);
    Product p("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    mgr.addProduct(p);

    TEST_CHECK(mgr.removeProduct("SKU-001"));
    TEST_CHECK(mgr.getProductCount() == 0);
    TEST_CHECK(mgr.isEmpty());
    TEST_CHECK(mgr.findProduct("SKU-001") == nullptr);
    TEST_CHECK(mgr.getProductIdsInOrder().empty());
}

// ---------------------------------------------------------------------------
// Test: Remove missing product
// ---------------------------------------------------------------------------
void testRemoveMissing()
{
    InventoryManager mgr(10);
    Product p("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    mgr.addProduct(p);

    const std::vector<std::string> before = mgr.getProductIdsInOrder();
    TEST_CHECK(!mgr.removeProduct("SKU-999"));
    TEST_CHECK(mgr.getProductCount() == 1);
    TEST_CHECK(mgr.getProductIdsInOrder() == before);
    TEST_CHECK(mgr.findProduct("SKU-001") != nullptr);
}

// ---------------------------------------------------------------------------
// Test: Update product quantity
// ---------------------------------------------------------------------------
void testUpdateQuantity()
{
    InventoryManager mgr(10);
    Product p("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    mgr.addProduct(p);

    TEST_CHECK(mgr.updateProductQuantity("SKU-001", 75));

    Product* found = mgr.findProduct("SKU-001");
    TEST_CHECK(found != nullptr);
    TEST_CHECK(found->getQuantity() == 75);

    // Update to zero
    TEST_CHECK(mgr.updateProductQuantity("SKU-001", 0));
    TEST_CHECK(mgr.findProduct("SKU-001")->getQuantity() == 0);

    const std::vector<std::string> expected{"SKU-001"};
    TEST_CHECK(mgr.getProductIdsInOrder() == expected);
}

// ---------------------------------------------------------------------------
// Test: Update missing product
// ---------------------------------------------------------------------------
void testUpdateMissing()
{
    InventoryManager mgr(10);
    TEST_CHECK(!mgr.updateProductQuantity("SKU-999", 100));
}

// ---------------------------------------------------------------------------
// Test: Display runs without crash
// ---------------------------------------------------------------------------
void testDisplay()
{
    InventoryManager mgr(5);
    mgr.displayInventory();  // empty
    TEST_CHECK(true);

    mgr.addProduct(Product("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7"));
    mgr.addProduct(Product("SKU-002", "Gadget", "Tools", 20, 14.50, "WH-B", "R1-B2"));
    mgr.displayInventory();  // populated
    TEST_CHECK(true);
}

// ---------------------------------------------------------------------------
// Test T14: Three-way add synchronization across HashTable, LinearProbing, and BST
// ---------------------------------------------------------------------------
void testThreeWayAddSynchronization()
{
    InventoryManager mgr(10, 8);
    Product p1("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    Product p2("SKU-002", "Gadget", "Tools", 20, 14.50, "WH-B", "R1-B2");
    Product p3("SKU-003", "Gizmo", "Hardware", 100, 5.00, "WH-A", "R5-B1");

    TEST_CHECK(mgr.addProduct(p1));
    TEST_CHECK(mgr.addProduct(p2));
    TEST_CHECK(mgr.addProduct(p3));

    // Verify count consistency across all tiers
    TEST_CHECK(mgr.getProductCount() == 3);
    TEST_CHECK(mgr.getDeviceProductCount() == 3);
    TEST_CHECK(mgr.getProductIdsInOrder().size() == 3);

    // Verify lookup through both master and embedded tiers return the exact same object
    Product* master1 = mgr.findProduct("SKU-001");
    Product* device1 = mgr.findProductOnDevice("SKU-001");
    TEST_CHECK(master1 != nullptr);
    TEST_CHECK(device1 != nullptr);
    TEST_CHECK(master1 == device1);  // Exact pointer equivalence — no duplicate objects!
    TEST_CHECK(device1->getName() == "Widget");

    Product* master2 = mgr.findProduct("SKU-002");
    Product* device2 = mgr.findProductOnDevice("SKU-002");
    TEST_CHECK(master2 != nullptr);
    TEST_CHECK(device2 != nullptr);
    TEST_CHECK(master2 == device2);

    Product* master3 = mgr.findProduct("SKU-003");
    Product* device3 = mgr.findProductOnDevice("SKU-003");
    TEST_CHECK(master3 != nullptr);
    TEST_CHECK(device3 != nullptr);
    TEST_CHECK(master3 == device3);
}

// ---------------------------------------------------------------------------
// Test T15: Three-way remove synchronization
// ---------------------------------------------------------------------------
void testThreeWayRemoveSynchronization()
{
    InventoryManager mgr(10, 8);
    Product p1("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    Product p2("SKU-002", "Gadget", "Tools", 20, 14.50, "WH-B", "R1-B2");
    Product p3("SKU-003", "Gizmo", "Hardware", 100, 5.00, "WH-A", "R5-B1");

    mgr.addProduct(p1);
    mgr.addProduct(p2);
    mgr.addProduct(p3);

    // Remove middle product
    TEST_CHECK(mgr.removeProduct("SKU-002"));

    // Check all three tiers no longer contain SKU-002
    TEST_CHECK(mgr.findProduct("SKU-002") == nullptr);
    TEST_CHECK(mgr.findProductOnDevice("SKU-002") == nullptr);

    const std::vector<std::string> remaining = mgr.getProductIdsInOrder();
    const std::vector<std::string> expected{"SKU-001", "SKU-003"};
    TEST_CHECK(remaining == expected);

    TEST_CHECK(mgr.getProductCount() == 2);
    TEST_CHECK(mgr.getDeviceProductCount() == 2);

    // Remaining items still intact on both tiers
    TEST_CHECK(mgr.findProduct("SKU-001") == mgr.findProductOnDevice("SKU-001"));
    TEST_CHECK(mgr.findProduct("SKU-003") == mgr.findProductOnDevice("SKU-003"));
}

// ---------------------------------------------------------------------------
// Test T16: Quantity update reflected through the device index
// ---------------------------------------------------------------------------
void testQuantityUpdateReflectedOnDevice()
{
    InventoryManager mgr(10, 8);
    Product p1("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    mgr.addProduct(p1);

    // Verify initial quantity on device index
    Product* deviceProd = mgr.findProductOnDevice("SKU-001");
    TEST_CHECK(deviceProd != nullptr);
    TEST_CHECK(deviceProd->getQuantity() == 50);

    // Update quantity via manager
    TEST_CHECK(mgr.updateProductQuantity("SKU-001", 175));

    // Must be automatically and immediately reflected through device index
    TEST_CHECK(mgr.findProduct("SKU-001")->getQuantity() == 175);
    TEST_CHECK(mgr.findProductOnDevice("SKU-001")->getQuantity() == 175);
}

// ---------------------------------------------------------------------------
// Test Accessor: Narrowly scoped friend accessor for deterministic test seams
// ---------------------------------------------------------------------------
class InventoryManagerTestAccessor
{
public:
    static bool preinsertBst(InventoryManager& mgr, const std::string& productId)
    {
        return mgr.bstIndex.insert(productId);
    }

    static bool removeBst(InventoryManager& mgr, const std::string& productId)
    {
        return mgr.bstIndex.remove(productId);
    }

    static bool containsBst(const InventoryManager& mgr, const std::string& productId)
    {
        return mgr.bstIndex.contains(productId);
    }
};

// ---------------------------------------------------------------------------
// Test T17A (Fix 5): Genuine rollback when secondary tier 2 (deviceIndex) fails
// Demonstrates that a failure in deviceIndex rolls back the canonical HashTable.
// ---------------------------------------------------------------------------
void testRollbackOnDeviceIndexFailure()
{
    // Configure device tier with a fixed hardware limit of 2 slots
    InventoryManager mgr(10, 2, 2);
    Product p1("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    Product p2("SKU-002", "Gadget", "Tools", 20, 14.50, "WH-B", "R1-B2");
    Product p3("SKU-003", "Gizmo", "Hardware", 100, 5.00, "WH-A", "R5-B1");

    TEST_CHECK(mgr.addProduct(p1));
    TEST_CHECK(mgr.addProduct(p2));
    TEST_CHECK(mgr.getProductCount() == 2);
    TEST_CHECK(mgr.getDeviceProductCount() == 2);

    // p3 is inserted into canonical HashTable, but deviceIndex is capped at 2 and rejects it.
    // InventoryManager must roll back canonical HashTable and return false.
    TEST_CHECK(!mgr.addProduct(p3));

    // Verify canonical HashTable was rolled back — p3 must NOT be present in any tier!
    TEST_CHECK(mgr.findProduct("SKU-003") == nullptr);
    TEST_CHECK(mgr.findProductOnDevice("SKU-003") == nullptr);
    TEST_CHECK(mgr.getProductCount() == 2);
    TEST_CHECK(mgr.getDeviceProductCount() == 2);
    TEST_CHECK(mgr.getProductIdsInOrder().size() == 2);

    // Initial products p1 and p2 must remain completely intact and synchronized
    TEST_CHECK(mgr.findProduct("SKU-001") != nullptr);
    TEST_CHECK(mgr.findProductOnDevice("SKU-001") != nullptr);
    TEST_CHECK(mgr.findProduct("SKU-002") != nullptr);
    TEST_CHECK(mgr.findProductOnDevice("SKU-002") != nullptr);
}

// ---------------------------------------------------------------------------
// Test T17B (Fix 5): Genuine rollback when secondary tier 3 (BST) fails
// Demonstrates that a failure in BST rolls back BOTH deviceIndex AND HashTable.
// Verifies cleanup of injected test state and subsequent successful synchronization.
// ---------------------------------------------------------------------------
void testRollbackOnBstFailure()
{
    InventoryManager mgr(10, 16);
    Product p1("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    TEST_CHECK(mgr.addProduct(p1));

    // Inject a conflicting key into BST via the test seam
    TEST_CHECK(InventoryManagerTestAccessor::preinsertBst(mgr, "SKU-002"));

    // Attempt to add SKU-002 normally:
    // 1. HashTable::insert succeeds
    // 2. LinearProbingHashTable::insert succeeds
    // 3. BST::insert fails (duplicate key in BST)
    // Rollback MUST remove SKU-002 from LinearProbingHashTable AND HashTable!
    Product p2("SKU-002", "Gadget", "Tools", 20, 14.50, "WH-B", "R1-B2");
    TEST_CHECK(!mgr.addProduct(p2));

    // Verify SKU-002 was rolled back from both HashTable and deviceIndex
    TEST_CHECK(mgr.findProduct("SKU-002") == nullptr);
    TEST_CHECK(mgr.findProductOnDevice("SKU-002") == nullptr);
    TEST_CHECK(mgr.getProductCount() == 1);
    TEST_CHECK(mgr.getDeviceProductCount() == 1);

    // Initial product p1 must remain completely intact
    TEST_CHECK(mgr.findProduct("SKU-001") == mgr.findProductOnDevice("SKU-001"));

    // Verify the injected conflicting key exists in BST before cleanup
    TEST_CHECK(InventoryManagerTestAccessor::containsBst(mgr, "SKU-002"));

    // Clean up the intentionally injected test seam state to prevent leaving an orphaned key
    TEST_CHECK(InventoryManagerTestAccessor::removeBst(mgr, "SKU-002"));
    TEST_CHECK(!InventoryManagerTestAccessor::containsBst(mgr, "SKU-002"));

    // Explicitly verify the clean state of ALL THREE indexes
    TEST_CHECK(mgr.findProduct("SKU-002") == nullptr);
    TEST_CHECK(mgr.findProductOnDevice("SKU-002") == nullptr);
    const std::vector<std::string> expectedClean{"SKU-001"};
    TEST_CHECK(mgr.getProductIdsInOrder() == expectedClean);
    TEST_CHECK(mgr.getProductCount() == 1);
    TEST_CHECK(mgr.getDeviceProductCount() == 1);

    // Verify that adding p2 now succeeds cleanly and all three tiers synchronize
    TEST_CHECK(mgr.addProduct(p2));
    TEST_CHECK(mgr.getProductCount() == 2);
    TEST_CHECK(mgr.getDeviceProductCount() == 2);
    TEST_CHECK(mgr.findProduct("SKU-002") != nullptr);
    TEST_CHECK(mgr.findProductOnDevice("SKU-002") != nullptr);
    TEST_CHECK(mgr.findProduct("SKU-002") == mgr.findProductOnDevice("SKU-002"));
    const std::vector<std::string> expectedFinal{"SKU-001", "SKU-002"};
    TEST_CHECK(mgr.getProductIdsInOrder() == expectedFinal);
}

// ---------------------------------------------------------------------------
// Test T18 (Fix 7): removeProduct consistency when invariant is violated
// ---------------------------------------------------------------------------
void testRemoveProductInvariantDefensive()
{
    InventoryManager mgr(10, 16);
    Product p1("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    mgr.addProduct(p1);

    // Remove missing key returns false
    TEST_CHECK(!mgr.removeProduct("NON-EXISTENT"));
    TEST_CHECK(mgr.getProductCount() == 1);
    TEST_CHECK(mgr.getDeviceProductCount() == 1);
    TEST_CHECK(mgr.findProduct("SKU-001") != nullptr);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    TEST_RUN(testEmptyInventory);
    TEST_RUN(testAddAndFind);
    TEST_RUN(testOrderedIndexAndSynchronization);
    TEST_RUN(testAddDuplicate);
    TEST_RUN(testRemoveProduct);
    TEST_RUN(testRemoveMissing);
    TEST_RUN(testUpdateQuantity);
    TEST_RUN(testUpdateMissing);
    TEST_RUN(testDisplay);
    TEST_RUN(testThreeWayAddSynchronization);
    TEST_RUN(testThreeWayRemoveSynchronization);
    TEST_RUN(testQuantityUpdateReflectedOnDevice);
    TEST_RUN(testRollbackOnDeviceIndexFailure);
    TEST_RUN(testRollbackOnBstFailure);
    TEST_RUN(testRemoveProductInvariantDefensive);
    TEST_REPORT();
}
