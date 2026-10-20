#include "test_utils.h"
#include "integration/SupplyChainPlatform.h"
#include "inventory/InventoryManager.h"
#include "inventory/Product.h"
#include "order/Order.h"
#include "order/OrderManager.h"
#include "packing/CargoPacking.h"

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#define SMART_SUPPLY_CHAIN_NO_MAIN
#include "../src/main.cpp"
#undef SMART_SUPPLY_CHAIN_NO_MAIN

namespace integration {

/// Test accessor following the repository's M4 OrderManagerTestAccessor convention.
/// Grants private access to SupplyChainPlatform test seam for injecting intervening state changes.
class SupplyChainPlatformTestAccessor
{
public:
    static void setPreRecheckHook(SupplyChainPlatform& platform, std::function<void(const std::string&)> hook)
    {
        platform.m_preRecheckHook = std::move(hook);
    }
};

} // namespace integration

using namespace integration;

// ---------------------------------------------------------------------------
// Helper: Seed standard platform topology, products, and packaging profiles
// ---------------------------------------------------------------------------
static void seedStandardPlatform(SupplyChainPlatform& platform)
{
    // 1. Warehouse Topology
    auto& graph = platform.getWarehouseGraph();
    graph.addVertex("DEPOT");
    graph.addVertex("BIN_A1");
    graph.addVertex("BIN_B1");
    graph.addVertex("BIN_C1");
    graph.addVertex("PACK_STATION");
    graph.addVertex("DOCK");
    graph.addVertex("ISOLATED_BIN"); // Exists in graph but has no edges

    // Add navigation aisles
    graph.addBidirectionalAisle("DEPOT", "BIN_A1", 10.0);
    graph.addBidirectionalAisle("BIN_A1", "BIN_B1", 15.0);
    graph.addBidirectionalAisle("BIN_B1", "BIN_C1", 20.0);
    graph.addBidirectionalAisle("BIN_A1", "PACK_STATION", 12.0);
    graph.addBidirectionalAisle("BIN_B1", "PACK_STATION", 18.0);
    graph.addBidirectionalAisle("BIN_C1", "PACK_STATION", 25.0);
    graph.addBidirectionalAisle("PACK_STATION", "DOCK", 8.0);

    // 2. Inventory Catalog
    auto& inv = platform.getInventoryManager();
    inv.addProduct(Product("SKU-LAPTOP", "Pro Laptop 15", "Electronics", 50, 1200.0, "WH-1", "BIN_A1"));
    inv.addProduct(Product("SKU-MONITOR", "4K Monitor 27", "Electronics", 30, 450.0, "WH-1", "BIN_B1"));
    inv.addProduct(Product("SKU-KEYBOARD", "Mechanical Keyboard", "Accessories", 100, 80.0, "WH-1", "BIN_A1")); // Same bin as Laptop!
    inv.addProduct(Product("SKU-DESK", "Standing Desk Frame", "Furniture", 10, 600.0, "WH-1", "BIN_C1"));
    inv.addProduct(Product("SKU-ISOLATED", "Spare Sensor", "Parts", 25, 40.0, "WH-1", "ISOLATED_BIN"));
    inv.addProduct(Product("SKU-NOBIN", "Ghost Component", "Parts", 10, 15.0, "WH-1", "UNREGISTERED_BIN"));
    inv.addProduct(Product("SKU-UNPROFILED", "Raw Cable Bulk", "Cables", 50, 20.0, "WH-1", "BIN_A1"));

    // 3. Packaging Profiles
    platform.registerPackagingProfile("SKU-LAPTOP", PackagingProfile(35, 25, 5, 2.5));
    platform.registerPackagingProfile("SKU-MONITOR", PackagingProfile(60, 40, 15, 6.0));
    platform.registerPackagingProfile("SKU-KEYBOARD", PackagingProfile(45, 15, 4, 1.2));
    platform.registerPackagingProfile("SKU-DESK", PackagingProfile(120, 80, 20, 35.0));
    platform.registerPackagingProfile("SKU-ISOLATED", PackagingProfile(10, 10, 10, 1.0));
    // Note: SKU-NOBIN and SKU-UNPROFILED have NO packaging profiles
}

// ---------------------------------------------------------------------------
// 1. Successful Multi-Bin Route Through Explicit Packing Station
// ---------------------------------------------------------------------------
void testSuccessfulMultiBinRoute()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    Order ord("ORD-MULTI-01", "CUST-01", OrderPriority::High);
    ord.addItem("SKU-LAPTOP", 1);
    ord.addItem("SKU-MONITOR", 1);
    ord.setShippingParameters(50.0, 8.5);
    TEST_CHECK(platform.getOrderManager().submitOrder(ord));

    OrderRoutePlan plan = platform.planOrderRoute("ORD-MULTI-01", "DEPOT", "PACK_STATION", "DOCK");
    TEST_CHECK(plan.success);
    TEST_CHECK(plan.errorMessage.empty());

    // Expected sequence: DEPOT -> BIN_A1 (10) -> BIN_B1 (15) -> PACK_STATION (18) -> DOCK (8)
    // Total cost = 10 + 15 + 18 + 8 = 51.0
    TEST_CHECK(plan.legs.size() == 4);
    TEST_CHECK(std::abs(plan.totalCost - 51.0) < 1e-6);

    TEST_CHECK(plan.legs[0].source == "DEPOT" && plan.legs[0].destination == "BIN_A1");
    TEST_CHECK(std::abs(plan.legs[0].legCost - 10.0) < 1e-6);

    TEST_CHECK(plan.legs[1].source == "BIN_A1" && plan.legs[1].destination == "BIN_B1");
    TEST_CHECK(std::abs(plan.legs[1].legCost - 15.0) < 1e-6);

    TEST_CHECK(plan.legs[2].source == "BIN_B1" && plan.legs[2].destination == "PACK_STATION");
    TEST_CHECK(std::abs(plan.legs[2].legCost - 18.0) < 1e-6);

    TEST_CHECK(plan.legs[3].source == "PACK_STATION" && plan.legs[3].destination == "DOCK");
    TEST_CHECK(std::abs(plan.legs[3].legCost - 8.0) < 1e-6);
}

// ---------------------------------------------------------------------------
// 2. Missing Depot, Packing-Station, or Dock Vertex
// ---------------------------------------------------------------------------
void testMissingEndpointVertices()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    Order ord("ORD-ENDPOINTS", "CUST-01", OrderPriority::Normal);
    ord.addItem("SKU-LAPTOP", 1);
    ord.setShippingParameters(20.0, 2.5);
    TEST_CHECK(platform.getOrderManager().submitOrder(ord));

    // Missing depot
    OrderRoutePlan p1 = platform.planOrderRoute("ORD-ENDPOINTS", "NONEXISTENT_DEPOT", "PACK_STATION", "DOCK");
    TEST_CHECK(!p1.success);
    TEST_CHECK(!p1.errorMessage.empty());

    // Missing packing station
    OrderRoutePlan p2 = platform.planOrderRoute("ORD-ENDPOINTS", "DEPOT", "NONEXISTENT_PACK", "DOCK");
    TEST_CHECK(!p2.success);
    TEST_CHECK(!p2.errorMessage.empty());

    // Missing dock
    OrderRoutePlan p3 = platform.planOrderRoute("ORD-ENDPOINTS", "DEPOT", "PACK_STATION", "NONEXISTENT_DOCK");
    TEST_CHECK(!p3.success);
    TEST_CHECK(!p3.errorMessage.empty());

    // Blank vertex identifier
    OrderRoutePlan p4 = platform.planOrderRoute("ORD-ENDPOINTS", "", "PACK_STATION", "DOCK");
    TEST_CHECK(!p4.success);
    TEST_CHECK(!p4.errorMessage.empty());
}

// ---------------------------------------------------------------------------
// 3. Missing Product Bin Vertex
// ---------------------------------------------------------------------------
void testMissingProductBinVertex()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    // SKU-NOBIN is assigned to "UNREGISTERED_BIN" which does not exist in WarehouseGraph
    Order ord("ORD-NOBIN", "CUST-01", OrderPriority::Normal);
    ord.addItem("SKU-NOBIN", 1);
    ord.setShippingParameters(10.0, 1.0);
    TEST_CHECK(platform.getOrderManager().submitOrder(ord));

    OrderRoutePlan plan = platform.planOrderRoute("ORD-NOBIN", "DEPOT", "PACK_STATION", "DOCK");
    TEST_CHECK(!plan.success);
    TEST_CHECK(!plan.errorMessage.empty());
}

