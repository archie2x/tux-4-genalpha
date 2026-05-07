# most systems use per-user cache locations. This attempts to set a
# SYS_CACHE_DIR variable to an appropriate value.

if(NOT DEFINED SYS_CACHE_DIR)
    if(XDG_CACHE_HOME)
        set(SYS_CACHE_DIR ${XDG_CACHE_HOME})
    elseif(DEFINED ENV{XDG_CACHE_HOME})
        set(SYS_CACHE_DIR "$ENV{XDG_CACHE_HOME}")
    elseif(CMAKE_HOST_WIN32)
        file(REAL_PATH ~/Cache SYS_CACHE_DIR EXPAND_TILDE)
    elseif(CMAKE_HOST_SYSTEM_NAME MATCHES "Darwin")
        set(SYS_CACHE_DIR $ENV{HOME}/Library/Caches)
    else()
        set(SYS_CACHE_DIR $ENV{HOME}/.cache)
    endif()
endif()
message(STATUS "SYS_CACHE_DIR=${SYS_CACHE_DIR}")
