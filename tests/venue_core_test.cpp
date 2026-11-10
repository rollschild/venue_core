#include <catch2/catch_test_macros.hpp>
#include <venue_core/venue_core.hpp>

TEST_CASE("greet formats the name", "[greet]") {
    REQUIRE(venue_core::greet("Nix") == "Hello, Nix!");
}

TEST_CASE("greet accepts an empty name", "[greet]") {
    REQUIRE(venue_core::greet("") == "Hello, !");
}
