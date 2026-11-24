#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <ranges>
#include <span>
#include <vector>
#include <venue_core/types.hpp>

#include "oracle.hpp"
#include "venue_core/messages.hpp"

using namespace venue_core;

namespace {

// A helper that compares the events with the expected events one by
//       one, and names the index of the first difference (std::views::zip).
int check_events(const std::span<const VenueEventEnvelope>& actual,
                 const std::span<const VenueEventEnvelope>& expected) {
    const auto events = std::views::zip(actual, expected);
    const auto size = events.size();
    int pos = 0;
    for (const auto& [actual_event, expected_event] : events) {
        if (actual_event == expected_event) {
            ++pos;
            continue;
        }
        return pos;
    }

    return (actual.size() > size or expected.size() > size) ? pos : -1;
}

// The expected event with venue sequence `sequence` on one channel.
VenueEventEnvelope private_event(std::uint64_t sequence,
                                 const VenueEvent& payload) {
    return {.sequence = VenueSequence{sequence},
            .channel = EventChannel::PrivateOrder,
            .payload = payload};
}

VenueEventEnvelope public_event(std::uint64_t sequence,
                                const VenueEvent& payload) {
    return {.sequence = VenueSequence{sequence},
            .channel = EventChannel::PublicMarket,
            .payload = payload};
}

// One SubmitLimit line of a golden trace. The strong types keep the
// arguments in order: an ID cannot go where a price goes.
SubmitLimit submit(CommandSequence sequence, OrderId id, Side side, Price price,
                   Quantity quantity, TimeInForce tif = TimeInForce::Gtc) {
    return {.command_sequence = sequence,
            .order_id = id,
            .side = side,
            .limit_price = price,
            .quantity = quantity,
            .tif = tif};
}

}  // namespace

TEST_CASE("oracle: a submit on an empty book rests and is announced",
          "[oracle]") {
    // SubmitLimit{seq 1, id 10, Buy, 100, 5, Gtc} on an empty book.
    // Expect: Accepted; #1 PRV OrderAccepted{10, Buy, 100, 5, Gtc};
    //         #2 PUB PublicAddOrder{10, Buy, 100, 5}; 1 live order.
    testing::Oracle oracle;

    const ProcessResult result = oracle.process(SubmitLimit{
        .command_sequence = CommandSequence{1},
        .order_id = OrderId{10},
        .side = Side::Buy,
        .limit_price = Price{100},
        .quantity = Quantity{5},
        .tif = TimeInForce::Gtc,
    });

    const std::vector expected{
        private_event(1, OrderAccepted{.order_id = OrderId{10},
                                       .side = Side::Buy,
                                       .price = Price{100},
                                       .quantity = Quantity{5},
                                       .tif = TimeInForce::Gtc}),
        public_event(2, PublicAddOrder{.order_id = OrderId{10},
                                       .side = Side::Buy,
                                       .price = Price{100},
                                       .quantity = Quantity{5}}),
    };
    CHECK(result.disposition == Disposition::Accepted);
    CHECK(check_events(result.events, expected) == -1);
    CHECK(oracle.live_orders() == 1);
}

TEST_CASE("oracle: a reject still uses its command sequence", "[oracle]") {
    // SPEC §3, readiness Q7.
    // seq 1: SubmitLimit id 3, Buy, 100, 5, Gtc. It rests.
    // seq 7: SubmitLimit id 3 again.
    // seq 7: Cancel id 3 (a gateway bug: the number is used again).
    // seq 8: Cancel id 3.
    // The last command proves that order 3 was still live.
    testing::Oracle oracle;  // IdPolicy::SessionUnique, the default
    SubmitLimit submit{
        .command_sequence = CommandSequence{1},
        .order_id = OrderId{3},
        .side = Side::Buy,
        .limit_price = Price{100},
        .quantity = Quantity{5},
        .tif = TimeInForce::Gtc,
    };

    // Each process() call ends the life of the last result's events, so
    // one variable holds the current result only.
    ProcessResult result = oracle.process(submit);
    REQUIRE(result.disposition == Disposition::Accepted);  // events #1, #2

    // seq 7: in order, but ID 3 is used. The reject moves the last command
    // sequence to 7 (SPEC §3).
    submit.command_sequence = CommandSequence{7};
    result = oracle.process(submit);
    CHECK(result.disposition == Disposition::Rejected);
    CHECK(check_events(result.events,
                       std::vector{private_event(
                           3, OrderRejected{
                                  .command_sequence = CommandSequence{7},
                                  .order_id = OrderId{3},
                                  .reason = RejectReason::DuplicateOrderId,
                              })}) == -1);

    // seq 7 again: 7 <= 7, so check #1 fails before the ID is looked at.
    result = oracle.process(
        Cancel{.command_sequence = CommandSequence{7}, .order_id = OrderId{3}});
    CHECK(result.disposition == Disposition::Rejected);
    CHECK(check_events(
              result.events,
              std::vector{private_event(
                  4, CancelRejected{
                         .command_sequence = CommandSequence{7},
                         .order_id = OrderId{3},
                         .reason = RejectReason::NonMonotonicCommandSequence,
                     })}) == -1);
    CHECK(oracle.live_orders() == 1);  // a reject changes no book state

    // seq 8: order 3 is still live, with leaves 5.
    result = oracle.process(
        Cancel{.command_sequence = CommandSequence{8}, .order_id = OrderId{3}});
    const std::vector expected{
        private_event(5, OrderCanceled{.order_id = OrderId{3},
                                       .canceled_quantity = Quantity{5},
                                       .reason = CancelReason::User}),
        public_event(6, PublicOrderDeleted{.order_id = OrderId{3}}),
    };
    CHECK(result.disposition == Disposition::Accepted);
    CHECK(check_events(result.events, expected) == -1);
    CHECK(oracle.live_orders() == 0);

    // seq 8 again: the accepted Cancel moved the last command sequence to 8.
    submit.command_sequence = CommandSequence{8};
    submit.order_id = OrderId{4};
    result = oracle.process(submit);
    CHECK(result.disposition == Disposition::Rejected);
    CHECK(check_events(
              result.events,
              std::vector{private_event(
                  7, OrderRejected{
                         .command_sequence = CommandSequence{8},
                         .order_id = OrderId{4},
                         .reason = RejectReason::NonMonotonicCommandSequence,
                     })}) == -1);
    CHECK(oracle.live_orders() == 0);
}

