#include "test_utils.h"
#include "inventory/InventoryManager.h"
#include "inventory/Product.h"
#include "inventory/SlottingEngine.h"

#include <cmath>
#include <string>
#include <vector>

namespace
{
bool approxEqual(double a, double b, double eps = 1e-6)
{
    return std::abs(a - b) <= eps;
}

// ---------------------------------------------------------------------------
// Test: Empty inventory slotting
// ---------------------------------------------------------------------------
void testEmptyInventorySlotting()
{
    // Direct engine call
    std::vector<const Product*> emptyList;
    SlottingReport report = SlottingEngine::analyze(emptyList);
    TEST_CHECK(report.totalProducts == 0);
    TEST_CHECK(report.misSlottedCount == 0);
    TEST_CHECK(approxEqual(report.totalInventoryValue, 0.0));
    TEST_CHECK(report.recommendations.empty());

    // Through InventoryManager
    InventoryManager mgr(10);
    SlottingReport mgrReport = mgr.analyzeSlotting();
    TEST_CHECK(mgrReport.totalProducts == 0);
    TEST_CHECK(mgrReport.recommendations.empty());
}

// ---------------------------------------------------------------------------
// Test: Single product slotting
// ---------------------------------------------------------------------------
void testSingleProductSlotting()
{
    Product p("SKU-SOLO", "Solo Item", "Tools", 5, 20.0, "WH-1", "ZONE_A-01");
    std::vector<const Product*> list{&p};

    SlottingReport report = SlottingEngine::analyze(list);
    TEST_CHECK(report.totalProducts == 1);
    TEST_CHECK(approxEqual(report.totalInventoryValue, 100.0));
    TEST_CHECK(report.countClassA == 1);
    TEST_CHECK(report.countClassB == 0);
    TEST_CHECK(report.countClassC == 0);
    TEST_CHECK(report.misSlottedCount == 0);

    const auto& rec = report.recommendations[0];
    TEST_CHECK(rec.productId == "SKU-SOLO");
    TEST_CHECK(rec.classification == ABCClass::A);
    TEST_CHECK(approxEqual(rec.inventoryValue, 100.0));
    TEST_CHECK(approxEqual(rec.valuePercentage, 100.0));
    TEST_CHECK(approxEqual(rec.cumulativePercentage, 100.0));
    TEST_CHECK(!rec.isMisSlotted);
}

// ---------------------------------------------------------------------------
// Test: Standard ABC Pareto value distribution
// ---------------------------------------------------------------------------
void testABCParetoValueClassification()
{
    // Total value = 1000 + 500 + 250 + 150 + 100 = $2000
    // P1: $1000 (50.0%) -> Class A (prev: 0% < 70%)
    // P2: $500  (25.0%) -> Class A (prev: 50% < 70%, cum: 75%)
    // P3: $250  (12.5%) -> Class B (prev: 75% < 90%, cum: 87.5%)
    // P4: $150  (7.5%)  -> Class B (prev: 87.5% < 90%, cum: 95%)
    // P5: $100  (5.0%)  -> Class C (prev: 95% >= 90%, cum: 100%)
    Product p1("SKU-1", "Item 1", "Cat", 10, 100.0, "WH-1", "ZONE_A-01");
    Product p2("SKU-2", "Item 2", "Cat", 5, 100.0, "WH-1", "ZONE_A-02");
    Product p3("SKU-3", "Item 3", "Cat", 10, 25.0, "WH-1", "ZONE_B-01");
    Product p4("SKU-4", "Item 4", "Cat", 10, 15.0, "WH-1", "ZONE_B-02");
    Product p5("SKU-5", "Item 5", "Cat", 10, 10.0, "WH-1", "ZONE_C-01");

    std::vector<const Product*> products{&p4, &p1, &p5, &p2, &p3}; // Unsorted input

    SlottingReport report = SlottingEngine::analyze(products);

    TEST_CHECK(report.totalProducts == 5);
    TEST_CHECK(approxEqual(report.totalInventoryValue, 2000.0));
    TEST_CHECK(report.countClassA == 2);
    TEST_CHECK(report.countClassB == 2);
    TEST_CHECK(report.countClassC == 1);
    TEST_CHECK(report.misSlottedCount == 0); // All in matching zones

    // Verify sorted descending order
    TEST_CHECK(report.recommendations[0].productId == "SKU-1");
    TEST_CHECK(report.recommendations[0].classification == ABCClass::A);
    TEST_CHECK(approxEqual(report.recommendations[0].inventoryValue, 1000.0));
    TEST_CHECK(approxEqual(report.recommendations[0].valuePercentage, 50.0));

    TEST_CHECK(report.recommendations[1].productId == "SKU-2");
    TEST_CHECK(report.recommendations[1].classification == ABCClass::A);
    TEST_CHECK(approxEqual(report.recommendations[1].inventoryValue, 500.0));
    TEST_CHECK(approxEqual(report.recommendations[1].valuePercentage, 25.0));

    TEST_CHECK(report.recommendations[2].productId == "SKU-3");
    TEST_CHECK(report.recommendations[2].classification == ABCClass::B);
    TEST_CHECK(approxEqual(report.recommendations[2].inventoryValue, 250.0));

    TEST_CHECK(report.recommendations[3].productId == "SKU-4");
    TEST_CHECK(report.recommendations[3].classification == ABCClass::B);
    TEST_CHECK(approxEqual(report.recommendations[3].inventoryValue, 150.0));

    TEST_CHECK(report.recommendations[4].productId == "SKU-5");
    TEST_CHECK(report.recommendations[4].classification == ABCClass::C);
    TEST_CHECK(approxEqual(report.recommendations[4].inventoryValue, 100.0));
}

// ---------------------------------------------------------------------------
// Test: Deterministic tie handling (equal values broken by SKU ascending)
// ---------------------------------------------------------------------------
void testDeterministicTieHandling()
{
    Product pZ("SKU-Z", "Zebra", "Cat", 10, 50.0, "WH-1", "ZONE_A-1"); // $500
    Product pA("SKU-A", "Apple", "Cat", 5, 100.0, "WH-1", "ZONE_A-2"); // $500
    Product pM("SKU-M", "Mango", "Cat", 20, 25.0, "WH-1", "ZONE_A-3"); // $500

    std::vector<const Product*> products{&pZ, &pM, &pA};
    SlottingReport report = SlottingEngine::analyze(products);

    TEST_CHECK(report.totalProducts == 3);
    // Equal values must be sorted lexicographically: SKU-A, SKU-M, SKU-Z
    TEST_CHECK(report.recommendations[0].productId == "SKU-A");
    TEST_CHECK(report.recommendations[1].productId == "SKU-M");
    TEST_CHECK(report.recommendations[2].productId == "SKU-Z");
}

// ---------------------------------------------------------------------------
// Test: Zero and negative value handling
// ---------------------------------------------------------------------------
void testZeroAndNegativeValueHandling()
{
    Product pZero1("SKU-Z1", "Zero Stock", "Cat", 0, 50.0, "WH-1", "ZONE_C-1");
    Product pZero2("SKU-Z2", "Free Sample", "Cat", 10, 0.0, "WH-1", "ZONE_C-2");
    Product pPos("SKU-POS", "Real Value", "Cat", 10, 10.0, "WH-1", "ZONE_A-1"); // $100

    std::vector<const Product*> products{&pZero1, &pPos, &pZero2};
    SlottingReport report = SlottingEngine::analyze(products);

    TEST_CHECK(report.totalProducts == 3);
    TEST_CHECK(approxEqual(report.totalInventoryValue, 100.0));
    TEST_CHECK(report.countClassA == 1);
    TEST_CHECK(report.countClassC == 2);

    // Positive product is Class A
    TEST_CHECK(report.recommendations[0].productId == "SKU-POS");
    TEST_CHECK(report.recommendations[0].classification == ABCClass::A);

    // Zero-value products are sorted after positive product and classified as Class C
    TEST_CHECK(report.recommendations[1].classification == ABCClass::C);
    TEST_CHECK(report.recommendations[2].classification == ABCClass::C);

    // All-zero inventory: all products classified as Class C
    std::vector<const Product*> allZero{&pZero1, &pZero2};
    SlottingReport reportZero = SlottingEngine::analyze(allZero);
    TEST_CHECK(reportZero.countClassC == 2);
    TEST_CHECK(approxEqual(reportZero.totalInventoryValue, 0.0));

    // Negative/non-finite inputs treated safely as 0.0 with notes
    Product pBad("SKU-BAD", "Bad Data", "Cat", -5, 20.0, "WH-1", "ZONE_C-3");
    std::vector<const Product*> badList{&pBad};
    SlottingReport reportBad = SlottingEngine::analyze(badList);
    TEST_CHECK(reportBad.recommendations[0].classification == ABCClass::C);
    TEST_CHECK(!reportBad.recommendations[0].notes.empty());
}

// ---------------------------------------------------------------------------
// Test: Configurable and validated thresholds
// ---------------------------------------------------------------------------
void testCustomThresholds()
{
    Product p1("SKU-1", "Item 1", "Cat", 10, 60.0, "WH-1", "ZONE_A-1"); // $600 (60%)
    Product p2("SKU-2", "Item 2", "Cat", 10, 30.0, "WH-1", "ZONE_B-1"); // $300 (30%)
    Product p3("SKU-3", "Item 3", "Cat", 10, 10.0, "WH-1", "ZONE_C-1"); // $100 (10%)
    std::vector<const Product*> products{&p1, &p2, &p3};

    // Under default 70/90:
    // P1 (60%): Class A (prev 0% < 70%)
    // P2 (30%): Class A (prev 60% < 70%) -> 2 Class A
    SlottingReport defaultReport = SlottingEngine::analyze(products);
    TEST_CHECK(defaultReport.countClassA == 2);
    TEST_CHECK(defaultReport.countClassB == 0);
    TEST_CHECK(defaultReport.countClassC == 1);

    // Custom tighter threshold: Class A cutoff at 50%, Class B at 85%
    ABCThresholds tightThresh;
    tightThresh.cumClassAPercent = 50.0;
    tightThresh.cumClassBPercent = 85.0;
    TEST_CHECK(tightThresh.isValid());

    // Under 50/85:
    // P1 (60%): Class A (prev 0% < 50%)
    // P2 (30%): Class B (prev 60% >= 50% and < 85%)
    // P3 (10%): Class C (prev 90% >= 85%)
    SlottingReport tightReport = SlottingEngine::analyze(products, tightThresh);
    TEST_CHECK(tightReport.countClassA == 1);
    TEST_CHECK(tightReport.countClassB == 1);
    TEST_CHECK(tightReport.countClassC == 1);

    // Invalid threshold (e.g. A >= B) safely falls back to default
    ABCThresholds invalidThresh;
    invalidThresh.cumClassAPercent = 95.0;
    invalidThresh.cumClassBPercent = 80.0;
    TEST_CHECK(!invalidThresh.isValid());
    SlottingReport fallbackReport = SlottingEngine::analyze(products, invalidThresh);
    TEST_CHECK(fallbackReport.countClassA == 2); // Falls back to default
}

// ---------------------------------------------------------------------------
// Test: Slotting recommendations and detection of mis-slotted products
// ---------------------------------------------------------------------------
void testMisSlottedDetection()
{
    // Class A items:
    // pA_good: placed in ZONE_A-01 -> MATCH
    // pA_bad: placed in ZONE_C-09 -> MIS-SLOTTED!
    Product pA_good("SKU-A1", "Good A", "Cat", 10, 100.0, "WH-1", "ZONE_A-01");
    Product pA_bad("SKU-A2", "Bad A", "Cat", 10, 100.0, "WH-1", "ZONE_C-09");

    // Class B item:
    // pB_good: placed in B-04-01 -> MATCH (alternate prefix B-)
    Product pB_good("SKU-B1", "Good B", "Cat", 10, 20.0, "WH-1", "B-04-01");

    // Class C item:
    // pC_bad: placed in ZONE_A-02 -> MIS-SLOTTED! (slow item in prime zone)
    Product pC_bad("SKU-C1", "Bad C", "Cat", 10, 5.0, "WH-1", "ZONE_A-02");

    std::vector<const Product*> products{&pA_good, &pA_bad, &pB_good, &pC_bad};
    SlottingReport report = SlottingEngine::analyze(products);

    TEST_CHECK(report.totalProducts == 4);
    TEST_CHECK(report.misSlottedCount == 2); // pA_bad and pC_bad

    for (const auto& rec : report.recommendations)
    {
        if (rec.productId == "SKU-A1")
        {
            TEST_CHECK(!rec.isMisSlotted);
            TEST_CHECK(rec.recommendedZone == "ZONE_A");
        }
        else if (rec.productId == "SKU-A2")
        {
            TEST_CHECK(rec.isMisSlotted);
            TEST_CHECK(rec.recommendedZone == "ZONE_A");
            TEST_CHECK(!rec.notes.empty());
        }
        else if (rec.productId == "SKU-B1")
        {
            TEST_CHECK(!rec.isMisSlotted);
            TEST_CHECK(rec.recommendedZone == "ZONE_B");
        }
        else if (rec.productId == "SKU-C1")
        {
            TEST_CHECK(rec.isMisSlotted);
            TEST_CHECK(rec.recommendedZone == "ZONE_C");
            TEST_CHECK(!rec.notes.empty());
        }
    }
}

// ---------------------------------------------------------------------------
// Test: Full integration with InventoryManager & three-index synchronization
// ---------------------------------------------------------------------------
void testInventoryManagerIntegration()
{
    InventoryManager mgr(50);

    Product p1("SKU-100", "High Value", "Electronics", 20, 250.0, "WH-1", "ZONE_A-01"); // $5000
    Product p2("SKU-200", "Mid Value", "Hardware", 50, 20.0, "WH-1", "ZONE_B-01");      // $1000
    Product p3("SKU-300", "Low Value", "Fasteners", 100, 2.0, "WH-1", "ZONE_C-01");     // $200

    TEST_CHECK(mgr.addProduct(p1));
    TEST_CHECK(mgr.addProduct(p2));
    TEST_CHECK(mgr.addProduct(p3));

    TEST_CHECK(mgr.getProductCount() == 3);
    TEST_CHECK(mgr.getDeviceProductCount() == 3);

    // 1. Verify read-only ordered enumeration matches BST in-order
    auto inOrderList = mgr.getAllProductsInOrder();
    TEST_CHECK(inOrderList.size() == 3);
    TEST_CHECK(inOrderList[0]->getProductId() == "SKU-100");
    TEST_CHECK(inOrderList[1]->getProductId() == "SKU-200");
    TEST_CHECK(inOrderList[2]->getProductId() == "SKU-300");

    // 2. Analyze slotting via InventoryManager
    SlottingReport report = mgr.analyzeSlotting();
    TEST_CHECK(report.totalProducts == 3);
    TEST_CHECK(approxEqual(report.totalInventoryValue, 6200.0));
    TEST_CHECK(report.countClassA == 1);
    TEST_CHECK(report.countClassB == 1);
    TEST_CHECK(report.countClassC == 1);
    TEST_CHECK(report.misSlottedCount == 0);

    // 3. Update quantity dynamically: verify analysis reflects updated state immediately
    // Increase SKU-300 quantity from 100 to 10,000 -> Value becomes $20,000 (now top item!)
    TEST_CHECK(mgr.updateProductQuantity("SKU-300", 10000));
    SlottingReport reportUpdated = mgr.analyzeSlotting();
    TEST_CHECK(approxEqual(reportUpdated.totalInventoryValue, 26000.0));
    // SKU-300 ($20,000 / $26,000 = ~76.9%) is now Class A!
    TEST_CHECK(reportUpdated.recommendations[0].productId == "SKU-300");
    TEST_CHECK(reportUpdated.recommendations[0].classification == ABCClass::A);
    // Since SKU-300 is currently in ZONE_C-01, it is now flagged as mis-slotted!
    TEST_CHECK(reportUpdated.recommendations[0].isMisSlotted);

    // 4. Test tree state serialization from InventoryManager
    std::string serializedIndex = mgr.serializeProductIndex();
    TEST_CHECK(!serializedIndex.empty());

    // 5. Remove product: verify clean unlinking across all 3 tiers and updated report
    TEST_CHECK(mgr.removeProduct("SKU-300"));
    TEST_CHECK(mgr.getProductCount() == 2);
    TEST_CHECK(mgr.getDeviceProductCount() == 2);
    SlottingReport reportAfterRemoval = mgr.analyzeSlotting();
    TEST_CHECK(reportAfterRemoval.totalProducts == 2);
    TEST_CHECK(reportAfterRemoval.misSlottedCount == 0);
}

// ---------------------------------------------------------------------------
// Test: Tiny positive inventory values below 1e-9 (Fix 2)
// ---------------------------------------------------------------------------
void testTinyPositiveInventoryValues()
{
    // 1. Single positive product whose inventory value is below 1e-9 (e.g. 1e-12)
    {
        Product p("SKU-TINY", "Nano Item", "Cat", 1, 1e-12, "WH-1", "ZONE_A-01");
        std::vector<const Product*> list{&p};

        SlottingReport report = SlottingEngine::analyze(list);
        TEST_CHECK(report.isValid);
        TEST_CHECK(static_cast<bool>(report));
        TEST_CHECK(report.errorMessage.empty());
        TEST_CHECK(report.totalProducts == 1);
        TEST_CHECK(report.totalInventoryValue > 0.0);
        TEST_CHECK(report.countClassA == 1);
        TEST_CHECK(report.countClassB == 0);
        TEST_CHECK(report.countClassC == 0);
        TEST_CHECK(report.misSlottedCount == 0);

        const auto& rec = report.recommendations[0];
        TEST_CHECK(rec.productId == "SKU-TINY");
        TEST_CHECK(rec.classification == ABCClass::A);
        TEST_CHECK(approxEqual(rec.valuePercentage, 100.0));
        TEST_CHECK(approxEqual(rec.cumulativePercentage, 100.0));
    }

    // 2. Multiple very small positive inventory values (< 1e-9 total)
    {
        // 5 products: total value = 2.0e-11 < 1e-9
        // P1: 1.0e-11 (50%) -> Class A (prev: 0% < 70%)
        // P2: 0.5e-11 (25%) -> Class A (prev: 50% < 70%)
        // P3: 0.25e-11 (12.5%) -> Class B (prev: 75% < 90%)
        // P4: 0.15e-11 (7.5%) -> Class B (prev: 87.5% < 90%)
        // P5: 0.10e-11 (5.0%) -> Class C (prev: 95% >= 90%)
        Product p1("SKU-1", "T1", "Cat", 1, 1.0e-11, "WH-1", "ZONE_A-1");
        Product p2("SKU-2", "T2", "Cat", 1, 0.5e-11, "WH-1", "ZONE_A-2");
        Product p3("SKU-3", "T3", "Cat", 1, 0.25e-11, "WH-1", "ZONE_B-1");
        Product p4("SKU-4", "T4", "Cat", 1, 0.15e-11, "WH-1", "ZONE_B-2");
        Product p5("SKU-5", "T5", "Cat", 1, 0.10e-11, "WH-1", "ZONE_C-1");

        std::vector<const Product*> products{&p3, &p1, &p5, &p2, &p4};
        SlottingReport report = SlottingEngine::analyze(products);

        TEST_CHECK(report.isValid);
        TEST_CHECK(static_cast<bool>(report));
        TEST_CHECK(report.errorMessage.empty());
        TEST_CHECK(report.totalProducts == 5);
        TEST_CHECK(report.countClassA == 2);
        TEST_CHECK(report.countClassB == 2);
        TEST_CHECK(report.countClassC == 1);
        TEST_CHECK(report.misSlottedCount == 0);

        TEST_CHECK(report.recommendations[0].productId == "SKU-1");
        TEST_CHECK(report.recommendations[0].classification == ABCClass::A);
        TEST_CHECK(report.recommendations[1].productId == "SKU-2");
        TEST_CHECK(report.recommendations[1].classification == ABCClass::A);
        TEST_CHECK(report.recommendations[2].productId == "SKU-3");
        TEST_CHECK(report.recommendations[2].classification == ABCClass::B);
        TEST_CHECK(report.recommendations[3].productId == "SKU-4");
        TEST_CHECK(report.recommendations[3].classification == ABCClass::B);
        TEST_CHECK(report.recommendations[4].productId == "SKU-5");
        TEST_CHECK(report.recommendations[4].classification == ABCClass::C);
    }

    // 3. Genuinely zero-valued inventory
    {
        Product pZ1("SKU-Z1", "Zero Stock", "Cat", 0, 100.0, "WH-1", "ZONE_C-1");
        Product pZ2("SKU-Z2", "Zero Price", "Cat", 50, 0.0, "WH-1", "ZONE_C-2");
        std::vector<const Product*> zeroList{&pZ1, &pZ2};

        SlottingReport report = SlottingEngine::analyze(zeroList);
        TEST_CHECK(report.isValid);
        TEST_CHECK(static_cast<bool>(report));
        TEST_CHECK(report.errorMessage.empty());
        TEST_CHECK(report.totalProducts == 2);
        TEST_CHECK(report.totalInventoryValue == 0.0);
        TEST_CHECK(report.countClassA == 0);
        TEST_CHECK(report.countClassB == 0);
        TEST_CHECK(report.countClassC == 2);
        TEST_CHECK(report.recommendations[0].classification == ABCClass::C);
        TEST_CHECK(report.recommendations[0].valuePercentage == 0.0);
        TEST_CHECK(report.recommendations[0].cumulativePercentage == 0.0);
        TEST_CHECK(report.recommendations[1].classification == ABCClass::C);
        TEST_CHECK(report.recommendations[1].valuePercentage == 0.0);
        TEST_CHECK(report.recommendations[1].cumulativePercentage == 0.0);
    }
}

// ---------------------------------------------------------------------------
// Test: Floating-point overflow handling (Fix 3 & Invalidation)
// ---------------------------------------------------------------------------
void testFloatingPointOverflowHandling()
{
    // 1. Multiplication overflow invalidates aggregate report
    {
        Product pOverflow("SKU-OVF", "Huge Item", "Cat", 10, 1e308, "WH-1", "ZONE_C-1");
        Product pNormal("SKU-NORM", "Normal Item", "Cat", 2, 50.0, "WH-1", "ZONE_A-1"); // $100

        std::vector<const Product*> products{&pOverflow, &pNormal};
        SlottingReport report = SlottingEngine::analyze(products);

        TEST_CHECK(!report.isValid);
        TEST_CHECK(!static_cast<bool>(report));
        TEST_CHECK(!report.errorMessage.empty());
        TEST_CHECK(report.errorMessage.find("overflow") != std::string::npos);
        // Does not return normal-looking class counts or imply complete classification
        TEST_CHECK(report.countClassA == 0);
        TEST_CHECK(report.countClassB == 0);
        TEST_CHECK(report.countClassC == 0);
        TEST_CHECK(report.misSlottedCount == 0);
        TEST_CHECK(report.recommendations.empty());
    }

    // 2. Accumulated total overflow invalidates aggregate report
    {
        Product p1("SKU-BIG1", "Big Item 1", "Cat", 1, 1e308, "WH-1", "ZONE_A-1");
        Product p2("SKU-BIG2", "Big Item 2", "Cat", 1, 1e308, "WH-1", "ZONE_C-1");

        std::vector<const Product*> products{&p1, &p2};
        SlottingReport report = SlottingEngine::analyze(products);

        TEST_CHECK(!report.isValid);
        TEST_CHECK(!static_cast<bool>(report));
        TEST_CHECK(!report.errorMessage.empty());
        TEST_CHECK(report.errorMessage.find("overflow") != std::string::npos);
        // Does not return normal-looking class counts or imply complete classification
        TEST_CHECK(report.countClassA == 0);
        TEST_CHECK(report.countClassB == 0);
        TEST_CHECK(report.countClassC == 0);
        TEST_CHECK(report.misSlottedCount == 0);
        TEST_CHECK(report.recommendations.empty());
    }

    // 3. Caller can detect invalid report via InventoryManager integration
    {
        InventoryManager mgr(10);
        Product pNormal("SKU-OK", "Normal", "Cat", 5, 20.0, "WH-1", "ZONE_A-1");
        Product pOverflow("SKU-OVF", "Huge", "Cat", 100, 1e308, "WH-1", "ZONE_C-1");

        TEST_CHECK(mgr.addProduct(pNormal));
        TEST_CHECK(mgr.addProduct(pOverflow));

        SlottingReport mgrReport = mgr.analyzeSlotting();
        TEST_CHECK(!mgrReport.isValid);
        TEST_CHECK(!static_cast<bool>(mgrReport));
        TEST_CHECK(!mgrReport.errorMessage.empty());
        TEST_CHECK(mgrReport.errorMessage.find("overflow") != std::string::npos);
        TEST_CHECK(mgrReport.recommendations.empty());
        TEST_CHECK(mgrReport.countClassA == 0);
        TEST_CHECK(mgrReport.countClassB == 0);
        TEST_CHECK(mgrReport.countClassC == 0);
    }
}

} // namespace

int main()
{
    TEST_RUN(testEmptyInventorySlotting);
    TEST_RUN(testSingleProductSlotting);
    TEST_RUN(testABCParetoValueClassification);
    TEST_RUN(testDeterministicTieHandling);
    TEST_RUN(testZeroAndNegativeValueHandling);
    TEST_RUN(testCustomThresholds);
    TEST_RUN(testMisSlottedDetection);
    TEST_RUN(testInventoryManagerIntegration);
    TEST_RUN(testTinyPositiveInventoryValues);
    TEST_RUN(testFloatingPointOverflowHandling);
    TEST_REPORT();
}
