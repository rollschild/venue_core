#pragma once

#include <string>
#include <string_view>

namespace venue_core {

/// Returns "Hello, <name>!".
[[nodiscard]] std::string greet(std::string_view name);

}  // namespace venue_core