// ---------------------------------------------------------------------------
// 4. Unreachable Route Leg
// ---------------------------------------------------------------------------
void testUnreachableRouteLeg()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    // SKU-ISOLATED is in "ISOLATED_BIN" which exists in graph but has 0 edges
    Order ord("ORD-ISOLATED", "CUST-01", OrderPriority::Normal);
    ord.addItem("SKU-ISOLATED", 1);
    ord.setShippingParameters(15.0, 1.0);
    TEST_CHECK(platform.getOrderManager().submitOrder(ord));

    OrderRoutePlan plan = platform.planOrderRoute("ORD-ISOLATED", "DEPOT", "PACK_STATION", "DOCK");
    TEST_CHECK(!plan.success);
    TEST_CHECK(!plan.errorMessage.empty());
    TEST_CHECK(plan.legs.empty());
}

// ---------------------------------------------------------------------------
// 5. Repeated Bin Locations Visited Only Once
// ---------------------------------------------------------------------------
void testRepeatedBinDeduplication()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    // SKU-LAPTOP and SKU-KEYBOARD both reside in "BIN_A1"
    // Order requests: LAPTOP (BIN_A1), MONITOR (BIN_B1), KEYBOARD (BIN_A1)
    Order ord("ORD-DEDUP", "CUST-01", OrderPriority::Normal);
    ord.addItem("SKU-LAPTOP", 1);
    ord.addItem("SKU-MONITOR", 1);
    ord.addItem("SKU-KEYBOARD", 2);
    ord.setShippingParameters(30.0, 10.9);
    TEST_CHECK(platform.getOrderManager().submitOrder(ord));

    OrderRoutePlan plan = platform.planOrderRoute("ORD-DEDUP", "DEPOT", "PACK_STATION", "DOCK");
    TEST_CHECK(plan.success);

    // Unique bins in first-seen order: BIN_A1, BIN_B1
    // Waypoints must be: DEPOT -> BIN_A1 -> BIN_B1 -> PACK_STATION -> DOCK
    // Total legs = 4 (NOT 5, no self-loop BIN_A1 -> BIN_A1)
    TEST_CHECK(plan.legs.size() == 4);
    TEST_CHECK(plan.legs[0].source == "DEPOT" && plan.legs[0].destination == "BIN_A1");
    TEST_CHECK(plan.legs[1].source == "BIN_A1" && plan.legs[1].destination == "BIN_B1");
    TEST_CHECK(plan.legs[2].source == "BIN_B1" && plan.legs[2].destination == "PACK_STATION");
    TEST_CHECK(plan.legs[3].source == "PACK_STATION" && plan.legs[3].destination == "DOCK");
    TEST_CHECK(std::abs(plan.totalCost - 51.0) < 1e-6);
}

// ---------------------------------------------------------------------------
// 6. Missing Order and Empty Order
// ---------------------------------------------------------------------------
void testMissingAndEmptyOrder()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    // Non-existent order ID
    OrderRoutePlan rp = platform.planOrderRoute("NONEXISTENT_ORDER_ID", "DEPOT", "PACK_STATION", "DOCK");
    TEST_CHECK(!rp.success);
    TEST_CHECK(!rp.errorMessage.empty());

    packing::Container container(100, 100, 100);
    packing::PackingResult pr = platform.validateOrderPacking("NONEXISTENT_ORDER_ID", container);
    TEST_CHECK(!pr.allPackagesPlaced);
    TEST_CHECK(!pr.summary.empty());

    // Blank order ID
    OrderRoutePlan rpBlank = platform.planOrderRoute("", "DEPOT", "PACK_STATION", "DOCK");
    TEST_CHECK(!rpBlank.success);

    packing::PackingResult prBlank = platform.validateOrderPacking("", container);
    TEST_CHECK(!prBlank.allPackagesPlaced);
}

// ---------------------------------------------------------------------------
// 7. Correct Package Expansion for Quantities Greater Than One
// ---------------------------------------------------------------------------
void testPackageQuantityExpansion()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    // Order with 2x SKU-LAPTOP and 3x SKU-KEYBOARD -> 5 discrete packages
    Order ord("ORD-EXPAND", "CUST-01", OrderPriority::Normal);
    ord.addItem("SKU-LAPTOP", 2);
    ord.addItem("SKU-KEYBOARD", 3);
    ord.setShippingParameters(25.0, 8.6);
    TEST_CHECK(platform.getOrderManager().submitOrder(ord));

    // Generous container: 120 x 100 x 80
    packing::Container container(120, 100, 80);
    packing::PackingResult res = platform.validateOrderPacking("ORD-EXPAND", container);

    TEST_CHECK(res.allPackagesPlaced);
    TEST_CHECK(res.placements.size() == 5);
    TEST_CHECK(res.unplacedPackageIds.empty());

    // Total weight = 2 * 2.5 + 3 * 1.2 = 5.0 + 3.6 = 8.6 kg
    TEST_CHECK(std::abs(res.totalPackedWeight - 8.6) < 1e-6);

    // Verify all 5 package IDs are strictly distinct
    std::unordered_set<std::string> placedIds;
    for (const auto& p : res.placements) {
        TEST_CHECK(placedIds.insert(p.packageId).second);
    }
    TEST_CHECK(placedIds.size() == 5);
}

// ---------------------------------------------------------------------------
// 8. Missing Packaging Profile Rejection
// ---------------------------------------------------------------------------
void testMissingPackagingProfile()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    // SKU-UNPROFILED has no packaging profile registered
    Order ord("ORD-NOPROFILE", "CUST-01", OrderPriority::Normal);
    ord.addItem("SKU-UNPROFILED", 2);
    ord.setShippingParameters(15.0, 5.0);
    TEST_CHECK(platform.getOrderManager().submitOrder(ord));

    packing::Container container(100, 100, 100);
    packing::PackingResult res = platform.validateOrderPacking("ORD-NOPROFILE", container);

    TEST_CHECK(!res.allPackagesPlaced);
    TEST_CHECK(!res.summary.empty());
}

// ---------------------------------------------------------------------------
// 9. Packing Overflow and Infeasible Placement
// ---------------------------------------------------------------------------
void testPackingOverflowInfeasible()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    // SKU-DESK dimensions are 120 x 80 x 20
    Order ord("ORD-BIGDESK", "CUST-01", OrderPriority::Normal);
    ord.addItem("SKU-DESK", 1);
    ord.setShippingParameters(50.0, 35.0);
    TEST_CHECK(platform.getOrderManager().submitOrder(ord));

    // Container is much too small: 50 x 50 x 50
    packing::Container tinyContainer(50, 50, 50);
    packing::PackingResult res = platform.validateOrderPacking("ORD-BIGDESK", tinyContainer);

    TEST_CHECK(!res.allPackagesPlaced);
    TEST_CHECK(res.placements.empty());
    TEST_CHECK(res.unplacedPackageIds.size() == 1);
}

