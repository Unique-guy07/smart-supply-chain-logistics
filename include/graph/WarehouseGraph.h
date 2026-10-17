#pragma once

#include "order/Queue.h"
#include "order/Stack.h"

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

/// Directed navigation edge in the warehouse layout.
/// Stores destination vertex index and operational travel/navigation weight.
struct DirectedEdge
{
    std::size_t to{0};
    double navigationWeight{0.0};

    bool operator==(const DirectedEdge& other) const noexcept
    {
        return to == other.to && navigationWeight == other.navigationWeight;
    }
};

/// Physical infrastructure candidate connection.
/// Installation cost is modeled separately from dynamic operational navigation weights.
struct InfrastructureEdge
{
    std::size_t u{0};
    std::size_t v{0};
    double installationCost{0.0};

    bool operator==(const InfrastructureEdge& other) const noexcept
    {
        return u == other.u && v == other.v && installationCost == other.installationCost;
    }
};

/// Result structure for Breadth-First Search (BFS) route generation.
struct BfsResult
{
    bool hasPath{false};
    std::size_t hopCount{0};
    std::vector<std::size_t> path;   ///< Vertex indices in forward order: [source, ..., destination]
    std::vector<std::string> route;  ///< Vertex identifiers in forward order: [sourceId, ..., destId]
};

/// Warehouse navigation graph storing spatial topology via adjacency lists.
///
/// Theoretical foundation & role:
/// Represents Module 7 spatial navigation network (PROJECT_SPEC.md Section 4 & 5).
/// Supports directed conveyors and bidirectional aisles for AGV and human picker travel.
/// Maintains distinct representations for operational navigation travel weights versus
/// capital infrastructure installation costs.
///
/// Algorithmic guarantees:
///   - Breadth-First Search (BFS): Computes unweighted minimum-hop routes in O(V + E) time.
///   - Route reconstruction: Backtracks via predecessor pointers using custom Stack<T> in O(L).
///
/// Exception safety & validations:
///   - Throws std::invalid_argument for blank identifiers, negative/non-finite weights,
///     and duplicate vertex/edge insertions.
///   - Throws std::out_of_range for out-of-bounds vertex index or unknown vertex identifier lookups.
///   - Move-only container semantics (copy explicitly deleted).
class WarehouseGraph
{
public:
    static constexpr std::size_t NO_PREDECESSOR = std::numeric_limits<std::size_t>::max();

private:
    struct VertexRecord
    {
        std::string identifier;
        std::size_t index{0};
    };

    std::vector<VertexRecord> vertices;
    std::unordered_map<std::string, std::size_t> identifierToIndex;
    std::vector<std::vector<DirectedEdge>> adj;
    std::vector<InfrastructureEdge> infrastructureLinks;
    std::size_t directedEdgeCount{0};

    void validateIndex(std::size_t index) const;
    static bool isBlank(std::string_view str) noexcept;

public:
    /// Constructs an empty WarehouseGraph.
    WarehouseGraph() = default;

    /// Constructs a WarehouseGraph pre-populated with numVertices default vertices ("0", "1", ...).
    explicit WarehouseGraph(std::size_t numVertices);

    ~WarehouseGraph() = default;

    // Move-only container semantics
    WarehouseGraph(const WarehouseGraph&) = delete;
    WarehouseGraph& operator=(const WarehouseGraph&) = delete;

    WarehouseGraph(WarehouseGraph&& other) noexcept = default;
    WarehouseGraph& operator=(WarehouseGraph&& other) noexcept = default;

    // --- Vertex Management ---

    /// Adds a new vertex with a unique non-blank identifier.
    /// @return The zero-based integer index assigned to the new vertex.
    /// @throws std::invalid_argument if identifier is empty, blank, or already exists.
    std::size_t addVertex(const std::string& identifier);

    /// Checks if a vertex identifier exists in the graph.
    [[nodiscard]] bool hasVertex(const std::string& identifier) const noexcept;

    /// Checks if a vertex index is valid in the graph.
    [[nodiscard]] bool hasVertex(std::size_t index) const noexcept;

    /// Returns the integer index corresponding to a vertex identifier.
    /// @throws std::out_of_range if identifier is not found.
    [[nodiscard]] std::size_t getVertexIndex(const std::string& identifier) const;

    /// Returns the string identifier corresponding to an integer vertex index.
    /// @throws std::out_of_range if index is out of bounds.
    [[nodiscard]] const std::string& getVertexIdentifier(std::size_t index) const;

