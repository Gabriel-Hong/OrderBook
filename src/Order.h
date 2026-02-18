#pragma once

#include "Types.h"

namespace orderbook {

struct Order {
    OrderId   id;
    Side      side;
    OrderType type;
    Price     price;
    Quantity  quantity;
    Timestamp timestamp;
    Order*    prev = nullptr;
    Order*    next = nullptr;
};

} // namespace orderbook
