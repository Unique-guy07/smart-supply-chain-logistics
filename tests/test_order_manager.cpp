#include "test_utils.h"
#include "order/OrderManager.h"
#include "inventory/InventoryManager.h"
#include "inventory/Product.h"
#include <functional>
#include <string>

// Test accessor to inject failure hook for deterministic rollback testing
class OrderManagerTestAccessor
{
public:
    static void setPreMutationHook(OrderManager& om, std::function<void(const std::string&)> hook)
    {
        om.preMutationHook = std::move(hook);
    }
};

// ---------------------------------------------------------------------------
// Helper: seed inventory manager with catalog items
// Product params: (id, name, category, quantity, price, warehouseId, binLocation)
// ---------------------------------------------------------------------------
static void seedTestInventory(InventoryManager& im)
{
    im.addProduct(Product("SKU-001", "Widget A", "Hardware", 100, 10.0, "WH-1", "Aisle 1"));
    im.addProduct(Product("SKU-002", "Widget B", "Hardware", 50, 25.50, "WH-1", "Aisle 2"));
    im.addProduct(Product("SKU-003", "Widget C", "Hardware", 20, 5.0, "WH-1", "Aisle 3"));
}

// ---------------------------------------------------------------------------
// T1: Order Submission Validation & Rejection
// ---------------------------------------------------------------------------
void testSubmissionValidation()
{
    InventoryManager im(50);
    seedTestInventory(im);
    OrderManager om(im);

    // Empty order ID rejected
    Order ordEmptyId("", "CUST-1");
    ordEmptyId.addItem("SKU-001", 1);
    TEST_CHECK(!om.submitOrder(ordEmptyId));

    // Empty customer ID rejected
    Order ordEmptyCust("ORD-001", "");
    ordEmptyCust.addItem("SKU-001", 1);
    TEST_CHECK(!om.submitOrder(ordEmptyCust));

    // Empty items rejected
    Order ordNoItems("ORD-002", "CUST-1");
    TEST_CHECK(!om.submitOrder(ordNoItems));

    // Missing SKU rejected
    Order ordMissingSku("ORD-003", "CUST-1");
    ordMissingSku.addItem("NONEXISTENT-SKU", 1);
    TEST_CHECK(!om.submitOrder(ordMissingSku));

    // Valid order accepted
    Order validOrd("ORD-004", "CUST-1", OrderPriority::High);
    validOrd.addItem("SKU-001", 2);
    TEST_CHECK(om.submitOrder(validOrd));
    TEST_CHECK(om.getPendingCount() == 1);
    TEST_CHECK(om.getTotalOrderCount() == 1);

    // Duplicate order ID rejected
    Order duplicateOrd("ORD-004", "CUST-2");
    duplicateOrd.addItem("SKU-001", 1);
    TEST_CHECK(!om.submitOrder(duplicateOrd));
    TEST_CHECK(om.getPendingCount() == 1);
    TEST_CHECK(om.getTotalOrderCount() == 1);
}

// ---------------------------------------------------------------------------
// T2: Catalog Price Snapshots & Total Calculation
// ---------------------------------------------------------------------------
void testCatalogPriceSnapshot()
{
    InventoryManager im(50);
    seedTestInventory(im);
    OrderManager om(im);

    Order ord("ORD-PRICING", "CUST-1");
    ord.addItem("SKU-001", 3); // 3 * 10.0 = 30.0
    ord.addItem("SKU-002", 2); // 2 * 25.50 = 51.0
    TEST_CHECK(om.submitOrder(ord));

    const Order* stored = om.getOrder("ORD-PRICING");
    TEST_CHECK(stored != nullptr);
    TEST_CHECK(stored->getItems().size() == 2);
    TEST_CHECK(stored->getItems()[0].unitPriceSnapshot == 10.0);
    TEST_CHECK(stored->getItems()[1].unitPriceSnapshot == 25.50);
    TEST_CHECK(stored->getTotalPrice() == 81.0);
    TEST_CHECK(stored->isSubmitted());
}

