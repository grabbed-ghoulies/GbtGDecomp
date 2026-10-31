#include "door_registration.h"
#include <cmath>
#include "../frontend/dialogue_resources.h"
#include "../graphics/pose_matrix.h"
#include "../entities/entity_heap.h"
#include "../navigation/grid_regions.h"
#include "../camera/camera.h"
#include "../camera/view_initialization.h"
#include "event_pause.h"
#include "../scene/actor_loading.h"
#include "../runtime/name_lookup.h"
#include "../entities/actor_transform.h"
#include "../camera/camera_body_slots.h"
#include "gbtg/entity.h"

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
            context.rows[row].fields = {pool->begin + row * pool->stride, 0x154};
        }
    }
    u8* cursor = pool->end;
    pool->end += pool->stride;
    context.rows.resize(index + 1);
    auto& door = context.rows[index];
    door.fields = {cursor, 0x154};
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

RuntimeDoorContext* g_runtime_doors = nullptr;

namespace
{
RuntimeDoorClips* find_runtime_door(u16 ordinal, RuntimeDoorContext& context)
{
    if (context.pool == nullptr)
    {
        throw Error("door lookup before 0x0011CBD0 allocated DAT_004EE2B8");
    }
    size_t index = 0;
    for (u8* row = context.pool->begin; row < context.pool->end; row += context.pool->stride, ++index)
    {
        if (read_u16({row, 8}, 4) == ordinal)
        {
            return &context.rows.at(index);
        }
    }
    return nullptr;
}

void put_half(std::span<u8> row, size_t offset, u16 value)
{
    std::memcpy(row.data() + offset, &value, 2);
}
}

// Xbox 0x0005C0A0, EDI point
RuntimeDoorClips* find_runtime_door_nearest(const Vec3& point, RuntimeDoorContext& context)
{
    if (context.pool == nullptr)
    {
        return nullptr;
    }
    float best = std::bit_cast<float>(0x497423FEu); // DAT_0032B3BC, 999999.875.
    RuntimeDoorClips* found = nullptr;
    size_t index = 0;
    for (u8* row = context.pool->begin; row < context.pool->end; row += context.pool->stride, ++index)
    {
        const std::span<const u8> fields{row, context.pool->stride};
        const long double dx = static_cast<long double>(point.x) - read_f32(fields, 0xC);
        const long double dy = static_cast<long double>(point.y) - read_f32(fields, 0x10);
        const long double dz = static_cast<long double>(point.z) - read_f32(fields, 0x14);
        const float distance = float(std::sqrt((dx * dx + dy * dy) + dz * dz));
        if (best > distance)
        {
            found = &context.rows.at(index);
            best = distance;
        }
    }
    return found;
}

// Xbox 0x0005C130, EDI point
void close_runtime_door_nearest(const Vec3& point, RuntimeDoorContext& context)
{
    const RuntimeDoorClips* door = find_runtime_door_nearest(point, context); // 0x0005C0A0
    if (door == nullptr)
    {
        return;
    }
    set_runtime_door_state(read_u16(door->fields, 4), 7, 0, 0, context); // 0x0005B540, AX the ordinal
}

// Xbox 0x0005C150, EAX point
bool runtime_door_nearest_idle(const Vec3& point, RuntimeDoorContext& context)
{
    const RuntimeDoorClips* door = find_runtime_door_nearest(point, context); // 0x0005C0A0, EDI point
    if (door == nullptr)
    {
        return false;
    }
    u16 state = 0;
    runtime_door_state(read_u16(door->fields, 4), state, context); // 0x0005B780, SI the ordinal
    return state == 0 && read_u32(door->fields, 0x14C) == 0 && read_u32(door->fields, 0xB8) == 0;
}

// Xbox 0x00146CE0, EAX from, ECX to
long double runtime_point_distance(const Vec3& from, const Vec3& to)
{
    const long double dx = static_cast<long double>(to.x) - from.x;
    const long double dy = static_cast<long double>(to.y) - from.y;
    const long double dz = static_cast<long double>(to.z) - from.z;
    return std::sqrt((dz * dz + dx * dx) + dy * dy);
}

