#include "order/Order.h"
#include <cmath>

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

Order::Order()
    : orderId("")
    , customerId("")
    , priority(OrderPriority::Normal)
    , status(OrderStatus::Pending)
    , totalPrice(0.0)
    , shippingDistance(0.0)
    , shippingWeight(0.0)
    , freightCost(0.0)
    , failureReason("")
    , isFrozen(false)
{
}

Order::Order(std::string orderId, std::string customerId, OrderPriority priority)
    : orderId(std::move(orderId))
    , customerId(std::move(customerId))
    , priority(priority)
    , status(OrderStatus::Pending)
    , totalPrice(0.0)
    , shippingDistance(0.0)
    , shippingWeight(0.0)
    , freightCost(0.0)
    , failureReason("")
    , isFrozen(false)
{
}

// ---------------------------------------------------------------------------
// Item addition & shipping configuration
// ---------------------------------------------------------------------------

bool Order::addItem(const std::string& productId, int quantity)
{
    if (isFrozen) {
        return false;
    }
    if (productId.empty() || quantity <= 0) {
        return false;
    }

    OrderItem item;
    item.productId = productId;
    item.quantity = quantity;
    item.unitPriceSnapshot = 0.0;
    items.push_back(std::move(item));
    return true;
}

bool Order::setShippingParameters(double distance, double weight)
{
    if (isFrozen) {
        return false;
    }
    if (!std::isfinite(distance) || distance < 0.0 || distance > 50000.0) {
        return false;
    }
    if (!std::isfinite(weight) || weight < 0.0 || weight > 100000.0) {
        return false;
    }

    shippingDistance = distance;
    shippingWeight = weight;
    return true;
}

// ---------------------------------------------------------------------------
// Private OrderManager-exclusive mutations
// ---------------------------------------------------------------------------

void Order::freeze() noexcept
{
    isFrozen = true;
}

bool Order::setStatus(OrderStatus newStatus) noexcept
{
    if (status == newStatus) {
        return true;
    }

    switch (status) {
    case OrderStatus::Pending:
        if (newStatus == OrderStatus::Processing || newStatus == OrderStatus::Cancelled) {
            status = newStatus;
            return true;
        }
        return false;

    case OrderStatus::Processing:
        if (newStatus == OrderStatus::Completed || newStatus == OrderStatus::Failed) {
            status = newStatus;
            return true;
        }
        return false;

    case OrderStatus::Completed:
    case OrderStatus::Failed:
    case OrderStatus::Cancelled:
        // Terminal states cannot transition to any other state
        return false;
    }

    return false;
}

void Order::setFailureReason(const std::string& reason)
{
    failureReason = reason;
}

void Order::setItemPriceSnapshot(std::size_t index, double price)
{
    if (index < items.size()) {
        items[index].unitPriceSnapshot = price;
    }
}

void Order::setTotalPrice(double total) noexcept
{
    totalPrice = total;
}

void Order::setFreightCost(double cost) noexcept
{
    freightCost = cost;
}

// ---------------------------------------------------------------------------
// Getters
// ---------------------------------------------------------------------------

const std::string& Order::getOrderId() const noexcept
{
    return orderId;
}

const std::string& Order::getCustomerId() const noexcept
{
    return customerId;
}

OrderPriority Order::getPriority() const noexcept
{
    return priority;
}

OrderStatus Order::getStatus() const noexcept
{
    return status;
}

const std::vector<OrderItem>& Order::getItems() const noexcept
{
    return items;
}

double Order::getTotalPrice() const noexcept
{
    return totalPrice;
}

double Order::getShippingDistance() const noexcept
{
    return shippingDistance;
}

double Order::getShippingWeight() const noexcept
{
    return shippingWeight;
}

double Order::getFreightCost() const noexcept
{
    return freightCost;
}

double Order::getGrandTotal() const noexcept
{
    return totalPrice + freightCost;
}

const std::string& Order::getFailureReason() const noexcept
{
    return failureReason;
}

bool Order::isSubmitted() const noexcept
{
    return isFrozen;
}