    /// Returns the total number of vertices in the graph.
    [[nodiscard]] std::size_t getVertexCount() const noexcept;

    // --- Navigation Edge Management ---

    /// Adds a directed navigation connection (e.g. unidirectional conveyor or aisle).
    /// @throws std::out_of_range if from or to is an invalid vertex index.
    /// @throws std::invalid_argument if navigationWeight < 0.0, non-finite, or if a directed
    ///         edge from -> to already exists.
    void addDirectedEdge(std::size_t from, std::size_t to, double navigationWeight);

    /// Overload for adding a directed edge using string identifiers.
    void addDirectedEdge(const std::string& fromId, const std::string& toId, double navigationWeight);

    /// Adds a bidirectional navigation aisle with symmetric travel weights in both directions.
    /// @throws std::out_of_range if u or v is an invalid vertex index.
    /// @throws std::invalid_argument if navigationWeight < 0.0, non-finite, if u == v (self-loop),
    ///         or if an edge between u and v already exists in either direction.
    void addBidirectionalAisle(std::size_t u, std::size_t v, double navigationWeight);

    /// Overload for adding a bidirectional aisle using string identifiers.
    void addBidirectionalAisle(const std::string& uId, const std::string& vId, double navigationWeight);

    /// Checks if a directed navigation edge exists from -> to.
    [[nodiscard]] bool hasDirectedEdge(std::size_t from, std::size_t to) const;

    /// Overload for checking if a directed navigation edge exists using string identifiers.
    [[nodiscard]] bool hasDirectedEdge(const std::string& fromId, const std::string& toId) const;

    /// Returns the travel weight of the directed edge from -> to.
    /// @throws std::out_of_range if vertices are invalid.
    /// @throws std::invalid_argument if the directed edge does not exist.
    [[nodiscard]] double getDirectedEdgeWeight(std::size_t from, std::size_t to) const;

    /// Returns all outgoing directed edges from the given vertex index.
    /// @throws std::out_of_range if vertex index is invalid.
    [[nodiscard]] const std::vector<DirectedEdge>& getAdjacentEdges(std::size_t vertex) const;

    /// Returns the total number of directed edges in the navigation network.
    [[nodiscard]] std::size_t getDirectedEdgeCount() const noexcept;

    // --- Infrastructure Candidate Link Management ---

    /// Adds an undirected physical infrastructure connection with explicit installation cost.
    /// Kept completely separate from operational navigation weights.
    /// @throws std::out_of_range if u or v is an invalid vertex index.
    /// @throws std::invalid_argument if installationCost < 0.0, non-finite, if u == v,
    ///         or if an infrastructure link between u and v already exists.
    void addInfrastructureLink(std::size_t u, std::size_t v, double installationCost);

    /// Overload for adding an infrastructure link using string identifiers.
    void addInfrastructureLink(const std::string& uId, const std::string& vId, double installationCost);

    /// Returns all candidate physical infrastructure links.
    [[nodiscard]] const std::vector<InfrastructureEdge>& getInfrastructureLinks() const noexcept;

    /// Returns the total number of candidate infrastructure links.
    [[nodiscard]] std::size_t getInfrastructureLinkCount() const noexcept;

    // --- BFS Minimum-Hop Routing ---

    /// Calculates the minimum-hop route from source to destination using Breadth-First Search (BFS).
    /// Uses Queue<T> for exploration and Stack<T> for predecessor route reconstruction.
    /// @throws std::out_of_range if source or destination is out of bounds.
    [[nodiscard]] BfsResult findShortestPathBfs(std::size_t source, std::size_t destination) const;

    /// Overload for BFS routing using string location identifiers.
    /// @throws std::out_of_range if sourceId or destId is not found.
    [[nodiscard]] BfsResult findShortestPathBfs(const std::string& sourceId, const std::string& destId) const;

    // --- Static Predecessor Reconstruction Helper ---

    /// Reconstructs a forward route from predecessor links using custom Stack<T>.
    /// Guards against invalid sentinels, out-of-bounds indices, and malformed predecessor cycles.
    /// @return Forward path [source, ..., destination]. If destination is unreachable, returns empty vector.
    /// @throws std::out_of_range if source or destination >= vertexCount.
    /// @throws std::invalid_argument if pred.size() != vertexCount.
    /// @throws std::runtime_error if a cyclic or malformed predecessor chain is encountered.
    [[nodiscard]] static std::vector<std::size_t> reconstructPath(
        std::size_t source,
        std::size_t destination,
        const std::vector<std::size_t>& pred,
        std::size_t vertexCount);
};
