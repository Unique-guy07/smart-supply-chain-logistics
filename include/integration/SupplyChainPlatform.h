#pragma once

#include "graph/WaitForGraph.h"
#include "graph/WarehouseGraph.h"
#include "inventory/InventoryManager.h"
#include "order/AuditCycle.h"
#include "order/OrderManager.h"
#include "packing/CargoPacking.h"
#include "task/TaskPriorityManager.h"

#include <cmath>
#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace integration {

/// Represents a directed traversal leg between two waypoints in the warehouse.
struct RouteLeg
{
    std::string source;
    std::string destination;
    double legCost{0.0};
    std::vector<std::string> route; ///< Vertex identifiers in forward order [source, ..., destination]

    [[nodiscard]] bool operator==(const RouteLeg& other) const noexcept = default;
};

/// Result of order picking and transit route planning.
///
/// Algorithmic Note:
/// Computes a sequential multi-leg Dijkstra route across waypoints in deterministic item order;
/// it does not solve the NP-hard Traveling Salesperson Problem (TSP) for globally optimal stop permutations.
struct OrderRoutePlan
{
    bool success{false};
    double totalCost{0.0};
    std::vector<RouteLeg> legs;
    std::string errorMessage;

    [[nodiscard]] explicit operator bool() const noexcept { return success; }
    [[nodiscard]] bool operator==(const OrderRoutePlan& other) const noexcept = default;
};

/// Physical dimensions and weight profile for discrete package formation.
/// Dimensions must be strictly positive integers; weight must be finite and strictly positive (> 0.0).
struct PackagingProfile
{
    int width{0};
    int depth{0};
    int height{0};
    double unitWeight{0.0};

    PackagingProfile() = default;

    PackagingProfile(int width, int depth, int height, double unitWeight);

    [[nodiscard]] bool isValid() const noexcept;

    [[nodiscard]] bool operator==(const PackagingProfile& other) const noexcept;
};

/// Result of a guarded order dispatch operation.
///
/// Semantics:
/// - `dispatchAttempted`: True if and only if prevalidation and safety guards passed,
///   and OrderManager::dispatchHighestPriorityOrder() was invoked.
/// - `success`: True if and only if dispatch was invoked AND the order was successfully
///   fulfilled into OrderStatus::Completed. Distinguishes successful dispatch invocation
///   from successful order fulfillment.
/// - `orderId`: Identifier of the evaluated order, or empty if the queue was empty.
/// - `resultingStatus`: The actual lifecycle status of the candidate order (e.g. Completed, Failed),
///   or std::nullopt if no candidate was available in the queue.
/// - `routePlan`: The route plan computed during route prevalidation.
/// - `packingResult`: The packing result computed during 3D cargo packing prevalidation.
/// - `message`: Informative diagnostic message detailing the outcome or reason for rejection.
struct GuardedDispatchResult
{
    bool success{false};
    bool dispatchAttempted{false};
    std::string orderId;
    std::optional<OrderStatus> resultingStatus{std::nullopt};
    OrderRoutePlan routePlan;
    packing::PackingResult packingResult;
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept { return success; }
};

/// Foundational integration facade for the Smart Supply Chain & Warehouse Logistics Platform.
///
/// Architecture & Guarantees:
/// - Composes the core subsystem managers into a single unified platform lifecycle.
/// - Member declaration order guarantees that dependencies outlive the objects referencing them:
///   `m_inventory` is initialized before `m_orderManager`, which binds a reference `InventoryManager&`.
/// - Copy and move operations are explicitly deleted to preserve valid internal reference bindings
///   and prevent state tearing.
/// - Provides packaging profile registry to map product SKUs to physical 3D dimensions and unit weights.
/// - Provides read-only and mutable accessors to internal managers.
/// - Provides read-only route planning and 3D cargo packing prevalidation without mutating inventory
///   or order state.
class SupplyChainPlatform
{
public:
    static constexpr int DEFAULT_INVENTORY_CAPACITY = 101;
    static constexpr int DEFAULT_DEVICE_CAPACITY = 16;

    /// Constructs the platform with configurable initial inventory indexing capacities.
    /// @throws std::invalid_argument if inventoryCapacity <= 0 or deviceCapacity <= 0.
    explicit SupplyChainPlatform(
        int inventoryCapacity = DEFAULT_INVENTORY_CAPACITY,
        int deviceCapacity = DEFAULT_DEVICE_CAPACITY);

