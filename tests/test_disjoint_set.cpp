#include "test_utils.h"
#include "graph/DisjointSet.h"

#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

void testZeroAndSingleElement()
{
    // Zero-element DisjointSet
    DisjointSet ds0(0);
    TEST_CHECK(ds0.size() == 0);
    TEST_CHECK(ds0.getSetCount() == 0);

    bool threw = false;
    try {
        [[maybe_unused]] auto rep = ds0.find(0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    threw = false;
    try {
        ds0.unionSets(0, 0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    threw = false;
    try {
        [[maybe_unused]] auto conn = ds0.isConnected(0, 0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Single-element DisjointSet
    DisjointSet ds1(1);
    TEST_CHECK(ds1.size() == 1);
    TEST_CHECK(ds1.getSetCount() == 1);
    TEST_CHECK(ds1.find(0) == 0);
    TEST_CHECK(ds1.isConnected(0, 0));
    TEST_CHECK(!ds1.unionSets(0, 0)); // unioning with self returns false
    TEST_CHECK(ds1.getSetCount() == 1);

    threw = false;
    try {
        [[maybe_unused]] auto rep = ds1.find(1);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);
}

void testInitialComponentCountAndRepresentatives()
{
    const std::size_t N = 10;
    DisjointSet ds(N);
    TEST_CHECK(ds.size() == N);
    TEST_CHECK(ds.getSetCount() == N);

    for (std::size_t i = 0; i < N; ++i) {
        TEST_CHECK(ds.find(i) == i);
    }

    for (std::size_t i = 0; i < N; ++i) {
        for (std::size_t j = i + 1; j < N; ++j) {
            TEST_CHECK(!ds.isConnected(i, j));
            TEST_CHECK(ds.find(i) != ds.find(j));
        }
    }
}

void testSuccessfulAndRedundantUnions()
{
    DisjointSet ds(6);
    TEST_CHECK(ds.getSetCount() == 6);

    // Union {0} and {1}
    TEST_CHECK(ds.unionSets(0, 1));
    TEST_CHECK(ds.getSetCount() == 5);
    TEST_CHECK(ds.isConnected(0, 1));
    TEST_CHECK(ds.find(0) == ds.find(1));

    // Redundant unions on {0, 1}
    TEST_CHECK(!ds.unionSets(0, 1));
    TEST_CHECK(!ds.unionSets(1, 0));
    TEST_CHECK(ds.getSetCount() == 5);

    // Union {2} and {3}
    TEST_CHECK(ds.unionSets(2, 3));
    TEST_CHECK(ds.getSetCount() == 4);
    TEST_CHECK(ds.isConnected(2, 3));
    TEST_CHECK(!ds.isConnected(0, 2));

    // Union {0, 1} and {2, 3} via unionSets(1, 2)
    TEST_CHECK(ds.unionSets(1, 2));
    TEST_CHECK(ds.getSetCount() == 3);

    // Verify full transitivity in {0, 1, 2, 3}
    TEST_CHECK(ds.isConnected(0, 3));
    TEST_CHECK(ds.isConnected(1, 3));
    TEST_CHECK(ds.isConnected(0, 2));
    TEST_CHECK(ds.find(0) == ds.find(3));

    // Unconnected elements {4} and {5} remain independent
    TEST_CHECK(!ds.isConnected(0, 4));
    TEST_CHECK(!ds.isConnected(2, 5));
    TEST_CHECK(!ds.isConnected(4, 5));

    // Redundant unions across already connected components
    TEST_CHECK(!ds.unionSets(0, 3));
    TEST_CHECK(!ds.unionSets(2, 0));
    TEST_CHECK(!ds.unionSets(3, 1));
    TEST_CHECK(ds.getSetCount() == 3);

    // Union remaining elements into one single set
    TEST_CHECK(ds.unionSets(4, 5));
    TEST_CHECK(ds.getSetCount() == 2);
    TEST_CHECK(ds.unionSets(0, 4));
    TEST_CHECK(ds.getSetCount() == 1);

    for (std::size_t i = 0; i < 6; ++i) {
        for (std::size_t j = 0; j < 6; ++j) {
            TEST_CHECK(ds.isConnected(i, j));
        }
    }
}

void testPathCompressionAndUnionByRank()
{
    // Build a balanced tree using equal-rank mergers
    DisjointSet ds(8);

    // Step 1: 4 pairs of rank 1
    TEST_CHECK(ds.unionSets(0, 1));
    TEST_CHECK(ds.unionSets(2, 3));
    TEST_CHECK(ds.unionSets(4, 5));
    TEST_CHECK(ds.unionSets(6, 7));
    TEST_CHECK(ds.getSetCount() == 4);

    // Step 2: 2 sets of rank 2
    TEST_CHECK(ds.unionSets(0, 2));
    TEST_CHECK(ds.unionSets(4, 6));
    TEST_CHECK(ds.getSetCount() == 2);

    // Step 3: 1 set of rank 3
    TEST_CHECK(ds.unionSets(0, 4));
    TEST_CHECK(ds.getSetCount() == 1);

    std::size_t canonicalRoot = ds.find(0);
    for (std::size_t i = 1; i < 8; ++i) {
        TEST_CHECK(ds.find(i) == canonicalRoot);
    }

    // Verify path compression: repeated lookups must produce the exact same root
    for (std::size_t i = 0; i < 8; ++i) {
        TEST_CHECK(ds.find(i) == canonicalRoot);
    }
}

void testInvalidIndexExceptions()
{
    DisjointSet ds(5);

    // Testing find out of bounds
    bool threw = false;
    try {
        [[maybe_unused]] auto r = ds.find(5);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    threw = false;
    try {
        [[maybe_unused]] auto r = ds.find(100);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Testing unionSets out of bounds
    threw = false;
    try {
        ds.unionSets(0, 5);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    threw = false;
    try {
        ds.unionSets(5, 0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    threw = false;
    try {
        ds.unionSets(5, 6);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Testing isConnected out of bounds
    threw = false;
    try {
        [[maybe_unused]] auto conn = ds.isConnected(0, 5);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    threw = false;
    try {
        [[maybe_unused]] auto conn = ds.isConnected(5, 0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    TEST_CHECK(threw);

    // Set count and internal state must remain completely uncorrupted
    TEST_CHECK(ds.getSetCount() == 5);
    TEST_CHECK(ds.size() == 5);
}

void testMoveSemantics()
{
    DisjointSet ds1(10);
    TEST_CHECK(ds1.unionSets(1, 2));
    TEST_CHECK(ds1.unionSets(3, 4));
    TEST_CHECK(ds1.getSetCount() == 8);

    // Move construction
    DisjointSet ds2 = std::move(ds1);
    TEST_CHECK(ds2.size() == 10);
    TEST_CHECK(ds2.getSetCount() == 8);
    TEST_CHECK(ds2.isConnected(1, 2));
    TEST_CHECK(ds2.isConnected(3, 4));
    TEST_CHECK(!ds2.isConnected(1, 3));

    // Move assignment
    DisjointSet ds3(2);
    ds3 = std::move(ds2);
    TEST_CHECK(ds3.size() == 10);
    TEST_CHECK(ds3.getSetCount() == 8);
    TEST_CHECK(ds3.isConnected(1, 2));
    TEST_CHECK(ds3.isConnected(3, 4));
}

void testDeterministicStress()
{
    // Stress test with > 10,000 operations verified against an independent reference oracle
    const std::size_t N = 2000;
    const std::size_t TOTAL_OPS = 12000;

    DisjointSet ds(N);
    TEST_CHECK(ds.size() == N);
    TEST_CHECK(ds.getSetCount() == N);

    // Independent reference model: flat component-label vector and component counter.
    // Does NOT use DisjointSet to calculate any expected results.
    std::vector<std::size_t> refLabels(N);
    for (std::size_t i = 0; i < N; ++i) {
        refLabels[i] = i;
    }
    std::size_t refComponentCount = N;

    std::mt19937_64 rng(42); // Fixed deterministic seed
    std::uniform_int_distribution<std::size_t> dist(0, N - 1);

    for (std::size_t op = 0; op < TOTAL_OPS; ++op) {
        std::size_t u = dist(rng);
        std::size_t v = dist(rng);

        // 1. Determine whether u and v belong to the same component using only the reference model
        const bool expectedConnected = (refLabels[u] == refLabels[v]);

        // 2. Compare expected connectivity with ds.isConnected(u, v)
        TEST_CHECK(ds.isConnected(u, v) == expectedConnected);

        // 3. Apply the union to the reference model independently
        bool expectedMerged = false;
        if (!expectedConnected) {
            const std::size_t oldLabel = refLabels[v];
            const std::size_t newLabel = refLabels[u];
            for (std::size_t i = 0; i < N; ++i) {
                if (refLabels[i] == oldLabel) {
                    refLabels[i] = newLabel;
                }
            }
            --refComponentCount;
            expectedMerged = true;
        }

        // 4. Call ds.unionSets(u, v) and compare return value with reference merge result
        const bool actualMerged = ds.unionSets(u, v);
        TEST_CHECK(actualMerged == expectedMerged);

        // 5. Compare ds.getSetCount() with reference component count
        TEST_CHECK(ds.getSetCount() == refComponentCount);

        // Post-operation verification for the operated pair
        TEST_CHECK(ds.isConnected(u, v));
        TEST_CHECK(refLabels[u] == refLabels[v]);

        // 6. Verify connectivity against reference model for an additional random probe pair
        std::size_t probeU = dist(rng);
        std::size_t probeV = dist(rng);
        TEST_CHECK(ds.isConnected(probeU, probeV) == (refLabels[probeU] == refLabels[probeV]));
    }

    // Final comprehensive verification across sample pairs against the independent reference oracle
    TEST_CHECK(ds.getSetCount() == refComponentCount);
    for (std::size_t i = 0; i < N; i += 17) {
        for (std::size_t j = 0; j < N; j += 23) {
            const bool expectedConn = (refLabels[i] == refLabels[j]);
            TEST_CHECK(ds.isConnected(i, j) == expectedConn);
            if (expectedConn) {
                TEST_CHECK(ds.find(i) == ds.find(j));
            } else {
                TEST_CHECK(ds.find(i) != ds.find(j));
            }
        }
    }

    // Structural invariant: find() idempotency
    for (std::size_t i = 0; i < N; ++i) {
        std::size_t r1 = ds.find(i);
        std::size_t r2 = ds.find(r1);
        TEST_CHECK(r1 == r2);
    }
}

int main()
{
    TEST_RUN(testZeroAndSingleElement);
    TEST_RUN(testInitialComponentCountAndRepresentatives);
    TEST_RUN(testSuccessfulAndRedundantUnions);
    TEST_RUN(testPathCompressionAndUnionByRank);
    TEST_RUN(testInvalidIndexExceptions);
    TEST_RUN(testMoveSemantics);
    TEST_RUN(testDeterministicStress);
    TEST_REPORT();
}
