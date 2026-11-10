# `cmake --install` layout plus an exported CMake package, so other projects can
# `find_package(venue_core)` and link `venue_core::venue_core`.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/venue_core")

install(TARGETS venue_core_lib EXPORT venue_core-targets FILE_SET HEADERS)
install(
    EXPORT venue_core-targets
    NAMESPACE venue_core::
    DESTINATION "${cmakedir}"
)

write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/venue_core-config-version.cmake"
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/venue_core-config.cmake.in"
    "${PROJECT_BINARY_DIR}/venue_core-config.cmake"
    INSTALL_DESTINATION "${cmakedir}"
)
install(
    FILES
        "${PROJECT_BINARY_DIR}/venue_core-config.cmake"
        "${PROJECT_BINARY_DIR}/venue_core-config-version.cmake"
    DESTINATION "${cmakedir}"
)
