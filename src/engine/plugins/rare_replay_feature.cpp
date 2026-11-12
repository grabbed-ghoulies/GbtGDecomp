#if !defined(GBTG_RETAIL_GAME_ONLY)
// The rare_replay feature: the logic ticks faster than retail's 30 Hz (runtime/rare_replay.h). The rare_replay plugin
// switches it on (src/plugins/rare_replay). [rare_replay] TickPerFrame is 1 (the default: one tick per presented
// frame) or 0 (a fixed 60 Hz, as the Rare Replay release does).
#include "plugin_features.h"

#include "../runtime/rare_replay.h"
#include "gbtg/log.h"

namespace gbtg
{
namespace
{
void boot_rare_replay(Game&, const PluginSettings& settings)
{
    bool tick_per_frame = true;
    const auto found = settings.find("tickperframe");
    if (found != settings.end() && !found->second.empty())
    {
        if (found->second == "0")
        {
            tick_per_frame = false;
        }
        else if (found->second != "1")
        {
            log::warn("rare_replay: TickPerFrame=%s is not 0 or 1; using 1", found->second.c_str());
        }
    }
    enable_rare_replay(tick_per_frame);
}
}

GBTG_PLUGIN_FEATURE(rare_replay, "rare_replay",
    "ticks the logic faster than retail's 30 Hz: once a frame, or at 60 Hz as the Rare Replay release does",
    boot_rare_replay, nullptr);
}
#endif
