#include "test_utils.h"
#include "task/OperationalMetricsRanker.h"
#include "task/PriorityTask.h"
#include "task/WorkerPerformance.h"
#include <cmath>
#include <limits>
#include <string>
#include <vector>

void testInvalidWorkerInitialization()
{
    // Direct constructor checks for invalid/blank strings
    bool caughtWorker = false;
    try {
        WorkerPerformance w("", "DC1", 10, 100, 60, 0);
    } catch (const std::invalid_argument&) {
        caughtWorker = true;
    }
    TEST_CHECK(caughtWorker);

    bool caughtDC = false;
    try {
        WorkerPerformance w("W1", "   ", 10, 100, 60, 0);
    } catch (const std::invalid_argument&) {
        caughtDC = true;
    }
    TEST_CHECK(caughtDC);

    // Ranker overload registration rejection
    OperationalMetricsRanker ranker;
    TEST_CHECK(!ranker.registerWorker("", "DC1", 10, 100, 60, 0));
    TEST_CHECK(!ranker.registerWorker("   ", "DC1", 10, 100, 60, 0));
    TEST_CHECK(!ranker.registerWorker("W1", "", 10, 100, 60, 0));
    TEST_CHECK(!ranker.registerWorker("W1", "  \t ", 10, 100, 60, 0));
    TEST_CHECK(ranker.isEmpty());
}

void testScoreCalculationAndZeroActiveMinutes()
{
    // Case 1: Zero active minutes produces exactly 0.0 without division by zero
    WorkerPerformance wZeroMin("W_ZERO", "DC1", 5, 100, 0, 0);
    TEST_CHECK(wZeroMin.calculateEfficiencyScore() == 0.0);

    // Case 2: Zero units handled produces 0.0
    WorkerPerformance wZeroUnits("W_ZERO_U", "DC1", 5, 0, 60, 0);
    TEST_CHECK(wZeroUnits.calculateEfficiencyScore() == 0.0);

    // Case 3: Errors equal to units handled produces 0.0
    WorkerPerformance wEqualErrors("W_ERR_EQ", "DC1", 5, 50, 60, 50);
    TEST_CHECK(wEqualErrors.calculateEfficiencyScore() == 0.0);

    // Case 4: Errors exceed units handled produces 0.0
    WorkerPerformance wMoreErrors("W_ERR_MORE", "DC1", 5, 50, 60, 60);
    TEST_CHECK(wMoreErrors.calculateEfficiencyScore() == 0.0);

    // Case 5: Standard calculation: (120 - 0) * 60 / 60 = 120.0
    WorkerPerformance wStandard("W_STD", "DC1", 10, 120, 60, 0);
    TEST_CHECK(std::abs(wStandard.calculateEfficiencyScore() - 120.0) < 1e-6);

    // Case 6: Net calculation with errors: (120 - 20) * 60 / 60 = 100.0
    WorkerPerformance wWithErrors("W_ERR", "DC1", 10, 120, 60, 20);
    TEST_CHECK(std::abs(wWithErrors.calculateEfficiencyScore() - 100.0) < 1e-6);

    // Case 7: Fractional hours: (90 - 0) * 60 / 30 = 180.0 units/hour
    WorkerPerformance wHalfHour("W_HALF", "DC1", 5, 90, 30, 0);
    TEST_CHECK(std::abs(wHalfHour.calculateEfficiencyScore() - 180.0) < 1e-6);
}

void testDeterministicTieBreaking()
{
    // Two workers with identical scores (100.0)
    // wA has more tasks completed than wB -> wA ranks greater
    WorkerPerformance wA("W_A", "DC1", 15, 100, 60, 0);
    WorkerPerformance wB("W_B", "DC1", 10, 100, 60, 0);
    TEST_CHECK(wB < wA);
    TEST_CHECK(!(wA < wB));

    // Two workers with identical scores and tasks, but wC has more units
    WorkerPerformance wC("W_C", "DC1", 10, 200, 120, 0); // 200 * 60 / 120 = 100.0
    WorkerPerformance wD("W_D", "DC1", 10, 100, 60, 0);   // 100 * 60 / 60 = 100.0
    TEST_CHECK(wD < wC); // wC has 200 units vs 100 units

    // Two workers with completely identical numerical attributes:
    // Tie-breaker: smaller workerId ("W_01" vs "W_02") has dispatch precedence and ranks greater
    WorkerPerformance w1("W_01", "DC1", 10, 100, 60, 0);
    WorkerPerformance w2("W_02", "DC1", 10, 100, 60, 0);
    TEST_CHECK(w2 < w1); // w1 is greater
    TEST_CHECK(!(w1 < w2));
}

