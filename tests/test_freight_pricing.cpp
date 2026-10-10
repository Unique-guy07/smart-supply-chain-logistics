#include "inventory/InventoryManager.h"
#include "order/FreightPricingEngine.h"
#include "order/Order.h"
#include "order/OrderManager.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace
{
void assertTrue(bool condition, const std::string& msg)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << msg << std::endl;
        std::exit(1);
    }
    std::cout << "PASS: " << msg << std::endl;
}

bool approxEqual(double a, double b, double eps = 1e-4)
{
    return std::abs(a - b) <= eps;
}

void testDefaultRatesAndMultipliers()
{
    std::cout << "\n--- testDefaultRatesAndMultipliers ---" << std::endl;
    FreightPricingEngine engine;

    // Default rates: base_fee=10, distance_rate=0.5, weight_rate=0.8
    // Default unset distance=0, weight=0
    // formula: max(base_fee, (base_fee + 0 + 0) * urgency_multiplier)

    // Normal: multiplier 1.0 -> max(10, 10 * 1.0) = 10.00
    auto rNormal = engine.calculateFreight(0.0, 0.0, OrderPriority::Normal);
    assertTrue(rNormal.success && approxEqual(rNormal.cost, 10.00), "Normal priority with default 0km/0kg = $10.00");

    // Low: multiplier 0.8 -> (10) * 0.8 = 8.0, max(10, 8.0) = 10.00 (floor applied)
    auto rLow = engine.calculateFreight(0.0, 0.0, OrderPriority::Low);
    assertTrue(rLow.success && approxEqual(rLow.cost, 10.00), "Low priority floor base_fee = $10.00");

    // High: multiplier 1.4 -> max(10, 10 * 1.4) = 14.00
    auto rHigh = engine.calculateFreight(0.0, 0.0, OrderPriority::High);
    assertTrue(rHigh.success && approxEqual(rHigh.cost, 14.00), "High priority with default 0km/0kg = $14.00");

    // Urgent: multiplier 2.0 -> max(10, 10 * 2.0) = 20.00
    auto rUrgent = engine.calculateFreight(0.0, 0.0, OrderPriority::Urgent);
    assertTrue(rUrgent.success && approxEqual(rUrgent.cost, 20.00), "Urgent priority with default 0km/0kg = $20.00");
}

void testDistanceAndWeightScaling()
{
    std::cout << "\n--- testDistanceAndWeightScaling ---" << std::endl;
    FreightPricingEngine engine;

    // distance = 50 km, weight = 20 kg
    // (10 + 50 * 0.5 + 20 * 0.8) = 10 + 25 + 16 = 51.0
    // Normal: 51.0 * 1.0 = 51.00
    auto rNormal = engine.calculateFreight(50.0, 20.0, OrderPriority::Normal);
    assertTrue(rNormal.success && approxEqual(rNormal.cost, 51.00), "Normal 50km, 20kg = $51.00");

    // Urgent: 51.0 * 2.0 = 102.00
    auto rUrgent = engine.calculateFreight(50.0, 20.0, OrderPriority::Urgent);
    assertTrue(rUrgent.success && approxEqual(rUrgent.cost, 102.00), "Urgent 50km, 20kg = $102.00");

    // High: 51.0 * 1.4 = 71.40
    auto rHigh = engine.calculateFreight(50.0, 20.0, OrderPriority::High);
    assertTrue(rHigh.success && approxEqual(rHigh.cost, 71.40), "High 50km, 20kg = $71.40");

    // Low: 51.0 * 0.8 = 40.80
    auto rLow = engine.calculateFreight(50.0, 20.0, OrderPriority::Low);
    assertTrue(rLow.success && approxEqual(rLow.cost, 40.80), "Low 50km, 20kg = $40.80");
}

