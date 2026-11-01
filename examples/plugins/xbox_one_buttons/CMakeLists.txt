# Example gbtg plugin: a data plugin with no library. Its textures replace the game's White and Black button
# pictures (aid_texture_joypad_buttons_white and _black in common.bnl) with the Black button labelled LB and RB, the
# shoulder buttons the host maps them to (docs/PLUGINS.md).
#
# It is a project of its own, so this folder can start a plugin repository. gbtg's build also builds it
# (GBTG_BUILD_PLUGIN_EXAMPLES). On its own:
#
#   cmake -S examples/plugins/xbox_one_buttons -B build-xbox-one-buttons
#   cmake --build build-xbox-one-buttons --config Release
#   cmake --install build-xbox-one-buttons --config Release --prefix <folder holding gbtg.exe>
#
# Outside gbtg's tree, set GBTG_SDK_DIR to a gbtg checkout, or to a folder with copies of its cmake/GbtgPlugin.cmake
# and include/gbtg/plugin_api.h.
cmake_minimum_required(VERSION 3.20)
project(gbtg_xbox_one_buttons NONE)

set(GBTG_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../.." CACHE PATH
    "Folder holding cmake/GbtgPlugin.cmake and include/gbtg/plugin_api.h")
include("${GBTG_SDK_DIR}/cmake/GbtgPlugin.cmake")

# No SOURCES: the plugin is its plugin.ini and its textures (files/aid/, made by make_buttons.py).
gbtg_add_plugin(gbtg_xbox_one_buttons ID xbox_one_buttons PLUGIN_INI plugin.ini FILES_DIR files)
