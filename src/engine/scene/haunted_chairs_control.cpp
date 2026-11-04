#include "haunted_chairs_control.h"
#include "scene_controls.h"
#include "scene_control_fields.h"
#include "../camera/camera_body_slots.h"
#include "../cutscenes/actor_tracks.h"
#include "../cutscenes/event_runtime.h"
#include "../entities/actor_spawn_query.h"
#include "../entities/actor_transform.h"
#include "../entities/entity.h"

#include <cstring>

namespace gbtg
{
namespace
{

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

// The test 0x00082780 and 0x00082810 share: a chair that is not bound to another actor and whose +0x93C byte, +0xAD0
// and +0xAC4 words say it stands as a free, live actor. Bound means the +0x918 word (the parent's link) is set, that
// link's own +0 word is set and the parent is not null (the word is not 0x918).
bool chair_stands_free(Actor& chair, HauntedChairsControlServices& services)
{
    const ActorLink* link = chair.link; // Actor +0x918.
    const auto* parent = link == nullptr ? nullptr : static_cast<const ActorLink*>(link->head);
    if (parent != nullptr && parent->head != nullptr && link->owner != nullptr)
    {
        return false;
    }
    const auto bytes = services.actor_bytes(chair);
    return bytes[0x93C] != 0 && read_u32(bytes, 0xAD0) == 0 && read_u32(bytes, 0xAC4) != 0;
}

// The marker record (DAT_0051108C: a count at +0, records from +8, each its size at +0) whose +6 id matches.
const u8* find_marker(const u8* markers, u16 id)
{
    const u32 count = read_u32({markers, 8}, 0);
    const u8* record = markers + 8;
    for (u32 index = 0; index < count; ++index)
    {
        if (read_u16({record, 8}, 6) == id)
        {
            return record;
        }
        record += read_u32({record, 4}, 0);
    }
    return nullptr;
}
}

// Xbox 0x000824D0
void haunted_chairs_control_object_sizes(ObjectSizes& output)
{
    output.instance = 0x74;
    output.parameters = 8;
}

// Xbox 0x00082540
bool construct_runtime_haunted_chairs_control(HauntedChairsControlState& control)
{
    control.phase = 0u;
    return true;
}

// Xbox 0x00082710, ECX state: the haunted chairs (class 0x89) on the DAT_00511B24 list, at most nine, each with the
// row its +0x1FC id has in UNK_00301B30.
void gather_runtime_haunted_chairs(HauntedChairsControlState& control,
    HauntedChairsControlServices& services)
{
    control.count = 0;
    for (auto* node = static_cast<ActorSpawnLink*>(entity_detail::g_world.body_b.next); node != nullptr;
         node = node->next) // DAT_00511B28.
    {
        Actor* actor = node->actor; // The object at node - 0x934.
        if (actor == nullptr)
        {
            throw Error("DAT_00511B24 node has no actor");
        }
        if (actor->class_key == 0x89)
        {
            const i32 count = control.count;
            control.chairs[size_t(count)] = actor;
            const u16 id = read_u16(services.actor_bytes(*actor), 0x1FC);
            i32 row = 0;
            while (row < 9 && services.chair_ids[size_t(row)] != u32(id))
            {
                ++row;
            }
            if (row == 9)
            {
                row = -1;
            }
            auto rows = control.rows;
            rows[size_t(count)] = row;
            control.rows = rows;
            control.count = count + 1;
        }
        if (control.count >= 9)
        {
            return;
        }
    }
}

// Xbox 0x00082780: each free chair leaves the render bins (0x0003CBB0); the others' rows in the current cutscene
// are released (0x0004E9E0).
void hide_runtime_haunted_chairs(const HauntedChairsControlState& control,
    HauntedChairsControlServices& services)
{
    const auto rows = control.rows;
    for (i32 index = 0; index < control.count; ++index)
    {
        Actor* chair = control.chairs[size_t(index)];
        if (chair != nullptr && chair_stands_free(*chair, services))
        {
            unregister_actor_render_bins(services.render(*chair)); // 0x0003CBB0
            continue;
        }
        if (rows[size_t(index)] != -1)
        {
            auto& cutscenes = services.cutscenes;
            if (cutscenes.index < 0)
            {
                throw Error("0x00082780 reads DAT_004E1C38[DAT_004E1C34] with no current cutscene");
            }
            RuntimeCutsceneInstance* instance = cutscenes.instances.at(size_t(cutscenes.index));
            auto& runtime = cutscenes.bindings.runtime;
            auto& instance_rows = runtime.instances.at(instance);
            CutsceneActorReleaseContext release{runtime.factory, runtime.objects, runtime.render,
                instance_rows.events, runtime.resources};
            release_cutscene_actor_row(instance_rows.actors.data(), std::int8_t(rows[size_t(index)]),
                release); // 0x0004E9E0, AL the row
        }
    }
}

// Xbox 0x00082810: each free chair rejoins the render bins (0x0003CAE0) and, with a +0x1FE marker id, stands at its
// marker's position and euler.
void show_runtime_haunted_chairs(const HauntedChairsControlState& control,
    HauntedChairsControlServices& services)
{
    for (i32 index = 0; index < control.count; ++index)
    {
        Actor* chair = control.chairs[size_t(index)];
        if (chair == nullptr || !chair_stands_free(*chair, services))
        {
            continue;
        }
        register_actor_render_bins(services.render(*chair)); // 0x0003CAE0
        const auto bytes = services.actor_bytes(*chair);
        const u16 id = read_u16(bytes, 0x1FE);
        if (id == 0)
        {
            continue;
        }
        // Two walks over the same records: the first takes the position (+0xC), the second the euler (+0x18).
        // A missing record leaves the stack words unset.
        const u8* markers = static_cast<const u8*>(services.markers);
        const u8* record = find_marker(markers, id);
        if (record == nullptr)
        {
            throw Error("0x00082810 places a chair from unset stack words: no marker has id " + std::to_string(id));
        }
        Vec3 position;
        std::memcpy(&position, record + 0xC, 12);
        Vec3 euler;
        std::memcpy(&euler, record + 0x18, 12);
        actor_set_world_position(*chair, position); // 0x00041730, EDI chair, ESI position
        wrap_runtime_euler(euler, chair->euler); // 0x00105AA0, ESI euler, EDI chair +0x14
        chair->xform_valid = 0; // +0x2A8.
        put_word(bytes, 0xAAC, 1);
        actor_refresh_transform(*chair); // 0x00040DE0
    }
}

// Xbox 0x00082560
void message_runtime_haunted_chairs_control(std::span<const u32> message,
    HauntedChairsControlState& control, SceneControlInstance& instance, SceneControlLists& lists,
    HauntedChairsControlServices& services)
{
    switch (message[0])
    {
    case 1:
        control.phase = 1u;
        break;
    case 0x7C:
        for (i32 index = 0; index < control.count; ++index)
        {
            Actor* chair = control.chairs[size_t(index)];
            if (chair != nullptr && services.actor_address(*chair) == message[1])
            {
                control.chairs[size_t(index)] = nullptr;
            }
        }
        break;
    case 0x7D:
        gather_runtime_haunted_chairs(control, services); // 0x00082710
        break;
    case 0x7E:
        if (control.phase == 1)
        {
            hide_runtime_haunted_chairs(control, services); // 0x00082780
            control.phase = 2u;
        }
        break;
    default:
        break;
    }
    if (message[0] == 0x80)
    {
        retire_runtime_scene_control(instance, lists); // 0x00115E70, ESI state
    }
}

// Xbox 0x00082680
void tick_runtime_haunted_chairs_control(SceneControlBase& base, HauntedChairsControlState& control,
    SceneControlInstance& instance, SceneControlLists& lists, HauntedChairsControlServices& services)
{
    auto& cutscenes = services.cutscenes;
    const auto& bindings = cutscenes.bindings;
    const float step = bindings.freeze > 0.f ? 0.f : bindings.dt * bindings.scene_scale;
    base.elapsed = base.elapsed + step;
    const u32 phase = control.phase;
    if (phase == 2)
    {
        if (cutscenes.index <= -1) // DAT_004E1C34.
        {
            show_runtime_haunted_chairs(control, services); // 0x00082810
            control.phase = 3u;
        }
        return;
    }
    // Phase 3, off the disposal list (+0x18): message 0x80 through vtable +0xC, then 0x00115E70 if still off it.
    if (phase == 3 && instance.disposal.previous == nullptr)
    {
        dispose_runtime_scene_control(instance, lists); // The inlined 0x00116140.
    }
}
}
