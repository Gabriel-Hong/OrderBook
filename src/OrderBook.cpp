#include "OrderBook.h"

#include <algorithm>

namespace orderbook {

OrderResult OrderBook::addOrder(Side side, OrderType type, Price price, Quantity quantity) {
    Order order{};
    order.id        = nextId_++;
    order.side      = side;
    order.type      = type;
    order.price     = price;
    order.quantity   = quantity;
    order.timestamp = std::chrono::steady_clock::now();

    OrderResult result{};
    result.orderId           = order.id;
    result.filledQuantity    = 0;
    result.remainingQuantity = quantity;

    matchOrder(order, result);

    // If there's remaining quantity and it's a limit order, rest on book
    if (order.quantity > 0 && type == OrderType::Limit) {
        OrderLocation loc{side, price};
        if (side == Side::Buy) {
            bids_[price].push_back(order);
        } else {
            asks_[price].push_back(order);
        }
        orders_[order.id] = loc;
    }

    result.remainingQuantity = order.quantity;
    return result;
}

bool OrderBook::cancelOrder(OrderId id) {
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return false;
    }

    const auto& loc = it->second;

    if (loc.side == Side::Buy) {
        auto levelIt = bids_.find(loc.price);
        if (levelIt != bids_.end()) {
            auto& q = levelIt->second;
            q.erase(std::remove_if(q.begin(), q.end(),
                [id](const Order& o) { return o.id == id; }), q.end());
            if (q.empty()) {
                bids_.erase(levelIt);
            }
        }
    } else {
        auto levelIt = asks_.find(loc.price);
        if (levelIt != asks_.end()) {
            auto& q = levelIt->second;
            q.erase(std::remove_if(q.begin(), q.end(),
                [id](const Order& o) { return o.id == id; }), q.end());
            if (q.empty()) {
                asks_.erase(levelIt);
            }
        }
    }

    orders_.erase(it);
    return true;
}

std::vector<PriceLevel> OrderBook::getBids(size_t depth) const {
    std::vector<PriceLevel> levels;
    levels.reserve(depth);
    for (const auto& [price, orders] : bids_) {
        if (levels.size() >= depth) break;
        Quantity total = 0;
        for (const auto& o : orders) {
            total += o.quantity;
        }
        levels.push_back({price, total, orders.size()});
    }
    return levels;
}

std::vector<PriceLevel> OrderBook::getAsks(size_t depth) const {
    std::vector<PriceLevel> levels;
    levels.reserve(depth);
    for (const auto& [price, orders] : asks_) {
        if (levels.size() >= depth) break;
        Quantity total = 0;
        for (const auto& o : orders) {
            total += o.quantity;
        }
        levels.push_back({price, total, orders.size()});
    }
    return levels;
}

void OrderBook::matchOrder(Order& order, OrderResult& result) {
    if (order.side == Side::Buy) {
        // Match against asks (lowest price first)
        while (order.quantity > 0 && !asks_.empty()) {
            auto bestAskIt = asks_.begin();
            // For limit orders, check price compatibility
            if (order.type == OrderType::Limit && order.price < bestAskIt->first) {
                break;
            }

            auto& queue = bestAskIt->second;
            while (order.quantity > 0 && !queue.empty()) {
                auto& resting = queue.front();
                Quantity fillQty = std::min(order.quantity, resting.quantity);

                Fill fill{};
                fill.makerOrderId = resting.id;
                fill.takerOrderId = order.id;
                fill.price        = resting.price;
                fill.quantity     = fillQty;
                result.fills.push_back(fill);

                order.quantity   -= fillQty;
                resting.quantity -= fillQty;
                result.filledQuantity += fillQty;

                if (resting.quantity == 0) {
                    orders_.erase(resting.id);
                    queue.pop_front();
                }
            }

            if (queue.empty()) {
                asks_.erase(bestAskIt);
            }
        }
    } else {
        // Match against bids (highest price first)
        while (order.quantity > 0 && !bids_.empty()) {
            auto bestBidIt = bids_.begin();
            if (order.type == OrderType::Limit && order.price > bestBidIt->first) {
                break;
            }

            auto& queue = bestBidIt->second;
            while (order.quantity > 0 && !queue.empty()) {
                auto& resting = queue.front();
                Quantity fillQty = std::min(order.quantity, resting.quantity);

                Fill fill{};
                fill.makerOrderId = resting.id;
                fill.takerOrderId = order.id;
                fill.price        = resting.price;
                fill.quantity     = fillQty;
                result.fills.push_back(fill);

                order.quantity   -= fillQty;
                resting.quantity -= fillQty;
                result.filledQuantity += fillQty;

                if (resting.quantity == 0) {
                    orders_.erase(resting.id);
                    queue.pop_front();
                }
            }

            if (queue.empty()) {
                bids_.erase(bestBidIt);
            }
        }
    }
}

} // namespace orderbook
