#include "OrderBook.h"

#include <iostream>
#include <iomanip>

using namespace orderbook;

void printBook(const OrderBook& book) {
    auto asks = book.getAsks(5);
    auto bids = book.getBids(5);

    std::cout << "\n--- Order Book ---\n";
    std::cout << std::left << std::setw(12) << "Price"
              << std::setw(12) << "Quantity"
              << std::setw(10) << "Orders" << "\n";
    std::cout << std::string(34, '-') << "\n";

    // Print asks in reverse (highest first) for visual clarity
    std::cout << "  Asks:\n";
    for (int i = static_cast<int>(asks.size()) - 1; i >= 0; --i) {
        std::cout << "    " << std::setw(10) << std::fixed << std::setprecision(2)
                  << (asks[i].price / 100.0)
                  << std::setw(10) << asks[i].totalQuantity
                  << std::setw(10) << asks[i].orderCount << "\n";
    }

    std::cout << "  ----------\n";

    std::cout << "  Bids:\n";
    for (const auto& lvl : bids) {
        std::cout << "    " << std::setw(10) << std::fixed << std::setprecision(2)
                  << (lvl.price / 100.0)
                  << std::setw(10) << lvl.totalQuantity
                  << std::setw(10) << lvl.orderCount << "\n";
    }
    std::cout << "\n";
}

void printResult(const char* action, const OrderResult& r) {
    std::cout << action << " -> OrderId=" << r.orderId
              << " filled=" << r.filledQuantity
              << " remaining=" << r.remainingQuantity;
    if (!r.fills.empty()) {
        std::cout << " fills=[";
        for (size_t i = 0; i < r.fills.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << r.fills[i].quantity << "@"
                      << std::fixed << std::setprecision(2) << (r.fills[i].price / 100.0);
        }
        std::cout << "]";
    }
    std::cout << "\n";
}

int main() {
    OrderBook book;

    std::cout << "=== Order Book Demo ===\n";

    // Place some sell orders
    printResult("SELL 100@$100.50", book.addOrder(Side::Sell, OrderType::Limit, 10050, 100));
    printResult("SELL  50@$100.00", book.addOrder(Side::Sell, OrderType::Limit, 10000,  50));
    printResult("SELL  75@$101.00", book.addOrder(Side::Sell, OrderType::Limit, 10100,  75));

    // Place some buy orders
    printResult("BUY  100@$99.50 ", book.addOrder(Side::Buy, OrderType::Limit,  9950, 100));
    printResult("BUY   80@$99.00 ", book.addOrder(Side::Buy, OrderType::Limit,  9900,  80));
    printResult("BUY   60@$99.50 ", book.addOrder(Side::Buy, OrderType::Limit,  9950,  60));

    printBook(book);

    // Aggressive buy order that crosses the spread
    std::cout << "--- Crossing the spread ---\n";
    printResult("BUY  120@$100.50", book.addOrder(Side::Buy, OrderType::Limit, 10050, 120));
    printBook(book);

    // Market order
    std::cout << "--- Market sell order ---\n";
    printResult("SELL MKT qty=200", book.addOrder(Side::Sell, OrderType::Market, 0, 200));
    printBook(book);

    // Cancel an order
    std::cout << "--- Cancel order ---\n";
    auto r = book.addOrder(Side::Buy, OrderType::Limit, 9800, 500);
    printResult("BUY  500@$98.00 ", r);
    std::cout << "Cancel OrderId=" << r.orderId << " -> "
              << (book.cancelOrder(r.orderId) ? "success" : "failed") << "\n";
    printBook(book);

    return 0;
}
