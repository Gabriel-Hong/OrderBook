# Order Book Engine

High-performance Limit Order Book engine built in C++20 with price-time priority matching.

## Performance

Measured on Windows 11, MSVC 19.44, Release + LTO (500,000 orders)

### Before / After

| Operation | Before (median) | After (median) | Improvement |
|-----------|-----------------|----------------|-------------|
| Limit Order Insert | 500 ns | **200 ns** | **2.5x** |
| Cancel Order | 1,000 ns | **200 ns** | **5.0x** |
| Market Order (w/ matching) | 200 ns | **100 ns** | **2.0x** |
| Throughput | 1.38M orders/sec | **4.87M orders/sec** | **3.5x** |

### Latency Breakdown (After)

| Operation | Mean (ns) | Median (ns) | P99 (ns) | Min (ns) | Max (ns) |
|-----------|-----------|-------------|----------|----------|----------|
| Limit Order Insert | 201 | 200 | 500 | 100 | 150,900 |
| Cancel Order | 232 | 200 | 700 | 0 | 212,500 |
| Market Order (w/ matching) | 225 | 100 | 900 | 100 | 61,700 |

## Key Optimizations

| # | Technique | Detail | Effect |
|---|-----------|--------|--------|
| 1 | `fills.reserve(16)` | Pre-allocate match result vector | Eliminate dynamic allocation on matching hot path |
| 2 | Intrusive Linked List + Object Pool | `std::deque` &rarr; prev/next embedded list, heap alloc &rarr; pre-allocated pool | Cancel O(N) &rarr; O(1), alloc ~100 ns &rarr; ~1 ns |
| 3 | Flat Array Price Levels | `std::map` &rarr; `std::vector` direct indexing | Price lookup O(log N) &rarr; O(1), eliminate cache misses |
| 4 | Flat Vector Order Lookup | `std::unordered_map` &rarr; `std::vector<Order*>` | Remove hash computation, O(1) direct indexing |
| 5 | Benchmark Warm-up + CPU Pinning | Stabilize cache before measurement, `SetThreadAffinityMask` | Max latency 185x reduction (27.9 ms &rarr; 150 us) |
| 6 | LTO (Link-Time Optimization) | `CMAKE_INTERPROCEDURAL_OPTIMIZATION` | Cross-TU inlining, ~5-15% overall improvement |
| 7 | `[[likely]]` / `[[unlikely]]` | C++20 branch prediction hints | Improve branch prediction in matching loop |

## Architecture

| Data Structure | Purpose | Time Complexity |
|----------------|---------|-----------------|
| Flat array (`std::vector<PriceLevelList>`) | Bid/Ask price levels | O(1) price access |
| Intrusive doubly-linked list | Order queue per price level (FIFO) | O(1) insert/remove |
| Object Pool (`OrderPool`) | Pre-allocated Order objects | O(1) alloc/dealloc, zero heap allocation |
| Flat vector (`std::vector<Order*>`) | OrderId &rarr; Order pointer direct indexing | O(1) lookup |

## Features

- **Limit Order** &mdash; Place buy/sell orders at a specified price; unmatched quantity rests in the book
- **Market Order** &mdash; Immediate execution against best available price; unfilled remainder is discarded
- **Cancel Order** &mdash; O(1) lookup and O(1) removal via intrusive linked list
- **Price-Time Priority** &mdash; Best price matched first; ties broken by earliest arrival time

## Build & Run

Requires CMake 3.14+, C++20 compiler (MSVC / GCC / Clang).

```bash
cmake -B build
cmake --build build --config Release

# Demo
./build/Release/orderbook

# Unit tests (Google Test)
./build/Release/tests

# Benchmark
./build/Release/benchmark
```

## Tests

16 unit tests covering:
- Limit order insertion and book state verification
- Price-time priority matching
- Market buy/sell execution
- Market order on empty book
- Cancel order (success, failure, empty price level cleanup)
- Partial fills
- Multi-level matching across price levels
- Depth-limited book display
- Market order exceeding available liquidity

## Project Structure

```
src/
  Types.h          - Common type definitions (Price, Quantity, OrderId, Side, OrderType, etc.)
  Order.h          - Order struct (with prev/next for intrusive list)
  OrderBook.h      - OrderBook, OrderPool, PriceLevelList class declarations
  OrderBook.cpp    - OrderBook implementation (insert, cancel, matching)
  main.cpp         - Demo entry point
bench/
  Benchmark.cpp    - Latency and throughput benchmarks
tests/
  TestOrderBook.cpp - Google Test unit tests (16 cases)
```
