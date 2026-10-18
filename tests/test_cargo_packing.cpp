#include "test_utils.h"
#include "packing/CargoPacking.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace packing;

void testEmptyPackageCollection()
{
    Container container(10, 10, 10);
    std::vector<Package> packages;

    PackingResult result = packCargo(container, packages);
    TEST_CHECK(result.placements.empty());
    TEST_CHECK(result.unplacedPackageIds.empty());
    TEST_CHECK(result.allPackagesPlaced);
    TEST_CHECK(result.totalPackedWeight == 0.0);
    TEST_CHECK(result.totalPackedVolume == 0);
    TEST_CHECK(result.containerVolume == 1000);
}

void testSinglePackageExactFit()
{
    Container container(10, 10, 10);
    std::vector<Package> packages = {
        Package("P_EXACT", 10, 10, 10, 50.0)
    };

    PackingResult result = packCargo(container, packages);
    TEST_CHECK(result.allPackagesPlaced);
    TEST_CHECK(result.placements.size() == 1);
    TEST_CHECK(result.unplacedPackageIds.empty());

    const auto& p = result.placements[0];
    TEST_CHECK(p.packageId == "P_EXACT");
    TEST_CHECK(p.x == 0 && p.y == 0 && p.z == 0);
    TEST_CHECK(p.width == 10 && p.depth == 10 && p.height == 10);
    TEST_CHECK(p.weight == 50.0);
    TEST_CHECK(result.totalPackedVolume == 1000);
    TEST_CHECK(result.totalPackedWeight == 50.0);

    Vector3D expectedCoM{5.0, 5.0, 5.0};
    TEST_CHECK(result.centerOfMass == expectedCoM);
}

void testPackageLargerThanContainer()
{
    Container container(10, 10, 10);
    std::vector<Package> packages = {
        Package("P_OVERSIZED", 12, 10, 10, 25.0)
    };

    PackingResult result = packCargo(container, packages);
    TEST_CHECK(!result.allPackagesPlaced);
    TEST_CHECK(result.placements.empty());
    TEST_CHECK(result.unplacedPackageIds.size() == 1);
    TEST_CHECK(result.unplacedPackageIds[0] == "P_OVERSIZED");
    TEST_CHECK(result.totalPackedVolume == 0);
    TEST_CHECK(result.totalPackedWeight == 0.0);
}

