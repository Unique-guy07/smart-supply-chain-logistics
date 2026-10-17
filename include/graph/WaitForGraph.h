#pragma once

#include "order/Stack.h"
#include "task/PriorityTask.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

/// Result structure for Wait-For Graph directed deadlock cycle detection.
/// Represents whether a deadlock exists, the sequence of AGV identifiers forming
/// the cycle with its closing dependency, and the corresponding internal vertex indices.
struct DeadlockCycle
{
    bool hasDeadlock{false};
    std::vector<std::string> cycle;        ///< AGV IDs in forward cycle order with closing dependency: [A, B, C, A]
    std::vector<std::size_t> cycleIndices;  ///< Corresponding internal vertex indices: [u, v, w, u]

    bool operator==(const DeadlockCycle& other) const noexcept
    {
        return hasDeadlock == other.hasDeadlock
            && cycle == other.cycle
            && cycleIndices == other.cycleIndices;
    }
};

/// Wait-For Graph (WFG) modeling AGV waiting dependencies and deadlock cycles (Module 7).
///
/// Theoretical foundation & role:
/// Represents directed blocking dependencies between Automated Guided Vehicles (AGVs).
/// A directed edge `AGV_A -> AGV_B` signifies that AGV_A is waiting for AGV_B to release
/// a spatial aisle, resource, or intersection (PROJECT_SPEC.md Section 4, 5 & 7).
/// A directed cycle indicates a circular wait (deadlock) where none of the involved AGVs can proceed.
///
/// Algorithmic guarantees:
///   - Directed-cycle detection: Uses Depth-First Search (DFS) with three-color states
///     (White = Unvisited, Gray = Visiting on recursion stack, Black = Completed).
///     Guarantees detection of cycles of length >= 2 in O(V + E) time, avoiding false positives
///     when encountering completed (Black) nodes.
///   - Cycle extraction: Reconstructs the exact directed cycle path using custom Stack<T>
///     backtracking along parent predecessor pointers in O(C) time (where C is cycle length).
///   - Deterministic traversal: Evaluates vertices in ascending index order (0 to V-1), exploring
///     outgoing dependency edges in insertion order. When multiple cycles exist, the first cycle
///     encountered by this traversal order is deterministically extracted.
///   - Deterministic victim selection: Selects the AGV in the detected cycle with the lowest
///     TaskPriority value (TaskPriority::Low = 0 yields first). Ties are broken deterministically
///     by selecting the lexicographically smallest AGV identifier.
///   - Non-destructive detection: Cycle detection and victim selection do NOT mutate graph state,
///     remove dependencies, or alter physical vehicle states.
///
/// Complexity:
///   - Space: O(V + E) for adjacency list, AGV records, and identifier lookup map.
///   - Vertex registration / lookup: O(1) amortized time.
///   - Dependency addition / removal: O(deg(u)) time.
///   - Deadlock detection: O(V + E) time.
///   - Victim selection: O(C) time, where C <= V is the number of vertices in the cycle.
///
/// Exception safety & validations:
///   - Throws std::invalid_argument for blank AGV IDs, duplicate AGV IDs, self-dependencies,
///     or duplicate dependencies.
///   - Throws std::out_of_range for unregistered AGV lookups or out-of-bounds vertex indices.
///   - Throws std::runtime_error when victim selection is attempted on a graph or cycle with no deadlock.
///   - Provides strong exception guarantee for vertex and dependency insertions.
class WaitForGraph
{
public:
    static constexpr std::size_t NO_PREDECESSOR = std::numeric_limits<std::size_t>::max();

private:
    struct AgvRecord
    {
        std::string identifier;
        std::size_t index{0};
        TaskPriority priority{TaskPriority::Normal};
    };

    enum class Color : std::uint8_t
    {
        White = 0, ///< Unvisited vertex
        Gray = 1,  ///< Vertex currently on active DFS recursion path
        Black = 2  ///< Vertex completely processed; subtree has no active back-edges
    };

    std::vector<AgvRecord> agvRecords;
    std::unordered_map<std::string, std::size_t> identifierToIndex;
    std::vector<std::vector<std::size_t>> adj;
    std::size_t dependencyCount{0};

