#pragma once
// Plugin libraries (.dll / .so) for plugin_loader.cpp. See include/gbtg/plugin_api.h.
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "plugins.h"

#include "gbtg/plugin_api.h"

namespace gbtg
{
// Opens `plugin`'s [Plugin] Library and calls its gbtg_plugin_load. False (logged) when it is missing or declines.
bool load_plugin_library(const LoadedPlugin& plugin, const std::filesystem::path& executable_dir);
void reset_plugin_libraries();
void plugin_libraries_boot(Game& game);
void plugin_libraries_frame(Game& game);
void plugin_libraries_graphics_ready(Game& game);
void plugin_libraries_graphics_shutdown(Game& game);
}
#endif
