/* Example feature library: logs each scene script the game loads. Plain C, to show the interface needs no C++.
 * [SceneLog] Prefix in plugin.ini sets the log line's prefix. */
#include "gbtg/plugin_api.h"

#include <stdio.h>
#include <string.h>

static const GbtgPluginHost* g_host = NULL;
static const char* g_prefix = "scene";
static char g_last[256];

static void say_scene(GbtgGame* game)
{
    char line[512];
    strncpy(g_last, g_host->scene_name(game), sizeof(g_last) - 1);
    g_last[sizeof(g_last) - 1] = '\0';
    snprintf(line, sizeof(line), "%s: %s at frame %u", g_prefix, g_last, (unsigned)g_host->frame(game));
    g_host->log(g_host->plugin, GBTG_LOG_INFO, line);
}

static void scene_log_boot(void* user, GbtgGame* game)
{
    (void)user;
    say_scene(game);
}

static void scene_log_frame(void* user, GbtgGame* game)
{
    (void)user;
    if (strncmp(g_host->scene_name(game), g_last, sizeof(g_last) - 1) != 0)
    {
        say_scene(game);
    }
}

GBTG_PLUGIN_EXPORT int gbtg_plugin_load(const GbtgPluginHost* host)
{
    static const GbtgFeature feature = {"scene_log", "logs each scene script the game loads", scene_log_boot,
        scene_log_frame, NULL, NULL, NULL};
    const char* prefix;
    if (host->api_version != GBTG_PLUGIN_API_VERSION)
    {
        return 0;
    }
    g_host = host;
    prefix = host->setting(host->plugin, "SceneLog", "Prefix");
    if (prefix != NULL)
    {
        g_prefix = prefix;
    }
    return host->add_feature(host->plugin, &feature);
}
