# Example gbtg plugin: a feature library in C that logs each scene script the game loads (docs/PLUGINS.md).
#
# It is a project of its own, so this folder can start a plugin repository. gbtg's build also builds it
# (GBTG_BUILD_PLUGIN_EXAMPLES). On its own:
#
#   cmake -S examples/plugins/scene_log -B build-scene-log
#   cmake --build build-scene-log --config Release
#   cmake --install build-scene-log --config Release --prefix <folder holding gbtg.exe>
#
# Outside gbtg's tree, set GBTG_SDK_DIR to a gbtg checkout, or to a folder with copies of its cmake/GbtgPlugin.cmake
# and include/gbtg/plugin_api.h. Set GBTG_PLUGIN_OUTPUT_DIR to a game's plugins folder to build straight into it.
cmake_minimum_required(VERSION 3.20)
project(gbtg_scene_log LANGUAGES C)

set(GBTG_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../.." CACHE PATH
    "Folder holding cmake/GbtgPlugin.cmake and include/gbtg/plugin_api.h")
include("${GBTG_SDK_DIR}/cmake/GbtgPlugin.cmake")

gbtg_add_plugin(gbtg_scene_log ID scene_log SOURCES scene_log_plugin.c PLUGIN_INI plugin.ini)
