#include "packing/CargoPacking.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace packing {

Package::Package(std::string id, int width, int depth, int height, double weight)
    : id(std::move(id)), width(width), depth(depth), height(height), weight(weight)
{
}

Container::Container(int width, int depth, int height)
    : width(width), depth(depth), height(height)
{
}

static bool isBlank(std::string_view str) noexcept
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

static bool multiplySafe(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept
{
    if (a == 0 || b == 0) {
        out = 0;
        return true;
    }
    if (a > std::numeric_limits<std::uint64_t>::max() / b) {
        return false;
    }
    out = a * b;
    return true;
}

// --- Quick Sort Implementation ---

static bool packageSortComparator(const Package& a, const Package& b) noexcept
{
    const std::uint64_t volA = a.volume();
    const std::uint64_t volB = b.volume();
    if (volA != volB) {
        return volA > volB; // Descending volume
    }
    if (a.weight != b.weight) {
        return a.weight > b.weight; // Descending weight
    }
    return a.id < b.id; // Ascending package ID (lexicographical tie-breaker)
}

static int partition(std::vector<Package>& arr, int low, int high)
{
    if (high - low >= 2) {
        int mid = low + (high - low) / 2;
        if (packageSortComparator(arr[mid], arr[low])) {
            std::swap(arr[low], arr[mid]);
        }
        if (packageSortComparator(arr[high], arr[low])) {
            std::swap(arr[low], arr[high]);
        }
        if (packageSortComparator(arr[high], arr[mid])) {
            std::swap(arr[mid], arr[high]);
        }
        std::swap(arr[mid], arr[high]);
    }

    const Package pivot = arr[high];
    int i = low - 1;
    for (int j = low; j < high; ++j) {
        if (packageSortComparator(arr[j], pivot)) {
            ++i;
            std::swap(arr[i], arr[j]);
        }
    }
    std::swap(arr[i + 1], arr[high]);
    return i + 1;
}

static void quickSortImpl(std::vector<Package>& arr, int low, int high)
{
    while (low < high) {
        int pi = partition(arr, low, high);

        // Tail-call recursion elimination on smaller partition
        if (pi - low < high - pi) {
            quickSortImpl(arr, low, pi - 1);
            low = pi + 1;
        } else {
            quickSortImpl(arr, pi + 1, high);
            high = pi - 1;
        }
    }
}

static void validatePackage(const Package& pkg)
{
    if (isBlank(pkg.id)) {
        throw std::invalid_argument("CargoPacking: package ID must not be empty or blank");
    }
    if (pkg.width <= 0 || pkg.depth <= 0 || pkg.height <= 0) {
        throw std::invalid_argument("CargoPacking: package dimensions must be positive integers (id=" + pkg.id + ")");
    }
    std::uint64_t step1 = 0;
    if (!multiplySafe(static_cast<std::uint64_t>(pkg.width),
                      static_cast<std::uint64_t>(pkg.depth),
                      step1)) {
        throw std::invalid_argument("CargoPacking: package volume overflow (id=" + pkg.id + ")");
    }
    std::uint64_t pVol = 0;
    if (!multiplySafe(step1, static_cast<std::uint64_t>(pkg.height), pVol)) {
        throw std::invalid_argument("CargoPacking: package volume overflow (id=" + pkg.id + ")");
    }
    if (pkg.weight <= 0.0 || !std::isfinite(pkg.weight)) {
        throw std::invalid_argument("CargoPacking: package weight must be positive and finite (id=" + pkg.id + ")");
    }
}

void quickSortPackages(std::vector<Package>& packages)
{
    // Validate every package before mutating caller's vector (strong exception guarantee)
    for (const auto& pkg : packages) {
        validatePackage(pkg);
    }

    if (packages.size() <= 1) {
        return;
    }
    quickSortImpl(packages, 0, static_cast<int>(packages.size()) - 1);
}


// --- Bounded 3D Occupancy Matrix ---

class OccupancyGrid
{
private:
    int width_{0};
    int depth_{0};
    int height_{0};
    std::vector<std::uint8_t> cells_;

    [[nodiscard]] std::size_t index(int x, int y, int z) const noexcept
    {
        return (static_cast<std::size_t>(z) * depth_ + static_cast<std::size_t>(y)) * width_ + static_cast<std::size_t>(x);
    }

public:
    OccupancyGrid(int w, int d, int h, std::size_t maxCells)
        : width_(w), depth_(d), height_(h)
    {
        if (w <= 0 || d <= 0 || h <= 0) {
            throw std::invalid_argument("OccupancyGrid: dimensions must be positive integers");
        }
        std::uint64_t step1 = 0;
        if (!multiplySafe(static_cast<std::uint64_t>(w), static_cast<std::uint64_t>(d), step1)) {
            throw std::invalid_argument("OccupancyGrid: integer overflow in container dimensions");
        }
        std::uint64_t totalCells = 0;
        if (!multiplySafe(step1, static_cast<std::uint64_t>(h), totalCells)) {
            throw std::invalid_argument("OccupancyGrid: integer overflow in container dimensions");
        }
        if (totalCells > maxCells) {
            throw std::invalid_argument("OccupancyGrid: total cells exceed limit ("
                                        + std::to_string(totalCells) + " > " + std::to_string(maxCells) + ")");
        }
        cells_.resize(static_cast<std::size_t>(totalCells), 0);
    }

    [[nodiscard]] bool isOccupied(int x, int y, int z) const noexcept
    {
        if (x < 0 || x >= width_ || y < 0 || y >= depth_ || z < 0 || z >= height_) {
            return false;
        }
        return cells_[index(x, y, z)] != 0;
    }

    [[nodiscard]] bool canFit(int x, int y, int z, int w, int d, int h) const noexcept
    {
        if (x < 0 || y < 0 || z < 0 || w <= 0 || d <= 0 || h <= 0) return false;
        if (static_cast<std::int64_t>(x) + w > width_ ||
            static_cast<std::int64_t>(y) + d > depth_ ||
            static_cast<std::int64_t>(z) + h > height_) return false;

        for (int k = z; k < z + h; ++k) {
            for (int j = y; j < y + d; ++j) {
                for (int i = x; i < x + w; ++i) {
                    if (cells_[index(i, j, k)] != 0) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    [[nodiscard]] bool isSupported(int x, int y, int z, int w, int d) const noexcept
    {
        if (x < 0 || y < 0 || z < 0 || w <= 0 || d <= 0) return false;
        if (static_cast<std::int64_t>(x) + w > width_ ||
            static_cast<std::int64_t>(y) + d > depth_) return false;

        if (z == 0) {
            return true; // Supported by container floor
        }
        // Elevated: entire bottom footprint immediately below (at z - 1) must be occupied
        for (int j = y; j < y + d; ++j) {
            for (int i = x; i < x + w; ++i) {
                if (cells_[index(i, j, z - 1)] == 0) {
                    return false;
                }
            }
        }
        return true;
    }

    void mark(int x, int y, int z, int w, int d, int h, bool occupied) noexcept
    {
        const std::uint8_t val = occupied ? 1 : 0;
        for (int k = z; k < z + h; ++k) {
            for (int j = y; j < y + d; ++j) {
                for (int i = x; i < x + w; ++i) {
                    cells_[index(i, j, k)] = val;
                }
            }
        }
    }
};

// --- Center of Mass and Convex Hull Geometry ---

Vector3D computeCenterOfMass(const std::vector<Placement>& placements)
{
    if (placements.empty()) {
        return {0.0, 0.0, 0.0};
    }

    double totalWeight = 0.0;
    double sumX = 0.0;
    double sumY = 0.0;
    double sumZ = 0.0;

    for (const auto& p : placements) {
        if (p.weight <= 0.0 || !std::isfinite(p.weight)) {
            throw std::invalid_argument("CargoPacking: placement weight must be positive and finite (id=" + p.packageId + ")");
        }
        if (p.width <= 0 || p.depth <= 0 || p.height <= 0) {
            throw std::invalid_argument("CargoPacking: placement dimensions must be positive integers (id=" + p.packageId + ")");
        }
        if (p.x < 0 || p.y < 0 || p.z < 0) {
            throw std::invalid_argument("CargoPacking: placement coordinates must be non-negative (id=" + p.packageId + ")");
        }

        constexpr std::int64_t kIntMax = std::numeric_limits<int>::max();
        const std::int64_t maxX = static_cast<std::int64_t>(p.x) + p.width;
        const std::int64_t maxY = static_cast<std::int64_t>(p.y) + p.depth;
        const std::int64_t maxZ = static_cast<std::int64_t>(p.z) + p.height;
        if (maxX > kIntMax || maxY > kIntMax || maxZ > kIntMax) {
            throw std::overflow_error("CargoPacking: placement extent coordinates overflow integer bounds (id=" + p.packageId + ")");
        }

        Vector3D c = p.center();
        if (!std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.z)) {
            throw std::overflow_error("CargoPacking: placement center calculation overflow (id=" + p.packageId + ")");
        }

        double nextTotalWeight = totalWeight + p.weight;
        if (!std::isfinite(nextTotalWeight)) {
            throw std::overflow_error("CargoPacking: total weight arithmetic overflow");
        }
        totalWeight = nextTotalWeight;

        double wx = p.weight * c.x;
        double wy = p.weight * c.y;
        double wz = p.weight * c.z;
        if (!std::isfinite(wx) || !std::isfinite(wy) || !std::isfinite(wz)) {
            throw std::overflow_error("CargoPacking: weighted coordinate multiplication overflow");
        }

        double nextSumX = sumX + wx;
        double nextSumY = sumY + wy;
        double nextSumZ = sumZ + wz;
        if (!std::isfinite(nextSumX) || !std::isfinite(nextSumY) || !std::isfinite(nextSumZ)) {
            throw std::overflow_error("CargoPacking: weighted coordinate sum overflow");
        }

        sumX = nextSumX;
        sumY = nextSumY;
        sumZ = nextSumZ;
    }

    if (totalWeight <= 0.0 || !std::isfinite(totalWeight)) {
        throw std::overflow_error("CargoPacking: invalid total weight in center of mass calculation");
    }

    double comX = sumX / totalWeight;
    double comY = sumY / totalWeight;
    double comZ = sumZ / totalWeight;

    if (!std::isfinite(comX) || !std::isfinite(comY) || !std::isfinite(comZ)) {
        throw std::overflow_error("CargoPacking: center of mass quotient overflow");
    }

    return {comX, comY, comZ};
}

static double cross2D(const Point2D& O, const Point2D& A, const Point2D& B) noexcept
{
    return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
}

static std::vector<Point2D> compute2DConvexHull(std::vector<Point2D>& points)
{
    if (points.size() <= 2) {
        return points;
    }

    // Sort points lexicographically by x, then y
    std::sort(points.begin(), points.end(), [](const Point2D& a, const Point2D& b) {
        if (std::abs(a.x - b.x) > 1e-9) {
            return a.x < b.x;
        }
        return a.y < b.y;
    });

    // Remove duplicates
    auto last = std::unique(points.begin(), points.end(), [](const Point2D& a, const Point2D& b) {
        return std::abs(a.x - b.x) < 1e-9 && std::abs(a.y - b.y) < 1e-9;
    });
    points.erase(last, points.end());

    const std::size_t n = points.size();
    if (n <= 2) {
        return points;
    }

    std::vector<Point2D> hull;
    hull.reserve(2 * n);

    // Build lower hull
    for (std::size_t i = 0; i < n; ++i) {
        while (hull.size() >= 2 && cross2D(hull[hull.size() - 2], hull[hull.size() - 1], points[i]) <= 1e-9) {
            hull.pop_back();
        }
        hull.push_back(points[i]);
    }

    // Build upper hull
    const std::size_t lowerSize = hull.size();
    for (std::size_t i = n - 1; i > 0; --i) {
        const std::size_t idx = i - 1;
        while (hull.size() > lowerSize && cross2D(hull[hull.size() - 2], hull[hull.size() - 1], points[idx]) <= 1e-9) {
            hull.pop_back();
        }
        hull.push_back(points[idx]);
    }

    if (!hull.empty()) {
        hull.pop_back();
    }

    return hull;
}

std::vector<Point2D> computeFloorSupportPolygon(const std::vector<Placement>& placements)
{
    std::vector<Point2D> floorPoints;
    for (const auto& p : placements) {
        if (p.width <= 0 || p.depth <= 0 || p.height <= 0) {
            throw std::invalid_argument("CargoPacking: placement dimensions must be positive integers (id=" + p.packageId + ")");
        }
        if (p.x < 0 || p.y < 0 || p.z < 0) {
            throw std::invalid_argument("CargoPacking: placement coordinates must be non-negative (id=" + p.packageId + ")");
        }
        if (p.weight <= 0.0 || !std::isfinite(p.weight)) {
            throw std::invalid_argument("CargoPacking: placement weight must be positive and finite (id=" + p.packageId + ")");
        }

        constexpr std::int64_t kIntMax = std::numeric_limits<int>::max();
        const std::int64_t maxX = static_cast<std::int64_t>(p.x) + p.width;
        const std::int64_t maxY = static_cast<std::int64_t>(p.y) + p.depth;
        const std::int64_t maxZ = static_cast<std::int64_t>(p.z) + p.height;
        if (maxX > kIntMax || maxY > kIntMax || maxZ > kIntMax) {
            throw std::overflow_error("CargoPacking: placement extent coordinates overflow integer bounds (id=" + p.packageId + ")");
        }

        if (p.z == 0) { // Resting on container floor
            floorPoints.push_back({static_cast<double>(p.x), static_cast<double>(p.y)});
            floorPoints.push_back({static_cast<double>(maxX), static_cast<double>(p.y)});
            floorPoints.push_back({static_cast<double>(maxX), static_cast<double>(maxY)});
            floorPoints.push_back({static_cast<double>(p.x), static_cast<double>(maxY)});
        }
    }
    if (floorPoints.empty()) {
        return {};
    }
    return compute2DConvexHull(floorPoints);
}

bool isPointInPolygon(const Point2D& pt, const std::vector<Point2D>& polygon, double tolerance)
{
    if (tolerance < 0.0 || !std::isfinite(tolerance)) {
        throw std::invalid_argument("CargoPacking: tolerance must be non-negative and finite");
    }
    if (!std::isfinite(pt.x) || !std::isfinite(pt.y)) {
        throw std::invalid_argument("CargoPacking: query point coordinates must be finite");
    }
    for (const auto& v : polygon) {
        if (!std::isfinite(v.x) || !std::isfinite(v.y)) {
            throw std::invalid_argument("CargoPacking: polygon vertices must be finite");
        }
    }

    if (polygon.empty()) {
        return false;
    }
    if (polygon.size() == 1) {
        return std::abs(pt.x - polygon[0].x) <= tolerance && std::abs(pt.y - polygon[0].y) <= tolerance;
    }
    if (polygon.size() == 2) {
        const Point2D& A = polygon[0];
        const Point2D& B = polygon[1];
        double cross = cross2D(A, B, pt);
        if (!std::isfinite(cross) || std::abs(cross) > tolerance) {
            return false;
        }
        double minX = std::min(A.x, B.x) - tolerance;
        double maxX = std::max(A.x, B.x) + tolerance;
        double minY = std::min(A.y, B.y) - tolerance;
        double maxY = std::max(A.y, B.y) + tolerance;
        return pt.x >= minX && pt.x <= maxX && pt.y >= minY && pt.y <= maxY;
    }

    // For convex polygon with vertices in CCW order:
    // pt must lie to the left of or on each directed edge: cross2D(V_i, V_{i+1}, pt) >= -tolerance
    const std::size_t k = polygon.size();
    for (std::size_t i = 0; i < k; ++i) {
        const Point2D& vi = polygon[i];
        const Point2D& viNext = polygon[(i + 1) % k];
        double cp = cross2D(vi, viNext, pt);
        if (!std::isfinite(cp) || cp < -tolerance) {
            return false;
        }
    }
    return true;
}

bool isCenterOfMassStable(const std::vector<Placement>& placements, double tolerance)
{
    if (tolerance < 0.0 || !std::isfinite(tolerance)) {
        throw std::invalid_argument("CargoPacking: tolerance must be non-negative and finite");
    }
    if (placements.empty()) {
        return true;
    }
    auto polygon = computeFloorSupportPolygon(placements);
    if (polygon.empty()) {
        return false; // No floor contact -> unstable
    }
    Vector3D com = computeCenterOfMass(placements);
    Point2D pt{com.x, com.y};
    return isPointInPolygon(pt, polygon, tolerance);
}

// --- Extreme Points Update ---

static bool compareExtremePoints(const Point3D& a, const Point3D& b) noexcept
{
    if (a.z != b.z) {
        return a.z < b.z; // Lower height first (Bottom)
    }
    if (a.y != b.y) {
        return a.y < b.y; // Lower depth first (Back/Front)
    }
    return a.x < b.x;     // Lower width first (Left)
}

static void updateExtremePoints(std::vector<Point3D>& points,
                                const Placement& p,
                                const Container& container,
                                const OccupancyGrid& grid,
                                std::size_t maxPoints)
{
    // Remove points falling inside the newly occupied package volume
    auto it = std::remove_if(points.begin(), points.end(), [&](const Point3D& pt) {
        return pt.x >= p.x && static_cast<std::int64_t>(pt.x) < static_cast<std::int64_t>(p.x) + p.width
            && pt.y >= p.y && static_cast<std::int64_t>(pt.y) < static_cast<std::int64_t>(p.y) + p.depth
            && pt.z >= p.z && static_cast<std::int64_t>(pt.z) < static_cast<std::int64_t>(p.z) + p.height;
    });
    points.erase(it, points.end());

    // Generate up to 3 face-adjacent candidate points in fixed-size local stack storage (capacity 3)
    const std::int64_t candX = static_cast<std::int64_t>(p.x) + p.width;
    const std::int64_t candY = static_cast<std::int64_t>(p.y) + p.depth;
    const std::int64_t candZ = static_cast<std::int64_t>(p.z) + p.height;

    Point3D candidates[3];
    int candidateCount = 0;

    if (candX < container.width) {
        candidates[candidateCount++] = {static_cast<int>(candX), p.y, p.z};
    }
    if (candY < container.depth) {
        candidates[candidateCount++] = {p.x, static_cast<int>(candY), p.z};
    }
    if (candZ < container.height) {
        candidates[candidateCount++] = {p.x, p.y, static_cast<int>(candZ)};
    }

    // Filter valid, unoccupied, non-duplicate candidates and enforce resource bound BEFORE vector growth
    for (int i = 0; i < candidateCount; ++i) {
        const auto& cand = candidates[i];
        if (!grid.isOccupied(cand.x, cand.y, cand.z)) {
            // Check if candidate point is already present in points
            bool exists = false;
            for (const auto& pt : points) {
                if (pt.x == cand.x && pt.y == cand.y && pt.z == cand.z) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                // Strictly enforce maxPoints BEFORE adding to points vector
                if (points.size() >= maxPoints) {
                    throw std::length_error("CargoPacking: candidate extreme points limit exceeded (max: "
                                            + std::to_string(maxPoints) + ")");
                }
                points.push_back(cand);
            }
        }
    }

    // Sort in deterministic BLF order
    std::sort(points.begin(), points.end(), compareExtremePoints);
}

// --- Primary Packing Entry Point ---

PackingResult packCargo(const Container& container,
                        const std::vector<Package>& packages,
                        const PackingConfig& config)
{
    // 1. Validate configuration parameters
    if (config.maxExtremePoints == 0) {
        throw std::invalid_argument("CargoPacking: maxExtremePoints must be positive");
    }
    if (config.centerOfMassTolerance < 0.0 || !std::isfinite(config.centerOfMassTolerance)) {
        throw std::invalid_argument("CargoPacking: centerOfMassTolerance must be non-negative and finite");
    }

    // 2. Validate container dimensions
    if (container.width <= 0 || container.depth <= 0 || container.height <= 0) {
        throw std::invalid_argument("CargoPacking: container dimensions must be positive integers");
    }
    std::uint64_t step1 = 0;
    if (!multiplySafe(static_cast<std::uint64_t>(container.width),
                      static_cast<std::uint64_t>(container.depth),
                      step1)) {
        throw std::invalid_argument("CargoPacking: container dimensions multiplication overflow");
    }
    std::uint64_t containerVol = 0;
    if (!multiplySafe(step1, static_cast<std::uint64_t>(container.height), containerVol)) {
        throw std::invalid_argument("CargoPacking: container dimensions multiplication overflow");
    }
    if (containerVol > config.maxOccupancyCells) {
        throw std::invalid_argument("CargoPacking: container volume (" + std::to_string(containerVol)
                                    + ") exceeds maximum occupancy grid capacity ("
                                    + std::to_string(config.maxOccupancyCells) + ")");
    }

    // 3. Validate packages
    std::unordered_set<std::string> seenIds;
    seenIds.reserve(packages.size());
    for (const auto& pkg : packages) {
        validatePackage(pkg);
        if (!seenIds.insert(pkg.id).second) {
            throw std::invalid_argument("CargoPacking: duplicate package ID: " + pkg.id);
        }
    }

    PackingResult result;
    result.containerVolume = containerVol;

    if (packages.empty()) {
        result.allPackagesPlaced = true;
        result.summary = "Empty package list; packing complete.";
        return result;
    }

    // 4. Sort packages via custom Quick Sort
    std::vector<Package> sortedPackages = packages;
    quickSortPackages(sortedPackages);

    // 5. Initialize Occupancy Grid and Extreme Points List
    OccupancyGrid grid(container.width, container.depth, container.height, config.maxOccupancyCells);
    std::vector<Point3D> extremePoints = {{0, 0, 0}};

    // 6. Bottom-Left-Fill (BLF) packing loop
    for (const auto& pkg : sortedPackages) {
        bool placed = false;

        // Candidate extreme points are in BLF order (lower z, then lower y, then lower x)
        for (const auto& pt : extremePoints) {
            // A. Check container bounds and cell collisions
            if (!grid.canFit(pt.x, pt.y, pt.z, pkg.width, pkg.depth, pkg.height)) {
                continue;
            }

            // B. Check direct physical support
            if (!grid.isSupported(pt.x, pt.y, pt.z, pkg.width, pkg.depth)) {
                continue;
            }

            // C. Transactional trial: mark occupancy and evaluate center-of-mass stability
            grid.mark(pt.x, pt.y, pt.z, pkg.width, pkg.depth, pkg.height, true);

            Placement candidatePlacement{
                pkg.id,
                pt.x, pt.y, pt.z,
                pkg.width, pkg.depth, pkg.height,
                pkg.weight
            };
            result.placements.push_back(candidatePlacement);

            bool stable = false;
            try {
                stable = isCenterOfMassStable(result.placements, config.centerOfMassTolerance);
            } catch (const std::overflow_error&) {
                // Aggregate arithmetic overflow: candidate cannot be safely approved
                stable = false;
            }

            if (!stable) {
                // Roll back candidate placement and occupancy changes
                result.placements.pop_back();
                grid.mark(pt.x, pt.y, pt.z, pkg.width, pkg.depth, pkg.height, false);
                continue;
            }

            // Successfully committed placement: update extreme points with resource check
            updateExtremePoints(extremePoints, candidatePlacement, container, grid, config.maxExtremePoints);
            placed = true;
            break;
        }

        if (!placed) {
            result.unplacedPackageIds.push_back(pkg.id);
        }
    }

    // 7. Final diagnostic summary and validation
    result.allPackagesPlaced = result.unplacedPackageIds.empty();
    result.totalPackedWeight = 0.0;
    result.totalPackedVolume = 0;
    for (const auto& p : result.placements) {
        result.totalPackedWeight += p.weight;
        result.totalPackedVolume += p.dimensions().volume();
    }
    if (!std::isfinite(result.totalPackedWeight)) {
        throw std::overflow_error("CargoPacking: aggregate total packed weight is non-finite");
    }
    result.centerOfMass = computeCenterOfMass(result.placements);
    if (!result.placements.empty() && (!std::isfinite(result.centerOfMass.x) ||
                                       !std::isfinite(result.centerOfMass.y) ||
                                       !std::isfinite(result.centerOfMass.z))) {
        throw std::overflow_error("CargoPacking: aggregate center of mass is non-finite");
    }

    result.summary = "Packed " + std::to_string(result.placements.size()) + "/"
                   + std::to_string(packages.size()) + " packages ("
                   + std::to_string(result.unplacedPackageIds.size()) + " unplaced). Total weight: "
                   + std::to_string(result.totalPackedWeight) + ". Total volume: "
                   + std::to_string(result.totalPackedVolume) + "/" + std::to_string(containerVol) + ".";

    return result;
}

} // namespace packing
