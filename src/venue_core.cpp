#include <format>
#include <string>
#include <string_view>
#include <venue_core/venue_core.hpp>

namespace venue_core {

std::string greet(std::string_view name) {
    return std::format("Hello, {}!", name);
}

}  // namespace venue_core