void testRegistrationAndUpdates()
{
    OperationalMetricsRanker ranker;

    TEST_CHECK(ranker.registerWorker("W1", "DC1", 5, 50, 30, 1));
    TEST_CHECK(!ranker.registerWorker("W1", "DC2", 10, 100, 60, 0)); // Duplicate ID rejected
    TEST_CHECK(ranker.getWorkerCount() == 1);
    TEST_CHECK(ranker.hasWorker("W1"));

    // Update worker replaces statistics
    WorkerPerformance updatedW1("W1", "DC1", 8, 80, 45, 2);
    TEST_CHECK(ranker.updateWorker(updatedW1));
    const WorkerPerformance* ptr = ranker.getWorker("W1");
    TEST_CHECK(ptr != nullptr);
    TEST_CHECK(ptr->getTasksCompleted() == 8);
    TEST_CHECK(ptr->getUnitsHandled() == 80);

    // Updating non-existent worker fails
    WorkerPerformance nonExistent("NO_SUCH", "DC1", 1, 1, 1, 0);
    TEST_CHECK(!ranker.updateWorker(nonExistent));

    // Accumulate metrics
    TEST_CHECK(ranker.accumulateMetrics("W1", 2, 20, 15, 1));
    ptr = ranker.getWorker("W1");
    TEST_CHECK(ptr != nullptr);
    TEST_CHECK(ptr->getTasksCompleted() == 10);
    TEST_CHECK(ptr->getUnitsHandled() == 100);
    TEST_CHECK(ptr->getActiveMinutes() == 60);
    TEST_CHECK(ptr->getErrorCount() == 3);

    // Record task fulfillment integration with PriorityTask
    PriorityTask task("TASK_PICK_1", TaskCategory::Pick, TaskPriority::High, "DC1", 1);
    TEST_CHECK(ranker.recordTaskFulfillment("W1", task, 10, 5, false));
    ptr = ranker.getWorker("W1");
    TEST_CHECK(ptr->getTasksCompleted() == 11);
    TEST_CHECK(ptr->getUnitsHandled() == 110);
    TEST_CHECK(ptr->getActiveMinutes() == 65);
    TEST_CHECK(ptr->getErrorCount() == 3);

    // Fulfillment with error
    TEST_CHECK(ranker.recordTaskFulfillment("W1", task, 5, 5, true));
    ptr = ranker.getWorker("W1");
    TEST_CHECK(ptr->getTasksCompleted() == 12);
    TEST_CHECK(ptr->getUnitsHandled() == 115);
    TEST_CHECK(ptr->getActiveMinutes() == 70);
    TEST_CHECK(ptr->getErrorCount() == 4);
}

void testAccumulationOverflowGuards()
{
    OperationalMetricsRanker ranker;
    const std::uint32_t maxU32 = std::numeric_limits<std::uint32_t>::max();

    ranker.registerWorker("W_MAX", "DC1", maxU32 - 5, maxU32 - 10, maxU32 - 15, maxU32 - 20);

    // Safe accumulation under limit
    TEST_CHECK(ranker.accumulateMetrics("W_MAX", 2, 5, 10, 15));

    // Overflow in tasksCompleted
    TEST_CHECK(!ranker.accumulateMetrics("W_MAX", 10, 0, 0, 0));

    // Overflow in unitsHandled
    TEST_CHECK(!ranker.accumulateMetrics("W_MAX", 0, 10, 0, 0));

    // Overflow in activeMinutes
    TEST_CHECK(!ranker.accumulateMetrics("W_MAX", 0, 0, 10, 0));

    // Overflow in errorCount
    TEST_CHECK(!ranker.accumulateMetrics("W_MAX", 0, 0, 0, 10));

    // Verify record was unmodified by rejected operations
    const WorkerPerformance* ptr = ranker.getWorker("W_MAX");
    TEST_CHECK(ptr != nullptr);
    TEST_CHECK(ptr->getTasksCompleted() == maxU32 - 3);
    TEST_CHECK(ptr->getUnitsHandled() == maxU32 - 5);
    TEST_CHECK(ptr->getActiveMinutes() == maxU32 - 5);
    TEST_CHECK(ptr->getErrorCount() == maxU32 - 5);
}

