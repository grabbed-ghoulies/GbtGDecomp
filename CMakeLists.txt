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

option(GBTG_WITH_SDL "Fetch SDL3 and build an interactive engine window" ON)

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
    src/gbtg/wavebank.cpp
    src/gbtg/xbe.cpp
    src/gbtg/catalog.cpp
    src/gbtg/scene.cpp
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

add_executable(gbtg src/engine/main.cpp)
target_link_libraries(gbtg PRIVATE gbtg_lib)
if(GBTG_WITH_SDL)
    target_compile_definitions(gbtg PRIVATE GBTG_WITH_SDL)
    target_link_libraries(gbtg PRIVATE SDL3::SDL3)
endif()

if(MSVC)
    target_compile_options(gbtg_lib PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(gbtg-catalog PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(gbtg-dump PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(gbtg PRIVATE /W3 /EHsc /utf-8)
endif()
