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
}
