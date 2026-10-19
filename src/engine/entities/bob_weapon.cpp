#include "bob_weapon.h"
#include "weapon_actor_detail.h"
#include "actor_model_anchors.h"
#include "actor_transform.h"
#include "player_mind_requests.h"
#include "../cutscenes/event_pause.h"
#include "../cutscenes/event_runtime.h"
#include "../cutscenes/instance_construction.h"

namespace gbtg
{
using namespace weapon_detail;

namespace
{
// The inline strcpy of a parameter name into the instance.
void copy_name(std::span<u8> bytes, size_t offset, std::span<const u8> parameters, size_t from)
{
    size_t index = 0;
    do
    {
        bytes[offset + index] = parameters[from + index];
    }
    while (parameters[from + index++] != 0);
}

CutsceneEventPauseContext pause_context(RuntimeWeaponActor& weapon, RuntimeCutsceneInstance& instance)
{
    CutsceneRuntimeContext& runtime = weapon.services.cutscenes();
    return CutsceneEventPauseContext{runtime.instances.at(&instance).actors.data(), runtime.animations,
        weapon.services.voices, runtime.resume_callbacks};
}

void set_spawn_blocked(RuntimeWeaponActor& weapon, i32 value)
{
    weapon.actor.spawn_blocked = value;
    store32(weapon.bytes, 0xAD0, u32(value));
}

// 0x0004D720 on +0xFE0, which is then cleared.
void release_cutscene(RuntimeWeaponActor& weapon)
{
    release_runtime_cutscene(weapon.cutscene, weapon.services.cutscenes()); // 0x0004D720
    weapon.cutscene = nullptr;
}
}

// Xbox 0x000C5B90
void bob_weapon_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = 0xFE8;
    sizes.parameters = 0x7FC;
}

// Xbox 0x000C5C20
bool initialize_runtime_bob_weapon(RuntimeWeaponActor& weapon, std::span<const u8> parameters)
{
    auto bytes = weapon.bytes;
    store32(bytes, 0x99C, 0x000C61D0); // The class's contact test, called through +0x99C.
    store8(bytes, 0x941, 1);
    store32(bytes, 0xE54, read_u32(parameters, 0x5F8));
    copy_name(bytes, 0xE5C, parameters, 0x5FC);
    copy_name(bytes, 0xEDC, parameters, 0x67C);
    copy_name(bytes, 0xF60, parameters, 0x6FC);
    if ((weapon.services.story_flags & 8) != 0) // DAT_0050EA98.
    {
        CutsceneRuntimeContext& runtime = weapon.services.cutscenes();
        weapon.cutscene = load_runtime_cutscene(reinterpret_cast<const char*>(bytes.data() + 0xF60), 2, nullptr,
            runtime); // 0x0004D680
        if (weapon.cutscene != nullptr)
        {
            RuntimeCutsceneInstance& instance = *weapon.cutscene;
            instance.position = actor_world_position(weapon.actor); // 0x000412A0
            update_runtime_cutscene_actors(instance, -1.f, instance.time / instance.duration, runtime); // 0x0004E1E0
            CutsceneEventPauseContext pause = pause_context(weapon, instance);
            pause_cutscene_events(instance, 1, pause); // 0x0004E640, EAX 1
            change_runtime_weapon_state(weapon, 0xC); // 0x0014A290
        }
    }
    store8(bytes, 0x331, 1);
    return true;
}

// Xbox 0x000C5D30
void destroy_runtime_bob_weapon(RuntimeWeaponActor& weapon)
{
    if (weapon.cutscene != nullptr)
    {
        release_cutscene(weapon);
    }
}