// ---------------------------------------------------------------------------
// 10. Invariant: Inventory and Order State Remain Unchanged After Prevalidation
// ---------------------------------------------------------------------------
void testPrevalidationNonMutationInvariant()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    Order ord("ORD-INVARIANT", "CUST-01", OrderPriority::Urgent);
    ord.addItem("SKU-LAPTOP", 4);
    ord.addItem("SKU-MONITOR", 2);
    ord.setShippingParameters(100.0, 22.0);
    TEST_CHECK(platform.getOrderManager().submitOrder(ord));

    auto& inv = platform.getInventoryManager();
    auto& om = platform.getOrderManager();
    auto& graph = platform.getWarehouseGraph();

    // Baseline graph topology
    const std::size_t initialVertexCount = graph.getVertexCount();
    const std::size_t initialEdgeCount = graph.getDirectedEdgeCount();

    // Baseline catalog stock
    const int initialLaptopQty = inv.findProduct("SKU-LAPTOP")->getQuantity();
    const int initialMonitorQty = inv.findProduct("SKU-MONITOR")->getQuantity();
    TEST_CHECK(initialLaptopQty == 50);
    TEST_CHECK(initialMonitorQty == 30);

    // Baseline order state
    const Order* storedOrder = om.getOrder("ORD-INVARIANT");
    TEST_CHECK(storedOrder != nullptr);
    TEST_CHECK(storedOrder->getStatus() == OrderStatus::Pending);

    const std::size_t initialPendingCount = om.getPendingCount();
    const std::size_t initialCompletedCount = om.getCompletedCount();
    const std::size_t initialFailedCount = om.getFailedCount();

    // 1. Run successful route planning
    OrderRoutePlan pSuccess = platform.planOrderRoute("ORD-INVARIANT", "DEPOT", "PACK_STATION", "DOCK");
    TEST_CHECK(pSuccess.success);

    // 2. Run successful cargo packing
    packing::Container largeContainer(200, 150, 100);
    packing::PackingResult packSuccess = platform.validateOrderPacking("ORD-INVARIANT", largeContainer);
    TEST_CHECK(packSuccess.allPackagesPlaced);

    // 3. Run failed route planning (invalid dock)
    OrderRoutePlan pFail = platform.planOrderRoute("ORD-INVARIANT", "DEPOT", "PACK_STATION", "NONEXISTENT_DOCK");
    TEST_CHECK(!pFail.success);

    // 4. Run failed cargo packing (tiny container)
    packing::Container tinyContainer(10, 10, 10);
    packing::PackingResult packFail = platform.validateOrderPacking("ORD-INVARIANT", tinyContainer);
    TEST_CHECK(!packFail.allPackagesPlaced);

    // VERIFY ABSOLUTE IMMUTABILITY:
    // Catalog stock must not change by even 1 unit
    TEST_CHECK(inv.findProduct("SKU-LAPTOP")->getQuantity() == initialLaptopQty);
    TEST_CHECK(inv.findProduct("SKU-MONITOR")->getQuantity() == initialMonitorQty);

    // Order status must remain strictly Pending
    TEST_CHECK(om.getOrder("ORD-INVARIANT")->getStatus() == OrderStatus::Pending);

    // Queue counts must be strictly identical
    TEST_CHECK(om.getPendingCount() == initialPendingCount);
    TEST_CHECK(om.getCompletedCount() == initialCompletedCount);
    TEST_CHECK(om.getFailedCount() == initialFailedCount);

    // Graph topology must remain strictly identical
    TEST_CHECK(graph.getVertexCount() == initialVertexCount);
    TEST_CHECK(graph.getDirectedEdgeCount() == initialEdgeCount);
}

// ---------------------------------------------------------------------------
// 11. Route-Cost Overflow Regression (Finite Individual Legs Summing to +inf)
// ---------------------------------------------------------------------------
void testRouteCostOverflowRegression()
{
    SupplyChainPlatform platform(101, 16);
    auto& graph = platform.getWarehouseGraph();
    auto& inv = platform.getInventoryManager();
    auto& om = platform.getOrderManager();

    // Vertices for route: DEPOT -> BIN_OVERFLOW -> PACK_STATION -> DOCK
    graph.addVertex("DEPOT");
    graph.addVertex("BIN_OVERFLOW");
    graph.addVertex("PACK_STATION");
    graph.addVertex("DOCK");

    // Add aisles with 1.0e308 navigation weight:
    // Individual legs have finite weight (1.0e308 < DBL_MAX approx 1.797e308),
    // but their sum (1.0e308 + 1.0e308 = 2.0e308) overflows double precision to +inf.
    graph.addBidirectionalAisle("DEPOT", "BIN_OVERFLOW", 1.0e308);
    graph.addBidirectionalAisle("BIN_OVERFLOW", "PACK_STATION", 1.0e308);
    graph.addBidirectionalAisle("PACK_STATION", "DOCK", 10.0);

    inv.addProduct(Product("SKU-OVERFLOW", "Heavy Duty Asset", "Industrial", 10, 500.0, "WH-1", "BIN_OVERFLOW"));
    platform.registerPackagingProfile("SKU-OVERFLOW", PackagingProfile(20, 20, 20, 5.0));

    Order ord("ORD-OVERFLOW", "CUST-01", OrderPriority::Normal);
    ord.addItem("SKU-OVERFLOW", 1);
    ord.setShippingParameters(50.0, 5.0);
    TEST_CHECK(om.submitOrder(ord));

    // Verify individual Dijkstra legs find paths with finite cost:
    auto leg1 = graph.findShortestPathDijkstra("DEPOT", "BIN_OVERFLOW");
    TEST_CHECK(leg1.hasPath);
    TEST_CHECK(std::isfinite(leg1.totalCost));
    TEST_CHECK(leg1.totalCost == 1.0e308);

    auto leg2 = graph.findShortestPathDijkstra("BIN_OVERFLOW", "PACK_STATION");
    TEST_CHECK(leg2.hasPath);
    TEST_CHECK(std::isfinite(leg2.totalCost));
    TEST_CHECK(leg2.totalCost == 1.0e308);

    // Now invoke planOrderRoute: aggregate route cost must overflow and be cleanly rejected
    OrderRoutePlan plan = platform.planOrderRoute("ORD-OVERFLOW", "DEPOT", "PACK_STATION", "DOCK");
    TEST_CHECK(!plan.success);
    TEST_CHECK(plan.totalCost == 0.0);
    TEST_CHECK(plan.legs.empty()); // Partial legs MUST be cleared
    TEST_CHECK(!plan.errorMessage.empty());
    TEST_CHECK(plan.errorMessage.find("overflow") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 12. Lifecycle Status Policy: Inspection of Cancelled, Completed, and Failed Orders
// ---------------------------------------------------------------------------
void testLifecycleStatusPolicy()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    auto& inv = platform.getInventoryManager();
    auto& om = platform.getOrderManager();
    auto& graph = platform.getWarehouseGraph();

    const std::size_t initialVertexCount = graph.getVertexCount();
    const std::size_t initialEdgeCount = graph.getDirectedEdgeCount();
    const int laptopInitialQty = inv.findProduct("SKU-LAPTOP")->getQuantity(); // 50
    const int monitorInitialQty = inv.findProduct("SKU-MONITOR")->getQuantity(); // 30

    // Container for packing validation
    packing::Container container(150, 150, 100);

    // --- Scenario A: Cancelled Order ---
    Order ordCancel("ORD-STATUS-CANCELLED", "CUST-01", OrderPriority::Normal);
    ordCancel.addItem("SKU-LAPTOP", 2);
    ordCancel.setShippingParameters(20.0, 5.0);
    TEST_CHECK(om.submitOrder(ordCancel));
    TEST_CHECK(om.cancelOrder("ORD-STATUS-CANCELLED"));
    TEST_CHECK(om.getOrder("ORD-STATUS-CANCELLED")->getStatus() == OrderStatus::Cancelled);

    // Prevalidation should succeed on cancelled order without mutating status or catalog
    OrderRoutePlan planCancel = platform.planOrderRoute("ORD-STATUS-CANCELLED", "DEPOT", "PACK_STATION", "DOCK");
    TEST_CHECK(planCancel.success);
    TEST_CHECK(planCancel.legs.size() == 3); // DEPOT -> BIN_A1 -> PACK_STATION -> DOCK

    packing::PackingResult packCancel = platform.validateOrderPacking("ORD-STATUS-CANCELLED", container);
    TEST_CHECK(packCancel.allPackagesPlaced);
    TEST_CHECK(packCancel.placements.size() == 2);

    // Order status must remain Cancelled, inventory stock and graph topology untouched
    TEST_CHECK(om.getOrder("ORD-STATUS-CANCELLED")->getStatus() == OrderStatus::Cancelled);
    TEST_CHECK(inv.findProduct("SKU-LAPTOP")->getQuantity() == laptopInitialQty);

    // --- Scenario B: Completed Order ---
    Order ordComplete("ORD-STATUS-COMPLETED", "CUST-01", OrderPriority::Urgent);
    ordComplete.addItem("SKU-LAPTOP", 1);
    ordComplete.setShippingParameters(15.0, 2.5);
    TEST_CHECK(om.submitOrder(ordComplete));
    // Dispatch to fulfill and complete the order
    TEST_CHECK(om.dispatchHighestPriorityOrder());
    TEST_CHECK(om.getOrder("ORD-STATUS-COMPLETED")->getStatus() == OrderStatus::Completed);
    // Stock is now 49
    TEST_CHECK(inv.findProduct("SKU-LAPTOP")->getQuantity() == laptopInitialQty - 1);

    // Prevalidation should succeed on completed order
    OrderRoutePlan planComplete = platform.planOrderRoute("ORD-STATUS-COMPLETED", "DEPOT", "PACK_STATION", "DOCK");
    TEST_CHECK(planComplete.success);
    TEST_CHECK(planComplete.legs.size() == 3);

    packing::PackingResult packComplete = platform.validateOrderPacking("ORD-STATUS-COMPLETED", container);
    TEST_CHECK(packComplete.allPackagesPlaced);
    TEST_CHECK(packComplete.placements.size() == 1);

    // Order status must remain Completed, inventory stock not further modified
    TEST_CHECK(om.getOrder("ORD-STATUS-COMPLETED")->getStatus() == OrderStatus::Completed);
    TEST_CHECK(inv.findProduct("SKU-LAPTOP")->getQuantity() == laptopInitialQty - 1);

    // --- Scenario C: Failed Order ---
    // Register product with stock=1 and order quantity=2 to trigger insufficient stock failure
    inv.addProduct(Product("SKU-LIMITED", "Limited Stock Item", "Hardware", 1, 50.0, "WH-1", "BIN_C1"));
    platform.registerPackagingProfile("SKU-LIMITED", PackagingProfile(30, 20, 10, 2.0));

    Order ordFail("ORD-STATUS-FAILED", "CUST-01", OrderPriority::Urgent);
    ordFail.addItem("SKU-LIMITED", 2);
    ordFail.setShippingParameters(25.0, 4.0);
    TEST_CHECK(om.submitOrder(ordFail));
    // Dispatch triggers fulfillment failure due to insufficient stock (required 2 > available 1)
    TEST_CHECK(om.dispatchHighestPriorityOrder());
    TEST_CHECK(om.getOrder("ORD-STATUS-FAILED")->getStatus() == OrderStatus::Failed);
    // Stock remains 1
    TEST_CHECK(inv.findProduct("SKU-LIMITED")->getQuantity() == 1);

    // Prevalidation route planning should succeed on failed order (evaluates route for SKU bin BIN_C1)
    OrderRoutePlan planFail = platform.planOrderRoute("ORD-STATUS-FAILED", "DEPOT", "PACK_STATION", "DOCK");
    TEST_CHECK(planFail.success);
    TEST_CHECK(planFail.legs.size() == 3); // DEPOT -> BIN_C1 -> PACK_STATION -> DOCK

    // Prevalidation packing should pack the 2 discrete units into the container
    packing::PackingResult packFailResult = platform.validateOrderPacking("ORD-STATUS-FAILED", container);
    TEST_CHECK(packFailResult.allPackagesPlaced);
    TEST_CHECK(packFailResult.placements.size() == 2);

    // Order status must remain Failed, stock untouched
    TEST_CHECK(om.getOrder("ORD-STATUS-FAILED")->getStatus() == OrderStatus::Failed);
    TEST_CHECK(inv.findProduct("SKU-LIMITED")->getQuantity() == 1);

    // Verify absolute immutability of graph topology across all tests
    TEST_CHECK(graph.getVertexCount() == initialVertexCount);
    TEST_CHECK(graph.getDirectedEdgeCount() == initialEdgeCount);
}

// ---------------------------------------------------------------------------
// 13. Guarded Dispatch: Successful Dispatch of Highest-Priority Order
// ---------------------------------------------------------------------------
void testGuardedDispatchSuccessful()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    auto& inv = platform.getInventoryManager();
    auto& om = platform.getOrderManager();

    // Baseline stock
    const int initialLaptopQty = inv.findProduct("SKU-LAPTOP")->getQuantity(); // 50
    const int initialMonitorQty = inv.findProduct("SKU-MONITOR")->getQuantity(); // 30

    Order ord("ORD-GUARD-SUCCESS", "CUST-01", OrderPriority::High);
    ord.addItem("SKU-LAPTOP", 2);
    ord.addItem("SKU-MONITOR", 1);
    ord.setShippingParameters(50.0, 11.0);
    TEST_CHECK(om.submitOrder(ord));
    TEST_CHECK(om.getPendingCount() == 1);

    packing::Container container(150, 150, 100);
    GuardedDispatchResult res = platform.dispatchHighestPriorityOrder(
        "DEPOT", "PACK_STATION", "DOCK", container);

    // Verify dispatch result semantics
    TEST_CHECK(res.success);
    TEST_CHECK(res.dispatchAttempted);
    TEST_CHECK(res.orderId == "ORD-GUARD-SUCCESS");
    TEST_CHECK(res.resultingStatus.has_value());
    TEST_CHECK(res.resultingStatus.value() == OrderStatus::Completed);
    TEST_CHECK(res.routePlan.success);
    TEST_CHECK(res.packingResult.allPackagesPlaced);
    TEST_CHECK(!res.message.empty());

    // Verify exact stock deductions
    TEST_CHECK(inv.findProduct("SKU-LAPTOP")->getQuantity() == initialLaptopQty - 2);
    TEST_CHECK(inv.findProduct("SKU-MONITOR")->getQuantity() == initialMonitorQty - 1);

    // Verify order lifecycle transition in OrderManager
    const Order* stored = om.getOrder("ORD-GUARD-SUCCESS");
    TEST_CHECK(stored != nullptr);
    TEST_CHECK(stored->getStatus() == OrderStatus::Completed);
    TEST_CHECK(om.getPendingCount() == 0);
    TEST_CHECK(om.getCompletedCount() == 1);
}

