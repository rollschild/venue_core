# Strict warnings for this project's own targets. PRIVATE, so they never reach
# consumers of the library. -Werror is not set here: the presets and the Nix
# package turn on CMake's built-in CMAKE_COMPILE_WARNING_AS_ERROR instead.
function(venue_core_enable_warnings target)
    set(warnings
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Woverloaded-virtual
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        list(
            APPEND warnings
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
            -Wuseless-cast
        )
    endif()
    target_compile_options(${target} PRIVATE ${warnings})
endfunction()
