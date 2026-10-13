#include "door_registration.h"
#include "../frontend/dialogue_resources.h"
#include "../graphics/pose_matrix.h"
#include "../entities/entity_heap.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void store(std::span<u8> row, size_t offset, u32 value)
{
    std::memcpy(row.data() + offset, &value, 4);
}
void pause_door_clip(RuntimeCutsceneInstance& instance, CutsceneRuntimeContext& context)
{
    ++instance.pending[2];
    std::int8_t index = 0;
    if (instance.actor_count > 0)
    {
        do
        {
            auto& row = context.instances.at(&instance).actors[index];
            if (row.actor != nullptr)
            {
                auto* controller = context.animations.at(row.actor)->controller.get();
                if (row.flags != 0 && controller != nullptr)
                {
                    auto* player = controller->slots[1].player.get();
                    if (player != nullptr)
                    {
                        ++player->paused;
                    }
                    controller->dirty = true;
                }
                if (controller != nullptr)
                {
                    auto* player = controller->slots[0].player.get();
                    if (player != nullptr)
                    {
                        ++player->paused;
                    }
                    controller->dirty = true;
                }
            }
            index = std::bit_cast<std::int8_t>(u8(u8(index) + 1));
        }
        while (i32(index) < instance.actor_count);
    }
    if ((instance.flags & 0x10) == 0)
    {
        for (i32 slot = 0; slot < 3; ++slot)
        {
            const i16 voice_index = std::bit_cast<i16>(read_u16(instance.event_state, slot * 2));
            if (voice_index != -1)
            {
                auto& voice = context.audio.voices.voices[voice_index];
                if (voice.cue != nullptr)
                {
                    switch (voice.state)
                    {
                    case 2:
                        voice.state = 3;
                        break;
                    case 5:
                    case 6:
                    case 7:
                        voice.state = 7;
                        pause_runtime_sound_cue(voice.cue->runtime, 1, context.audio.voices.core.sound);
                        break;
                    }
                }
            }
        }
    }
}
}

// Xbox 0x001410F0
const char* runtime_door_room_suffix(const char* room, std::span<const DoorRoomAlias, 2> aliases)
{
    i32 index = i32(std::strlen(room)) - 1;
    if (index >= 0)
    {
        while (room[index] != '_')
        {
            --index;
            if (index < 0)
            {
                return nullptr;
            }
        }
        if (index >= 0)
        {
            const char* suffix = room + index + 1;
            for (u32 alias = 0; alias < 2; ++alias)
            {
                size_t character = 0;
                do
                {
                    u8 left = u8(suffix[character]);
                    u8 right = u8(aliases[alias].source[character]);
                    if (left > 0x20)
                    {
                        left &= 0xDF;
                    }
                    if (right > 0x20)
                    {
                        right &= 0xDF;
                    }
                    if (left != right)
                    {
                        break;
                    }
                    ++character;
                    if (left == 0)
                    {
                        return aliases[alias].target;
                    }
                }
                while (true);
            }
            return suffix;
        }
    }
    return nullptr;
}

// Xbox 0x0005B890
bool build_runtime_door_clip_names(char* clip, char* camera, u16 first_variant,
    u16 second_variant, i32 shape, u16 ordinal, const RuntimeDoorNames& names,
    AnimationResourceRegistry& resources)
{
    if (shape == 12)
    {
        format_dialogue_resource_name(clip, "aid_cutscene_ghoulies_doors_%s_%d%s%s",
            runtime_door_room_suffix(names.room, names.aliases), u32(ordinal),
            names.variants[first_variant], names.variants[second_variant]);
    }
    else
    {
        format_dialogue_resource_name(clip, "aid_cutscene_ghoulies_doors_%s_%s%s",
            names.shapes[shape], names.variants[first_variant], names.variants[second_variant]);
    }
    format_dialogue_resource_name(camera, "aid_cutscene_ghoulies_doors_cameras_%s_%d%s%s",
        runtime_door_room_suffix(names.room, names.aliases), u32(ordinal),
        names.variants[first_variant], names.variants[second_variant]);
    if (find_animation_resource(resources.records, clip) != nullptr)
    {
        return true;
    }
    return cutscene_resource_exists(clip, resources);
}

