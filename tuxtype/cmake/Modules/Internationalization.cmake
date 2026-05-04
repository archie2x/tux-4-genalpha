#
# Gettext i18n support for C++ sources using _("...") translation markers.
# Each module maintains .po files (translations) in its po/ directory.
# At build time, .po files from all linked libraries are merged (msgcat)
# into a single .mo per executable's gettext domain.
#
# i18n_targets(<target>... [DOMAIN <name>] [PO_DIR <path>])
#   Libraries: tag with I18N_PO_DIR
#   Executables (DOMAIN required): merge .po from link tree → <domain>.mo
# i18n_check(<target>... TRANSLATIONS <locale> <string> ...)
# i18n_install(TARGETS ... COMPONENT ...)  — product install rules for .mo files
#
# Safe to include() multiple times — uses include_guard(GLOBAL).
#
include_guard(GLOBAL)

if(NOT DEFINED I18N_LANGUAGES)
    set(I18N_LANGUAGES "")
endif()

# Find the system's name for the UTF-8 C locale.
# RHEL/EL uses "C.utf8", Debian/Ubuntu uses "C.UTF-8".
# Falls back to plain "C" if neither is available.
# Sets I18N_UTF8_LOCALE in the caller's scope.
function(_i18n_find_utf8_locale)
    set(I18N_UTF8_LOCALE "C")
    execute_process(
        COMMAND locale -a
        OUTPUT_VARIABLE _locales
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_locales MATCHES "C\\.utf8")
        set(I18N_UTF8_LOCALE "C.utf8")
    elseif(_locales MATCHES "C\\.UTF-8")
        set(I18N_UTF8_LOCALE "C.UTF-8")
    endif()
    return(PROPAGATE I18N_UTF8_LOCALE)
endfunction()
_i18n_find_utf8_locale()

find_program(I18N_MSGCAT msgcat REQUIRED)
find_program(I18N_MSGFMT msgfmt REQUIRED)
find_program(I18N_MSGMERGE msgmerge REQUIRED)
find_program(I18N_XGETTEXT xgettext REQUIRED)

#
# _i18n_collect(<target> <po_dirs_var> <sources_var>)
#
# Recursively walk LINK_LIBRARIES. Collect I18N_PO_DIR properties and
# SOURCES (C/C++) from every target in the link tree.
#
function(_i18n_collect target po_dirs_var sources_var)
    set(_visited "")
    set(_po_dirs "")
    set(_sources "")
    _i18n_walk("${target}" _visited _po_dirs _sources)
    list(REMOVE_DUPLICATES _po_dirs)
    list(REMOVE_DUPLICATES _sources)
    set(${po_dirs_var} "${_po_dirs}" PARENT_SCOPE)
    set(${sources_var} "${_sources}" PARENT_SCOPE)
endfunction()