// ---------------------------------------------------------------------------
// 14. Guarded Dispatch: Empty Order Queue Rejection
// ---------------------------------------------------------------------------
void testGuardedDispatchEmptyQueue()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    packing::Container container(100, 100, 100);
    GuardedDispatchResult res = platform.dispatchHighestPriorityOrder(
        "DEPOT", "PACK_STATION", "DOCK", container);

    TEST_CHECK(!res.success);
    TEST_CHECK(!res.dispatchAttempted);
    TEST_CHECK(res.orderId.empty());
    TEST_CHECK(!res.resultingStatus.has_value());
    TEST_CHECK(!res.message.empty());
    TEST_CHECK(res.message.find("no eligible pending orders") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 15. Guarded Dispatch: Missing Packaging Profile Rejection (Zero Mutation)
// ---------------------------------------------------------------------------
void testGuardedDispatchMissingPackagingProfileRejection()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    auto& inv = platform.getInventoryManager();
    auto& om = platform.getOrderManager();

    // SKU-UNPROFILED has no registered packaging profile; initial stock is 50
    const int initialStock = inv.findProduct("SKU-UNPROFILED")->getQuantity();
    TEST_CHECK(initialStock == 50);

    Order ord("ORD-GUARD-NOPROF", "CUST-01", OrderPriority::High);
    ord.addItem("SKU-UNPROFILED", 2);
    ord.setShippingParameters(20.0, 4.0);
    TEST_CHECK(om.submitOrder(ord));
    TEST_CHECK(om.getPendingCount() == 1);

    packing::Container container(100, 100, 100);
    GuardedDispatchResult res = platform.dispatchHighestPriorityOrder(
        "DEPOT", "PACK_STATION", "DOCK", container);

    TEST_CHECK(!res.success);
    TEST_CHECK(!res.dispatchAttempted);
    TEST_CHECK(res.orderId == "ORD-GUARD-NOPROF");
    TEST_CHECK(res.resultingStatus.has_value());
    TEST_CHECK(res.resultingStatus.value() == OrderStatus::Pending);
    TEST_CHECK(!res.packingResult.allPackagesPlaced);
    TEST_CHECK(res.message.find("packing prevalidation failed") != std::string::npos);

    // Order status and inventory stock must remain strictly unmodified
    TEST_CHECK(om.getOrder("ORD-GUARD-NOPROF")->getStatus() == OrderStatus::Pending);
    TEST_CHECK(inv.findProduct("SKU-UNPROFILED")->getQuantity() == initialStock);
    TEST_CHECK(om.getPendingCount() == 1);
    TEST_CHECK(om.getCompletedCount() == 0);
}

