#include "graph/WaitForGraph.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string_view>
#include <utility>

bool WaitForGraph::isBlank(std::string_view str) noexcept
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

void WaitForGraph::validateIndex(std::size_t index) const
{
    if (index >= agvRecords.size()) {
        throw std::out_of_range("WaitForGraph: AGV index out of range: " + std::to_string(index)
                                + " (registered AGV count is " + std::to_string(agvRecords.size()) + ")");
    }
}

WaitForGraph::WaitForGraph(std::size_t expectedAgvCount)
{
    agvRecords.reserve(expectedAgvCount);
    identifierToIndex.reserve(expectedAgvCount);
    adj.reserve(expectedAgvCount);
}

std::size_t WaitForGraph::addAgv(const std::string& agvId, TaskPriority priority)
{
    if (!isValidTaskPriority(priority)) {
        throw std::invalid_argument("WaitForGraph: invalid TaskPriority value: "
                                    + std::to_string(static_cast<int>(priority)));
    }
    if (isBlank(agvId)) {
        throw std::invalid_argument("WaitForGraph: AGV identifier must not be empty or blank");
    }
    if (identifierToIndex.contains(agvId)) {
        throw std::invalid_argument("WaitForGraph: duplicate AGV identifier: " + agvId);
    }

    const std::size_t newIndex = agvRecords.size();

    // Step 1: Insert into identifier lookup map.
    auto [mapIt, inserted] = identifierToIndex.try_emplace(agvId, newIndex);
    if (!inserted) {
        throw std::invalid_argument("WaitForGraph: duplicate AGV identifier: " + agvId);
    }

    // Step 2: Push into agvRecords vector with rollback on failure.
    try {
        agvRecords.push_back({agvId, newIndex, priority});
    } catch (...) {
        identifierToIndex.erase(mapIt);
        throw;
    }

    // Step 3: Emplace empty adjacency list with rollback on failure.
    try {
        adj.emplace_back();
    } catch (...) {
        agvRecords.pop_back();
        identifierToIndex.erase(mapIt);
        throw;
    }

    return newIndex;
}

bool WaitForGraph::hasAgv(const std::string& agvId) const noexcept
{
    return identifierToIndex.contains(agvId);
}

bool WaitForGraph::hasAgv(std::size_t index) const noexcept
{
    return index < agvRecords.size();
}

std::size_t WaitForGraph::getAgvIndex(const std::string& agvId) const
{
    auto it = identifierToIndex.find(agvId);
    if (it == identifierToIndex.end()) {
        throw std::out_of_range("WaitForGraph: unregistered AGV identifier: " + agvId);
    }
    return it->second;
}

const std::string& WaitForGraph::getAgvId(std::size_t index) const
{
    validateIndex(index);
    return agvRecords[index].identifier;
}

std::size_t WaitForGraph::getAgvCount() const noexcept
{
    return agvRecords.size();
}

std::vector<std::string> WaitForGraph::getAllAgvIds() const
{
    std::vector<std::string> ids;
    ids.reserve(agvRecords.size());
    for (const auto& record : agvRecords) {
        ids.push_back(record.identifier);
    }
    return ids;
}

void WaitForGraph::setAgvPriority(const std::string& agvId, TaskPriority priority)
{
    setAgvPriority(getAgvIndex(agvId), priority);
}

void WaitForGraph::setAgvPriority(std::size_t index, TaskPriority priority)
{
    validateIndex(index);
    if (!isValidTaskPriority(priority)) {
        throw std::invalid_argument("WaitForGraph: invalid TaskPriority value: "
                                    + std::to_string(static_cast<int>(priority)));
    }
    agvRecords[index].priority = priority;
}

TaskPriority WaitForGraph::getAgvPriority(const std::string& agvId) const
{
    return getAgvPriority(getAgvIndex(agvId));
}

TaskPriority WaitForGraph::getAgvPriority(std::size_t index) const
{
    validateIndex(index);
    return agvRecords[index].priority;
}

void WaitForGraph::addDependency(std::size_t waitingIndex, std::size_t blockingIndex)
{
    validateIndex(waitingIndex);
    validateIndex(blockingIndex);

    if (waitingIndex == blockingIndex) {
        throw std::invalid_argument("WaitForGraph: self-dependency not allowed (AGV cannot wait on itself)");
    }

    for (std::size_t existingTo : adj[waitingIndex]) {
        if (existingTo == blockingIndex) {
            throw std::invalid_argument("WaitForGraph: dependency from "
                                        + agvRecords[waitingIndex].identifier + " to "
                                        + agvRecords[blockingIndex].identifier + " already exists");
        }
    }

    adj[waitingIndex].push_back(blockingIndex);
    ++dependencyCount;
}

void WaitForGraph::addDependency(const std::string& waitingAgv, const std::string& blockingAgv)
{
    addDependency(getAgvIndex(waitingAgv), getAgvIndex(blockingAgv));
}

bool WaitForGraph::removeDependency(std::size_t waitingIndex, std::size_t blockingIndex)
{
    validateIndex(waitingIndex);
    validateIndex(blockingIndex);

    auto& edges = adj[waitingIndex];
    auto it = std::find(edges.begin(), edges.end(), blockingIndex);
    if (it != edges.end()) {
        edges.erase(it);
        --dependencyCount;
        return true;
    }
    return false;
}

