#include "OrderBook.h"

#include <algorithm>

namespace orderbook {

OrderBook::OrderBook(size_t poolCapacity)
    : bidLevels_(NUM_PRICE_LEVELS)
    , askLevels_(NUM_PRICE_LEVELS)
    , orders_(poolCapacity + 1, nullptr)
    , pool_(poolCapacity)
{
}

OrderResult OrderBook::addOrder(Side side, OrderType type, Price price, Quantity quantity) {
    Order* order   = pool_.alloc();
    order->id      = nextId_++;
    order->side    = side;
    order->type    = type;
    order->price   = price;
    order->quantity = quantity;
    order->timestamp = std::chrono::steady_clock::now();
    order->prev    = nullptr;
    order->next    = nullptr;

    OrderResult result{};
    result.orderId           = order->id;
    result.filledQuantity    = 0;
    result.remainingQuantity = quantity;

    matchOrder(order, result);

    result.remainingQuantity = order->quantity;

    // Rest remaining quantity on book (limit orders only)
    if (order->quantity > 0 && type == OrderType::Limit) [[likely]] {
        size_t idx = static_cast<size_t>(price - MIN_PRICE);

        if (side == Side::Buy) {
            bool wasEmpty = bidLevels_[idx].empty();
            bidLevels_[idx].pushBack(order);
            if (wasEmpty) ++numBidLevels_;
            if (price > bestBid_) bestBid_ = price;
        } else {
            bool wasEmpty = askLevels_[idx].empty();
            askLevels_[idx].pushBack(order);
            if (wasEmpty) ++numAskLevels_;
            if (price < bestAsk_) bestAsk_ = price;
        }

        // Grow lookup vector if needed
        if (order->id >= static_cast<OrderId>(orders_.size())) [[unlikely]] {
            orders_.resize(static_cast<size_t>(order->id) * 2, nullptr);
        }
        orders_[static_cast<size_t>(order->id)] = order;
        ++numOrders_;
    } else {
        // Fully filled or market order — return to pool
        pool_.dealloc(order);
    }

    return result;
}

bool OrderBook::cancelOrder(OrderId id) {
    auto idx = static_cast<size_t>(id);
    if (idx >= orders_.size() || orders_[idx] == nullptr) [[unlikely]] {
        return false;
    }

    Order* order = orders_[idx];
    size_t levelIdx = static_cast<size_t>(order->price - MIN_PRICE);

    if (order->side == Side::Buy) {
        bidLevels_[levelIdx].remove(order);
        if (bidLevels_[levelIdx].empty()) {
            --numBidLevels_;
            if (order->price == bestBid_) {
                updateBestBidDown();
            }
        }
    } else {
        askLevels_[levelIdx].remove(order);
        if (askLevels_[levelIdx].empty()) {
            --numAskLevels_;
            if (order->price == bestAsk_) {
                updateBestAskUp();
            }
        }
    }

    orders_[idx] = nullptr;
    --numOrders_;
    pool_.dealloc(order);
    return true;
}

std::vector<PriceLevel> OrderBook::getBids(size_t depth) const {
    std::vector<PriceLevel> levels;
    levels.reserve(depth);

    if (bestBid_ < MIN_PRICE) return levels;

    Price p = bestBid_;
    while (p >= MIN_PRICE && levels.size() < depth) {
        const auto& level = bidLevels_[static_cast<size_t>(p - MIN_PRICE)];
        if (!level.empty()) {
            Quantity total = 0;
            for (const Order* o = level.head; o != nullptr; o = o->next) {
                total += o->quantity;
            }
            levels.push_back({p, total, level.count});
        }
        --p;
    }

    return levels;
}

std::vector<PriceLevel> OrderBook::getAsks(size_t depth) const {
    std::vector<PriceLevel> levels;
    levels.reserve(depth);

    if (bestAsk_ > MAX_PRICE) return levels;

    Price p = bestAsk_;
    while (p <= MAX_PRICE && levels.size() < depth) {
        const auto& level = askLevels_[static_cast<size_t>(p - MIN_PRICE)];
        if (!level.empty()) {
            Quantity total = 0;
            for (const Order* o = level.head; o != nullptr; o = o->next) {
                total += o->quantity;
            }
            levels.push_back({p, total, level.count});
        }
        ++p;
    }

    return levels;
}

void OrderBook::matchOrder(Order* order, OrderResult& result) {
    result.fills.reserve(16);  // #1: pre-allocate fills vector

    if (order->side == Side::Buy) {
        // Match against asks (lowest price first)
        while (order->quantity > 0 && bestAsk_ <= MAX_PRICE) {
            if (order->type == OrderType::Limit && order->price < bestAsk_) [[unlikely]] {
                break;
            }

            auto& level = askLevels_[static_cast<size_t>(bestAsk_ - MIN_PRICE)];
            while (order->quantity > 0 && !level.empty()) {
                Order* resting = level.front();
                Quantity fillQty = std::min(order->quantity, resting->quantity);

                Fill fill{};
                fill.makerOrderId = resting->id;
                fill.takerOrderId = order->id;
                fill.price        = resting->price;
                fill.quantity     = fillQty;
                result.fills.push_back(fill);

                order->quantity   -= fillQty;
                resting->quantity -= fillQty;
                result.filledQuantity += fillQty;

                if (resting->quantity == 0) [[unlikely]] {
                    level.remove(resting);
                    orders_[static_cast<size_t>(resting->id)] = nullptr;
                    --numOrders_;
                    pool_.dealloc(resting);
                }
            }

            if (level.empty()) [[unlikely]] {
                --numAskLevels_;
                updateBestAskUp();
            }
        }
    } else {
        // Match against bids (highest price first)
        while (order->quantity > 0 && bestBid_ >= MIN_PRICE) {
            if (order->type == OrderType::Limit && order->price > bestBid_) [[unlikely]] {
                break;
            }

            auto& level = bidLevels_[static_cast<size_t>(bestBid_ - MIN_PRICE)];
            while (order->quantity > 0 && !level.empty()) {
                Order* resting = level.front();
                Quantity fillQty = std::min(order->quantity, resting->quantity);

                Fill fill{};
                fill.makerOrderId = resting->id;
                fill.takerOrderId = order->id;
                fill.price        = resting->price;
                fill.quantity     = fillQty;
                result.fills.push_back(fill);

                order->quantity   -= fillQty;
                resting->quantity -= fillQty;
                result.filledQuantity += fillQty;

                if (resting->quantity == 0) [[unlikely]] {
                    level.remove(resting);
                    orders_[static_cast<size_t>(resting->id)] = nullptr;
                    --numOrders_;
                    pool_.dealloc(resting);
                }
            }

            if (level.empty()) [[unlikely]] {
                --numBidLevels_;
                updateBestBidDown();
            }
        }
    }
}

void OrderBook::updateBestBidDown() {
    --bestBid_;
    while (bestBid_ >= MIN_PRICE &&
           bidLevels_[static_cast<size_t>(bestBid_ - MIN_PRICE)].empty()) {
        --bestBid_;
    }
    // bestBid_ < MIN_PRICE means no bids exist (sentinel)
}

void OrderBook::updateBestAskUp() {
    ++bestAsk_;
    while (bestAsk_ <= MAX_PRICE &&
           askLevels_[static_cast<size_t>(bestAsk_ - MIN_PRICE)].empty()) {
        ++bestAsk_;
    }
    // bestAsk_ > MAX_PRICE means no asks exist (sentinel)
}

} // namespace orderbook
