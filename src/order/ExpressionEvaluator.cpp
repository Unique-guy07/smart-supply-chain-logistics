#include "order/ExpressionEvaluator.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace
{
constexpr double MAX_MAGNITUDE = 1e15;
constexpr double EPSILON = 1e-12;

bool isIdentifierStart(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_');
}

bool isIdentifierChar(char c)
{
    return isIdentifierStart(c) || (c >= '0' && c <= '9');
}
} // namespace

ExpressionResult ExpressionEvaluator::tokenize(const std::string& expr, std::vector<Token>& outTokens)
{
    outTokens.clear();
    const size_t len = expr.length();
    size_t i = 0;

    // Helper to check if next non-whitespace char exists
    auto peekNextNonWhitespace = [&](size_t start) -> char {
        for (size_t k = start; k < len; ++k)
        {
            if (!std::isspace(static_cast<unsigned char>(expr[k])))
            {
                return expr[k];
            }
        }
        return '\0';
    };

    while (i < len)
    {
        char c = expr[i];

        if (std::isspace(static_cast<unsigned char>(c)))
        {
            ++i;
            continue;
        }

        // Numbers: digits or leading decimal dot followed by digit
        if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && i + 1 < len && std::isdigit(static_cast<unsigned char>(expr[i + 1]))))
        {
            size_t start = i;
            bool seenDot = false;
            bool seenExp = false;

            while (i < len)
            {
                char ch = expr[i];
                if (ch == '.')
                {
                    if (seenDot || seenExp)
                    {
                        return ExpressionResult::fail(ExpressionErrorCode::InvalidNumberFormat,
                                                      "Invalid number format: multiple decimal points");
                    }
                    seenDot = true;
                    ++i;
                }
                else if (ch == 'e' || ch == 'E')
                {
                    if (seenExp)
                    {
                        return ExpressionResult::fail(ExpressionErrorCode::InvalidNumberFormat,
                                                      "Invalid number format: multiple exponent indicators");
                    }
                    seenExp = true;
                    ++i;
                    if (i < len && (expr[i] == '+' || expr[i] == '-'))
                    {
                        ++i;
                    }
                    if (i >= len || !std::isdigit(static_cast<unsigned char>(expr[i])))
                    {
                        return ExpressionResult::fail(ExpressionErrorCode::InvalidNumberFormat,
                                                      "Invalid number format: missing exponent digits");
                    }
                }
                else if (std::isdigit(static_cast<unsigned char>(ch)))
                {
                    ++i;
                }
                else
                {
                    break;
                }
            }

            std::string numStr = expr.substr(start, i - start);
            char* endPtr = nullptr;
            double val = std::strtod(numStr.c_str(), &endPtr);
            if (endPtr != numStr.c_str() + numStr.length())
            {
                return ExpressionResult::fail(ExpressionErrorCode::InvalidNumberFormat,
                                              "Invalid number literal: " + numStr);
            }
            if (!std::isfinite(val) || std::abs(val) > MAX_MAGNITUDE)
            {
                return ExpressionResult::fail(ExpressionErrorCode::NumericOverflow,
                                              "Numeric literal exceeds allowable bounds: " + numStr);
            }

            Token tok;
            tok.type = TokenType::Number;
            tok.text = numStr;
            tok.numberValue = val;
            outTokens.push_back(tok);
            continue;
        }

        // Standalone dot error
        if (c == '.')
        {
            return ExpressionResult::fail(ExpressionErrorCode::InvalidNumberFormat,
                                          "Unexpected isolated decimal point");
        }

        // Identifiers or functions
        if (isIdentifierStart(c))
        {
            size_t start = i;
            while (i < len && isIdentifierChar(expr[i]))
            {
                ++i;
            }
            std::string name = expr.substr(start, i - start);

            if (name == "max" || name == "min")
            {
                Token tok;
                tok.type = TokenType::Function;
                tok.text = name;
                tok.isFunction = true;
                tok.precedence = 5;
                outTokens.push_back(tok);
            }
            else
            {
                Token tok;
                tok.type = TokenType::Identifier;
                tok.text = name;
                outTokens.push_back(tok);
            }
            continue;
        }

        // Parentheses and Comma
        if (c == '(')
        {
            Token tok;
            tok.type = TokenType::LeftParen;
            tok.text = "(";
            outTokens.push_back(tok);
            ++i;
            continue;
        }
        if (c == ')')
        {
            Token tok;
            tok.type = TokenType::RightParen;
            tok.text = ")";
            outTokens.push_back(tok);
            ++i;
            continue;
        }
        if (c == ',')
        {
            Token tok;
            tok.type = TokenType::Comma;
            tok.text = ",";
            outTokens.push_back(tok);
            ++i;
            continue;
        }

        // Operators
        if (c == '+')
        {
            Token tok;
            tok.type = TokenType::Plus;
            tok.text = "+";
            tok.precedence = 1;
            tok.associativity = Associativity::Left;
            outTokens.push_back(tok);
            ++i;
            continue;
        }
        if (c == '-')
        {
            // Disambiguate unary vs binary minus
            bool isUnary = false;
            if (outTokens.empty())
            {
                isUnary = true;
            }
            else
            {
                TokenType prevType = outTokens.back().type;
                if (prevType == TokenType::Plus || prevType == TokenType::Minus ||
                    prevType == TokenType::Multiply || prevType == TokenType::Divide ||
                    prevType == TokenType::Power || prevType == TokenType::UnaryMinus ||
                    prevType == TokenType::LeftParen || prevType == TokenType::Comma)
                {
                    isUnary = true;
                }
            }

            Token tok;
            if (isUnary)
            {
                tok.type = TokenType::UnaryMinus;
                tok.text = "-";
                tok.precedence = 3;
                tok.associativity = Associativity::Right;
                tok.isUnary = true;
            }
            else
            {
                tok.type = TokenType::Minus;
                tok.text = "-";
                tok.precedence = 1;
                tok.associativity = Associativity::Left;
            }
            outTokens.push_back(tok);
            ++i;
            continue;
        }
        if (c == '*')
        {
            Token tok;
            tok.type = TokenType::Multiply;
            tok.text = "*";
            tok.precedence = 2;
            tok.associativity = Associativity::Left;
            outTokens.push_back(tok);
            ++i;
            continue;
        }
        if (c == '/')
        {
            Token tok;
            tok.type = TokenType::Divide;
            tok.text = "/";
            tok.precedence = 2;
            tok.associativity = Associativity::Left;
            outTokens.push_back(tok);
            ++i;
            continue;
        }
        if (c == '^')
        {
            Token tok;
            tok.type = TokenType::Power;
            tok.text = "^";
            tok.precedence = 4;
            tok.associativity = Associativity::Right;
            outTokens.push_back(tok);
            ++i;
            continue;
        }

        return ExpressionResult::fail(ExpressionErrorCode::InvalidCharacter,
                                      std::string("Unrecognized character in expression: '") + c + "'");
    }

    if (outTokens.empty())
    {
        return ExpressionResult::fail(ExpressionErrorCode::EmptyExpression, "Expression is empty");
    }

    // Sequence syntax validation to catch missing operators and missing operands early
    for (size_t idx = 0; idx < outTokens.size(); ++idx)
    {
        const Token& cur = outTokens[idx];
        const Token* next = (idx + 1 < outTokens.size()) ? &outTokens[idx + 1] : nullptr;

        // Function must be followed by '('
        if (cur.type == TokenType::Function)
        {
            if (!next || next->type != TokenType::LeftParen)
            {
                return ExpressionResult::fail(ExpressionErrorCode::UnexpectedToken,
                                              "Function '" + cur.text + "' must be followed by '('");
            }
        }

        // Operand (Number or Identifier) or RightParen followed by an operand or '(' without operator
        if (cur.type == TokenType::Number || cur.type == TokenType::Identifier || cur.type == TokenType::RightParen)
        {
            if (next && (next->type == TokenType::Number || next->type == TokenType::Identifier ||
                         next->type == TokenType::Function || next->type == TokenType::LeftParen))
            {
                return ExpressionResult::fail(ExpressionErrorCode::MissingOperator,
                                              "Missing operator between '" + cur.text + "' and '" + next->text + "'");
            }
        }

        // Binary operator at start or end, or consecutive binary operators
        if (cur.type == TokenType::Plus || cur.type == TokenType::Minus ||
            cur.type == TokenType::Multiply || cur.type == TokenType::Divide ||
            cur.type == TokenType::Power)
        {
            if (idx == 0)
            {
                return ExpressionResult::fail(ExpressionErrorCode::MissingOperand,
                                              "Leading binary operator '" + cur.text + "' has no left operand");
            }
            if (!next)
            {
                return ExpressionResult::fail(ExpressionErrorCode::MissingOperand,
                                              "Trailing binary operator '" + cur.text + "' has no right operand");
            }
            if (next->type == TokenType::Plus || next->type == TokenType::Minus ||
                next->type == TokenType::Multiply || next->type == TokenType::Divide ||
                next->type == TokenType::Power || next->type == TokenType::RightParen ||
                next->type == TokenType::Comma)
            {
                return ExpressionResult::fail(ExpressionErrorCode::MissingOperand,
                                              "Operator '" + cur.text + "' followed by invalid token '" + next->text + "'");
            }
        }

        // Unary minus at end or followed by invalid token
        if (cur.type == TokenType::UnaryMinus)
        {
            if (!next)
            {
                return ExpressionResult::fail(ExpressionErrorCode::MissingOperand,
                                              "Trailing unary negation operator has no operand");
            }
            if (next->type == TokenType::Plus || next->type == TokenType::Minus ||
                next->type == TokenType::Multiply || next->type == TokenType::Divide ||
                next->type == TokenType::Power || next->type == TokenType::RightParen ||
                next->type == TokenType::Comma)
            {
                return ExpressionResult::fail(ExpressionErrorCode::MissingOperand,
                                              "Unary negation followed by invalid token '" + next->text + "'");
            }
        }

        // Comma at start or end
        if (cur.type == TokenType::Comma)
        {
            if (idx == 0 || !next)
            {
                return ExpressionResult::fail(ExpressionErrorCode::MismatchedComma,
                                              "Comma cannot appear at start or end of expression");
            }
            if (next->type == TokenType::RightParen || next->type == TokenType::Comma)
            {
                return ExpressionResult::fail(ExpressionErrorCode::MissingOperand,
                                              "Missing operand near comma");
            }
        }

        // Empty parentheses: '()'
        if (cur.type == TokenType::LeftParen && next && next->type == TokenType::RightParen)
        {
            // If preceded by function: e.g. max(), report invalid argument count
            if (idx > 0 && outTokens[idx - 1].type == TokenType::Function)
            {
                return ExpressionResult::fail(ExpressionErrorCode::InvalidArgumentCount,
                                              "Function '" + outTokens[idx - 1].text + "' expects 2 arguments, got 0");
            }
            return ExpressionResult::fail(ExpressionErrorCode::MissingOperand, "Empty parentheses '()'");
        }
    }

    return ExpressionResult::ok(0.0);
}