// Xbox 0x0005BEC0, CX ordinal, EAX door position
bool runtime_door_distance(u16 ordinal, const Vec3& from, Vec3& door_position, float& distance,
    RuntimeDoorContext& context)
{
    RuntimeDoorClips* door = find_runtime_door(ordinal, context);
    if (door == nullptr || door->primary == nullptr) // Row +0x140.
    {
        return false;
    }
    RuntimeCutsceneInstance& clip = *door->primary;
    if (clip.resource[3] == 0) // Clip +0x3C -> +3.
    {
        return false;
    }
    if (context.cutscenes == nullptr)
    {
        throw Error("0x0005BEC0 reads a door clip's track rows with no scene cutscene runtime");
    }
    const auto& rows = context.cutscenes->instances.at(&clip).actors; // Clip +0x44.
    Actor* actor = rows.empty() ? nullptr : rows.front().actor;
    if (actor == nullptr)
    {
        return false;
    }
    door_position = actor_world_position(*actor); // 0x000412A0, ESI the actor, EDI the output
    distance = float(runtime_point_distance(from, door_position)); // 0x00146CE0, EAX from, ECX the door
    return true;
}

// Xbox 0x0005B7C0
bool runtime_door_at_event(u16 ordinal, RuntimeDoorContext& context)
{
    const RuntimeDoorClips* door = find_runtime_door(ordinal, context);
    if (door == nullptr)
    {
        return false;
    }
    if (read_u16(door->fields, 4) == 2)
    {
        return read_u16(door->fields, 6) == 5;
    }
    if (read_u16(door->fields, 6) != 5)
    {
        if (read_u16(door->fields, 8) != 5)
        {
            return false;
        }
        if (door->primary == nullptr)
        {
            throw Error("door moving to its event state without a clip at +0x140");
        }
        if (door->primary->time / door->primary->duration <= 0.5f) // DAT_0032B0F4.
        {
            return false;
        }
    }
    return true;
}

// Xbox 0x0005B780
bool runtime_door_state(u16 ordinal, u16& state, RuntimeDoorContext& context)
{
    const RuntimeDoorClips* door = find_runtime_door(ordinal, context);
    if (door == nullptr)
    {
        return false;
    }
    state = read_u16(door->fields, 6);
    return true;
}