    void validateIndex(std::size_t index) const;
    static bool isBlank(std::string_view str) noexcept;

    bool dfsCycle(std::size_t u,
                  std::vector<Color>& color,
                  std::vector<std::size_t>& parent,
                  DeadlockCycle& result) const;

public:
    /// Constructs an empty WaitForGraph.
    WaitForGraph() = default;

    /// Constructs a WaitForGraph pre-reserving storage for an estimated number of AGVs.
    explicit WaitForGraph(std::size_t expectedAgvCount);

    ~WaitForGraph() = default;

    // Move-only container semantics consistent with WarehouseGraph
    WaitForGraph(const WaitForGraph&) = delete;
    WaitForGraph& operator=(const WaitForGraph&) = delete;

    WaitForGraph(WaitForGraph&& other) noexcept = default;
    WaitForGraph& operator=(WaitForGraph&& other) noexcept = default;

    // --- AGV Registration and Lookup ---

    /// Registers a new AGV with a unique non-blank identifier and initial task priority.
    /// @return The zero-based integer index assigned to the new AGV.
    /// @throws std::invalid_argument if agvId is empty, blank, already registered, or if
    ///         priority is not a valid TaskPriority value.
    std::size_t addAgv(const std::string& agvId, TaskPriority priority = TaskPriority::Normal);

    /// Checks if an AGV identifier exists in the graph.
    [[nodiscard]] bool hasAgv(const std::string& agvId) const noexcept;

    /// Checks if a vertex index is valid in the graph.
    [[nodiscard]] bool hasAgv(std::size_t index) const noexcept;

    /// Returns the integer index corresponding to an AGV identifier.
    /// @throws std::out_of_range if agvId is not registered.
    [[nodiscard]] std::size_t getAgvIndex(const std::string& agvId) const;

    /// Returns the string identifier corresponding to an integer vertex index.
    /// @throws std::out_of_range if index is out of bounds.
    [[nodiscard]] const std::string& getAgvId(std::size_t index) const;

    /// Returns the total number of registered AGVs.
    [[nodiscard]] std::size_t getAgvCount() const noexcept;

    /// Returns a list of all registered AGV identifiers in index order.
    [[nodiscard]] std::vector<std::string> getAllAgvIds() const;

    // --- Priority Management ---

    /// Sets the operational TaskPriority for a registered AGV.
    /// @throws std::invalid_argument if priority is not a valid TaskPriority value.
    /// @throws std::out_of_range if agvId is not registered.
    void setAgvPriority(const std::string& agvId, TaskPriority priority);

    /// Sets the operational TaskPriority for a registered AGV index.
    /// @throws std::invalid_argument if priority is not a valid TaskPriority value.
    /// @throws std::out_of_range if index is out of bounds.
    void setAgvPriority(std::size_t index, TaskPriority priority);

    /// Returns the TaskPriority of a registered AGV.
    /// @throws std::out_of_range if agvId is not registered.
    [[nodiscard]] TaskPriority getAgvPriority(const std::string& agvId) const;

    /// Returns the TaskPriority of a registered AGV index.
    /// @throws std::out_of_range if index is out of bounds.
    [[nodiscard]] TaskPriority getAgvPriority(std::size_t index) const;

    // --- Dependency Management ---

    /// Adds a directed waiting dependency: waitingAgv waits for blockingAgv.
    /// @throws std::out_of_range if waitingAgv or blockingAgv is not registered.
    /// @throws std::invalid_argument if waitingAgv == blockingAgv (self-dependency)
    ///         or if the dependency already exists.
    void addDependency(const std::string& waitingAgv, const std::string& blockingAgv);

    /// Adds a directed waiting dependency by vertex indices: waitingIndex waits for blockingIndex.
    /// @throws std::out_of_range if waitingIndex or blockingIndex is out of bounds.
    /// @throws std::invalid_argument if waitingIndex == blockingIndex (self-dependency)
    ///         or if the dependency already exists.
    void addDependency(std::size_t waitingIndex, std::size_t blockingIndex);

    /// Removes a directed waiting dependency: waitingAgv waiting for blockingAgv.
    /// @return true if the dependency was found and removed; false if it did not exist.
    /// @throws std::out_of_range if waitingAgv or blockingAgv is not registered.
    bool removeDependency(const std::string& waitingAgv, const std::string& blockingAgv);