// ---------------------------------------------------------------------------
// 16. Guarded Dispatch: Route Prevalidation Failure Rejection (Zero Mutation)
// ---------------------------------------------------------------------------
void testGuardedDispatchRoutePrevalidationFailureRejection()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    auto& inv = platform.getInventoryManager();
    auto& om = platform.getOrderManager();

    const int initialLaptopQty = inv.findProduct("SKU-LAPTOP")->getQuantity();

    Order ord("ORD-GUARD-BADROUTE", "CUST-01", OrderPriority::High);
    ord.addItem("SKU-LAPTOP", 1);
    ord.setShippingParameters(20.0, 2.5);
    TEST_CHECK(om.submitOrder(ord));

    packing::Container container(100, 100, 100);
    // Invalid destination dock vertex
    GuardedDispatchResult res = platform.dispatchHighestPriorityOrder(
        "DEPOT", "PACK_STATION", "NONEXISTENT_DOCK", container);

    TEST_CHECK(!res.success);
    TEST_CHECK(!res.dispatchAttempted);
    TEST_CHECK(res.orderId == "ORD-GUARD-BADROUTE");
    TEST_CHECK(res.resultingStatus.has_value());
    TEST_CHECK(res.resultingStatus.value() == OrderStatus::Pending);
    TEST_CHECK(!res.routePlan.success);
    TEST_CHECK(res.message.find("route prevalidation failed") != std::string::npos);

    // Zero mutation guaranteed
    TEST_CHECK(om.getOrder("ORD-GUARD-BADROUTE")->getStatus() == OrderStatus::Pending);
    TEST_CHECK(inv.findProduct("SKU-LAPTOP")->getQuantity() == initialLaptopQty);
    TEST_CHECK(om.getPendingCount() == 1);
    TEST_CHECK(om.getCompletedCount() == 0);
}

// ---------------------------------------------------------------------------
// 17. Guarded Dispatch: Packing Prevalidation Failure Rejection (Zero Mutation)
// ---------------------------------------------------------------------------
void testGuardedDispatchPackingPrevalidationFailureRejection()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    auto& inv = platform.getInventoryManager();
    auto& om = platform.getOrderManager();

    // SKU-DESK is 120 x 80 x 20; stock is 10
    const int initialDeskQty = inv.findProduct("SKU-DESK")->getQuantity();

    Order ord("ORD-GUARD-DESKTOOBIG", "CUST-01", OrderPriority::High);
    ord.addItem("SKU-DESK", 1);
    ord.setShippingParameters(50.0, 35.0);
    TEST_CHECK(om.submitOrder(ord));

    // Container is much too small (30 x 30 x 30)
    packing::Container tinyContainer(30, 30, 30);
    GuardedDispatchResult res = platform.dispatchHighestPriorityOrder(
        "DEPOT", "PACK_STATION", "DOCK", tinyContainer);

    TEST_CHECK(!res.success);
    TEST_CHECK(!res.dispatchAttempted);
    TEST_CHECK(res.orderId == "ORD-GUARD-DESKTOOBIG");
    TEST_CHECK(res.resultingStatus.has_value());
    TEST_CHECK(res.resultingStatus.value() == OrderStatus::Pending);
    TEST_CHECK(!res.packingResult.allPackagesPlaced);
    TEST_CHECK(res.message.find("packing prevalidation failed") != std::string::npos);

    // Zero mutation guaranteed
    TEST_CHECK(om.getOrder("ORD-GUARD-DESKTOOBIG")->getStatus() == OrderStatus::Pending);
    TEST_CHECK(inv.findProduct("SKU-DESK")->getQuantity() == initialDeskQty);
    TEST_CHECK(om.getPendingCount() == 1);
    TEST_CHECK(om.getCompletedCount() == 0);
}

// ---------------------------------------------------------------------------
// 18. Guarded Dispatch: OrderManager Fulfillment-Failure Behavior Reported
// ---------------------------------------------------------------------------
void testGuardedDispatchFulfillmentFailureBehavior()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    auto& inv = platform.getInventoryManager();
    auto& om = platform.getOrderManager();

    // Register SKU with stock=1 and order quantity=5
    inv.addProduct(Product("SKU-SCARCE", "Scarce Commodity", "Hardware", 1, 100.0, "WH-1", "BIN_A1"));
    platform.registerPackagingProfile("SKU-SCARCE", PackagingProfile(20, 20, 20, 1.0));

    Order ord("ORD-GUARD-FULFILLFAIL", "CUST-01", OrderPriority::Urgent);
    ord.addItem("SKU-SCARCE", 5);
    ord.setShippingParameters(30.0, 5.0);
    TEST_CHECK(om.submitOrder(ord));

    // Route and packing prevalidations will pass because bin BIN_A1 exists and container is large
    packing::Container container(150, 150, 100);
    GuardedDispatchResult res = platform.dispatchHighestPriorityOrder(
        "DEPOT", "PACK_STATION", "DOCK", container);

    // Dispatch was invoked, but fulfillment failed due to insufficient inventory
    TEST_CHECK(!res.success);
    TEST_CHECK(res.dispatchAttempted);
    TEST_CHECK(res.orderId == "ORD-GUARD-FULFILLFAIL");
    TEST_CHECK(res.resultingStatus.has_value());
    TEST_CHECK(res.resultingStatus.value() == OrderStatus::Failed);
    TEST_CHECK(res.routePlan.success);
    TEST_CHECK(res.packingResult.allPackagesPlaced);
    TEST_CHECK(res.message.find("fulfillment failed") != std::string::npos);

    // Verify OrderManager fulfillment contract: failed fulfillment touches zero inventory
    TEST_CHECK(inv.findProduct("SKU-SCARCE")->getQuantity() == 1);
    const Order* stored = om.getOrder("ORD-GUARD-FULFILLFAIL");
    TEST_CHECK(stored != nullptr);
    TEST_CHECK(stored->getStatus() == OrderStatus::Failed);
    TEST_CHECK(om.getFailedCount() == 1);
    TEST_CHECK(om.getCompletedCount() == 0);
}

// ---------------------------------------------------------------------------
// 19. Guarded Dispatch: Dispatches Exactly One Order Per Invocation
// ---------------------------------------------------------------------------
void testGuardedDispatchSingleOrderAtATime()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    auto& om = platform.getOrderManager();

    // Order 1: Urgent priority
    Order ord1("ORD-BATCH-1", "CUST-01", OrderPriority::Urgent);
    ord1.addItem("SKU-LAPTOP", 1);
    ord1.setShippingParameters(20.0, 2.5);
    TEST_CHECK(om.submitOrder(ord1));

    // Order 2: Normal priority
    Order ord2("ORD-BATCH-2", "CUST-02", OrderPriority::Normal);
    ord2.addItem("SKU-MONITOR", 1);
    ord2.setShippingParameters(20.0, 6.0);
    TEST_CHECK(om.submitOrder(ord2));

    TEST_CHECK(om.getPendingCount() == 2);

    packing::Container container(150, 150, 100);

    // Call 1: Must dispatch ONLY ORD-BATCH-1 (highest priority)
    GuardedDispatchResult res1 = platform.dispatchHighestPriorityOrder(
        "DEPOT", "PACK_STATION", "DOCK", container);
    TEST_CHECK(res1.success);
    TEST_CHECK(res1.orderId == "ORD-BATCH-1");
    TEST_CHECK(om.getOrder("ORD-BATCH-1")->getStatus() == OrderStatus::Completed);
    TEST_CHECK(om.getOrder("ORD-BATCH-2")->getStatus() == OrderStatus::Pending);
    TEST_CHECK(om.getPendingCount() == 1);
    TEST_CHECK(om.getCompletedCount() == 1);

    // Call 2: Dispatches remaining ORD-BATCH-2
    GuardedDispatchResult res2 = platform.dispatchHighestPriorityOrder(
        "DEPOT", "PACK_STATION", "DOCK", container);
    TEST_CHECK(res2.success);
    TEST_CHECK(res2.orderId == "ORD-BATCH-2");
    TEST_CHECK(om.getOrder("ORD-BATCH-2")->getStatus() == OrderStatus::Completed);
    TEST_CHECK(om.getPendingCount() == 0);
    TEST_CHECK(om.getCompletedCount() == 2);
}

