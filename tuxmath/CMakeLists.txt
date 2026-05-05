cmake_minimum_required(VERSION 3.25)

# CMP0140: return(PROPAGATE ...) used by vendored Internationalization.cmake
if(POLICY CMP0140)
    cmake_policy(SET CMP0140 NEW)
endif()

project(tuxmath
    VERSION 2.0.4
    DESCRIPTION "Tux of Math Command — educational math game (SDL3 port)"
    HOMEPAGE_URL "https://github.com/tux4kids/tuxmath"
    LANGUAGES C
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")
include(GNUInstallDirs)

# Bake link-time library locations into the installed binary's rpath so
# tuxmath can find libt4k_common.dylib without DYLD_LIBRARY_PATH.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
if(APPLE)
    set(CMAKE_INSTALL_RPATH "@executable_path/../${CMAKE_INSTALL_LIBDIR}")
else()
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()

option(ENABLE_I18N "Build with gettext/i18n support" ON)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(SDL3       REQUIRED CONFIG)
find_package(SDL3_image REQUIRED CONFIG)
find_package(SDL3_ttf   REQUIRED CONFIG)
find_package(SDL3_mixer REQUIRED CONFIG)

# Either built as part of a parent super-project (add_subdirectory(t4kcommon)
# above us → target already defined) or against an installed t4kcommon.
if(NOT TARGET t4k_common::t4k_common)
    find_package(t4k_common REQUIRED CONFIG)
endif()

find_package(Intl  REQUIRED)
find_package(Iconv REQUIRED)

# ---------------------------------------------------------------------------
# Runtime path-prefix definitions baked into the binary
# ---------------------------------------------------------------------------
set(TUXMATH_DATA_PREFIX "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}"
    CACHE PATH "Runtime data dir baked into tuxmath binary"
)
set(TUXMATH_VAR_PREFIX "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/${PROJECT_NAME}"
    CACHE PATH "Runtime var (modifiable user data) dir"
)
set(TUXMATH_CONF_PREFIX "${CMAKE_INSTALL_FULL_SYSCONFDIR}/${PROJECT_NAME}"
    CACHE PATH "Runtime config dir"
)

# ---------------------------------------------------------------------------
# tuxmath executable — networking (SDL_net) and the admin tool dropped for
# the SDL3 port. Bring them back as separate targets if anyone wants them.
# ---------------------------------------------------------------------------
add_executable(tuxmath
    src/audio.c
    src/campaign.c
    src/comets.c
    src/comets_graphics.c
    src/credits.c
    src/draw_utils.c
    src/factoroids.c
    src/factoroids_graphics.c
    src/fileops.c
    src/fileops_media.c
    src/frame_counter.c
    src/game.c
    src/highscore.c
    src/lessons.c
    src/linewrap.c
    src/mathcards.c
    src/menu.c
    src/multiplayer.c
    src/mysetenv.c
    src/options.c
    src/sdl3_compat.c
    src/setup.c
    src/titlescreen.c
    src/tuxmath.c
)

target_compile_definitions(tuxmath PRIVATE
    DATA_PREFIX="${TUXMATH_DATA_PREFIX}"
    VAR_PREFIX="${TUXMATH_VAR_PREFIX}"
    CONF_PREFIX="${TUXMATH_CONF_PREFIX}"
    LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
    PACKAGE="${PROJECT_NAME}"
    VERSION="${PROJECT_VERSION}"
    ENABLE_NLS=1
    HAVE_LIBT4K_COMMON=1
    HAVE_SCANDIR=1
    HAVE_ALPHASORT=1
)

target_link_libraries(tuxmath PRIVATE
    t4k_common::t4k_common
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    SDL3_mixer::SDL3_mixer
    Intl::Intl
    Iconv::Iconv
    m
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tuxmath PRIVATE -Wall -Wno-unused -Wno-deprecated-declarations)
endif()

# ---------------------------------------------------------------------------
# i18n
# ---------------------------------------------------------------------------
if(ENABLE_I18N)
    include(Internationalization OPTIONAL RESULT_VARIABLE _i18n_found)
    if(_i18n_found)
        i18n_targets(tuxmath DOMAIN tuxmath
            PO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/po"
        )
    else()
        message(WARNING "Internationalization.cmake not found — translations disabled")
    endif()
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS tuxmath RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY DESTINATION ${CMAKE_INSTALL_LOCALSTATEDIR}/${PROJECT_NAME})
install(DIRECTORY DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME})

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

message(STATUS "tuxmath configuration:")
message(STATUS "  data prefix : ${TUXMATH_DATA_PREFIX}")
message(STATUS "  var prefix  : ${TUXMATH_VAR_PREFIX}")
message(STATUS "  conf prefix : ${TUXMATH_CONF_PREFIX}")
message(STATUS "  i18n        : ${ENABLE_I18N}")
message(STATUS "  t4k_common  : ${t4k_common_DIR}")
