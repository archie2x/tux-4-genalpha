# Standalone CMake script invoked by the `dmg` custom target.
# Drives `cmake --install` into a temp prefix, then re-layouts that
# install tree into tuxtype.app + tuxmath.app, fixes install_names,
# ad-hoc-signs, and wraps in a .dmg.
#
# Required cache-vars (set by the caller via cmake -D):
#   SRC          - top-level source dir
#   BIN          - top-level build dir (CMAKE_BINARY_DIR)
#   VERSION      - project version string
#   PLIST_IN     - path to Info.plist.in template

cmake_minimum_required(VERSION 3.25)

foreach(_v SRC BIN VERSION PLIST_IN)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "build-dmg: missing -D${_v}=...")
    endif()
endforeach()

set(STAGE   "${BIN}/dmg-stage")     # .app bundles + .dmg source
set(WORK    "${BIN}/dmg-work")      # icon-build + other intermediates
set(INSTALL "${BIN}/dmg-install")   # cmake --install --prefix target
set(DMG_VOLUME "Tux4Kids Gen Alpha")
set(DMG_NAME   "tux-4-genalpha-${VERSION}-macos")
set(DMG_FILE   "${BIN}/${DMG_NAME}.dmg")

file(REMOVE_RECURSE "${STAGE}" "${WORK}" "${INSTALL}")
file(MAKE_DIRECTORY "${STAGE}" "${WORK}" "${INSTALL}")

# ---------------------------------------------------------------------------
# Stage the install tree. Single source of truth for which files ship —
# avoids re-listing data patterns / dylib lists here.
# ---------------------------------------------------------------------------
message(STATUS "build-dmg: cmake --install --prefix ${INSTALL}")
execute_process(
    COMMAND ${CMAKE_COMMAND} --install "${BIN}" --prefix "${INSTALL}"
    RESULT_VARIABLE _rc
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "cmake --install failed (${_rc})")
endif()

# ---------------------------------------------------------------------------
# Icon for tuxtype generated from icon.png (tuxmath ships its own .icns).
# ---------------------------------------------------------------------------
set(TUXMATH_ICNS  "${SRC}/tuxmath/data/images/icons/tuxmath.icns")
set(TUXTYPE_ICNS  "${WORK}/tuxtype.icns")
set(TUXTYPE_ICONSET "${WORK}/tuxtype.iconset")
file(MAKE_DIRECTORY "${TUXTYPE_ICONSET}")
foreach(_size IN ITEMS "16:icon_16x16" "32:icon_16x16@2x" "32:icon_32x32"
                       "64:icon_32x32@2x" "128:icon_128x128" "256:icon_128x128@2x"
                       "256:icon_256x256" "512:icon_256x256@2x"
                       "512:icon_512x512" "1024:icon_512x512@2x")
    string(REPLACE ":" ";" _pair "${_size}")
    list(GET _pair 0 _px)
    list(GET _pair 1 _name)
    execute_process(
        COMMAND sips -z ${_px} ${_px} "${SRC}/tuxtype/icon.png"
                --out "${TUXTYPE_ICONSET}/${_name}.png"
        OUTPUT_QUIET ERROR_QUIET
    )
endforeach()
execute_process(
    COMMAND iconutil -c icns "${TUXTYPE_ICONSET}" -o "${TUXTYPE_ICNS}"
    RESULT_VARIABLE _rc
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "iconutil failed (${_rc})")
endif()

