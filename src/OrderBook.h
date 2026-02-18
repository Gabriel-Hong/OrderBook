#pragma once

#include "Order.h"

#include <vector>
#include <cassert>

namespace orderbook {

// Price range for flat array indexing
constexpr Price  MIN_PRICE = 0;
constexpr Price  MAX_PRICE = 20000;
constexpr size_t NUM_PRICE_LEVELS = static_cast<size_t>(MAX_PRICE - MIN_PRICE) + 1;

// Intrusive doubly-linked list for orders at a single price level
struct PriceLevelList {
    Order* head = nullptr;
    Order* tail = nullptr;
    size_t count = 0;

    bool empty() const { return head == nullptr; }

    void pushBack(Order* order) {
        order->prev = tail;
        order->next = nullptr;
        if (tail) tail->next = order;
        else head = order;
        tail = order;
        ++count;
    }

    void remove(Order* order) {
        if (order->prev) order->prev->next = order->next;
        else head = order->next;
        if (order->next) order->next->prev = order->prev;
        else tail = order->prev;
        order->prev = nullptr;
        order->next = nullptr;
        --count;
    }

    Order* front() const { return head; }
};

// Pre-allocated object pool for Order objects
class OrderPool {
    std::vector<Order> pool_;
    std::vector<Order*> freeList_;
public:
    explicit OrderPool(size_t capacity) : pool_(capacity) {
        freeList_.reserve(capacity);
        for (size_t i = 0; i < capacity; ++i) {
            freeList_.push_back(&pool_[i]);
        }
    }

    Order* alloc() {
        assert(!freeList_.empty() && "OrderPool exhausted");
        Order* p = freeList_.back();
        freeList_.pop_back();
        return p;
    }

    void dealloc(Order* p) {
        freeList_.push_back(p);
    }

    OrderPool(const OrderPool&) = delete;
    OrderPool& operator=(const OrderPool&) = delete;
    OrderPool(OrderPool&&) = default;
    OrderPool& operator=(OrderPool&&) = default;
};

class OrderBook {
public:
    explicit OrderBook(size_t poolCapacity = 1'048'576);

    OrderResult addOrder(Side side, OrderType type, Price price, Quantity quantity);
    bool cancelOrder(OrderId id);

    std::vector<PriceLevel> getBids(size_t depth = 10) const;
    std::vector<PriceLevel> getAsks(size_t depth = 10) const;

    size_t bidLevelCount() const { return numBidLevels_; }
    size_t askLevelCount() const { return numAskLevels_; }
    size_t orderCount()    const { return numOrders_; }

private:
    void matchOrder(Order* order, OrderResult& result);
    void updateBestBidDown();
    void updateBestAskUp();

    // Flat arrays for price levels (#3: std::map -> flat array)
    std::vector<PriceLevelList> bidLevels_;
    std::vector<PriceLevelList> askLevels_;

    // Best price tracking
    Price bestBid_ = MIN_PRICE - 1;   // sentinel: no bids
    Price bestAsk_ = MAX_PRICE + 1;   // sentinel: no asks

    // O(1) order lookup by OrderId (#4: unordered_map -> flat vector)
    std::vector<Order*> orders_;

    // Object pool (#2: heap allocation -> pre-allocated pool)
    OrderPool pool_;

    // Counters
    size_t numBidLevels_ = 0;
    size_t numAskLevels_ = 0;
    size_t numOrders_ = 0;

    OrderId nextId_ = 1;
};

} // namespace orderbook
