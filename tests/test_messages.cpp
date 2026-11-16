#include <array>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <venue_core/messages.hpp>
#include <venue_core/types.hpp>

using namespace venue_core;

namespace {

// ---- Compile-time contract. A failure here is a build failure.

static_assert(std::variant_size_v<VenueCommand> == 5);
static_assert(std::variant_size_v<VenueEvent> == 16);

// every command, every event, VenueEventEnvelope and ProcessResult are
//       trivially copyable. The engine copies events into a preallocated
//       buffer; a std::string field would add a heap allocation per event.
template <class Variant>
constexpr bool all_trivially_copyable = false;

template <class... Alternatives>
constexpr bool all_trivially_copyable<std::variant<Alternatives...>> =
    (std::is_trivially_copyable_v<Alternatives> && ...);

static_assert(all_trivially_copyable<VenueCommand>);
static_assert(all_trivially_copyable<VenueEvent>);
static_assert(std::is_trivially_copyable_v<VenueCommand>);
static_assert(std::is_trivially_copyable_v<VenueEventEnvelope>);
static_assert(std::is_trivially_copyable_v<ProcessResult>);

static_assert(std::is_same_v<decltype(SubmitLimit::limit_price), Price>);
static_assert(std::is_same_v<decltype(SubmitLimit::quantity), Quantity>);
static_assert(std::is_same_v<decltype(Replace::new_price), Price>);
static_assert(std::is_same_v<decltype(Uncross::reference_price), Price>);
static_assert(std::is_same_v<decltype(OrderExecuted::leaves), Quantity>);
static_assert(
    std::is_same_v<decltype(OrderCanceled::canceled_quantity), Quantity>);
static_assert(
    std::is_same_v<decltype(OrderReduced::removed_quantity), Quantity>);

// A requires-expression outside a template is a hard error, so each check is a
// concept. The positive control proves that the concepts can be true
template <class T>
concept TakesStrongValues =
    requires { T{.limit_price = Price{5}, .quantity = Quantity{7}}; };

template <class T>
concept TakesQuantityAsPrice = requires { T{.limit_price = Quantity{5}}; };

template <class T>
concept TakesPositionalInts = requires { T{1, 2}; };

template <class T>
concept TakesIntAsPrice = requires { T{.limit_price = 5}; };

static_assert(TakesStrongValues<SubmitLimit>);
static_assert(!TakesQuantityAsPrice<SubmitLimit>);
static_assert(!TakesPositionalInts<SubmitLimit>);
static_assert(!TakesIntAsPrice<SubmitLimit>);

}  // namespace

TEST_CASE("messages: commands compare field by field", "[messages]") {
    // A fully filled SubmitLimit equals its copy.
    // Change one field at a time (all 6): each changed copy is not equal.
    const SubmitLimit base{
        .command_sequence = CommandSequence{1},
        .order_id = OrderId{10},
        .side = Side::Sell,
        .limit_price = Price{100},
        .quantity = Quantity{5},
        .tif = TimeInForce::Ioc,
    };
    const SubmitLimit same = base;
    CHECK(same == base);

    // Each copy changes one field.
    const auto with = [&base](auto change) {
        SubmitLimit other = base;
        change(other);
        return other;
    };
    const std::array<std::pair<std::string_view, SubmitLimit>, 6> changed{{
        {"command_sequence",
         with([](SubmitLimit& m) { m.command_sequence = CommandSequence{2}; })},
        {"order_id", with([](SubmitLimit& m) { m.order_id = OrderId{11}; })},
        {"side", with([](SubmitLimit& m) { m.side = Side::Buy; })},
        {"limit_price",
         with([](SubmitLimit& m) { m.limit_price = Price{101}; })},
        {"quantity", with([](SubmitLimit& m) { m.quantity = Quantity{6}; })},
        {"tif", with([](SubmitLimit& m) { m.tif = TimeInForce::Gtc; })},
    }};
    for (const auto& [field, other] : changed) {
        INFO("changed field: " << field);
        CHECK(other != base);
    }

    // An empty command holds zeros. Validation rejects price 0 and quantity 0.
    // A forgotten side means BUY and a forgotten tif means GTC: both are valid.
    const SubmitLimit zeros{
        .command_sequence = CommandSequence{0},
        .order_id = OrderId{0},
        .side = Side::Buy,
        .limit_price = Price{0},
        .quantity = Quantity{0},
        .tif = TimeInForce::Gtc,
    };
    CHECK(SubmitLimit{} == zeros);
}

