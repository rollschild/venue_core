#include "oracle.hpp"

#include <algorithm>
#include <cstddef>
#include <variant>
#include <vector>
#include <venue_core/messages.hpp>
#include <venue_core/types.hpp>

namespace venue_core::testing {

Oracle::Oracle(VenueConfig config)
    : config_{config},
      phase_{config.start_in_auction ? TradingPhase::PreOpen
                                     : TradingPhase::Continuous} {}

ProcessResult Oracle::process(const VenueCommand& command) {
    // The buffer is used again for each command.
    events_.clear();
    // std::visit calls the lambda with the alternative that the command holds.
    // Overload resolution then selects the handle() for that type.
    const Disposition disposition = std::visit(
        [this](const auto& alternative) { return handle(alternative); },
        command);
    return ProcessResult{.disposition = disposition, .events = events_};
}

std::size_t Oracle::live_orders() const noexcept { return live_.size(); }

Disposition Oracle::handle(const SubmitLimit& command) {
    // SPEC §3 SubmitLimit checks #1 to #5, in order. On the first
    //       failure: one PRV OrderRejected, then return Rejected.
    if (!sequence_in_order(command.command_sequence)) {
        emit(
            EventChannel::PrivateOrder,
            OrderRejected{.command_sequence = command.command_sequence,
                          .order_id = command.order_id,
                          .reason = RejectReason::NonMonotonicCommandSequence});
        return Disposition::Rejected;
    }

    last_command_sequence_ = command.command_sequence;

    if (!id_is_fresh(command.order_id)) {
        const RejectReason reason = config_.id_policy == IdPolicy::SessionUnique
                                        ? RejectReason::DuplicateOrderId
                                        : RejectReason::NonIncreasingOrderId;
        emit(EventChannel::PrivateOrder,
             OrderRejected{.command_sequence = command.command_sequence,
                           .order_id = command.order_id,
                           .reason = reason});
        return Disposition::Rejected;
    }
    if (!price_in_collar(command.limit_price)) {
        emit(EventChannel::PrivateOrder,
             OrderRejected{.command_sequence = command.command_sequence,
                           .order_id = command.order_id,
                           .reason = RejectReason::InvalidPrice});
        return Disposition::Rejected;
    }
    if (!quantity_valid(command.quantity)) {
        emit(EventChannel::PrivateOrder,
             OrderRejected{.command_sequence = command.command_sequence,
                           .order_id = command.order_id,
                           .reason = RejectReason::InvalidQuantity});
        return Disposition::Rejected;
    }
    if (command.tif == TimeInForce::Ioc and phase_ == TradingPhase::PreOpen) {
        emit(EventChannel::PrivateOrder,
             OrderRejected{.command_sequence = command.command_sequence,
                           .order_id = command.order_id,
                           .reason = RejectReason::IocInAuction});
        return Disposition::Rejected;
    }

    // all validation passed
    used_ids_.push_back(command.order_id);
    highest_accepted_id_ = command.order_id;
    emit(EventChannel::PrivateOrder, OrderAccepted{.order_id = command.order_id,
                                                   .side = command.side,
                                                   .price = command.limit_price,
                                                   .quantity = command.quantity,
                                                   .tif = command.tif});
    const LiveOrder taker{.id = command.order_id,
                          .side = command.side,
                          .price = command.limit_price,
                          .total = command.quantity,
                          .filled = Quantity{0},
                          .leaves = command.quantity,
                          .priority = PrioritySequence{0}};
    match(taker, command.tif);

    return Disposition::Accepted;
}

Disposition Oracle::handle(const Cancel& command) {
    if (!sequence_in_order(command.command_sequence)) {
        emit(EventChannel::PrivateOrder,
             CancelRejected{
                 .command_sequence = command.command_sequence,
                 .order_id = command.order_id,
                 .reason = RejectReason::NonMonotonicCommandSequence});
        return Disposition::Rejected;
    }

    last_command_sequence_ = command.command_sequence;

    auto it = std::ranges::find_if(live_, [&command](const auto& live_order) {
        return live_order.id == command.order_id;
    });
    if (it == live_.end()) {
        emit(EventChannel::PrivateOrder,
             CancelRejected{.command_sequence = command.command_sequence,
                            .order_id = command.order_id,
                            .reason = RejectReason::UnknownOrTerminalOrderId});
        return Disposition::Rejected;
    }
    emit(EventChannel::PrivateOrder,
         OrderCanceled{.order_id = command.order_id,
                       .canceled_quantity = it->leaves,
                       .reason = CancelReason::User});
    emit(EventChannel::PublicMarket,
         PublicOrderDeleted{.order_id = command.order_id});

    live_.erase(it);
    return Disposition::Accepted;
}

// A stub. Remove the NOLINT line when the body uses the state.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
Disposition Oracle::handle(const Reduce& /*command*/) {
    // TODO (trace 2): SPEC §3 Reduce checks, then SPEC §6.2 Reduce.
    return Disposition::Rejected;
}

// A stub. Remove the NOLINT line when the body uses the state.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
Disposition Oracle::handle(const Replace& /*command*/) {
    // TODO (traces 2 and 4): SPEC §3 Replace checks, then SPEC §6.2
    // Replace.
    return Disposition::Rejected;
}

// A stub. Remove the NOLINT line when the body uses the state.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
Disposition Oracle::handle(const Uncross& /*command*/) {
    // TODO: SPEC §3 Uncross checks. The auction itself is Ph8.
    return Disposition::Rejected;
}

bool Oracle::sequence_in_order(CommandSequence sequence) const noexcept {
    // SPEC §1: strictly increasing from one command to the next.
    return sequence > last_command_sequence_;
}

bool Oracle::id_is_fresh(OrderId id) const {
    // SPEC §1, order IDs. The handlers record each accepted ID.
    switch (config_.id_policy) {
        case IdPolicy::SessionUnique:
            return std::ranges::find(used_ids_, id) == used_ids_.end();
        case IdPolicy::StrictlyIncreasing:
            return id > highest_accepted_id_;
    }
    return false;
}

bool Oracle::price_in_collar(Price price) const noexcept {
    // SPEC §1: [min_price, max_price], both ends included.
    return price >= config_.min_price && price <= config_.max_price;
}

bool Oracle::quantity_valid(Quantity quantity) const noexcept {
    // SPEC §1: 1 to max_order_quantity.
    return quantity >= Quantity{1} && quantity <= config_.max_order_quantity;
}

std::vector<Oracle::LiveOrder>::iterator Oracle::best_maker(Side /*taker_side*/,
                                                            Price /*limit*/) {
    // TODO: scan all of live_. Keep the best order that can trade (SPEC
    // §6.1).
    //       Return live_.end() if there is none.
    return live_.end();
}

/**
 * Trades the incoming order against the book, then disposes of what is left.
 */
void Oracle::match(LiveOrder taker, TimeInForce tif) {
    // TODO (golden trace 1): while the taker has leaves and best_maker()
    //       finds a maker: one fill group (SPEC §6.2 SubmitLimit
    //       step 2). A maker with leaves 0 leaves live_.
    if (taker.leaves > Quantity{0}) {
        switch (tif) {
            case venue_core::TimeInForce::Gtc:
                rest(taker);
                break;
            case venue_core::TimeInForce::Ioc:
                emit(EventChannel::PrivateOrder,
                     OrderCanceled{.order_id = taker.id,
                                   .canceled_quantity = taker.leaves,
                                   .reason = CancelReason::IocRemainder});
                break;
        }
    }
}

/**
 * The only way into the book:
 *   1. assigns the priority, which is the order's place in the queue at its
 * price;
 *   2. adds the order to live_, so best_maker can find it from now on;
 *   3. announces it on the public channel with leaves, not total.
 */
void Oracle::rest(LiveOrder order) {
    order.priority = PrioritySequence{next_priority_};
    ++next_priority_;
    live_.push_back(order);
    emit(EventChannel::PublicMarket, PublicAddOrder{
                                         .order_id = order.id,
                                         .side = order.side,
                                         .price = order.price,
                                         .quantity = order.leaves,
                                     });
}

void Oracle::emit(EventChannel channel, const VenueEvent& payload) {
    // SPEC §1: the venue sequence starts at 1 and rises by 1 for each
    // event.
    events_.push_back(VenueEventEnvelope{
        .sequence = VenueSequence{next_venue_sequence_},
        .channel = channel,
        .payload = payload,
    });
    ++next_venue_sequence_;
}

}  // namespace venue_core::testing
