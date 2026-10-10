#include "inventory/SlottingEngine.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace
{
struct ItemData
{
    const Product* product{nullptr};
    double inventoryValue{0.0};
    bool hasValidInputs{true};
    std::string customNote;
};
} // namespace

const char* to_string(ABCClass classification) noexcept
{
    switch (classification)
    {
    case ABCClass::A:
        return "A";
    case ABCClass::B:
        return "B";
    case ABCClass::C:
        return "C";
    }
    return "Unknown";
}

bool SlottingZoneMapping::matchesZone(ABCClass classification, const std::string& binLocation) const
{
    if (binLocation.empty())
    {
        return false;
    }

    std::string targetPrefix;
    char altLetter = ' ';
    switch (classification)
    {
    case ABCClass::A:
        targetPrefix = prefixClassA;
        altLetter = 'A';
        break;
    case ABCClass::B:
        targetPrefix = prefixClassB;
        altLetter = 'B';
        break;
    case ABCClass::C:
        targetPrefix = prefixClassC;
        altLetter = 'C';
        break;
    }

    auto startsWithCaseInsensitive = [](const std::string& str, const std::string& prefix) {
        if (str.length() < prefix.length())
        {
            return false;
        }
        for (std::size_t i = 0; i < prefix.length(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(str[i])) !=
                std::tolower(static_cast<unsigned char>(prefix[i])))
            {
                return false;
            }
        }
        return true;
    };

    // 1. Direct match against configured target prefix (e.g. "ZONE_A")
    if (!targetPrefix.empty() && startsWithCaseInsensitive(binLocation, targetPrefix))
    {
        return true;
    }

    // 2. Standard alternate tier formats: e.g. "A-...", "A/...", or single letter "A"
    std::string altPrefixHyphen = std::string(1, altLetter) + "-";
    if (startsWithCaseInsensitive(binLocation, altPrefixHyphen))
    {
        return true;
    }

    std::string altPrefixSlash = std::string(1, altLetter) + "/";
    if (startsWithCaseInsensitive(binLocation, altPrefixSlash))
    {
        return true;
    }

    if (binLocation.length() == 1 &&
        std::toupper(static_cast<unsigned char>(binLocation[0])) == altLetter)
    {
        return true;
    }

    return false;
}

