# macOS .dmg packaging — produces tuxtype.app + tuxmath.app inside one
# tux-4-genalpha-<version>-macos.dmg.
#
# Deliberately does NOT use CMake's MACOSX_BUNDLE target property. The
# build targets stay platform-agnostic and produce plain binaries; the
# `dmg` custom target stages a full `cmake --install` tree and re-layouts
# it into .app bundles via cmake/packaging/macos-app/build-dmg.cmake.
#
# Usage:
#   cmake --preset Release
#   cmake --build _build/Darwin/Release --target dmg

if(NOT APPLE)
    return()
endif()

set(_t4k_dmg_script
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/packaging/macos-app/build-dmg.cmake"
)
set(_t4k_dmg_plist_in
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/packaging/macos-app/Info.plist.in"
)

# `--target dmg` skips ALL, so list every target referenced by an install
# rule (binaries, library, smoke-test) plus i18n custom targets that
# produce .mo files.
set(_t4k_dmg_deps tuxtype tuxmath t4k_common)
foreach(_t t4k_test tuxtype-i18n tuxmath-i18n t4k_common-i18n)
    if(TARGET ${_t})
        list(APPEND _t4k_dmg_deps ${_t})
    endif()
endforeach()

add_custom_target(
    dmg
    COMMAND
        ${CMAKE_COMMAND}
            -D SRC=${CMAKE_CURRENT_SOURCE_DIR}
            -D BIN=${CMAKE_BINARY_DIR}
            -D VERSION=${CMAKE_PROJECT_VERSION}
            -D PLIST_IN=${_t4k_dmg_plist_in}
            -P ${_t4k_dmg_script}
    DEPENDS ${_t4k_dmg_deps}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Assembling .app bundles and building .dmg"
    VERBATIM
)
