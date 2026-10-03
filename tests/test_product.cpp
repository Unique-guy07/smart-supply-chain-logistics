#include "test_utils.h"
#include "inventory/Product.h"

// ---------------------------------------------------------------------------
// Test: Default construction
// ---------------------------------------------------------------------------
void testDefaultConstruction()
{
    Product p;
    TEST_CHECK(p.getProductId().empty());
    TEST_CHECK(p.getName().empty());
    TEST_CHECK(p.getCategory().empty());
    TEST_CHECK(p.getQuantity() == 0);
    TEST_CHECK(p.getPrice() == 0.0);
    TEST_CHECK(p.getWarehouseId().empty());
    TEST_CHECK(p.getBinLocation().empty());
}

// ---------------------------------------------------------------------------
// Test: Parameterized construction
// ---------------------------------------------------------------------------
void testParameterizedConstruction()
{
    Product p("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");

    TEST_CHECK(p.getProductId() == "SKU-001");
    TEST_CHECK(p.getName() == "Widget");
    TEST_CHECK(p.getCategory() == "Electronics");
    TEST_CHECK(p.getQuantity() == 50);
    TEST_CHECK(p.getPrice() == 29.99);
    TEST_CHECK(p.getWarehouseId() == "WH-A");
    TEST_CHECK(p.getBinLocation() == "R3-B7");
}

// ---------------------------------------------------------------------------
// Test: display() runs without crash
// ---------------------------------------------------------------------------
void testDisplay()
{
    Product p("SKU-002", "Gadget", "Tools", 10, 14.50, "WH-B", "R1-B2");
    p.display();  // should not crash
    TEST_CHECK(true);  // if we reach here, display didn't crash

    Product empty;
    empty.display();  // default product display should also not crash
    TEST_CHECK(true);
}

// ---------------------------------------------------------------------------
// Test: operator== for equal products
// ---------------------------------------------------------------------------
void testEqualityEqual()
{
    Product a("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    Product b("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");

    TEST_CHECK(a == b);
    TEST_CHECK(!(a != b));
}

// ---------------------------------------------------------------------------
// Test: operator!= for differing products
// ---------------------------------------------------------------------------
void testEqualityDifferent()
{
    Product a("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    Product b("SKU-002", "Gadget", "Tools", 10, 14.50, "WH-B", "R1-B2");

    TEST_CHECK(a != b);
    TEST_CHECK(!(a == b));

    // Products differing in only one field
    Product c("SKU-001", "Widget", "Electronics", 51, 29.99, "WH-A", "R3-B7");
    TEST_CHECK(a != c);
}

// ---------------------------------------------------------------------------
// Test: Copy semantics
// ---------------------------------------------------------------------------
void testCopySemantics()
{
    Product original("SKU-001", "Widget", "Electronics", 50, 29.99, "WH-A", "R3-B7");
    Product copy = original;

    TEST_CHECK(copy == original);
    TEST_CHECK(copy.getProductId() == "SKU-001");
    TEST_CHECK(copy.getQuantity() == 50);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    TEST_RUN(testDefaultConstruction);
    TEST_RUN(testParameterizedConstruction);
    TEST_RUN(testDisplay);
    TEST_RUN(testEqualityEqual);
    TEST_RUN(testEqualityDifferent);
    TEST_RUN(testCopySemantics);
    TEST_REPORT();
}
