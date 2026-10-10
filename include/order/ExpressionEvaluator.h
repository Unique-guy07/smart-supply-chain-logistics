#pragma once

#include "order/Stack.h"
#include <string>
#include <unordered_map>
#include <vector>

/// Error codes for lexical analysis, parsing, and RPN evaluation.
enum class ExpressionErrorCode
{
    None = 0,
    EmptyExpression,
    InvalidCharacter,
    InvalidNumberFormat,
    MismatchedParentheses,
    MismatchedComma,
    UnexpectedToken,
    MissingOperator,
    MissingOperand,
    UndefinedVariable,
    DivisionByZero,
    InvalidNumericDomain,
    NumericOverflow,
    InvalidArgumentCount
};

/// Result structure returned by expression evaluation.
struct ExpressionResult
{
    bool success{false};
    double value{0.0};
    ExpressionErrorCode errorCode{ExpressionErrorCode::None};
    std::string errorMessage;

    static ExpressionResult ok(double val)
    {
        ExpressionResult r;
        r.success = true;
        r.value = val;
        r.errorCode = ExpressionErrorCode::None;
        return r;
    }

    static ExpressionResult fail(ExpressionErrorCode code, const std::string& msg)
    {
        ExpressionResult r;
        r.success = false;
        r.value = 0.0;
        r.errorCode = code;
        r.errorMessage = msg;
        return r;
    }
};

enum class TokenType
{
    Number,
    Identifier,
    Plus,
    Minus,
    Multiply,
    Divide,
    Power,
    UnaryMinus,
    LeftParen,
    RightParen,
    Comma,
    Function
};

enum class Associativity
{
    Left,
    Right
};

struct Token
{
    TokenType type{TokenType::Number};
    std::string text;
    double numberValue{0.0};
    int precedence{0};
    Associativity associativity{Associativity::Left};
    bool isUnary{false};
    bool isFunction{false};
    int functionArgCount{0}; // For function tracking during parsing
};

/// Pre-compiled expression in Reverse Polish Notation (RPN) for fast repeated evaluation.
class CompiledExpression
{
private:
    std::vector<Token> rpnTokens;
    std::string originalInfix;
    bool valid{false};

    friend class ExpressionEvaluator;

public:
    CompiledExpression() = default;

    [[nodiscard]] bool isValid() const noexcept
    {
        return valid;
    }

    [[nodiscard]] const std::string& getInfix() const noexcept
    {
        return originalInfix;
    }

    [[nodiscard]] const std::vector<Token>& getRpnTokens() const noexcept
    {
        return rpnTokens;
    }
};

/// Shunting Yard infix-to-RPN converter and RPN stack evaluator.
///
/// Implements Dijkstra's Shunting Yard algorithm using custom Stack<Token> for operator
/// precedence and custom Stack<double> for postfix evaluation.
class ExpressionEvaluator
{
public:
    using VariableMap = std::unordered_map<std::string, double>;

    /// Tokenizes and converts an infix expression into compiled RPN tokens.
    static ExpressionResult compile(const std::string& infixExpr, CompiledExpression& outCompiled);

    /// Evaluates a pre-compiled RPN expression given variable bindings.
    static ExpressionResult evaluate(const CompiledExpression& compiled, const VariableMap& variables);

    /// Convenience all-in-one method to compile and evaluate in a single step.
    static ExpressionResult evaluateInfix(const std::string& infixExpr, const VariableMap& variables);

private:
    static ExpressionResult tokenize(const std::string& expr, std::vector<Token>& outTokens);
    static ExpressionResult shuntingYard(const std::vector<Token>& tokens, std::vector<Token>& outRpn);
};
