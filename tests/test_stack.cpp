#include "test_utils.h"
#include "order/Stack.h"
#include <string>
#include <utility>

// ---------------------------------------------------------------------------
// T1: Empty Stack Behavior
// ---------------------------------------------------------------------------
void testEmptyStack()
{
    Stack<int> s;
    TEST_CHECK(s.isEmpty());
    TEST_CHECK(s.getSize() == 0);

    int val = 999;
    TEST_CHECK(!s.pop(val));
    TEST_CHECK(val == 999); // Must not alter outValue on empty

    TEST_CHECK(!s.pop());
    TEST_CHECK(!s.peek(val));
    TEST_CHECK(val == 999);

    bool caughtUnderflow = false;
    try {
        [[maybe_unused]] int topVal = s.top();
    } catch (const std::underflow_error&) {
        caughtUnderflow = true;
    }
    TEST_CHECK(caughtUnderflow);
}

// ---------------------------------------------------------------------------
// T2: Single Element Push, Peek, Top, Pop
// ---------------------------------------------------------------------------
void testSingleElement()
{
    Stack<std::string> s;
    s.push("Hello");
    TEST_CHECK(!s.isEmpty());
    TEST_CHECK(s.getSize() == 1);
    TEST_CHECK(s.top() == "Hello");

    std::string peeked;
    TEST_CHECK(s.peek(peeked));
    TEST_CHECK(peeked == "Hello");
    TEST_CHECK(s.getSize() == 1);

    std::string popped;
    TEST_CHECK(s.pop(popped));
    TEST_CHECK(popped == "Hello");
    TEST_CHECK(s.isEmpty());
    TEST_CHECK(s.getSize() == 0);
}

// ---------------------------------------------------------------------------
// T3: Strict LIFO Ordering
// ---------------------------------------------------------------------------
void testLifoOrdering()
{
    Stack<int> s;
    for (int i = 1; i <= 100; ++i) {
        s.push(i);
    }
    TEST_CHECK(s.getSize() == 100);

    for (int expected = 100; expected >= 1; --expected) {
        TEST_CHECK(s.top() == expected);
        int outVal = 0;
        TEST_CHECK(s.pop(outVal));
        TEST_CHECK(outVal == expected);
        TEST_CHECK(s.getSize() == static_cast<std::size_t>(expected - 1));
    }
    TEST_CHECK(s.isEmpty());
}

// ---------------------------------------------------------------------------
// T4: Move Construction and Move Assignment
// ---------------------------------------------------------------------------
void testMoveSemantics()
{
    Stack<int> s1;
    s1.push(10);
    s1.push(20);
    s1.push(30);

    // Move construct
    Stack<int> s2(std::move(s1));
    TEST_CHECK(s1.isEmpty());
    TEST_CHECK(s1.getSize() == 0);
    TEST_CHECK(s2.getSize() == 3);
    TEST_CHECK(s2.top() == 30);

    // Move assign
    Stack<int> s3;
    s3.push(999);
    s3 = std::move(s2);
    TEST_CHECK(s2.isEmpty());
    TEST_CHECK(s2.getSize() == 0);
    TEST_CHECK(s3.getSize() == 3);
    TEST_CHECK(s3.top() == 30);

    int val = 0;
    TEST_CHECK(s3.pop(val));
    TEST_CHECK(val == 30);
    TEST_CHECK(s3.pop(val));
    TEST_CHECK(val == 20);
    TEST_CHECK(s3.pop(val));
    TEST_CHECK(val == 10);
    TEST_CHECK(s3.isEmpty());
}

// ---------------------------------------------------------------------------
// T5: Clear and Reuse
// ---------------------------------------------------------------------------
void testClearAndReuse()
{
    Stack<int> s;
    s.push(1);
    s.push(2);
    s.push(3);
    TEST_CHECK(s.getSize() == 3);

    s.clear();
    TEST_CHECK(s.isEmpty());
    TEST_CHECK(s.getSize() == 0);

    // Stack must be fully reusable after clear
    s.push(42);
    s.push(84);
    TEST_CHECK(s.getSize() == 2);
    TEST_CHECK(s.top() == 84);

    int outVal = 0;
    TEST_CHECK(s.pop(outVal));
    TEST_CHECK(outVal == 84);
    TEST_CHECK(s.pop(outVal));
    TEST_CHECK(outVal == 42);
    TEST_CHECK(s.isEmpty());
}

// ---------------------------------------------------------------------------
// T6: Strong Exception Guarantee on Push
// ---------------------------------------------------------------------------
struct ThrowingType
{
    int value{0};
    static inline bool shouldThrow{false};

    ThrowingType() = default;
    explicit ThrowingType(int v) : value(v) {}

    ThrowingType(const ThrowingType& other)
        : value(other.value)
    {
        if (shouldThrow) {
            throw std::runtime_error("Simulated copy constructor failure");
        }
    }

    ThrowingType(ThrowingType&& other) noexcept
        : value(other.value)
    {
    }

    ThrowingType& operator=(const ThrowingType& other) = default;
    ThrowingType& operator=(ThrowingType&& other) noexcept = default;
};

void testExceptionSafety()
{
    Stack<ThrowingType> s;
    s.push(ThrowingType(100));
    s.push(ThrowingType(200));
    TEST_CHECK(s.getSize() == 2);

    ThrowingType::shouldThrow = true;
    bool caught = false;
    try {
        ThrowingType item(300);
        s.push(item); // Copy push will invoke throwing copy constructor
    } catch (const std::runtime_error&) {
        caught = true;
    }
    ThrowingType::shouldThrow = false;

    TEST_CHECK(caught);
    // Stack state must be completely preserved (strong exception guarantee)
    TEST_CHECK(s.getSize() == 2);
    TEST_CHECK(s.top().value == 200);

    ThrowingType out;
    TEST_CHECK(s.pop(out));
    TEST_CHECK(out.value == 200);
    TEST_CHECK(s.pop(out));
    TEST_CHECK(out.value == 100);
    TEST_CHECK(s.isEmpty());
}

int main()
{
    TEST_RUN(testEmptyStack);
    TEST_RUN(testSingleElement);
    TEST_RUN(testLifoOrdering);
    TEST_RUN(testMoveSemantics);
    TEST_RUN(testClearAndReuse);
    TEST_RUN(testExceptionSafety);
    TEST_REPORT();
}
