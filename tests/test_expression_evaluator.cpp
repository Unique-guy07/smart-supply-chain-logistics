#include "order/ExpressionEvaluator.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

namespace
{
void assertTrue(bool condition, const std::string& msg)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << msg << std::endl;
        std::exit(1);
    }
    std::cout << "PASS: " << msg << std::endl;
}

bool approxEqual(double a, double b, double eps = 1e-6)
{
    return std::abs(a - b) <= eps;
}

void testBasicArithmetic()
{
    std::cout << "\n--- testBasicArithmetic ---" << std::endl;
    ExpressionEvaluator::VariableMap vars;

    auto r1 = ExpressionEvaluator::evaluateInfix("3 + 5", vars);
    assertTrue(r1.success && approxEqual(r1.value, 8.0), "3 + 5 == 8");

    auto r2 = ExpressionEvaluator::evaluateInfix("10 - 4 - 2", vars);
    assertTrue(r2.success && approxEqual(r2.value, 4.0), "10 - 4 - 2 == 4 (left associativity)");

    auto r3 = ExpressionEvaluator::evaluateInfix("2 * 3 + 4", vars);
    assertTrue(r3.success && approxEqual(r3.value, 10.0), "2 * 3 + 4 == 10");

    auto r4 = ExpressionEvaluator::evaluateInfix("2 + 3 * 4", vars);
    assertTrue(r4.success && approxEqual(r4.value, 14.0), "2 + 3 * 4 == 14");

    auto r5 = ExpressionEvaluator::evaluateInfix("12 / 4 / 3", vars);
    assertTrue(r5.success && approxEqual(r5.value, 1.0), "12 / 4 / 3 == 1 (left associativity)");

    auto r6 = ExpressionEvaluator::evaluateInfix("(2 + 3) * 4", vars);
    assertTrue(r6.success && approxEqual(r6.value, 20.0), "(2 + 3) * 4 == 20");
}

void testPrecedenceAndAssociativity()
{
    std::cout << "\n--- testPrecedenceAndAssociativity ---" << std::endl;
    ExpressionEvaluator::VariableMap vars;

    // -2^2 -> -(2^2) = -4
    auto r1 = ExpressionEvaluator::evaluateInfix("-2^2", vars);
    assertTrue(r1.success && approxEqual(r1.value, -4.0), "-2^2 == -4");

    // (-2)^2 -> 4
    auto r2 = ExpressionEvaluator::evaluateInfix("(-2)^2", vars);
    assertTrue(r2.success && approxEqual(r2.value, 4.0), "(-2)^2 == 4");

    // 2^3^2 -> 2^(3^2) = 2^9 = 512
    auto r3 = ExpressionEvaluator::evaluateInfix("2^3^2", vars);
    assertTrue(r3.success && approxEqual(r3.value, 512.0), "2^3^2 == 512 (right associativity)");

    // 2^-2 -> 2^(-2) = 0.25
    auto r4 = ExpressionEvaluator::evaluateInfix("2^-2", vars);
    assertTrue(r4.success && approxEqual(r4.value, 0.25), "2^-2 == 0.25 (unary minus after power)");

    // 10-4-2 -> 4
    auto r5 = ExpressionEvaluator::evaluateInfix("10-4-2", vars);
    assertTrue(r5.success && approxEqual(r5.value, 4.0), "10-4-2 == 4");

    // Multiple unary negation: -(-5) -> 5
    auto r6 = ExpressionEvaluator::evaluateInfix("-(-5)", vars);
    assertTrue(r6.success && approxEqual(r6.value, 5.0), "-(-5) == 5");
}

void testFunctions()
{
    std::cout << "\n--- testFunctions ---" << std::endl;
    ExpressionEvaluator::VariableMap vars;

    auto r1 = ExpressionEvaluator::evaluateInfix("max(10, 20)", vars);
    assertTrue(r1.success && approxEqual(r1.value, 20.0), "max(10, 20) == 20");

    auto r2 = ExpressionEvaluator::evaluateInfix("min(10, 20)", vars);
    assertTrue(r2.success && approxEqual(r2.value, 10.0), "min(10, 20) == 10");

    // Nested functions: max(1, min(2, 3)) -> 2
    auto r3 = ExpressionEvaluator::evaluateInfix("max(1, min(2, 3))", vars);
    assertTrue(r3.success && approxEqual(r3.value, 2.0), "max(1, min(2, 3)) == 2");

    // Negative arguments: max(-5, -2) -> -2
    auto r4 = ExpressionEvaluator::evaluateInfix("max(-5, -2)", vars);
    assertTrue(r4.success && approxEqual(r4.value, -2.0), "max(-5, -2) == -2");

    // Complex expressions in function arguments
    auto r5 = ExpressionEvaluator::evaluateInfix("min(2 * 5, 3^2 + 2)", vars);
    assertTrue(r5.success && approxEqual(r5.value, 10.0), "min(10, 11) == 10");

    // Nested min in max
    auto r6 = ExpressionEvaluator::evaluateInfix("max(min(50, 100), min(30, 20))", vars);
    assertTrue(r6.success && approxEqual(r6.value, 50.0), "max(50, 20) == 50");
}

