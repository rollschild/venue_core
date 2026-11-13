#ifndef MESSAGES_HPP
#define MESSAGES_HPP

#include <span>
#include <variant>
#include <venue_core/types.hpp>

namespace venue_core {
// ----- Commands -----
// Commands request actions
struct SubmitLimit {
    CommandSequence command_sequence;
    OrderId order_id;
    Side side{};
    Price limit_price;
    Quantity quantity;
    TimeInForce tif{};

    // `==default` asks compiler to generate the member-by-member comparison;
    // every corresponding field must compare equal.
    // prerequisite: the fields must themselves support equality.
    // `friend` makes this declaration a _nonmember_ function.
    // A defaulted comparison must be a member _or_ a friend
    friend bool operator==(const SubmitLimit&, const SubmitLimit&) = default;
};

struct Cancel {
    CommandSequence command_sequence;
    OrderId order_id;
    friend bool operator==(const Cancel&, const Cancel&) = default;
};

struct Reduce {
    CommandSequence command_sequence;
    OrderId order_id;
    Quantity new_total_quantity;
    friend bool operator==(const Reduce&, const Reduce&) = default;
};

struct Replace {
    CommandSequence command_sequence;
    OrderId old_order_id;
    OrderId new_order_id;
    Price new_price;
    Quantity new_total_quantity;
    TimeInForce tif{};
    friend bool operator==(const Replace&, const Replace&) = default;
};

struct Uncross {
    CommandSequence command_sequence;
    Price reference_price;
    friend bool operator==(const Uncross&, const Uncross&) = default;
};

using VenueCommand =
    std::variant<SubmitLimit, Cancel, Reduce, Replace, Uncross>;

// ----- Events -----
// Events report decisions

// Private channel
// private events inform an order's owner
struct OrderAccepted {
    OrderId order_id;
    Side side{};
    Price price;
    Quantity quantity;
    TimeInForce tif{};
    friend bool operator==(const OrderAccepted&,
                           const OrderAccepted&) = default;
};
struct OrderRejected {
    CommandSequence command_sequence;
    OrderId order_id;
    RejectReason reason{};
    friend bool operator==(const OrderRejected&,
                           const OrderRejected&) = default;
};
struct CancelRejected {
    CommandSequence command_sequence;
    OrderId order_id;
    RejectReason reason{};
    friend bool operator==(const CancelRejected&,
                           const CancelRejected&) = default;
};
struct ReplaceRejected {
    CommandSequence command_sequence;
    OrderId old_order_id;
    OrderId new_order_id;
    RejectReason reason{};
    friend bool operator==(const ReplaceRejected&,
                           const ReplaceRejected&) = default;
};
struct ControlRejected {
    CommandSequence command_sequence;
    RejectReason reason{};
    friend bool operator==(const ControlRejected&,
                           const ControlRejected&) = default;
};
struct OrderExecuted {
    OrderId order_id;
    MatchId match_id;
    Price price;
    Quantity quantity;  // quantity of this fill
    Quantity leaves;    // open quantity after this fill
    Liquidity liquidity{};
    friend bool operator==(const OrderExecuted&,
                           const OrderExecuted&) = default;
};
struct OrderCanceled {
    OrderId order_id;
    Quantity canceled_quantity;
    CancelReason reason{};
    friend bool operator==(const OrderCanceled&,
                           const OrderCanceled&) = default;
};
struct OrderReduced {
    OrderId order_id;
    Quantity new_total_quantity;
    Quantity leaves;
    Quantity removed_quantity;
    friend bool operator==(const OrderReduced&, const OrderReduced&) = default;
};
struct OrderReplaced {
    OrderId old_order_id;
    OrderId new_order_id;
    Price price;
    Quantity new_total_quantity;
    Quantity leaves;
    friend bool operator==(const OrderReplaced&,
                           const OrderReplaced&) = default;
};

// Public events
// Public events let consumers reconstruct the visible book
struct PublicAddOrder {
    OrderId order_id;
    Side side{};
    Price price;
    Quantity quantity;

    friend bool operator==(const PublicAddOrder&,
                           const PublicAddOrder&) = default;
};
struct PublicOrderExecuted {
    OrderId order_id;
    MatchId match_id;
    Quantity quantity;

    friend bool operator==(const PublicOrderExecuted&,
                           const PublicOrderExecuted&) = default;
};
struct PublicOrderCanceled {
    OrderId order_id;
    Quantity canceled_quantity;

    friend bool operator==(const PublicOrderCanceled&,
                           const PublicOrderCanceled&) = default;
};
struct PublicOrderDeleted {
    OrderId order_id;

    friend bool operator==(const PublicOrderDeleted&,
                           const PublicOrderDeleted&) = default;
};
struct PublicOrderReplaced {
    OrderId old_order_id;
    OrderId new_order_id;
    Price price;
    Quantity total_quantity;
    friend bool operator==(const PublicOrderReplaced&,
                           const PublicOrderReplaced&) = default;
};
struct PublicTrade {
    MatchId match_id;
    Price price;
    Quantity quantity;
    Side aggressor{};
    friend bool operator==(const PublicTrade&, const PublicTrade&) = default;
};
struct PublicCross {
    Price price;
    Quantity quantity;
    friend bool operator==(const PublicCross&, const PublicCross&) = default;
};

// ----- Event Stream -----
using VenueEvent =
    std::variant<OrderAccepted, OrderRejected, CancelRejected, ReplaceRejected,
                 ControlRejected, OrderExecuted, OrderCanceled, OrderReduced,
                 OrderReplaced, PublicAddOrder, PublicOrderExecuted,
                 PublicOrderCanceled, PublicOrderDeleted, PublicOrderReplaced,
                 PublicTrade, PublicCross>;

enum class EventChannel : std::uint8_t {
    PRIVATE_ORDER,
    PUBLIC_MARKET,
};

struct VenueEventEnvelope {
    VenueSequence sequence;
    EventChannel channel{};
    VenueEvent payload;
    friend bool operator==(const VenueEventEnvelope&,
                           const VenueEventEnvelope&) = default;
};

enum class Disposition : std::uint8_t { ACCEPTED, REJECTED };

struct ProcessResult {
    Disposition disposition{};

    // A view of the engine’s event buffer without owning it.
    // The engine owns this buffer. Valid until the next process() call.
    std::span<const VenueEventEnvelope> events;
}

}  // namespace venue_core

#endif  // !MESSAGES_HPP
