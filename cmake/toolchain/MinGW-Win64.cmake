# Cross-compile to Windows x86_64 from POSIX (macOS or Linux) via mingw-w64.
#
# Prereqs:
#   brew install mingw-w64                    # macOS
#   apt install gcc-mingw-w64-x86-64          # Debian / Ubuntu
#
# SDL3 Windows binaries are not in mingw-w64 — point CMake at an unpacked
# SDL3-devel-*-mingw.tar.gz tree via SDL3_MINGW_ROOT, e.g.:
#   cmake --preset MinGW-Win64 -DSDL3_MINGW_ROOT=$HOME/sdl3-mingw

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(_cross x86_64-w64-mingw32)
set(CMAKE_C_COMPILER ${_cross}-gcc)
set(CMAKE_CXX_COMPILER ${_cross}-g++)
set(CMAKE_RC_COMPILER ${_cross}-windres)

# Static-link gcc/stdc++ runtimes so the .exe works without bundling
# libgcc_s_seh-1.dll / libstdc++-6.dll / libwinpthread-1.dll.
set(CMAKE_C_FLAGS_INIT "-static-libgcc")
set(CMAKE_CXX_FLAGS_INIT "-static-libgcc -static-libstdc++")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++ -static")

# Search paths: mingw runtime/headers + the SDL3 mingw drop the user points at.
set(CMAKE_FIND_ROOT_PATH
    /opt/homebrew/opt/mingw-w64
    /usr/${_cross} # Linux mingw layout
    ${SDL3_MINGW_ROOT}
)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# SDL3 ships its CMake config files under <root>/x86_64-w64-mingw32/lib/cmake/
# — make sure find_package finds them.
if(SDL3_MINGW_ROOT)
    list(APPEND CMAKE_PREFIX_PATH "${SDL3_MINGW_ROOT}/${_cross}")
endif()
