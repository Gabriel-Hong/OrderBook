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
};

} // namespace orderbook
