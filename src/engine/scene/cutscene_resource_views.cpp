#include "cutscene_resource_views.h"
#include <cstring>

namespace gbtg
{
const std::array<u32, 33> retail_cutscene_event_sizes
{
    0x14, 0x94, 0x94, 0x50, 0x90, 0x18, 0x1C, 0x14,
    0x14, 0x18, 0x90, 0x90, 0x98, 0x14, 0x94, 0x220,
    0x94, 0x1A0, 0x14, 0x114, 0x14, 0x154, 0x90, 0x120,
    0x124, 0x124, 0x94, 0x1C, 0x28, 0x14, 0x94, 0x294, 0x94
};
const std::array<std::array<char, 128>, 6> retail_cutscene_dialogue_resources
{{
    {"aid_loctext_ghoulies_dialog_dialogs"},
    {"aid_texture_joypad_buttons_a"},
    {"aid_cutscene_ghoulies_dialogs_dialogpage1"},
    {"aid_model_ghoulies_actor_buttonbat"},
    {"aid_model_ghoulies_actor_dialogbat"},
    {"aid_model_ghoulies_dialogs_backdrop1"}
}};
namespace
{
void require_resource_range(std::span<const u8> resource, size_t offset, size_t length)
{
    if (offset > resource.size() || length > resource.size() - offset)
    {
        throw Error("cutscene native pointer view is outside its resource");
    }
}
const char* resource_string(std::span<const u8> resource, u32 offset)
{
    require_resource_range(resource, offset, 1);
    const auto* text = reinterpret_cast<const char*>(resource.data() + offset);
    if (std::memchr(text, 0, resource.size() - offset) == nullptr)
    {
        throw Error("cutscene native string view has no terminator");
    }
    return text;
}
CutsceneScalarCurveView resource_curve(std::span<const u8> resource,
    size_t offset, size_t data_base, bool nullable = false)
{
    require_resource_range(resource, offset, 12);
    const u16 count = read_u16(resource, offset);
    const u32 relative = read_u32(resource, offset + 8);
    if (nullable && relative == 0)
    {
        return {count, read_f32(resource, offset + 4), nullptr};
    }
    const size_t data = data_base + static_cast<size_t>(relative);
    require_resource_range(resource, data, static_cast<size_t>(count) * sizeof(float));
    return {count, read_f32(resource, offset + 4),
        reinterpret_cast<const float*>(resource.data() + data)};
}
}

void register_scene_cutscene_resource(std::span<const u8> resource,
    SceneCutsceneResourceViews& output)
{
    require_resource_range(resource, 0, 16);
    auto owned = std::make_unique<SceneCutsceneResourceStorage>();
    const size_t actor_table = read_u32(resource, 12);
    const size_t camera_table = read_u32(resource, 8);
    owned->tracks.resize(resource[3]);
    owned->cameras.resize(resource[2]);
    require_resource_range(resource, actor_table, owned->tracks.size() * 0x64);
    require_resource_range(resource, camera_table, owned->cameras.size() * 8);
    for (size_t index = 0; index < owned->tracks.size(); ++index)
    {
        const size_t row = actor_table + index * 0x64;
        auto& track = owned->tracks[index];
        track.model = resource_string(resource, read_u32(resource, row));
        const u32 animation = read_u32(resource, row + 8);
        if (animation != 0)
        {
            require_resource_range(resource, animation, 1);
            track.animation = resource.subspan(animation);
        }
        else
        {
            track.animation = {};
        }
        track.flags = read_u32(resource, row + 0xC);
        track.id = static_cast<std::int8_t>(resource[row + 0x10]);
        const u32 auxiliary = read_u32(resource, row + 0x18);
        track.auxiliary = auxiliary != 0 ? resource_string(resource, auxiliary) : nullptr;
        for (size_t axis = 0; axis < 3; ++axis)
        {
            track.position.position[axis] = resource_curve(resource, row + 0x1C + axis * 12, 0);
            track.scale.scale[axis] = resource_curve(resource, row + 0x40 + axis * 12, 0);
        }
    }
    for (size_t index = 0; index < owned->cameras.size(); ++index)
    {
        const size_t channels = read_u32(resource, camera_table + index * 8 + 4);
        auto& camera = owned->cameras[index];
        for (size_t axis = 0; axis < 3; ++axis)
        {
            camera.position[axis] = resource_curve(resource, channels + axis * 12, channels);
        }
        for (size_t axis = 0; axis < 4; ++axis)
        {
            camera.rotation[axis] = resource_curve(resource, channels + 0x24 + axis * 12, channels);
        }
        camera.focal = resource_curve(resource, channels + 0x54, channels);
        camera.aspect = resource_curve(resource, channels + 0x60, channels, true);
    }
    output.views[resource.data()] = {owned->tracks, owned->cameras};
    output.storage[resource.data()] = std::move(owned);
}

void unregister_scene_cutscene_resource(const u8* resource,
    SceneCutsceneResourceViews& output)
{
    output.views.erase(resource);
    output.storage.erase(resource);
}

void register_scene_cutscene_bundle_provider(AnimationResourceRegistry& registry,
    std::span<AnimationBundlePool> pools, SceneCutsceneResourceViews& output,
    i32 category, const void* owner)
{
    register_animation_resource_provider(registry,
        [pools](u32, const AnimationResourceRecord& record, const void*& resolved)
        {
            return find_bundle_resource_provider(pools, record.name.c_str(), resolved);
        },
        [&output](u32, const void* resolved, AnimationLoadedResource& loaded)
        {
            const bool result = load_bundle_resource_provider(resolved, loaded);
            if (result)
            {
                const auto& record = *static_cast<const AnimationBundleRecord*>(resolved);
                if (record.name.starts_with("aid_cutscene_"))
                {
                    register_scene_cutscene_resource(loaded.cpu, output);
                }
            }
            return result;
        },
        [pools, &output](u32, const char* name, const AnimationLoadedResource& loaded)
        {
            unregister_scene_cutscene_resource(loaded.cpu.data(), output);
            unload_bundle_resource_provider(pools, name);
        }, category, owner);
}
}
