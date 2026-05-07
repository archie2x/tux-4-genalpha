# Dependency provider for the tux4kids super-project.
#
# Hooks find_package() so callsites can be uniform plain CMake — a
# `find_package(<pkg> REQUIRED)` succeeds whether <pkg> is:
#   1. already a target in the build (super-project add_subdirectory)
#   2. installed on the system (apt / brew / etc.)
#   3. needs fetching via CPM (declared in cmake/package-lock.cmake)
#
# Requires CMake 3.24+ and that include(CPM) + CPMUsePackageLock()
# have run before this module is included.

if(CMAKE_VERSION VERSION_LESS 3.24)
    message(
        FATAL_ERROR
        "t4k_provider needs CMake 3.24+ for cmake_language(SET DEPENDENCY_PROVIDER)"
    )
endif()

function(t4k_dependency_provider method package_name)
    if(NOT method STREQUAL "FIND_PACKAGE")
        return()
    endif()

    # 1. Already a target in our build? (super-project add_subdirectory)
    if(TARGET ${package_name}::${package_name} OR TARGET ${package_name})
        set(${package_name}_FOUND TRUE PARENT_SCOPE)
        return()
    endif()

    # 2. Only intercept packages we've declared in package-lock.cmake.
    #    Returning here lets CMake fall through to its default find_package
    #    flow — which is critical for "infrastructure" packages like
    #    PkgConfig and Threads: their internal state (PKG_CONFIG_VERSION,
    #    etc.) has to land in the *caller's* scope, not in this function's.
    if(NOT DEFINED CPM_DECLARATION_${package_name})
        return()
    endif()

    # 3. Try the system. BYPASS_PROVIDER avoids re-entering us.
    #    Strip REQUIRED — failure here is fine, we'll fall back to CPM.
    set(_args ${ARGN})
    list(REMOVE_ITEM _args REQUIRED)
    find_package(${package_name} ${_args} BYPASS_PROVIDER QUIET)
    if(${package_name}_FOUND)
        set(${package_name}_FOUND TRUE PARENT_SCOPE)
        return()
    endif()

    # 4. CPM fallback. After CPMGetPackage, the package's CMakeLists has
    #    been processed (or it would have errored). Whatever target name
    #    upstream creates is now in our build graph — consumers find it
    #    via the alias they target_link_libraries against.
    CPMGetPackage(${package_name})
    set(${package_name}_FOUND TRUE PARENT_SCOPE)
endfunction()

cmake_language(
    SET_DEPENDENCY_PROVIDER t4k_dependency_provider
    SUPPORTED_METHODS FIND_PACKAGE
)
