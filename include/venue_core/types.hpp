#ifndef VENUE_CORE_TYPES_HPP
#define VENUE_CORE_TYPES_HPP

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

namespace venue_core {

template <class Tag, class Rep>
struct Strong {
    Rep value{};

    Strong() = default;
    // To stop brace elison.
    // Any user-declared constructor makes `Strong` a non-aggregate;
    // so no aggregate initialization and no brace elision
    constexpr explicit Strong(Rep v) noexcept : value{v} {}

    // `<=>` supplies ordering and implicit equality
    // `std::strong_ordering::less`
    // `std::strong_ordering::equal`
    // `std::strong_ordering::greater`
    // result: **ordering object**
    // an implicitly declared `operator==` - because `default` is used.
    // A manually implemented `<=>` does _NOT_ automatically supply `==`
    friend constexpr auto operator<=>(const Strong&, const Strong&) = default;
};

struct PriceTag;
struct QuantityTag;
struct OrderIdTag;
struct CommandSequenceTag;
struct PrioritySequenceTag;
struct VenueSequenceTag;
struct MatchIdTag;

using Price = Strong<PriceTag, std::int64_t>;  // signed: tick diffs cna be neg
using Quantity = Strong<QuantityTag, std::uint64_t>;
using OrderId = Strong<OrderIdTag, std::uint64_t>;
using CommandSequence = Strong<CommandSequenceTag, std::uint64_t>;
using PrioritySequence = Strong<PrioritySequenceTag, std::uint64_t>;
using VenueSequence = Strong<VenueSequenceTag, std::uint64_t>;
using MatchId = Strong<MatchIdTag, std::uint64_t>;

// `noexcept`: no c++ exception may escape this function
// `std::terminate` called if exception thrown
// does _NOT_ check overflow/underflow
[[nodiscard]] constexpr Quantity operator+(Quantity a, Quantity b) noexcept {
    return Quantity{a.value + b.value};
}
[[nodiscard]] constexpr Quantity operator-(Quantity a, Quantity b) noexcept {
    return Quantity{a.value - b.value};
}

enum class Side : std::uint8_t {
    Buy,
    Sell,
};
enum class TimeInForce : std::uint8_t {
    Gtc,
    Ioc,
};

enum class Liquidity : std::uint8_t {
    Maker,
    Taker,
    Auction,
};

enum class CancelReason : std::uint8_t {
    User,
    IocRemainder,
};

enum class TradingPhase : std::uint8_t {
    PreOpen,
    Continuous,
};

enum class IdPolicy : std::uint8_t {
    SessionUnique,
    StrictlyIncreasing,
};

enum class PagePolicy : std::uint8_t {
    Normal,
    Transparent,
    Explicit2M,
};

enum class RejectReason : std::uint8_t {
    NonMonotonicCommandSequence,
    DuplicateOrderId,
    NonIncreasingOrderId,
    UnknownOrTerminalOrderId,
    DuplicateReplacementId,
    InvalidPrice,
    InvalidQuantity,
    InvalidReduction,
    ReplacementBelowFilledQuantity,
    IocInAuction,
    NotInAuction,
};

// Return the other side
[[nodiscard]] constexpr Side opposite(Side s) noexcept {
    return s == Side::Buy ? Side::Sell : Side::Buy;
}

[[nodiscard]] std::string_view to_string(RejectReason r) noexcept;

// ----- Venue Configuration ------

struct VenueConfig {
    Price min_price{1};
    Price max_price{4096};
    Quantity max_order_quantity{1'000'000'000};
    std::size_t expected_live_orders{std::size_t{1} << 16U};
    IdPolicy id_policy{IdPolicy::SessionUnique};
    bool start_in_auction{false};
    PagePolicy ladder_pages{PagePolicy::Normal};

    // Cap on one order, so lkkkjevel totals cannot overflow 64 bits
    static constexpr std::uint64_t max_order_quantity_cap = std::uint64_t{1}
                                                            << 32U;
    static constexpr std::int64_t max_collar_width = std::int64_t{1} << 24U;

    [[nodiscard]] constexpr bool valid() const noexcept {
        return min_price.value >= 1 and max_price.value >= min_price.value and
               max_price.value - min_price.value < max_collar_width &&
               max_order_quantity.value >= 1 and
               max_order_quantity.value <= max_order_quantity_cap;
    }
};

// ----- Order ID Hashing -----

struct OrderIdHash {
    [[nodiscard]] std::size_t operator()(OrderId id) const noexcept {
        // construct the hasher, then call it
        return std::hash<std::uint64_t>{}(id.value);
    }
};

}  // namespace venue_core

#endif  // !VENUE_CORE_TYPES_HPP
