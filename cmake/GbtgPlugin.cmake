# Builds a gbtg plugin library (docs/PLUGINS.md). Used by gbtg's own build and by plugin projects of their own:
#
#   cmake_minimum_required(VERSION 3.20)
#   project(my_plugin C CXX)
#   include(<gbtg checkout>/cmake/GbtgPlugin.cmake)
#   gbtg_add_plugin(my_plugin ID my_plugin SOURCES my_plugin.cpp PLUGIN_INI plugin.ini)
#
# A plugin project needs only this file and include/gbtg/plugin_api.h beside it as in gbtg's tree (cmake/ and
# include/ under one folder); it does not build gbtg. The library is named `target` with no "lib" prefix, since
# plugin.ini names it by that.
#
# The built plugin is laid out as gbtg reads it, in <output>/<id>/: the library, plugin.ini, and the contents of
# FILES_DIR (its loctext, AIDs and other disc files). <output> is, first that applies:
#   - OUTPUT_DIR;
#   - GBTG_PLUGIN_OUTPUT_DIR, e.g. a game's plugins folder: -DGBTG_PLUGIN_OUTPUT_DIR=C:/Games/gbtg/plugins;
#   - plugins/ beside gbtg, when gbtg is a target of the same build (and building gbtg builds the plugin);
#   - <build>/plugins.
# `cmake --install <build> --prefix <game folder>` installs it to <game folder>/plugins/<id>/.
include_guard(GLOBAL)

set(GBTG_PLUGIN_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../include" CACHE PATH
    "Folder holding gbtg/plugin_api.h")
set(GBTG_PLUGIN_OUTPUT_DIR "" CACHE PATH
    "Where built plugins are laid out, one folder each; empty: plugins/ beside gbtg, else <build>/plugins")
if(NOT EXISTS "${GBTG_PLUGIN_INCLUDE_DIR}/gbtg/plugin_api.h")
    message(FATAL_ERROR "gbtg/plugin_api.h is not in GBTG_PLUGIN_INCLUDE_DIR (${GBTG_PLUGIN_INCLUDE_DIR})")
endif()

function(gbtg_add_plugin target)
    cmake_parse_arguments(PLUGIN "" "ID;PLUGIN_INI;FILES_DIR;OUTPUT_DIR" "SOURCES" ${ARGN})
    if(NOT PLUGIN_ID)
        set(PLUGIN_ID "${target}")
    endif()
    if(NOT PLUGIN_SOURCES)
        message(FATAL_ERROR "gbtg_add_plugin(${target}): no SOURCES")
    endif()
    if(NOT PLUGIN_PLUGIN_INI)
        set(PLUGIN_PLUGIN_INI "${CMAKE_CURRENT_SOURCE_DIR}/plugin.ini")
    endif()
    get_filename_component(PLUGIN_PLUGIN_INI "${PLUGIN_PLUGIN_INI}" ABSOLUTE)
    if(NOT EXISTS "${PLUGIN_PLUGIN_INI}")
        message(FATAL_ERROR "gbtg_add_plugin(${target}): no plugin.ini at ${PLUGIN_PLUGIN_INI}")
    endif()

    add_library(${target} SHARED ${PLUGIN_SOURCES})
    target_include_directories(${target} PRIVATE "${GBTG_PLUGIN_INCLUDE_DIR}")
    # Only gbtg_plugin_load is exported (GBTG_PLUGIN_EXPORT).
    set_target_properties(${target} PROPERTIES PREFIX "" FOLDER "Plugins"
        C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W3 /utf-8 $<$<COMPILE_LANGUAGE:CXX>:/EHsc>)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    if(PLUGIN_OUTPUT_DIR)
        set(output "${PLUGIN_OUTPUT_DIR}")
    elseif(GBTG_PLUGIN_OUTPUT_DIR)
        set(output "${GBTG_PLUGIN_OUTPUT_DIR}")
    elseif(TARGET gbtg)
        set(output "$<TARGET_FILE_DIR:gbtg>/plugins")
        add_dependencies(gbtg ${target})
    else()
        set(output "${CMAKE_BINARY_DIR}/plugins")
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${output}/${PLUGIN_ID}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${target}>" "${PLUGIN_PLUGIN_INI}"
            "${output}/${PLUGIN_ID}"
        VERBATIM)
    install(TARGETS ${target} RUNTIME DESTINATION "plugins/${PLUGIN_ID}" LIBRARY DESTINATION "plugins/${PLUGIN_ID}")
    install(FILES "${PLUGIN_PLUGIN_INI}" DESTINATION "plugins/${PLUGIN_ID}")
    if(PLUGIN_FILES_DIR)
        get_filename_component(PLUGIN_FILES_DIR "${PLUGIN_FILES_DIR}" ABSOLUTE)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${PLUGIN_FILES_DIR}" "${output}/${PLUGIN_ID}"
            VERBATIM)
        install(DIRECTORY "${PLUGIN_FILES_DIR}/" DESTINATION "plugins/${PLUGIN_ID}")
    endif()
endfunction()
