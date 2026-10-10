#pragma once

#include <cstddef>
#include <string>
#include <vector>

/// Priority levels for order scheduling in M4.
enum class OrderPriority
{
    Low = 0,
    Normal = 1,
    High = 2,
    Urgent = 3
};

/// Operational lifecycle states of a customer order.
enum class OrderStatus
{
    Pending,
    Processing,
    Completed,
    Failed,
    Cancelled
};

/// Represents an individual line item in a customer order.
/// Unit price snapshot is populated authoritatively from the inventory catalog upon submission.
struct OrderItem
{
    std::string productId;
    int quantity{0};
    double unitPriceSnapshot{0.0};
};

/// Represents a customer order in the logistics platform.
///
/// Immutability contract:
/// - Callers construct an order and add requested items (productId and positive quantity).
/// - Callers do NOT supply authoritative prices; prices are snapshotted from the catalog on submission.
/// - Once submitted to OrderManager, the order is frozen: line items and pricing cannot be altered.
/// - Status transitions are restricted to OrderManager via friend declaration.
class Order
{
private:
    std::string orderId;
    std::string customerId;
    OrderPriority priority{OrderPriority::Normal};
    OrderStatus status{OrderStatus::Pending};
    std::vector<OrderItem> items;
    double totalPrice{0.0};
    double shippingDistance{0.0};
    double shippingWeight{0.0};
    double freightCost{0.0};
    std::string failureReason;
    bool isFrozen{false};

    // Private lifecycle and pricing mutations accessible only to OrderManager
    friend class OrderManager;

    void freeze() noexcept;
    bool setStatus(OrderStatus newStatus) noexcept;
    void setFailureReason(const std::string& reason);
    void setItemPriceSnapshot(std::size_t index, double price);
    void setTotalPrice(double total) noexcept;
    void setFreightCost(double cost) noexcept;

public:
    Order();
    Order(std::string orderId,
          std::string customerId,
          OrderPriority priority = OrderPriority::Normal);

    /// Add a line item to the order before submission.
    /// @return true if added, false if productId is empty, quantity <= 0, or order is frozen.
    bool addItem(const std::string& productId, int quantity);

    /// Sets explicit shipping distance (km) and weight (kg).
    /// Rejects non-finite, negative, or out-of-bound values without mutating existing configuration.
    /// @return true if parameters were valid and updated; false otherwise.
    bool setShippingParameters(double distance, double weight);

    const std::string& getOrderId() const noexcept;
    const std::string& getCustomerId() const noexcept;
    OrderPriority getPriority() const noexcept;
    OrderStatus getStatus() const noexcept;
    const std::vector<OrderItem>& getItems() const noexcept;
    double getTotalPrice() const noexcept;
    double getShippingDistance() const noexcept;
    double getShippingWeight() const noexcept;
    double getFreightCost() const noexcept;
    double getGrandTotal() const noexcept;
    const std::string& getFailureReason() const noexcept;
    bool isSubmitted() const noexcept;
};