// Xbox 0x0005B540
void set_runtime_door_state(u16 ordinal, u16 state, u32 hold, u32 last, RuntimeDoorContext& context)
{
    RuntimeDoorClips* door = find_runtime_door(ordinal, context);
    if (door == nullptr)
    {
        if (context.added_position == nullptr || context.added_euler == nullptr || context.added_scale == nullptr)
        {
            throw Error("0x0005B540 adds a door with no bound DAT_0050DD80 / DAT_0050DD74 / DAT_0042A840");
        }
        register_runtime_door(*context.added_position, *context.added_euler, *context.added_scale, 0xC, ordinal,
            context); // 0x0005B2D0
        door = find_runtime_door(ordinal, context); // 0x000663B0 over the rows with 0x0005B870 (+4 == ordinal)
    }
    if (state == 6)
    {
        if (door == nullptr)
        {
            throw Error("0x0005B540 writes +0x148 of a missing door row (ESI 0)");
        }
        if (door->primary != nullptr || door->camera != nullptr)
        {
            if (context.cutscenes == nullptr)
            {
                throw Error("door clips need the scene cutscene runtime, which is not bound");
            }
        }
        if (door->primary != nullptr)
        {
            release_runtime_cutscene(door->primary, *context.cutscenes); // 0x0004D720
            door->primary = nullptr;
            store(door->fields, 0x140, 0);
        }
        if (door->camera != nullptr)
        {
            release_runtime_cutscene(door->camera, *context.cutscenes); // 0x0004D720
            door->camera = nullptr;
            store(door->fields, 0x144, 0);
        }
        // The last row moves into this one (0x000F71F0, EAX the row, ECX the last, EDX the stride) and the pool
        // loses a row; the native clip views move with it.
        auto* pool = context.pool;
        u8* row = door->fields.data();
        u8* last = pool->end - pool->stride;
        const size_t count = size_t(pool->end - pool->begin) / pool->stride;
        if (row < last)
        {
            std::memmove(row, last, pool->stride);
            RuntimeDoorClips& moved = context.rows.at(count - 1);
            door->primary = moved.primary;
            door->camera = moved.camera;
        }
        pool->end -= pool->stride;
        context.rows.resize(count - 1);
        return;
    }
    if (state == 7)
    {
        break_runtime_door_actor(*door, context); // 0x0005A9B0, the row
        if (door->primary != nullptr || door->camera != nullptr)
        {
            if (context.cutscenes == nullptr)
            {
                throw Error("door clips need the scene cutscene runtime, which is not bound");
            }
        }
        if (door->primary != nullptr)
        {
            release_runtime_cutscene(door->primary, *context.cutscenes); // 0x0004D720
            door->primary = nullptr;
            store(door->fields, 0x140, 0);
        }
        if (door->camera != nullptr)
        {
            release_runtime_cutscene(door->camera, *context.cutscenes); // 0x0004D720
            door->camera = nullptr;
            store(door->fields, 0x144, 0);
        }
        store(door->fields, 0x148, 1);
        return;
    }
    if (state != read_u16(door->fields, 6))
    {
        if (context.cutscenes == nullptr)
        {
            throw Error("door clip names need the scene cutscene runtime, which is not bound");
        }
        char clip[128];
        char camera[128];
        const i32 shape = std::bit_cast<i32>(read_u32(door->fields, 0));
        const bool moved = read_u32(door->fields, 0x148) != 0;
        if (moved)
        {
            build_runtime_door_clip_names(clip, camera, read_u16(door->fields, 6), state, shape, ordinal,
                context.names, context.cutscenes->resources);
            put_half(door->fields, 8, state);
        }
        else
        {
            const u16 other = state == 0 ? 1 : 0;
            build_runtime_door_clip_names(clip, camera, state, other, shape, ordinal, context.names,
                context.cutscenes->resources);
            put_half(door->fields, 6, state);
            put_half(door->fields, 8, other);
        }
        if (!transition_aid_equal(reinterpret_cast<const char*>(door->fields.data() + 0xC0), clip))
        {
            load_runtime_door_clips(*door, clip, camera, context);
        }
        if (moved)
        {
            hold_runtime_door(*door, 1, context);
            auto& cutscenes = *context.cutscenes;
            if (door->primary == nullptr)
            {
                throw Error("door " + std::to_string(ordinal) + " moves without a clip at +0x140");
            }
            if (hold == 0)
            {
                CutsceneEventPauseContext pause{cutscenes.instances.at(door->primary).actors.data(),
                    cutscenes.animations, cutscenes.audio.voices, cutscenes.resume_callbacks};
                pause_cutscene_events(*door->primary, 0, pause); // 0x0004E640, EDI +0x140
            }
            RuntimeCutsceneInstance* view = door->camera;
            if (view == nullptr && door->primary->resource[2] != 0)
            {
                view = door->primary;
            }
            if (view != nullptr)
            {
                if (context.camera == nullptr)
                {
                    throw Error("door camera view DAT_004EE2BC is not bound");
                }
                update_cutscene_camera_view(*view, cutscenes.views.at(view->resource).cameras.data(),
                    *context.camera); // 0x0004D7D0
            }
        }
        else
        {
            snap_runtime_door_state(*door, state, context);
        }
        store(door->fields, 0x14C, hold);
        store(door->fields, 0x150, last);
    }
    store(door->fields, 0x148, 1);
}