// ---------------------------------------------------------------------------
// T3: Repeated SKU Cumulative Demand & Overselling Prevention
// ---------------------------------------------------------------------------
void testRepeatedSkuDemandAndOverselling()
{
    InventoryManager im(50);
    im.addProduct(Product("ITEM-X", "Limited Item", "Gadget", 10, 10.0, "WH-1", "Bin 5"));
    OrderManager om(im);

    // Order with repeated lines: line 1 = 6, line 2 = 5 -> total demand = 11 > 10
    Order oversellOrd("ORD-OVERSELL", "CUST-X");
    oversellOrd.addItem("ITEM-X", 6);
    oversellOrd.addItem("ITEM-X", 5);
    TEST_CHECK(om.submitOrder(oversellOrd));

    // Process order: preflight must detect total demand 11 > 10 and fail without touching stock
    TEST_CHECK(om.processNextOrder());
    const Order* stored = om.getOrder("ORD-OVERSELL");
    TEST_CHECK(stored != nullptr);
    TEST_CHECK(stored->getStatus() == OrderStatus::Failed);
    TEST_CHECK(stored->getFailureReason().find("Insufficient stock") != std::string::npos);

    // Verify inventory stock was unchanged (still 10)
    Product* p = im.findProduct("ITEM-X");
    TEST_CHECK(p != nullptr);
    TEST_CHECK(p->getQuantity() == 10);

    // Successful cumulative order within bounds: line 1 = 4, line 2 = 5 -> total 9 <= 10
    Order validRepeat("ORD-VALID-REPEAT", "CUST-Y");
    validRepeat.addItem("ITEM-X", 4);
    validRepeat.addItem("ITEM-X", 5);
    TEST_CHECK(om.submitOrder(validRepeat));

    TEST_CHECK(om.processNextOrder());
    const Order* storedValid = om.getOrder("ORD-VALID-REPEAT");
    TEST_CHECK(storedValid != nullptr);
    TEST_CHECK(storedValid->getStatus() == OrderStatus::Completed);
    TEST_CHECK(p->getQuantity() == 1); // 10 - 9 = 1
}

// ---------------------------------------------------------------------------
// T4: Priority Precedence & Intra-lane FIFO Ordering
// ---------------------------------------------------------------------------
void testPriorityPrecedenceAndFIFO()
{
    InventoryManager im(50);
    seedTestInventory(im);
    OrderManager om(im);

    // Submit in mixed order across 4 priority lanes
    // Urgent: U1, U2
    // High: H1, H2
    // Normal: N1, N2
    // Low: L1, L2
    Order n1("N1", "C", OrderPriority::Normal); n1.addItem("SKU-001", 1); om.submitOrder(n1);
    Order u1("U1", "C", OrderPriority::Urgent); u1.addItem("SKU-001", 1); om.submitOrder(u1);
    Order l1("L1", "C", OrderPriority::Low);    l1.addItem("SKU-001", 1); om.submitOrder(l1);
    Order h1("H1", "C", OrderPriority::High);   h1.addItem("SKU-001", 1); om.submitOrder(h1);
    Order u2("U2", "C", OrderPriority::Urgent); u2.addItem("SKU-001", 1); om.submitOrder(u2);
    Order h2("H2", "C", OrderPriority::High);   h2.addItem("SKU-001", 1); om.submitOrder(h2);
    Order n2("N2", "C", OrderPriority::Normal); n2.addItem("SKU-001", 1); om.submitOrder(n2);
    Order l2("L2", "C", OrderPriority::Low);    l2.addItem("SKU-001", 1); om.submitOrder(l2);

    TEST_CHECK(om.getPendingCount() == 8);

    // Expected processing sequence:
    // U1 -> U2 -> H1 -> H2 -> N1 -> N2 -> L1 -> L2
    std::vector<std::string> expected = {"U1", "U2", "H1", "H2", "N1", "N2", "L1", "L2"};

    for (const auto& expectedId : expected) {
        TEST_CHECK(om.processNextOrder());
        const Order* ord = om.getOrder(expectedId);
        TEST_CHECK(ord != nullptr);
        TEST_CHECK(ord->getStatus() == OrderStatus::Completed);
    }

    TEST_CHECK(om.getPendingCount() == 0);
    TEST_CHECK(om.getCompletedCount() == 8);
}

// ---------------------------------------------------------------------------
// T5: Lazy Cancellation & Skipping Cancelled Orders
// ---------------------------------------------------------------------------
void testLazyCancellation()
{
    InventoryManager im(50);
    seedTestInventory(im);
    OrderManager om(im);

    Order o1("O1", "C", OrderPriority::Normal); o1.addItem("SKU-001", 2); om.submitOrder(o1);
    Order o2("O2", "C", OrderPriority::Normal); o2.addItem("SKU-001", 2); om.submitOrder(o2);
    Order o3("O3", "C", OrderPriority::Normal); o3.addItem("SKU-001", 2); om.submitOrder(o3);

    // Cancel O2 while pending
    TEST_CHECK(om.cancelOrder("O2"));
    TEST_CHECK(om.getCancelledCount() == 1);
    TEST_CHECK(om.getOrder("O2")->getStatus() == OrderStatus::Cancelled);

    // Cannot cancel again
    TEST_CHECK(!om.cancelOrder("O2"));

    // Cannot cancel non-existent order
    TEST_CHECK(!om.cancelOrder("NONEXISTENT"));

    // Process next: O1 processed
    TEST_CHECK(om.processNextOrder());
    TEST_CHECK(om.getOrder("O1")->getStatus() == OrderStatus::Completed);

    // Process next: O2 is dequeued, recognized as Cancelled, skipped, and O3 is processed
    TEST_CHECK(om.processNextOrder());
    TEST_CHECK(om.getOrder("O3")->getStatus() == OrderStatus::Completed);

    // No more orders
    TEST_CHECK(!om.processNextOrder());
    TEST_CHECK(om.getPendingCount() == 0);
    TEST_CHECK(om.getCompletedCount() == 2);
    TEST_CHECK(om.getCancelledCount() == 1);
}

