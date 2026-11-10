#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <print>
#include <span>
#include <string_view>
#include <venue_core/venue_core.hpp>

int main(int argc, char* argv[]) try {
    const std::span args(argv, static_cast<std::size_t>(argc));
    const std::string_view name = args.size() > 1 ? args[1] : "world";
    std::println("{}", venue_core::greet(name));
    return EXIT_SUCCESS;
} catch (const std::exception& e) {
    // Fail with a message and an exit code instead of std::terminate. The
    // handler must not throw itself, hence fputs rather than std::println.
    std::fputs(e.what(), stderr);
    std::fputc('\n', stderr);
    return EXIT_FAILURE;
}