// ---------------------------------------------------------------------------
// 20. Guarded Dispatch: Pre-Dispatch Candidate Recheck & Eligibility
// ---------------------------------------------------------------------------
void testGuardedDispatchPrecheckAndCancelledSkipping()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    auto& om = platform.getOrderManager();

    // Submit Urgent order and immediately cancel it
    Order ordCancel("ORD-TOP-CANCELLED", "CUST-01", OrderPriority::Urgent);
    ordCancel.addItem("SKU-LAPTOP", 1);
    ordCancel.setShippingParameters(20.0, 2.5);
    TEST_CHECK(om.submitOrder(ordCancel));
    TEST_CHECK(om.cancelOrder("ORD-TOP-CANCELLED"));

    // Submit Normal order
    Order ordNext("ORD-NEXT-ELIGIBLE", "CUST-02", OrderPriority::Normal);
    ordNext.addItem("SKU-MONITOR", 1);
    ordNext.setShippingParameters(20.0, 6.0);
    TEST_CHECK(om.submitOrder(ordNext));

    packing::Container container(150, 150, 100);

    // Guarded dispatch should lazily bypass the cancelled order and select the next eligible pending candidate
    GuardedDispatchResult res = platform.dispatchHighestPriorityOrder(
        "DEPOT", "PACK_STATION", "DOCK", container);

    TEST_CHECK(res.success);
    TEST_CHECK(res.orderId == "ORD-NEXT-ELIGIBLE");
    TEST_CHECK(om.getOrder("ORD-TOP-CANCELLED")->getStatus() == OrderStatus::Cancelled);
    TEST_CHECK(om.getOrder("ORD-NEXT-ELIGIBLE")->getStatus() == OrderStatus::Completed);
}

// ---------------------------------------------------------------------------
// 21. Guarded Dispatch: Recheck Failure When Sole Candidate Cancelled Intermediately
// ---------------------------------------------------------------------------
void testGuardedDispatchRecheckFailureCancelledSoleOrder()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    auto& inv = platform.getInventoryManager();
    auto& om = platform.getOrderManager();

    const int initialLaptopQty = inv.findProduct("SKU-LAPTOP")->getQuantity();

    Order ord("ORD-RECHECK-SOLO", "CUST-01", OrderPriority::High);
    ord.addItem("SKU-LAPTOP", 1);
    ord.setShippingParameters(20.0, 2.5);
    TEST_CHECK(om.submitOrder(ord));
    TEST_CHECK(om.getPendingCount() == 1);

    // Inject intervening state change: cancel candidate order between initial validation and final recheck
    SupplyChainPlatformTestAccessor::setPreRecheckHook(platform, [&om](const std::string& orderId) {
        TEST_CHECK(om.cancelOrder(orderId));
    });

    packing::Container container(150, 150, 100);
    GuardedDispatchResult res = platform.dispatchHighestPriorityOrder(
        "DEPOT", "PACK_STATION", "DOCK", container);

    TEST_CHECK(!res.success);
    TEST_CHECK(!res.dispatchAttempted);
    TEST_CHECK(res.orderId == "ORD-RECHECK-SOLO");
    TEST_CHECK(res.resultingStatus.has_value());
    TEST_CHECK(res.resultingStatus.value() == OrderStatus::Cancelled);
    TEST_CHECK(res.message.find("candidate order changed") != std::string::npos);
    TEST_CHECK(res.message.find("found none") != std::string::npos);

    // Verify non-mutation of inventory and order status
    TEST_CHECK(inv.findProduct("SKU-LAPTOP")->getQuantity() == initialLaptopQty);
    const Order* stored = om.getOrder("ORD-RECHECK-SOLO");
    TEST_CHECK(stored != nullptr);
    TEST_CHECK(stored->getStatus() == OrderStatus::Cancelled);
    TEST_CHECK(om.getPendingCount() == 0);
    TEST_CHECK(om.getCompletedCount() == 0);
}

// ---------------------------------------------------------------------------
// 22. Guarded Dispatch: Recheck Failure When Candidate Cancelled With Standby Order
// ---------------------------------------------------------------------------
void testGuardedDispatchRecheckFailureCancelledWithStandbyOrder()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    auto& inv = platform.getInventoryManager();
    auto& om = platform.getOrderManager();

    const int initialLaptopQty = inv.findProduct("SKU-LAPTOP")->getQuantity();
    const int initialMonitorQty = inv.findProduct("SKU-MONITOR")->getQuantity();

    // Standby lower-priority order
    Order ordStandby("ORD-RECHECK-STANDBY", "CUST-02", OrderPriority::Normal);
    ordStandby.addItem("SKU-MONITOR", 1);
    ordStandby.setShippingParameters(20.0, 6.0);
    TEST_CHECK(om.submitOrder(ordStandby));

    // High-priority candidate order
    Order ordCancel("ORD-RECHECK-TOP", "CUST-01", OrderPriority::High);
    ordCancel.addItem("SKU-LAPTOP", 1);
    ordCancel.setShippingParameters(20.0, 2.5);
    TEST_CHECK(om.submitOrder(ordCancel));

    TEST_CHECK(om.getPendingCount() == 2);

    // Cancel top order between initial validation and final recheck
    SupplyChainPlatformTestAccessor::setPreRecheckHook(platform, [&om](const std::string& orderId) {
        TEST_CHECK(om.cancelOrder(orderId));
    });

    packing::Container container(150, 150, 100);
    GuardedDispatchResult res = platform.dispatchHighestPriorityOrder(
        "DEPOT", "PACK_STATION", "DOCK", container);

    TEST_CHECK(!res.success);
    TEST_CHECK(!res.dispatchAttempted);
    TEST_CHECK(res.orderId == "ORD-RECHECK-TOP");
    TEST_CHECK(res.resultingStatus.has_value());
    TEST_CHECK(res.resultingStatus.value() == OrderStatus::Cancelled);
    TEST_CHECK(res.message.find("candidate order changed") != std::string::npos);
    TEST_CHECK(res.message.find("found ORD-RECHECK-STANDBY") != std::string::npos);

    // Verify non-mutation of inventory, standby order remains safely Pending, top remains Cancelled
    TEST_CHECK(inv.findProduct("SKU-LAPTOP")->getQuantity() == initialLaptopQty);
    TEST_CHECK(inv.findProduct("SKU-MONITOR")->getQuantity() == initialMonitorQty);
    TEST_CHECK(om.getOrder("ORD-RECHECK-TOP")->getStatus() == OrderStatus::Cancelled);
    TEST_CHECK(om.getOrder("ORD-RECHECK-STANDBY")->getStatus() == OrderStatus::Pending);
    TEST_CHECK(om.getPendingCount() == 1);
    TEST_CHECK(om.getCompletedCount() == 0);
}

// ---------------------------------------------------------------------------
// 23. Guarded Dispatch: Recheck Failure When Higher-Priority Candidate Preempts
// ---------------------------------------------------------------------------
void testGuardedDispatchRecheckFailurePreemption()
{
    SupplyChainPlatform platform(101, 16);
    seedStandardPlatform(platform);

    auto& inv = platform.getInventoryManager();
    auto& om = platform.getOrderManager();

    const int initialLaptopQty = inv.findProduct("SKU-LAPTOP")->getQuantity();
    const int initialMonitorQty = inv.findProduct("SKU-MONITOR")->getQuantity();

    Order ordNormal("ORD-RECHECK-NORMAL", "CUST-01", OrderPriority::Normal);
    ordNormal.addItem("SKU-LAPTOP", 1);
    ordNormal.setShippingParameters(20.0, 2.5);
    TEST_CHECK(om.submitOrder(ordNormal));

    // Submit higher-priority Urgent order between initial validation and final recheck
    SupplyChainPlatformTestAccessor::setPreRecheckHook(platform, [&om](const std::string& /*orderId*/) {
        Order ordUrgent("ORD-RECHECK-URGENT", "CUST-02", OrderPriority::Urgent);
        ordUrgent.addItem("SKU-MONITOR", 1);
        ordUrgent.setShippingParameters(20.0, 6.0);
        TEST_CHECK(om.submitOrder(ordUrgent));
    });

    packing::Container container(150, 150, 100);
    GuardedDispatchResult res = platform.dispatchHighestPriorityOrder(
        "DEPOT", "PACK_STATION", "DOCK", container);

    TEST_CHECK(!res.success);
    TEST_CHECK(!res.dispatchAttempted);
    TEST_CHECK(res.orderId == "ORD-RECHECK-NORMAL");
    TEST_CHECK(res.resultingStatus.has_value());
    TEST_CHECK(res.resultingStatus.value() == OrderStatus::Pending);
    TEST_CHECK(res.message.find("candidate order changed") != std::string::npos);
    TEST_CHECK(res.message.find("found ORD-RECHECK-URGENT") != std::string::npos);

    // Verify non-mutation of inventory, both orders remain Pending, none dispatched
    TEST_CHECK(inv.findProduct("SKU-LAPTOP")->getQuantity() == initialLaptopQty);
    TEST_CHECK(inv.findProduct("SKU-MONITOR")->getQuantity() == initialMonitorQty);
    TEST_CHECK(om.getOrder("ORD-RECHECK-NORMAL")->getStatus() == OrderStatus::Pending);
    TEST_CHECK(om.getOrder("ORD-RECHECK-URGENT")->getStatus() == OrderStatus::Pending);
    TEST_CHECK(om.getPendingCount() == 2);
    TEST_CHECK(om.getCompletedCount() == 0);
}

