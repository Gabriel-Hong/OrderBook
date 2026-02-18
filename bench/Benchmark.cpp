#include "OrderBook.h"

#include <chrono>
#include <iostream>
#include <iomanip>
#include <random>
#include <vector>
#include <numeric>
#include <algorithm>

using namespace orderbook;
using Clock = std::chrono::high_resolution_clock;

struct Stats {
    double mean;
    double median;
    double p99;
    double min;
    double max;
};

Stats computeStats(std::vector<double>& samples) {
    std::sort(samples.begin(), samples.end());
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    size_t n = samples.size();
    return {
        sum / n,
        samples[n / 2],
        samples[static_cast<size_t>(n * 0.99)],
        samples.front(),
        samples.back()
    };
}

void printStats(const char* label, Stats s) {
    std::cout << std::left << std::setw(28) << label
              << std::right
              << std::setw(10) << std::fixed << std::setprecision(0) << s.mean
              << std::setw(10) << s.median
              << std::setw(10) << s.p99
              << std::setw(10) << s.min
              << std::setw(10) << s.max
              << "\n";
}

int main() {
    constexpr int NUM_ORDERS = 500'000;
    constexpr int NUM_LEVELS = 1000;

    std::mt19937 rng(42);
    std::uniform_int_distribution<Price> priceDist(9000, 11000);
    std::uniform_int_distribution<Quantity> qtyDist(1, 100);

    std::cout << "=== OrderBook Benchmark ===\n";
    std::cout << "Orders: " << NUM_ORDERS << "\n\n";

    // --- Benchmark 1: Add Limit Order ---
    {
        OrderBook book;
        std::vector<double> latencies;
        latencies.reserve(NUM_ORDERS);

        for (int i = 0; i < NUM_ORDERS; ++i) {
            Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
            Price price = priceDist(rng);
            // Spread orders to avoid excessive matching during add benchmark
            if (side == Side::Buy) price -= 500;
            else price += 500;
            Quantity qty = qtyDist(rng);

            auto start = Clock::now();
            book.addOrder(side, OrderType::Limit, price, qty);
            auto end = Clock::now();

            latencies.push_back(static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()));
        }

        std::cout << std::left << std::setw(28) << "Operation"
                  << std::right
                  << std::setw(10) << "Mean(ns)"
                  << std::setw(10) << "Med(ns)"
                  << std::setw(10) << "P99(ns)"
                  << std::setw(10) << "Min(ns)"
                  << std::setw(10) << "Max(ns)"
                  << "\n";
        std::cout << std::string(78, '-') << "\n";

        auto stats = computeStats(latencies);
        printStats("Add Limit Order", stats);
    }

    // --- Benchmark 2: Cancel Order ---
    {
        OrderBook book;
        std::vector<OrderId> ids;
        ids.reserve(NUM_ORDERS);

        for (int i = 0; i < NUM_ORDERS; ++i) {
            Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
            Price price = priceDist(rng);
            if (side == Side::Buy) price -= 500;
            else price += 500;
            auto r = book.addOrder(side, OrderType::Limit, price, qtyDist(rng));
            ids.push_back(r.orderId);
        }

        // Shuffle to cancel in random order
        std::shuffle(ids.begin(), ids.end(), rng);

        std::vector<double> latencies;
        latencies.reserve(NUM_ORDERS);

        for (auto id : ids) {
            auto start = Clock::now();
            book.cancelOrder(id);
            auto end = Clock::now();

            latencies.push_back(static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()));
        }

        auto stats = computeStats(latencies);
        printStats("Cancel Order", stats);
    }

    // --- Benchmark 3: Market Order (matching) ---
    {
        constexpr int NUM_MARKET = 100'000;
        OrderBook book;

        // Pre-populate with limit orders on both sides
        for (int i = 0; i < NUM_LEVELS; ++i) {
            Price askPrice = 10001 + i;
            Price bidPrice = 10000 - i;
            for (int j = 0; j < 10; ++j) {
                book.addOrder(Side::Sell, OrderType::Limit, askPrice, 100);
                book.addOrder(Side::Buy,  OrderType::Limit, bidPrice, 100);
            }
        }

        std::vector<double> latencies;
        latencies.reserve(NUM_MARKET);

        for (int i = 0; i < NUM_MARKET; ++i) {
            // Replenish liquidity periodically
            if (i % 100 == 0) {
                for (int j = 0; j < 10; ++j) {
                    Price p = priceDist(rng);
                    book.addOrder(Side::Sell, OrderType::Limit, p + 500, 100);
                    book.addOrder(Side::Buy,  OrderType::Limit, p - 500, 100);
                }
            }

            Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
            Quantity qty = qtyDist(rng);

            auto start = Clock::now();
            book.addOrder(side, OrderType::Market, 0, qty);
            auto end = Clock::now();

            latencies.push_back(static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()));
        }

        auto stats = computeStats(latencies);
        printStats("Market Order (w/ matching)", stats);
    }

    // --- Benchmark 4: Throughput ---
    {
        OrderBook book;
        auto start = Clock::now();

        for (int i = 0; i < NUM_ORDERS; ++i) {
            Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
            Price price = priceDist(rng);
            if (side == Side::Buy) price -= 500;
            else price += 500;
            book.addOrder(side, OrderType::Limit, price, qtyDist(rng));
        }

        auto end = Clock::now();
        double elapsed = std::chrono::duration<double>(end - start).count();
        double throughput = NUM_ORDERS / elapsed;

        std::cout << "\nThroughput: "
                  << std::fixed << std::setprecision(0) << throughput
                  << " orders/sec ("
                  << std::setprecision(3) << elapsed << " sec for "
                  << NUM_ORDERS << " orders)\n";
    }

    return 0;
}
