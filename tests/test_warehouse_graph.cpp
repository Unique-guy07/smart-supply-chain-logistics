#include "test_utils.h"
#include "graph/WarehouseGraph.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

void testConstructionAndVertexLookup()
{
    // Default empty graph
    WarehouseGraph gEmpty;
    TEST_CHECK(gEmpty.getVertexCount() == 0);
    TEST_CHECK(gEmpty.getDirectedEdgeCount() == 0);
    TEST_CHECK(gEmpty.getInfrastructureLinkCount() == 0);
    TEST_CHECK(!gEmpty.hasVertex(0));
    TEST_CHECK(!gEmpty.hasVertex("DockA"));

    // Pre-allocated graph
    WarehouseGraph gFixed(4);
    TEST_CHECK(gFixed.getVertexCount() == 4);
    TEST_CHECK(gFixed.hasVertex(0));
    TEST_CHECK(gFixed.hasVertex(3));
    TEST_CHECK(!gFixed.hasVertex(4));
    TEST_CHECK(gFixed.getVertexIdentifier(0) == "0");
    TEST_CHECK(gFixed.getVertexIdentifier(3) == "3");
    TEST_CHECK(gFixed.getVertexIndex("0") == 0);
    TEST_CHECK(gFixed.getVertexIndex("3") == 3);

    // Dynamic vertex addition
    WarehouseGraph g;
    std::size_t v0 = g.addVertex("ReceivingDock");
    std::size_t v1 = g.addVertex("Aisle_1");
    std::size_t v2 = g.addVertex("PackingStation");
    TEST_CHECK(v0 == 0);
    TEST_CHECK(v1 == 1);
    TEST_CHECK(v2 == 2);
    TEST_CHECK(g.getVertexCount() == 3);

    TEST_CHECK(g.hasVertex("ReceivingDock"));
    TEST_CHECK(g.hasVertex("Aisle_1"));
    TEST_CHECK(!g.hasVertex("ShippingBay"));

    TEST_CHECK(g.getVertexIndex("ReceivingDock") == 0);
    TEST_CHECK(g.getVertexIdentifier(0) == "ReceivingDock");

    // Duplicate vertex identifier rejection
    bool threw = false;
    try {
        g.addVertex("ReceivingDock");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Blank or empty vertex identifier rejection
    threw = false;
    try {
        g.addVertex("");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    threw = false;
    try {
        g.addVertex("   ");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Out-of-range index lookup
    threw = false;
    try {
        [[maybe_unused]] auto id = g.getVertexIdentifier(10);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Unknown identifier lookup
    threw = false;
    try {
        [[maybe_unused]] auto idx = g.getVertexIndex("NonExistent");
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);
}

void testEdgeCreationAndValidation()
{
    WarehouseGraph g;
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");

    // Valid directed edge
    g.addDirectedEdge(0, 1, 15.5);
    TEST_CHECK(g.hasDirectedEdge(0, 1));
    TEST_CHECK(!g.hasDirectedEdge(1, 0)); // Strictly directed
    TEST_CHECK(g.getDirectedEdgeWeight(0, 1) == 15.5);
    TEST_CHECK(g.getDirectedEdgeCount() == 1);

    // Valid bidirectional aisle
    g.addBidirectionalAisle(1, 2, 8.0);
    TEST_CHECK(g.hasDirectedEdge(1, 2));
    TEST_CHECK(g.hasDirectedEdge(2, 1));
    TEST_CHECK(g.getDirectedEdgeWeight(1, 2) == 8.0);
    TEST_CHECK(g.getDirectedEdgeWeight(2, 1) == 8.0);
    TEST_CHECK(g.getDirectedEdgeCount() == 3);

    // Duplicate directed edge rejection
    bool threw = false;
    try {
        g.addDirectedEdge(0, 1, 20.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Duplicate bidirectional edge rejection
    threw = false;
    try {
        g.addBidirectionalAisle(1, 2, 8.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Invalid weights: negative
    threw = false;
    try {
        g.addDirectedEdge(0, 2, -1.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Invalid weights: NaN
    threw = false;
    try {
        g.addDirectedEdge(0, 2, std::numeric_limits<double>::quiet_NaN());
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Invalid weights: Infinity
    threw = false;
    try {
        g.addDirectedEdge(0, 2, std::numeric_limits<double>::infinity());
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Self-loop in bidirectional aisle rejected
    threw = false;
    try {
        g.addBidirectionalAisle(0, 0, 5.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Out-of-bounds vertex index on edge creation
    threw = false;
    try {
        g.addDirectedEdge(0, 99, 10.0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // String identifier overload
    g.addDirectedEdge("A", "C", 25.0);
    TEST_CHECK(g.hasDirectedEdge("A", "C"));
    TEST_CHECK(g.getDirectedEdgeWeight(0, 2) == 25.0);
}

void testInfrastructureCostSeparation()
{
    WarehouseGraph g;
    g.addVertex("Node0");
    g.addVertex("Node1");

    // Add navigation edge with travel cost 2.5
    g.addDirectedEdge("Node0", "Node1", 2.5);

    // Add physical infrastructure candidate link with installation cost 50000.0
    g.addInfrastructureLink("Node0", "Node1", 50000.0);

    // Verify infrastructure links do not create reverse navigation edges
    TEST_CHECK(g.hasDirectedEdge(0, 1));
    TEST_CHECK(!g.hasDirectedEdge(1, 0));
    TEST_CHECK(g.getDirectedEdgeWeight(0, 1) == 2.5); // Navigation weight unchanged

    // Verify infrastructure links are tracked independently
    TEST_CHECK(g.getInfrastructureLinkCount() == 1);
    const auto& links = g.getInfrastructureLinks();
    TEST_CHECK(links.size() == 1);
    TEST_CHECK(links[0].u == 0);
    TEST_CHECK(links[0].v == 1);
    TEST_CHECK(links[0].installationCost == 50000.0);

    // Duplicate infrastructure link rejection
    bool threw = false;
    try {
        g.addInfrastructureLink("Node1", "Node0", 60000.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Self-loop in infrastructure link rejection
    threw = false;
    try {
        g.addInfrastructureLink(0, 0, 100.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);
}

void testBfsSimplePathAndHopMinimization()
{
    // Graph topology:
    // S -> A -> B -> D (3 hops, total weight 3.0)
    // S -> D (1 hop, total weight 100.0)
    // BFS must select S -> D (1 hop) because BFS minimizes edge hops, not navigation weights.
    WarehouseGraph g;
    g.addVertex("S");
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("D");

    g.addDirectedEdge("S", "A", 1.0);
    g.addDirectedEdge("A", "B", 1.0);
    g.addDirectedEdge("B", "D", 1.0);
    g.addDirectedEdge("S", "D", 100.0);

    BfsResult res = g.findShortestPathBfs("S", "D");
    TEST_CHECK(res.hasPath);
    TEST_CHECK(res.hopCount == 1);
    TEST_CHECK(res.path.size() == 2);
    TEST_CHECK(res.path[0] == 0);
    TEST_CHECK(res.path[1] == 3);
    TEST_CHECK(res.route.size() == 2);
    TEST_CHECK(res.route[0] == "S");
    TEST_CHECK(res.route[1] == "D");

    // Path from S to B
    BfsResult resB = g.findShortestPathBfs(0, 2);
    TEST_CHECK(resB.hasPath);
    TEST_CHECK(resB.hopCount == 2);
    TEST_CHECK(resB.path.size() == 3);
    TEST_CHECK(resB.path[0] == 0); // S
    TEST_CHECK(resB.path[1] == 1); // A
    TEST_CHECK(resB.path[2] == 2); // B
    TEST_CHECK(resB.route[0] == "S");
    TEST_CHECK(resB.route[1] == "A");
    TEST_CHECK(resB.route[2] == "B");
}

void testBfsSourceEqualsDestination()
{
    WarehouseGraph g;
    g.addVertex("Depot");
    g.addVertex("Rack1");
    g.addDirectedEdge("Depot", "Rack1", 10.0);

    BfsResult res = g.findShortestPathBfs("Depot", "Depot");
    TEST_CHECK(res.hasPath);
    TEST_CHECK(res.hopCount == 0);
    TEST_CHECK(res.path.size() == 1);
    TEST_CHECK(res.path[0] == 0);
    TEST_CHECK(res.route.size() == 1);
    TEST_CHECK(res.route[0] == "Depot");
}

void testBfsUnreachableAndDisconnected()
{
    WarehouseGraph g;
    g.addVertex("ZoneA_1");
    g.addVertex("ZoneA_2");
    g.addVertex("ZoneB_1");
    g.addVertex("ZoneB_2");

    g.addBidirectionalAisle("ZoneA_1", "ZoneA_2", 5.0);
    g.addBidirectionalAisle("ZoneB_1", "ZoneB_2", 5.0);
    // Zone A and Zone B are disconnected

    BfsResult res = g.findShortestPathBfs("ZoneA_1", "ZoneB_1");
    TEST_CHECK(!res.hasPath);
    TEST_CHECK(res.hopCount == 0);
    TEST_CHECK(res.path.empty());
    TEST_CHECK(res.route.empty());

    // Isolated single vertex in a graph
    g.addVertex("IsolatedIsland");
    BfsResult resIso = g.findShortestPathBfs("ZoneA_1", "IsolatedIsland");
    TEST_CHECK(!resIso.hasPath);
    TEST_CHECK(resIso.path.empty());
}

void testBfsDirectionalityAndCycles()
{
    WarehouseGraph g;
    g.addVertex("V0");
    g.addVertex("V1");
    g.addVertex("V2");

    // Directed cycle: V0 -> V1 -> V2 -> V0
    g.addDirectedEdge(0, 1, 1.0);
    g.addDirectedEdge(1, 2, 1.0);
    g.addDirectedEdge(2, 0, 1.0);

    // V0 to V2: forward cycle path is V0 -> V1 -> V2 (2 hops)
    BfsResult res02 = g.findShortestPathBfs(0, 2);
    TEST_CHECK(res02.hasPath);
    TEST_CHECK(res02.hopCount == 2);
    TEST_CHECK(res02.path.size() == 3);
    TEST_CHECK(res02.path[0] == 0);
    TEST_CHECK(res02.path[1] == 1);
    TEST_CHECK(res02.path[2] == 2);

    // V2 to V1: path is V2 -> V0 -> V1 (2 hops)
    BfsResult res21 = g.findShortestPathBfs(2, 1);
    TEST_CHECK(res21.hasPath);
    TEST_CHECK(res21.hopCount == 2);
    TEST_CHECK(res21.path[0] == 2);
    TEST_CHECK(res21.path[1] == 0);
    TEST_CHECK(res21.path[2] == 1);

    // Directed self-loop on V1: V1 -> V1
    g.addDirectedEdge(1, 1, 1.0);
    TEST_CHECK(g.hasDirectedEdge(1, 1));

    // Self-loop must not cause infinite loop or corrupt BFS traversal
    BfsResult resCycle = g.findShortestPathBfs(0, 2);
    TEST_CHECK(resCycle.hasPath);
    TEST_CHECK(resCycle.hopCount == 2);
    TEST_CHECK(resCycle.path[0] == 0);
    TEST_CHECK(resCycle.path[1] == 1);
    TEST_CHECK(resCycle.path[2] == 2);
}

void testBfsInvalidInputs()
{
    WarehouseGraph g;
    g.addVertex("A");
    g.addVertex("B");

    // Index out of bounds
    bool threw = false;
    try {
        [[maybe_unused]] auto res = g.findShortestPathBfs(0, 99);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    threw = false;
    try {
        [[maybe_unused]] auto res = g.findShortestPathBfs(99, 0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Identifier not found
    threw = false;
    try {
        [[maybe_unused]] auto res = g.findShortestPathBfs("A", "Z");
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);
}

void testReconstructPathDefensiveGuards()
{
    const std::size_t V = 4;
    const std::size_t NO_PRED = WarehouseGraph::NO_PREDECESSOR;

    // Normal valid predecessor chain: 0 -> 1 -> 2 -> 3
    // pred[1] = 0, pred[2] = 1, pred[3] = 2, pred[0] = NO_PRED
    std::vector<std::size_t> pred = {NO_PRED, 0, 1, 2};
    std::vector<std::size_t> path = WarehouseGraph::reconstructPath(0, 3, pred, V);
    TEST_CHECK(path.size() == 4);
    TEST_CHECK(path[0] == 0);
    TEST_CHECK(path[1] == 1);
    TEST_CHECK(path[2] == 2);
    TEST_CHECK(path[3] == 3);

    // Source equals destination
    std::vector<std::size_t> pathSelf = WarehouseGraph::reconstructPath(2, 2, pred, V);
    TEST_CHECK(pathSelf.size() == 1);
    TEST_CHECK(pathSelf[0] == 2);

    // Destination unreachable (pred is sentinel)
    std::vector<std::size_t> predUnreach = {NO_PRED, 0, NO_PRED, NO_PRED};
    std::vector<std::size_t> pathUnreach = WarehouseGraph::reconstructPath(0, 2, predUnreach, V);
    TEST_CHECK(pathUnreach.empty());

    // Out-of-bounds indices
    bool threw = false;
    try {
        [[maybe_unused]] auto p = WarehouseGraph::reconstructPath(0, 10, pred, V);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Size mismatch in predecessor array
    threw = false;
    try {
        std::vector<std::size_t> predShort = {NO_PRED, 0};
        [[maybe_unused]] auto p = WarehouseGraph::reconstructPath(0, 1, predShort, V);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Malformed predecessor chain: cycle detected (e.g. 1 -> 2 -> 1)
    std::vector<std::size_t> predCycle = {NO_PRED, 2, 1, 2};
    threw = false;
    try {
        [[maybe_unused]] auto p = WarehouseGraph::reconstructPath(0, 3, predCycle, V);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    TEST_CHECK(threw);
}

void testExceptionSafetyAndInvariants()
{
    WarehouseGraph g;
    std::size_t v0 = g.addVertex("V0");
    std::size_t v1 = g.addVertex("V1");
    std::size_t v2 = g.addVertex("V2");
    TEST_CHECK(v0 == 0);
    TEST_CHECK(v1 == 1);
    TEST_CHECK(v2 == 2);
    TEST_CHECK(g.getVertexCount() == 3);

    auto verifyInvariants = [](const WarehouseGraph& graph) {
        std::size_t totalEdges = 0;
        for (std::size_t i = 0; i < graph.getVertexCount(); ++i) {
            TEST_CHECK(graph.hasVertex(i));
            const std::string& id = graph.getVertexIdentifier(i);
            TEST_CHECK(graph.hasVertex(id));
            TEST_CHECK(graph.getVertexIndex(id) == i);
            totalEdges += graph.getAdjacentEdges(i).size();
        }
        TEST_CHECK(totalEdges == graph.getDirectedEdgeCount());
    };

    verifyInvariants(g);

    // 1. addVertex transactional behavior upon failure
    // Duplicate identifier
    bool threw = false;
    try {
        g.addVertex("V1");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);
    TEST_CHECK(g.getVertexCount() == 3);
    verifyInvariants(g);

    // Blank identifier
    threw = false;
    try {
        g.addVertex("   ");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);
    TEST_CHECK(g.getVertexCount() == 3);
    verifyInvariants(g);

    // Successful addVertex after failed attempts
    std::size_t v3 = g.addVertex("V3");
    TEST_CHECK(v3 == 3);
    TEST_CHECK(g.getVertexCount() == 4);
    verifyInvariants(g);

    // 2. addBidirectionalAisle transactional behavior upon failure
    // Establish a baseline directed edge V0 -> V1
    g.addDirectedEdge(v0, v1, 10.0);
    TEST_CHECK(g.getDirectedEdgeCount() == 1);
    verifyInvariants(g);

    // Attempting bidirectional aisle between V0 and V1 must fail because V0 -> V1 exists
    threw = false;
    try {
        g.addBidirectionalAisle(v0, v1, 20.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);
    // Invariant: no half-inserted reverse edge V1 -> V0, original weight unchanged, edge count unchanged
    TEST_CHECK(g.getDirectedEdgeCount() == 1);
    TEST_CHECK(!g.hasDirectedEdge(v1, v0));
    TEST_CHECK(g.getDirectedEdgeWeight(v0, v1) == 10.0);
    TEST_CHECK(g.getAdjacentEdges(v1).empty());
    verifyInvariants(g);

    // Attempting reverse call order V1 to V0 must also fail
    threw = false;
    try {
        g.addBidirectionalAisle(v1, v0, 25.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);
    TEST_CHECK(g.getDirectedEdgeCount() == 1);
    TEST_CHECK(!g.hasDirectedEdge(v1, v0));
    TEST_CHECK(g.getAdjacentEdges(v1).empty());
    verifyInvariants(g);

    // Self-loop failure
    threw = false;
    try {
        g.addBidirectionalAisle(v2, v2, 5.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);
    TEST_CHECK(g.getDirectedEdgeCount() == 1);
    TEST_CHECK(!g.hasDirectedEdge(v2, v2));
    verifyInvariants(g);

    // Out-of-bounds failure
    threw = false;
    try {
        g.addBidirectionalAisle(v2, 999, 5.0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);
    TEST_CHECK(g.getDirectedEdgeCount() == 1);
    verifyInvariants(g);

    // Valid bidirectional aisle between V2 and V3
    g.addBidirectionalAisle(v2, v3, 14.0);
    TEST_CHECK(g.getDirectedEdgeCount() == 3);
    TEST_CHECK(g.hasDirectedEdge(v2, v3));
    TEST_CHECK(g.hasDirectedEdge(v3, v2));
    TEST_CHECK(g.getDirectedEdgeWeight(v2, v3) == 14.0);
    TEST_CHECK(g.getDirectedEdgeWeight(v3, v2) == 14.0);
    verifyInvariants(g);
}

void testDijkstraWeightedRouting()
{
    // Topology:
    // S -> D directly with weight 100.0 (1 hop, cost 100.0)
    // S -> A -> B -> D with weights 2.0, 3.0, 4.0 (3 hops, cost 9.0)
    // BFS minimizes hops: chooses S -> D (1 hop, cost 100.0)
    // Dijkstra minimizes weight: chooses S -> A -> B -> D (3 hops, cost 9.0)
    WarehouseGraph g;
    g.addVertex("S");
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("D");

    g.addDirectedEdge("S", "D", 100.0);
    g.addDirectedEdge("S", "A", 2.0);
    g.addDirectedEdge("A", "B", 3.0);
    g.addDirectedEdge("B", "D", 4.0);

    // Verify BFS behavior
    BfsResult bfsRes = g.findShortestPathBfs("S", "D");
    TEST_CHECK(bfsRes.hasPath);
    TEST_CHECK(bfsRes.hopCount == 1);
    TEST_CHECK(bfsRes.path.size() == 2);
    TEST_CHECK(bfsRes.path[0] == 0 && bfsRes.path[1] == 3);

    // Verify Dijkstra behavior
    DijkstraResult dijkRes = g.findShortestPathDijkstra("S", "D");
    TEST_CHECK(dijkRes.hasPath);
    TEST_CHECK(dijkRes.totalCost == 9.0);
    TEST_CHECK(dijkRes.path.size() == 4);
    TEST_CHECK(dijkRes.path[0] == 0); // S
    TEST_CHECK(dijkRes.path[1] == 1); // A
    TEST_CHECK(dijkRes.path[2] == 2); // B
    TEST_CHECK(dijkRes.path[3] == 3); // D
    TEST_CHECK(dijkRes.route.size() == 4);
    TEST_CHECK(dijkRes.route[0] == "S");
    TEST_CHECK(dijkRes.route[1] == "A");
    TEST_CHECK(dijkRes.route[2] == "B");
    TEST_CHECK(dijkRes.route[3] == "D");

    // Integer index overload
    DijkstraResult idxRes = g.findShortestPathDijkstra(0, 3);
    TEST_CHECK(idxRes == dijkRes);
}

void testDijkstraDecreaseKeyAndComplexRelaxation()
{
    // Topology designed to trigger FibonacciHeap::decreaseKey:
    // S -> A (cost 10.0)   -- A discovered with tentative distance 10.0
    // S -> B (cost 1.0)
    // B -> A (cost 2.0)    -- relaxes A's tentative distance from 10.0 down to 3.0 via decreaseKey
    // A -> D (cost 1.0)    -- reaches destination with total cost 4.0
    // S -> C (cost 4.0)
    // C -> D (cost 2.0)    -- alternative path S -> C -> D has cost 6.0 > 4.0
    WarehouseGraph g;
    g.addVertex("S");
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");
    g.addVertex("D");

    g.addDirectedEdge("S", "A", 10.0);
    g.addDirectedEdge("S", "B", 1.0);
    g.addDirectedEdge("B", "A", 2.0);
    g.addDirectedEdge("A", "D", 1.0);
    g.addDirectedEdge("S", "C", 4.0);
    g.addDirectedEdge("C", "D", 2.0);

    DijkstraResult res = g.findShortestPathDijkstra("S", "D");
    TEST_CHECK(res.hasPath);
    TEST_CHECK(res.totalCost == 4.0);
    TEST_CHECK(res.path.size() == 4);
    TEST_CHECK(res.path[0] == 0); // S
    TEST_CHECK(res.path[1] == 2); // B
    TEST_CHECK(res.path[2] == 1); // A
    TEST_CHECK(res.path[3] == 4); // D

    // Verify cumulative cost consistency along reconstructed path
    double accumulated = 0.0;
    for (std::size_t i = 0; i + 1 < res.path.size(); ++i) {
        accumulated += g.getDirectedEdgeWeight(res.path[i], res.path[i + 1]);
    }
    TEST_CHECK(accumulated == res.totalCost);
}

void testDijkstraDirectedCyclesAndSelfLoop()
{
    WarehouseGraph g;
    g.addVertex("V0");
    g.addVertex("V1");
    g.addVertex("V2");
    g.addVertex("V3"); // Disconnected isolated vertex

    // Directed cycle: V0 -> V1 -> V2 -> V0
    g.addDirectedEdge(0, 1, 1.5);
    g.addDirectedEdge(1, 2, 2.5);
    g.addDirectedEdge(2, 0, 3.5);

    // Directed self-loop on V1
    g.addDirectedEdge(1, 1, 0.5);

    // Forward route V0 -> V2
    DijkstraResult res02 = g.findShortestPathDijkstra(0, 2);
    TEST_CHECK(res02.hasPath);
    TEST_CHECK(res02.totalCost == 4.0);
    TEST_CHECK(res02.path.size() == 3);
    TEST_CHECK(res02.path[0] == 0 && res02.path[1] == 1 && res02.path[2] == 2);

    // Reverse route V2 -> V1
    DijkstraResult res21 = g.findShortestPathDijkstra(2, 1);
    TEST_CHECK(res21.hasPath);
    TEST_CHECK(res21.totalCost == 5.0); // 2 -> 0 (3.5) + 0 -> 1 (1.5) = 5.0
    TEST_CHECK(res21.path.size() == 3);
    TEST_CHECK(res21.path[0] == 2 && res21.path[1] == 0 && res21.path[2] == 1);

    // Source equals destination: zero-cost route
    DijkstraResult resSelf = g.findShortestPathDijkstra(1, 1);
    TEST_CHECK(resSelf.hasPath);
    TEST_CHECK(resSelf.totalCost == 0.0);
    TEST_CHECK(resSelf.path.size() == 1 && resSelf.path[0] == 1);
    TEST_CHECK(resSelf.route.size() == 1 && resSelf.route[0] == "V1");

    // Unreachable destination to disconnected V3
    DijkstraResult resUnreach = g.findShortestPathDijkstra(0, 3);
    TEST_CHECK(!resUnreach.hasPath);
    TEST_CHECK(resUnreach.totalCost == 0.0);
    TEST_CHECK(resUnreach.path.empty());
    TEST_CHECK(resUnreach.route.empty());

    // From isolated V3 to V0
    DijkstraResult resFromIso = g.findShortestPathDijkstra(3, 0);
    TEST_CHECK(!resFromIso.hasPath);
    TEST_CHECK(resFromIso.path.empty());
}

void testDijkstraTieBreakingAndInvalidInputs()
{
    WarehouseGraph g;
    g.addVertex("S");
    g.addVertex("A"); // index 1
    g.addVertex("B"); // index 2
    g.addVertex("D"); // index 3

    // Two symmetric paths with identical total cost:
    // Path 1: S -> A -> D (cost 5.0 + 5.0 = 10.0, intermediate vertex index 1)
    // Path 2: S -> B -> D (cost 5.0 + 5.0 = 10.0, intermediate vertex index 2)
    // Deterministic tie-breaking on vertex index (1 < 2) selects Path 1
    g.addDirectedEdge(0, 1, 5.0);
    g.addDirectedEdge(0, 2, 5.0);
    g.addDirectedEdge(1, 3, 5.0);
    g.addDirectedEdge(2, 3, 5.0);

    DijkstraResult res = g.findShortestPathDijkstra(0, 3);
    TEST_CHECK(res.hasPath);
    TEST_CHECK(res.totalCost == 10.0);
    TEST_CHECK(res.path.size() == 3);
    TEST_CHECK(res.path[0] == 0);
    TEST_CHECK(res.path[1] == 1); // Deterministically picks A (index 1)
    TEST_CHECK(res.path[2] == 3);

    // Invalid index lookups
    bool threw = false;
    try {
        [[maybe_unused]] auto r = g.findShortestPathDijkstra(0, 999);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    threw = false;
    try {
        [[maybe_unused]] auto r = g.findShortestPathDijkstra(999, 0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Invalid string identifier lookups
    threw = false;
    try {
        [[maybe_unused]] auto r = g.findShortestPathDijkstra("S", "NonExistent");
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);
}

void testPrimAndKruskalConnectedMst()
{
    // 4-vertex connected diamond topology with known optimal MST:
    // Links:
    //   (0, 1) cost 10.0
    //   (1, 2) cost 20.0
    //   (2, 3) cost 30.0
    //   (0, 3) cost 40.0
    //   (0, 2) cost 25.0
    // Expected optimal MST:
    //   Edges: (0, 1, 10.0), (1, 2, 20.0), (2, 3, 30.0)
    //   Total cost: 60.0
    //   Edges count: 3
    //   componentCount: 1, isSingleSpanningTree: true
    WarehouseGraph g;
    g.addVertex("Dock0");
    g.addVertex("Dock1");
    g.addVertex("Dock2");
    g.addVertex("Dock3");

    g.addInfrastructureLink("Dock0", "Dock1", 10.0);
    g.addInfrastructureLink("Dock1", "Dock2", 20.0);
    g.addInfrastructureLink("Dock2", "Dock3", 30.0);
    g.addInfrastructureLink("Dock0", "Dock3", 40.0);
    g.addInfrastructureLink("Dock0", "Dock2", 25.0);

    SpanningForestResult primRes = g.computeMstPrim();
    SpanningForestResult kruskalRes = g.computeMstKruskal();

    // Verify Prim
    TEST_CHECK(primRes.totalCost == 60.0);
    TEST_CHECK(primRes.componentCount == 1);
    TEST_CHECK(primRes.isSingleSpanningTree);
    TEST_CHECK(primRes.edges.size() == 3);
    TEST_CHECK(primRes.componentIds.size() == 4);
    for (std::size_t cid : primRes.componentIds) {
        TEST_CHECK(cid == 0);
    }

    // Verify Kruskal
    TEST_CHECK(kruskalRes.totalCost == 60.0);
    TEST_CHECK(kruskalRes.componentCount == 1);
    TEST_CHECK(kruskalRes.isSingleSpanningTree);
    TEST_CHECK(kruskalRes.edges.size() == 3);
    TEST_CHECK(kruskalRes.componentIds.size() == 4);
    for (std::size_t cid : kruskalRes.componentIds) {
        TEST_CHECK(cid == 0);
    }

    // Both algorithms must select the exact same optimal edges
    TEST_CHECK(primRes.edges == kruskalRes.edges);
    TEST_CHECK(primRes.edges[0] == InfrastructureEdge(0, 1, 10.0));
    TEST_CHECK(primRes.edges[1] == InfrastructureEdge(1, 2, 20.0));
    TEST_CHECK(primRes.edges[2] == InfrastructureEdge(2, 3, 30.0));
}

void testPrimAndKruskalCyclePreventionAndGreedyChoice()
{
    // Topology where a locally greedy choice would form a cycle or produce higher cost:
    // Vertices: 0, 1, 2, 3
    // Links:
    //   (0, 1) cost 5.0
    //   (0, 2) cost 10.0
    //   (1, 2) cost 8.0
    //   (2, 3) cost 2.0
    //   (1, 3) cost 7.0
    //
    // Candidate edges sorted by cost:
    //   (2, 3) = 2.0   -> Selected
    //   (0, 1) = 5.0   -> Selected
    //   (1, 3) = 7.0   -> Selected (connects {0, 1} with {2, 3})
    //   (1, 2) = 8.0   -> Cycle! Rejected
    //   (0, 2) = 10.0  -> Cycle! Rejected
    // Expected optimal MST:
    //   Edges: (2, 3, 2.0), (0, 1, 5.0), (1, 3, 7.0)
    //   Total cost: 2.0 + 5.0 + 7.0 = 14.0
    WarehouseGraph g;
    g.addVertex("V0");
    g.addVertex("V1");
    g.addVertex("V2");
    g.addVertex("V3");

    g.addInfrastructureLink(0, 1, 5.0);
    g.addInfrastructureLink(0, 2, 10.0);
    g.addInfrastructureLink(1, 2, 8.0);
    g.addInfrastructureLink(2, 3, 2.0);
    g.addInfrastructureLink(1, 3, 7.0);

    SpanningForestResult primRes = g.computeMstPrim();
    SpanningForestResult kruskalRes = g.computeMstKruskal();

    TEST_CHECK(primRes.totalCost == 14.0);
    TEST_CHECK(kruskalRes.totalCost == 14.0);
    TEST_CHECK(primRes.edges.size() == 3);
    TEST_CHECK(kruskalRes.edges.size() == 3);
    TEST_CHECK(primRes.componentCount == 1);
    TEST_CHECK(kruskalRes.componentCount == 1);
    TEST_CHECK(primRes.isSingleSpanningTree);
    TEST_CHECK(kruskalRes.isSingleSpanningTree);

    // Verify exact expected edges: (2, 3, 2.0), (0, 1, 5.0), (1, 3, 7.0)
    TEST_CHECK(primRes.edges == kruskalRes.edges);
    TEST_CHECK(kruskalRes.edges[0] == InfrastructureEdge(2, 3, 2.0));
    TEST_CHECK(kruskalRes.edges[1] == InfrastructureEdge(0, 1, 5.0));
    TEST_CHECK(kruskalRes.edges[2] == InfrastructureEdge(1, 3, 7.0));
}

void testPrimAndKruskalDisconnectedForest()
{
    // 6-vertex disconnected infrastructure layout:
    // Component 1: {0, 1, 2}
    //   Links: (0, 1, 5.0), (1, 2, 7.0), (0, 2, 12.0) -> MST cost = 5.0 + 7.0 = 12.0 (2 edges)
    // Component 2: {3, 4}
    //   Links: (3, 4, 15.0)                            -> MST cost = 15.0 (1 edge)
    // Component 3: {5} (isolated vertex)               -> MST cost = 0.0 (0 edges)
    //
    // Expected Spanning Forest:
    //   Total cost: 12.0 + 15.0 = 27.0
    //   Selected edges: 3 edges total
    //   componentCount: 3
    //   isSingleSpanningTree: false
    //   componentIds: [0, 0, 0, 1, 1, 2]
    WarehouseGraph g;
    g.addVertex("C1_A");
    g.addVertex("C1_B");
    g.addVertex("C1_C");
    g.addVertex("C2_A");
    g.addVertex("C2_B");
    g.addVertex("C3_Isolated");

    g.addInfrastructureLink(0, 1, 5.0);
    g.addInfrastructureLink(1, 2, 7.0);
    g.addInfrastructureLink(0, 2, 12.0);
    g.addInfrastructureLink(3, 4, 15.0);

    SpanningForestResult primRes = g.computeMstPrim();
    SpanningForestResult kruskalRes = g.computeMstKruskal();

    TEST_CHECK(primRes.totalCost == 27.0);
    TEST_CHECK(kruskalRes.totalCost == 27.0);

    TEST_CHECK(primRes.componentCount == 3);
    TEST_CHECK(kruskalRes.componentCount == 3);

    TEST_CHECK(!primRes.isSingleSpanningTree);
    TEST_CHECK(!kruskalRes.isSingleSpanningTree);

    TEST_CHECK(primRes.edges.size() == 3);
    TEST_CHECK(kruskalRes.edges.size() == 3);

    // Selected edges must agree
    TEST_CHECK(primRes.edges == kruskalRes.edges);
    TEST_CHECK(primRes.edges[0] == InfrastructureEdge(0, 1, 5.0));
    TEST_CHECK(primRes.edges[1] == InfrastructureEdge(1, 2, 7.0));
    TEST_CHECK(primRes.edges[2] == InfrastructureEdge(3, 4, 15.0));

    // Component membership must be identical and correctly partitioned
    TEST_CHECK(primRes.componentIds == kruskalRes.componentIds);
    TEST_CHECK(primRes.componentIds.size() == 6);
    TEST_CHECK(primRes.componentIds[0] == 0);
    TEST_CHECK(primRes.componentIds[1] == 0);
    TEST_CHECK(primRes.componentIds[2] == 0);
    TEST_CHECK(primRes.componentIds[3] == 1);
    TEST_CHECK(primRes.componentIds[4] == 1);
    TEST_CHECK(primRes.componentIds[5] == 2);
}

void testPrimAndKruskalBoundaryCases()
{
    // Case 1: Empty graph (V = 0)
    WarehouseGraph g0;
    SpanningForestResult p0 = g0.computeMstPrim();
    SpanningForestResult k0 = g0.computeMstKruskal();
    TEST_CHECK(p0.edges.empty() && k0.edges.empty());
    TEST_CHECK(p0.totalCost == 0.0 && k0.totalCost == 0.0);
    TEST_CHECK(p0.componentCount == 0 && k0.componentCount == 0);
    TEST_CHECK(!p0.isSingleSpanningTree && !k0.isSingleSpanningTree);
    TEST_CHECK(p0.componentIds.empty() && k0.componentIds.empty());

    // Case 2: Single-vertex graph (V = 1)
    WarehouseGraph g1;
    g1.addVertex("SingleDock");
    SpanningForestResult p1 = g1.computeMstPrim();
    SpanningForestResult k1 = g1.computeMstKruskal();
    TEST_CHECK(p1.edges.empty() && k1.edges.empty());
    TEST_CHECK(p1.totalCost == 0.0 && k1.totalCost == 0.0);
    TEST_CHECK(p1.componentCount == 1 && k1.componentCount == 1);
    TEST_CHECK(p1.isSingleSpanningTree && k1.isSingleSpanningTree);
    TEST_CHECK(p1.componentIds.size() == 1 && p1.componentIds[0] == 0);
    TEST_CHECK(k1.componentIds.size() == 1 && k1.componentIds[0] == 0);

    // Case 3: Vertices exist with navigation edges, but ZERO infrastructure links
    WarehouseGraph gNavOnly;
    gNavOnly.addVertex("A");
    gNavOnly.addVertex("B");
    gNavOnly.addVertex("C");
    gNavOnly.addBidirectionalAisle("A", "B", 5.0);
    gNavOnly.addBidirectionalAisle("B", "C", 5.0);

    SpanningForestResult pNav = gNavOnly.computeMstPrim();
    SpanningForestResult kNav = gNavOnly.computeMstKruskal();
    TEST_CHECK(pNav.edges.empty() && kNav.edges.empty());
    TEST_CHECK(pNav.totalCost == 0.0 && kNav.totalCost == 0.0);
    TEST_CHECK(pNav.componentCount == 3 && kNav.componentCount == 3);
    TEST_CHECK(!pNav.isSingleSpanningTree && !kNav.isSingleSpanningTree);
    TEST_CHECK(pNav.componentIds.size() == 3);
    TEST_CHECK(pNav.componentIds[0] == 0 && pNav.componentIds[1] == 1 && pNav.componentIds[2] == 2);
    TEST_CHECK(pNav.componentIds == kNav.componentIds);
}

void testCostSeparationNavigationVsInfrastructure()
{
    // Proves that Dijkstra uses ONLY navigation travel weights,
    // while Prim and Kruskal use ONLY physical installation costs.
    WarehouseGraph g;
    g.addVertex("Bay0");
    g.addVertex("Bay1");

    // High navigation travel cost (e.g. slow congested path)
    g.addDirectedEdge("Bay0", "Bay1", 999.0);

    // Low infrastructure installation cost (e.g. cheap cable link)
    g.addInfrastructureLink("Bay0", "Bay1", 12.5);

    // Dijkstra must report 999.0, ignoring 12.5
    DijkstraResult dijkRes = g.findShortestPathDijkstra("Bay0", "Bay1");
    TEST_CHECK(dijkRes.hasPath);
    TEST_CHECK(dijkRes.totalCost == 999.0);

    // Prim and Kruskal must report 12.5, ignoring 999.0
    SpanningForestResult primRes = g.computeMstPrim();
    SpanningForestResult kruskalRes = g.computeMstKruskal();
    TEST_CHECK(primRes.totalCost == 12.5);
    TEST_CHECK(kruskalRes.totalCost == 12.5);
    TEST_CHECK(primRes.edges.size() == 1);
    TEST_CHECK(primRes.edges[0].installationCost == 12.5);
}

void testPrimAndKruskalEqualCostDeterministic()
{
    // 4-vertex ring with equal installation cost 10.0 on all 4 candidate links:
    // (0, 1), (1, 2), (2, 3), (0, 3)
    // Any 3 edges form a valid spanning tree of cost 30.0.
    // Deterministic tie-breaking on endpoints guarantees reproducible selection.
    WarehouseGraph g;
    g.addVertex("N0");
    g.addVertex("N1");
    g.addVertex("N2");
    g.addVertex("N3");

    g.addInfrastructureLink(0, 1, 10.0);
    g.addInfrastructureLink(1, 2, 10.0);
    g.addInfrastructureLink(2, 3, 10.0);
    g.addInfrastructureLink(0, 3, 10.0);

    SpanningForestResult primRes = g.computeMstPrim();
    SpanningForestResult kruskalRes = g.computeMstKruskal();

    TEST_CHECK(primRes.totalCost == 30.0);
    TEST_CHECK(kruskalRes.totalCost == 30.0);
    TEST_CHECK(primRes.componentCount == 1);
    TEST_CHECK(kruskalRes.componentCount == 1);
    TEST_CHECK(primRes.isSingleSpanningTree);
    TEST_CHECK(kruskalRes.isSingleSpanningTree);
    TEST_CHECK(primRes.edges.size() == 3);
    TEST_CHECK(kruskalRes.edges.size() == 3);
    TEST_CHECK(primRes.edges == kruskalRes.edges);
}

void testPrimAndKruskalAggregateCostOverflow()
{
    // Minimal graph with 3 vertices: 0, 1, 2
    // Two selected infrastructure links: (0, 1) and (1, 2)
    // Each link has cost = std::numeric_limits<double>::max() (finite individual cost).
    // Summing them overflows finite double to +infinity.
    // Both Prim and Kruskal must throw std::overflow_error before returning or assigning totalCost.
    WarehouseGraph g;
    g.addVertex("V0");
    g.addVertex("V1");
    g.addVertex("V2");

    const double maxCost = std::numeric_limits<double>::max();
    g.addInfrastructureLink(0, 1, maxCost);
    g.addInfrastructureLink(1, 2, maxCost);

    // Test computeMstPrim throws std::overflow_error
    bool primThrewOverflow = false;
    try {
        [[maybe_unused]] auto res = g.computeMstPrim();
    } catch (const std::overflow_error&) {
        primThrewOverflow = true;
    }
    TEST_CHECK(primThrewOverflow);

    // Test computeMstKruskal throws std::overflow_error
    bool kruskalThrewOverflow = false;
    try {
        [[maybe_unused]] auto res = g.computeMstKruskal();
    } catch (const std::overflow_error&) {
        kruskalThrewOverflow = true;
    }
    TEST_CHECK(kruskalThrewOverflow);
}

int main()
{
    TEST_RUN(testConstructionAndVertexLookup);
    TEST_RUN(testEdgeCreationAndValidation);
    TEST_RUN(testInfrastructureCostSeparation);
    TEST_RUN(testBfsSimplePathAndHopMinimization);
    TEST_RUN(testBfsSourceEqualsDestination);
    TEST_RUN(testBfsUnreachableAndDisconnected);
    TEST_RUN(testBfsDirectionalityAndCycles);
    TEST_RUN(testBfsInvalidInputs);
    TEST_RUN(testReconstructPathDefensiveGuards);
    TEST_RUN(testExceptionSafetyAndInvariants);
    TEST_RUN(testDijkstraWeightedRouting);
    TEST_RUN(testDijkstraDecreaseKeyAndComplexRelaxation);
    TEST_RUN(testDijkstraDirectedCyclesAndSelfLoop);
    TEST_RUN(testDijkstraTieBreakingAndInvalidInputs);
    TEST_RUN(testPrimAndKruskalConnectedMst);
    TEST_RUN(testPrimAndKruskalCyclePreventionAndGreedyChoice);
    TEST_RUN(testPrimAndKruskalDisconnectedForest);
    TEST_RUN(testPrimAndKruskalBoundaryCases);
    TEST_RUN(testCostSeparationNavigationVsInfrastructure);
    TEST_RUN(testPrimAndKruskalEqualCostDeterministic);
    TEST_RUN(testPrimAndKruskalAggregateCostOverflow);
    TEST_REPORT();
}
