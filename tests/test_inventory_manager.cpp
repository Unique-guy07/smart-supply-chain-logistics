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
    TEST_REPORT();
}
