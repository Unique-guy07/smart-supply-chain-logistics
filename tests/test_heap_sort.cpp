#include "test_utils.h"
#include "task/HeapSort.h"
#include <algorithm>
#include <random>
#include <string>
#include <vector>

void testEmptyAndSingletonSort()
{
    std::vector<int> emptyVec;
    logistics::algo::heapSort(emptyVec);
    TEST_CHECK(emptyVec.empty());

    std::vector<int> singleton = {42};
    logistics::algo::heapSort(singleton);
    TEST_CHECK(singleton.size() == 1);
    TEST_CHECK(singleton[0] == 42);
}

void testSortedAndReverseSorted()
{
    std::vector<int> sortedVec = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    logistics::algo::heapSort(sortedVec);
    std::vector<int> expectedSorted = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    TEST_CHECK(sortedVec == expectedSorted);

    std::vector<int> reverseVec = {9, 8, 7, 6, 5, 4, 3, 2, 1};
    logistics::algo::heapSort(reverseVec);
    TEST_CHECK(reverseVec == expectedSorted);
}

void testDuplicateElements()
{
    std::vector<int> dups = {5, 2, 8, 5, 2, 8, 5, 1, 9, 1};
    logistics::algo::heapSort(dups);
    std::vector<int> expected = {1, 1, 2, 2, 5, 5, 5, 8, 8, 9};
    TEST_CHECK(dups == expected);

    std::vector<int> allSame = {7, 7, 7, 7, 7};
    logistics::algo::heapSort(allSame);
    std::vector<int> expectedSame = {7, 7, 7, 7, 7};
    TEST_CHECK(allSame == expectedSame);
}

void testCustomComparatorDescending()
{
    std::vector<int> vec = {4, 1, 8, 3, 9, 2};
    // Sort descending using std::greater<int>
    logistics::algo::heapSort(vec, std::greater<int>());
    std::vector<int> expectedDesc = {9, 8, 4, 3, 2, 1};
    TEST_CHECK(vec == expectedDesc);
}

struct TaskRecordMock
{
    int priority;
    std::string name;

    bool operator<(const TaskRecordMock& other) const noexcept
    {
        if (priority != other.priority) {
            return priority < other.priority;
        }
        return name < other.name; // Tie-breaker
    }

    bool operator==(const TaskRecordMock& other) const noexcept
    {
        return priority == other.priority && name == other.name;
    }
};

void testDomainObjectTieBreaking()
{
    std::vector<TaskRecordMock> records = {
        {2, "Beta"},
        {1, "Delta"},
        {2, "Alpha"},
        {3, "Gamma"},
        {1, "Charlie"}
    };

    logistics::algo::heapSort(records);

    std::vector<TaskRecordMock> expected = {
        {1, "Charlie"},
        {1, "Delta"},
        {2, "Alpha"},
        {2, "Beta"},
        {3, "Gamma"}
    };

    TEST_CHECK(records == expected);
}

void testDifferentialOracleAgainstStdSort()
{
    // Test multiple random inputs against std::sort as oracle
    std::mt19937 rng(1337);
    std::uniform_int_distribution<int> dist(-1000, 1000);

    for (std::size_t testSize : {10, 50, 100, 500}) {
        std::vector<int> testData(testSize);
        for (std::size_t i = 0; i < testSize; ++i) {
            testData[i] = dist(rng);
        }

        std::vector<int> oracleData = testData;
        std::sort(oracleData.begin(), oracleData.end());

        logistics::algo::heapSort(testData);

        TEST_CHECK(testData == oracleData);
    }
}

int main()
{
    TEST_RUN(testEmptyAndSingletonSort);
    TEST_RUN(testSortedAndReverseSorted);
    TEST_RUN(testDuplicateElements);
    TEST_RUN(testCustomComparatorDescending);
    TEST_RUN(testDomainObjectTieBreaking);
    TEST_RUN(testDifferentialOracleAgainstStdSort);
    TEST_REPORT();
}
