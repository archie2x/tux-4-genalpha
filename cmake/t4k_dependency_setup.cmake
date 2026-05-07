# Loaded by CMake during project() initialization via the
# CMAKE_PROJECT_TOP_LEVEL_INCLUDES variable set in the root CMakeLists.txt.
# This is the only context where cmake_language(SET DEPENDENCY_PROVIDER)
# is permitted, so all the find_package()-redirection setup lives here.

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/modules")

# Pin CPM's source cache to the OS-native per-user cache dir so fetched
# packages survive `rm -rf _build` and are shared across CPM-using projects.
include(CacheDir)
set(CPM_SOURCE_CACHE "${SYS_CACHE_DIR}/CPM")
include(CPM)

# Declare pinned versions for everything CPM might fetch.
cpmusepackagelock("${CMAKE_CURRENT_LIST_DIR}/package-lock.cmake")

# Hook find_package() — see cmake/modules/t4k_provider.cmake.
include(t4k_provider)