// Xbox 0x000C5EB0
void dispatch_runtime_bob_weapon_message(RuntimeWeaponActor& weapon, std::span<u32> message)
{
    auto bytes = weapon.bytes;
    switch (message[0])
    {
    case 1:
        change_runtime_weapon_state(weapon, 1); // 0x0014A290
        return;
    case 0x23:
        dispatch_runtime_weapon_message(weapon, message); // 0x001463D0
        if ((weapon.services.story_flags & 8) != 0) // DAT_0050EA98.
        {
            clear_actor_model_anchors(weapon.view.anchors); // 0x0003EF40, actor +0x1F8
        }
        return;
    case 0x80:
        if (weapon.cutscene != nullptr)
        {
            release_cutscene(weapon);
        }
        break;
    case 0x86:
        if (message[2] != 0xD)
        {
            break;
        }
        switch (read_u16(bytes, 0xC0C))
        {
        case 3:
            store32(bytes, 0xE54, read_u32(bytes, 0xE54) + 1);
            break;
        case 7:
            change_runtime_weapon_state(weapon, 0x10); // 0x0014A290
            break;
        case 0x10:
            if (bytes[0x93C] != 0)
            {
                store32(bytes, 0xE54, read_u32(bytes, 0xE54) + 1);
                break;
            }
            change_runtime_weapon_state(weapon, 0xE); // 0x0014A290
            break;
        default:
            break;
        }
        break;
    case 0x87:
        if (message[2] == 0xD && read_u16(bytes, 0xC0C) == 0x10 && read_u32(bytes, 0xFE4) == 0)
        {
            store32(bytes, 0xE54, read_u32(bytes, 0xE54) - 1);
            store32(bytes, 0xFE4, 0);
        }
        break;
    case 0x91:
        change_runtime_weapon_state(weapon, 3); // 0x0014A290
        break;
    case 0x95:
        untranslated(0x000C6250, "the bob's message 0x95 (0x000C5EB0)");
    case 0x9D:
        change_runtime_weapon_state(weapon, 7); // 0x0014A290
        break;
    default:
        break;
    }
    dispatch_runtime_weapon_message(weapon, message); // 0x001463D0
}

// Xbox 0x000C6100
void enter_runtime_bob_weapon_state(RuntimeWeaponActor& weapon, u16 state, u16 previous)
{
    switch (state)
    {
    case 1:
    {
        // 0x0004E640 on +0xFE0 with EAX 0.
        CutsceneEventPauseContext pause = pause_context(weapon, *weapon.cutscene);
        pause_cutscene_events(*weapon.cutscene, 0, pause);
        return;
    }
    case 3:
        weapon.services.runtime_flags |= 0x20000; // DAT_0050EA9C.
        return;
    case 7:
        weapon.services.runtime_flags &= ~0x20000u;
        untranslated(0x0012ADC0, "the bob entering state 7 (0x000C6100)");
    case 0xE:
        untranslated(0x000C63A0, "the bob entering state 14 (0x000C6100)");
    case 0x10:
        start_runtime_weapon_idle(weapon); // 0x00144D00
        return;
    default:
        enter_runtime_weapon_state(weapon, state, previous); // 0x0014A120
        return;
    }
}

// Xbox 0x000C61B0
void leave_runtime_bob_weapon_state(RuntimeWeaponActor& weapon, u16 state, u16 next)
{
    if (state == 0 || state == 0x10)
    {
        return;
    }
    leave_runtime_weapon_state(weapon, state, next); // 0x0014A180
}

// Xbox 0x000C5D50
void update_runtime_bob_weapon_state(RuntimeWeaponActor& weapon, u16 state)
{
    auto bytes = weapon.bytes;
    switch (state)
    {
    case 1:
    {
        const float step = frame_step(weapon.frame);
        const float rate = weapon.actor.animation_pause > 0.f ? 0.f : weapon.actor.animation_rate;
        if (tick_runtime_cutscene(*weapon.cutscene, rate * step, nullptr, weapon.services.cutscenes())) // 0x0004DA50
        {
            release_cutscene(weapon);
            store16(bytes, 0xC0E, 0x10);
        }
        return;
    }
    case 7:
    {
        if (bytes[0x93C] != 0)
        {
            store16(bytes, 0xC0E, 0x10);
            return;
        }
        if (weapon.blocked_players > -1) // DAT_004E1C34.
        {
            store32(bytes, 0xF5C, std::bit_cast<u32>(1.f)); // DAT_0032B0F8.
            return;
        }
        float timer = read_f32(bytes, 0xF5C);
        runtime_frame_timer_elapsed(timer, weapon.actor, weapon.frame); // 0x00042360
        store32(bytes, 0xF5C, std::bit_cast<u32>(timer));
        if (timer != 0.f)
        {
            return;
        }
        untranslated(0x000C62C0, "the bob's state 7 update (0x000C5D50)");
    }
    case 0xE:
        if (actor_animation_completed(weapon.view.animation, 0)) // 0x0003B290
        {
            set_spawn_blocked(weapon, weapon.actor.spawn_blocked - 1);
            if (weapon.actor.spawn_blocked == 0)
            {
                register_actor_model_anchors(weapon.actor, weapon.view.anchors); // 0x0003EEB0
            }
            store8(bytes, 0x93C, 1);
            weapon.actor.mask_flag = i32((u32(weapon.actor.mask_flag) & ~0xFFu) | 1u); // Native alias of +0x93C.
            store16(bytes, 0xC0E, 0x10);
        }
        return;
    case 0x10:
        notify_runtime_weapon_grab_points(weapon); // 0x00147430
        return;
    default:
        return;
    }
}
}