// ---------------------------------------------------------------------------
// 24. End-to-End Integrated Logistics Demonstration Workflow
// ---------------------------------------------------------------------------
void testEndToEndDemonstrationWorkflow()
{
    SupplyChainPlatform platform(101, 16);

    // 1. Warehouse Spatial Navigation Network
    auto& graph = platform.getWarehouseGraph();
    graph.addVertex("DEPOT");
    graph.addVertex("BIN_A1");
    graph.addVertex("BIN_B1");
    graph.addVertex("BIN_C1");
    graph.addVertex("PACK_STATION_1");
    graph.addVertex("DOCK_ALPHA");

    graph.addBidirectionalAisle("DEPOT", "BIN_A1", 12.0);
    graph.addBidirectionalAisle("BIN_A1", "BIN_B1", 15.0);
    graph.addBidirectionalAisle("BIN_B1", "BIN_C1", 20.0);
    graph.addBidirectionalAisle("BIN_A1", "PACK_STATION_1", 14.0);
    graph.addBidirectionalAisle("BIN_B1", "PACK_STATION_1", 18.0);
    graph.addBidirectionalAisle("BIN_C1", "PACK_STATION_1", 22.0);
    graph.addDirectedEdge("PACK_STATION_1", "DOCK_ALPHA", 8.0);

    // 2. Inventory Catalog & Packaging Profiles
    auto& inv = platform.getInventoryManager();
    TEST_CHECK(inv.addProduct(Product("SKU-LAPTOP", "Enterprise Laptop 15", "Electronics", 50, 1200.0, "WH-1", "BIN_A1")));
    TEST_CHECK(inv.addProduct(Product("SKU-MONITOR", "Ultra-HD 4K Display 27", "Electronics", 30, 450.0, "WH-1", "BIN_B1")));
    TEST_CHECK(inv.addProduct(Product("SKU-KEYBOARD", "Mechanical Pro Keyboard", "Accessories", 100, 80.0, "WH-1", "BIN_A1")));
    TEST_CHECK(inv.addProduct(Product("SKU-DESK", "Motorized Standing Desk", "Furniture", 10, 600.0, "WH-1", "BIN_C1")));
    TEST_CHECK(inv.addProduct(Product("SKU-CABLE", "Braided USB-C Cable 2m", "Accessories", 200, 15.0, "WH-1", "BIN_B1")));

    platform.registerPackagingProfile("SKU-LAPTOP", PackagingProfile(35, 25, 5, 2.5));
    platform.registerPackagingProfile("SKU-MONITOR", PackagingProfile(60, 40, 15, 6.0));
    platform.registerPackagingProfile("SKU-KEYBOARD", PackagingProfile(45, 15, 4, 1.2));
    platform.registerPackagingProfile("SKU-DESK", PackagingProfile(120, 80, 20, 35.0));
    platform.registerPackagingProfile("SKU-CABLE", PackagingProfile(15, 10, 5, 0.4));

    // 3. Slotting Analysis (Pareto ABC)
    SlottingReport slotReport = inv.analyzeSlotting();
    TEST_CHECK(slotReport.isValid);
    TEST_CHECK(slotReport.totalProducts == 5);
    TEST_CHECK(slotReport.totalInventoryValue > 90000.0);

    // 4. Inbound Customer Orders (DEPQ Priority Scheduling)
    auto& om = platform.getOrderManager();
    Order ord1("ORD-1001", "CORP-GLOBAL", OrderPriority::Urgent);
    ord1.addItem("SKU-LAPTOP", 1);
    ord1.addItem("SKU-KEYBOARD", 1);
    ord1.setShippingParameters(15.0, 3.7);
    TEST_CHECK(om.submitOrder(ord1));

    Order ord2("ORD-1002", "CUST-RETAIL", OrderPriority::Normal);
    ord2.addItem("SKU-MONITOR", 2);
    ord2.setShippingParameters(25.0, 12.0);
    TEST_CHECK(om.submitOrder(ord2));

    Order ord3("ORD-1003", "STORE-SOUTH", OrderPriority::Low);
    ord3.addItem("SKU-DESK", 1);
    ord3.setShippingParameters(40.0, 35.0);
    TEST_CHECK(om.submitOrder(ord3));

    TEST_CHECK(om.getPendingCount() == 3);

    std::string topCandidate;
    TEST_CHECK(om.peekHighestPriorityOrder(topCandidate));
    TEST_CHECK(topCandidate == "ORD-1001");

    // 5. Route Prevalidation
    auto routePlan = platform.planOrderRoute(topCandidate, "DEPOT", "PACK_STATION_1", "DOCK_ALPHA");
    TEST_CHECK(routePlan.success);
    TEST_CHECK(routePlan.totalCost == 34.0); // 12 + 14 + 8
    TEST_CHECK(routePlan.legs.size() == 3);

    // 6. 3D Cargo Packing Prevalidation
    packing::Container container(120, 80, 80);
    auto packResult = platform.validateOrderPacking(topCandidate, container);
    TEST_CHECK(packResult.allPackagesPlaced);
    TEST_CHECK(packResult.placements.size() == 2);

    // 7. Guarded Order Dispatch & Inventory Deduction
    const int laptopBefore = inv.findProduct("SKU-LAPTOP")->getQuantity();
    const int keyboardBefore = inv.findProduct("SKU-KEYBOARD")->getQuantity();

    auto dispatchResult = platform.dispatchHighestPriorityOrder(
        "DEPOT", "PACK_STATION_1", "DOCK_ALPHA", container);
    TEST_CHECK(dispatchResult.success);
    TEST_CHECK(dispatchResult.dispatchAttempted);
    TEST_CHECK(dispatchResult.orderId == "ORD-1001");
    TEST_CHECK(dispatchResult.resultingStatus.has_value());
    TEST_CHECK(dispatchResult.resultingStatus.value() == OrderStatus::Completed);

    TEST_CHECK(inv.findProduct("SKU-LAPTOP")->getQuantity() == laptopBefore - 1);
    TEST_CHECK(inv.findProduct("SKU-KEYBOARD")->getQuantity() == keyboardBefore - 1);
    TEST_CHECK(om.getPendingCount() == 2);
    TEST_CHECK(om.getCompletedCount() == 1);

    // Verify next in line is ORD-1002
    std::string nextCandidate;
    TEST_CHECK(om.peekHighestPriorityOrder(nextCandidate));
    TEST_CHECK(nextCandidate == "ORD-1002");

    // 8. AGV Traffic & Deadlock Resolution
    auto& wfg = platform.getWaitForGraph();
    wfg.addAgv("AGV-ALPHA", TaskPriority::High);
    wfg.addAgv("AGV-BETA", TaskPriority::Urgent);
    wfg.addAgv("AGV-GAMMA", TaskPriority::Low);
    wfg.addDependency("AGV-ALPHA", "AGV-BETA");
    wfg.addDependency("AGV-BETA", "AGV-GAMMA");
    wfg.addDependency("AGV-GAMMA", "AGV-ALPHA");

    DeadlockCycle cycle = wfg.detectDeadlock();
    TEST_CHECK(cycle.hasDeadlock);
    TEST_CHECK(cycle.cycle.size() == 4); // [ALPHA, BETA, GAMMA, ALPHA]
    std::string victim = wfg.selectVictim(cycle);
    TEST_CHECK(victim == "AGV-GAMMA");

    wfg.removeDependency("AGV-BETA", "AGV-GAMMA");
    wfg.removeDependency("AGV-GAMMA", "AGV-ALPHA");
    TEST_CHECK(!wfg.detectDeadlock().hasDeadlock);

    // 9. Cyclic QA Auditing
    auto& audit = platform.getAuditCycle();
    audit.addTarget("BIN_A1");
    audit.addTarget("BIN_B1");
    audit.addTarget("BIN_C1");
    audit.addTarget("PACK_STATION_1");
    audit.addTarget("DOCK_ALPHA");
    TEST_CHECK(audit.getTargetCount() == 5);

    auto audited = audit.performAuditSelection(2);
    TEST_CHECK(audited.size() == 2);
    TEST_CHECK(audit.getTargetCount() == 3);

    // 10. Operational Task Prioritization
    auto& tm = platform.getTaskManager();
    TEST_CHECK(tm.registerTask("TASK-SPILL", TaskCategory::Emergency, TaskPriority::Critical, "DC-CENTRAL"));
    TEST_CHECK(tm.registerTask("TASK-SWEEP", TaskCategory::Maintenance, TaskPriority::Low, "DC-CENTRAL"));
    TEST_CHECK(tm.getActiveCount() == 2);

    PriorityTask dispHighest("TEMP", TaskCategory::Pick, TaskPriority::Low, "DC", 0);
    TEST_CHECK(tm.dispatchHighestPriority(dispHighest));
    TEST_CHECK(dispHighest.getTaskId() == "TASK-SPILL");

    PriorityTask dispLowest("TEMP", TaskCategory::Pick, TaskPriority::Low, "DC", 0);
    TEST_CHECK(tm.dispatchLowestPriority(dispLowest));
    TEST_CHECK(dispLowest.getTaskId() == "TASK-SWEEP");
    TEST_CHECK(tm.getActiveCount() == 0);
}