# ---------------------------------------------------------------------------
# assemble_bundle(<appname> <icns>)
#
# Layout from the staged install tree:
#   <INSTALL>/bin/<app>           → Contents/MacOS/<app>
#   <INSTALL>/share/<app>/        → Contents/share/<app>/
#   <INSTALL>/share/t4k_common/   → Contents/share/t4k_common/
#   <INSTALL>/share/locale/       → Contents/share/locale/
# Frameworks/ populated by fixup_bundle below.
# ---------------------------------------------------------------------------
function(assemble_bundle app icns)
    set(BUNDLE   "${STAGE}/${app}.app")
    set(CONTENTS "${BUNDLE}/Contents")

    file(MAKE_DIRECTORY "${CONTENTS}/MacOS")
    file(MAKE_DIRECTORY "${CONTENTS}/Frameworks")
    file(MAKE_DIRECTORY "${CONTENTS}/Resources")
    file(MAKE_DIRECTORY "${CONTENTS}/share")

    file(COPY "${INSTALL}/bin/${app}"      DESTINATION "${CONTENTS}/MacOS")
    file(COPY "${INSTALL}/share/${app}"    DESTINATION "${CONTENTS}/share")
    file(COPY "${INSTALL}/share/t4k_common" DESTINATION "${CONTENTS}/share")
    if(IS_DIRECTORY "${INSTALL}/share/locale")
        file(COPY "${INSTALL}/share/locale" DESTINATION "${CONTENTS}/share")
    endif()

    get_filename_component(_icns_name "${icns}" NAME)
    file(COPY "${icns}" DESTINATION "${CONTENTS}/Resources")

    set(MACOSX_BUNDLE_EXECUTABLE_NAME      "${app}")
    set(MACOSX_BUNDLE_BUNDLE_NAME          "${app}")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER       "io.github.archie2x.${app}")
    set(MACOSX_BUNDLE_ICON_FILE            "${_icns_name}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION       "${VERSION}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${VERSION}")
    set(MACOSX_BUNDLE_COPYRIGHT
        "Copyright Tux4Kids contributors. GPL v3+.")
    # Template uses ${VAR} placeholders — no @ONLY.
    configure_file("${PLIST_IN}" "${CONTENTS}/Info.plist")
endfunction()

assemble_bundle(tuxtype "${TUXTYPE_ICNS}")
assemble_bundle(tuxmath "${TUXMATH_ICNS}")

# ---------------------------------------------------------------------------
# Dylib bundling: fixup_bundle copies SDL3 family + libt4k_common into
# Contents/Frameworks and rewrites install_names to @executable_path/../
# Frameworks/. Search includes the staged install tree's lib/ so the
# build-tree copies of t4k_common / bundled SDL3 (CPM) get picked up.
# ---------------------------------------------------------------------------
include(BundleUtilities)
set(BU_CHMOD_BUNDLE_ITEMS ON)
set(_search_dirs
    "${INSTALL}/lib"
    "/opt/homebrew/lib"
    "/usr/local/lib"
)

foreach(_app tuxtype tuxmath)
    message(STATUS "fixup_bundle: ${_app}.app")
    fixup_bundle("${STAGE}/${_app}.app" "" "${_search_dirs}")
endforeach()

# ---------------------------------------------------------------------------
# Ad-hoc code-sign each bundle. fixup_bundle's install_name_tool calls
# invalidate Apple's signatures on the modified Homebrew dylibs; modern
# macOS refuses to load dylibs whose signatures are invalid inside an
# unsigned app — manifests as a launch hang.
# ---------------------------------------------------------------------------
foreach(_app tuxtype tuxmath)
    message(STATUS "codesign --force --deep --sign - ${_app}.app")
    execute_process(
        COMMAND codesign --force --deep --sign - "${STAGE}/${_app}.app"
        RESULT_VARIABLE _rc
    )
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "codesign ${_app}.app failed (${_rc})")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Wrap with create-dmg — handles Applications-folder shortcut styling,
# icon positions, window layout. Pure hdiutil leaves the "Applications"
# symlink unstyled because there's no .DS_Store metadata.
# ---------------------------------------------------------------------------
if(EXISTS "${DMG_FILE}")
    file(REMOVE "${DMG_FILE}")
endif()

find_program(CREATE_DMG create-dmg)
if(NOT CREATE_DMG)
    message(
        FATAL_ERROR
        "create-dmg not found in PATH. Install with: brew install create-dmg"
    )
endif()

message(STATUS "build-dmg: create-dmg ${DMG_FILE}")
execute_process(
    COMMAND ${CREATE_DMG}
        --volname "${DMG_VOLUME}"
        --window-pos 200 120
        --window-size 640 360
        --icon-size 100
        --icon "tuxtype.app" 150 180
        --icon "tuxmath.app" 350 180
        --app-drop-link 500 180
        --hide-extension "tuxtype.app"
        --hide-extension "tuxmath.app"
        --no-internet-enable
        "${DMG_FILE}"
        "${STAGE}"
    RESULT_VARIABLE _rc
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "create-dmg failed (${_rc})")
endif()

message(STATUS "build-dmg: produced ${DMG_FILE}")