void testRoundingBehavior()
{
    std::cout << "\n--- testRoundingBehavior ---" << std::endl;
    FreightPricingEngine engine;

    // Set distance_rate = 0.333, weight_rate = 0.0
    engine.setDistanceRate(0.333);
    engine.setWeightRate(0.0);

    // distance = 1 km, Normal priority -> 10 + 0.333 = 10.333 -> rounds half-up to 10.33
    auto r1 = engine.calculateFreight(1.0, 0.0, OrderPriority::Normal);
    assertTrue(r1.success && approxEqual(r1.cost, 10.33), "Half-up rounding down: 10.333 -> 10.33");

    // distance = 1.05 km -> 10 + 1.05 * 0.333 = 10 + 0.34965 = 10.34965 -> rounds to 10.35
    engine.setDistanceRate(0.335);
    // 10 + 0.335 = 10.335 -> rounds half-up to 10.34
    auto r2 = engine.calculateFreight(1.0, 0.0, OrderPriority::Normal);
    assertTrue(r2.success && approxEqual(r2.cost, 10.34), "Half-up rounding at midpoint: 10.335 -> 10.34");
}

void testInvalidInputsRejection()
{
    std::cout << "\n--- testInvalidInputsRejection ---" << std::endl;
    FreightPricingEngine engine;

    // Negative distance
    auto rNegDist = engine.calculateFreight(-1.0, 10.0, OrderPriority::Normal);
    assertTrue(!rNegDist.success, "Negative distance rejected");

    // Negative weight
    auto rNegWt = engine.calculateFreight(10.0, -5.0, OrderPriority::Normal);
    assertTrue(!rNegWt.success, "Negative weight rejected");

    // NaN and Inf
    double nanVal = std::numeric_limits<double>::quiet_NaN();
    double infVal = std::numeric_limits<double>::infinity();

    auto rNanDist = engine.calculateFreight(nanVal, 10.0, OrderPriority::Normal);
    assertTrue(!rNanDist.success, "NaN distance rejected");

    auto rInfWt = engine.calculateFreight(10.0, infVal, OrderPriority::Normal);
    assertTrue(!rInfWt.success, "Infinity weight rejected");

    // Over limit
    auto rOverDist = engine.calculateFreight(60000.0, 10.0, OrderPriority::Normal);
    assertTrue(!rOverDist.success, "Distance exceeding 50,000 km rejected");

    auto rOverWt = engine.calculateFreight(10.0, 150000.0, OrderPriority::Normal);
    assertTrue(!rOverWt.success, "Weight exceeding 100,000 kg rejected");

    // Order::setShippingParameters preserves existing state on rejection
    Order ord("ORD-TEST", "CUST-1");
    bool ok1 = ord.setShippingParameters(50.0, 20.0);
    assertTrue(ok1 && ord.getShippingDistance() == 50.0 && ord.getShippingWeight() == 20.0,
               "Valid shipping parameters accepted");

    bool ok2 = ord.setShippingParameters(-5.0, 10.0);
    assertTrue(!ok2 && ord.getShippingDistance() == 50.0 && ord.getShippingWeight() == 20.0,
               "Invalid distance rejected and previous valid configuration preserved");
}

