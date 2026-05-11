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
# get_property(_launch GLOBAL PROPERTY RULE_LAUNCH_COMPILE)
# if(NOT _launch)
#     find_program(CCACHE_PROGRAM ccache)
#     if(CCACHE_PROGRAM)
#         set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
#         message(STATUS "ccache enabled: ${CCACHE_PROGRAM}")
#     endif()
# endif()

# Subprojects are submodules pinned at known-compatible revisions
# (a2x/sdl3_port branch). Build in dependency order.
add_subdirectory(t4kcommon)
add_subdirectory(tuxtype)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tuxmath/CMakeLists.txt")
    add_subdirectory(tuxmath)
endif()

# ---------------------------------------------------------------------------
# API documentation
# ---------------------------------------------------------------------------
option(BUILD_DOXYGEN "Add Doxygen API documentation targets" ON)
if(BUILD_DOXYGEN)
    find_package(Doxygen QUIET)
    if(DOXYGEN_FOUND)
        set(DOXYGEN_GENERATE_LATEX NO)
        set(DOXYGEN_GENERATE_HTML YES)
        set(DOXYGEN_RECURSIVE YES)
        set(DOXYGEN_EXTRACT_ALL NO)
        set(DOXYGEN_EXTRACT_STATIC YES)
        set(DOXYGEN_OPTIMIZE_OUTPUT_FOR_C YES)
        set(DOXYGEN_WARN_IF_UNDOCUMENTED NO)
        set(DOXYGEN_QUIET YES)
        set(DOXYGEN_EXCLUDE_PATTERNS
            "*/.git/*"
            "*/_build/*"
            "*/build/*"
            "*/.#*"
            "*/src/linebreak/*"
            "*/src/tests/*"
        )

        add_custom_target(doxygen)
        add_custom_target(docs DEPENDS doxygen)
        add_custom_target(doc DEPENDS doxygen)

        file(
            GLOB_RECURSE T4KCOMMON_DOC_INPUT
            CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/t4kcommon/include/*.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/t4kcommon/src/*.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/t4kcommon/src/*.h"
        )
        list(
            FILTER T4KCOMMON_DOC_INPUT
            EXCLUDE REGEX
            "/src/(linebreak|tests)/|/src/t4k_test\\.c$"
        )
        set(DOXYGEN_PROJECT_NAME "t4k_common")
        set(DOXYGEN_OUTPUT_DIRECTORY
            "${CMAKE_CURRENT_BINARY_DIR}/docs/t4kcommon"
        )
        doxygen_add_docs(
            docs-t4kcommon
            ${T4KCOMMON_DOC_INPUT}
            COMMENT "Generating t4k_common API documentation"
        )
        add_dependencies(doxygen docs-t4kcommon)

        file(
            GLOB_RECURSE TUXTYPE_DOC_INPUT
            CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/tuxtype/src/*.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/tuxtype/src/*.h"
        )
        list(FILTER TUXTYPE_DOC_INPUT EXCLUDE REGEX "/\\.#[^/]*$")
        set(DOXYGEN_PROJECT_NAME "tuxtype")
        set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs/tuxtype")
        doxygen_add_docs(
            docs-tuxtype
            ${TUXTYPE_DOC_INPUT}
            COMMENT "Generating tuxtype API documentation"
        )
        add_dependencies(doxygen docs-tuxtype)

        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tuxmath/src")
            file(
                GLOB_RECURSE TUXMATH_DOC_INPUT
                CONFIGURE_DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/tuxmath/src/*.c"
                "${CMAKE_CURRENT_SOURCE_DIR}/tuxmath/src/*.h"
            )
            set(DOXYGEN_PROJECT_NAME "tuxmath")
            set(DOXYGEN_OUTPUT_DIRECTORY
                "${CMAKE_CURRENT_BINARY_DIR}/docs/tuxmath"
            )
            doxygen_add_docs(
                docs-tuxmath
                ${TUXMATH_DOC_INPUT}
                COMMENT "Generating tuxmath API documentation"
            )
            add_dependencies(doxygen docs-tuxmath)
        endif()
    else()
        message(STATUS "Doxygen not found; documentation targets disabled")
        add_custom_target(
            doxygen
            COMMAND
                ${CMAKE_COMMAND} -E echo
                "Doxygen not found. Install doxygen to generate API documentation."
        )
        add_custom_target(docs DEPENDS doxygen)
        add_custom_target(doc DEPENDS doxygen)
    endif()
endif()

# Cross-compile runtime bundle: gather the SDL3*.dll's that CPM built
# (their own install rules are suppressed by CPM's EXCLUDE_FROM_ALL),
# plus libwinpthread-1.dll (mingw's pthread shim, dynamically linked
# by SDL3_ttf — the rest of mingw's runtime is static via
# -static-libgcc in the toolchain file).
include(GNUInstallDirs)

# When SDL3 family is CPM-fetched (vs. found via system find_package), each
# upstream's own install rules are gated on its <PROJECT>_INSTALL option,
# which defaults to PROJECT_IS_TOP_LEVEL — i.e. off when added via
# add_subdirectory. Force-enabling those triggers a chain of broken
# config-package exports (SDL3_ttf-shared depending on SDL3-shared which
# isn't in any export set, etc.). Just install the .so / .dylib / .dll
# files directly — that's all our binaries need at runtime.
foreach(_lib SDL3 SDL3_image SDL3_ttf SDL3_mixer)
    if(TARGET ${_lib}-shared)
        if(CMAKE_CROSSCOMPILING)
            # Windows: shared libs go in bin/ alongside .exe.
            install(
                FILES $<TARGET_FILE:${_lib}-shared>
                DESTINATION ${CMAKE_INSTALL_BINDIR}
            )
        else()
            # Linux/macOS: shared libs in lib/, with the SOVERSION
            # symlink chain. install(TARGETS) on a non-imported target
            # from another directory works since CMake 3.13.
            install(
                TARGETS ${_lib}-shared
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            )
        endif()
    endif()
endforeach()

# libwinpthread-1.dll: mingw's pthread shim, dynamically linked by
# SDL3_ttf. The rest of mingw's runtime is static via -static-libgcc
# in the toolchain file, so this is the only stray .dll we need.
if(CMAKE_CROSSCOMPILING)
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