    ~SupplyChainPlatform() = default;

    // Non-copyable, non-movable due to reference binding and internal manager semantics
    SupplyChainPlatform(const SupplyChainPlatform&) = delete;
    SupplyChainPlatform& operator=(const SupplyChainPlatform&) = delete;
    SupplyChainPlatform(SupplyChainPlatform&&) = delete;
    SupplyChainPlatform& operator=(SupplyChainPlatform&&) = delete;

    // --- Packaging Profile Management ---

    /// Registers a packaging profile for a product SKU.
    /// If a packaging profile is already registered for productId, its profile is replaced.
    /// @throws std::invalid_argument if productId is empty/blank or profile is invalid.
    void registerPackagingProfile(const std::string& productId, const PackagingProfile& profile);

    /// Checks if a packaging profile exists for the given product SKU.
    [[nodiscard]] bool hasPackagingProfile(const std::string& productId) const noexcept;

    /// Retrieves a pointer to the packaging profile for a product SKU, or nullptr if unregistered.
    ///
    /// Lifetime and Invalidation Limitations:
    /// - The returned pointer references the PackagingProfile directly inside the internal
    ///   std::unordered_map registry.
    /// - Callers must not retain or dereference this pointer after the corresponding profile is
    ///   replaced or erased, or after any registry mutation (such as subsequent profile registrations)
    ///   that may trigger container rehashing or entry replacement.
    /// - The pointer becomes dangling if the parent SupplyChainPlatform instance is destroyed.
    /// - If persistent access is needed across registry mutations, callers should copy the
    ///   PackagingProfile by value.
    [[nodiscard]] const PackagingProfile* getPackagingProfile(const std::string& productId) const noexcept;

    /// Returns the number of registered packaging profiles.
    [[nodiscard]] std::size_t getPackagingProfileCount() const noexcept;

    // --- Subsystem Component Accessors (Mutable) ---

    [[nodiscard]] InventoryManager& getInventoryManager() noexcept;
    [[nodiscard]] OrderManager& getOrderManager() noexcept;
    [[nodiscard]] WarehouseGraph& getWarehouseGraph() noexcept;
    [[nodiscard]] WaitForGraph& getWaitForGraph() noexcept;
    [[nodiscard]] AuditCycle& getAuditCycle() noexcept;
    [[nodiscard]] TaskPriorityManager& getTaskManager() noexcept;

    // --- Subsystem Component Accessors (Read-Only) ---

    [[nodiscard]] const InventoryManager& getInventoryManager() const noexcept;
    [[nodiscard]] const OrderManager& getOrderManager() const noexcept;
    [[nodiscard]] const WarehouseGraph& getWarehouseGraph() const noexcept;
    [[nodiscard]] const WaitForGraph& getWaitForGraph() const noexcept;
    [[nodiscard]] const AuditCycle& getAuditCycle() const noexcept;
    [[nodiscard]] const TaskPriorityManager& getTaskManager() const noexcept;

    // --- Workflow Prevalidation (Read-Only) ---

    /// Plans a sequential picking and transit route for an order without mutating order or inventory state.
    /// Traversal sequence: depotVertex -> unique item pick bins (in first-seen order) -> packingStationVertex -> dockVertex.
    ///
    /// Algorithmic Note:
    /// Computes optimal Dijkstra shortest paths for each sequential leg. This optimizes each leg between
    /// prescribed consecutive stops, rather than computing an NP-hard Traveling Salesperson (TSP) permutation.
    ///
    /// Lifecycle Status Policy:
    /// - Permits inspection of orders in all registered lifecycle states (Pending, Processing, Completed,
    ///   Failed, Cancelled). Prevalidation is an analytical, read-only feasibility query that inspects an
    ///   order's immutable line items without modifying order status, inventory stock, or graph topology,
    ///   and without asserting or requiring dispatch eligibility.
    ///
    /// Validation & Failure Semantics:
    /// - Returns success=false if orderId is not found or order has no items.
    /// - Returns success=false if depotVertex, packingStationVertex, or dockVertex are blank or missing in the graph.
    /// - Returns success=false if any item's product is not found in catalog, has a blank bin, or bin is missing in graph.
    /// - Returns success=false if any route leg between consecutive waypoints has no path.
    /// - Route-cost overflow: Returns success=false with totalCost=0.0 and no partial legs if individual leg costs
    ///   or the cumulative aggregate total overflows double precision or evaluates to non-finite (inf/NaN).
    /// - Exception contract: Catches expected domain exceptions (std::out_of_range, std::invalid_argument) as ordinary
    ///   validation failures; allows unexpected failures (such as std::bad_alloc) to propagate.
    /// - Does not modify order status, inventory stock, or graph structure.
    [[nodiscard]] OrderRoutePlan planOrderRoute(
        const std::string& orderId,
        const std::string& depotVertex,
        const std::string& packingStationVertex,
        const std::string& dockVertex) const;

