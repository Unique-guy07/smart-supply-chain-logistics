#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace packing {

/// 3D floating-point vector/point representation (e.g. for center of mass).
struct Vector3D
{
    double x{0.0};
    double y{0.0};
    double z{0.0};

    [[nodiscard]] bool operator==(const Vector3D& other) const noexcept
    {
        return std::abs(x - other.x) < 1e-9
            && std::abs(y - other.y) < 1e-9
            && std::abs(z - other.z) < 1e-9;
    }
};

/// 2D floating-point coordinate representation for floor footprint projections.
struct Point2D
{
    double x{0.0};
    double y{0.0};

    [[nodiscard]] bool operator==(const Point2D& other) const noexcept
    {
        return std::abs(x - other.x) < 1e-9
            && std::abs(y - other.y) < 1e-9;
    }
};

/// 3D integer coordinates for spatial origin and extreme points.
/// Uses Cartesian axes: x = width, y = depth, z = height.
struct Point3D
{
    int x{0};
    int y{0};
    int z{0};

    bool operator==(const Point3D& other) const noexcept = default;
    auto operator<=>(const Point3D& other) const noexcept = default;
};

/// 3D integer package or container dimensions.
struct Dimensions3D
{
    int width{0};  ///< Extent along x-axis
    int depth{0};  ///< Extent along y-axis
    int height{0}; ///< Extent along z-axis

    bool operator==(const Dimensions3D& other) const noexcept = default;

    [[nodiscard]] std::uint64_t volume() const noexcept
    {
        if (width <= 0 || depth <= 0 || height <= 0) {
            return 0;
        }
        return static_cast<std::uint64_t>(width)
             * static_cast<std::uint64_t>(depth)
             * static_cast<std::uint64_t>(height);
    }
};

/// Cargo package to be packed into a container.
/// Dimensions must be positive integers; weight must be finite and strictly positive (> 0.0).
struct Package
{
    std::string id;
    int width{0};
    int depth{0};
    int height{0};
    double weight{0.0};

    Package() = default;
    Package(std::string id, int width, int depth, int height, double weight);

    [[nodiscard]] std::uint64_t volume() const noexcept
    {
        if (width <= 0 || depth <= 0 || height <= 0) {
            return 0;
        }
        return static_cast<std::uint64_t>(width)
             * static_cast<std::uint64_t>(depth)
             * static_cast<std::uint64_t>(height);
    }

    [[nodiscard]] bool operator==(const Package& other) const noexcept
    {
        return id == other.id
            && width == other.width
            && depth == other.depth
            && height == other.height
            && std::abs(weight - other.weight) < 1e-9;
    }
};

/// Container representing the physical bounds [0, width) x [0, depth) x [0, height).
struct Container
{
    int width{0};
    int depth{0};
    int height{0};

    Container() = default;
    Container(int width, int depth, int height);

    [[nodiscard]] std::uint64_t volume() const noexcept
    {
        if (width <= 0 || depth <= 0 || height <= 0) {
            return 0;
        }
        return static_cast<std::uint64_t>(width)
             * static_cast<std::uint64_t>(depth)
             * static_cast<std::uint64_t>(height);
    }

    bool operator==(const Container& other) const noexcept = default;
};

/// Committed package placement within the container.
/// Extent occupies half-open interval [x, x + width) x [y, y + depth) x [z, z + height).
struct Placement
{
    std::string packageId;
    int x{0};
    int y{0};
    int z{0};
    int width{0};
    int depth{0};
    int height{0};
    double weight{0.0};

    [[nodiscard]] Point3D origin() const noexcept { return {x, y, z}; }
    [[nodiscard]] Dimensions3D dimensions() const noexcept { return {width, depth, height}; }

    [[nodiscard]] Vector3D center() const noexcept
    {
        return {
            static_cast<double>(x) + width / 2.0,
            static_cast<double>(y) + depth / 2.0,
            static_cast<double>(z) + height / 2.0
        };
    }

    [[nodiscard]] bool operator==(const Placement& other) const noexcept
    {
        return packageId == other.packageId
            && x == other.x
            && y == other.y
            && z == other.z
            && width == other.width
            && depth == other.depth
            && height == other.height
            && std::abs(weight - other.weight) < 1e-9;
    }
};