SlottingReport SlottingEngine::analyze(
    const std::vector<const Product*>& products,
    const ABCThresholds& thresholds,
    const SlottingZoneMapping& mapping)
{
    SlottingReport report;
    ABCThresholds activeThresholds = thresholds.isValid() ? thresholds : ABCThresholds{};

    if (products.empty())
    {
        return report;
    }

    std::vector<ItemData> items;
    items.reserve(products.size());
    double totalVal = 0.0;

    for (const Product* p : products)
    {
        if (p == nullptr)
        {
            continue;
        }

        ItemData item;
        item.product = p;

        double price = p->getPrice();
        int quantity = p->getQuantity();

        if (!std::isfinite(price) || price < 0.0 || quantity < 0)
        {
            item.inventoryValue = 0.0;
            item.hasValidInputs = false;
            item.customNote = "Invalid non-finite or negative price/quantity treated as 0.0";
        }
        else
        {
            double val = price * static_cast<double>(quantity);
            if (!std::isfinite(val))
            {
                report.isValid = false;
                report.errorMessage = "Product valuation multiplication overflow for product '" +
                                      p->getProductId() + "'";
                report.totalProducts = static_cast<int>(products.size());
                report.totalInventoryValue = 0.0;
                report.countClassA = 0;
                report.countClassB = 0;
                report.countClassC = 0;
                report.misSlottedCount = 0;
                report.recommendations.clear();
                return report;
            }
            else if (!std::isfinite(totalVal + val))
            {
                report.isValid = false;
                report.errorMessage = "Accumulated total inventory value overflow at product '" +
                                      p->getProductId() + "'";
                report.totalProducts = static_cast<int>(products.size());
                report.totalInventoryValue = 0.0;
                report.countClassA = 0;
                report.countClassB = 0;
                report.countClassC = 0;
                report.misSlottedCount = 0;
                report.recommendations.clear();
                return report;
            }
            else
            {
                item.inventoryValue = val;
                totalVal += val;
            }
        }

        items.push_back(item);
    }

    report.totalProducts = static_cast<int>(items.size());
    report.totalInventoryValue = totalVal;

    if (items.empty())
    {
        return report;
    }

    // Deterministic sorting:
    // 1. Descending by inventoryValue.
    // 2. Tie-breaker: Ascending by productId (lexicographic string comparison).
    std::sort(items.begin(), items.end(), [](const ItemData& a, const ItemData& b) {
        if (a.inventoryValue != b.inventoryValue)
        {
            return a.inventoryValue > b.inventoryValue;
        }
        return a.product->getProductId() < b.product->getProductId();
    });

    report.recommendations.reserve(items.size());

    // Edge case: total inventory value is zero (e.g. all prices or quantities 0)
    const bool zeroTotal = (totalVal <= 0.0);

    double runningTotal = 0.0;

    for (const auto& item : items)
    {
        SlottingRecommendation rec;
        rec.productId = item.product->getProductId();
        rec.productName = item.product->getName();
        rec.inventoryValue = item.inventoryValue;
        rec.currentBinLocation = item.product->getBinLocation();

        if (!item.customNote.empty())
        {
            rec.notes = item.customNote;
        }
        else if (!item.hasValidInputs)
        {
            rec.notes = "Invalid non-finite or negative price/quantity treated as 0.0";
        }

        if (zeroTotal || item.inventoryValue <= 0.0)
        {
            // Zero-value items deterministically belong to Class C (lowest priority tier)
            rec.classification = ABCClass::C;
            rec.valuePercentage = 0.0;
            rec.cumulativePercentage = zeroTotal ? 0.0 : 100.0;
        }
        else
        {
            double prevPct = (runningTotal / totalVal) * 100.0;
            runningTotal += item.inventoryValue;
            double cumPct = (runningTotal / totalVal) * 100.0;

            rec.valuePercentage = (item.inventoryValue / totalVal) * 100.0;
            rec.cumulativePercentage = cumPct;

            // Discrete Pareto cutoff:
            // Any item whose inclusion begins before the Class A threshold belongs to Class A.
            // Subsequent items beginning before the Class B threshold belong to Class B.
            // All remaining items belong to Class C.
            if (prevPct < activeThresholds.cumClassAPercent)
            {
                rec.classification = ABCClass::A;
            }
            else if (prevPct < activeThresholds.cumClassBPercent)
            {
                rec.classification = ABCClass::B;
            }
            else
            {
                rec.classification = ABCClass::C;
            }
        }

        // Assign recommended zone tier based on classification
        switch (rec.classification)
        {
        case ABCClass::A:
            rec.recommendedZone = mapping.prefixClassA;
            ++report.countClassA;
            break;
        case ABCClass::B:
            rec.recommendedZone = mapping.prefixClassB;
            ++report.countClassB;
            break;
        case ABCClass::C:
            rec.recommendedZone = mapping.prefixClassC;
            ++report.countClassC;
            break;
        }

        // Evaluate whether current location matches recommended zone
        if (!mapping.matchesZone(rec.classification, rec.currentBinLocation))
        {
            rec.isMisSlotted = true;
            ++report.misSlottedCount;
            std::string mismatchNote = "Current bin '" + rec.currentBinLocation +
                                       "' does not match recommended " + rec.recommendedZone;
            if (rec.notes.empty())
            {
                rec.notes = mismatchNote;
            }
            else
            {
                rec.notes += "; " + mismatchNote;
            }
        }
        else
        {
            rec.isMisSlotted = false;
        }

        report.recommendations.push_back(std::move(rec));
    }

    return report;
}
