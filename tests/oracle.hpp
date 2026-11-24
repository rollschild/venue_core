// The oracle: a slow venue that is simple enough to be obviously correct.
// It shares only types.hpp, messages.hpp and SPEC.md with the engine.
#ifndef VENUE_CORE_TESTS_ORACLE_HPP
#define VENUE_CORE_TESTS_ORACLE_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include <venue_core/messages.hpp>
#include <venue_core/types.hpp>

namespace venue_core::testing {

class Oracle {
   public:
    explicit Oracle(VenueConfig config = {});

    // Process one command. The events stay valid until the next call.
    [[nodiscard]] ProcessResult process(const VenueCommand& command);

    [[nodiscard]] std::size_t live_orders() const noexcept;

   private:
    struct LiveOrder {
        OrderId id;
        Side side{};
        Price price;
        Quantity total;
        Quantity filled;
        Quantity leaves;
        PrioritySequence priority;
    };

    // One handler for each command type (SPEC §2, §3, §6.2).
    Disposition handle(const SubmitLimit& command);
    Disposition handle(const Cancel& command);
    Disposition handle(const Reduce& command);   // trace 2
    Disposition handle(const Replace& command);  // traces 2 and 4
    Disposition handle(const Uncross& command);  // Ph8; reject only for now

    // Validation (SPEC §3). A check reads the state and changes nothing.
    [[nodiscard]] bool sequence_in_order(
        CommandSequence sequence) const noexcept;
    [[nodiscard]] bool id_is_fresh(OrderId id) const;
    [[nodiscard]] bool price_in_collar(Price price) const noexcept;
    [[nodiscard]] bool quantity_valid(Quantity quantity) const noexcept;

    // Matching (SPEC §6.1). The taker is not in live_ while it matches.
    [[nodiscard]] std::vector<LiveOrder>::iterator best_maker(Side taker_side,
                                                              Price limit);
    void match(LiveOrder taker, TimeInForce tif);
    void rest(LiveOrder order);

    // Append one event with the next venue sequence.
    void emit(EventChannel channel, const VenueEvent& payload);

    VenueConfig config_;
    TradingPhase phase_{};
    CommandSequence last_command_sequence_;
    std::uint64_t next_venue_sequence_{1};
    // Remove [[maybe_unused]] when the fill loop uses it.
    [[maybe_unused]] std::uint64_t next_match_id_{1};
    std::uint64_t next_priority_{1};
    std::vector<OrderId> used_ids_;  // policy SessionUnique
    OrderId highest_accepted_id_;    // policy StrictlyIncreasing
    std::vector<LiveOrder> live_;
    std::vector<VenueEventEnvelope> events_;
};

}  // namespace venue_core::testing

#endif  // VENUE_CORE_TESTS_ORACLE_HPP