namespace
{
struct FunctionCallContext
{
    std::string name;
    int argCount{0};
    bool hasContent{false};
};
} // namespace

ExpressionResult ExpressionEvaluator::shuntingYard(const std::vector<Token>& tokens, std::vector<Token>& outRpn)
{
    outRpn.clear();
    Stack<Token> opStack;
    Stack<FunctionCallContext> funcCtxStack;

    for (const auto& tok : tokens)
    {
        if (tok.type == TokenType::Number || tok.type == TokenType::Identifier)
        {
            outRpn.push_back(tok);
            if (!funcCtxStack.empty())
            {
                funcCtxStack.top().hasContent = true;
            }
        }
        else if (tok.type == TokenType::Function)
        {
            opStack.push(tok);
        }
        else if (tok.type == TokenType::LeftParen)
        {
            Token parenTok = tok;
            if (!opStack.empty() && opStack.top().type == TokenType::Function)
            {
                parenTok.isFunction = true;
                FunctionCallContext ctx;
                ctx.name = opStack.top().text;
                ctx.argCount = 0;
                ctx.hasContent = false;
                funcCtxStack.push(ctx);
            }
            opStack.push(parenTok);
        }
        else if (tok.type == TokenType::Comma)
        {
            if (funcCtxStack.empty())
            {
                return ExpressionResult::fail(ExpressionErrorCode::MismatchedComma,
                                              "Comma outside of function invocation");
            }
            if (!funcCtxStack.top().hasContent)
            {
                return ExpressionResult::fail(ExpressionErrorCode::MissingOperand,
                                              "Empty argument before comma in function '" + funcCtxStack.top().name + "'");
            }

            while (!opStack.empty() && opStack.top().type != TokenType::LeftParen)
            {
                outRpn.push_back(opStack.top());
                opStack.pop();
            }

            if (opStack.empty())
            {
                return ExpressionResult::fail(ExpressionErrorCode::MismatchedParentheses,
                                              "Missing '(' for comma in function call");
            }

            funcCtxStack.top().argCount++;
            funcCtxStack.top().hasContent = false;
        }
        else if (tok.type == TokenType::RightParen)
        {
            while (!opStack.empty() && opStack.top().type != TokenType::LeftParen)
            {
                outRpn.push_back(opStack.top());
                opStack.pop();
            }

            if (opStack.empty())
            {
                return ExpressionResult::fail(ExpressionErrorCode::MismatchedParentheses,
                                              "Mismatched closing parenthesis ')'");
            }

            Token leftParen = opStack.top();
            opStack.pop();

            if (leftParen.isFunction)
            {
                if (opStack.empty() || opStack.top().type != TokenType::Function)
                {
                    return ExpressionResult::fail(ExpressionErrorCode::UnexpectedToken,
                                                  "Internal parser error: missing function operator below parenthesis");
                }
                Token funcTok = opStack.top();
                opStack.pop();

                if (funcCtxStack.empty())
                {
                    return ExpressionResult::fail(ExpressionErrorCode::UnexpectedToken,
                                                  "Internal parser error: empty function context stack");
                }
                FunctionCallContext ctx = funcCtxStack.top();
                funcCtxStack.pop();

                if (ctx.hasContent)
                {
                    ctx.argCount++;
                }

                if (ctx.argCount != 2)
                {
                    return ExpressionResult::fail(ExpressionErrorCode::InvalidArgumentCount,
                                                  "Function '" + ctx.name + "' expects exactly 2 arguments, got " +
                                                      std::to_string(ctx.argCount));
                }

                outRpn.push_back(funcTok);

                // If this function call was an argument in an outer function, mark outer content as present
                if (!funcCtxStack.empty())
                {
                    funcCtxStack.top().hasContent = true;
                }
            }
            else
            {
                // Normal grouping sub-expression completed
                if (!funcCtxStack.empty())
                {
                    funcCtxStack.top().hasContent = true;
                }
            }
        }
        else // Operators: +, -, *, /, ^, UnaryMinus
        {
            if (tok.isUnary)
            {
                // Unary prefix operator does not pop preceding binary operators
                opStack.push(tok);
            }
            else
            {
                while (!opStack.empty())
                {
                    const Token& topOp = opStack.top();
                    if (topOp.type == TokenType::LeftParen || topOp.type == TokenType::Function)
                    {
                        break;
                    }

                    bool shouldPop = false;
                    if (topOp.precedence > tok.precedence)
                    {
                        shouldPop = true;
                    }
                    else if (topOp.precedence == tok.precedence && tok.associativity == Associativity::Left)
                    {
                        shouldPop = true;
                    }

                    if (shouldPop)
                    {
                        outRpn.push_back(topOp);
                        opStack.pop();
                    }
                    else
                    {
                        break;
                    }
                }
                opStack.push(tok);
            }
        }
    }

    while (!opStack.empty())
    {
        const Token& topOp = opStack.top();
        if (topOp.type == TokenType::LeftParen)
        {
            return ExpressionResult::fail(ExpressionErrorCode::MismatchedParentheses,
                                          "Mismatched unclosed opening parenthesis '('");
        }
        outRpn.push_back(topOp);
        opStack.pop();
    }

    return ExpressionResult::ok(0.0);
}