TEST_CASE("messages: VenueCommand holds each command type", "[messages]") {
    // Put each of the 5 commands into a VenueCommand.
    // holds_alternative is true for its own type; index() follows the SPEC §2
    // order.
    const std::array<VenueCommand, 5> commands{
        SubmitLimit{}, Cancel{}, Reduce{}, Replace{}, Uncross{},
    };

    // index() follows the SPEC §2 order.
    std::size_t expected_index = 0;
    for (const VenueCommand& command : commands) {
        // `.index()` member function of `std::variant`
        CHECK(command.index() == expected_index);
        ++expected_index;
    }

    const Cancel cancel{.command_sequence = CommandSequence{3},
                        .order_id = OrderId{7}};
    const VenueCommand command = cancel;
    REQUIRE(std::holds_alternative<Cancel>(command));
    CHECK(std::get<Cancel>(command) == cancel);
}

TEST_CASE("messages: envelopes compare sequence, channel and payload",
          "[messages]") {
    // Start from one envelope. Change only the sequence; only the channel;
    // the payload to the same type with another value; the payload to another
    // type. Each changed envelope is not equal to the first.
    const VenueEventEnvelope base{
        .sequence = VenueSequence{1},
        .channel = EventChannel::PublicMarket,
        .payload = PublicOrderCanceled{.order_id = OrderId{7},
                                       .canceled_quantity = Quantity{5}},
    };

    VenueEventEnvelope other_sequence = base;
    other_sequence.sequence = VenueSequence{2};
    CHECK(other_sequence != base);

    VenueEventEnvelope other_channel = base;
    other_channel.channel = EventChannel::PrivateOrder;
    CHECK(other_channel != base);

    VenueEventEnvelope other_value = base;
    other_value.payload = PublicOrderCanceled{.order_id = OrderId{7},
                                              .canceled_quantity = Quantity{4}};
    CHECK(other_value != base);

    // Another type with the same field values: variant compares index first.
    VenueEventEnvelope other_type = base;
    other_type.payload = OrderCanceled{.order_id = OrderId{7},
                                       .canceled_quantity = Quantity{5},
                                       .reason = CancelReason::User};
    CHECK(other_type != base);
}

TEST_CASE("messages: ProcessResult views events without a copy", "[messages]") {
    // A default ProcessResult has no events.
    // Over an array of 2 envelopes: size 2, the same address as the array,
    // and a change to the array shows through the span.
    const ProcessResult empty{};
    CHECK(empty.events.empty());

    std::array<VenueEventEnvelope, 2> buffer{{
        {.sequence = VenueSequence{1},
         .channel = EventChannel::PublicMarket,
         .payload = PublicOrderDeleted{.order_id = OrderId{7}}},
        {.sequence = VenueSequence{2},
         .channel = EventChannel::PublicMarket,
         .payload = PublicOrderDeleted{.order_id = OrderId{8}}},
    }};
    const ProcessResult result{.disposition = Disposition::Accepted,
                               .events = buffer};

    REQUIRE(result.events.size() == 2);
    CHECK(result.events.data() == buffer.data());

    // The span is a view: a change to the buffer shows through it.
    std::get<1>(buffer).sequence = VenueSequence{9};
    CHECK(result.events.back().sequence == VenueSequence{9});
}