void testVariables()
{
    std::cout << "\n--- testVariables ---" << std::endl;
    ExpressionEvaluator::VariableMap vars = {
        {"base_fee", 10.0},
        {"distance", 50.0},
        {"distance_rate", 0.5},
        {"weight", 20.0},
        {"weight_rate", 0.8},
        {"urgency_multiplier", 1.4}
    };

    // Standard formula: max(base_fee, (base_fee + distance * distance_rate + weight * weight_rate) * urgency_multiplier)
    std::string formula = "max(base_fee, (base_fee + distance * distance_rate + weight * weight_rate) * urgency_multiplier)";
    auto res = ExpressionEvaluator::evaluateInfix(formula, vars);
    // 10 + 50*0.5 + 20*0.8 = 10 + 25 + 16 = 51.0
    // 51.0 * 1.4 = 71.4
    // max(10, 71.4) = 71.4
    assertTrue(res.success && approxEqual(res.value, 71.4), "Freight formula evaluates to 71.4");

    // Undefined variable
    auto rUndef = ExpressionEvaluator::evaluateInfix("base_fee + non_existent_var", vars);
    assertTrue(!rUndef.success && rUndef.errorCode == ExpressionErrorCode::UndefinedVariable,
               "Undefined variable rejected");
}

void testCompiledExpressionReuse()
{
    std::cout << "\n--- testCompiledExpressionReuse ---" << std::endl;
    std::string expr = "x^2 + 2 * x + 1";
    CompiledExpression compiled;
    auto compRes = ExpressionEvaluator::compile(expr, compiled);
    assertTrue(compRes.success && compiled.isValid(), "Compile expression successfully");

    ExpressionEvaluator::VariableMap v1 = {{"x", 3.0}};
    auto r1 = ExpressionEvaluator::evaluate(compiled, v1);
    assertTrue(r1.success && approxEqual(r1.value, 16.0), "Compiled eval at x=3: 9 + 6 + 1 == 16");

    ExpressionEvaluator::VariableMap v2 = {{"x", -1.0}};
    auto r2 = ExpressionEvaluator::evaluate(compiled, v2);
    assertTrue(r2.success && approxEqual(r2.value, 0.0), "Compiled eval at x=-1: 1 - 2 + 1 == 0");
}

void testDivisionByZeroAndDomainErrors()
{
    std::cout << "\n--- testDivisionByZeroAndDomainErrors ---" << std::endl;
    ExpressionEvaluator::VariableMap vars;

    auto r1 = ExpressionEvaluator::evaluateInfix("10 / 0", vars);
    assertTrue(!r1.success && r1.errorCode == ExpressionErrorCode::DivisionByZero,
               "10 / 0 rejected as DivisionByZero");

    auto r2 = ExpressionEvaluator::evaluateInfix("5 / (2 - 2)", vars);
    assertTrue(!r2.success && r2.errorCode == ExpressionErrorCode::DivisionByZero,
               "5 / (2 - 2) rejected as DivisionByZero");

    // 0^0
    auto r3 = ExpressionEvaluator::evaluateInfix("0^0", vars);
    assertTrue(!r3.success && r3.errorCode == ExpressionErrorCode::InvalidNumericDomain,
               "0^0 rejected as InvalidNumericDomain");

    // Negative base with fractional exponent: (-4)^0.5
    auto r4 = ExpressionEvaluator::evaluateInfix("(-4)^0.5", vars);
    assertTrue(!r4.success && r4.errorCode == ExpressionErrorCode::InvalidNumericDomain,
               "(-4)^0.5 rejected as InvalidNumericDomain");

    // Negative base with integer exponent is allowed: (-4)^2 = 16
    auto r5 = ExpressionEvaluator::evaluateInfix("(-4)^2", vars);
    assertTrue(r5.success && approxEqual(r5.value, 16.0), "(-4)^2 allowed and equals 16");
}

