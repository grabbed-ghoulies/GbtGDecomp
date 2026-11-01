#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "plugin_features.h"

namespace gbtg
{
namespace
{
// Function-local so registration from other translation units' static initializers never sees it unconstructed.
std::vector<PluginFeature>& registry()
{
    static std::vector<PluginFeature> features;
    return features;
}
}

bool register_plugin_feature(const PluginFeature& feature)
{
    registry().push_back(feature);
    return true;
}

const std::vector<PluginFeature>& plugin_features()
{
    return registry();
}

const PluginFeature* find_plugin_feature(const std::string& id)
{
    for (const PluginFeature& feature : registry())
    {
        if (id == feature.id)
        {
            return &feature;
        }
    }
    return nullptr;
}
}
#endif
