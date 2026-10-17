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