// ---------------------------------------------------------------------------
// 25. Demo Runner Failure Reporting Regression
// ---------------------------------------------------------------------------
void testDemoRunnerFailureReportingRegression()
{
    // 1. Failure Scenario: Insufficient Stock for candidate order (Step 7 failure)
    {
        SupplyChainPlatform platform(101, 16);
        cli::seedDemoPlatform(platform);

        // Deplete stock for SKU-LAPTOP so order ORD-1001 cannot be fulfilled
        bool updated = platform.getInventoryManager().updateProductQuantity("SKU-LAPTOP", 0);
        TEST_CHECK(updated);

        std::ostringstream out;
        bool ok = cli::runFullDemonstration(platform, out);

        TEST_CHECK(!ok);
        std::string text = out.str();
        TEST_CHECK(text.find("[SUCCESS] ALL INTEGRATED MODULES DEMONSTRATED WITH 100% PASSING INVARIANTS") == std::string::npos);
        TEST_CHECK(text.find("[FAILURE] DEMONSTRATION WORKFLOW FAILED") != std::string::npos);
        TEST_CHECK(text.find("Step 7 - Guarded Order Dispatch and Fulfillment") != std::string::npos);
        TEST_CHECK(text.find("fulfillment failed") != std::string::npos || text.find("Insufficient stock") != std::string::npos);
    }

    // 2. Failure Scenario: Cargo-packing prevalidation failure (Step 6 failure)
    {
        SupplyChainPlatform platform(101, 16);
        cli::seedDemoPlatform(platform);

        // Re-register SKU-LAPTOP packaging profile with oversized dimensions that exceed container (120x80x80)
        platform.registerPackagingProfile("SKU-LAPTOP", PackagingProfile(200, 200, 200, 50.0));

        std::ostringstream out;
        bool ok = cli::runFullDemonstration(platform, out);

        TEST_CHECK(!ok);
        std::string text = out.str();
        TEST_CHECK(text.find("[SUCCESS] ALL INTEGRATED MODULES DEMONSTRATED WITH 100% PASSING INVARIANTS") == std::string::npos);
        TEST_CHECK(text.find("[FAILURE] DEMONSTRATION WORKFLOW FAILED") != std::string::npos);
        TEST_CHECK(text.find("Step 6 - Cargo Packing Prevalidation") != std::string::npos);
    }

    // 3. Failure Scenario: Route prevalidation failure (Step 5 failure)
    {
        SupplyChainPlatform platform(101, 16);
        cli::seedDemoPlatform(platform);

        // Move SKU-LAPTOP to an isolated bin that cannot reach packing station
        auto& graph = platform.getWarehouseGraph();
        graph.addVertex("ISOLATED_BIN");
        auto& inv = platform.getInventoryManager();
        TEST_CHECK(inv.removeProduct("SKU-LAPTOP"));
        TEST_CHECK(inv.addProduct(Product("SKU-LAPTOP", "Enterprise Laptop 15", "Electronics", 50, 1200.0, "WH-1", "ISOLATED_BIN")));

        std::ostringstream out;
        bool ok = cli::runFullDemonstration(platform, out);

        TEST_CHECK(!ok);
        std::string text = out.str();
        TEST_CHECK(text.find("[SUCCESS] ALL INTEGRATED MODULES DEMONSTRATED WITH 100% PASSING INVARIANTS") == std::string::npos);
        TEST_CHECK(text.find("[FAILURE] DEMONSTRATION WORKFLOW FAILED") != std::string::npos);
        TEST_CHECK(text.find("Step 5 - Route Prevalidation") != std::string::npos);
    }

    // 4. Successful Seeded Scenario: All critical steps succeed, producing success banner
    {
        SupplyChainPlatform platform(101, 16);
        cli::seedDemoPlatform(platform);

        std::ostringstream out;
        bool ok = cli::runFullDemonstration(platform, out);

        TEST_CHECK(ok);
        std::string text = out.str();
        TEST_CHECK(text.find("[SUCCESS] ALL INTEGRATED MODULES DEMONSTRATED WITH 100% PASSING INVARIANTS") != std::string::npos);
        TEST_CHECK(text.find("[FAILURE]") == std::string::npos);
    }
}

// ---------------------------------------------------------------------------
// Main test runner
// ---------------------------------------------------------------------------
int main()
{
    TEST_RUN(testSuccessfulMultiBinRoute);
    TEST_RUN(testMissingEndpointVertices);
    TEST_RUN(testMissingProductBinVertex);
    TEST_RUN(testUnreachableRouteLeg);
    TEST_RUN(testRepeatedBinDeduplication);
    TEST_RUN(testMissingAndEmptyOrder);
    TEST_RUN(testPackageQuantityExpansion);
    TEST_RUN(testMissingPackagingProfile);
    TEST_RUN(testPackingOverflowInfeasible);
    TEST_RUN(testPrevalidationNonMutationInvariant);
    TEST_RUN(testRouteCostOverflowRegression);
    TEST_RUN(testLifecycleStatusPolicy);
    TEST_RUN(testGuardedDispatchSuccessful);
    TEST_RUN(testGuardedDispatchEmptyQueue);
    TEST_RUN(testGuardedDispatchMissingPackagingProfileRejection);
    TEST_RUN(testGuardedDispatchRoutePrevalidationFailureRejection);
    TEST_RUN(testGuardedDispatchPackingPrevalidationFailureRejection);
    TEST_RUN(testGuardedDispatchFulfillmentFailureBehavior);
    TEST_RUN(testGuardedDispatchSingleOrderAtATime);
    TEST_RUN(testGuardedDispatchPrecheckAndCancelledSkipping);
    TEST_RUN(testGuardedDispatchRecheckFailureCancelledSoleOrder);
    TEST_RUN(testGuardedDispatchRecheckFailureCancelledWithStandbyOrder);
    TEST_RUN(testGuardedDispatchRecheckFailurePreemption);
    TEST_RUN(testEndToEndDemonstrationWorkflow);
    TEST_RUN(testDemoRunnerFailureReportingRegression);

    TEST_REPORT();
}
