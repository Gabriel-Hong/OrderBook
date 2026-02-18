#pragma once

#include <cstdint>
#include <chrono>
#include <vector>

namespace orderbook {

using OrderId   = uint64_t;
using Price     = int64_t;   // Fixed-point: actual price * 100 (e.g., 10050 = $100.50)
using Quantity  = uint32_t;
using Timestamp = std::chrono::steady_clock::time_point;

enum class Side : uint8_t {
    Buy,
    Sell
};

enum class OrderType : uint8_t {
    Limit,
    Market
};

struct PriceLevel {
    Price    price;
    Quantity totalQuantity;
    size_t   orderCount;
};

struct Fill {
    OrderId  makerOrderId;
    OrderId  takerOrderId;
    Price    price;
    Quantity quantity;
};

struct OrderResult {
    OrderId            orderId;
    Quantity           filledQuantity;
    Quantity           remainingQuantity;
    std::vector<Fill>  fills;
};

} // namespace orderbook
