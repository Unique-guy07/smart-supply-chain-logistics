#pragma once

#include "order/ExpressionEvaluator.h"
#include "order/Order.h"
#include <string>
#include <unordered_map>

/// Result structure returned by freight cost calculation.
struct FreightCalculationResult
{
    bool success{false};
    double cost{0.0};
    std::string errorMessage;

    static FreightCalculationResult ok(double amount)
    {
        FreightCalculationResult r;
        r.success = true;
        r.cost = amount;
        return r;
    }

    static FreightCalculationResult fail(const std::string& msg)
    {
        FreightCalculationResult r;
        r.success = false;
        r.cost = 0.0;
        r.errorMessage = msg;
        return r;
    }
};

/// Dynamic freight pricing engine powered by ExpressionEvaluator.
///
/// Default formula:
/// max(base_fee, (base_fee + distance * distance_rate + weight * weight_rate) * urgency_multiplier)
///
/// Default rates:
/// - base_fee = 10.00
/// - distance_rate = 0.50 (per km)
/// - weight_rate = 0.80 (per kg)
/// - urgency_multiplier: Low=0.8, Normal=1.0, High=1.4, Urgent=2.0
///
/// Bound limits:
/// - Max distance: 50,000.0 km
/// - Max weight: 100,000.0 kg
/// - Max freight ceiling: 10,000,000.00
class FreightPricingEngine
{
public:
    static constexpr double DEFAULT_BASE_FEE = 10.00;
    static constexpr double DEFAULT_DISTANCE_RATE = 0.50;
    static constexpr double DEFAULT_WEIGHT_RATE = 0.80;

    static constexpr double MAX_DISTANCE_KM = 50000.0;
    static constexpr double MAX_WEIGHT_KG = 100000.0;
    static constexpr double MAX_FREIGHT_CEILING = 10000000.0;

    FreightPricingEngine();

    /// Updates the dynamic pricing formula for future orders.
    /// @return true if expression compiles and validates successfully; false otherwise.
    bool setFormula(const std::string& formulaStr);

    /// Gets the current infix pricing formula string.
    [[nodiscard]] const std::string& getFormula() const noexcept;

    // Rate setters
    bool setBaseFee(double fee) noexcept;
    bool setDistanceRate(double rate) noexcept;
    bool setWeightRate(double rate) noexcept;
    bool setUrgencyMultiplier(OrderPriority priority, double multiplier) noexcept;

    // Rate getters
    [[nodiscard]] double getBaseFee() const noexcept;
    [[nodiscard]] double getDistanceRate() const noexcept;
    [[nodiscard]] double getWeightRate() const noexcept;
    [[nodiscard]] double getUrgencyMultiplier(OrderPriority priority) const noexcept;

    /// Calculates freight cost given distance, weight, and priority.
    /// Applies two-decimal half-up rounding.
    [[nodiscard]] FreightCalculationResult calculateFreight(double distance,
                                                            double weight,
                                                            OrderPriority priority) const;

    /// Calculates freight cost directly from an Order instance.
    [[nodiscard]] FreightCalculationResult calculateFreight(const Order& order) const;

private:
    std::string formulaString;
    CompiledExpression compiledFormula;

    double baseFee{DEFAULT_BASE_FEE};
    double distanceRate{DEFAULT_DISTANCE_RATE};
    double weightRate{DEFAULT_WEIGHT_RATE};

    double lowMultiplier{0.8};
    double normalMultiplier{1.0};
    double highMultiplier{1.4};
    double urgentMultiplier{2.0};

    static double roundHalfUp(double value) noexcept;
};