    /// Removes a directed waiting dependency by vertex indices.
    /// @return true if the dependency was found and removed; false if it did not exist.
    /// @throws std::out_of_range if waitingIndex or blockingIndex is out of bounds.
    bool removeDependency(std::size_t waitingIndex, std::size_t blockingIndex);

    /// Checks if a directed waiting dependency exists between two AGVs.
    /// @throws std::out_of_range if waitingAgv or blockingAgv is not registered.
    [[nodiscard]] bool hasDependency(const std::string& waitingAgv, const std::string& blockingAgv) const;

    /// Checks if a directed waiting dependency exists between two AGV indices.
    /// @throws std::out_of_range if waitingIndex or blockingIndex is out of bounds.
    [[nodiscard]] bool hasDependency(std::size_t waitingIndex, std::size_t blockingIndex) const;

    /// Returns the total number of directed waiting dependencies in the graph.
    [[nodiscard]] std::size_t getDependencyCount() const noexcept;

    /// Returns a list of AGV identifiers that the specified AGV is waiting for.
    /// @throws std::out_of_range if waitingAgv is not registered.
    [[nodiscard]] std::vector<std::string> getDependencies(const std::string& waitingAgv) const;

    /// Returns a list of vertex indices that the specified AGV index is waiting for.
    /// @throws std::out_of_range if waitingIndex is out of bounds.
    [[nodiscard]] std::vector<std::size_t> getDependencies(std::size_t waitingIndex) const;

    /// Clears all waiting dependencies while keeping registered AGVs intact.
    void clearDependencies() noexcept;

    /// Clears all AGVs and dependencies, resetting the graph to an empty state.
    void clear() noexcept;

    // --- Deadlock Detection and Cycle Extraction ---

    /// Detects directed waiting cycles representing deadlocks using 3-color DFS.
    /// Deterministically explores vertices in increasing index order (0 to V-1) and
    /// outgoing edges in insertion order. Extracts the first cycle encountered.
    /// @return DeadlockCycle with hasDeadlock=true and forward cycle path if a deadlock
    ///         exists; otherwise hasDeadlock=false and empty cycle.
    [[nodiscard]] DeadlockCycle detectDeadlock() const;

    // --- Deterministic Victim Selection ---

    /// Evaluates the graph for a deadlock cycle and selects the deterministic victim AGV.
    /// Victim selection rules:
    ///   1. Lowest TaskPriority in the cycle (Low < Normal < High < Urgent < Critical).
    ///   2. Tie-breaker: Lexicographically smallest AGV identifier.
    /// @return String identifier of the chosen victim AGV.
    /// @throws std::runtime_error if no deadlock cycle exists in the graph.
    [[nodiscard]] std::string selectVictim() const;

    /// Selects the deterministic victim AGV from an explicitly provided DeadlockCycle.
    /// Note: Caller-supplied cycles are treated as trusted inputs and should originate from
    /// detectDeadlock() or be independently verified by the caller. The implementation verifies
    /// AGV registrations but does not independently validate the supplied cycle's actual
    /// dependency-edge connectivity, and the cycleIndices field is not independently validated
    /// against cycle.
    /// @return String identifier of the chosen victim AGV.
    /// @throws std::runtime_error if deadlock.hasDeadlock is false or cycle is empty.
    /// @throws std::out_of_range if an AGV in the cycle is not registered in the graph.
    [[nodiscard]] std::string selectVictim(const DeadlockCycle& deadlock) const;

    /// Selects the deterministic victim AGV from an explicitly provided cycle of AGV IDs.
    /// Note: Caller-supplied cycles are treated as trusted inputs and should originate from
    /// detectDeadlock() or be independently verified by the caller. The implementation verifies
    /// AGV registrations but does not independently validate the supplied cycle's actual
    /// dependency-edge connectivity.
    /// @return String identifier of the chosen victim AGV.
    /// @throws std::runtime_error if cycle is empty.
    /// @throws std::out_of_range if an AGV in cycle is not registered in the graph.
    [[nodiscard]] std::string selectVictim(const std::vector<std::string>& cycle) const;
};
