#pragma once

/// Lightweight test infrastructure for CTest-compatible test executables.
///
/// Does NOT rely on assert() — failures always produce non-zero exit codes
/// regardless of build configuration (Debug, Release, NDEBUG).
///
/// Usage:
///   void testSomething() {
///       TEST_CHECK(1 + 1 == 2);
///       TEST_CHECK(someFunction() != nullptr);
///   }
///
///   int main() {
///       TEST_RUN(testSomething);
///       TEST_REPORT();
///   }

#include <iostream>

inline int g_test_fail_count = 0;

#define TEST_CHECK(expr)                                                     \
    do {                                                                     \
        if (!(expr)) {                                                       \
            std::cerr << "  FAIL: " << #expr                                 \
                      << " (" << __FILE__ << ":" << __LINE__ << ")\n";       \
            ++g_test_fail_count;                                             \
        }                                                                    \
    } while (0)

#define TEST_RUN(func)                                                       \
    do {                                                                     \
        std::cout << "Running " << #func << "...\n";                         \
        func();                                                              \
    } while (0)

/// Call at the end of main(). Returns 0 if all checks passed, 1 otherwise.
#define TEST_REPORT()                                                        \
    do {                                                                     \
        if (g_test_fail_count > 0) {                                         \
            std::cerr << "\n" << g_test_fail_count                           \
                      << " check(s) FAILED.\n";                              \
            return 1;                                                        \
        }                                                                    \
        std::cout << "\nAll checks passed.\n";                               \
        return 0;                                                            \
    } while (0)
