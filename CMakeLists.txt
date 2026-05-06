cmake_minimum_required(VERSION 3.25)

# Vendored Internationalization.cmake (used by both subprojects) needs
# CMP0140 set NEW for return(PROPAGATE ...) to apply.
if(POLICY CMP0140)
    cmake_policy(SET CMP0140 NEW)
endif()

project(
    tux4kids
    DESCRIPTION
        "Tux4Kids super-project: t4kcommon + tuxtype + tuxmath (SDL3 port)"
    LANGUAGES C
)

# Auto-detect ccache. Skips silently if not installed, and yields if the
# launcher's already been pinned by a parent build / explicit override.
get_property(_launch GLOBAL PROPERTY RULE_LAUNCH_COMPILE)
if(NOT _launch)
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
        message(STATUS "ccache enabled: ${CCACHE_PROGRAM}")
    endif()
endif()

# Subprojects are submodules pinned at known-compatible revisions
# (a2x/sdl3_port branch). Build in dependency order.
add_subdirectory(t4kcommon)
add_subdirectory(tuxtype)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tuxmath/CMakeLists.txt")
    add_subdirectory(tuxmath)
endif()
