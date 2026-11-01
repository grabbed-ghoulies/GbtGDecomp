#if !defined(GBTG_RETAIL_GAME_ONLY)
// Plugin stories: entries a plugin adds to game select's Features menu, each a list of scenes the game plays the
// way the menu's Credits play (docs/PLUGINS.md, "Stories"). From a plugin.ini [Story] section or a library's
// add_story.
#include "plugins.h"

#include "../../gbtg/text/localtext_resources.h"
#include "../scene/dialogue_resource_owner.h"
#include "gbtg/game.h"
#include "gbtg/hash.h"
#include "gbtg/language.h"
#include "gbtg/log.h"

#include <algorithm>

namespace gbtg
{
namespace
{
std::vector<PluginStory> g_stories;
Game* g_game = nullptr;

std::u16string widen(std::string_view text)
{
    // UTF-8 to UTF-16; a malformed byte stands for itself.
    std::u16string out;
    for (size_t at = 0; at < text.size();)
    {
        const u8 lead = u8(text[at]);
        const int length = lead < 0x80 ? 1 : (lead >> 5) == 6 ? 2 : (lead >> 4) == 14 ? 3 : (lead >> 3) == 30 ? 4 : 1;
        char32_t codepoint = length == 1 ? lead : lead & (0x7F >> length);
        bool valid = at + size_t(length) <= text.size();
        for (int index = 1; valid && index < length; ++index)
        {
            const u8 next = u8(text[at + size_t(index)]);
            valid = (next & 0xC0) == 0x80;
            codepoint = (codepoint << 6) | (next & 0x3F);
        }
        if (!valid)
        {
            codepoint = lead;
        }
        if (codepoint >= 0x10000)
        {
            codepoint -= 0x10000;
            out.push_back(char16_t(0xD800 + (codepoint >> 10)));
            out.push_back(char16_t(0xDC00 + (codepoint & 0x3FF)));
        }
        else
        {
            out.push_back(char16_t(codepoint));
        }
        at += valid ? size_t(length) : 1;
    }
    return out;
}
}

void add_plugin_story(PluginStory story)
{
    if (story.scenes.empty())
    {
        log::warn("plugin %s: story %s has no scenes, skipped", story.plugin.c_str(), story.name.c_str());
        return;
    }
    for (const std::string& scene : story.scenes)
    {
        if (!scene.starts_with("aid_script_") || scene.size() >= 0x80)
        {
            log::warn("plugin %s: story %s: %s is not an aid_script_ name, skipped", story.plugin.c_str(),
                story.name.c_str(), scene.c_str());
            return;
        }
    }
    log::verbose("plugin %s: story %s, %zu scenes", story.plugin.c_str(), story.name.c_str(), story.scenes.size());
    g_stories.push_back(std::move(story));
}

void reset_plugin_stories()
{
    g_stories.clear();
}

const std::vector<PluginStory>& plugin_stories()
{
    return g_stories;
}

void bind_plugin_stories(Game& game)
{
    g_game = &game;
}

bool plugin_loctext(Game& game, const std::string& resource, const std::string& key, std::u16string& out)
{
    out.clear();
    if (game.native_resources == nullptr || !resource.starts_with("aid_loctext_"))
    {
        return false;
    }
    // The native acquire treats a missing resource as a damaged disc (resource_error 2, which never returns), so a
    // plugin's missing translation is checked here first, where the dialogue resource owner would read it.
    const auto path = plugin_disc_path(game.aids.game_dir / "loctext" / language_folder(g_language)
        / aid_hash_hex(aid_hash(resource)));
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec))
    {
        log::warn("plugin loctext %s: no %s file", resource.c_str(), language_folder(g_language));
        return false;
    }
    char16_t text[0x400]{};
    if (acquire_localtext_key(resource.c_str(), key.c_str(), text, game.native_resources->text) == 0)
    {
        return false;
    }
    out = text;
    return true;
}

std::u16string plugin_story_label(const PluginStory& story)
{
    std::u16string label;
    if (g_game != nullptr && !story.loctext.empty() && plugin_loctext(*g_game, story.loctext, story.key, label))
    {
        return label;
    }
    return widen(story.name);
}
}
#endif
