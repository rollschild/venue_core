#include <array>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <compare>
#include <concepts>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <venue_core/types.hpp>

using namespace venue_core;

namespace {

template <class Left, class Right>
concept EqualityComparable = requires(const Left& left, const Right& right) {
    { left == right } -> std::same_as<bool>;
};

template <class Left, class Right>
concept LessComparable = requires(const Left& left, const Right& right) {
    { left < right } -> std::same_as<bool>;
};

template <class T>
concept Addable = requires(T left, T right) { left + right; };

template <class T>
concept Subtractable = requires(T left, T right) { left - right; };

constexpr auto max_quantity = std::numeric_limits<std::uint64_t>::max();
constexpr auto min_price_rep = std::numeric_limits<std::int64_t>::min();
constexpr auto max_price_rep = std::numeric_limits<std::int64_t>::max();

using ArithmeticCase = std::array<std::uint64_t, 3>;
using PriceBand = std::pair<std::int64_t, std::int64_t>;
using ReasonText = std::pair<RejectReason, std::string_view>;

static_assert(!std::is_convertible_v<Price, Quantity>);
static_assert(!std::is_convertible_v<Quantity, Price>);
static_assert(!std::is_convertible_v<std::int64_t, Price>);
static_assert(!std::is_convertible_v<Price, std::int64_t>);
static_assert(!std::is_assignable_v<Price&, Quantity>);
static_assert(!std::is_assignable_v<OrderId&, Quantity>);
static_assert(!std::is_same_v<CommandSequence, PrioritySequence>);
static_assert(!std::is_same_v<PrioritySequence, VenueSequence>);
static_assert(!std::is_same_v<CommandSequence, VenueSequence>);
static_assert(!std::is_same_v<OrderId, MatchId>);

static_assert(!EqualityComparable<Price, Quantity>);
static_assert(!EqualityComparable<OrderId, Quantity>);
static_assert(!EqualityComparable<CommandSequence, VenueSequence>);
static_assert(!LessComparable<Price, Quantity>);
static_assert(!LessComparable<CommandSequence, PrioritySequence>);

static_assert(Addable<Quantity> && Subtractable<Quantity>);
static_assert(!Addable<Price> && !Subtractable<Price>);
static_assert(!Addable<OrderId> && !Subtractable<OrderId>);
// asks compiler what the exact type is
// does _NOT_ evaluate the expression
static_assert(
    std::same_as<decltype(Price{} <=> Price{}), std::strong_ordering>);
static_assert(std::same_as<decltype(Quantity{} + Quantity{}), Quantity>);
static_assert(std::same_as<decltype(Quantity{} - Quantity{}), Quantity>);

static_assert((Quantity{7} + Quantity{5}).value == 12);
static_assert((Quantity{7} - Quantity{5}).value == 2);
static_assert(opposite(Side::Buy) == Side::Sell);
static_assert(VenueConfig{}.valid());

static_assert(noexcept(Quantity{1} + Quantity{1}));
static_assert(noexcept(Quantity{1} - Quantity{1}));
static_assert(noexcept(opposite(Side::Buy)));

// `declval`: lets compiler inspect an expression without constructing an object
// no object is created;
// `valid()` is not executed;
// the operand of `noexcept` is _unevaluated_ - only examined by compiler
static_assert(noexcept(std::declval<const VenueConfig&>().valid()));
static_assert(noexcept(to_string(RejectReason::InvalidPrice)));
static_assert(noexcept(OrderIdHash{}(OrderId{1})));

}  // namespace

TEST_CASE("types: comparisons preserve numeric order", "[types]") {
    CHECK(Price{-1} < Price{0});
    CHECK(Price{0} < Price{1});
    CHECK(Price{100} == Price{100});
    CHECK(Price{100} != Price{101});
    CHECK(Price{100} <= Price{100});
    CHECK(Price{101} > Price{100});
    CHECK(Price{101} >= Price{101});
    CHECK((Price{100} <=> Price{101}) == std::strong_ordering::less);

    CHECK(Quantity{0} < Quantity{max_quantity});
    CHECK(PrioritySequence{41} < PrioritySequence{42});
    CHECK(OrderId{7} == OrderId{7});
    CHECK(MatchId{7} != MatchId{8});
}

TEST_CASE("types: quantity addition", "[types]") {
    constexpr std::array cases{
        ArithmeticCase{0, 0, 0},
        ArithmeticCase{0, 17, 17},
        ArithmeticCase{17, 0, 17},
        ArithmeticCase{7, 5, 12},
        ArithmeticCase{5, 7, 12},
        ArithmeticCase{
            VenueConfig::max_order_quantity_cap - 1,
            1,
            VenueConfig::max_order_quantity_cap,
        },
        ArithmeticCase{max_quantity - 1, 1, max_quantity},
        ArithmeticCase{max_quantity, 0, max_quantity},
    };

    for (const auto& [left, right, expected] : cases) {
        CAPTURE(left, right, expected);
        CHECK((Quantity{left} + Quantity{right}).value == expected);
    }
}