    /// Validates physical 3D cargo packing feasibility for an order into a container without mutating state.
    /// Expands each ordered SKU quantity into separate discrete packing::Package units using registered PackagingProfiles.
    ///
    /// Lifecycle Status Policy:
    /// - Permits inspection of orders in all registered lifecycle states (Pending, Processing, Completed,
    ///   Failed, Cancelled). Evaluates packing feasibility for the order's immutable SKU composition.
    ///
    /// Validation & Failure Semantics:
    /// - Returns a PackingResult with allPackagesPlaced=false if orderId is not found, order has no items,
    ///   any item has quantity <= 0, any item's product is missing from catalog, or packaging profile is missing/invalid.
    /// - Invokes packing::packCargo() with the supplied container and configuration.
    /// - Exception contract: Catches expected domain packing exceptions (std::invalid_argument, std::length_error,
    ///   std::overflow_error) and returns allPackagesPlaced=false with error summary; allows unexpected failures
    ///   (such as std::bad_alloc) to propagate.
    /// - Does not dispatch the order, deduct stock, or mutate order status.
    [[nodiscard]] packing::PackingResult validateOrderPacking(
        const std::string& orderId,
        const packing::Container& container,
        const packing::PackingConfig& config = packing::PackingConfig{}) const;

    // --- Guarded Workflow Operations ---

    /// Attempts guarded dispatch of the highest-priority eligible pending order.
    ///
    /// Required Dispatch Sequence:
    /// 1. Peeks at highest-priority candidate order in DEPQ without dequeuing.
    /// 2. If the queue is empty, returns failure without mutating platform state.
    /// 3. Retrieves candidate order and verifies it is in OrderStatus::Pending.
    /// 4. Executes read-only route prevalidation (planOrderRoute) through depot, pick bins,
    ///    packing station, and outbound dock.
    /// 5. Executes read-only 3D cargo packing prevalidation (validateOrderPacking) against the
    ///    supplied shipping container.
    /// 6. If either prevalidation fails, aborts immediately without dispatching or mutating state.
    /// 7. Immediately prior to dispatch, peeks again to confirm that the same order ID remains
    ///    highest priority and that its status is still Pending.
    /// 8. If candidate changed, disappeared, or is no longer Pending, aborts without dispatching.
    /// 9. Invokes OrderManager::dispatchHighestPriorityOrder() exactly once.
    /// 10. Inspects and reports the actual resulting order status (Completed or Failed).
    ///
    /// Concurrency & Operational Limitations:
    /// This design assumes a single-threaded workflow without intervening mutations between
    /// the prevalidation recheck and dispatch. It does not provide multi-threaded concurrency safety.
    ///
    /// Failure Guarantees:
    /// Rejections prior to dispatch guarantee zero mutation to inventory stock, order lifecycle state,
    /// or warehouse graph topology.
    [[nodiscard]] GuardedDispatchResult dispatchHighestPriorityOrder(
        const std::string& depotVertex,
        const std::string& packingStationVertex,
        const std::string& dockVertex,
        const packing::Container& container,
        const packing::PackingConfig& config = packing::PackingConfig{});

private:
    // Member declaration order strictly dictates initialization and destruction order:
    // m_inventory MUST precede m_orderManager because m_orderManager holds an InventoryManager& reference.
    InventoryManager m_inventory;
    OrderManager m_orderManager;
    WarehouseGraph m_warehouseGraph;
    WaitForGraph m_waitForGraph;
    AuditCycle m_auditCycle;
    TaskPriorityManager m_taskManager;
    std::unordered_map<std::string, PackagingProfile> m_packagingProfiles;

    // Test seam for simulating intervening state changes prior to dispatch recheck (M4 convention)
    std::function<void(const std::string& orderId)> m_preRecheckHook;
    friend class SupplyChainPlatformTestAccessor;
};

} // namespace integration
