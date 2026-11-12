#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>
#include <string>
#include <string_view>

namespace venue_core {

template <class Tag, class Rep>
struct Strong {
    Rep value{};
    // TODO: comparisons (== and ordering)
};

struct PriceTag;
struct QuantityTag;
struct OrderIdTag;
struct CommonSequenceTag;
struct VenueSequenceTag;
struct MatchIdTag;

using Price = Strong<PriceTag, std::int64_t>;  // signed: tick diffs cna be neg
using Quantity = Strong<QuantityTag, std::uint64_t>;
using OrderId = Strong<OrderIdTag, std::uint64_t>;
using CommandSequence = Strong<CommonSequenceTag, std::uint64_t>;
using VenueSequence = Strong<VenueSequenceTag, std::uint64_t>;
using MatchId = Strong<MatchIdTag, std::uint64_t>;

[[nodiscard]] constexpr Quantity operator+(Quantity a, Quantity b) noexcept;
[[nodiscard]] constexpr Quantity operator-(Quantity a, Quantity b) noexcept;

enum class Side : std::uint8_t {
    BUY,
    SELL,
};
enum class TimeInForce : std::uint8_t {
    GTC,
    IOC,
};

enum class Liquidity : std::uint8_t {
    MAKER,
    TAKER,
    AUCTION,
};

enum class CancelReason : std::uint8_t {
    USER,
    IOC_REMAINDER,
};

enum class TradingPhase : std::uint8_t {
    // TODO
};

enum class IdPolicy : std::uint8_t {
    SESSION_UINQUE,
    STRICTLY_INCREASING,
};

enum class RejectReason : std::uint8_t {
    NON_MONOTONIC_COMMAND_SEQUENCE,
    DUPLICATE_ORDER_ID,
    NON_INCREASING_ORDER_ID,
    INVALID_PRICE,
    INVALID_QUANTITY,
    REPLACEMENT_BELOW_FILLED_QUANTITY,
    IOC_IN_AUCTION,
    NOT_IN_AUCTION,
};

// Return the other side
[[nodiscard]] constexpr Side opposite(Side s) noexcept;

[[nodiscard]] std::string_view to_string(RejectReason r) noexcept;

struct VenueConfig {
    // Cap on one order, so level totals cannot overflow 64 bits
    static constexpr std::uint64_t kMaxOrderQuantityCap = std::uint64_t{1}
                                                          << 32U;
    [[nodiscard]] constexpr bool valid() const noexcept;
};

}  // namespace venue_core

#endif  // !TYPES_HPP
