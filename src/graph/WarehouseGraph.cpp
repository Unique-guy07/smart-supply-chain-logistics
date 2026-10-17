#include "graph/WarehouseGraph.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string_view>
#include <utility>

bool WarehouseGraph::isBlank(std::string_view str) noexcept
{
    if (str.empty()) {
        return true;
    }
    for (char c : str) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

void WarehouseGraph::validateIndex(std::size_t index) const
{
    if (index >= vertices.size()) {
        throw std::out_of_range("WarehouseGraph: vertex index out of range: " + std::to_string(index)
                                + " (vertex count is " + std::to_string(vertices.size()) + ")");
    }
}

WarehouseGraph::WarehouseGraph(std::size_t numVertices)
{
    vertices.reserve(numVertices);
    adj.resize(numVertices);
    for (std::size_t i = 0; i < numVertices; ++i) {
        std::string defaultId = std::to_string(i);
        vertices.push_back({defaultId, i});
        identifierToIndex[defaultId] = i;
    }
}

std::size_t WarehouseGraph::addVertex(const std::string& identifier)
{
    if (isBlank(identifier)) {
        throw std::invalid_argument("WarehouseGraph: vertex identifier must not be empty or blank");
    }
    if (identifierToIndex.contains(identifier)) {
        throw std::invalid_argument("WarehouseGraph: duplicate vertex identifier: " + identifier);
    }

    const std::size_t newIndex = vertices.size();

    // Step 1: Insert into identifier lookup map.
    // If try_emplace throws std::bad_alloc, identifierToIndex is unchanged,
    // and neither vertices nor adj has been modified.
    auto [mapIt, inserted] = identifierToIndex.try_emplace(identifier, newIndex);
    if (!inserted) {
        throw std::invalid_argument("WarehouseGraph: duplicate vertex identifier: " + identifier);
    }

    // Step 2: Push into vertices record vector.
    // If push_back throws, roll back map insertion. std::unordered_map::erase(iterator) is noexcept.
    try {
        vertices.push_back({identifier, newIndex});
    } catch (...) {
        identifierToIndex.erase(mapIt);
        throw;
    }

    // Step 3: Emplace into adjacency list vector.
    // If emplace_back throws, roll back vertices (pop_back is noexcept) and map insertion.
    try {
        adj.emplace_back();
    } catch (...) {
        vertices.pop_back();
        identifierToIndex.erase(mapIt);
        throw;
    }

    return newIndex;
}

bool WarehouseGraph::hasVertex(const std::string& identifier) const noexcept
{
    return identifierToIndex.contains(identifier);
}

bool WarehouseGraph::hasVertex(std::size_t index) const noexcept
{
    return index < vertices.size();
}

std::size_t WarehouseGraph::getVertexIndex(const std::string& identifier) const
{
    auto it = identifierToIndex.find(identifier);
    if (it == identifierToIndex.end()) {
        throw std::out_of_range("WarehouseGraph: unknown vertex identifier: " + identifier);
    }
    return it->second;
}

const std::string& WarehouseGraph::getVertexIdentifier(std::size_t index) const
{
    validateIndex(index);
    return vertices[index].identifier;
}

std::size_t WarehouseGraph::getVertexCount() const noexcept
{
    return vertices.size();
}

void WarehouseGraph::addDirectedEdge(std::size_t from, std::size_t to, double navigationWeight)
{
    validateIndex(from);
    validateIndex(to);

    if (navigationWeight < 0.0 || !std::isfinite(navigationWeight)) {
        throw std::invalid_argument("WarehouseGraph: navigation weight must be a non-negative finite number");
    }

    for (const auto& edge : adj[from]) {
        if (edge.to == to) {
            throw std::invalid_argument("WarehouseGraph: duplicate directed edge from " + std::to_string(from)
                                        + " to " + std::to_string(to));
        }
    }

    adj[from].push_back({to, navigationWeight});
    ++directedEdgeCount;
}

void WarehouseGraph::addDirectedEdge(const std::string& fromId, const std::string& toId, double navigationWeight)
{
    addDirectedEdge(getVertexIndex(fromId), getVertexIndex(toId), navigationWeight);
}

void WarehouseGraph::addBidirectionalAisle(std::size_t u, std::size_t v, double navigationWeight)
{
    validateIndex(u);
    validateIndex(v);

    if (u == v) {
        throw std::invalid_argument("WarehouseGraph: bidirectional aisle cannot be a self-loop");
    }
    if (navigationWeight < 0.0 || !std::isfinite(navigationWeight)) {
        throw std::invalid_argument("WarehouseGraph: navigation weight must be a non-negative finite number");
    }

    for (const auto& edge : adj[u]) {
        if (edge.to == v) {
            throw std::invalid_argument("WarehouseGraph: edge from " + std::to_string(u) + " to "
                                        + std::to_string(v) + " already exists");
        }
    }
    for (const auto& edge : adj[v]) {
        if (edge.to == u) {
            throw std::invalid_argument("WarehouseGraph: edge from " + std::to_string(v) + " to "
                                        + std::to_string(u) + " already exists");
        }
    }

    // Insert u -> v edge
    adj[u].push_back({v, navigationWeight});
    try {
        // Insert v -> u edge
        adj[v].push_back({u, navigationWeight});
    } catch (...) {
        // Roll back u -> v insertion. std::vector::pop_back() is noexcept.
        adj[u].pop_back();
        throw;
    }

    // Only update directedEdgeCount after both directional edges are safely inserted.
    directedEdgeCount += 2;
}

void WarehouseGraph::addBidirectionalAisle(const std::string& uId, const std::string& vId, double navigationWeight)
{
    addBidirectionalAisle(getVertexIndex(uId), getVertexIndex(vId), navigationWeight);
}

bool WarehouseGraph::hasDirectedEdge(std::size_t from, std::size_t to) const
{
    if (from >= vertices.size() || to >= vertices.size()) {
        return false;
    }
    for (const auto& edge : adj[from]) {
        if (edge.to == to) {
            return true;
        }
    }
    return false;
}

bool WarehouseGraph::hasDirectedEdge(const std::string& fromId, const std::string& toId) const
{
    if (!hasVertex(fromId) || !hasVertex(toId)) {
        return false;
    }
    return hasDirectedEdge(getVertexIndex(fromId), getVertexIndex(toId));
}

double WarehouseGraph::getDirectedEdgeWeight(std::size_t from, std::size_t to) const
{
    validateIndex(from);
    validateIndex(to);
    for (const auto& edge : adj[from]) {
        if (edge.to == to) {
            return edge.navigationWeight;
        }
    }
    throw std::invalid_argument("WarehouseGraph: directed edge does not exist from "
                                + std::to_string(from) + " to " + std::to_string(to));
}

const std::vector<DirectedEdge>& WarehouseGraph::getAdjacentEdges(std::size_t vertex) const
{
    validateIndex(vertex);
    return adj[vertex];
}

std::size_t WarehouseGraph::getDirectedEdgeCount() const noexcept
{
    return directedEdgeCount;
}

void WarehouseGraph::addInfrastructureLink(std::size_t u, std::size_t v, double installationCost)
{
    validateIndex(u);
    validateIndex(v);

    if (u == v) {
        throw std::invalid_argument("WarehouseGraph: infrastructure link cannot connect a vertex to itself");
    }
    if (installationCost < 0.0 || !std::isfinite(installationCost)) {
        throw std::invalid_argument("WarehouseGraph: installation cost must be a non-negative finite number");
    }

    std::size_t canU = std::min(u, v);
    std::size_t canV = std::max(u, v);

    for (const auto& link : infrastructureLinks) {
        if (link.u == canU && link.v == canV) {
            throw std::invalid_argument("WarehouseGraph: duplicate infrastructure link between "
                                        + std::to_string(canU) + " and " + std::to_string(canV));
        }
    }

    infrastructureLinks.push_back({canU, canV, installationCost});
}

void WarehouseGraph::addInfrastructureLink(const std::string& uId, const std::string& vId, double installationCost)
{
    addInfrastructureLink(getVertexIndex(uId), getVertexIndex(vId), installationCost);
}

const std::vector<InfrastructureEdge>& WarehouseGraph::getInfrastructureLinks() const noexcept
{
    return infrastructureLinks;
}

std::size_t WarehouseGraph::getInfrastructureLinkCount() const noexcept
{
    return infrastructureLinks.size();
}

std::vector<std::size_t> WarehouseGraph::reconstructPath(
    std::size_t source,
    std::size_t destination,
    const std::vector<std::size_t>& pred,
    std::size_t vertexCount)
{
    if (source >= vertexCount || destination >= vertexCount) {
        throw std::out_of_range("WarehouseGraph: source or destination index out of range for path reconstruction");
    }
    if (pred.size() != vertexCount) {
        throw std::invalid_argument("WarehouseGraph: predecessor vector size does not match vertexCount");
    }

    if (source == destination) {
        return {source};
    }

    if (pred[destination] == NO_PREDECESSOR) {
        return {};
    }

    Stack<std::size_t> pathStack;
    std::size_t curr = destination;
    std::vector<bool> cycleGuard(vertexCount, false);

    while (curr != source) {
        if (curr >= vertexCount || pred[curr] == NO_PREDECESSOR || cycleGuard[curr]) {
            throw std::runtime_error("WarehouseGraph: malformed predecessor chain: cycle or invalid index detected");
        }
        cycleGuard[curr] = true;
        pathStack.push(curr);
        curr = pred[curr];
    }
    pathStack.push(source);

    std::vector<std::size_t> path;
    path.reserve(pathStack.getSize());
    std::size_t node = 0;
    while (pathStack.pop(node)) {
        path.push_back(node);
    }

    return path;
}

BfsResult WarehouseGraph::findShortestPathBfs(std::size_t source, std::size_t destination) const
{
    validateIndex(source);
    validateIndex(destination);

    if (source == destination) {
        return BfsResult{
            .hasPath = true,
            .hopCount = 0,
            .path = {source},
            .route = {getVertexIdentifier(source)}
        };
    }

    const std::size_t n = vertices.size();
    std::vector<bool> visited(n, false);
    std::vector<std::size_t> pred(n, NO_PREDECESSOR);
    std::vector<std::size_t> dist(n, 0);

    Queue<std::size_t> queue;
    visited[source] = true;
    queue.enqueue(source);

    while (!queue.isEmpty()) {
        std::size_t u = 0;
        queue.dequeue(u);

        if (u == destination) {
            break;
        }

        for (const auto& edge : adj[u]) {
            std::size_t v = edge.to;
            if (!visited[v]) {
                visited[v] = true;
                pred[v] = u;
                dist[v] = dist[u] + 1;
                queue.enqueue(v);
            }
        }
    }

    if (!visited[destination]) {
        return BfsResult{
            .hasPath = false,
            .hopCount = 0,
            .path = {},
            .route = {}
        };
    }

    std::vector<std::size_t> path = reconstructPath(source, destination, pred, n);
    std::vector<std::string> route;
    route.reserve(path.size());
    for (std::size_t node : path) {
        route.push_back(getVertexIdentifier(node));
    }

    return BfsResult{
        .hasPath = true,
        .hopCount = dist[destination],
        .path = std::move(path),
        .route = std::move(route)
    };
}

BfsResult WarehouseGraph::findShortestPathBfs(const std::string& sourceId, const std::string& destId) const
{
    return findShortestPathBfs(getVertexIndex(sourceId), getVertexIndex(destId));
}

DijkstraResult WarehouseGraph::findShortestPathDijkstra(std::size_t source, std::size_t destination) const
{
    validateIndex(source);
    validateIndex(destination);

    if (source == destination) {
        return DijkstraResult{
            .hasPath = true,
            .totalCost = 0.0,
            .path = {source},
            .route = {getVertexIdentifier(source)}
        };
    }

    const std::size_t n = vertices.size();
    std::vector<double> dist(n, std::numeric_limits<double>::infinity());
    std::vector<std::size_t> pred(n, NO_PREDECESSOR);
    std::vector<bool> settled(n, false);

    struct DijkstraNode
    {
        std::size_t vertex{0};
        double distance{0.0};
    };

    struct DijkstraNodeComparator
    {
        bool operator()(const DijkstraNode& a, const DijkstraNode& b) const noexcept
        {
            if (b.distance != a.distance) {
                return b.distance < a.distance;
            }
            return b.vertex < a.vertex;
        }
    };

    using HeapType = FibonacciHeap<DijkstraNode, DijkstraNodeComparator>;
    HeapType heap;
    std::vector<HeapType::NodeHandle> handles(n);

    dist[source] = 0.0;
    handles[source] = heap.insert({source, 0.0});

    while (!heap.isEmpty()) {
        DijkstraNode current;
        heap.extract(current);
        const std::size_t u = current.vertex;

        if (settled[u]) {
            continue;
        }
        settled[u] = true;

        if (u == destination) {
            break;
        }

        if (std::isinf(dist[u])) {
            break;
        }

        for (const auto& edge : adj[u]) {
            if (edge.navigationWeight < 0.0 || !std::isfinite(edge.navigationWeight)) {
                throw std::invalid_argument("WarehouseGraph: negative or non-finite edge weight encountered in Dijkstra");
            }

            const std::size_t v = edge.to;
            if (settled[v]) {
                continue;
            }

            const double newDist = dist[u] + edge.navigationWeight;
            if (!std::isfinite(newDist)) {
                continue;
            }

            if (newDist < dist[v]) {
                dist[v] = newDist;
                pred[v] = u;
                if (!handles[v].isValid()) {
                    handles[v] = heap.insert({v, newDist});
                } else {
                    heap.decreaseKey(handles[v], {v, newDist});
                }
            }
        }
    }

    if (!settled[destination]) {
        return DijkstraResult{
            .hasPath = false,
            .totalCost = 0.0,
            .path = {},
            .route = {}
        };
    }

    std::vector<std::size_t> path = reconstructPath(source, destination, pred, n);
    std::vector<std::string> route;
    route.reserve(path.size());
    for (std::size_t node : path) {
        route.push_back(getVertexIdentifier(node));
    }

    return DijkstraResult{
        .hasPath = true,
        .totalCost = dist[destination],
        .path = std::move(path),
        .route = std::move(route)
    };
}

DijkstraResult WarehouseGraph::findShortestPathDijkstra(const std::string& sourceId, const std::string& destId) const
{
    return findShortestPathDijkstra(getVertexIndex(sourceId), getVertexIndex(destId));
}

SpanningForestResult WarehouseGraph::computeMstPrim() const
{
    const std::size_t n = vertices.size();
    if (n == 0) {
        return SpanningForestResult{
            .edges = {},
            .totalCost = 0.0,
            .componentCount = 0,
            .isSingleSpanningTree = false,
            .componentIds = {}
        };
    }

    struct PrimAdjEdge
    {
        std::size_t to{0};
        double cost{0.0};
    };

    std::vector<std::vector<PrimAdjEdge>> infraAdj(n);
    for (const auto& link : infrastructureLinks) {
        infraAdj[link.u].push_back({link.v, link.installationCost});
        infraAdj[link.v].push_back({link.u, link.installationCost});
    }

    for (std::size_t i = 0; i < n; ++i) {
        std::sort(infraAdj[i].begin(), infraAdj[i].end(), [](const PrimAdjEdge& a, const PrimAdjEdge& b) {
            if (a.cost != b.cost) {
                return a.cost < b.cost;
            }
            return a.to < b.to;
        });
    }

    struct PrimCandidateEdge
    {
        std::size_t u{0};
        std::size_t v{0};
        double cost{0.0};
    };

    struct PrimCandidateEdgeGreater
    {
        bool operator()(const PrimCandidateEdge& a, const PrimCandidateEdge& b) const noexcept
        {
            if (a.cost != b.cost) {
                return a.cost > b.cost;
            }
            std::size_t aMin = std::min(a.u, a.v);
            std::size_t aMax = std::max(a.u, a.v);
            std::size_t bMin = std::min(b.u, b.v);
            std::size_t bMax = std::max(b.u, b.v);
            if (aMin != bMin) {
                return aMin > bMin;
            }
            return aMax > bMax;
        }
    };

    std::vector<bool> inTree(n, false);
    std::vector<InfrastructureEdge> selectedEdges;
    selectedEdges.reserve(n > 1 ? n - 1 : 0);
    double totalCost = 0.0;
    std::size_t componentCount = 0;
    std::vector<std::size_t> componentIds(n, 0);

    for (std::size_t s = 0; s < n; ++s) {
        if (inTree[s]) {
            continue;
        }

        const std::size_t currCompId = componentCount++;
        inTree[s] = true;
        componentIds[s] = currCompId;

        MinHeap<PrimCandidateEdge, PrimCandidateEdgeGreater> heap;
        for (const auto& edge : infraAdj[s]) {
            if (!inTree[edge.to]) {
                heap.insert({s, edge.to, edge.cost});
            }
        }

        while (!heap.isEmpty()) {
            PrimCandidateEdge cand;
            heap.extractMin(cand);

            if (inTree[cand.v]) {
                continue;
            }

            inTree[cand.v] = true;
            componentIds[cand.v] = currCompId;

            const double candidateTotal = totalCost + cand.cost;
            if (!std::isfinite(candidateTotal)) {
                throw std::overflow_error("WarehouseGraph: MST aggregate installation cost exceeded finite floating-point range");
            }

            std::size_t normU = std::min(cand.u, cand.v);
            std::size_t normV = std::max(cand.u, cand.v);
            selectedEdges.push_back({normU, normV, cand.cost});
            totalCost = candidateTotal;

            for (const auto& nextEdge : infraAdj[cand.v]) {
                if (!inTree[nextEdge.to]) {
                    heap.insert({cand.v, nextEdge.to, nextEdge.cost});
                }
            }
        }
    }

    std::sort(selectedEdges.begin(), selectedEdges.end(), [](const InfrastructureEdge& a, const InfrastructureEdge& b) {
        if (a.installationCost != b.installationCost) {
            return a.installationCost < b.installationCost;
        }
        if (a.u != b.u) {
            return a.u < b.u;
        }
        return a.v < b.v;
    });

    const bool isSingleTree = (n > 0 && componentCount == 1);

    return SpanningForestResult{
        .edges = std::move(selectedEdges),
        .totalCost = totalCost,
        .componentCount = componentCount,
        .isSingleSpanningTree = isSingleTree,
        .componentIds = std::move(componentIds)
    };
}

SpanningForestResult WarehouseGraph::computeMstKruskal() const
{
    const std::size_t n = vertices.size();
    if (n == 0) {
        return SpanningForestResult{
            .edges = {},
            .totalCost = 0.0,
            .componentCount = 0,
            .isSingleSpanningTree = false,
            .componentIds = {}
        };
    }

    std::vector<InfrastructureEdge> sortedEdges = infrastructureLinks;
    std::sort(sortedEdges.begin(), sortedEdges.end(), [](const InfrastructureEdge& a, const InfrastructureEdge& b) {
        if (a.installationCost != b.installationCost) {
            return a.installationCost < b.installationCost;
        }
        std::size_t aMin = std::min(a.u, a.v);
        std::size_t aMax = std::max(a.u, a.v);
        std::size_t bMin = std::min(b.u, b.v);
        std::size_t bMax = std::max(b.u, b.v);
        if (aMin != bMin) {
            return aMin < bMin;
        }
        return aMax < bMax;
    });

    DisjointSet ds(n);
    std::vector<InfrastructureEdge> selectedEdges;
    selectedEdges.reserve(n > 1 ? n - 1 : 0);
    double totalCost = 0.0;

    for (const auto& edge : sortedEdges) {
        if (ds.unionSets(edge.u, edge.v)) {
            const double candidateTotal = totalCost + edge.installationCost;
            if (!std::isfinite(candidateTotal)) {
                throw std::overflow_error("WarehouseGraph: MST aggregate installation cost exceeded finite floating-point range");
            }

            std::size_t normU = std::min(edge.u, edge.v);
            std::size_t normV = std::max(edge.u, edge.v);
            selectedEdges.push_back({normU, normV, edge.installationCost});
            totalCost = candidateTotal;
            if (selectedEdges.size() == n - 1) {
                break;
            }
        }
    }

    std::sort(selectedEdges.begin(), selectedEdges.end(), [](const InfrastructureEdge& a, const InfrastructureEdge& b) {
        if (a.installationCost != b.installationCost) {
            return a.installationCost < b.installationCost;
        }
        if (a.u != b.u) {
            return a.u < b.u;
        }
        return a.v < b.v;
    });

    const std::size_t componentCount = ds.getSetCount();
    const bool isSingleTree = (n > 0 && componentCount == 1);

    std::unordered_map<std::size_t, std::size_t> rootToComponent;
    std::vector<std::size_t> componentIds(n);
    std::size_t nextCompId = 0;
    for (std::size_t i = 0; i < n; ++i) {
        std::size_t root = ds.find(i);
        auto it = rootToComponent.find(root);
        if (it == rootToComponent.end()) {
            rootToComponent[root] = nextCompId;
            componentIds[i] = nextCompId;
            ++nextCompId;
        } else {
            componentIds[i] = it->second;
        }
    }

    return SpanningForestResult{
        .edges = std::move(selectedEdges),
        .totalCost = totalCost,
        .componentCount = componentCount,
        .isSingleSpanningTree = isSingleTree,
        .componentIds = std::move(componentIds)
    };
}