TEST_CASE("oracle: golden trace 1, a sweep at maker prices",
          "[oracle][golden]") {
    testing::Oracle oracle;

    // seq 1: Sell 3 @ 101 GTC. Events #1, #2.
    ProcessResult result = oracle.process(submit(
        CommandSequence{1}, OrderId{1}, Side::Sell, Price{101}, Quantity{3}));
    CHECK(result.disposition == Disposition::Accepted);
    CHECK(check_events(
              result.events,
              std::vector{
                  private_event(
                      1, OrderAccepted{.order_id = OrderId{1},
                                       .side = venue_core::Side::Sell,
                                       .price = Price{101},
                                       .quantity = Quantity{3},
                                       .tif = venue_core::TimeInForce::Gtc}),
                  public_event(2, PublicAddOrder{.order_id = OrderId{1},
                                                 .side = venue_core::Side::Sell,
                                                 .price = Price{101},
                                                 .quantity = Quantity{3}})}) ==
          -1);

    // seq 2: Sell 2 @ 101 GTC. Events #3, #4.
    result = oracle.process(submit(CommandSequence{2}, OrderId{2}, Side::Sell,
                                   Price{101}, Quantity{2}));
    CHECK(result.disposition == Disposition::Accepted);
    CHECK(check_events(
              result.events,
              std::vector{
                  private_event(
                      3, OrderAccepted{.order_id = OrderId{2},
                                       .side = venue_core::Side::Sell,
                                       .price = Price{101},
                                       .quantity = Quantity{2},
                                       .tif = venue_core::TimeInForce::Gtc}),
                  public_event(4, PublicAddOrder{.order_id = OrderId{2},
                                                 .side = venue_core::Side::Sell,
                                                 .price = Price{101},
                                                 .quantity = Quantity{2}})}) ==
          -1);

    // seq 3: Sell 4 @ 103 GTC. Events #5, #6.
    result = oracle.process(submit(CommandSequence{3}, OrderId{3}, Side::Sell,
                                   Price{103}, Quantity{4}));
    CHECK(result.disposition == Disposition::Accepted);
    CHECK(check_events(
              result.events,
              std::vector{
                  private_event(
                      5, OrderAccepted{.order_id = OrderId{3},
                                       .side = venue_core::Side::Sell,
                                       .price = Price{103},
                                       .quantity = Quantity{4},
                                       .tif = venue_core::TimeInForce::Gtc}),
                  public_event(6, PublicAddOrder{.order_id = OrderId{3},
                                                 .side = venue_core::Side::Sell,
                                                 .price = Price{103},
                                                 .quantity = Quantity{4}})}) ==
          -1);

    // seq 4: Buy 8 @ 104 GTC, the sweep. Events #7 to #19.
    result = oracle.process(submit(CommandSequence{4}, OrderId{4}, Side::Buy,
                                   Price{104}, Quantity{8}));
    CHECK(result.disposition == Disposition::Accepted);
    const std::vector seq4{
        private_event(7, OrderAccepted{.order_id = OrderId{4},
                                       .side = Side::Buy,
                                       .price = Price{104},
                                       .quantity = Quantity{8},
                                       .tif = TimeInForce::Gtc}),
        // Fill 1: order 1, 3 @ 101, match 1.
        private_event(8, OrderExecuted{.order_id = OrderId{1},
                                       .match_id = MatchId{1},
                                       .price = Price{101},
                                       .quantity = Quantity{3},
                                       .leaves = Quantity{0},
                                       .liquidity = Liquidity::Maker}),
        private_event(9, OrderExecuted{.order_id = OrderId{4},
                                       .match_id = MatchId{1},
                                       .price = Price{101},
                                       .quantity = Quantity{3},
                                       .leaves = Quantity{5},
                                       .liquidity = Liquidity::Taker}),
        public_event(10, PublicOrderExecuted{.order_id = OrderId{1},
                                             .match_id = MatchId{1},
                                             .quantity = Quantity{3}}),
        public_event(11, PublicTrade{.match_id = MatchId{1},
                                     .price = Price{101},
                                     .quantity = Quantity{3},
                                     .aggressor = Side::Buy}),
        // Fill 2: order 2, 2 @ 101, match 2.
        private_event(12, OrderExecuted{.order_id = OrderId{2},
                                        .match_id = MatchId{2},
                                        .price = Price{101},
                                        .quantity = Quantity{2},
                                        .leaves = Quantity{0},
                                        .liquidity = Liquidity::Maker}),
        private_event(13, OrderExecuted{.order_id = OrderId{4},
                                        .match_id = MatchId{2},
                                        .price = Price{101},
                                        .quantity = Quantity{2},
                                        .leaves = Quantity{3},
                                        .liquidity = Liquidity::Taker}),
        public_event(14, PublicOrderExecuted{.order_id = OrderId{2},
                                             .match_id = MatchId{2},
                                             .quantity = Quantity{2}}),
        public_event(15, PublicTrade{.match_id = MatchId{2},
                                     .price = Price{101},
                                     .quantity = Quantity{2},
                                     .aggressor = Side::Buy}),
        // Fill 3: order 3, 3 @ 103, match 3. Order 3 keeps 1.
        private_event(16, OrderExecuted{.order_id = OrderId{3},
                                        .match_id = MatchId{3},
                                        .price = Price{103},
                                        .quantity = Quantity{3},
                                        .leaves = Quantity{1},
                                        .liquidity = Liquidity::Maker}),
        private_event(17, OrderExecuted{.order_id = OrderId{4},
                                        .match_id = MatchId{3},
                                        .price = Price{103},
                                        .quantity = Quantity{3},
                                        .leaves = Quantity{0},
                                        .liquidity = Liquidity::Taker}),
        public_event(18, PublicOrderExecuted{.order_id = OrderId{3},
                                             .match_id = MatchId{3},
                                             .quantity = Quantity{3}}),
        public_event(19, PublicTrade{.match_id = MatchId{3},
                                     .price = Price{103},
                                     .quantity = Quantity{3},
                                     .aggressor = Side::Buy}),
        // Order 4 is filled: no remainder, never public (D2).
    };
    CHECK(check_events(result.events, seq4) == -1);

    // seq 5: Buy 3 @ 103 GTC. Events #20 to #25.
    result = oracle.process(submit(CommandSequence{5}, OrderId{5}, Side::Buy,
                                   Price{103}, Quantity{3}));
    CHECK(result.disposition == Disposition::Accepted);
    const std::vector seq5{
        private_event(20, OrderAccepted{.order_id = OrderId{5},
                                        .side = Side::Buy,
                                        .price = Price{103},
                                        .quantity = Quantity{3},
                                        .tif = TimeInForce::Gtc}),
        // Fill 1: order 3 (1 left), 1 @ 103, match 4.
        private_event(21, OrderExecuted{.order_id = OrderId{3},
                                        .match_id = MatchId{4},
                                        .price = Price{103},
                                        .quantity = Quantity{1},
                                        .leaves = Quantity{0},
                                        .liquidity = Liquidity::Maker}),
        private_event(22, OrderExecuted{.order_id = OrderId{5},
                                        .match_id = MatchId{4},
                                        .price = Price{103},
                                        .quantity = Quantity{1},
                                        .leaves = Quantity{2},
                                        .liquidity = Liquidity::Taker}),
        public_event(23, PublicOrderExecuted{.order_id = OrderId{3},
                                             .match_id = MatchId{4},
                                             .quantity = Quantity{1}}),
        public_event(24, PublicTrade{.match_id = MatchId{4},
                                     .price = Price{103},
                                     .quantity = Quantity{1},
                                     .aggressor = Side::Buy}),
        // The GTC remainder rests at 103 with its leaves.
        public_event(25, PublicAddOrder{.order_id = OrderId{5},
                                        .side = Side::Buy,
                                        .price = Price{103},
                                        .quantity = Quantity{2}}),
    };
    CHECK(check_events(result.events, seq5) == -1);

    // Book after seq 5: order 5 only.
    CHECK(oracle.live_orders() == 1);
}