// Xbox 0x0005A9B0
void break_runtime_door_actor(RuntimeDoorClips& door, RuntimeDoorContext& context)
{
    if (door.primary == nullptr)
    {
        throw Error("0x0005A9B0 reads the track actors of a door with no clip at +0x140");
    }
    if (context.cutscenes == nullptr)
    {
        throw Error("0x0005A9B0 reads a door clip's track rows with no scene cutscene runtime");
    }
    const auto& rows = context.cutscenes->instances.at(door.primary).actors; // Clip +0x44.
    Actor* actor = rows.empty() ? nullptr : rows.front().actor;
    if (actor == nullptr)
    {
        throw Error("0x0005A9B0 breaks a door clip with no first track actor");
    }
    if (context.added_position == nullptr)
    {
        throw Error("0x0005A9B0 needs DAT_0050DD80 for 0x0003AEB0");
    }
    Vec3 position{};
    Vec3 euler{};
    bool placed = false;
    bool turned = false;
    if (actor->model != nullptr && actor->model->cpu.data() != nullptr) // Actor +0x204 and its first word.
    {
        const u32 joint = actor_lookup_joint_index(actor->model->cpu, 0) & 0xFFu; // 0x00052FC0
        if (joint != 0xFFu && joint != 0xFFFFFFFFu)
        {
            actor_transform_joint_point(*actor, u16(joint), *context.added_position, position); // 0x0003AEB0
            placed = true;
        }
    }
    if (actor->model != nullptr && actor->model->cpu.data() != nullptr)
    {
        const u32 joint = actor_lookup_joint_index(actor->model->cpu, 0) & 0xFFu; // 0x00052FC0
        if (joint != 0xFFu && joint != 0xFFFFFFFFu)
        {
            actor_joint_world_euler(*actor, u16(joint), euler); // 0x0003B050
            turned = true;
        }
    }
    if (!placed || !turned)
    {
        throw Error("0x0005A9B0 places a door actor whose model has no joint 0 (retail reads stack scratch)");
    }
    actor_set_world_position(*actor, position); // 0x00041730, ESI position, EDI actor
    wrap_runtime_euler(euler, actor->euler); // 0x00105AA0, ESI euler, EDI actor +0x14
    actor->xform_valid = 0; // Actor +0x2A8.
    const i32 shape = std::bit_cast<i32>(read_u32(door.fields, 0));
    const size_t offset = size_t(shape) * 0x94 + 0x10;
    if (shape < 0 || offset >= context.names.dimensions.size())
    {
        throw Error("0x0005A9B0 reads a door shape outside DAT_004249A4");
    }
    if (!context.burst)
    {
        throw Error("0x0005A9B0 bursts the door actor (0x0013B1F0) without the scene's effect views");
    }
    context.burst(*actor, 1.f, reinterpret_cast<const char*>(context.names.dimensions.data() + offset)); // 0x0013B1F0
}

// Xbox 0x0005AA90
void update_runtime_doors(RuntimeDoorContext& context, float step)
{
    if (context.pool == nullptr)
    {
        return;
    }
    const u32 count = u32(context.pool->end - context.pool->begin) / context.pool->stride;
    if (count == 0)
    {
        return;
    }
    if (context.cutscenes == nullptr || context.camera_registered == nullptr || context.paused == nullptr
        || context.active_strategies == nullptr || context.camera_hold == nullptr || context.runtime_flags == nullptr)
    {
        throw Error("door update without its cutscene runtime and hold globals");
    }
    auto& cutscenes = *context.cutscenes;
    bool unbind = false;
    for (u32 index = 0; index < count; ++index)
    {
        RuntimeDoorClips& door = context.rows.at(index);
        if (read_u32(door.fields, 0xB8) == 0)
        {
            continue;
        }
        if (read_u32(door.fields, 0x14C) != 0 && actor_loading_complete()) // 0x001412F0
        {
            if (door.primary == nullptr)
            {
                throw Error("running door without a clip at +0x140");
            }
            CutsceneEventPauseContext pause{cutscenes.instances.at(door.primary).actors.data(),
                cutscenes.animations, cutscenes.audio.voices, cutscenes.resume_callbacks};
            pause_cutscene_events(*door.primary, 0, pause); // 0x0004E640, EDI +0x140
            store(door.fields, 0x14C, 0);
        }
        if (door.primary == nullptr)
        {
            throw Error("running door without a clip at +0x140");
        }
        // ESI: the door camera unless a separate camera clip drives it.
        EntityCamNode* view = read_u32(door.fields, 0x144) != 0 ? nullptr : context.camera;
        if (tick_runtime_cutscene(*door.primary, step, view, cutscenes)) // 0x0004DA50
        {
            if (read_u32(door.fields, 0x144) != 0 && read_u32(door.fields, 0x150) == 0)
            {
                unbind = true;
            }
            if (read_u32(door.fields, 0xB8) != 0)
            {
                store(door.fields, 0xB8, 0);
                if (*context.paused != 0)
                {
                    --*context.active_strategies;
                    --*context.camera_hold;
                    *context.runtime_flags &= 0xFDFFFFFFu;
                    *context.paused = 0;
                }
            }
            snap_runtime_door_state(door, read_u16(door.fields, 8), context); // 0x0005BDB0, AX +8
        }
        if (read_u32(door.fields, 0x144) != 0)
        {
            if (door.camera == nullptr)
            {
                throw Error("door +0x144 names a camera clip with no native instance");
            }
            tick_runtime_cutscene(*door.camera, step, context.camera, cutscenes); // 0x0004DA50, ESI DAT_004EE2BC
        }
    }
    if (unbind && *context.camera_registered != 0)
    {
        camera_unlink_view(context.camera); // 0x00048C70, EAX DAT_004EE2BC
        *context.camera_registered = 0;
    }
}