ExpressionResult ExpressionEvaluator::compile(const std::string& infixExpr, CompiledExpression& outCompiled)
{
    outCompiled.valid = false;
    outCompiled.rpnTokens.clear();
    outCompiled.originalInfix.clear();

    std::vector<Token> tokens;
    ExpressionResult tokRes = tokenize(infixExpr, tokens);
    if (!tokRes.success)
    {
        return tokRes;
    }

    std::vector<Token> rpn;
    ExpressionResult shuntingRes = shuntingYard(tokens, rpn);
    if (!shuntingRes.success)
    {
        return shuntingRes;
    }

    outCompiled.rpnTokens = std::move(rpn);
    outCompiled.originalInfix = infixExpr;
    outCompiled.valid = true;
    return ExpressionResult::ok(0.0);
}

ExpressionResult ExpressionEvaluator::evaluate(const CompiledExpression& compiled, const VariableMap& variables)
{
    if (!compiled.isValid())
    {
        return ExpressionResult::fail(ExpressionErrorCode::EmptyExpression,
                                      "Cannot evaluate uncompiled or invalid expression");
    }

    Stack<double> evalStack;

    for (const auto& tok : compiled.getRpnTokens())
    {
        if (tok.type == TokenType::Number)
        {
            evalStack.push(tok.numberValue);
        }
        else if (tok.type == TokenType::Identifier)
        {
            auto it = variables.find(tok.text);
            if (it == variables.end())
            {
                return ExpressionResult::fail(ExpressionErrorCode::UndefinedVariable,
                                              "Undefined variable: '" + tok.text + "'");
            }
            double val = it->second;
            if (!std::isfinite(val))
            {
                return ExpressionResult::fail(ExpressionErrorCode::NumericOverflow,
                                              "Variable '" + tok.text + "' has non-finite value");
            }
            if (std::abs(val) > MAX_MAGNITUDE)
            {
                return ExpressionResult::fail(ExpressionErrorCode::NumericOverflow,
                                              "Variable '" + tok.text + "' exceeds allowable magnitude");
            }
            evalStack.push(val);
        }
        else if (tok.type == TokenType::UnaryMinus)
        {
            if (evalStack.empty())
            {
                return ExpressionResult::fail(ExpressionErrorCode::MissingOperand,
                                              "Missing operand for unary negation");
            }
            double operand = evalStack.top();
            evalStack.pop();
            double res = -operand;
            if (!std::isfinite(res) || std::abs(res) > MAX_MAGNITUDE)
            {
                return ExpressionResult::fail(ExpressionErrorCode::NumericOverflow,
                                              "Numeric overflow during unary negation");
            }
            evalStack.push(res);
        }
        else if (tok.type == TokenType::Plus || tok.type == TokenType::Minus ||
                 tok.type == TokenType::Multiply || tok.type == TokenType::Divide ||
                 tok.type == TokenType::Power)
        {
            if (evalStack.size() < 2)
            {
                return ExpressionResult::fail(ExpressionErrorCode::MissingOperand,
                                              "Missing operands for binary operator '" + tok.text + "'");
            }
            double b = evalStack.top();
            evalStack.pop();
            double a = evalStack.top();
            evalStack.pop();
            double res = 0.0;

            if (tok.type == TokenType::Plus)
            {
                res = a + b;
            }
            else if (tok.type == TokenType::Minus)
            {
                res = a - b;
            }
            else if (tok.type == TokenType::Multiply)
            {
                res = a * b;
            }
            else if (tok.type == TokenType::Divide)
            {
                if (std::abs(b) < EPSILON)
                {
                    return ExpressionResult::fail(ExpressionErrorCode::DivisionByZero,
                                                  "Division by zero or near-zero divisor");
                }
                res = a / b;
            }
            else if (tok.type == TokenType::Power)
            {
                if (std::abs(a) < EPSILON && b < 0.0)
                {
                    return ExpressionResult::fail(ExpressionErrorCode::DivisionByZero,
                                                  "Zero raised to negative power is undefined");
                }
                if (std::abs(a) < EPSILON && std::abs(b) < EPSILON)
                {
                    return ExpressionResult::fail(ExpressionErrorCode::InvalidNumericDomain,
                                                  "0^0 is an indeterminate form");
                }
                if (a < 0.0)
                {
                    double intPart;
                    if (std::modf(b, &intPart) != 0.0)
                    {
                        return ExpressionResult::fail(ExpressionErrorCode::InvalidNumericDomain,
                                                      "Negative base with fractional exponent produces complex value");
                    }
                }
                res = std::pow(a, b);
            }

            if (!std::isfinite(res) || std::abs(res) > MAX_MAGNITUDE)
            {
                return ExpressionResult::fail(ExpressionErrorCode::NumericOverflow,
                                              "Intermediate arithmetic overflow or non-finite result");
            }
            evalStack.push(res);
        }
        else if (tok.type == TokenType::Function)
        {
            if (evalStack.size() < 2)
            {
                return ExpressionResult::fail(ExpressionErrorCode::MissingOperand,
                                              "Insufficient arguments on stack for function '" + tok.text + "'");
            }
            double b = evalStack.top();
            evalStack.pop();
            double a = evalStack.top();
            evalStack.pop();
            double res = 0.0;

            if (tok.text == "max")
            {
                res = std::max(a, b);
            }
            else if (tok.text == "min")
            {
                res = std::min(a, b);
            }
            else
            {
                return ExpressionResult::fail(ExpressionErrorCode::UnexpectedToken,
                                              "Unsupported function: '" + tok.text + "'");
            }

            if (!std::isfinite(res) || std::abs(res) > MAX_MAGNITUDE)
            {
                return ExpressionResult::fail(ExpressionErrorCode::NumericOverflow,
                                              "Function result overflow or non-finite");
            }
            evalStack.push(res);
        }
        else
        {
            return ExpressionResult::fail(ExpressionErrorCode::UnexpectedToken,
                                          "Unexpected token in compiled RPN: '" + tok.text + "'");
        }
    }

    if (evalStack.size() != 1)
    {
        return ExpressionResult::fail(ExpressionErrorCode::MissingOperator,
                                      "Malformed expression left multiple operands on evaluation stack");
    }

    double finalValue = evalStack.top();
    evalStack.pop();

    if (!std::isfinite(finalValue) || std::abs(finalValue) > MAX_MAGNITUDE)
    {
        return ExpressionResult::fail(ExpressionErrorCode::NumericOverflow,
                                      "Final expression value is non-finite or overflows ceiling");
    }

    return ExpressionResult::ok(finalValue);
}

ExpressionResult ExpressionEvaluator::evaluateInfix(const std::string& infixExpr, const VariableMap& variables)
{
    CompiledExpression compiled;
    ExpressionResult compRes = compile(infixExpr, compiled);
    if (!compRes.success)
    {
        return compRes;
    }
    return evaluate(compiled, variables);
}
