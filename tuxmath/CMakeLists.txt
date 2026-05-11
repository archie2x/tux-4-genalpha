cmake_minimum_required(VERSION 3.25)

# CMP0140: return(PROPAGATE ...) used by vendored Internationalization.cmake
if(POLICY CMP0140)
    cmake_policy(SET CMP0140 NEW)
endif()

project(
    tuxmath
    VERSION 2.0.4
    DESCRIPTION "Tux of Math Command — educational math game (SDL3 port)"
    HOMEPAGE_URL "https://github.com/tux4kids/tuxmath"
    LANGUAGES C
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
include(CMakeDependentOption)

# Force off when cross-compiling — mingw cross prefix has no libintl/libiconv.
cmake_dependent_option(
    ENABLE_I18N
    "Build with gettext/i18n support"
    ON
    "NOT CMAKE_CROSSCOMPILING"
    OFF
)

# ---------------------------------------------------------------------------
# Dependencies — SDL3 / Intl / Iconv inherit transitively from t4k_common.
# ---------------------------------------------------------------------------
find_package(t4k_common REQUIRED)

# ---------------------------------------------------------------------------
# tuxmath executable — networking (SDL_net) and the admin tool dropped for
# the SDL3 port. Bring them back as separate targets if anyone wants them.
# ---------------------------------------------------------------------------
add_executable(
    tuxmath
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

# Build-time-only compile defs. Install-path defines (DATA_PREFIX et al.) are
# added by the active packaging module (cmake/packaging/).
target_compile_definitions(
    tuxmath
    PRIVATE
        PACKAGE="${PROJECT_NAME}"
        VERSION="${PROJECT_VERSION}"
        $<$<BOOL:${ENABLE_I18N}>:ENABLE_NLS=1>
        HAVE_LIBT4K_COMMON=1
)

target_link_libraries(tuxmath PRIVATE t4k_common::t4k_common m)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tuxmath PRIVATE -Wall -Werror)
endif()
# Suppress paranoia warnings — see t4kcommon/CMakeLists.txt for the why.
include(CheckCCompilerFlag)
foreach(_w format-truncation format-overflow stringop-truncation)
    string(MAKE_C_IDENTIFIER "_HAS_NO_${_w}" _var)
    check_c_compiler_flag("-Wno-${_w}" ${_var})
    if(${_var})
        target_compile_options(tuxmath PRIVATE "-Wno-${_w}")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# i18n
# ---------------------------------------------------------------------------
if(ENABLE_I18N)
    include(Internationalization OPTIONAL RESULT_VARIABLE _i18n_found)
    if(_i18n_found)
        i18n_targets(
            tuxmath
            DOMAIN tuxmath
            PO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/po"
        )
    else()
        message(
            WARNING
            "Internationalization.cmake not found — translations disabled"
        )
    endif()
endif()
