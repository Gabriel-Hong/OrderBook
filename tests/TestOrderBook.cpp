#include <gtest/gtest.h>
#include "OrderBook.h"

using namespace orderbook;

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook book;
};

TEST_F(OrderBookTest, AddLimitOrderToBids) {
    auto result = book.addOrder(Side::Buy, OrderType::Limit, 10000, 100);
    EXPECT_EQ(result.filledQuantity, 0);
    EXPECT_EQ(result.remainingQuantity, 100);
    EXPECT_EQ(book.bidLevelCount(), 1);
    EXPECT_EQ(book.askLevelCount(), 0);

    auto bids = book.getBids(10);
    ASSERT_EQ(bids.size(), 1);
    EXPECT_EQ(bids[0].price, 10000);
    EXPECT_EQ(bids[0].totalQuantity, 100);
    EXPECT_EQ(bids[0].orderCount, 1);
}

TEST_F(OrderBookTest, AddLimitOrderToAsks) {
    auto result = book.addOrder(Side::Sell, OrderType::Limit, 10100, 50);
    EXPECT_EQ(result.filledQuantity, 0);
    EXPECT_EQ(result.remainingQuantity, 50);
    EXPECT_EQ(book.askLevelCount(), 1);

    auto asks = book.getAsks(10);
    ASSERT_EQ(asks.size(), 1);
    EXPECT_EQ(asks[0].price, 10100);
    EXPECT_EQ(asks[0].totalQuantity, 50);
}

TEST_F(OrderBookTest, MultipleLevelsOrdered) {
    book.addOrder(Side::Buy, OrderType::Limit, 10000, 100);
    book.addOrder(Side::Buy, OrderType::Limit, 10050, 200);
    book.addOrder(Side::Buy, OrderType::Limit,  9900,  50);

    auto bids = book.getBids(10);
    ASSERT_EQ(bids.size(), 3);
    // Highest price first
    EXPECT_EQ(bids[0].price, 10050);
    EXPECT_EQ(bids[1].price, 10000);
    EXPECT_EQ(bids[2].price,  9900);
}

TEST_F(OrderBookTest, PriceTimePriorityMatching) {
    // Two sell orders at same price — first one should fill first
    book.addOrder(Side::Sell, OrderType::Limit, 10000, 100);  // maker 1
    book.addOrder(Side::Sell, OrderType::Limit, 10000, 100);  // maker 2

    auto result = book.addOrder(Side::Buy, OrderType::Limit, 10000, 150);
    EXPECT_EQ(result.filledQuantity, 150);
    ASSERT_EQ(result.fills.size(), 2);
    // First fill: full 100 from maker 1
    EXPECT_EQ(result.fills[0].quantity, 100);
    // Second fill: 50 from maker 2
    EXPECT_EQ(result.fills[1].quantity, 50);

    // 50 remaining from maker 2 should still be on book
    auto asks = book.getAsks(10);
    ASSERT_EQ(asks.size(), 1);
    EXPECT_EQ(asks[0].totalQuantity, 50);
}

TEST_F(OrderBookTest, LimitOrderFullMatch) {
    book.addOrder(Side::Sell, OrderType::Limit, 10000, 100);
    auto result = book.addOrder(Side::Buy, OrderType::Limit, 10000, 100);

    EXPECT_EQ(result.filledQuantity, 100);
    EXPECT_EQ(result.remainingQuantity, 0);
    EXPECT_EQ(book.askLevelCount(), 0);
    EXPECT_EQ(book.bidLevelCount(), 0);
}

TEST_F(OrderBookTest, LimitOrderNoMatchPriceGap) {
    book.addOrder(Side::Sell, OrderType::Limit, 10100, 100);
    auto result = book.addOrder(Side::Buy, OrderType::Limit, 10000, 100);

    EXPECT_EQ(result.filledQuantity, 0);
    EXPECT_EQ(result.remainingQuantity, 100);
    // Both orders should rest on book
    EXPECT_EQ(book.bidLevelCount(), 1);
    EXPECT_EQ(book.askLevelCount(), 1);
}

TEST_F(OrderBookTest, MarketOrderBuy) {
    book.addOrder(Side::Sell, OrderType::Limit, 10000, 50);
    book.addOrder(Side::Sell, OrderType::Limit, 10100, 50);

    auto result = book.addOrder(Side::Buy, OrderType::Market, 0, 80);
    EXPECT_EQ(result.filledQuantity, 80);
    EXPECT_EQ(result.remainingQuantity, 0);
    ASSERT_EQ(result.fills.size(), 2);
    // Fills at best ask first
    EXPECT_EQ(result.fills[0].price, 10000);
    EXPECT_EQ(result.fills[0].quantity, 50);
    EXPECT_EQ(result.fills[1].price, 10100);
    EXPECT_EQ(result.fills[1].quantity, 30);

    // 20 remaining at 10100
    auto asks = book.getAsks(10);
    ASSERT_EQ(asks.size(), 1);
    EXPECT_EQ(asks[0].price, 10100);
    EXPECT_EQ(asks[0].totalQuantity, 20);
}

