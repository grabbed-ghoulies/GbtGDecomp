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
    src/engine/aid.cpp
    src/engine/entity.cpp
    src/engine/clock.cpp
    src/engine/game.cpp
    src/engine/platform.cpp
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
    target_link_libraries(gbtg PRIVATE d3d11 dxgi d3dcompiler user32)
else()
    target_sources(gbtg PRIVATE src/engine/gfx_null.cpp)
    target_compile_definitions(gbtg PRIVATE GBTG_GFX_NULL)
endif()
if(GBTG_WITH_SDL)
    target_compile_definitions(gbtg PRIVATE GBTG_WITH_SDL)
    target_link_libraries(gbtg PRIVATE SDL3::SDL3)
endif()

if(MSVC)
    target_compile_options(gbtg_lib PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(gbtg-catalog PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(gbtg-dump PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(gbtg-xemu-patch PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(gbtg PRIVATE /W3 /EHsc /utf-8)
endif()
