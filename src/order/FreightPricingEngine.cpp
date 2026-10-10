#include "order/FreightPricingEngine.h"

#include <cmath>

namespace
{
constexpr const char* DEFAULT_FORMULA =
    "max(base_fee, (base_fee + distance * distance_rate + weight * weight_rate) * urgency_multiplier)";
} // namespace

FreightPricingEngine::FreightPricingEngine()
    : formulaString(DEFAULT_FORMULA)
{
    ExpressionEvaluator::compile(formulaString, compiledFormula);
}

bool FreightPricingEngine::setFormula(const std::string& formulaStr)
{
    if (formulaStr.empty())
    {
        return false;
    }

    CompiledExpression tempCompiled;
    auto compRes = ExpressionEvaluator::compile(formulaStr, tempCompiled);
    if (!compRes.success)
    {
        return false;
    }

    // Dry-run evaluation with dummy variables to ensure expression can be evaluated
    ExpressionEvaluator::VariableMap testVars = {
        {"base_fee", 10.0},
        {"distance", 1.0},
        {"distance_rate", 1.0},
        {"weight", 1.0},
        {"weight_rate", 1.0},
        {"urgency_multiplier", 1.0}
    };

    auto evalRes = ExpressionEvaluator::evaluate(tempCompiled, testVars);
    if (!evalRes.success)
    {
        return false;
    }

    formulaString = formulaStr;
    compiledFormula = std::move(tempCompiled);
    return true;
}

const std::string& FreightPricingEngine::getFormula() const noexcept
{
    return formulaString;
}

bool FreightPricingEngine::setBaseFee(double fee) noexcept
{
    if (!std::isfinite(fee) || fee < 0.0 || fee > MAX_FREIGHT_CEILING)
    {
        return false;
    }
    baseFee = fee;
    return true;
}

bool FreightPricingEngine::setDistanceRate(double rate) noexcept
{
    if (!std::isfinite(rate) || rate < 0.0 || rate > 10000.0)
    {
        return false;
    }
    distanceRate = rate;
    return true;
}

bool FreightPricingEngine::setWeightRate(double rate) noexcept
{
    if (!std::isfinite(rate) || rate < 0.0 || rate > 10000.0)
    {
        return false;
    }
    weightRate = rate;
    return true;
}

bool FreightPricingEngine::setUrgencyMultiplier(OrderPriority priority, double multiplier) noexcept
{
    if (!std::isfinite(multiplier) || multiplier <= 0.0 || multiplier > 100.0)
    {
        return false;
    }

    switch (priority)
    {
    case OrderPriority::Low:
        lowMultiplier = multiplier;
        return true;
    case OrderPriority::Normal:
        normalMultiplier = multiplier;
        return true;
    case OrderPriority::High:
        highMultiplier = multiplier;
        return true;
    case OrderPriority::Urgent:
        urgentMultiplier = multiplier;
        return true;
    }
    return false;
}

double FreightPricingEngine::getBaseFee() const noexcept
{
    return baseFee;
}

double FreightPricingEngine::getDistanceRate() const noexcept
{
    return distanceRate;
}

double FreightPricingEngine::getWeightRate() const noexcept
{
    return weightRate;
}

double FreightPricingEngine::getUrgencyMultiplier(OrderPriority priority) const noexcept
{
    switch (priority)
    {
    case OrderPriority::Low:
        return lowMultiplier;
    case OrderPriority::Normal:
        return normalMultiplier;
    case OrderPriority::High:
        return highMultiplier;
    case OrderPriority::Urgent:
        return urgentMultiplier;
    }
    return normalMultiplier;
}

double FreightPricingEngine::roundHalfUp(double value) noexcept
{
    return std::floor(value * 100.0 + 0.5) / 100.0;
}

FreightCalculationResult FreightPricingEngine::calculateFreight(double distance,
                                                               double weight,
                                                               OrderPriority priority) const
{
    if (!std::isfinite(distance) || distance < 0.0 || distance > MAX_DISTANCE_KM)
    {
        return FreightCalculationResult::fail("Shipping distance is invalid, negative, non-finite, or exceeds maximum limit");
    }

    if (!std::isfinite(weight) || weight < 0.0 || weight > MAX_WEIGHT_KG)
    {
        return FreightCalculationResult::fail("Shipping weight is invalid, negative, non-finite, or exceeds maximum limit");
    }

    ExpressionEvaluator::VariableMap variables = {
        {"base_fee", baseFee},
        {"distance", distance},
        {"distance_rate", distanceRate},
        {"weight", weight},
        {"weight_rate", weightRate},
        {"urgency_multiplier", getUrgencyMultiplier(priority)}
    };

    auto evalRes = ExpressionEvaluator::evaluate(compiledFormula, variables);
    if (!evalRes.success)
    {
        return FreightCalculationResult::fail("Dynamic formula evaluation failed: " + evalRes.errorMessage);
    }

    double rawCost = evalRes.value;
    if (!std::isfinite(rawCost) || rawCost < 0.0)
    {
        return FreightCalculationResult::fail("Calculated freight cost is negative or non-finite");
    }

    if (rawCost > MAX_FREIGHT_CEILING)
    {
        return FreightCalculationResult::fail("Calculated freight cost exceeds maximum ceiling");
    }

    double roundedCost = roundHalfUp(rawCost);
    if (roundedCost > MAX_FREIGHT_CEILING)
    {
        return FreightCalculationResult::fail("Rounded freight cost exceeds maximum ceiling");
    }

    return FreightCalculationResult::ok(roundedCost);
}

FreightCalculationResult FreightPricingEngine::calculateFreight(const Order& order) const
{
    return calculateFreight(order.getShippingDistance(), order.getShippingWeight(), order.getPriority());
}
