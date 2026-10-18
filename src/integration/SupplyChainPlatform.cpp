#include "integration/SupplyChainPlatform.h"

#include <cctype>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace integration {

PackagingProfile::PackagingProfile(int width, int depth, int height, double unitWeight)
    : width(width), depth(depth), height(height), unitWeight(unitWeight)
{
    if (width <= 0 || depth <= 0 || height <= 0) {
        throw std::invalid_argument("PackagingProfile: dimensions must be positive integers");
    }
    if (unitWeight <= 0.0 || !std::isfinite(unitWeight)) {
        throw std::invalid_argument("PackagingProfile: unitWeight must be positive and finite");
    }
}

bool PackagingProfile::isValid() const noexcept
{
    return width > 0 && depth > 0 && height > 0
        && unitWeight > 0.0 && std::isfinite(unitWeight);
}

bool PackagingProfile::operator==(const PackagingProfile& other) const noexcept
{
    return width == other.width
        && depth == other.depth
        && height == other.height
        && std::abs(unitWeight - other.unitWeight) < 1e-9;
}

static bool isBlank(std::string_view s) noexcept
{
    if (s.empty()) {
        return true;
    }
    for (char c : s) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

SupplyChainPlatform::SupplyChainPlatform(int inventoryCapacity, int deviceCapacity)
    : m_inventory(inventoryCapacity, deviceCapacity)
    , m_orderManager(m_inventory)
    , m_warehouseGraph()
    , m_waitForGraph()
    , m_auditCycle()
    , m_taskManager()
    , m_packagingProfiles()
{
}

void SupplyChainPlatform::registerPackagingProfile(const std::string& productId, const PackagingProfile& profile)
{
    if (isBlank(productId)) {
        throw std::invalid_argument("SupplyChainPlatform: productId must not be empty or blank");
    }
    if (!profile.isValid()) {
        throw std::invalid_argument("SupplyChainPlatform: packaging profile is invalid");
    }
    m_packagingProfiles[productId] = profile;
}

bool SupplyChainPlatform::hasPackagingProfile(const std::string& productId) const noexcept
{
    return m_packagingProfiles.find(productId) != m_packagingProfiles.end();
}

const PackagingProfile* SupplyChainPlatform::getPackagingProfile(const std::string& productId) const noexcept
{
    auto it = m_packagingProfiles.find(productId);
    if (it == m_packagingProfiles.end()) {
        return nullptr;
    }
    return &it->second;
}

std::size_t SupplyChainPlatform::getPackagingProfileCount() const noexcept
{
    return m_packagingProfiles.size();
}

// --- Subsystem Component Accessors (Mutable) ---

InventoryManager& SupplyChainPlatform::getInventoryManager() noexcept
{
    return m_inventory;
}

OrderManager& SupplyChainPlatform::getOrderManager() noexcept
{
    return m_orderManager;
}

WarehouseGraph& SupplyChainPlatform::getWarehouseGraph() noexcept
{
    return m_warehouseGraph;
}

WaitForGraph& SupplyChainPlatform::getWaitForGraph() noexcept
{
    return m_waitForGraph;
}

AuditCycle& SupplyChainPlatform::getAuditCycle() noexcept
{
    return m_auditCycle;
}

TaskPriorityManager& SupplyChainPlatform::getTaskManager() noexcept
{
    return m_taskManager;
}

// --- Subsystem Component Accessors (Read-Only) ---

const InventoryManager& SupplyChainPlatform::getInventoryManager() const noexcept
{
    return m_inventory;
}

const OrderManager& SupplyChainPlatform::getOrderManager() const noexcept
{
    return m_orderManager;
}

const WarehouseGraph& SupplyChainPlatform::getWarehouseGraph() const noexcept
{
    return m_warehouseGraph;
}

const WaitForGraph& SupplyChainPlatform::getWaitForGraph() const noexcept
{
    return m_waitForGraph;
}

const AuditCycle& SupplyChainPlatform::getAuditCycle() const noexcept
{
    return m_auditCycle;
}

const TaskPriorityManager& SupplyChainPlatform::getTaskManager() const noexcept
{
    return m_taskManager;
}

// --- Workflow Prevalidation (Read-Only) ---

OrderRoutePlan SupplyChainPlatform::planOrderRoute(
    const std::string& orderId,
    const std::string& depotVertex,
    const std::string& packingStationVertex,
    const std::string& dockVertex) const
{
    OrderRoutePlan plan;

    if (isBlank(orderId)) {
        plan.errorMessage = "SupplyChainPlatform::planOrderRoute: orderId must not be empty or blank";
        return plan;
    }

    const Order* order = m_orderManager.getOrder(orderId);
    if (order == nullptr) {
        plan.errorMessage = "SupplyChainPlatform::planOrderRoute: order not found: " + orderId;
        return plan;
    }

    const auto& items = order->getItems();
    if (items.empty()) {
        plan.errorMessage = "SupplyChainPlatform::planOrderRoute: order contains no line items: " + orderId;
        return plan;
    }

    // Validate facility endpoints exist in graph
    if (isBlank(depotVertex) || !m_warehouseGraph.hasVertex(depotVertex)) {
        plan.errorMessage = "SupplyChainPlatform::planOrderRoute: depot vertex not found in warehouse graph: " + depotVertex;
        return plan;
    }
    if (isBlank(packingStationVertex) || !m_warehouseGraph.hasVertex(packingStationVertex)) {
        plan.errorMessage = "SupplyChainPlatform::planOrderRoute: packing station vertex not found in warehouse graph: " + packingStationVertex;
        return plan;
    }
    if (isBlank(dockVertex) || !m_warehouseGraph.hasVertex(dockVertex)) {
        plan.errorMessage = "SupplyChainPlatform::planOrderRoute: dock vertex not found in warehouse graph: " + dockVertex;
        return plan;
    }

    // Validate products and bin locations, and deduplicate bins preserving first-seen order
    std::vector<std::string> uniqueBins;
    std::unordered_set<std::string> seenBins;

    for (const auto& item : items) {
        if (isBlank(item.productId)) {
            plan.errorMessage = "SupplyChainPlatform::planOrderRoute: order item contains blank productId";
            return plan;
        }

        const Product* p = m_inventory.findProduct(item.productId);
        if (p == nullptr) {
            plan.errorMessage = "SupplyChainPlatform::planOrderRoute: product not found in catalog: " + item.productId;
            return plan;
        }

        const std::string& bin = p->getBinLocation();
        if (isBlank(bin)) {
            plan.errorMessage = "SupplyChainPlatform::planOrderRoute: product has blank bin location: " + item.productId;
            return plan;
        }

        if (!m_warehouseGraph.hasVertex(bin)) {
            plan.errorMessage = "SupplyChainPlatform::planOrderRoute: bin location not found in warehouse graph: " + bin;
            return plan;
        }

        if (seenBins.insert(bin).second) {
            uniqueBins.push_back(bin);
        }
    }

    // Build ordered waypoints: depot -> unique pick bins -> packing station -> dock
    std::vector<std::string> waypoints;
    waypoints.reserve(2 + uniqueBins.size() + 1);
    waypoints.push_back(depotVertex);
    for (const auto& b : uniqueBins) {
        waypoints.push_back(b);
    }
    waypoints.push_back(packingStationVertex);
    waypoints.push_back(dockVertex);

    double totalCost = 0.0;
    std::vector<RouteLeg> legs;
    legs.reserve(waypoints.size() - 1);

    for (std::size_t i = 0; i + 1 < waypoints.size(); ++i) {
        const std::string& src = waypoints[i];
        const std::string& dst = waypoints[i + 1];

        DijkstraResult res;
        try {
            res = m_warehouseGraph.findShortestPathDijkstra(src, dst);
        } catch (const std::out_of_range& e) {
            plan.success = false;
            plan.totalCost = 0.0;
            plan.legs.clear();
            plan.errorMessage = "SupplyChainPlatform::planOrderRoute: routing lookup error from " + src + " to " + dst + ": " + e.what();
            return plan;
        } catch (const std::invalid_argument& e) {
            plan.success = false;
            plan.totalCost = 0.0;
            plan.legs.clear();
            plan.errorMessage = "SupplyChainPlatform::planOrderRoute: routing argument error from " + src + " to " + dst + ": " + e.what();
            return plan;
        }

        if (!res.hasPath) {
            plan.success = false;
            plan.totalCost = 0.0;
            plan.legs.clear();
            plan.errorMessage = "SupplyChainPlatform::planOrderRoute: unreachable route leg from " + src + " to " + dst;
            return plan;
        }

        const double nextTotal = totalCost + res.totalCost;
        if (!std::isfinite(res.totalCost) || !std::isfinite(nextTotal)) {
            plan.success = false;
            plan.totalCost = 0.0;
            plan.legs.clear();
            plan.errorMessage = "SupplyChainPlatform::planOrderRoute: route cost overflow after adding leg from " + src + " to " + dst;
            return plan;
        }

        RouteLeg leg;
        leg.source = src;
        leg.destination = dst;
        leg.legCost = res.totalCost;
        leg.route = std::move(res.route);

        totalCost = nextTotal;
        legs.push_back(std::move(leg));
    }

    plan.success = true;
    plan.totalCost = totalCost;
    plan.legs = std::move(legs);
    plan.errorMessage.clear();
    return plan;
}

packing::PackingResult SupplyChainPlatform::validateOrderPacking(
    const std::string& orderId,
    const packing::Container& container,
    const packing::PackingConfig& config) const
{
    packing::PackingResult result;
    result.containerVolume = container.volume();

    if (isBlank(orderId)) {
        result.summary = "SupplyChainPlatform::validateOrderPacking: orderId must not be empty or blank";
        return result;
    }

    const Order* order = m_orderManager.getOrder(orderId);
    if (order == nullptr) {
        result.summary = "SupplyChainPlatform::validateOrderPacking: order not found: " + orderId;
        return result;
    }

    const auto& items = order->getItems();
    if (items.empty()) {
        result.summary = "SupplyChainPlatform::validateOrderPacking: order contains no line items: " + orderId;
        return result;
    }

    std::vector<packing::Package> packages;
    std::unordered_map<std::string, int> skuUnitCounts;

    for (const auto& item : items) {
        if (isBlank(item.productId)) {
            result.summary = "SupplyChainPlatform::validateOrderPacking: order item has blank productId";
            return result;
        }
        if (item.quantity <= 0) {
            result.summary = "SupplyChainPlatform::validateOrderPacking: invalid non-positive item quantity for SKU: " + item.productId;
            return result;
        }

        const Product* prod = m_inventory.findProduct(item.productId);
        if (prod == nullptr) {
            result.summary = "SupplyChainPlatform::validateOrderPacking: product not found in catalog: " + item.productId;
            return result;
        }

        const PackagingProfile* prof = getPackagingProfile(item.productId);
        if (prof == nullptr) {
            result.summary = "SupplyChainPlatform::validateOrderPacking: missing packaging profile for SKU: " + item.productId;
            return result;
        }
        if (!prof->isValid()) {
            result.summary = "SupplyChainPlatform::validateOrderPacking: invalid packaging profile for SKU: " + item.productId;
            return result;
        }

        // Expand ordered quantity into discrete packing::Package units with unique IDs
        for (int k = 0; k < item.quantity; ++k) {
            int unitNum = ++skuUnitCounts[item.productId];
            std::string pkgId = orderId + "_" + item.productId + "_" + std::to_string(unitNum);
            packages.emplace_back(pkgId, prof->width, prof->depth, prof->height, prof->unitWeight);
        }
    }

    try {
        return packing::packCargo(container, packages, config);
    } catch (const std::invalid_argument& e) {
        result.allPackagesPlaced = false;
        result.summary = "SupplyChainPlatform::validateOrderPacking: invalid packing parameter: " + std::string(e.what());
        result.unplacedPackageIds.clear();
        result.unplacedPackageIds.reserve(packages.size());
        for (const auto& pkg : packages) {
            result.unplacedPackageIds.push_back(pkg.id);
        }
        return result;
    } catch (const std::length_error& e) {
        result.allPackagesPlaced = false;
        result.summary = "SupplyChainPlatform::validateOrderPacking: packing resource limit exceeded: " + std::string(e.what());
        result.unplacedPackageIds.clear();
        result.unplacedPackageIds.reserve(packages.size());
        for (const auto& pkg : packages) {
            result.unplacedPackageIds.push_back(pkg.id);
        }
        return result;
    } catch (const std::overflow_error& e) {
        result.allPackagesPlaced = false;
        result.summary = "SupplyChainPlatform::validateOrderPacking: packing arithmetic overflow: " + std::string(e.what());
        result.unplacedPackageIds.clear();
        result.unplacedPackageIds.reserve(packages.size());
        for (const auto& pkg : packages) {
            result.unplacedPackageIds.push_back(pkg.id);
        }
        return result;
    }
}

// --- Guarded Workflow Operations ---

GuardedDispatchResult SupplyChainPlatform::dispatchHighestPriorityOrder(
    const std::string& depotVertex,
    const std::string& packingStationVertex,
    const std::string& dockVertex,
    const packing::Container& container,
    const packing::PackingConfig& config)
{
    GuardedDispatchResult result;

    // 1. Peek at highest-priority candidate order in DEPQ without dequeuing
    std::string candidateId;
    if (!m_orderManager.peekHighestPriorityOrder(candidateId)) {
        result.success = false;
        result.dispatchAttempted = false;
        result.orderId.clear();
        result.resultingStatus = std::nullopt;
        result.message = "SupplyChainPlatform::dispatchHighestPriorityOrder: no eligible pending orders in queue";
        return result;
    }

    result.orderId = candidateId;

    // 2. Retrieve candidate order and verify it is currently Pending
    const Order* candidateOrder = m_orderManager.getOrder(candidateId);
    if (candidateOrder == nullptr) {
        result.success = false;
        result.dispatchAttempted = false;
        result.resultingStatus = std::nullopt;
        result.message = "SupplyChainPlatform::dispatchHighestPriorityOrder: candidate order not found in registry: " + candidateId;
        return result;
    }

    result.resultingStatus = candidateOrder->getStatus();
    if (candidateOrder->getStatus() != OrderStatus::Pending) {
        result.success = false;
        result.dispatchAttempted = false;
        result.message = "SupplyChainPlatform::dispatchHighestPriorityOrder: candidate order is not in Pending status: " + candidateId;
        return result;
    }

    // 3. Execute route prevalidation
    result.routePlan = planOrderRoute(candidateId, depotVertex, packingStationVertex, dockVertex);
    if (!result.routePlan.success) {
        result.success = false;
        result.dispatchAttempted = false;
        const Order* cur = m_orderManager.getOrder(candidateId);
        result.resultingStatus = (cur != nullptr) ? std::optional<OrderStatus>(cur->getStatus()) : std::nullopt;
        result.message = "SupplyChainPlatform::dispatchHighestPriorityOrder: route prevalidation failed for order "
                         + candidateId + ": " + result.routePlan.errorMessage;
        return result;
    }

    // 4. Execute 3D cargo packing prevalidation
    result.packingResult = validateOrderPacking(candidateId, container, config);
    if (!result.packingResult.allPackagesPlaced) {
        result.success = false;
        result.dispatchAttempted = false;
        const Order* cur = m_orderManager.getOrder(candidateId);
        result.resultingStatus = (cur != nullptr) ? std::optional<OrderStatus>(cur->getStatus()) : std::nullopt;
        result.message = "SupplyChainPlatform::dispatchHighestPriorityOrder: packing prevalidation failed for order "
                         + candidateId + ": " + result.packingResult.summary;
        return result;
    }

    // Test seam: optional hook for simulating state changes prior to final recheck (M4 convention)
    if (m_preRecheckHook) {
        m_preRecheckHook(candidateId);
    }

    // 5. Recheck candidate immediately prior to dispatch
    std::string recheckId;
    const bool recheckPeekOk = m_orderManager.peekHighestPriorityOrder(recheckId);
    if (!recheckPeekOk || recheckId != candidateId) {
        result.success = false;
        result.dispatchAttempted = false;
        const Order* cur = m_orderManager.getOrder(candidateId);
        result.resultingStatus = (cur != nullptr) ? std::optional<OrderStatus>(cur->getStatus()) : std::nullopt;
        result.message = "SupplyChainPlatform::dispatchHighestPriorityOrder: candidate order changed during prevalidation (expected "
                         + candidateId + ", found " + (recheckPeekOk ? recheckId : "none") + ")";
        return result;
    }

    const Order* recheckOrder = m_orderManager.getOrder(candidateId);
    if (recheckOrder == nullptr || recheckOrder->getStatus() != OrderStatus::Pending) {
        result.success = false;
        result.dispatchAttempted = false;
        result.resultingStatus = (recheckOrder != nullptr) ? std::optional<OrderStatus>(recheckOrder->getStatus()) : std::nullopt;
        result.message = "SupplyChainPlatform::dispatchHighestPriorityOrder: candidate order is no longer eligible or Pending immediately prior to dispatch: " + candidateId;
        return result;
    }

    // 6. Invoke OrderManager::dispatchHighestPriorityOrder() exactly once
    result.dispatchAttempted = true;
    const bool dispatchInvoked = m_orderManager.dispatchHighestPriorityOrder();
    if (!dispatchInvoked) {
        result.success = false;
        result.message = "SupplyChainPlatform::dispatchHighestPriorityOrder: OrderManager::dispatchHighestPriorityOrder returned false for order " + candidateId;
        const Order* postOrder = m_orderManager.getOrder(candidateId);
        result.resultingStatus = (postOrder != nullptr) ? std::optional<OrderStatus>(postOrder->getStatus()) : std::nullopt;
        return result;
    }

    // 7. Inspect actual resulting status and report real outcome
    const Order* finalOrder = m_orderManager.getOrder(candidateId);
    if (finalOrder != nullptr) {
        result.resultingStatus = finalOrder->getStatus();
        if (finalOrder->getStatus() == OrderStatus::Completed) {
            result.success = true;
            result.message = "SupplyChainPlatform::dispatchHighestPriorityOrder: order " + candidateId + " successfully dispatched and fulfilled";
        } else if (finalOrder->getStatus() == OrderStatus::Failed) {
            result.success = false;
            result.message = "SupplyChainPlatform::dispatchHighestPriorityOrder: order " + candidateId + " dispatched but fulfillment failed: " + finalOrder->getFailureReason();
        } else {
            result.success = false;
            result.message = "SupplyChainPlatform::dispatchHighestPriorityOrder: order " + candidateId + " dispatched with non-terminal status";
        }
    } else {
        result.success = false;
        result.resultingStatus = std::nullopt;
        result.message = "SupplyChainPlatform::dispatchHighestPriorityOrder: order " + candidateId + " disappeared from registry after dispatch";
    }

    return result;
}

} // namespace integration