/// Configuration options and safety thresholds for 3D cargo packing.
struct PackingConfig
{
    std::size_t maxOccupancyCells{10'000'000}; ///< Safe upper bound on 3D occupancy grid cells (default 10M)
    std::size_t maxExtremePoints{50'000};      ///< Strict resource bound on candidate extreme points list (> 0)
    double centerOfMassTolerance{1e-6};        ///< Non-negative finite tolerance for polygon containment
};

/// Result of the 3D cargo packing execution.
struct PackingResult
{
    std::vector<Placement> placements;           ///< Successfully placed packages
    std::vector<std::string> unplacedPackageIds; ///< Valid packages that could not be placed
    bool allPackagesPlaced{false};               ///< True if every input package was successfully placed
    double totalPackedWeight{0.0};               ///< Sum of weights of placed packages (guaranteed finite)
    std::uint64_t totalPackedVolume{0};          ///< Sum of volumes of placed packages
    std::uint64_t containerVolume{0};            ///< Total container capacity volume
    Vector3D centerOfMass{0.0, 0.0, 0.0};        ///< Total cargo center of mass (guaranteed finite)
    std::string summary;                         ///< Diagnostic status summary
};

/// Sorts packages using custom Quick Sort algorithm in deterministic order:
/// descending volume, descending weight, then lexicographically ascending package ID.
///
/// Validation and Preconditions:
/// Every package in the collection is validated before any sorting or vector mutation occurs.
/// All packages must have non-empty/non-blank IDs, positive integer dimensions with non-overflowing
/// volume, and positive finite weights.
///
/// Exceptions:
/// Throws std::invalid_argument if any package has an empty/blank ID, non-positive dimensions,
/// volume multiplication overflow, or non-positive / non-finite weight.
/// If an exception is thrown, the caller's input vector remains completely unmodified (strong exception guarantee).
void quickSortPackages(std::vector<Package>& packages);

/// Computes the weight-weighted center of mass for a collection of placements.
/// Geometric center of package i is (x + width/2, y + depth/2, z + height/2).
///
/// Preconditions:
/// - If placements is empty, returns {0.0, 0.0, 0.0}.
/// - Each placement must have non-negative coordinates, positive dimensions, positive finite weight,
///   and coordinate-plus-dimension bounds that do not overflow maximum integer representation.
///
/// Exceptions:
/// - Throws std::invalid_argument if any placement has invalid dimensions, coordinates, or weight.
/// - Throws std::overflow_error if placement extent coordinates overflow integer bounds (INT_MAX),
///   or if total packed weight or weighted coordinate sums/products overflow double precision.
[[nodiscard]] Vector3D computeCenterOfMass(const std::vector<Placement>& placements);

/// Computes the 2D convex hull of floor-contact package footprint corners (z == 0)
/// using Andrew's monotone chain convex hull algorithm.
///
/// Preconditions:
/// Each placement must have non-negative coordinates, positive dimensions, positive finite weight,
/// and coordinate-plus-dimension bounds that do not overflow maximum integer representation.
/// Returns an empty vector if placements is empty or contains no packages at floor level (z == 0).
///
/// Exceptions:
/// - Throws std::invalid_argument if any placement has invalid dimensions, coordinates, or weight.
/// - Throws std::overflow_error if coordinate plus dimension exceeds maximum integer bounds (INT_MAX).
[[nodiscard]] std::vector<Point2D> computeFloorSupportPolygon(const std::vector<Placement>& placements);

/// Tests whether a 2D point lies inside or on the boundary of a 2D convex polygon within tolerance.
///
/// Preconditions:
/// - tolerance must be non-negative and finite.
/// - pt and all polygon vertices must have finite coordinates.
///
/// Exceptions:
/// Throws std::invalid_argument if tolerance is negative/non-finite or point/vertex coordinates are non-finite.
[[nodiscard]] bool isPointInPolygon(const Point2D& pt, const std::vector<Point2D>& polygon, double tolerance = 1e-6);

/// Validates whether the cargo load's center-of-mass vertical projection lies within the floor support polygon.
///
/// Preconditions:
/// - tolerance must be non-negative and finite.
/// - Returns true if placements is empty.
/// - Returns false if there is no floor contact (no packages at z == 0).
///
/// Exceptions:
/// - Throws std::invalid_argument if tolerance is negative/non-finite or any placement is invalid.
/// - Throws std::overflow_error if center-of-mass arithmetic overflows double precision.
[[nodiscard]] bool isCenterOfMassStable(const std::vector<Placement>& placements, double tolerance = 1e-6);

/// Primary entry point: Packs packages into a container using Quick Sort, Extreme Points,
/// Bottom-Left-Fill (BLF), full direct physical support, and center-of-mass validation.
///
/// Rotation Policy:
/// Package rotation is NOT performed in this initial version. Packages are placed in their
/// supplied orientations (width along x, depth along y, height along z).
///
/// Exceptions:
/// Throws std::invalid_argument if:
///   - Container dimensions are non-positive or exceed maxOccupancyCells or cause multiplication overflow.
///   - Any package has an empty or blank ID.
///   - Any duplicate package ID exists.
///   - Any package has non-positive dimensions or non-positive / non-finite weight.
///   - config.maxExtremePoints == 0.
///   - config.centerOfMassTolerance is negative or non-finite.
///
/// Throws std::length_error if:
///   - The candidate extreme points pool exceeds config.maxExtremePoints, indicating that
///     the search space cannot be fully evaluated under the configured resource limit.
///     Potentially feasible candidates are never silently discarded.
///
/// Throws std::overflow_error if:
///   - Final aggregate packed weight or center-of-mass arithmetic cannot be represented in finite precision.
[[nodiscard]] PackingResult packCargo(const Container& container,
                                      const std::vector<Package>& packages,
                                      const PackingConfig& config = PackingConfig{});

/// Object-oriented wrapper around packCargo.
class CargoPacker
{
public:
    static PackingResult pack(const Container& container,
                              const std::vector<Package>& packages,
                              const PackingConfig& config = PackingConfig{})
    {
        return packCargo(container, packages, config);
    }
};

} // namespace packing
