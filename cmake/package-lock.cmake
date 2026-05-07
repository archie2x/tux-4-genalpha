# Pinned versions of external dependencies. The provider in
# cmake/modules/t4k_provider.cmake calls CPMGetPackage(<name>) when
# find_package(<name>) can't satisfy a request from the system —
# at which point CPM uses these declarations to fetch the source.
#
# Intentionally NOT declared: Intl, Iconv, libunistring. No clean
# CPM-buildable upstream. find_package falls through to the system,
# fails honestly under cross-compile (where ENABLE_I18N is forced off).

cpmdeclarepackage(
    SDL3
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-3.4.8
)
cpmdeclarepackage(
    SDL3_image
    GITHUB_REPOSITORY libsdl-org/SDL_image
    GIT_TAG release-3.4.4
    OPTIONS
        # SDL3_image's heuristic doesn't auto-vendor on missing optional
        # codecs. Cross prefix doesn't ship avif / jxl / tiff / webp.
        "SDLIMAGE_AVIF OFF"
        "SDLIMAGE_JXL  OFF"
        "SDLIMAGE_TIF  OFF"
        "SDLIMAGE_WEBP OFF"
)
cpmdeclarepackage(
    SDL3_ttf
    GITHUB_REPOSITORY libsdl-org/SDL_ttf
    GIT_TAG release-3.2.2
    OPTIONS
        "SDLTTF_VENDORED ON" # bring our own freetype + harfbuzz
)
cpmdeclarepackage(
    SDL3_mixer
    GITHUB_REPOSITORY libsdl-org/SDL_mixer
    GIT_TAG release-3.2.0
    OPTIONS
        "SDLMIXER_VENDORED ON" # bring our own ogg + vorbis + flac
)
cpmdeclarepackage(
    LibXml2
    GITHUB_REPOSITORY GNOME/libxml2
    GIT_TAG v2.13.5
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "LIBXML2_WITH_PYTHON   OFF"
        "LIBXML2_WITH_TESTS    OFF"
        "LIBXML2_WITH_PROGRAMS OFF"
        "LIBXML2_WITH_LZMA     OFF"
        "LIBXML2_WITH_HTTP     OFF"
        "LIBXML2_WITH_ICONV    OFF"
        "LIBXML2_WITH_ZLIB     OFF"
        "LIBXML2_WITH_THREADS  OFF"
)