// Xbox 0x0005B1E0
void release_runtime_doors(RuntimeDoorRelease& context)
{
    auto& doors = context.doors;
    if (doors.pool != nullptr)
    {
        u8* row = doors.pool->begin;
        u8* end = doors.pool->end;
        size_t index = 0;
        while (row < end)
        {
            auto& clips = doors.rows[index];
            if (clips.primary != nullptr)
            {
                release_runtime_cutscene(clips.primary, *doors.cutscenes);
                clips.primary = nullptr;
                store(clips.fields, 0x140, 0);
            }
            if (clips.camera != nullptr)
            {
                release_runtime_cutscene(clips.camera, *doors.cutscenes);
                clips.camera = nullptr;
                store(clips.fields, 0x144, 0);
            }
            row += 0x154;
            ++index;
        }
        engine_heap_free(doors.pool);
        doors.pool = nullptr;
    }
    if (context.camera_registered != 0)
    {
        camera_unlink_view(doors.camera);
        context.camera_registered = 0;
    }
    if (context.paused != 0)
    {
        --context.active_strategies;
        --context.camera_hold;
        context.runtime_flags &= 0xFDFFFFFFu;
        context.paused = 0;
    }
}

// Xbox 0x0005B960
void load_runtime_door_clips(RuntimeDoorClips& door, const char* clip,
    const char* camera, RuntimeDoorContext& context)
{
    if (context.cutscenes == nullptr)
    {
        throw Error("door clips need the scene cutscene runtime, which is not bound");
    }
    auto& cutscenes = *context.cutscenes;
    const bool separate = camera != nullptr &&
        (find_animation_resource(cutscenes.resources.records, camera) != nullptr ||
            cutscene_resource_exists(camera, cutscenes.resources)) && !resource_names_match(clip, camera);
    if (door.primary != nullptr)
    {
        release_runtime_cutscene(door.primary, cutscenes);
        door.primary = nullptr;
        store(door.fields, 0x140, 0);
    }
    if (door.camera != nullptr)
    {
        release_runtime_cutscene(door.camera, cutscenes);
        door.camera = nullptr;
        store(door.fields, 0x144, 0);
    }
    if (separate)
    {
        door.primary = load_runtime_cutscene(clip, 2, nullptr, cutscenes);
        store(door.fields, 0x140, context.instance_address(door.primary));
        door.camera = load_runtime_cutscene(camera, 2, context.camera, cutscenes);
        store(door.fields, 0x144, context.instance_address(door.camera));
        pause_door_clip(*door.camera, cutscenes);
    }
    else
    {
        door.primary = load_runtime_cutscene(clip, 2, context.camera, cutscenes);
        store(door.fields, 0x140, context.instance_address(door.primary));
        door.camera = nullptr;
        store(door.fields, 0x144, 0);
    }
    std::memcpy(&door.primary->position, door.fields.data() + 0xC, 12);
    update_runtime_cutscene_actors(*door.primary, -1.f, door.primary->time / door.primary->duration, cutscenes);
    std::memcpy(&door.primary->euler, door.fields.data() + 0x18, 12);
    update_runtime_cutscene_actors(*door.primary, -1.f, door.primary->time / door.primary->duration, cutscenes);
    std::memcpy(&door.primary->scale, door.fields.data() + 0x24, 12);
    update_runtime_cutscene_actors(*door.primary, -1.f, door.primary->time / door.primary->duration, cutscenes);
    pause_door_clip(*door.primary, cutscenes);
    size_t character = 0;
    char copied;
    do
    {
        copied = clip[character];
        door.fields[0xC0 + character] = u8(copied);
        ++character;
    }
    while (copied != 0);
    store(door.fields, 0xB8, 0);
    u32 index = 0;
    if (door.primary->resource[3] != 0)
    {
        do
        {
            const auto& row = cutscenes.instances.at(door.primary).actors[std::bit_cast<std::int8_t>(u8(index))];
            const auto models = context.models(row.actor);
            if (models.reference != nullptr)
            {
                const auto cpu = *models.reference;
                const u32 count = read_u32(cpu, 4);
                const u32 table = read_u32(cpu, 0);
                u32 flags = 0;
                for (u32 entry = 0; entry < count; ++entry)
                {
                    if (read_u32(cpu, table + entry * 8) == 1)
                    {
                        flags = read_u32(cpu, table + entry * 8 + 4);
                        break;
                    }
                }
                if ((flags & 1) != 0)
                {
                    if (models.reference != nullptr)
                    {
                        set_runtime_model_node_state(*models.primary, 0, 0, context.shared_model_states);
                    }
                    if (models.secondary != nullptr)
                    {
                        set_runtime_model_node_state(*models.secondary, 0, 0, context.shared_model_states);
                    }
                }
            }
            ++index;
        }
        while (index < u32(door.primary->resource[3]));
    }
}

