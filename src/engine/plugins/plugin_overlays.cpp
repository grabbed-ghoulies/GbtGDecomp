#if !defined(GBTG_RETAIL_GAME_ONLY)
// Plugin overlays (GbtgOverlay): host presentation the gfx backends draw over each presented frame, with the window's
// input first. The debug UI is one (src/plugins/debug_ui).
#include "plugin_libraries.h"

#include "../graphics/gfx_render_thread.h"
#include "gbtg/game.h"
#include "gbtg/gfx.h"
#include "gbtg/log.h"

#include <cstring>

namespace gbtg
{
namespace
{
std::vector<GbtgOverlay> g_overlays;

int host_add_overlay(void* plugin, const GbtgOverlay* overlay)
{
    if (overlay == nullptr || overlay->id == nullptr)
    {
        return 0;
    }
    log::verbose("plugin %s: overlay %s", plugin_library_id(plugin), overlay->id);
    g_overlays.push_back(*overlay);
    return 1;
}

int host_graphics_device(GbtgGame*, GbtgGraphicsDevice* out)
{
    if (out == nullptr || out->size < sizeof(uint32_t))
    {
        return 0;
    }
    GfxOverlayDevice device;
    if (!gfx_overlay_device(device))
    {
        return 0;
    }
    GbtgGraphicsDevice full{};
    full.size = sizeof(full);
    full.backend = device.backend;
    full.window = device.window;
    full.d3d11_device = device.d3d11_device;
    full.d3d11_context = device.d3d11_context;
    full.vk_api_version = device.vk_api_version;
    full.vk_instance = device.vk_instance;
    full.vk_physical_device = device.vk_physical_device;
    full.vk_device = device.vk_device;
    full.vk_queue_family = device.vk_queue_family;
    full.vk_queue = device.vk_queue;
    full.vk_color_format = device.vk_color_format;
    full.vk_image_count = device.vk_image_count;
    // A plugin built against an older header gets the members it knows.
    const uint32_t size = (std::min)(out->size, uint32_t(sizeof(full)));
    std::memcpy(out, &full, size);
    out->size = size;
    return 1;
}
}

void fill_overlay_calls(GbtgPluginHost& host)
{
    host.add_overlay = host_add_overlay;
    host.graphics_device = host_graphics_device;
}

void reset_plugin_overlays()
{
    g_overlays.clear();
}

void plugin_overlays_wait_presented()
{
#if GBTG_OPTIMIZE
    // The render thread draws the previous build while presenting it.
    if (!g_overlays.empty())
    {
        gfx_render_thread_wait_presented();
    }
#endif
}

bool plugins_window_event(const void* event)
{
    for (const GbtgOverlay& overlay : g_overlays)
    {
        if (overlay.window_event != nullptr && overlay.window_event(overlay.user, event) != 0)
        {
            return true;
        }
    }
    return false;
}

bool plugins_capture_input()
{
    for (const GbtgOverlay& overlay : g_overlays)
    {
        if (overlay.captures_input != nullptr && overlay.captures_input(overlay.user) != 0)
        {
            return true;
        }
    }
    return false;
}

void plugins_overlay_build(Game& game, float host_dt)
{
    plugin_overlays_wait_presented();
    for (const GbtgOverlay& overlay : g_overlays)
    {
        if (overlay.build != nullptr)
        {
            overlay.build(overlay.user, reinterpret_cast<GbtgGame*>(&game), host_dt);
        }
    }
}

bool plugins_overlay_has_draws()
{
    for (const GbtgOverlay& overlay : g_overlays)
    {
        if (overlay.has_draws != nullptr && overlay.has_draws(overlay.user) != 0)
        {
            return true;
        }
    }
    return false;
}

void plugins_overlay_render(void* command)
{
    for (const GbtgOverlay& overlay : g_overlays)
    {
        if (overlay.render != nullptr && (overlay.has_draws == nullptr || overlay.has_draws(overlay.user) != 0))
        {
            overlay.render(overlay.user, command);
        }
    }
}
}
#endif