void testMalformedExpressions()
{
    std::cout << "\n--- testMalformedExpressions ---" << std::endl;
    ExpressionEvaluator::VariableMap vars;

    // Empty expression
    auto rEmpty = ExpressionEvaluator::evaluateInfix("   ", vars);
    assertTrue(!rEmpty.success && rEmpty.errorCode == ExpressionErrorCode::EmptyExpression,
               "Empty expression rejected");

    // Missing operator: 2 3
    auto rMissingOp1 = ExpressionEvaluator::evaluateInfix("2 3", vars);
    assertTrue(!rMissingOp1.success && rMissingOp1.errorCode == ExpressionErrorCode::MissingOperator,
               "'2 3' rejected for missing operator");

    // Missing operator: 2(3)
    auto rMissingOp2 = ExpressionEvaluator::evaluateInfix("2(3)", vars);
    assertTrue(!rMissingOp2.success && rMissingOp2.errorCode == ExpressionErrorCode::MissingOperator,
               "'2(3)' rejected for missing operator");

    // Leading binary operator: + 5
    auto rLeadOp = ExpressionEvaluator::evaluateInfix("+ 5", vars);
    assertTrue(!rLeadOp.success && rLeadOp.errorCode == ExpressionErrorCode::MissingOperand,
               "'+ 5' rejected for missing operand");

    // Trailing binary operator: 5 +
    auto rTrailOp = ExpressionEvaluator::evaluateInfix("5 +", vars);
    assertTrue(!rTrailOp.success && rTrailOp.errorCode == ExpressionErrorCode::MissingOperand,
               "'5 +' rejected for missing operand");

    // Trailing unary operator: 5 * -
    auto rTrailUnary = ExpressionEvaluator::evaluateInfix("5 * -", vars);
    assertTrue(!rTrailUnary.success && rTrailUnary.errorCode == ExpressionErrorCode::MissingOperand,
               "'5 * -' rejected for missing operand");

    // Mismatched parentheses: (1 + 2
    auto rUnclosedParen = ExpressionEvaluator::evaluateInfix("(1 + 2", vars);
    assertTrue(!rUnclosedParen.success && rUnclosedParen.errorCode == ExpressionErrorCode::MismatchedParentheses,
               "'(1 + 2' rejected for mismatched parentheses");

    // Mismatched parentheses: 1 + 2)
    auto rUnopenedParen = ExpressionEvaluator::evaluateInfix("1 + 2)", vars);
    assertTrue(!rUnopenedParen.success && rUnopenedParen.errorCode == ExpressionErrorCode::MismatchedParentheses,
               "'1 + 2)' rejected for mismatched parentheses");

    // Comma outside function: 1, 2
    auto rBadComma = ExpressionEvaluator::evaluateInfix("1, 2", vars);
    assertTrue(!rBadComma.success && rBadComma.errorCode == ExpressionErrorCode::MismatchedComma,
               "'1, 2' rejected for mismatched comma");

    // Function arity errors: max(1)
    auto rArity1 = ExpressionEvaluator::evaluateInfix("max(1)", vars);
    assertTrue(!rArity1.success && rArity1.errorCode == ExpressionErrorCode::InvalidArgumentCount,
               "'max(1)' rejected for incorrect argument count");

    // Function arity errors: max()
    auto rArity0 = ExpressionEvaluator::evaluateInfix("max()", vars);
    assertTrue(!rArity0.success && rArity0.errorCode == ExpressionErrorCode::InvalidArgumentCount,
               "'max()' rejected for incorrect argument count");

    // Function arity errors: max(1, 2, 3)
    auto rArity3 = ExpressionEvaluator::evaluateInfix("max(1, 2, 3)", vars);
    assertTrue(!rArity3.success && rArity3.errorCode == ExpressionErrorCode::InvalidArgumentCount,
               "'max(1, 2, 3)' rejected for incorrect argument count");

    // Invalid character
    auto rBadChar = ExpressionEvaluator::evaluateInfix("2 $ 3", vars);
    assertTrue(!rBadChar.success && rBadChar.errorCode == ExpressionErrorCode::InvalidCharacter,
               "'2 $ 3' rejected for invalid character");

    // Invalid number format: multiple decimal points
    auto rBadNum = ExpressionEvaluator::evaluateInfix("1.2.3 + 4", vars);
    assertTrue(!rBadNum.success && rBadNum.errorCode == ExpressionErrorCode::InvalidNumberFormat,
               "'1.2.3' rejected for invalid number format");
}
} // namespace

int main()
{
    std::cout << "Running test_expression_evaluator..." << std::endl;
    testBasicArithmetic();
    testPrecedenceAndAssociativity();
    testFunctions();
    testVariables();
    testCompiledExpressionReuse();
    testDivisionByZeroAndDomainErrors();
    testMalformedExpressions();
    std::cout << "\nAll expression evaluator tests passed successfully!" << std::endl;
    return 0;
}
