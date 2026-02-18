#pragma once

#include "Order.h"

#include <map>
#include <deque>
#include <unordered_map>
#include <functional>

namespace orderbook {

struct OrderLocation {
    Side   side;
    Price  price;
};

class OrderBook {
public:
    OrderResult addOrder(Side side, OrderType type, Price price, Quantity quantity);
    bool cancelOrder(OrderId id);

    std::vector<PriceLevel> getBids(size_t depth = 10) const;
    std::vector<PriceLevel> getAsks(size_t depth = 10) const;

    size_t bidLevelCount() const { return bids_.size(); }
    size_t askLevelCount() const { return asks_.size(); }
    size_t orderCount()    const { return orders_.size(); }

private:
    void matchOrder(Order& order, OrderResult& result);

    // Buy side: highest price first
    std::map<Price, std::deque<Order>, std::greater<Price>> bids_;
    // Sell side: lowest price first
    std::map<Price, std::deque<Order>>                      asks_;
    // O(1) order lookup for cancel
    std::unordered_map<OrderId, OrderLocation>              orders_;

    OrderId nextId_{1};
};

} // namespace orderbook
