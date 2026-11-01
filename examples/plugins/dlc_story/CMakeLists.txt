# Example gbtg plugin: a data plugin with no library. It adds a "DLC Story" entry to game select's Features menu
# that plays its own scene, a recipe made with the inspector's scene builder (docs/PLUGINS.md).
#
# It is a project of its own, so this folder can start a plugin repository. gbtg's build also builds it
# (GBTG_BUILD_PLUGIN_EXAMPLES). On its own:
#
#   cmake -S examples/plugins/dlc_story -B build-dlc-story
#   cmake --build build-dlc-story --config Release
#   cmake --install build-dlc-story --config Release --prefix <folder holding gbtg.exe>
#
# Outside gbtg's tree, set GBTG_SDK_DIR to a gbtg checkout, or to a folder with copies of its cmake/GbtgPlugin.cmake
# and include/gbtg/plugin_api.h.
cmake_minimum_required(VERSION 3.20)
project(gbtg_dlc_story NONE)

set(GBTG_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../.." CACHE PATH
    "Folder holding cmake/GbtgPlugin.cmake and include/gbtg/plugin_api.h")
include("${GBTG_SDK_DIR}/cmake/GbtgPlugin.cmake")

# No SOURCES: the plugin is its plugin.ini, its scene recipe (scenes/) and its loctext (files/, made from loctext/).
gbtg_add_plugin(gbtg_dlc_story ID dlc_story PLUGIN_INI plugin.ini FILES_DIR files EXTRA_DIRS scenes)
