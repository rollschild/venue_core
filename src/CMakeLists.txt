add_library(venue_core_lib)
add_library(venue_core::venue_core ALIAS venue_core_lib)

target_sources(
    venue_core_lib
    PRIVATE venue_core.cpp
    PUBLIC
        FILE_SET HEADERS
            BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
            FILES "${PROJECT_SOURCE_DIR}/include/venue_core/venue_core.hpp"
)

target_compile_features(venue_core_lib PUBLIC cxx_std_23)

set_target_properties(
    venue_core_lib
    PROPERTIES
        OUTPUT_NAME venue_core # libvenue_core.a, not libvenue_core_lib.a
        EXPORT_NAME venue_core # consumers link venue_core::venue_core
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
)

venue_core_enable_warnings(venue_core_lib)