// Xbox 0x0005C020
void apply_runtime_door_regions(RuntimeDoorContext& context)
{
    if (context.pool == nullptr)
    {
        return;
    }
    for (u8* row = context.pool->begin; row < context.pool->end; row += 0x154)
    {
        const std::span<const u8> fields{row, 8};
        set_all_navigation_regions_blocked(read_u16(fields, 4), read_u16(fields, 6) != 0 ? 1 : 0); // 0x00036A00
    }
}

// Xbox 0x0005BF30
void hold_runtime_door(RuntimeDoorClips& door, u32 active, RuntimeDoorContext& context)
{
    if (read_u32(door.fields, 0xB8) == active)
    {
        return;
    }
    store(door.fields, 0xB8, active);
    if (context.camera_registered == nullptr || context.paused == nullptr || context.active_strategies == nullptr
        || context.camera_hold == nullptr || context.runtime_flags == nullptr)
    {
        throw Error("door hold globals (DAT_004EE31C, DAT_004EE320, DAT_00510CEC, DAT_004DEA24) are not bound");
    }
    if (active == 0)
    {
        if (*context.paused != 0)
        {
            --*context.active_strategies;
            --*context.camera_hold;
            *context.runtime_flags &= 0xFDFFFFFFu;
            *context.paused = 0;
        }
        return;
    }
    if (*context.camera_registered != 0 || read_u32(door.fields, 0x144) == 0)
    {
        return;
    }
    if (*context.paused == 0)
    {
        ++*context.active_strategies;
        ++*context.camera_hold;
        *context.runtime_flags |= 0x02000000u;
        *context.paused = 1;
    }
    if (context.camera == nullptr || context.camera_name == nullptr)
    {
        throw Error("door camera view DAT_004EE2BC is not bound");
    }
    initialize_camera_view(*context.camera, context.camera_name, 0); // 0x000496C0, EDX view, EAX name
    link_camera_view(0, *context.camera); // 0x00048C20, ESI 0
    camera_bind_named_view(0, context.camera_name); // 0x00048CF0, EBX name
    *context.camera_registered = 1;
}

// Xbox 0x0005BDB0
void snap_runtime_door_state(RuntimeDoorClips& door, u16 state, RuntimeDoorContext& context)
{
    set_all_navigation_regions_blocked(read_u16(door.fields, 4), state == 0 ? 0 : 1); // 0x001117E0, EBX
    put_half(door.fields, 6, state);
    put_half(door.fields, 8, state == 0 ? 1 : 0);
    if (door.primary == nullptr || door.primary->resource[3] == 0)
    {
        return;
    }
    if (!context.actor_message)
    {
        throw Error("door clip actor messages are not bound");
    }
    // The message is a one-word stack slot; receivers read its id.
    const u32 id = state == 1 ? 0x1F : 0x1E;
    std::int8_t index = 0;
    do
    {
        Actor* actor = context.cutscenes->instances.at(door.primary).actors[index].actor;
        if (actor != nullptr)
        {
            std::array<u32, 0x42> message{};
            message[0] = id;
            context.actor_message(*actor, message);
        }
        index = std::bit_cast<std::int8_t>(u8(u8(index) + 1));
    } while (i32(index) < i32(door.primary->resource[3]));
}
}
