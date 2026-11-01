#if !defined(GBTG_RETAIL_GAME_ONLY)
// The host table's read-only diagnostics (include/gbtg/plugin_api.h), which the debug UI plugin shows.
#include "plugin_libraries.h"

#include "../scene/factory_scene_runtime.h"
#include "gbtg/engine_internal.h"
#include "gbtg/game.h"
#include "gbtg/gfx.h"

#include <cstring>

namespace gbtg
{
namespace
{
const Game& game_of(const GbtgGame* game)
{
    return *reinterpret_cast<const Game*>(game);
}

void host_runtime_info(const GbtgGame* game, GbtgRuntimeInfo* out)
{
    if (game == nullptr || out == nullptr || out->size < sizeof(uint32_t))
    {
        return;
    }
    const Game& state = game_of(game);
    // requested_script need not end in a zero inside its array.
    static std::string requested;
    requested.assign(state.requested_script, strnlen(state.requested_script, sizeof(state.requested_script)));

    GbtgRuntimeInfo info{};
    info.size = sizeof(info);
    info.scene_name = state.scene_name.c_str();
    info.frame = state.frame;
    info.clock_frame = state.clock.frame;
    info.clock_time = state.clock.time;
    info.dt = state.clock.dt;
    info.dt_raw = state.clock.dt_raw;
    info.target_hz = clock_fps(state.clock);
    info.pal = state.clock.pal ? 1 : 0;
    info.script_phase = state.script_phase;
    info.script_hold = state.script_hold ? 1 : 0;
    info.requested_script = requested.c_str();
    info.presenter = gfx_backend_name();
    gfx_size(info.width, info.height);
    info.last_missing_resource = "";
    if (state.factory_scene)
    {
        const auto& scene = *state.factory_scene;
        info.has_native_scene = 1;
        info.actor_start = scene.actor_start;
        info.native_objects = scene.objects.size();
        info.registered_objects = scene.factory.count;
        info.address_bindings = scene.address_count();
        info.loading = scene.resources.loading;
        info.last_missing_resource = scene.resources.last_missing.c_str();
    }
    for (const auto& slot : state.clock.delayed)
    {
        if (slot.fn != nullptr)
        {
            ++info.delayed_active;
        }
    }
    info.delayed_slots = state.clock.delayed.size();
    info.cached_bundles = state.aids.bundles.size();
    info.demand_resources = state.aids.demand_resources.size();

    // A plugin built against an older header gets the members it knows.
    const uint32_t size = (std::min)(out->size, uint32_t(sizeof(info)));
    std::memcpy(out, &info, size);
    out->size = size;
}

void host_native_objects(const GbtgGame* game, void (*visit)(void*, const GbtgNativeObject*), void* context)
{
    if (game == nullptr || visit == nullptr || !game_of(game).factory_scene)
    {
        return;
    }
    const auto& scene = *game_of(game).factory_scene;
    for (const auto& object : scene.objects)
    {
        const GbtgNativeObject entry{scene.find_address(object.bytes.get()), uint32_t(object.type), uint32_t(object.size),
            object.vtable ? object.vtable->address : 0};
        visit(context, &entry);
    }
}

void host_delayed_callbacks(const GbtgGame* game, void (*visit)(void*, size_t, uint32_t), void* context)
{
    if (game == nullptr || visit == nullptr)
    {
        return;
    }
    const auto& delayed = game_of(game).clock.delayed;
    for (size_t i = 0; i < delayed.size(); ++i)
    {
        if (delayed[i].fn != nullptr)
        {
            visit(context, i, delayed[i].frames_left);
        }
    }
}

void host_resource_bundles(const GbtgGame* game, void (*visit)(void*, const GbtgBundleInfo*), void* context)
{
    if (game == nullptr || visit == nullptr)
    {
        return;
    }
    const auto row = [&](const char* name, const Bundle& bundle)
    {
        const GbtgBundleInfo info{name, bundle.entries.size(), bundle.cpu_heap.size(), bundle.gpu_heap.size()};
        visit(context, &info);
    };
    const AidStore& aids = game_of(game).aids;
    row("common", aids.common);
    for (const auto& [name, bundle] : aids.bundles)
    {
        row(name.c_str(), bundle);
    }
}

size_t host_scene_count(const GbtgGame* game)
{
    return game != nullptr ? game_of(game).scene_order.size() : 0;
}

const char* host_scene_at(const GbtgGame* game, size_t index)
{
    if (game == nullptr || index >= game_of(game).scene_order.size())
    {
        return "";
    }
    return game_of(game).scene_order[index].c_str();
}

void host_request_scene(GbtgGame* game, const char* name)
{
    if (game != nullptr && name != nullptr && *name != '\0')
    {
        script_request(*reinterpret_cast<Game*>(game), name);
    }
}
}

void fill_diagnostics_calls(GbtgPluginHost& host)
{
    host.runtime_info = host_runtime_info;
    host.native_objects = host_native_objects;
    host.delayed_callbacks = host_delayed_callbacks;
    host.resource_bundles = host_resource_bundles;
    host.scene_count = host_scene_count;
    host.scene_at = host_scene_at;
    host.request_scene = host_request_scene;
}
}
#endif