void testInvalidContainerDimensions()
{
    // Width <= 0
    bool threw = false;
    try {
        Container c(0, 10, 10);
        [[maybe_unused]] auto res = packCargo(c, {});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Depth <= 0
    threw = false;
    try {
        Container c(10, -5, 10);
        [[maybe_unused]] auto res = packCargo(c, {});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Height <= 0
    threw = false;
    try {
        Container c(10, 10, 0);
        [[maybe_unused]] auto res = packCargo(c, {});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);
}

void testEmptyAndDuplicatePackageIds()
{
    Container container(10, 10, 10);

    // Empty package ID
    bool threw = false;
    try {
        std::vector<Package> pkgs = { Package("", 2, 2, 2, 10.0) };
        [[maybe_unused]] auto res = packCargo(container, pkgs);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Whitespace-only package ID
    threw = false;
    try {
        std::vector<Package> pkgs = { Package("   \t ", 2, 2, 2, 10.0) };
        [[maybe_unused]] auto res = packCargo(container, pkgs);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Duplicate package ID
    threw = false;
    try {
        std::vector<Package> pkgs = {
            Package("P1", 2, 2, 2, 5.0),
            Package("P1", 3, 3, 3, 8.0)
        };
        [[maybe_unused]] auto res = packCargo(container, pkgs);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);
}

void testZeroNegativeDimensionsAndInvalidWeights()
{
    Container container(10, 10, 10);

    // Zero width
    bool threw = false;
    try {
        std::vector<Package> pkgs = { Package("P_BAD_W", 0, 2, 2, 10.0) };
        [[maybe_unused]] auto res = packCargo(container, pkgs);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Negative depth
    threw = false;
    try {
        std::vector<Package> pkgs = { Package("P_BAD_D", 2, -1, 2, 10.0) };
        [[maybe_unused]] auto res = packCargo(container, pkgs);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Zero height
    threw = false;
    try {
        std::vector<Package> pkgs = { Package("P_BAD_H", 2, 2, 0, 10.0) };
        [[maybe_unused]] auto res = packCargo(container, pkgs);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Zero weight
    threw = false;
    try {
        std::vector<Package> pkgs = { Package("P_ZERO_WT", 2, 2, 2, 0.0) };
        [[maybe_unused]] auto res = packCargo(container, pkgs);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Negative weight
    threw = false;
    try {
        std::vector<Package> pkgs = { Package("P_NEG_WT", 2, 2, 2, -5.0) };
        [[maybe_unused]] auto res = packCargo(container, pkgs);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // NaN weight
    threw = false;
    try {
        std::vector<Package> pkgs = { Package("P_NAN_WT", 2, 2, 2, std::numeric_limits<double>::quiet_NaN()) };
        [[maybe_unused]] auto res = packCargo(container, pkgs);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Infinity weight
    threw = false;
    try {
        std::vector<Package> pkgs = { Package("P_INF_WT", 2, 2, 2, std::numeric_limits<double>::infinity()) };
        [[maybe_unused]] auto res = packCargo(container, pkgs);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);
}

void testMultiplePackagesPlacedWithoutOverlap()
{
    Container container(6, 6, 6);
    std::vector<Package> packages = {
        Package("P1", 2, 2, 2, 10.0),
        Package("P2", 2, 2, 2, 10.0),
        Package("P3", 2, 2, 2, 10.0),
        Package("P4", 2, 2, 2, 10.0)
    };

    PackingResult result = packCargo(container, packages);
    TEST_CHECK(result.allPackagesPlaced);
    TEST_CHECK(result.placements.size() == 4);

    // Assert pairwise non-overlap using half-open intervals [x, x+w) x [y, y+d) x [z, z+h)
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        for (std::size_t j = i + 1; j < result.placements.size(); ++j) {
            const auto& a = result.placements[i];
            const auto& b = result.placements[j];

            const bool disjointX = (a.x + a.width <= b.x) || (b.x + b.width <= a.x);
            const bool disjointY = (a.y + a.depth <= b.y) || (b.y + b.depth <= a.y);
            const bool disjointZ = (a.z + a.height <= b.z) || (b.z + b.height <= a.z);

            const bool overlaps = (!disjointX) && (!disjointY) && (!disjointZ);
            TEST_CHECK(!overlaps);
        }
    }
}

void testFaceTouchingPackagesAccepted()
{
    Container container(6, 4, 4);
    // Two packages placed side-by-side touching at face x = 2
    std::vector<Package> packages = {
        Package("P1", 2, 2, 2, 10.0),
        Package("P2", 2, 2, 2, 10.0)
    };

    PackingResult result = packCargo(container, packages);
    TEST_CHECK(result.allPackagesPlaced);
    TEST_CHECK(result.placements.size() == 2);

    const auto& p1 = result.placements[0];
    const auto& p2 = result.placements[1];

    // Both should be placed on floor z = 0
    TEST_CHECK(p1.z == 0 && p2.z == 0);
    // One touches the face of the other along x-axis
    const bool touchesOnX = (p1.x + p1.width == p2.x) || (p2.x + p2.width == p1.x);
    TEST_CHECK(touchesOnX);
}

void testContainerBoundaryChecks()
{
    Container container(5, 5, 5);
    std::vector<Package> packages = {
        Package("P1", 3, 3, 3, 10.0),
        Package("P2", 3, 3, 3, 10.0) // Second package cannot fit alongside P1 in 5x5x5
    };

    PackingResult result = packCargo(container, packages);
    // Both 3x3x3 packages take 27 volume each, total 54 > 125, but boundary constraints in each dimension:
    // P1 at (0,0,0) extends to (3,3,3). Extreme points: (3,0,0), (0,3,0), (0,0,3).
    // At (3,0,0): 3 + 3 = 6 > 5 (exceeds width!)
    // At (0,3,0): 3 + 3 = 6 > 5 (exceeds depth!)
    // At (0,0,3): 3 + 3 = 6 > 5 (exceeds height!)
    // P2 cannot fit in any boundary!
    TEST_CHECK(!result.allPackagesPlaced);
    TEST_CHECK(result.placements.size() == 1);
    TEST_CHECK(result.unplacedPackageIds.size() == 1);
    TEST_CHECK(result.unplacedPackageIds[0] == "P2");
}

void testDeterministicQuickSortOrdering()
{
    std::vector<Package> packages = {
        Package("P_SMALL", 1, 1, 1, 10.0),  // Vol 1
        Package("P_VOL8_LIGHT", 2, 2, 2, 5.0), // Vol 8, wt 5
        Package("P_VOL8_HEAVY", 2, 2, 2, 20.0), // Vol 8, wt 20
        Package("P_VOL27", 3, 3, 3, 15.0),     // Vol 27
        Package("P_VOL8_MID_B", 2, 2, 2, 10.0), // Vol 8, wt 10, ID B
        Package("P_VOL8_MID_A", 2, 2, 2, 10.0)  // Vol 8, wt 10, ID A
    };

    quickSortPackages(packages);

    // Expected order:
    // 1. P_VOL27 (Vol 27)
    // 2. P_VOL8_HEAVY (Vol 8, wt 20)
    // 3. P_VOL8_MID_A (Vol 8, wt 10, ID A)
    // 4. P_VOL8_MID_B (Vol 8, wt 10, ID B)
    // 5. P_VOL8_LIGHT (Vol 8, wt 5)
    // 6. P_SMALL (Vol 1)
    TEST_CHECK(packages[0].id == "P_VOL27");
    TEST_CHECK(packages[1].id == "P_VOL8_HEAVY");
    TEST_CHECK(packages[2].id == "P_VOL8_MID_A");
    TEST_CHECK(packages[3].id == "P_VOL8_MID_B");
    TEST_CHECK(packages[4].id == "P_VOL8_LIGHT");
    TEST_CHECK(packages[5].id == "P_SMALL");

    // Test empty vector and single element vector
    std::vector<Package> emptyPkgs;
    quickSortPackages(emptyPkgs);
    TEST_CHECK(emptyPkgs.empty());

    std::vector<Package> singlePkg = { Package("ONE", 2, 2, 2, 1.0) };
    quickSortPackages(singlePkg);
    TEST_CHECK(singlePkg.size() == 1 && singlePkg[0].id == "ONE");
}

void testValidFloorSupportedPlacement()
{
    Container container(10, 10, 10);
    std::vector<Package> packages = {
        Package("FLOOR_PKG", 4, 4, 2, 15.0)
    };

    PackingResult result = packCargo(container, packages);
    TEST_CHECK(result.allPackagesPlaced);
    TEST_CHECK(result.placements.size() == 1);
    TEST_CHECK(result.placements[0].z == 0); // resting on floor
}

void testUnsupportedElevatedPlacementRejected()
{
    // Container width=3, depth=2, height=6
    // Base: (2, 2, 3), weight 50.0 -> volume 12
    // Top/Overhang: (3, 2, 2), weight 10.0 -> volume 12
    // Same volume, but Base has higher weight (50.0 > 10.0), so Base is placed first at (0,0,0).
    // P_OVERHANG cannot fit on floor because floor remaining width is 3 - 2 = 1 < 3.
    // At z = 3, P_OVERHANG needs 3x2 footprint, but P_BASE only covers 2x2.
    // Direct physical support fails, so P_OVERHANG is rejected and reported as unplaced!
    Container c(3, 2, 6);
    std::vector<Package> pkgs = {
        Package("P_BASE", 2, 2, 3, 50.0),
        Package("P_OVERHANG", 3, 2, 2, 10.0)
    };

    PackingResult result = packCargo(c, pkgs);
    TEST_CHECK(result.placements.size() == 1);
    if (!result.placements.empty()) {
        TEST_CHECK(result.placements[0].packageId == "P_BASE");
    }
    TEST_CHECK(result.unplacedPackageIds.size() == 1);
    if (!result.unplacedPackageIds.empty()) {
        TEST_CHECK(result.unplacedPackageIds[0] == "P_OVERHANG");
    }
}

void testPlacementSupportedByMultipleLowerPackages()
{
    Container container(4, 2, 6);
    // Base 1: 2x2x2 at (0,0,0), volume 8, weight 50.0
    // Base 2: 2x2x2 at (2,0,0), volume 8, weight 50.0
    // Elevated Bridge: 4x2x1, volume 8, weight 10.0 < 50.0
    // Due to weight tie-breaker, BASE_1 and BASE_2 are placed on floor first.
    // TOP_BRIDGE is elevated at z = 2, spanning across both BASE_1 and BASE_2.
    std::vector<Package> pkgsMulti = {
        Package("BASE_1", 2, 2, 2, 50.0),
        Package("BASE_2", 2, 2, 2, 50.0),
        Package("TOP_BRIDGE", 4, 2, 1, 10.0)
    };

    PackingResult result = packCargo(container, pkgsMulti);
    TEST_CHECK(result.allPackagesPlaced);
    TEST_CHECK(result.placements.size() == 3);

    // Verify TOP_BRIDGE is at z = 2, spanning across both BASE_1 and BASE_2
    bool foundElevatedBridge = false;
    for (const auto& p : result.placements) {
        if (p.packageId == "TOP_BRIDGE") {
            TEST_CHECK(p.z == 2);
            TEST_CHECK(p.x == 0 && p.y == 0);
            TEST_CHECK(p.width == 4 && p.depth == 2);
            foundElevatedBridge = true;
        }
    }
    TEST_CHECK(foundElevatedBridge);
}

void testRejectedPlacementLeavesOccupancyUnchanged()
{
    Container container(4, 4, 4);
    // Place one package, then attempt an oversized/unsupported package
    std::vector<Package> pkgs = {
        Package("P_GOOD", 2, 2, 2, 10.0),
        Package("P_NOFIT", 5, 2, 2, 10.0) // width 5 > container width 4
    };

    PackingResult result = packCargo(container, pkgs);
    TEST_CHECK(result.placements.size() == 1);
    if (!result.placements.empty()) {
        TEST_CHECK(result.placements[0].packageId == "P_GOOD");
    }
    TEST_CHECK(result.totalPackedVolume == 8);
    TEST_CHECK(result.unplacedPackageIds.size() == 1);
    if (!result.unplacedPackageIds.empty()) {
        TEST_CHECK(result.unplacedPackageIds[0] == "P_NOFIT");
    }
}

void testCenterOfMassCalculationKnownWeights()
{
    // Hand-calculated CoM:
    // Package 1 at (0, 0, 0), size (2, 2, 2), weight 10.0. Center = (1, 1, 1)
    // Package 2 at (2, 0, 0), size (2, 2, 2), weight 30.0. Center = (3, 1, 1)
    // Total weight = 40.0
    // CoM_x = (10*1 + 30*3) / 40 = 100 / 40 = 2.5
    // CoM_y = (10*1 + 30*1) / 40 = 40 / 40 = 1.0
    // CoM_z = (10*1 + 30*1) / 40 = 40 / 40 = 1.0
    std::vector<Placement> placements = {
        Placement{"P1", 0, 0, 0, 2, 2, 2, 10.0},
        Placement{"P2", 2, 0, 0, 2, 2, 2, 30.0}
    };

    Vector3D com = computeCenterOfMass(placements);
    TEST_CHECK(std::abs(com.x - 2.5) < 1e-6);
    TEST_CHECK(std::abs(com.y - 1.0) < 1e-6);
    TEST_CHECK(std::abs(com.z - 1.0) < 1e-6);
}

void testStableAndUnstableSupportPolygonCases()
{
    // Floor footprint: rectangle [0, 4] x [0, 4]
    std::vector<Placement> floorPlacements = {
        Placement{"BASE", 0, 0, 0, 4, 4, 2, 100.0}
    };

    auto polygon = computeFloorSupportPolygon(floorPlacements);
    TEST_CHECK(!polygon.empty());

    // 1. Strictly inside point (stable)
    Point2D insidePt{2.0, 2.0};
    TEST_CHECK(isPointInPolygon(insidePt, polygon, 1e-6));

    // 2. Exact boundary point (stable)
    Point2D boundaryPt{4.0, 2.0}; // on the right edge x = 4
    TEST_CHECK(isPointInPolygon(boundaryPt, polygon, 1e-6));

    Point2D cornerPt{0.0, 0.0}; // corner point
    TEST_CHECK(isPointInPolygon(cornerPt, polygon, 1e-6));

    // 3. Outside point (unstable)
    Point2D outsidePt{5.0, 2.0}; // outside x = 4
    TEST_CHECK(!isPointInPolygon(outsidePt, polygon, 1e-6));

    Point2D outsideDiagonal{4.5, 4.5};
    TEST_CHECK(!isPointInPolygon(outsideDiagonal, polygon, 1e-6));

    // Verify stability helper
    TEST_CHECK(isCenterOfMassStable(floorPlacements, 1e-6));

    // Add an unstable elevated placement where CoM gets dragged outside footprint
    // Floor base weight = 1.0 at (0, 0, 0)
    // Huge cantilever/offset weight = 1000.0 at (3, 0, 2) of size (2, 2, 2)
    // Wait, footprint is [0, 4] x [0, 4]. CoM of (3,0,2) size (2,2,2) has center x=4, y=1.
    // If placed at (4, 0, 0) with no floor support, polygon check:
    std::vector<Placement> noFloorPlacements = {
        Placement{"FLOATING", 2, 2, 2, 4, 4, 2, 50.0}
    };
    TEST_CHECK(!isCenterOfMassStable(noFloorPlacements, 1e-6)); // No floor contact -> unstable
}

void testUnplacedPackageReportedExactlyOnce()
{
    Container container(4, 4, 4); // Volume 64
    // 3 packages of 4x4x4 (Volume 64 each)
    std::vector<Package> packages = {
        Package("PKG_A", 4, 4, 4, 10.0),
        Package("PKG_B", 4, 4, 4, 10.0),
        Package("PKG_C", 4, 4, 4, 10.0)
    };

    PackingResult result = packCargo(container, packages);
    TEST_CHECK(!result.allPackagesPlaced);
    TEST_CHECK(result.placements.size() == 1);
    TEST_CHECK(result.unplacedPackageIds.size() == 2);

    // Each package must appear exactly once across placements and unplacedPackageIds
    std::vector<std::string> allIds;
    for (const auto& p : result.placements) {
        allIds.push_back(p.packageId);
    }
    for (const auto& id : result.unplacedPackageIds) {
        allIds.push_back(id);
    }
    TEST_CHECK(allIds.size() == 3);

    bool hasA = false, hasB = false, hasC = false;
    for (const auto& id : allIds) {
        if (id == "PKG_A") hasA = true;
        if (id == "PKG_B") hasB = true;
        if (id == "PKG_C") hasC = true;
    }
    TEST_CHECK(hasA && hasB && hasC);
}

void testDuplicatePackageIdsRejectedBeforePacking()
{
    Container container(10, 10, 10);
    std::vector<Package> pkgs = {
        Package("DUP_ID", 2, 2, 2, 10.0),
        Package("OTHER", 3, 3, 3, 15.0),
        Package("DUP_ID", 1, 1, 1, 5.0)
    };

    bool threw = false;
    try {
        [[maybe_unused]] auto res = packCargo(container, pkgs);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);
}

void testOccupancyGridLimitAndSizeOverflow()
{
    // Container exceeding default maxOccupancyCells (10,000,000)
    Container hugeContainer(500, 500, 100); // 25,000,000 cells > 10,000,000
    bool threw = false;
    try {
        [[maybe_unused]] auto res = packCargo(hugeContainer, {});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Custom config allowing larger grid
    PackingConfig customConfig;
    customConfig.maxOccupancyCells = 50'000'000;
    PackingResult res = packCargo(hugeContainer, {}, customConfig);
    TEST_CHECK(res.allPackagesPlaced);
    TEST_CHECK(res.containerVolume == 25'000'000);

    // Overflow container dimensions
    bool overflowThrew = false;
    try {
        Container overflowContainer(2'000'000'000, 2'000'000'000, 2'000'000'000);
        [[maybe_unused]] auto r = packCargo(overflowContainer, {});
    } catch (const std::invalid_argument&) {
        overflowThrew = true;
    }
    TEST_CHECK(overflowThrew);
}

void testRegressionMixedPackagesDeterministic()
{
    Container container(10, 10, 8);
    std::vector<Package> packages = {
        Package("BOX_1", 4, 4, 2, 20.0),
        Package("BOX_2", 2, 4, 2, 12.0),
        Package("BOX_3", 4, 2, 2, 10.0),
        Package("BOX_4", 2, 2, 2, 8.0),
        Package("BOX_5", 4, 4, 2, 18.0),
        Package("BOX_6", 2, 2, 2, 5.0),
        Package("BOX_7", 3, 3, 2, 15.0),
        Package("BOX_8", 1, 1, 1, 2.0)
    };

    // Run 1
    PackingResult run1 = packCargo(container, packages);
    TEST_CHECK(run1.placements.size() >= 5);

    // Run 2 (Deterministic repetition)
    PackingResult run2 = packCargo(container, packages);
    TEST_CHECK(run1.placements.size() == run2.placements.size());
    TEST_CHECK(run1.unplacedPackageIds == run2.unplacedPackageIds);
    TEST_CHECK(run1.totalPackedWeight == run2.totalPackedWeight);
    TEST_CHECK(run1.totalPackedVolume == run2.totalPackedVolume);
    TEST_CHECK(run1.centerOfMass == run2.centerOfMass);

    for (std::size_t i = 0; i < run1.placements.size(); ++i) {
        TEST_CHECK(run1.placements[i] == run2.placements[i]);
    }
}

void testCenterOfMassArithmeticOverflow()
{
    const double maxDbl = std::numeric_limits<double>::max();

    // 1. Two individually finite max() weights in computeCenterOfMass()
    std::vector<Placement> twoMaxPlacements = {
        Placement{"P1", 0, 0, 0, 2, 2, 2, maxDbl},
        Placement{"P2", 2, 0, 0, 2, 2, 2, maxDbl}
    };
    bool twoMaxThrew = false;
    try {
        [[maybe_unused]] auto com = computeCenterOfMass(twoMaxPlacements);
    } catch (const std::overflow_error&) {
        twoMaxThrew = true;
    }
    TEST_CHECK(twoMaxThrew);

    // 2. Weighted coordinate multiplication overflow
    // Weight is maxDbl, center.x is 2.0 -> maxDbl * 2.0 = +infinity
    std::vector<Placement> coordOverflowPlacements = {
        Placement{"P_COORD", 1, 0, 0, 2, 2, 2, maxDbl} // center: x = 1 + 1 = 2.0
    };
    bool coordOverflowThrew = false;
    try {
        [[maybe_unused]] auto com = computeCenterOfMass(coordOverflowPlacements);
    } catch (const std::overflow_error&) {
        coordOverflowThrew = true;
    }
    TEST_CHECK(coordOverflowThrew);

    // 3. isCenterOfMassStable() throws std::overflow_error instead of fabricating {0,0,0}
    bool stableThrew = false;
    try {
        [[maybe_unused]] bool s = isCenterOfMassStable(twoMaxPlacements);
    } catch (const std::overflow_error&) {
        stableThrew = true;
    }
    TEST_CHECK(stableThrew);

    // 4. packCargo() transactional overflow behavior:
    // Place HEAVY_1 (0.75 * maxDbl).
    // Attempting HEAVY_2 (0.75 * maxDbl) would overflow total weight (1.5 * maxDbl).
    // Transactional check rolls back HEAVY_2 and records it as unplaced.
    // HEAVY_1 remains placed, total weight and CoM are finite and valid.
    Container container(10, 10, 10);
    const double heavyWeight = maxDbl * 0.75;
    std::vector<Package> heavyPackages = {
        Package("HEAVY_1", 2, 2, 2, heavyWeight),
        Package("HEAVY_2", 2, 2, 2, heavyWeight)
    };

    PackingResult res = packCargo(container, heavyPackages);
    TEST_CHECK(res.placements.size() == 1);
    if (!res.placements.empty()) {
        TEST_CHECK(res.placements[0].packageId == "HEAVY_1");
    }
    TEST_CHECK(res.unplacedPackageIds.size() == 1);
    if (!res.unplacedPackageIds.empty()) {
        TEST_CHECK(res.unplacedPackageIds[0] == "HEAVY_2");
    }
    TEST_CHECK(std::isfinite(res.totalPackedWeight));
    TEST_CHECK(res.totalPackedWeight == heavyWeight);
    TEST_CHECK(std::isfinite(res.centerOfMass.x) &&
               std::isfinite(res.centerOfMass.y) &&
               std::isfinite(res.centerOfMass.z));
    // Verify no fabricated center of mass
    TEST_CHECK(res.centerOfMass.x > 0.0);

    // 5. Preconditions: negative or non-finite weight in computeCenterOfMass
    std::vector<Placement> invalidWeightPlacements = {
        Placement{"BAD_W", 0, 0, 0, 2, 2, 2, -5.0}
    };
    bool invalidWeightThrew = false;
    try {
        [[maybe_unused]] auto com = computeCenterOfMass(invalidWeightPlacements);
    } catch (const std::invalid_argument&) {
        invalidWeightThrew = true;
    }
    TEST_CHECK(invalidWeightThrew);
}

void testExtremePointTruncationResourceLimit()
{
    // Specific regression case from review finding 2:
    // Container(3, 3, 1), BASE(2, 1, 1, 1.0), NEXT(2, 2, 1, 1.0)
    // BASE at (0,0,0) occupies [0,2) x [0,1) x [0,1).
    // Candidates generated: (2, 0, 0) and (0, 1, 0).
    // NEXT (2, 2, 1) cannot fit at (2, 0, 0) because 2 + 2 = 4 > 3 (container width).
    // NEXT CAN fit at (0, 1, 0) because [0, 2) x [1, 3) x [0, 1) fits container bounds [0, 3) x [0, 3) x [0, 1).
    // With maxExtremePoints = 1: the search space cannot be fully evaluated without
    // discarding candidates, so std::length_error MUST be thrown rather than falsely reporting NEXT unplaced!
    Container container(3, 3, 1);
    std::vector<Package> packages = {
        Package("BASE", 2, 1, 1, 1.0),
        Package("NEXT", 2, 2, 1, 1.0)
    };

    PackingConfig limitedConfig;
    limitedConfig.maxExtremePoints = 1;

    bool threwLengthError = false;
    try {
        [[maybe_unused]] auto res = packCargo(container, packages, limitedConfig);
    } catch (const std::length_error&) {
        threwLengthError = true;
    }
    TEST_CHECK(threwLengthError);

    // With sufficient maxExtremePoints (e.g. 10 or default), both packages are placed without error!
    PackingConfig normalConfig;
    normalConfig.maxExtremePoints = 10;
    PackingResult normalResult = packCargo(container, packages, normalConfig);
    TEST_CHECK(normalResult.allPackagesPlaced);
    TEST_CHECK(normalResult.placements.size() == 2);
    TEST_CHECK(normalResult.unplacedPackageIds.empty());

    bool foundBase = false;
    bool foundNext = false;
    for (const auto& p : normalResult.placements) {
        if (p.packageId == "NEXT") {
            TEST_CHECK(p.x == 0 && p.y == 0 && p.z == 0);
            foundNext = true;
        } else if (p.packageId == "BASE") {
            TEST_CHECK(p.x == 0 && p.y == 2 && p.z == 0);
            foundBase = true;
        }
    }
    TEST_CHECK(foundBase && foundNext);
}

void testPackingConfigurationValidation()
{
    Container container(10, 10, 10);
    std::vector<Package> packages = {
        Package("P1", 2, 2, 2, 10.0)
    };

    // 1. maxExtremePoints == 0 rejected with std::invalid_argument
    PackingConfig zeroPointsConfig;
    zeroPointsConfig.maxExtremePoints = 0;
    bool zeroPointsThrew = false;
    try {
        [[maybe_unused]] auto r = packCargo(container, packages, zeroPointsConfig);
    } catch (const std::invalid_argument&) {
        zeroPointsThrew = true;
    }
    TEST_CHECK(zeroPointsThrew);

    // 2. Negative centerOfMassTolerance rejected with std::invalid_argument
    PackingConfig negTolConfig;
    negTolConfig.centerOfMassTolerance = -0.01;
    bool negTolThrew = false;
    try {
        [[maybe_unused]] auto r = packCargo(container, packages, negTolConfig);
    } catch (const std::invalid_argument&) {
        negTolThrew = true;
    }
    TEST_CHECK(negTolThrew);

    // 3. NaN centerOfMassTolerance rejected with std::invalid_argument
    PackingConfig nanTolConfig;
    nanTolConfig.centerOfMassTolerance = std::numeric_limits<double>::quiet_NaN();
    bool nanTolThrew = false;
    try {
        [[maybe_unused]] auto r = packCargo(container, packages, nanTolConfig);
    } catch (const std::invalid_argument&) {
        nanTolThrew = true;
    }
    TEST_CHECK(nanTolThrew);

    // 4. Infinity centerOfMassTolerance rejected with std::invalid_argument
    PackingConfig infTolConfig;
    infTolConfig.centerOfMassTolerance = std::numeric_limits<double>::infinity();
    bool infTolThrew = false;
    try {
        [[maybe_unused]] auto r = packCargo(container, packages, infTolConfig);
    } catch (const std::invalid_argument&) {
        infTolThrew = true;
    }
    TEST_CHECK(infTolThrew);

    // 5. Preconditions on helper functions:
    // isPointInPolygon with negative tolerance
    bool pointInPolyTolThrew = false;
    try {
        [[maybe_unused]] bool in = isPointInPolygon({0.0, 0.0}, {}, -1.0);
    } catch (const std::invalid_argument&) {
        pointInPolyTolThrew = true;
    }
    TEST_CHECK(pointInPolyTolThrew);

    // isPointInPolygon with non-finite query point
    bool pointInPolyPtThrew = false;
    try {
        [[maybe_unused]] bool in = isPointInPolygon({std::numeric_limits<double>::infinity(), 0.0}, {}, 1e-6);
    } catch (const std::invalid_argument&) {
        pointInPolyPtThrew = true;
    }
    TEST_CHECK(pointInPolyPtThrew);

    // computeFloorSupportPolygon with invalid coordinates
    std::vector<Placement> negCoordPlacements = {
        Placement{"NEG", -1, 0, 0, 2, 2, 2, 10.0}
    };
    bool floorNegCoordThrew = false;
    try {
        [[maybe_unused]] auto poly = computeFloorSupportPolygon(negCoordPlacements);
    } catch (const std::invalid_argument&) {
        floorNegCoordThrew = true;
    }
    TEST_CHECK(floorNegCoordThrew);
}

void testFloorPolygonSignedIntegerOverflow()
{
    constexpr int kIntMax = std::numeric_limits<int>::max();

    // 1. Placement with x = INT_MAX and width = 1 -> (int64)x + width > INT_MAX
    std::vector<Placement> xOverflow = {
        Placement{"P_X_OVF", kIntMax, 0, 0, 1, 1, 1, 10.0}
    };
    bool floorXThrew = false;
    try {
        [[maybe_unused]] auto poly = computeFloorSupportPolygon(xOverflow);
    } catch (const std::overflow_error&) {
        floorXThrew = true;
    }
    TEST_CHECK(floorXThrew);

    bool comXThrew = false;
    try {
        [[maybe_unused]] auto com = computeCenterOfMass(xOverflow);
    } catch (const std::overflow_error&) {
        comXThrew = true;
    }
    TEST_CHECK(comXThrew);

    // 2. Placement with y = INT_MAX - 1 and depth = 2 -> (int64)y + depth > INT_MAX
    std::vector<Placement> yOverflow = {
        Placement{"P_Y_OVF", 0, kIntMax - 1, 0, 1, 2, 1, 10.0}
    };
    bool floorYThrew = false;
    try {
        [[maybe_unused]] auto poly = computeFloorSupportPolygon(yOverflow);
    } catch (const std::overflow_error&) {
        floorYThrew = true;
    }
    TEST_CHECK(floorYThrew);

    // 3. Placement with z = INT_MAX and height = 5 -> (int64)z + height > INT_MAX
    std::vector<Placement> zOverflow = {
        Placement{"P_Z_OVF", 0, 0, kIntMax, 1, 1, 5, 10.0}
    };
    bool floorZThrew = false;
    try {
        [[maybe_unused]] auto poly = computeFloorSupportPolygon(zOverflow);
    } catch (const std::overflow_error&) {
        floorZThrew = true;
    }
    TEST_CHECK(floorZThrew);

    // 4. Large-but-representable boundary case:
    // x = kIntMax - 2, width = 2 -> maxX = kIntMax (representable in int!)
    // y = 0, depth = 3 -> maxY = 3
    // z = 0, height = 1
    std::vector<Placement> validBoundary = {
        Placement{"P_VALID_BOUNDARY", kIntMax - 2, 0, 0, 2, 3, 1, 10.0}
    };
    std::vector<Point2D> poly = computeFloorSupportPolygon(validBoundary);
    TEST_CHECK(poly.size() == 4);

    // Verify coordinates of the convex hull vertices
    bool foundMinCorner = false;
    bool foundMaxCorner = false;
    for (const auto& pt : poly) {
        if (std::abs(pt.x - static_cast<double>(kIntMax - 2)) < 1e-6 && std::abs(pt.y - 0.0) < 1e-6) {
            foundMinCorner = true;
        }
        if (std::abs(pt.x - static_cast<double>(kIntMax)) < 1e-6 && std::abs(pt.y - 3.0) < 1e-6) {
            foundMaxCorner = true;
        }
    }
    TEST_CHECK(foundMinCorner);
    TEST_CHECK(foundMaxCorner);
}

void testQuickSortPackagesValidation()
{
    // 1. Strong exception guarantee: invalid package in vector leaves caller vector unmodified
    const std::vector<Package> original = {
        Package("PKG_B", 2, 2, 2, 10.0),
        Package("", 3, 3, 3, 15.0), // Invalid: empty ID
        Package("PKG_A", 4, 4, 4, 20.0)
    };
    std::vector<Package> pkgs = original;
    bool sortThrew = false;
    try {
        quickSortPackages(pkgs);
    } catch (const std::invalid_argument&) {
        sortThrew = true;
    }
    TEST_CHECK(sortThrew);
    TEST_CHECK(pkgs == original); // Vector is completely unmodified!

    // 2. Direct helper validation: Blank ID
    std::vector<Package> blankId = {Package("   \t\n", 2, 2, 2, 5.0)};
    bool blankThrew = false;
    try {
        quickSortPackages(blankId);
    } catch (const std::invalid_argument&) {
        blankThrew = true;
    }
    TEST_CHECK(blankThrew);

    // 3. Direct helper validation: Nonpositive dimensions
    std::vector<Package> zeroWidth = {Package("P", 0, 2, 2, 5.0)};
    bool zeroWidthThrew = false;
    try {
        quickSortPackages(zeroWidth);
    } catch (const std::invalid_argument&) {
        zeroWidthThrew = true;
    }
    TEST_CHECK(zeroWidthThrew);

    std::vector<Package> negDepth = {Package("P", 2, -1, 2, 5.0)};
    bool negDepthThrew = false;
    try {
        quickSortPackages(negDepth);
    } catch (const std::invalid_argument&) {
        negDepthThrew = true;
    }
    TEST_CHECK(negDepthThrew);

    // 4. Direct helper validation: Zero, negative, NaN, infinite weights
    std::vector<Package> zeroWeight = {Package("P", 2, 2, 2, 0.0)};
    bool zeroWeightThrew = false;
    try {
        quickSortPackages(zeroWeight);
    } catch (const std::invalid_argument&) {
        zeroWeightThrew = true;
    }
    TEST_CHECK(zeroWeightThrew);

    std::vector<Package> negWeight = {Package("P", 2, 2, 2, -3.5)};
    bool negWeightThrew = false;
    try {
        quickSortPackages(negWeight);
    } catch (const std::invalid_argument&) {
        negWeightThrew = true;
    }
    TEST_CHECK(negWeightThrew);

    std::vector<Package> nanWeight = {Package("P", 2, 2, 2, std::numeric_limits<double>::quiet_NaN())};
    bool nanWeightThrew = false;
    try {
        quickSortPackages(nanWeight);
    } catch (const std::invalid_argument&) {
        nanWeightThrew = true;
    }
    TEST_CHECK(nanWeightThrew);

    std::vector<Package> infWeight = {Package("P", 2, 2, 2, std::numeric_limits<double>::infinity())};
    bool infWeightThrew = false;
    try {
        quickSortPackages(infWeight);
    } catch (const std::invalid_argument&) {
        infWeightThrew = true;
    }
    TEST_CHECK(infWeightThrew);

    // 5. Direct helper validation: Volume multiplication overflow
    std::vector<Package> volOverflow = {Package("P", 2'000'000'000, 2'000'000'000, 2'000'000'000, 5.0)};
    bool volOverflowThrew = false;
    try {
        quickSortPackages(volOverflow);
    } catch (const std::invalid_argument&) {
        volOverflowThrew = true;
    }
    TEST_CHECK(volOverflowThrew);

    // 6. Valid packages are properly sorted: descending volume, descending weight, ascending ID
    std::vector<Package> validPkgs = {
        Package("ID_C", 2, 2, 2, 10.0), // vol = 8, w = 10
        Package("ID_A", 4, 4, 4, 5.0),  // vol = 64, w = 5
        Package("ID_B", 2, 2, 2, 20.0), // vol = 8, w = 20
        Package("ID_D", 2, 2, 2, 10.0)  // vol = 8, w = 10, tie-breaker with ID_C
    };
    quickSortPackages(validPkgs);
    TEST_CHECK(validPkgs[0].id == "ID_A"); // vol = 64
    TEST_CHECK(validPkgs[1].id == "ID_B"); // vol = 8, w = 20
    TEST_CHECK(validPkgs[2].id == "ID_C"); // vol = 8, w = 10, ID_C < ID_D
    TEST_CHECK(validPkgs[3].id == "ID_D"); // vol = 8, w = 10
}

int main()
{
    TEST_RUN(testEmptyPackageCollection);
    TEST_RUN(testSinglePackageExactFit);
    TEST_RUN(testPackageLargerThanContainer);
    TEST_RUN(testInvalidContainerDimensions);
    TEST_RUN(testEmptyAndDuplicatePackageIds);
    TEST_RUN(testZeroNegativeDimensionsAndInvalidWeights);
    TEST_RUN(testMultiplePackagesPlacedWithoutOverlap);
    TEST_RUN(testFaceTouchingPackagesAccepted);
    TEST_RUN(testContainerBoundaryChecks);
    TEST_RUN(testDeterministicQuickSortOrdering);
    TEST_RUN(testValidFloorSupportedPlacement);
    TEST_RUN(testUnsupportedElevatedPlacementRejected);
    TEST_RUN(testPlacementSupportedByMultipleLowerPackages);
    TEST_RUN(testRejectedPlacementLeavesOccupancyUnchanged);
    TEST_RUN(testCenterOfMassCalculationKnownWeights);
    TEST_RUN(testStableAndUnstableSupportPolygonCases);
    TEST_RUN(testUnplacedPackageReportedExactlyOnce);
    TEST_RUN(testDuplicatePackageIdsRejectedBeforePacking);
    TEST_RUN(testOccupancyGridLimitAndSizeOverflow);
    TEST_RUN(testRegressionMixedPackagesDeterministic);
    TEST_RUN(testCenterOfMassArithmeticOverflow);
    TEST_RUN(testExtremePointTruncationResourceLimit);
    TEST_RUN(testPackingConfigurationValidation);
    TEST_RUN(testFloorPolygonSignedIntegerOverflow);
    TEST_RUN(testQuickSortPackagesValidation);

    TEST_REPORT();
}