void testFullRankingVsTopKConsistency()
{
    OperationalMetricsRanker ranker;

    // Register 6 workers with distinct performance rates
    // W1: 150 net units / hr
    ranker.registerWorker("W1", "DC1", 10, 150, 60, 0); // 150.0
    // W2: 200 net units / hr
    ranker.registerWorker("W2", "DC1", 15, 200, 60, 0); // 200.0
    // W3: 80 net units / hr
    ranker.registerWorker("W3", "DC2", 8, 80, 60, 0);   // 80.0
    // W4: 250 net units / hr
    ranker.registerWorker("W4", "DC2", 20, 250, 60, 0); // 250.0
    // W5: 50 net units / hr
    ranker.registerWorker("W5", "DC1", 5, 60, 60, 10);  // 50.0
    // W6: 0 net units / hr
    ranker.registerWorker("W6", "DC2", 2, 10, 60, 10);  // 0.0

    // Full ranking descending: W4 (250) -> W2 (200) -> W1 (150) -> W3 (80) -> W5 (50) -> W6 (0)
    std::vector<WorkerPerformance> rankedDesc = ranker.rankAllWorkers(false);
    TEST_CHECK(rankedDesc.size() == 6);
    TEST_CHECK(rankedDesc[0].getWorkerId() == "W4");
    TEST_CHECK(rankedDesc[1].getWorkerId() == "W2");
    TEST_CHECK(rankedDesc[2].getWorkerId() == "W1");
    TEST_CHECK(rankedDesc[3].getWorkerId() == "W3");
    TEST_CHECK(rankedDesc[4].getWorkerId() == "W5");
    TEST_CHECK(rankedDesc[5].getWorkerId() == "W6");

    // Full ranking ascending: W6 -> W5 -> W3 -> W1 -> W2 -> W4
    std::vector<WorkerPerformance> rankedAsc = ranker.rankAllWorkers(true);
    TEST_CHECK(rankedAsc.size() == 6);
    TEST_CHECK(rankedAsc[0].getWorkerId() == "W6");
    TEST_CHECK(rankedAsc[5].getWorkerId() == "W4");

    // Top-K queries using MaxHeap
    // Top-3
    std::vector<WorkerPerformance> top3 = ranker.getTopKWorkers(3);
    TEST_CHECK(top3.size() == 3);
    TEST_CHECK(top3[0].getWorkerId() == rankedDesc[0].getWorkerId());
    TEST_CHECK(top3[1].getWorkerId() == rankedDesc[1].getWorkerId());
    TEST_CHECK(top3[2].getWorkerId() == rankedDesc[2].getWorkerId());

    // Top-6 (matches full descending sort)
    std::vector<WorkerPerformance> top6 = ranker.getTopKWorkers(6);
    TEST_CHECK(top6.size() == 6);
    for (std::size_t i = 0; i < 6; ++i) {
        TEST_CHECK(top6[i].getWorkerId() == rankedDesc[i].getWorkerId());
    }

    // Top-10 request when only 6 exist returns 6
    std::vector<WorkerPerformance> top10 = ranker.getTopKWorkers(10);
    TEST_CHECK(top10.size() == 6);

    // Top-0 returns empty
    TEST_CHECK(ranker.getTopKWorkers(0).empty());
}

void testDistributionCenterFiltering()
{
    OperationalMetricsRanker ranker;

    ranker.registerWorker("W_DC1_A", "DC1", 10, 100, 60, 0); // 100.0
    ranker.registerWorker("W_DC1_B", "DC1", 12, 120, 60, 0); // 120.0
    ranker.registerWorker("W_DC2_A", "DC2", 15, 200, 60, 0); // 200.0
    ranker.registerWorker("W_DC2_B", "DC2", 8, 90, 60, 0);   // 90.0

    // Filter DC1: should contain only DC1 workers, sorted descending (B then A)
    std::vector<WorkerPerformance> dc1Ranked = ranker.rankWorkersByDistributionCenter("DC1", false);
    TEST_CHECK(dc1Ranked.size() == 2);
    TEST_CHECK(dc1Ranked[0].getWorkerId() == "W_DC1_B");
    TEST_CHECK(dc1Ranked[1].getWorkerId() == "W_DC1_A");

    // Filter DC2: should contain only DC2 workers (A then B)
    std::vector<WorkerPerformance> dc2Ranked = ranker.rankWorkersByDistributionCenter("DC2", false);
    TEST_CHECK(dc2Ranked.size() == 2);
    TEST_CHECK(dc2Ranked[0].getWorkerId() == "W_DC2_A");
    TEST_CHECK(dc2Ranked[1].getWorkerId() == "W_DC2_B");

    // Non-existent DC returns empty vector
    std::vector<WorkerPerformance> dc3Ranked = ranker.rankWorkersByDistributionCenter("DC_UNKNOWN", false);
    TEST_CHECK(dc3Ranked.empty());
}

int main()
{
    TEST_RUN(testInvalidWorkerInitialization);
    TEST_RUN(testScoreCalculationAndZeroActiveMinutes);
    TEST_RUN(testDeterministicTieBreaking);
    TEST_RUN(testRegistrationAndUpdates);
    TEST_RUN(testAccumulationOverflowGuards);
    TEST_RUN(testFullRankingVsTopKConsistency);
    TEST_RUN(testDistributionCenterFiltering);
    TEST_REPORT();
}
