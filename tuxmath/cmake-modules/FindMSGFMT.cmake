#
# Find the msgfmt program
#
# Defined variables:
#  MSGFMT_FOUND
#  MSGFMT_EXECUTABLE
#
# Macro:
#  ADD_TRANSLATIONS
#
# This requires that the variable LOCALE_INSTALL_DIR be set to the place you
# want to install the .mo files.

if(MSGFMT_EXECUTABLE)
    set(MSGFMT_FOUND TRUE)
else(MSGFMT_EXECUTABLE)
    find_program(
        MSGFMT_EXECUTABLE
        NAMES msgfmt gmsgfmt
        PATHS /bin /usr/bin /usr/local/bin /opt/local/bin
    )
    if(MSGFMT_EXECUTABLE)
        set(MSGFMT_FOUND TRUE)
    else(MSGFMT_EXECUTABLE)
        if(NOT MSGFMT_FIND_QUIETLY)
            if(MSGFMT_FIND_REQUIRED)
                message(FATAL_ERROR "msgfmt program couldn't be found")
            endif(MSGFMT_FIND_REQUIRED)
        endif(NOT MSGFMT_FIND_QUIETLY)
    endif(MSGFMT_EXECUTABLE)
    mark_as_advanced(MSGFMT_EXECUTABLE)
endif(MSGFMT_EXECUTABLE)

macro(ADD_TRANSLATIONS _baseName)
    set(_outputs)
    foreach(_file ${ARGN})
        get_filename_component(_file_we ${_file} NAME_WE)
        set(_out "${CMAKE_CURRENT_BINARY_DIR}/${_file_we}.gmo")
        set(_in "${CMAKE_CURRENT_SOURCE_DIR}/${_file_we}.po")
        add_custom_command(
            OUTPUT ${_out}
            COMMAND ${MSGFMT_EXECUTABLE} -o ${_out} ${_in}
            DEPENDS ${_in}
        )
        install(
            FILES ${_out}
            DESTINATION ${LOCALE_INSTALL_DIR}/${_file_we}/LC_MESSAGES/
            RENAME ${_baseName}.mo
        )
        set(_outputs ${_outputs} ${_out})
    endforeach(_file)
    add_custom_target(translations ALL DEPENDS ${_outputs})
endmacro(ADD_TRANSLATIONS)
