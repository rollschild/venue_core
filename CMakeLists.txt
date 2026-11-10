cmake_minimum_required(VERSION 3.28...4.4)

# The name, VERSION and DESCRIPTION are the single source of truth:
# nix/project.nix reads them from here for the Nix package and flake.
project(
    venue_core
    VERSION 0.1.0
    DESCRIPTION "An exchange-side matching engine"
    HOMEPAGE_URL "https://github.com/rollschild/venue_core"
    LANGUAGES CXX
)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source build. Use a preset: cmake --preset dev")
endif()

# -std=c++23, not -std=gnu++23.
set(CMAKE_CXX_EXTENSIONS OFF)
# No C++20 modules here. Scanning is on by default for C++20+ with Ninja and GCC
# 14+, and it injects GCC-only flags into compile_commands.json that clangd and
# clang-tidy reject.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

include(cmake/CompilerWarnings.cmake) # venue_core_enable_warnings()

add_subdirectory(src)

if(PROJECT_IS_TOP_LEVEL)
    add_subdirectory(apps)

    include(CTest) # defines BUILD_TESTING (default ON), calls enable_testing()
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()

if(NOT CMAKE_SKIP_INSTALL_RULES)
    include(cmake/InstallRules.cmake)
endif()
