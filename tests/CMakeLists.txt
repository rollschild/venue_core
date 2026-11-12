find_package(Catch2 3 REQUIRED)
include(Catch) # catch_discover_tests()

add_executable(venue_core_tests test_types.cpp test_messages.cpp)
target_link_libraries(
    venue_core_tests
    PRIVATE venue_core::venue_core Catch2::Catch2WithMain
)
venue_core_enable_warnings(venue_core_tests)

# Register every TEST_CASE with CTest individually.
catch_discover_tests(venue_core_tests)
