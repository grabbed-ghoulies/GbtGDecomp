#if !defined(GBTG_RETAIL_GAME_ONLY)
// The wide_aspect feature: gameplay views wider than 16:9 (camera/wide_aspect.h). The wide_aspect plugin switches
// it on (src/plugins/wide_aspect). [wide_aspect] Aspect is `auto` (the back buffer's own shape), `W:H` or a ratio.
#include "plugin_features.h"

#include "../camera/wide_aspect.h"
#include "gbtg/log.h"

#include <cctype>
#include <cstdlib>
#include <string>

namespace gbtg
{
namespace
{
// `auto` or empty is 0 (the back buffer's shape); `21:9` and `2.333` are ratios; anything else is -1.
float parse_aspect(const std::string& text)
{
    if (text.empty() || text == "auto")
    {
        return 0.f;
    }
    const size_t colon = text.find(':');
    char* end = nullptr;
    if (colon == std::string::npos)
    {
        const float ratio = std::strtof(text.c_str(), &end);
        return end != text.c_str() && *end == '\0' && ratio > 0.f ? ratio : -1.f;
    }
    const std::string across_text = text.substr(0, colon);
    const std::string down_text = text.substr(colon + 1);
    const float across = std::strtof(across_text.c_str(), &end);
    if (end == across_text.c_str() || *end != '\0')
    {
        return -1.f;
    }
    const float down = std::strtof(down_text.c_str(), &end);
    if (end == down_text.c_str() || *end != '\0' || across <= 0.f || down <= 0.f)
    {
        return -1.f;
    }
    return across / down;
}

std::string lower_setting(const PluginSettings& settings, const char* key)
{
    const auto found = settings.find(key);
    std::string text = found != settings.end() ? found->second : std::string();
    for (char& c : text)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

// [wide_aspect] Hud: `stretch` (retail's), or the HUD's shape as W:H or a ratio; empty is 4:3.
void boot_hud_aspect(const PluginSettings& settings)
{
    const std::string text = lower_setting(settings, "hud");
    if (text == "stretch")
    {
        request_hud_aspect(0.f);
        return;
    }
    const float aspect = text.empty() ? 4.f / 3.f : parse_aspect(text);
    if (aspect <= 0.f)
    {
        log::warn("wide_aspect: Hud=%s is not stretch, W:H or a ratio; using 4:3", text.c_str());
        request_hud_aspect(4.f / 3.f);
        return;
    }
    request_hud_aspect(aspect);
}

void boot_wide_aspect(Game&, const PluginSettings& settings)
{
    boot_hud_aspect(settings);
    const std::string text = lower_setting(settings, "aspect");
    const float ratio = parse_aspect(text);
    if (ratio < 0.f)
    {
        log::warn("wide_aspect: Aspect=%s is not auto, W:H or a ratio; using auto", text.c_str());
        request_wide_aspect(0.f);
        return;
    }
    request_wide_aspect(ratio);
}
}

GBTG_PLUGIN_FEATURE(wide_aspect, "wide_aspect", "gameplay views wider than 16:9, such as 21:9 or 32:9",
    boot_wide_aspect, nullptr);
}
#endif
