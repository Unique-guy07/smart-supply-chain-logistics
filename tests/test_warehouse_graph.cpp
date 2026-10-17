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
    TEST_REPORT();
}