TEST_F(OrderBookTest, MarketOrderSell) {
    book.addOrder(Side::Buy, OrderType::Limit, 10050, 60);
    book.addOrder(Side::Buy, OrderType::Limit, 10000, 40);

    auto result = book.addOrder(Side::Sell, OrderType::Market, 0, 80);
    EXPECT_EQ(result.filledQuantity, 80);
    ASSERT_EQ(result.fills.size(), 2);
    // Fills at best bid first (highest)
    EXPECT_EQ(result.fills[0].price, 10050);
    EXPECT_EQ(result.fills[0].quantity, 60);
    EXPECT_EQ(result.fills[1].price, 10000);
    EXPECT_EQ(result.fills[1].quantity, 20);
}

TEST_F(OrderBookTest, MarketOrderOnEmptyBook) {
    auto result = book.addOrder(Side::Buy, OrderType::Market, 0, 100);
    EXPECT_EQ(result.filledQuantity, 0);
    EXPECT_EQ(result.remainingQuantity, 100);
    // Market order should NOT rest on book
    EXPECT_EQ(book.orderCount(), 0);
}

TEST_F(OrderBookTest, CancelOrder) {
    auto r1 = book.addOrder(Side::Buy, OrderType::Limit, 10000, 100);
    auto r2 = book.addOrder(Side::Buy, OrderType::Limit, 10000, 200);
    EXPECT_EQ(book.orderCount(), 2);

    EXPECT_TRUE(book.cancelOrder(r1.orderId));
    EXPECT_EQ(book.orderCount(), 1);

    auto bids = book.getBids(10);
    ASSERT_EQ(bids.size(), 1);
    EXPECT_EQ(bids[0].totalQuantity, 200);

    // Cancel again should fail
    EXPECT_FALSE(book.cancelOrder(r1.orderId));
}

TEST_F(OrderBookTest, CancelRemovesEmptyLevel) {
    auto r1 = book.addOrder(Side::Sell, OrderType::Limit, 10000, 100);
    EXPECT_EQ(book.askLevelCount(), 1);

    book.cancelOrder(r1.orderId);
    EXPECT_EQ(book.askLevelCount(), 0);
}

TEST_F(OrderBookTest, CancelNonexistentOrder) {
    EXPECT_FALSE(book.cancelOrder(99999));
}

TEST_F(OrderBookTest, PartialFillLimitOrder) {
    book.addOrder(Side::Sell, OrderType::Limit, 10000, 30);
    auto result = book.addOrder(Side::Buy, OrderType::Limit, 10000, 100);

    EXPECT_EQ(result.filledQuantity, 30);
    EXPECT_EQ(result.remainingQuantity, 70);
    // Remaining 70 rests on book as bid
    EXPECT_EQ(book.bidLevelCount(), 1);
    EXPECT_EQ(book.askLevelCount(), 0);

    auto bids = book.getBids(10);
    EXPECT_EQ(bids[0].totalQuantity, 70);
}

TEST_F(OrderBookTest, MatchAcrossMultipleLevels) {
    book.addOrder(Side::Sell, OrderType::Limit, 10000, 50);
    book.addOrder(Side::Sell, OrderType::Limit, 10100, 50);
    book.addOrder(Side::Sell, OrderType::Limit, 10200, 50);

    auto result = book.addOrder(Side::Buy, OrderType::Limit, 10200, 120);
    EXPECT_EQ(result.filledQuantity, 120);
    ASSERT_EQ(result.fills.size(), 3);
    EXPECT_EQ(result.fills[0].price, 10000);
    EXPECT_EQ(result.fills[1].price, 10100);
    EXPECT_EQ(result.fills[2].price, 10200);
    EXPECT_EQ(result.fills[2].quantity, 20);

    // 30 left at 10200
    auto asks = book.getAsks(10);
    ASSERT_EQ(asks.size(), 1);
    EXPECT_EQ(asks[0].totalQuantity, 30);
}

TEST_F(OrderBookTest, DepthLimitsOutput) {
    for (int i = 0; i < 20; ++i) {
        book.addOrder(Side::Buy, OrderType::Limit, 10000 - i * 100, 10);
    }
    auto bids = book.getBids(5);
    EXPECT_EQ(bids.size(), 5);
    EXPECT_EQ(bids[0].price, 10000);  // best bid
}

TEST_F(OrderBookTest, MarketOrderExceedsLiquidity) {
    book.addOrder(Side::Sell, OrderType::Limit, 10000, 50);
    auto result = book.addOrder(Side::Buy, OrderType::Market, 0, 200);
    EXPECT_EQ(result.filledQuantity, 50);
    EXPECT_EQ(result.remainingQuantity, 150);
    // Unfilled market order quantity is discarded
    EXPECT_EQ(book.orderCount(), 0);
}