TEST_CASE("types: quantity subtraction", "[types]") {
    constexpr std::array cases{
        ArithmeticCase{0, 0, 0},
        ArithmeticCase{7, 5, 2},
        ArithmeticCase{17, 0, 17},
        ArithmeticCase{17, 17, 0},
        ArithmeticCase{
            VenueConfig::max_order_quantity_cap,
            1,
            VenueConfig::max_order_quantity_cap - 1,
        },
        ArithmeticCase{max_quantity, 1, max_quantity - 1},
        ArithmeticCase{max_quantity, max_quantity, 0},
    };

    for (const auto& [left, right, expected] : cases) {
        CAPTURE(left, right, expected);
        CHECK((Quantity{left} - Quantity{right}).value == expected);
    }
}

TEST_CASE("types: opposite switches sides", "[types]") {
    CHECK(opposite(Side::Buy) == Side::Sell);
    CHECK(opposite(Side::Sell) == Side::Buy);
}

TEST_CASE("types: valid price bands", "[types]") {
    constexpr auto width = VenueConfig::max_collar_width;
    constexpr std::array bands{
        PriceBand{1, 1},
        PriceBand{1, width},
        PriceBand{10, 10 + width - 1},
        PriceBand{max_price_rep, max_price_rep},
        PriceBand{max_price_rep - width + 1, max_price_rep},
    };

    CHECK(VenueConfig{}.valid());

    for (const auto& [low, high] : bands) {
        CAPTURE(low, high);

        VenueConfig config;
        config.min_price = Price{low};
        config.max_price = Price{high};

        CHECK(config.valid());
    }
}

TEST_CASE("types: invalid price bands", "[types]") {
    constexpr auto width = VenueConfig::max_collar_width;
    constexpr std::array bands{
        PriceBand{0, 4096},
        PriceBand{-1, 4096},
        PriceBand{2, 1},
        PriceBand{1, 0},
        PriceBand{1, width + 1},
        PriceBand{10, 10 + width},
        PriceBand{1, max_price_rep},
        PriceBand{min_price_rep, max_price_rep},
        PriceBand{max_price_rep, min_price_rep},
    };

    for (const auto& [low, high] : bands) {
        CAPTURE(low, high);

        VenueConfig config;
        config.min_price = Price{low};
        config.max_price = Price{high};

        CHECK_FALSE(config.valid());
    }
}

TEST_CASE("types: order quantity configuration bounds", "[types]") {
    constexpr auto cap = VenueConfig::max_order_quantity_cap;

    for (const auto quantity : std::array<std::uint64_t, 3>{1, cap - 1, cap}) {
        CAPTURE(quantity);

        VenueConfig config;
        config.max_order_quantity = Quantity{quantity};

        CHECK(config.valid());
    }

    for (const auto quantity :
         std::array<std::uint64_t, 3>{0, cap + 1, max_quantity}) {
        CAPTURE(quantity);

        VenueConfig config;
        config.max_order_quantity = Quantity{quantity};

        CHECK_FALSE(config.valid());
    }
}

TEST_CASE("types: supported configuration options", "[types]") {
    VenueConfig config;
    config.id_policy = IdPolicy::StrictlyIncreasing;
    config.start_in_auction = true;

    for (const auto pages : std::array{
             PagePolicy::Normal,
             PagePolicy::Transparent,
             PagePolicy::Explicit2M,
         }) {
        config.ladder_pages = pages;
        CHECK(config.valid());
    }
}

TEST_CASE("types: rejection strings match the contract", "[types]") {
    constexpr std::array cases{
        ReasonText{
            RejectReason::NonMonotonicCommandSequence,
            "non_monotonic_command_sequence",
        },
        ReasonText{
            RejectReason::DuplicateOrderId,
            "duplicate_order_id",
        },
        ReasonText{
            RejectReason::NonIncreasingOrderId,
            "non_increasing_order_id",
        },
        ReasonText{
            RejectReason::UnknownOrTerminalOrderId,
            "unknown_or_terminal_order_id",
        },
        ReasonText{
            RejectReason::DuplicateReplacementId,
            "duplicate_replacement_id",
        },
        ReasonText{
            RejectReason::InvalidPrice,
            "invalid_price",
        },
        ReasonText{
            RejectReason::InvalidQuantity,
            "invalid_quantity",
        },
        ReasonText{
            RejectReason::InvalidReduction,
            "invalid_reduction",
        },
        ReasonText{
            RejectReason::ReplacementBelowFilledQuantity,
            "replacement_below_filled_quantity",
        },
        ReasonText{
            RejectReason::IocInAuction,
            "ioc_in_auction",
        },
        ReasonText{
            RejectReason::NotInAuction,
            "not_in_auction",
        },
    };

    for (const auto& [reason, expected] : cases) {
        CAPTURE(expected);  // ?
        CHECK(to_string(reason) == expected);
    }
}

TEST_CASE("types: order IDs support hash lookup by value", "[types]") {
    const OrderIdHash hash;
    CHECK(hash(OrderId{7}) == hash(OrderId{7}));

    std::unordered_map<OrderId, int, OrderIdHash> values;
    values.emplace(OrderId{0}, 10);
    values.emplace(OrderId{7}, 20);
    values.emplace(OrderId{max_quantity}, 30);

    CHECK(values.at(OrderId{0}) == 10);
    CHECK(values.at(OrderId{7}) == 20);
    CHECK(values.at(OrderId{max_quantity}) == 30);

    values.insert_or_assign(OrderId{7}, 21);

    CHECK(values.size() == 3);
    CHECK(values.at(OrderId{7}) == 21);
    CHECK_FALSE(values.contains(OrderId{8}));
}
