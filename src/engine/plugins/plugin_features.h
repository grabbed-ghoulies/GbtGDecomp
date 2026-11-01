#pragma once
// Compiled-in plugin features. A feature is host code that registers itself here; it runs only when a loaded
// plugin lists its id under [Plugin] Features. Its settings are the plugin.ini section named after the id.
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include <map>
#include <string>
#include <vector>

namespace gbtg
{
struct Game;

using PluginSettings = std::map<std::string, std::string>; // Keys lower case.

struct PluginFeature
{
    const char* id;
    const char* summary;
    void (*boot)(Game& game, const PluginSettings& settings); // Once, after Game::init. May be null.
    void (*frame)(Game& game); // Each frame, after frame_tick. May be null.
};

// Called from a static initializer (GBTG_PLUGIN_FEATURE); returns true so it can seed a static bool.
bool register_plugin_feature(const PluginFeature& feature);
const std::vector<PluginFeature>& plugin_features();
const PluginFeature* find_plugin_feature(const std::string& id);
}

#define GBTG_PLUGIN_FEATURE(symbol, ...) \
    static const bool symbol##_registered = ::gbtg::register_plugin_feature(::gbtg::PluginFeature{__VA_ARGS__})
#endif