void testFormulaUpdatesAndOrderSnapshotImmutability()
{
    std::cout << "\n--- testFormulaUpdatesAndOrderSnapshotImmutability ---" << std::endl;
    InventoryManager inv(50);
    inv.addProduct(Product("SKU-1", "Product 1", "Cat", 50, 100.0, "WH-1", "Bin-1"));

    OrderManager mgr(inv);

    // Create Order 1 with distance=50km, weight=20kg, Normal
    Order o1("ORD-1", "CUST-A", OrderPriority::Normal);
    o1.addItem("SKU-1", 1);
    o1.setShippingParameters(50.0, 20.0);

    bool sub1 = mgr.submitOrder(o1);
    assertTrue(sub1, "Order 1 submitted successfully");

    const Order* stored1 = mgr.getOrder("ORD-1");
    assertTrue(stored1 != nullptr, "Order 1 found in manager");
    assertTrue(approxEqual(stored1->getTotalPrice(), 100.00), "Merchandise subtotal is $100.00");
    assertTrue(approxEqual(stored1->getFreightCost(), 51.00), "Freight cost is $51.00");
    assertTrue(approxEqual(stored1->getGrandTotal(), 151.00), "Grand total is $151.00 ($100 + $51)");

    // Now update pricing formula on OrderManager
    // New formula: base_fee + distance * distance_rate (ignoring weight)
    bool setFormOk = mgr.getPricingEngine().setFormula("base_fee + distance * distance_rate");
    assertTrue(setFormOk, "Updated pricing formula successfully");

    // Verify stored Order 1 STILL has original snapshotted freight cost ($51.00)!
    assertTrue(approxEqual(stored1->getFreightCost(), 51.00),
               "Historical Order 1 retains snapshotted freight cost after formula change");
    assertTrue(approxEqual(stored1->getGrandTotal(), 151.00),
               "Historical Order 1 retains grand total after formula change");

    // Submit Order 2 with identical shipping parameters
    Order o2("ORD-2", "CUST-B", OrderPriority::Normal);
    o2.addItem("SKU-1", 1);
    o2.setShippingParameters(50.0, 20.0);

    bool sub2 = mgr.submitOrder(o2);
    assertTrue(sub2, "Order 2 submitted successfully under new formula");

    const Order* stored2 = mgr.getOrder("ORD-2");
    assertTrue(stored2 != nullptr, "Order 2 found in manager");
    // Under new formula: 10 + 50 * 0.5 = 35.00
    assertTrue(approxEqual(stored2->getFreightCost(), 35.00),
               "New Order 2 receives new formula freight cost ($35.00)");
    assertTrue(approxEqual(stored2->getGrandTotal(), 135.00),
               "New Order 2 grand total reflects new freight cost ($100 + $35 = $135)");
}

void testFailedPricingLeavesSystemClean()
{
    std::cout << "\n--- testFailedPricingLeavesSystemClean ---" << std::endl;
    InventoryManager inv(50);
    inv.addProduct(Product("SKU-SAFE", "Safe Product", "Cat", 10, 50.0, "WH-1", "Bin-2"));

    OrderManager mgr(inv);

    // Formula is valid on generic test variables (e.g. distance = 1.0 -> 10 + 1/(1-10) = 9.88)
    // but triggers division by zero when an order has distance == 10.0
    bool setFormOk = mgr.getPricingEngine().setFormula("base_fee + weight / (distance - 10.0)");
    assertTrue(setFormOk, "Formula with potential conditional zero-divisor accepted");

    Order badOrder("ORD-BAD", "CUST-X", OrderPriority::Normal);
    badOrder.addItem("SKU-SAFE", 2);
    badOrder.setShippingParameters(10.0, 5.0); // distance = 10.0 triggers (distance - 10.0) == 0

    bool subRes = mgr.submitOrder(badOrder);
    assertTrue(!subRes, "Order submission rejected when freight pricing fails");

    // Verify system state: 0 registered orders, stock untouched
    assertTrue(mgr.getTotalOrderCount() == 0, "No orders registered in manager");
    assertTrue(!mgr.hasPendingOrders(), "No pending orders queued");

    Product* p = inv.findProduct("SKU-SAFE");
    assertTrue(p != nullptr && p->getQuantity() == 10, "Inventory stock completely untouched (still 10)");
}

void testBackwardCompatibilityWithUnsetShipping()
{
    std::cout << "\n--- testBackwardCompatibilityWithUnsetShipping ---" << std::endl;
    InventoryManager inv(50);
    inv.addProduct(Product("SKU-LEGACY", "Legacy Product", "Cat", 20, 25.0, "WH-1", "Bin-3"));

    OrderManager mgr(inv);

    // Order created without calling setShippingParameters (unset defaults to 0km, 0kg)
    Order legacyOrder("ORD-LEGACY", "CUST-LEG", OrderPriority::Normal);
    legacyOrder.addItem("SKU-LEGACY", 2); // merchandise total = 50.00

    bool subRes = mgr.submitOrder(legacyOrder);
    assertTrue(subRes, "Legacy order with unset shipping accepted seamlessly");

    const Order* stored = mgr.getOrder("ORD-LEGACY");
    assertTrue(stored != nullptr, "Legacy order found in manager");
    assertTrue(approxEqual(stored->getTotalPrice(), 50.00), "Merchandise total is strictly catalog total ($50.00)");
    assertTrue(approxEqual(stored->getFreightCost(), 10.00), "Default freight cost is base fee ($10.00)");
    assertTrue(approxEqual(stored->getGrandTotal(), 60.00), "Grand total is $60.00 ($50 + $10)");
}