// ---------------------------------------------------------------------------
// T6: Deterministic Mid-Transaction Failure & Compensating Rollback
// ---------------------------------------------------------------------------
void testDeterministicCompensatingRollback()
{
    InventoryManager im(50);
    im.addProduct(Product("SKU-A", "Alpha", "Type", 10, 10.0, "WH-1", "A"));
    im.addProduct(Product("SKU-B", "Beta",  "Type", 10, 20.0, "WH-1", "B"));

    OrderManager om(im);

    // Order requests 4 of SKU-A, then 5 of SKU-B (first-seen unique order: SKU-A, then SKU-B)
    Order ord("ORD-ROLLBACK", "CUST-R");
    ord.addItem("SKU-A", 4);
    ord.addItem("SKU-B", 5);
    TEST_CHECK(om.submitOrder(ord));

    // Install pre-mutation hook: when SKU-B is about to be updated, remove SKU-B from inventory
    // to force inventoryManager.updateProductQuantity("SKU-B", ...) to fail!
    OrderManagerTestAccessor::setPreMutationHook(om, [&im](const std::string& sku) {
        if (sku == "SKU-B") {
            // Verify that SKU-A was ALREADY deducted to 6 before SKU-B fails
            Product* pA = im.findProduct("SKU-A");
            TEST_CHECK(pA != nullptr);
            TEST_CHECK(pA->getQuantity() == 6);

            // Sabotage SKU-B so its update fails
            im.removeProduct("SKU-B");
        }
    });

    // Process order: SKU-A deduction succeeds, SKU-B fails, compensation restores SKU-A to 10
    TEST_CHECK(om.processNextOrder());

    const Order* stored = om.getOrder("ORD-ROLLBACK");
    TEST_CHECK(stored != nullptr);
    TEST_CHECK(stored->getStatus() == OrderStatus::Failed);
    TEST_CHECK(stored->getFailureReason().find("prior deductions successfully rolled back") != std::string::npos);

    // Verify SKU-A stock was restored back to 10
    Product* pA = im.findProduct("SKU-A");
    TEST_CHECK(pA != nullptr);
    TEST_CHECK(pA->getQuantity() == 10);
    TEST_CHECK(om.getFailedCount() == 1);
}

// ---------------------------------------------------------------------------
// T7: Exact Stock Depletion to Zero
// ---------------------------------------------------------------------------
void testExactStockDepletion()
{
    InventoryManager im(50);
    im.addProduct(Product("SKU-DEPLETE", "Deplete Me", "Type", 5, 15.0, "WH-1", "Shelf 1"));
    OrderManager om(im);

    Order ord("ORD-DEPLETE", "CUST-D");
    ord.addItem("SKU-DEPLETE", 5);
    TEST_CHECK(om.submitOrder(ord));

    TEST_CHECK(om.processNextOrder());
    const Order* stored = om.getOrder("ORD-DEPLETE");
    TEST_CHECK(stored != nullptr);
    TEST_CHECK(stored->getStatus() == OrderStatus::Completed);

    // Product remains in catalog but quantity is 0
    Product* p = im.findProduct("SKU-DEPLETE");
    TEST_CHECK(p != nullptr);
    TEST_CHECK(p->getQuantity() == 0);
}

// ---------------------------------------------------------------------------
// T8: Lifecycle Transition Restrictions
// ---------------------------------------------------------------------------
void testLifecycleStateRestrictions()
{
    InventoryManager im(50);
    seedTestInventory(im);
    OrderManager om(im);

    Order ord("ORD-LIFECYCLE", "CUST-L");
    ord.addItem("SKU-001", 1);
    TEST_CHECK(om.submitOrder(ord));

    // Process to completion
    TEST_CHECK(om.processNextOrder());
    TEST_CHECK(om.getOrder("ORD-LIFECYCLE")->getStatus() == OrderStatus::Completed);

    // Terminal state: completed order CANNOT be cancelled
    TEST_CHECK(!om.cancelOrder("ORD-LIFECYCLE"));
    TEST_CHECK(om.getOrder("ORD-LIFECYCLE")->getStatus() == OrderStatus::Completed);
}

int main()
{
    TEST_RUN(testSubmissionValidation);
    TEST_RUN(testCatalogPriceSnapshot);
    TEST_RUN(testRepeatedSkuDemandAndOverselling);
    TEST_RUN(testPriorityPrecedenceAndFIFO);
    TEST_RUN(testLazyCancellation);
    TEST_RUN(testDeterministicCompensatingRollback);
    TEST_RUN(testExactStockDepletion);
    TEST_RUN(testLifecycleStateRestrictions);
    TEST_REPORT();
}
