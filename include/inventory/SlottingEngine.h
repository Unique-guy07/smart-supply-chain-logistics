#pragma once

#include "inventory/Product.h"
#include <string>
#include <vector>

/// Strongly typed ABC inventory classification tiers based on Pareto inventory analysis.
enum class ABCClass
{
    A,  ///< Class A: High value / high velocity items (top cumulative value share)
    B,  ///< Class B: Moderate value / velocity items
    C   ///< Class C: Low value / slow-moving items
};

[[nodiscard]] const char* to_string(ABCClass classification) noexcept;

/// Configurable cumulative value percentage thresholds for ABC classification.
/// Note: Initial classification is based on inventory value (price * quantity).
/// This provides a rigorous monetary valuation proxy; it does NOT claim to directly
/// represent historical sales turnover velocity.
struct ABCThresholds
{
    double cumClassAPercent{70.0};  ///< Cumulative percentage cutoff for Class A (default: 70.0%)
    double cumClassBPercent{90.0};  ///< Cumulative percentage cutoff for Class B (default: 90.0%)

    [[nodiscard]] bool isValid() const noexcept
    {
        return cumClassAPercent > 0.0 &&
               cumClassAPercent < cumClassBPercent &&
               cumClassBPercent < 100.0;
    }
};

/// Configurable warehouse zone prefix mapping for storage locations.
/// Used to compare a product's current binLocation against recommended zone tiers.
struct SlottingZoneMapping
{
    std::string prefixClassA{"ZONE_A"};  ///< High-accessibility fast pick zone
    std::string prefixClassB{"ZONE_B"};  ///< Standard-accessibility reserve zone
    std::string prefixClassC{"ZONE_C"};  ///< Deep-storage / bulk / high-rack zone

    /// Evaluates whether a product's current binLocation matches the expected zone for its class.
    [[nodiscard]] bool matchesZone(ABCClass classification, const std::string& binLocation) const;
};

/// Individual recommendation record for an inventory product.
struct SlottingRecommendation
{
    std::string productId;
    std::string productName;
    ABCClass classification{ABCClass::C};
    double inventoryValue{0.0};
    double valuePercentage{0.0};
    double cumulativePercentage{0.0};
    std::string currentBinLocation;
    std::string recommendedZone;
    bool isMisSlotted{false};
    std::string notes;
};

/// Complete aggregate slotting and classification report.
struct SlottingReport
{
    std::vector<SlottingRecommendation> recommendations;
    int totalProducts{0};
    int misSlottedCount{0};
    double totalInventoryValue{0.0};
    int countClassA{0};
    int countClassB{0};
    int countClassC{0};
    bool isValid{true};
    std::string errorMessage;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return isValid;
    }
};

/// Hierarchical inventory slotting engine.
///
/// Implements deterministic Pareto ABC classification and provides location
/// recommendations without mutating canonical inventory state.
class SlottingEngine
{
public:
    /// Analyzes a set of canonical product pointers and produces an ABC slotting report.
    ///
    /// Complexity: O(N log N) time due to descending value sorting, O(N) auxiliary space.
    ///
    /// Deterministic rules:
    /// - Items sorted descending by inventoryValue (price * quantity).
    /// - Ties in inventoryValue are broken deterministically by productId ascending.
    /// - Negative or non-finite values are handled deterministically (flagged with 0 value).
    /// - If any product valuation or accumulated total overflows, the report is marked
    ///   invalid (isValid = false) with a descriptive errorMessage and empty recommendations.
    /// - If total inventory value is 0.0, all items are assigned Class C.
    /// - If inventory is empty, an empty report is returned.
    /// - Single positive product is classified as Class A.
    [[nodiscard]] static SlottingReport analyze(
        const std::vector<const Product*>& products,
        const ABCThresholds& thresholds = ABCThresholds{},
        const SlottingZoneMapping& mapping = SlottingZoneMapping{});
};
