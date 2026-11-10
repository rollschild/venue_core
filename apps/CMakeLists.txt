add_executable(venue_core_exe main.cpp)
set_target_properties(venue_core_exe PROPERTIES OUTPUT_NAME venue_core)
target_link_libraries(venue_core_exe PRIVATE venue_core::venue_core)
venue_core_enable_warnings(venue_core_exe)

install(TARGETS venue_core_exe)
