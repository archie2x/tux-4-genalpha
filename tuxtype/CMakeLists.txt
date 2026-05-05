cmake_minimum_required(VERSION 3.25)

# CMP0140: return(PROPAGATE ...) used by vendored Internationalization.cmake
if(POLICY CMP0140)
    cmake_policy(SET CMP0140 NEW)
endif()

project(tuxtype
    VERSION 1.8.3
    DESCRIPTION "Tux Typing — educational typing tutor (SDL3 port)"
    HOMEPAGE_URL "https://github.com/tux4kids/tuxtype"
    LANGUAGES C
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")
include(GNUInstallDirs)

# Bake the link-time library locations into the installed binary's rpath so
# `tuxtype` can find libt4k_common.dylib without DYLD_LIBRARY_PATH.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
# Also add the standard "next to the binary in /lib" location for portability.
if(APPLE)
    set(CMAKE_INSTALL_RPATH "@executable_path/../${CMAKE_INSTALL_LIBDIR}")
else()
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()

option(ENABLE_I18N "Build with gettext/i18n support" ON)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_image REQUIRED CONFIG)
find_package(SDL3_ttf REQUIRED CONFIG)
# SDL3_mixer is intentionally not used — audio is stubbed in t4k_common too.
# See task #13 for the real port.

# Either built as part of a parent super-project (add_subdirectory(t4kcommon)
# above us → target already defined) or against an installed t4kcommon
# (find_package looks up the CMake config-package).
if(NOT TARGET t4k_common::t4k_common)
    find_package(t4k_common REQUIRED CONFIG)
endif()

# Intl (gettext libintl) — Homebrew on macOS, glibc on Linux
find_package(Intl REQUIRED)
find_package(Iconv REQUIRED)

# ---------------------------------------------------------------------------
# Runtime path-prefix definitions baked into the binary
# ---------------------------------------------------------------------------
set(TUXTYPE_DATA_PREFIX "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}"
    CACHE PATH "Runtime data dir baked into tuxtype binary"
)
set(TUXTYPE_VAR_PREFIX "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/${PROJECT_NAME}"
    CACHE PATH "Runtime var (modifiable user data) dir"
)
set(TUXTYPE_CONF_PREFIX "${CMAKE_INSTALL_FULL_SYSCONFDIR}/${PROJECT_NAME}"
    CACHE PATH "Runtime config dir"
)
set(TUXTYPE_IM_PREFIX "${TUXTYPE_DATA_PREFIX}/input_methods"
    CACHE PATH "Input methods dir"
)

# ---------------------------------------------------------------------------
# tuxtype executable
# ---------------------------------------------------------------------------
add_executable(tuxtype
    src/alphabet.c
    src/audio.c
    src/braille.c
    src/convert_utf.c
    src/editor.c
    src/globals.c
    src/input_methods.c
    src/laser.c
    src/loaders.c
    src/main.c
    src/menu.c
    src/mysetenv.c
    src/options.c
    src/pause.c
    src/pixels.c
    src/playgame.c
    src/practice.c
    src/scandir.c
    src/scripting.c
    src/SDL_extras.c
    src/setup.c
    src/snow.c
    src/theme.c
    src/titlescreen.c
)

target_compile_definitions(tuxtype PRIVATE
    DATA_PREFIX="${TUXTYPE_DATA_PREFIX}"
    VAR_PREFIX="${TUXTYPE_VAR_PREFIX}"
    CONF_PREFIX="${TUXTYPE_CONF_PREFIX}"
    IM_PREFIX="${TUXTYPE_IM_PREFIX}"
    LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
    LOCALSTATEDIR="${TUXTYPE_VAR_PREFIX}"
    SYSCONFDIRDIR="${TUXTYPE_CONF_PREFIX}"
    IMDIR="${TUXTYPE_IM_PREFIX}"
    PACKAGE="${PROJECT_NAME}"
    VERSION="${PROJECT_VERSION}"
    NO_RAND_THEME=1
    ENABLE_NLS=1
)

target_link_libraries(tuxtype PRIVATE
    t4k_common::t4k_common
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    Intl::Intl
    Iconv::Iconv
    m
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tuxtype PRIVATE -Wall -Wno-unused -Wno-deprecated-declarations)
endif()

# ---------------------------------------------------------------------------
# i18n — auto-discovers t4k_common's po dir from the link graph too
# ---------------------------------------------------------------------------
if(ENABLE_I18N)
    include(Internationalization)
    i18n_targets(tuxtype DOMAIN tuxtype
        PO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/po"
    )
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS tuxtype RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# tuxtype's SetupPaths checks for these dirs at startup. They normally hold
# user-modifiable data + per-machine config, populated by the package manager
# or first run. For a relocatable install we just create them empty.
install(DIRECTORY DESTINATION ${CMAKE_INSTALL_LOCALSTATEDIR}/${PROJECT_NAME})
install(DIRECTORY DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME})

# Replace the 148 autotools Makefile.am files with a single recursive copy
# of the data tree. The runtime looks under DATA_PREFIX (= share/tuxtype).
install(DIRECTORY data/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}
    FILES_MATCHING
        PATTERN "*.png"
        PATTERN "*.svg"
        PATTERN "*.jpg"
        PATTERN "*.gif"
        PATTERN "*.wav"
        PATTERN "*.ogg"
        PATTERN "*.mp3"
        PATTERN "*.ttf"
        PATTERN "*.txt"
        PATTERN "*.xml"
        PATTERN "*.lst"
)

# Desktop entry + app icon (Linux)
install(FILES tuxtype.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES icon.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps
    RENAME ${PROJECT_NAME}.png
)

# Print summary so configure output is informative
message(STATUS "tuxtype configuration:")
message(STATUS "  data prefix : ${TUXTYPE_DATA_PREFIX}")
message(STATUS "  var prefix  : ${TUXTYPE_VAR_PREFIX}")
message(STATUS "  conf prefix : ${TUXTYPE_CONF_PREFIX}")
message(STATUS "  i18n        : ${ENABLE_I18N}")
message(STATUS "  t4k_common  : ${t4k_common_DIR}")
