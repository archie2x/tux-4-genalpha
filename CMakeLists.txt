cmake_minimum_required(VERSION 3.25)

# Vendored Internationalization.cmake (used by both subprojects) needs
# CMP0140 set NEW for return(PROPAGATE ...) to apply.
if(POLICY CMP0140)
    cmake_policy(SET CMP0140 NEW)
endif()

# Hook find_package() via a dependency provider. Must be wired up before
# project() since cmake_language(SET DEPENDENCY_PROVIDER) is only allowed
# from a file listed in CMAKE_PROJECT_TOP_LEVEL_INCLUDES — i.e., loaded
# during project() initialization.
list(
    APPEND CMAKE_PROJECT_TOP_LEVEL_INCLUDES
    "${CMAKE_CURRENT_LIST_DIR}/cmake/t4k_dependency_setup.cmake"
)

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

# Cross-compile runtime bundle: gather the SDL3*.dll's that CPM built
# (their own install rules are suppressed by CPM's EXCLUDE_FROM_ALL),
# plus libwinpthread-1.dll (mingw's pthread shim, dynamically linked
# by SDL3_ttf — the rest of mingw's runtime is static via
# -static-libgcc in the toolchain file).
if(CMAKE_CROSSCOMPILING)
    include(GNUInstallDirs)
    foreach(_lib SDL3 SDL3_image SDL3_ttf SDL3_mixer)
        if(TARGET ${_lib}-shared)
            install(
                FILES $<TARGET_FILE:${_lib}-shared>
                DESTINATION ${CMAKE_INSTALL_BINDIR}
            )
        endif()
    endforeach()

    get_filename_component(_mingw_bin "${CMAKE_C_COMPILER}" REALPATH)
    get_filename_component(_mingw_bin "${_mingw_bin}" DIRECTORY)
    find_file(
        LIBWINPTHREAD_DLL
        libwinpthread-1.dll
        PATHS "${_mingw_bin}" "${_mingw_bin}/../x86_64-w64-mingw32/bin"
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
    )
    if(LIBWINPTHREAD_DLL)
        install(FILES ${LIBWINPTHREAD_DLL} DESTINATION ${CMAKE_INSTALL_BINDIR})
    else()
        message(
            WARNING
            "libwinpthread-1.dll not found near ${_mingw_bin}; SDL3_ttf will fail to load at runtime"
        )
    endif()
endif()
