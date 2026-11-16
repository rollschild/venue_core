#include <string_view>
#include <venue_core/types.hpp>

namespace venue_core {
[[nodiscard]] std::string_view to_string(RejectReason reason) noexcept {
    switch (reason) {
        case RejectReason::NonMonotonicCommandSequence:
            return "non_monotonic_command_sequence";
        case RejectReason::DuplicateOrderId:
            return "duplicate_order_id";
        case RejectReason::NonIncreasingOrderId:
            return "non_increasing_order_id";
        case RejectReason::UnknownOrTerminalOrderId:
            return "unknown_or_terminal_order_id";
        case RejectReason::DuplicateReplacementId:
            return "duplicate_replacement_id";
        case RejectReason::InvalidPrice:
            return "invalid_price";
        case RejectReason::InvalidQuantity:
            return "invalid_quantity";
        case RejectReason::InvalidReduction:
            return "invalid_reduction";
        case RejectReason::ReplacementBelowFilledQuantity:
            return "replacement_below_filled_quantity";
        case RejectReason::IocInAuction:
            return "ioc_in_auction";
        case RejectReason::NotInAuction:
            return "not_in_auction";
    }

    return "unknown_reject_reason";
}
}  // namespace venue_core
