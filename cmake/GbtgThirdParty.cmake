# Third-party runtime libraries (FFmpeg, DXC) ship in a thirdparty/ folder beside each executable that uses them,
# not loose beside it.
#
# Windows: the FFmpeg DLLs gbtg imports are delay-loaded, and the delay-load hook in
# src/engine/runtime/thirdparty_libraries.cpp loads them from thirdparty/ (their own imports of each other resolve
# there too). DXC is loaded by path (vk_shader_compiler.cpp).
# Linux: executables carry DT_RPATH $ORIGIN/thirdparty, which also applies to the FFmpeg libraries' dependencies on
# each other.
#
# The library sections of CMakeLists.txt fill these lists; gbtg_use_thirdparty(<target>) reads them, so call it after
# every library section has run.
set(GBTG_THIRDPARTY_DIR thirdparty)
# Files copied into thirdparty/ as they are.
set(GBTG_THIRDPARTY_FILES "")
# Windows: DLL names the executables import and delay-load.
set(GBTG_THIRDPARTY_DELAYLOAD "")
# Linux: directories whose lib*.so* are copied with their versioned symlinks (cp -P).
set(GBTG_THIRDPARTY_SHARED_LIBRARY_DIRS "")
set(GBTG_THIRDPARTY_HOOK_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../src/engine/runtime/thirdparty_libraries.cpp")
cmake_path(NORMAL_PATH GBTG_THIRDPARTY_HOOK_SOURCE)

function(gbtg_use_thirdparty target)
    set(dir "$<TARGET_FILE_DIR:${target}>/${GBTG_THIRDPARTY_DIR}")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${dir}")
    if(GBTG_THIRDPARTY_FILES)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GBTG_THIRDPARTY_FILES} "${dir}")
    endif()
    foreach(library_dir IN LISTS GBTG_THIRDPARTY_SHARED_LIBRARY_DIRS)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND sh -c "cp -P '${library_dir}'/lib*.so* '${dir}'/")
    endforeach()

    # The executable's own sources may already carry the hook (test targets copy gbtg's source list).
    get_target_property(sources ${target} SOURCES)
    set(has_hook FALSE)
    foreach(source IN LISTS sources)
        if(source MATCHES "runtime/thirdparty_libraries\\.cpp$")
            set(has_hook TRUE)
        endif()
    endforeach()
    if(NOT has_hook)
        target_sources(${target} PRIVATE "${GBTG_THIRDPARTY_HOOK_SOURCE}")
    endif()

    if(WIN32)
        if(GBTG_THIRDPARTY_DELAYLOAD)
            target_link_libraries(${target} PRIVATE delayimp)
            foreach(dll IN LISTS GBTG_THIRDPARTY_DELAYLOAD)
                target_link_options(${target} PRIVATE "/DELAYLOAD:${dll}")
            endforeach()
        endif()
    elseif(UNIX)
        # DT_RPATH, unlike DT_RUNPATH, also applies to the libraries' dependencies on each other.
        set_target_properties(${target} PROPERTIES
            INSTALL_RPATH "$ORIGIN/${GBTG_THIRDPARTY_DIR}" BUILD_WITH_INSTALL_RPATH ON)
        target_link_options(${target} PRIVATE "LINKER:--disable-new-dtags")
    endif()
endfunction()