function(_i18n_walk target visited_var po_dirs_var sources_var)
    list(FIND ${visited_var} "${target}" _idx)
    if(NOT _idx EQUAL -1)
        return()
    endif()
    list(APPEND ${visited_var} "${target}")

    if(NOT TARGET ${target})
        set(${visited_var} "${${visited_var}}" PARENT_SCOPE)
        return()
    endif()

    # Collect I18N_PO_DIR if set
    get_target_property(_po_dir ${target} I18N_PO_DIR)
    if(_po_dir)
        list(APPEND ${po_dirs_var} "${_po_dir}")
    endif()

    # Collect C/C++ sources
    get_target_property(_srcs ${target} SOURCES)
    get_target_property(_src_dir ${target} SOURCE_DIR)
    if(_srcs)
        foreach(_s ${_srcs})
            if(NOT _s MATCHES "\\.(C|c|cpp|cxx|cc)$")
                continue()
            endif()
            if(NOT IS_ABSOLUTE "${_s}")
                set(_s "${_src_dir}/${_s}")
            endif()
            if(EXISTS "${_s}")
                list(APPEND ${sources_var} "${_s}")
            endif()
        endforeach()
    endif()

    # Recurse
    foreach(_prop LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_target_property(_libs ${target} ${_prop})
        if(_libs)
            foreach(_lib ${_libs})
                _i18n_walk(
                    "${_lib}"
                    ${visited_var}
                    ${po_dirs_var}
                    ${sources_var}
                )
            endforeach()
        endif()
    endforeach()

    set(${visited_var} "${${visited_var}}" PARENT_SCOPE)
    set(${po_dirs_var} "${${po_dirs_var}}" PARENT_SCOPE)
    set(${sources_var} "${${sources_var}}" PARENT_SCOPE)
endfunction()

#
# i18n_targets(<target>... [DOMAIN <name>] [PO_DIR <path>])
#
# Without DOMAIN (libraries):
#   Tags each target with I18N_PO_DIR from the current scope (or PO_DIR arg).
#
# With DOMAIN (executables):
#   Tags each target with the domain, then walks the first target's link graph
#   to collect PO_DIRs, merge .po files, and compile .mo.
#   Registers <domain>-update-i18n and update-i18n targets.
#
function(i18n_targets)
    if(NOT ENABLE_I18N)
        return()
    endif()
    cmake_parse_arguments(_TI "" "DOMAIN;PO_DIR" "" ${ARGN})
    set(_targets ${_TI_UNPARSED_ARGUMENTS})

    if(NOT _targets)
        message(FATAL_ERROR "i18n_targets(): no targets specified")
    endif()

    # --- No DOMAIN: library mode, tag each target with PO_DIR ---
    if(NOT DEFINED _TI_DOMAIN)
        foreach(_tgt ${_targets})
            if(DEFINED _TI_PO_DIR)
                set_target_properties(
                    ${_tgt}
                    PROPERTIES I18N_PO_DIR "${_TI_PO_DIR}"
                )
            elseif(DEFINED I18N_PO_DIR)
                set_target_properties(
                    ${_tgt}
                    PROPERTIES I18N_PO_DIR "${I18N_PO_DIR}"
                )
            endif()
        endforeach()
        return()
    endif()

    # --- DOMAIN set: executable mode ---
    set(_languages ${I18N_LANGUAGES})
    set(_bugs "icm_frqdev@icmanage.com")
    set(_workdir "${CMAKE_CURRENT_BINARY_DIR}/_i18n_${_TI_DOMAIN}")
    # pGettext looks for <exe>/../../share/locale at runtime.
    if(DEFINED BUILD_STAGE_DIR)
        set(_localedir "${BUILD_STAGE_DIR}/share/locale")
    else()
        set(_localedir "${CMAKE_BINARY_DIR}/share/locale")
    endif()
    file(MAKE_DIRECTORY "${_workdir}")

    # Tag all targets with domain and locale dir
    foreach(_tgt ${_targets})
        set_target_properties(
            ${_tgt}
            PROPERTIES
                I18N_DOMAIN "${_TI_DOMAIN}"
                I18N_LOCALEDIR "${_localedir}"
        )
        if(DEFINED _TI_PO_DIR)
            set_target_properties(
                ${_tgt}
                PROPERTIES I18N_PO_DIR "${_TI_PO_DIR}"
            )
        endif()
    endforeach()

    # Use the first target for the link-graph walk and build setup
    list(GET _targets 0 _primary)

    # Dedup by domain
    get_property(_done GLOBAL PROPERTY _I18N_DOMAIN_${_TI_DOMAIN})
    if(_done)
        return()
    endif()
    set_property(GLOBAL PROPERTY _I18N_DOMAIN_${_TI_DOMAIN} TRUE)

    # Walk link graph to collect PO_DIRs and sources
    _i18n_collect(${_primary} _po_dirs _all_sources)

    # Discover languages from .po files if not set
    if(NOT _languages)
        foreach(_pd ${_po_dirs})
            file(GLOB _pos "${_pd}/*.po")
            foreach(_po ${_pos})
                get_filename_component(_lang "${_po}" NAME_WE)
                list(APPEND _languages "${_lang}")
            endforeach()
        endforeach()
        list(REMOVE_DUPLICATES _languages)
    endif()
    if(NOT _languages)
        return()
    endif()

    # ------------------------------------------------------------------
    # ALL build: merge .po files from all PO_DIRs → .mo
    # ------------------------------------------------------------------
    set(_mo_files)
    foreach(_lang ${_languages})
        set(_modir "${_localedir}/${_lang}/LC_MESSAGES")
        set(_mo "${_modir}/${_TI_DOMAIN}.mo")
        set(_merged_po "${_workdir}/${_lang}.po")
        list(APPEND _mo_files "${_mo}")

        # Collect all .po files for this language from all PO_DIRs
        set(_lang_pos "")
        set(_lang_po_deps "")
        foreach(_pd ${_po_dirs})
            if(EXISTS "${_pd}/${_lang}.po")
                list(APPEND _lang_pos "${_pd}/${_lang}.po")
                list(APPEND _lang_po_deps "${_pd}/${_lang}.po")
            endif()
        endforeach()

        if(_lang_pos)
            file(MAKE_DIRECTORY "${_modir}")
            # msgcat merges multiple .po → one combined .po
            add_custom_command(
                OUTPUT "${_merged_po}"
                COMMAND
                    ${CMAKE_COMMAND} -E env LC_ALL=${I18N_UTF8_LOCALE}
                    ${I18N_MSGCAT} --no-location --sort-output --use-first -o
                    "${_merged_po}" ${_lang_pos}
                DEPENDS ${_lang_po_deps}
                COMMENT "${_TI_DOMAIN}: merge ${_lang} translations"
            )
            # msgfmt compiles merged .po → .mo
            add_custom_command(
                OUTPUT "${_mo}"
                COMMAND ${I18N_MSGFMT} -o "${_mo}" "${_merged_po}"
                COMMAND ${CMAKE_COMMAND} -E touch "${_mo}"
                DEPENDS "${_merged_po}"
            )
            install(
                FILES "${_mo}"
                DESTINATION share/locale/${_lang}/LC_MESSAGES
            )
        endif()
    endforeach()
    if(_mo_files)
        add_custom_target(${_TI_DOMAIN}-i18n ALL DEPENDS ${_mo_files})
    endif()

    # ------------------------------------------------------------------
    # Manual: update-i18n  (always available)
    #
    # For each PO_DIR: scan its sources → .pot → msgmerge into its .po
    # ------------------------------------------------------------------

    set(_all_stamps)

    # Per-PO_DIR update: each module updates its own .po files
    set(_podir_idx 0)
    foreach(_pd ${_po_dirs})
        # Find sources that belong to this PO_DIR's module
        # Use the PO_DIR's parent directories to match source paths
        get_filename_component(_module_root "${_pd}" DIRECTORY)
        set(_module_sources "")
        foreach(_f ${_all_sources})
            string(FIND "${_f}" "${_module_root}" _pos)
            if(_pos EQUAL 0)
                list(APPEND _module_sources "${_f}")
            endif()
        endforeach()

        if(NOT _module_sources)
            math(EXPR _podir_idx "${_podir_idx} + 1")
            continue()
        endif()

        # Write POTFILES.in for this module
        set(_potfiles_in "${_workdir}/POTFILES_${_podir_idx}.in")
        set(_potfiles_content "")
        foreach(_f ${_module_sources})
            file(RELATIVE_PATH _rel "${CMAKE_HOME_DIRECTORY}" "${_f}")
            string(APPEND _potfiles_content "${_rel}\n")
        endforeach()
        file(WRITE "${_potfiles_in}" "${_potfiles_content}")

        # xgettext → module .pot
        get_filename_component(_pd_name "${_pd}" DIRECTORY)
        get_filename_component(_pd_name "${_pd_name}" NAME)
        set(_pot "${_workdir}/${_pd_name}.pot")
        add_custom_command(
            OUTPUT "${_pot}"
            COMMAND
                ${I18N_XGETTEXT} --no-location --sort-output -o "${_pot}"
                --keyword=_ --keyword=N_ -s --add-comments
                "--package-name=${_pd_name}"
                "--package-version=${PROJECT_VERSION}"
                "--msgid-bugs-address=${_bugs}"
                --directory=${CMAKE_HOME_DIRECTORY}
                --files-from="${_potfiles_in}"
            COMMAND ${CMAKE_COMMAND} -E touch "${_pot}"
            DEPENDS ${_module_sources}
            COMMENT "${_TI_DOMAIN}: extract strings from ${_pd_name}"
            WORKING_DIRECTORY "${_workdir}"
        )

        # msgmerge → update this module's .po files
        foreach(_lang ${_languages})
            if(EXISTS "${_pd}/${_lang}.po")
                set(_stamp "${_workdir}/${_pd_name}_${_lang}.po.stamp")
                list(APPEND _all_stamps "${_stamp}")
                add_custom_command(
                    OUTPUT "${_stamp}"
                    COMMAND
                        ${I18N_MSGMERGE} --no-location --sort-output --update
                        "${_pd}/${_lang}.po" "${_pot}"
                    COMMAND ${CMAKE_COMMAND} -E touch "${_stamp}"
                    DEPENDS "${_pot}" "${_pd}/${_lang}.po"
                    COMMENT "${_TI_DOMAIN}: merge ${_pd_name}/${_lang}.po"
                )
            endif()
        endforeach()

        math(EXPR _podir_idx "${_podir_idx} + 1")
    endforeach()

    if(_all_stamps)
        add_custom_target(${_TI_DOMAIN}-update-i18n DEPENDS ${_all_stamps})
    else()
        add_custom_target(${_TI_DOMAIN}-update-i18n)
    endif()

    if(NOT TARGET update-i18n)
        add_custom_target(update-i18n)
    endif()
    add_dependencies(update-i18n ${_TI_DOMAIN}-update-i18n)
endfunction()

#
# i18n_check(<target>... TRANSLATIONS <locale> <string> [<locale> <string>] ...)
#
# POST_BUILD check: runs each target with --hello per locale and verifies the
# output contains the expected string.
#
# Example:
#   i18n_check(vp vpm vtrq
#       TRANSLATIONS
#       en "Hello, world!"
#       es "¡Hola, mundo!"
#       fr "Bonjour, le monde !"
#       ru "Привет, мир!"
#   )
#
function(i18n_check)
    if(NOT ENABLE_I18N)
        return()
    endif()
    if(DEFINED ENABLE_I18N_TESTS AND NOT ENABLE_I18N_TESTS)
        return()
    endif()

    # Discover which locales are available on this system
    execute_process(
        COMMAND locale -a
        OUTPUT_VARIABLE _available_locales
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # gersemi: hints { TRANSLATIONS: pairs }
    cmake_parse_arguments(_IC "" "" "TRANSLATIONS" ${ARGN})
    set(_targets ${_IC_UNPARSED_ARGUMENTS})
    set(_pairs ${_IC_TRANSLATIONS})

    list(LENGTH _pairs _len)
    foreach(_tgt ${_targets})
        # Ensure .mo files are built before the POST_BUILD check runs
        get_target_property(_domain ${_tgt} I18N_DOMAIN)
        if(_domain AND TARGET ${_domain}-i18n)
            add_dependencies(${_tgt} ${_domain}-i18n)
        endif()
        set(_i 0)
        while(_i LESS _len)
            list(GET _pairs ${_i} _locale)
            math(EXPR _i1 "${_i} + 1")
            list(GET _pairs ${_i1} _expected)
            math(EXPR _i "${_i} + 2")
            # Skip locales not installed on this system
            if(
                _locale STREQUAL "en"
                OR _available_locales MATCHES "${_locale}_"
            )
                add_custom_command(
                    TARGET ${_tgt}
                    POST_BUILD
                    COMMAND echo -n "*** i18n ${_tgt} ${_locale} --hello "
                    COMMAND
                        ${CMAKE_COMMAND} -E env LANG=${I18N_UTF8_LOCALE}
                        LANGUAGE=${_locale} -- sh -c
                        "$<TARGET_FILE:${_tgt}> --hello 2>/dev/null | grep -qF '${_expected}'"
                    COMMAND echo OK
                    VERBATIM
                )
            else()
                get_property(_warned GLOBAL PROPERTY _I18N_WARNED_${_locale})
                if(NOT _warned)
                    set_property(GLOBAL PROPERTY _I18N_WARNED_${_locale} TRUE)
                    message(
                        STATUS
                        "i18n_check: skipping ${_locale} (locale not installed)"
                    )
                endif()
            endif()
        endwhile()
    endforeach()
endfunction()

#
# i18n_install(TARGETS <target>... <install_args>...)
#
# Emit install() rules for .mo files belonging to the listed targets.
# Targets without I18N_DOMAIN are silently skipped.
# Duplicate domains are handled — each domain's .mo is installed once.
#
function(i18n_install)
    if(NOT ENABLE_I18N)
        return()
    endif()
    cmake_parse_arguments(_II "" "COMPONENT" "TARGETS" ${ARGN})

    set(_languages ${I18N_LANGUAGES})
    if(NOT _languages)
        return()
    endif()

    set(_seen_domains "")
    set(_extra_args)
    if(DEFINED _II_COMPONENT)
        list(APPEND _extra_args COMPONENT ${_II_COMPONENT})
    endif()
    list(APPEND _extra_args ${_II_UNPARSED_ARGUMENTS})

    foreach(_tgt ${_II_TARGETS})
        if(NOT TARGET ${_tgt})
            continue()
        endif()
        get_target_property(_domain ${_tgt} I18N_DOMAIN)
        if(NOT _domain)
            continue()
        endif()
        list(FIND _seen_domains "${_domain}" _idx)
        if(NOT _idx EQUAL -1)
            continue()
        endif()
        list(APPEND _seen_domains "${_domain}")

        get_target_property(_localedir ${_tgt} I18N_LOCALEDIR)
        foreach(_lang ${_languages})
            install(
                FILES "${_localedir}/${_lang}/LC_MESSAGES/${_domain}.mo"
                DESTINATION share/locale/${_lang}/LC_MESSAGES
                ${_extra_args}
            )
        endforeach()
    endforeach()
endfunction()