void testFreightCeilingBoundaries()
{
    std::cout << "\n--- testFreightCeilingBoundaries ---" << std::endl;
    FreightPricingEngine engine;

    // 1. Exactly at ceiling: set base_fee to MAX_FREIGHT_CEILING with 0 distance and 0 weight
    bool setBaseOk = engine.setBaseFee(FreightPricingEngine::MAX_FREIGHT_CEILING);
    assertTrue(setBaseOk, "Base fee can be set exactly to MAX_FREIGHT_CEILING");

    auto rAtCeiling = engine.calculateFreight(0.0, 0.0, OrderPriority::Normal);
    assertTrue(rAtCeiling.success && approxEqual(rAtCeiling.cost, FreightPricingEngine::MAX_FREIGHT_CEILING),
               "Freight calculation exactly at ceiling is accepted");

    // 2. Above ceiling: base_fee at ceiling plus distance freight exceeds ceiling
    auto rAboveCeiling = engine.calculateFreight(1.0, 0.0, OrderPriority::Normal);
    assertTrue(!rAboveCeiling.success, "Freight calculation exceeding ceiling is rejected");

    // 3. Setting base fee above ceiling is rejected
    bool setBaseAbove = engine.setBaseFee(FreightPricingEngine::MAX_FREIGHT_CEILING + 0.01);
    assertTrue(!setBaseAbove, "Setting base fee above MAX_FREIGHT_CEILING is rejected");

    // 4. Half-up rounding boundary at ceiling:
    // With custom formula returning MAX_FREIGHT_CEILING - 0.004, it rounds half-up to MAX_FREIGHT_CEILING and is accepted.
    FreightPricingEngine engineRounding;
    bool setFormOk1 = engineRounding.setFormula("base_fee - 0.004");
    assertTrue(setFormOk1, "Formula for half-up rounding to ceiling configured");
    engineRounding.setBaseFee(FreightPricingEngine::MAX_FREIGHT_CEILING);
    auto rRoundToCeiling = engineRounding.calculateFreight(0.0, 0.0, OrderPriority::Normal);
    assertTrue(rRoundToCeiling.success && approxEqual(rRoundToCeiling.cost, FreightPricingEngine::MAX_FREIGHT_CEILING),
               "Freight calculation rounding to exactly ceiling is accepted");

    // With custom formula returning MAX_FREIGHT_CEILING + 0.001, it exceeds ceiling and is rejected.
    FreightPricingEngine engineExceed;
    bool setFormOk2 = engineExceed.setFormula("base_fee + 0.001");
    assertTrue(setFormOk2, "Formula for exceeding ceiling configured");
    engineExceed.setBaseFee(FreightPricingEngine::MAX_FREIGHT_CEILING);
    auto rRoundExceed = engineExceed.calculateFreight(0.0, 0.0, OrderPriority::Normal);
    assertTrue(!rRoundExceed.success, "Freight calculation exceeding ceiling by 0.001 is rejected");
}
} // namespace

int main()
{
    std::cout << "Running test_freight_pricing..." << std::endl;
    testDefaultRatesAndMultipliers();
    testDistanceAndWeightScaling();
    testRoundingBehavior();
    testInvalidInputsRejection();
    testFormulaUpdatesAndOrderSnapshotImmutability();
    testFailedPricingLeavesSystemClean();
    testBackwardCompatibilityWithUnsetShipping();
    testFreightCeilingBoundaries();
    std::cout << "\nAll freight pricing tests passed successfully!" << std::endl;
    return 0;
}
