#include <string_view>
#include <venue_core/types.hpp>

namespace venue_core {
[[nodiscard]] std::string_view to_string(RejectReason reason) noexcept {
    switch (reason) {
        case RejectReason::NON_MONOTONIC_COMMAND_SEQUENCE:
            return "non_monotonic_command_sequence";
        case RejectReason::DUPLICATE_ORDER_ID:
            return "duplicate_order_id";
        case RejectReason::NON_INCREASING_ORDER_ID:
            return "non_increasing_order_id";
        case RejectReason::UNKNOWN_OR_TERMINAL_ORDER_ID:
            return "unknown_or_terminal_order_id";
        case RejectReason::DUPLICATE_REPLACEMENT_ID:
            return "duplicate_replacement_id";
        case RejectReason::INVALID_PRICE:
            return "invalid_price";
        case RejectReason::INVALID_QUANTITY:
            return "invalid_quantity";
        case RejectReason::INVALID_REDUCTION:
            return "invalid_reduction";
        case RejectReason::REPLACEMENT_BELOW_FILLED_QUANTITY:
            return "replacement_below_filled_quantity";
        case RejectReason::IOC_IN_AUCTION:
            return "ioc_in_auction";
        case RejectReason::NOT_IN_AUCTION:
            return "not_in_auction";
    }

    return "unknown_reject_reason";
}
}  // namespace venue_core
