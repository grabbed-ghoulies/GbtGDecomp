#if !defined(GBTG_RETAIL_GAME_ONLY)
// The rare_replay feature: the logic ticks at 60 Hz, as the Rare Replay release does (runtime/rare_replay.h). The
// rare_replay plugin switches it on (src/plugins/rare_replay).
#include "plugin_features.h"

#include "../runtime/rare_replay.h"

namespace gbtg
{
namespace
{
void boot_rare_replay(Game&, const PluginSettings&)
{
    enable_rare_replay();
}
}

GBTG_PLUGIN_FEATURE(rare_replay, "rare_replay", "ticks the logic at 60 Hz, as the Rare Replay release does",
    boot_rare_replay, nullptr);
}
#endif
