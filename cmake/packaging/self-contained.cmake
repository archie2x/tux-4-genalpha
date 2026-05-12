# Self-contained install layout: binaries find their libs via rpath
# ($ORIGIN / @loader_path), data files via baked-in absolute paths
# under CMAKE_INSTALL_PREFIX. Default packaging variant — used by all
# presets that don't pick a different one.
#
# This module owns ALL install / RPATH / CPack concerns. Each
# submodule's CMakeLists.txt just builds its target; this file applies
# the install layout across them.

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ---------------------------------------------------------------------------
# RPATH — per target, so the build trees don't need it (LD_LIBRARY_PATH is
# fine during dev). INSTALL_RPATH takes effect only when CMake links the
# installed copy.
# ---------------------------------------------------------------------------
if(APPLE)
    set(_t4k_rpath_lib "@loader_path")
    set(_t4k_rpath_bin "@executable_path/../${CMAKE_INSTALL_LIBDIR}")
else()
    set(_t4k_rpath_lib "$ORIGIN")
    set(_t4k_rpath_bin "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()

set_target_properties(
    t4k_common
    PROPERTIES
        INSTALL_RPATH "${_t4k_rpath_lib}"
        INSTALL_RPATH_USE_LINK_PATH ON
)
foreach(_bin tuxtype tuxmath)
    if(TARGET ${_bin})
        set_target_properties(
            ${_bin}
            PROPERTIES
                INSTALL_RPATH "${_t4k_rpath_bin}"
                INSTALL_RPATH_USE_LINK_PATH ON
        )
    endif()
endforeach()

# All path resolution happens at runtime via SDL_GetBasePath() and
# SDL_GetPrefPath(). No baked-in directories.

# ---------------------------------------------------------------------------
# Install rules — library + headers + data + desktop entries.
# ---------------------------------------------------------------------------
install(
    TARGETS t4k_common
    EXPORT t4k_common-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/t4kcommon/include/t4k
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
    PATTERN "*.h"
)
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/t4kcommon/data/images/menu/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/t4k_common/images/menu
    FILES_MATCHING
    PATTERN "*.png"
    PATTERN "*.svg"
)
if(TARGET t4k_test)
    install(TARGETS t4k_test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Game data + binaries
set(_t4k_data_patterns
    PATTERN
    "*.png"
    PATTERN
    "*.svg"
    PATTERN
    "*.jpg"
    PATTERN
    "*.gif"
    PATTERN
    "*.wav"
    PATTERN
    "*.ogg"
    PATTERN
    "*.mp3"
    PATTERN
    "*.ttf"
    PATTERN
    "*.txt"
    PATTERN
    "*.xml"
    PATTERN
    "*.lst"
)

if(TARGET tuxtype)
    install(TARGETS tuxtype RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY DESTINATION ${CMAKE_INSTALL_LOCALSTATEDIR}/tuxtype)
    install(DIRECTORY DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/tuxtype)
    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tuxtype/data/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/tuxtype
        FILES_MATCHING ${_t4k_data_patterns}
    )
    install(
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/tuxtype/tuxtype.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    install(
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/tuxtype/icon.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps
        RENAME tuxtype.png
    )
endif()

if(TARGET tuxmath)
    install(TARGETS tuxmath RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY DESTINATION ${CMAKE_INSTALL_LOCALSTATEDIR}/tuxmath)
    install(DIRECTORY DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/tuxmath)
    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tuxmath/data/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/tuxmath
        FILES_MATCHING ${_t4k_data_patterns}
    )
    # Mission/lesson configs have no extension — separate rule.
    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tuxmath/data/missions
        DESTINATION ${CMAKE_INSTALL_DATADIR}/tuxmath
    )
endif()

# ---------------------------------------------------------------------------
# CMake config-package emission so downstream find_package(t4k_common)
# consumers (i.e. tuxtype/tuxmath when built standalone) work. Skip when
# any of our PUBLIC deps came from CPM (in-tree add_subdirectory targets
# aren't in any export set, so install(EXPORT) chokes). Also skip on
# Windows cross-compile — no devel-package layout there.
# ---------------------------------------------------------------------------
set(_t4k_inhibit_export FALSE)
foreach(
    _t
    IN
    ITEMS SDL3-shared SDL3_image-shared SDL3_ttf-shared SDL3_mixer-shared
)
    if(TARGET ${_t})
        set(_t4k_inhibit_export TRUE)
        break()
    endif()
endforeach()
if(NOT CMAKE_CROSSCOMPILING AND NOT _t4k_inhibit_export)
    install(
        EXPORT t4k_common-targets
        FILE t4k_common-targets.cmake
        NAMESPACE t4k_common::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/t4k_common
    )
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/t4kcommon/cmake/package/t4k_commonConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/t4k_commonConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/t4k_common
    )
    get_target_property(_t4k_ver t4k_common VERSION)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/t4k_commonConfigVersion.cmake"
        VERSION ${_t4k_ver}
        COMPATIBILITY SameMajorVersion
    )
    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/t4k_commonConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/t4k_commonConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/t4k_common
    )
endif()

# ---------------------------------------------------------------------------
# Bundle SDL3 family + winpthread for cross-compile / CPM builds. These
# upstream targets' own install rules are gated behind their <PROJECT>_INSTALL
# option, which defaults off for in-tree add_subdirectory. Just install the
# shared lib directly — that's all our binaries need at runtime.
# ---------------------------------------------------------------------------
foreach(_lib SDL3 SDL3_image SDL3_ttf SDL3_mixer)
    if(TARGET ${_lib}-shared)
        if(CMAKE_CROSSCOMPILING)
            install(
                FILES $<TARGET_FILE:${_lib}-shared>
                DESTINATION ${CMAKE_INSTALL_BINDIR}
            )
        else()
            install(
                TARGETS ${_lib}-shared
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            )
        endif()
    endif()
endforeach()

# libwinpthread-1.dll: mingw's pthread shim, dynamically linked by SDL3_ttf.
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