// Xbox 0x0005B2D0
void register_runtime_door(const Vec3& position, const Vec3& euler, const Vec3& scale,
    i32 shape, u16 ordinal, RuntimeDoorContext& context)
{
    auto*& pool = context.pool;
    const u32 index = u32(pool->end - pool->begin) / pool->stride;
    if (pool->end == pool->capacity)
    {
        pool = entity_detail::grow_navigation_cell_registration(pool);
        for (u32 row = 0; row < index; ++row)
        {
            context.rows[row].fields = {pool->begin + row * pool->stride, 0x150};
        }
    }
    u8* cursor = pool->end;
    pool->end += pool->stride;
    context.rows.resize(index + 1);
    auto& door = context.rows[index];
    door.fields = {cursor, 0x150};
    door.primary = nullptr;
    door.camera = nullptr;
    store(door.fields, 0, std::bit_cast<u32>(shape));
    std::memcpy(cursor + 4, &ordinal, 2);
    const u16 one = 1;
    const u16 zero = 0;
    std::memcpy(cursor + 6, &one, 2);
    for (size_t field : {0x140u,0x144u,0x14Cu,0xB8u,0x148u})
    {
        store(door.fields, field, 0);
    }
    std::memcpy(cursor + 8, &zero, 2);
    std::memcpy(cursor + 0xC, &position, 12);
    std::memcpy(cursor + 0x18, &euler, 12);
    std::memcpy(cursor + 0x24, &scale, 12);
    float matrix[12];
    build_runtime_pose_matrix(matrix, *reinterpret_cast<Vec3*>(cursor + 0xC),
        *reinterpret_cast<Vec3*>(cursor + 0x18), *reinterpret_cast<Vec3*>(cursor + 0x24),
        reinterpret_cast<float*>(cursor + 0x70));
    const float x = read_f32(context.names.dimensions, u32(shape) * 0x94u);
    const float z = read_f32(context.names.dimensions, u32(shape) * 0x94u + 4);
    store(door.fields, 0xA8, std::bit_cast<u32>(z - 15.f));
    store(door.fields, 0xA0, std::bit_cast<u32>(x - 20.f));
    store(door.fields, 0xA4, 0);
    store(door.fields, 0xB4, std::bit_cast<u32>(z + 15.f));
    store(door.fields, 0xAC, std::bit_cast<u32>(x + 20.f));
    store(door.fields, 0xB0, std::bit_cast<u32>(40.f));
    for (u32 column = 0; column < 4; ++column)
    {
        for (u32 row = 0; row < 3; ++row)
        {
            store(door.fields, 0x30 + column * 16 + row * 4,
                std::bit_cast<u32>(matrix[row * 4 + column]));
        }
    }
    store(door.fields, 0x5C, 0);
    store(door.fields, 0x4C, 0);
    store(door.fields, 0x3C, 0);
    store(door.fields, 0x6C, std::bit_cast<u32>(1.f));
    char clip[128];
    char camera[128];
    if (context.cutscenes == nullptr)
    {
        throw Error("door clip names need the scene cutscene runtime, which is not bound");
    }
    build_runtime_door_clip_names(clip, camera, 1, 0, shape, ordinal, context.names, context.cutscenes->resources);
    load_runtime_door_clips(door, clip, camera, context);
}
}