bool WaitForGraph::removeDependency(const std::string& waitingAgv, const std::string& blockingAgv)
{
    return removeDependency(getAgvIndex(waitingAgv), getAgvIndex(blockingAgv));
}

bool WaitForGraph::hasDependency(std::size_t waitingIndex, std::size_t blockingIndex) const
{
    validateIndex(waitingIndex);
    validateIndex(blockingIndex);

    for (std::size_t existingTo : adj[waitingIndex]) {
        if (existingTo == blockingIndex) {
            return true;
        }
    }
    return false;
}

bool WaitForGraph::hasDependency(const std::string& waitingAgv, const std::string& blockingAgv) const
{
    return hasDependency(getAgvIndex(waitingAgv), getAgvIndex(blockingAgv));
}

std::size_t WaitForGraph::getDependencyCount() const noexcept
{
    return dependencyCount;
}

std::vector<std::string> WaitForGraph::getDependencies(const std::string& waitingAgv) const
{
    const std::size_t waitingIndex = getAgvIndex(waitingAgv);
    std::vector<std::string> deps;
    deps.reserve(adj[waitingIndex].size());
    for (std::size_t toIdx : adj[waitingIndex]) {
        deps.push_back(agvRecords[toIdx].identifier);
    }
    return deps;
}

std::vector<std::size_t> WaitForGraph::getDependencies(std::size_t waitingIndex) const
{
    validateIndex(waitingIndex);
    return adj[waitingIndex];
}

void WaitForGraph::clearDependencies() noexcept
{
    for (auto& edges : adj) {
        edges.clear();
    }
    dependencyCount = 0;
}

void WaitForGraph::clear() noexcept
{
    agvRecords.clear();
    identifierToIndex.clear();
    adj.clear();
    dependencyCount = 0;
}

bool WaitForGraph::dfsCycle(std::size_t u,
                            std::vector<Color>& color,
                            std::vector<std::size_t>& parent,
                            DeadlockCycle& result) const
{
    color[u] = Color::Gray;

    for (std::size_t v : adj[u]) {
        if (color[v] == Color::White) {
            parent[v] = u;
            if (dfsCycle(v, color, parent, result)) {
                return true;
            }
        } else if (color[v] == Color::Gray) {
            // Directed cycle detected: back-edge u -> v.
            // Vertex v is an ancestor of u on the active DFS recursion stack.
            // Reconstruct the cycle from u back to v using custom LIFO Stack<T>.
            Stack<std::size_t> cycleStack;
            std::size_t curr = u;
            while (curr != v) {
                cycleStack.push(curr);
                curr = parent[curr];
            }
            cycleStack.push(v);

            result.hasDeadlock = true;
            result.cycle.clear();
            result.cycleIndices.clear();

            while (!cycleStack.isEmpty()) {
                std::size_t node = 0;
                cycleStack.pop(node);
                result.cycleIndices.push_back(node);
                result.cycle.push_back(agvRecords[node].identifier);
            }

            // Append closing node v to clearly represent the closing dependency u -> v
            result.cycleIndices.push_back(v);
            result.cycle.push_back(agvRecords[v].identifier);

            return true;
        }
        // If color[v] == Color::Black: cross/forward edge to already completed subtree; ignore.
    }

    color[u] = Color::Black;
    return false;
}

DeadlockCycle WaitForGraph::detectDeadlock() const
{
    DeadlockCycle result;
    const std::size_t n = agvRecords.size();
    if (n == 0 || dependencyCount == 0) {
        return result;
    }

    std::vector<Color> color(n, Color::White);
    std::vector<std::size_t> parent(n, NO_PREDECESSOR);

    // Deterministic outer traversal: iterate vertices in ascending index order
    for (std::size_t i = 0; i < n; ++i) {
        if (color[i] == Color::White) {
            if (dfsCycle(i, color, parent, result)) {
                return result;
            }
        }
    }

    return result;
}

std::string WaitForGraph::selectVictim() const
{
    DeadlockCycle deadlock = detectDeadlock();
    if (!deadlock.hasDeadlock) {
        throw std::runtime_error("WaitForGraph: no deadlock cycle exists for victim selection");
    }
    return selectVictim(deadlock.cycle);
}

std::string WaitForGraph::selectVictim(const DeadlockCycle& deadlock) const
{
    if (!deadlock.hasDeadlock || deadlock.cycle.empty()) {
        throw std::runtime_error("WaitForGraph: no deadlock cycle exists for victim selection");
    }
    return selectVictim(deadlock.cycle);
}

std::string WaitForGraph::selectVictim(const std::vector<std::string>& cycle) const
{
    if (cycle.empty()) {
        throw std::runtime_error("WaitForGraph: cycle is empty for victim selection");
    }

    std::string bestAgv;
    TaskPriority bestPriority = TaskPriority::Critical;
    bool first = true;

    for (const std::string& agvId : cycle) {
        // Validates that agvId is registered; throws std::out_of_range if not found.
        TaskPriority prio = getAgvPriority(agvId);

        if (first) {
            bestAgv = agvId;
            bestPriority = prio;
            first = false;
            continue;
        }

        // Rule 1: AGV with lowest TaskPriority value yields first.
        if (prio < bestPriority) {
            bestAgv = agvId;
            bestPriority = prio;
        } else if (prio == bestPriority && agvId < bestAgv) {
            // Rule 2 & 3: Tie-breaker selects lexicographically smallest AGV identifier.
            bestAgv = agvId;
        }
    }

    return bestAgv;
}
