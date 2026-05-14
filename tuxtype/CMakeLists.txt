cmake_minimum_required(VERSION 3.25)

# CMP0140: return(PROPAGATE ...) used by vendored Internationalization.cmake
if(POLICY CMP0140)
    cmake_policy(SET CMP0140 NEW)
endif()

project(
    tuxtype
    VERSION 1.8.3
    DESCRIPTION "Tux Typing — educational typing tutor (SDL3 port)"
    HOMEPAGE_URL "https://github.com/tux4kids/tuxtype"
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
# tuxtype executable
# ---------------------------------------------------------------------------
add_executable(
    tuxtype
    src/alphabet.c
    src/audio.c
    src/braille.c
    src/editor.c
    src/globals.c
    src/hand_display.c
    src/input.c
    src/input_braille.c
    src/input_typewriter.c
    src/keyboard_display.c
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
    src/scripting.c
    src/SDL_extras.c
    src/setup.c
    src/settings_volume.c
    src/snow.c
    src/target_hint.c
    src/theme.c
    src/titlescreen.c
)

# Build-time-only compile defs. Install-path defines (DATA_PREFIX et al.) are
# added by the active packaging module (cmake/packaging/).
target_compile_definitions(
    tuxtype
    PRIVATE
        PACKAGE="${PROJECT_NAME}"
        VERSION="${PROJECT_VERSION}"
        NO_RAND_THEME=1
        $<$<BOOL:${ENABLE_I18N}>:ENABLE_NLS=1>
)

target_link_libraries(tuxtype PRIVATE t4k_common::t4k_common m)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tuxtype PRIVATE -Wall -Werror)
endif()
# Suppress paranoia warnings — see t4kcommon/CMakeLists.txt for the why.
include(CheckCCompilerFlag)
foreach(_w format-truncation format-overflow stringop-truncation)
    string(MAKE_C_IDENTIFIER "_HAS_NO_${_w}" _var)
    check_c_compiler_flag("-Wno-${_w}" ${_var})
    if(${_var})
        target_compile_options(tuxtype PRIVATE "-Wno-${_w}")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# i18n — auto-discovers t4k_common's po dir from the link graph too
# ---------------------------------------------------------------------------
if(ENABLE_I18N)
    include(Internationalization)
    i18n_targets(tuxtype DOMAIN tuxtype PO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/po")
endif()
