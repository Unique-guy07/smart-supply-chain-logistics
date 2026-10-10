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

    static void setNextSequenceNumber(OrderManager& om, std::uint64_t seq)
    {
        om.nextSequenceNumber = seq;
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

// ---------------------------------------------------------------------------
// T9: M4B DEPQ Highest-Priority Dispatch
// ---------------------------------------------------------------------------
void testHighestPriorityDepqDispatch()
{
    InventoryManager im(50);
    seedTestInventory(im);
    OrderManager om(im);

    Order oLow("ORD-LOW", "CUST-1", OrderPriority::Low);       oLow.addItem("SKU-001", 1);
    Order oNorm("ORD-NORM", "CUST-1", OrderPriority::Normal);  oNorm.addItem("SKU-001", 1);
    Order oHigh("ORD-HIGH", "CUST-1", OrderPriority::High);    oHigh.addItem("SKU-001", 1);
    Order oUrg("ORD-URG", "CUST-1", OrderPriority::Urgent);    oUrg.addItem("SKU-001", 1);

    om.submitOrder(oLow);
    om.submitOrder(oNorm);
    om.submitOrder(oHigh);
    om.submitOrder(oUrg);

    std::string peekId;
    TEST_CHECK(om.peekHighestPriorityOrder(peekId));
    TEST_CHECK(peekId == "ORD-URG");

    // Dispatch highest priority: Urgent
    TEST_CHECK(om.dispatchHighestPriorityOrder());
    TEST_CHECK(om.getOrder("ORD-URG")->getStatus() == OrderStatus::Completed);

    // Next highest is High
    TEST_CHECK(om.peekHighestPriorityOrder(peekId));
    TEST_CHECK(peekId == "ORD-HIGH");
    TEST_CHECK(om.dispatchHighestPriorityOrder());
    TEST_CHECK(om.getOrder("ORD-HIGH")->getStatus() == OrderStatus::Completed);

    // Next highest is Normal
    TEST_CHECK(om.peekHighestPriorityOrder(peekId));
    TEST_CHECK(peekId == "ORD-NORM");
    TEST_CHECK(om.dispatchHighestPriorityOrder());
    TEST_CHECK(om.getOrder("ORD-NORM")->getStatus() == OrderStatus::Completed);

    // Last is Low
    TEST_CHECK(om.peekHighestPriorityOrder(peekId));
    TEST_CHECK(peekId == "ORD-LOW");
    TEST_CHECK(om.dispatchHighestPriorityOrder());
    TEST_CHECK(om.getOrder("ORD-LOW")->getStatus() == OrderStatus::Completed);

    // No more pending orders
    TEST_CHECK(!om.peekHighestPriorityOrder(peekId));
    TEST_CHECK(!om.dispatchHighestPriorityOrder());
    TEST_CHECK(om.getPendingCount() == 0);
    TEST_CHECK(om.getCompletedCount() == 4);
}

// ---------------------------------------------------------------------------
// T10: M4B DEPQ Lowest-Priority Dispatch (Backfill / Economy)
// ---------------------------------------------------------------------------
void testLowestPriorityDepqDispatch()
{
    InventoryManager im(50);
    seedTestInventory(im);
    OrderManager om(im);

    Order oUrg("ORD-U", "CUST-1", OrderPriority::Urgent);  oUrg.addItem("SKU-001", 1);
    Order oHigh("ORD-H", "CUST-1", OrderPriority::High);   oHigh.addItem("SKU-001", 1);
    Order oNorm("ORD-N", "CUST-1", OrderPriority::Normal); oNorm.addItem("SKU-001", 1);
    Order oLow("ORD-L", "CUST-1", OrderPriority::Low);     oLow.addItem("SKU-001", 1);

    om.submitOrder(oUrg);
    om.submitOrder(oHigh);
    om.submitOrder(oNorm);
    om.submitOrder(oLow);

    std::string peekId;
    TEST_CHECK(om.peekLowestPriorityOrder(peekId));
    TEST_CHECK(peekId == "ORD-L");

    // Dispatch lowest priority: Low
    TEST_CHECK(om.dispatchLowestPriorityOrder());
    TEST_CHECK(om.getOrder("ORD-L")->getStatus() == OrderStatus::Completed);

    // Next lowest is Normal
    TEST_CHECK(om.peekLowestPriorityOrder(peekId));
    TEST_CHECK(peekId == "ORD-N");
    TEST_CHECK(om.dispatchLowestPriorityOrder());
    TEST_CHECK(om.getOrder("ORD-N")->getStatus() == OrderStatus::Completed);

    // Next lowest is High
    TEST_CHECK(om.peekLowestPriorityOrder(peekId));
    TEST_CHECK(peekId == "ORD-H");
    TEST_CHECK(om.dispatchLowestPriorityOrder());
    TEST_CHECK(om.getOrder("ORD-H")->getStatus() == OrderStatus::Completed);

    // Last is Urgent
    TEST_CHECK(om.peekLowestPriorityOrder(peekId));
    TEST_CHECK(peekId == "ORD-U");
    TEST_CHECK(om.dispatchLowestPriorityOrder());
    TEST_CHECK(om.getOrder("ORD-U")->getStatus() == OrderStatus::Completed);

    TEST_CHECK(!om.peekLowestPriorityOrder(peekId));
    TEST_CHECK(!om.dispatchLowestPriorityOrder());
}

// ---------------------------------------------------------------------------
// T11: Interleaved FIFO and DEPQ Dispatch Without Double-Fulfillment
// ---------------------------------------------------------------------------
void testInterleavingFifoAndDepqDispatch()
{
    InventoryManager im(50);
    seedTestInventory(im); // SKU-001 has quantity 100
    OrderManager om(im);

    Order u1("U1", "C", OrderPriority::Urgent); u1.addItem("SKU-001", 10);
    Order h1("H1", "C", OrderPriority::High);   h1.addItem("SKU-001", 10);
    Order n1("N1", "C", OrderPriority::Normal); n1.addItem("SKU-001", 10);
    Order l1("L1", "C", OrderPriority::Low);    l1.addItem("SKU-001", 10);

    om.submitOrder(u1);
    om.submitOrder(h1);
    om.submitOrder(n1);
    om.submitOrder(l1);

    // 1. Dispatch highest priority via DEPQ -> U1
    TEST_CHECK(om.dispatchHighestPriorityOrder());
    TEST_CHECK(om.getOrder("U1")->getStatus() == OrderStatus::Completed);
    TEST_CHECK(im.findProduct("SKU-001")->getQuantity() == 90);

    // 2. Dispatch next order via FIFO -> U1 is already Completed, so FIFO lazily skips U1 and processes H1!
    TEST_CHECK(om.processNextOrder());
    TEST_CHECK(om.getOrder("H1")->getStatus() == OrderStatus::Completed);
    TEST_CHECK(im.findProduct("SKU-001")->getQuantity() == 80);

    // 3. Dispatch lowest priority via DEPQ -> L1
    TEST_CHECK(om.dispatchLowestPriorityOrder());
    TEST_CHECK(om.getOrder("L1")->getStatus() == OrderStatus::Completed);
    TEST_CHECK(im.findProduct("SKU-001")->getQuantity() == 70);

    // 4. Dispatch next order via FIFO -> H1 is completed, so FIFO processes N1!
    TEST_CHECK(om.processNextOrder());
    TEST_CHECK(om.getOrder("N1")->getStatus() == OrderStatus::Completed);
    TEST_CHECK(im.findProduct("SKU-001")->getQuantity() == 60);

    // All orders are completed
    TEST_CHECK(!om.processNextOrder());
    TEST_CHECK(!om.dispatchHighestPriorityOrder());
    TEST_CHECK(!om.dispatchLowestPriorityOrder());
    TEST_CHECK(om.getPendingCount() == 0);
    TEST_CHECK(om.getCompletedCount() == 4);

    // Crucial invariant: Stock was deducted exactly 40 units (100 - 4*10 = 60). No double deductions!
    TEST_CHECK(im.findProduct("SKU-001")->getQuantity() == 60);
}

// ---------------------------------------------------------------------------
// T12: Cancellation and Stale Entries in Both Scheduling Paths
// ---------------------------------------------------------------------------
void testCancellationAndStaleEntriesInBothPaths()
{
    InventoryManager im(50);
    seedTestInventory(im);
    OrderManager om(im);

    Order o1("O1", "C", OrderPriority::Urgent); o1.addItem("SKU-001", 5);
    Order o2("O2", "C", OrderPriority::Urgent); o2.addItem("SKU-001", 5);
    Order o3("O3", "C", OrderPriority::Low);    o3.addItem("SKU-001", 5);

    om.submitOrder(o1);
    om.submitOrder(o2);
    om.submitOrder(o3);

    // Cancel O1
    TEST_CHECK(om.cancelOrder("O1"));
    TEST_CHECK(om.getOrder("O1")->getStatus() == OrderStatus::Cancelled);

    // DEPQ peek must lazily discard O1 and inspect O2
    std::string peekId;
    TEST_CHECK(om.peekHighestPriorityOrder(peekId));
    TEST_CHECK(peekId == "O2");

    // DEPQ dispatch must dispatch O2
    TEST_CHECK(om.dispatchHighestPriorityOrder());
    TEST_CHECK(om.getOrder("O2")->getStatus() == OrderStatus::Completed);

    // Cancel O3
    TEST_CHECK(om.cancelOrder("O3"));
    TEST_CHECK(om.getOrder("O3")->getStatus() == OrderStatus::Cancelled);

    // Now DEPQ min peek finds no eligible pending orders
    TEST_CHECK(!om.peekLowestPriorityOrder(peekId));
    TEST_CHECK(!om.dispatchLowestPriorityOrder());

    // FIFO process finds no eligible pending orders (O1 and O2 already visited/skipped)
    TEST_CHECK(!om.processNextOrder());
    TEST_CHECK(om.getPendingCount() == 0);
    TEST_CHECK(om.getCompletedCount() == 1);
    TEST_CHECK(om.getCancelledCount() == 2);
}

// ---------------------------------------------------------------------------
// T13: Tied Priorities Deterministic Ordering
// ---------------------------------------------------------------------------
void testTiedPrioritiesDeterministicOrdering()
{
    InventoryManager im(50);
    seedTestInventory(im);
    OrderManager om(im);

    // Submit 3 orders with the SAME priority (Normal)
    Order n1("N1", "C", OrderPriority::Normal); n1.addItem("SKU-001", 1);
    Order n2("N2", "C", OrderPriority::Normal); n2.addItem("SKU-001", 1);
    Order n3("N3", "C", OrderPriority::Normal); n3.addItem("SKU-001", 1);

    om.submitOrder(n1);
    om.submitOrder(n2);
    om.submitOrder(n3);

    // Deterministic tie-breaking: earlier sequence number (N1) comes out first in max-extraction
    std::string peekId;
    TEST_CHECK(om.peekHighestPriorityOrder(peekId));
    TEST_CHECK(peekId == "N1");
    TEST_CHECK(om.dispatchHighestPriorityOrder());
    TEST_CHECK(om.getOrder("N1")->getStatus() == OrderStatus::Completed);

    TEST_CHECK(om.peekHighestPriorityOrder(peekId));
    TEST_CHECK(peekId == "N2");
    TEST_CHECK(om.dispatchHighestPriorityOrder());
    TEST_CHECK(om.getOrder("N2")->getStatus() == OrderStatus::Completed);

    TEST_CHECK(om.peekHighestPriorityOrder(peekId));
    TEST_CHECK(peekId == "N3");
    TEST_CHECK(om.dispatchHighestPriorityOrder());
    TEST_CHECK(om.getOrder("N3")->getStatus() == OrderStatus::Completed);

    TEST_CHECK(!om.dispatchHighestPriorityOrder());
}

// ---------------------------------------------------------------------------
// T14: Sequence Counter Boundary & Wrap Prevention Policy
// ---------------------------------------------------------------------------
void testSequenceCounterBoundaryPolicy()
{
    InventoryManager im(50);
    seedTestInventory(im);
    OrderManager om(im);

    // 1. Submit Order 1 normally (Urgent priority)
    Order ord1("ORD-SEQ-001", "CUST-1", OrderPriority::Urgent);
    ord1.addItem("SKU-001", 1);
    TEST_CHECK(om.submitOrder(ord1));

    // 2. Set sequence counter to maximum boundary using test accessor
    OrderManagerTestAccessor::setNextSequenceNumber(om, std::numeric_limits<std::uint64_t>::max());

    // 3. Attempt to submit Order 2 while Order 1 is still Pending
    // Must be rejected: counter cannot wrap or reuse numbers while live entries exist
    Order ord2("ORD-SEQ-002", "CUST-2", OrderPriority::Urgent);
    ord2.addItem("SKU-002", 1);
    TEST_CHECK(!om.submitOrder(ord2));

    // Confirm ord1 is still pending and can be dispatched cleanly
    std::string highestId;
    TEST_CHECK(om.peekHighestPriorityOrder(highestId));
    TEST_CHECK(highestId == "ORD-SEQ-001");
    TEST_CHECK(om.dispatchHighestPriorityOrder());

    // Verify ord1 is completed, no live pending orders remain in om
    const Order* stored1 = om.getOrder("ORD-SEQ-001");
    TEST_CHECK(stored1 != nullptr);
    TEST_CHECK(stored1->getStatus() == OrderStatus::Completed);

    // 4. Now that no live pending orders remain, submitting ord2 should trigger safe reset
    TEST_CHECK(om.submitOrder(ord2));

    // Verify ord2 is scheduled, peekable, and dispatchable
    TEST_CHECK(om.peekHighestPriorityOrder(highestId));
    TEST_CHECK(highestId == "ORD-SEQ-002");
    TEST_CHECK(om.dispatchHighestPriorityOrder());

    const Order* stored2 = om.getOrder("ORD-SEQ-002");
    TEST_CHECK(stored2 != nullptr);
    TEST_CHECK(stored2->getStatus() == OrderStatus::Completed);
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
    TEST_RUN(testHighestPriorityDepqDispatch);
    TEST_RUN(testLowestPriorityDepqDispatch);
    TEST_RUN(testInterleavingFifoAndDepqDispatch);
    TEST_RUN(testCancellationAndStaleEntriesInBothPaths);
    TEST_RUN(testTiedPrioritiesDeterministicOrdering);
    TEST_RUN(testSequenceCounterBoundaryPolicy);
    TEST_REPORT();
}