cmake_minimum_required(VERSION 3.20)
project(GbtGDecomp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v1.3.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(zlib)

option(GBTG_WITH_SDL "Fetch SDL3 for a windowed tick loop (optional)" OFF)

if(GBTG_WITH_SDL)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.20
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

add_library(gbtg_lib STATIC
    src/gbtg/io.cpp
    src/gbtg/resource.cpp
    src/gbtg/bundle.cpp
    src/gbtg/texture.cpp
    src/gbtg/model.cpp
    src/gbtg/loctext.cpp
    src/gbtg/font.cpp
    src/gbtg/wavebank.cpp
    src/gbtg/xbe.cpp
    src/gbtg/catalog.cpp
    src/gbtg/script.cpp
    src/gbtg/cutscene.cpp
    src/gbtg/anim.cpp
)
target_include_directories(gbtg_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
if(TARGET zlibstatic)
    target_link_libraries(gbtg_lib PUBLIC zlibstatic)
else()
    target_link_libraries(gbtg_lib PUBLIC zlib)
endif()

add_executable(gbtg-catalog src/tools/catalog_main.cpp)
target_link_libraries(gbtg-catalog PRIVATE gbtg_lib)

add_executable(gbtg-dump src/tools/dump_main.cpp)
target_link_libraries(gbtg-dump PRIVATE gbtg_lib)

add_executable(gbtg-xemu-patch src/tools/xemu_patch_main.cpp)
target_link_libraries(gbtg-xemu-patch PRIVATE gbtg_lib)

add_executable(gbtg
    src/engine/main.cpp
    src/engine/host_config.cpp
    src/engine/aid.cpp
    src/engine/entity.cpp
    src/engine/clock.cpp
    src/engine/audio.cpp
    src/engine/pcm_out.cpp
    src/engine/frontend.cpp
    src/engine/scene.cpp
    src/engine/game.cpp
    src/engine/platform.cpp
    src/engine/movie.cpp
)
target_link_libraries(gbtg PRIVATE gbtg_lib)
if(WIN32)
    set(_gbtg_gfx_default d3d11)
else()
    set(_gbtg_gfx_default null)
endif()
set(GBTG_GFX "${_gbtg_gfx_default}" CACHE STRING "Graphics backend: null or d3d11")
if(GBTG_GFX STREQUAL "d3d11")
    target_sources(gbtg PRIVATE src/engine/gfx_d3d11.cpp)
    target_compile_definitions(gbtg PRIVATE GBTG_GFX_D3D11)
    target_link_libraries(gbtg PRIVATE d3d11 dxgi d3dcompiler user32 xinput xaudio2 ole32)

    # Dear ImGui is compiled into Debug gbtg only. Release, RelWithDebInfo, and the
    # null backend never see GBTG_DEBUG_UI. Populate skips add_subdirectory: upstream
    # has no library target that includes the Win32 and D3D11 backends.
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.9b
        GIT_SHALLOW TRUE
    )
    # CMP0169 wants MakeAvailable, which add_subdirectory's a CMakeLists.txt this
    # tag does not have. Populate only downloads the sources we compile ourselves.
    if(POLICY CMP0169)
        cmake_policy(PUSH)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_GetProperties(imgui)
    if(NOT imgui_POPULATED)
        FetchContent_Populate(imgui)
    endif()
    if(POLICY CMP0169)
        cmake_policy(POP)
    endif()
    set(GBTG_IMGUI_SOURCES
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp"
        "${CMAKE_SOURCE_DIR}/src/engine/debug_ui.cpp"
    )
    target_sources(gbtg PRIVATE "$<$<CONFIG:Debug>:${GBTG_IMGUI_SOURCES}>")
    target_compile_definitions(gbtg PRIVATE
        $<$<CONFIG:Debug>:GBTG_DEBUG_UI>
        $<$<CONFIG:Debug>:IMGUI_IMPL_WIN32_DISABLE_GAMEPAD>
    )
    target_include_directories(gbtg PRIVATE
        $<$<CONFIG:Debug>:${imgui_SOURCE_DIR}>
        $<$<CONFIG:Debug>:${imgui_SOURCE_DIR}/backends>
    )
    target_link_libraries(gbtg PRIVATE $<$<CONFIG:Debug>:imm32>)
else()
    target_sources(gbtg PRIVATE src/engine/gfx_null.cpp)
    target_compile_definitions(gbtg PRIVATE GBTG_GFX_NULL)
endif()
if(GBTG_WITH_SDL)
    target_compile_definitions(gbtg PRIVATE GBTG_WITH_SDL)
    target_link_libraries(gbtg PRIVATE SDL3::SDL3)
endif()

# LGPL FFmpeg supplies the Bink decoder. The RAD SDK is not linked. DLLs are copied
# next to gbtg.exe so running the exe does not need a separate install.
if(WIN32)
    set(GBTG_FFMPEG_URL "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n8.1-latest-win64-lgpl-shared-8.1.zip")
    if(EXISTS "${CMAKE_SOURCE_DIR}/cache/ffmpeg-lgpl.zip")
        set(GBTG_FFMPEG_URL "${CMAKE_SOURCE_DIR}/cache/ffmpeg-lgpl.zip")
    endif()
    FetchContent_Declare(
        ffmpeg_lgpl
        URL "${GBTG_FFMPEG_URL}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(ffmpeg_lgpl)
    file(GLOB_RECURSE GBTG_FFMPEG_LIBS "${ffmpeg_lgpl_SOURCE_DIR}/*.lib")
    set(GBTG_FFMPEG_AVCODEC "")
    foreach(lib ${GBTG_FFMPEG_LIBS})
        if(lib MATCHES "avcodec\\.lib$")
            set(GBTG_FFMPEG_AVCODEC "${lib}")
            break()
        endif()
    endforeach()
    if(GBTG_FFMPEG_AVCODEC STREQUAL "")
        message(FATAL_ERROR "FFmpeg avcodec.lib was not found under ${ffmpeg_lgpl_SOURCE_DIR}")
    endif()
    cmake_path(GET GBTG_FFMPEG_AVCODEC PARENT_PATH GBTG_FFMPEG_LIBDIR)
    cmake_path(GET GBTG_FFMPEG_LIBDIR PARENT_PATH GBTG_FFMPEG_ROOT)
    target_include_directories(gbtg PRIVATE "${GBTG_FFMPEG_ROOT}/include")
    target_compile_definitions(gbtg PRIVATE GBTG_HAS_FFMPEG)
    target_link_libraries(gbtg PRIVATE
        "${GBTG_FFMPEG_LIBDIR}/avformat.lib"
        "${GBTG_FFMPEG_LIBDIR}/avcodec.lib"
        "${GBTG_FFMPEG_LIBDIR}/swscale.lib"
        "${GBTG_FFMPEG_LIBDIR}/swresample.lib"
        "${GBTG_FFMPEG_LIBDIR}/avutil.lib"
    )
    file(GLOB GBTG_FFMPEG_DLLS "${GBTG_FFMPEG_ROOT}/bin/*.dll")
    foreach(dll ${GBTG_FFMPEG_DLLS})
        add_custom_command(TARGET gbtg POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "$<TARGET_FILE_DIR:gbtg>"
        )
    endforeach()
endif()

if(MSVC)
    target_compile_options(gbtg_lib PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(gbtg-catalog PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(gbtg-dump PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(gbtg-xemu-patch PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(gbtg PRIVATE /W3 /EHsc /utf-8)
endif()
