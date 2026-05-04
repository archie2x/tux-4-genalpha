cmake_minimum_required(VERSION 3.25)

# Vendored Internationalization.cmake (used by both subprojects) needs
# CMP0140 set NEW for return(PROPAGATE ...) to apply.
if(POLICY CMP0140)
    cmake_policy(SET CMP0140 NEW)
endif()

project(tux4kids
    DESCRIPTION "Tux4Kids super-project: t4kcommon + tuxtype (SDL3 port)"
    LANGUAGES C
)

# Both subprojects are submodules pinned at known-compatible revisions
# (a2x/sdl3_port branch). Build them in dependency order.
add_subdirectory(t4kcommon)
add_subdirectory(tuxtype)
