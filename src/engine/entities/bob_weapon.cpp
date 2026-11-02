#include "bob_weapon.h"
#include "powerup_flags.h"
#include "weapon_actor_detail.h"
#include "actor_model_anchors.h"
#include "actor_transform.h"
#include "player_mind_requests.h"
#include "../cutscenes/event_pause.h"
#include "../cutscenes/event_runtime.h"
#include "../cutscenes/instance_construction.h"
#include "../scripts/script_event_tallies.h"
#include "weapon_breakage.h"
#include "entity.h"
#include "actor_attachment_events.h"
#include <string>

namespace gbtg
{
using namespace weapon_detail;

namespace
{
namespace field = bob_weapon_field;
namespace parameter = bob_weapon_parameter;

// The inline strcpy of a parameter name into the instance.
template <typename To, typename From>
void copy_name(std::span<u8> bytes, To to, std::span<const u8> parameters, From from)
{
    char* target = field_data(bytes, to);
    const char* source = field_data(parameters, from);
    size_t index = 0;
    do
    {
        target[index] = source[index];
    }
    while (source[index++] != 0);
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
    set(weapon.bytes, weapon_field::spawn_blocked, u32(value));
}

// The mix 0x000C62C0 and 0x000C63A0 build on the stack for 0x0003ADA0: no fades, mode 1, the +0xEDC events.
AnimationMix bob_mix(std::span<const u8> bytes, std::string& events)
{
    AnimationMix mix;
    mix.duration = 0.f;
    mix.weight = -10.f; // DAT_0032B1FC.
    mix.blend_mode = 0x21;
    mix.fade_duration = 0.f;
    mix.position = 0.f;
    mix.mode = 1;
    mix.priority = 0x31;
    mix.old_pose_duration = 0.f;
    events = field_data(bytes, field::animation_events);
    mix.events = events;
    mix.borrowed_animation = {};
    mix.borrowed_events = {};
    mix.record = nullptr;
    return mix;
}

void dispatch_callouts(RuntimeWeaponActor& weapon, i32 id)
{
    dispatch_actor_attachment_records(weapon.view.startup.resource.resource.data(), id, nullptr, nullptr,
        weapon.base().callouts, 0); // 0x0003BE20 over actor +0x8C
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
    set(bytes, weapon_field::contact_test, 0x000C61D0); // The class's contact test, called through +0x99C.
    set(bytes, weapon_field::unk_941, 1);
    set(bytes, field::holders, get(parameters, parameter::holders));
    copy_name(bytes, field::animation, parameters, parameter::animation);
    copy_name(bytes, field::animation_events, parameters, parameter::animation_events);
    copy_name(bytes, field::cutscene, parameters, parameter::cutscene);
    if ((weapon.services.story_flags & 8) != 0) // DAT_0050EA98.
    {
        CutsceneRuntimeContext& runtime = weapon.services.cutscenes();
        weapon.cutscene = load_runtime_cutscene(field_data(bytes, field::cutscene), 2, nullptr,
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
        switch (get(bytes, weapon_field::state))
        {
        case 3:
            set(bytes, field::holders, get(bytes, field::holders) + 1);
            break;
        case 7:
            change_runtime_weapon_state(weapon, 0x10); // 0x0014A290
            break;
        case 0x10:
            if (get(bytes, weapon_field::mask) != 0)
            {
                set(bytes, field::holders, get(bytes, field::holders) + 1);
                break;
            }
            change_runtime_weapon_state(weapon, 0xE); // 0x0014A290
            break;
        default:
            break;
        }
        break;
    case 0x87:
        if (message[2] == 0xD && get(bytes, weapon_field::state) == 0x10 && get(bytes, field::unk_FE4) == 0)
        {
            set(bytes, field::holders, get(bytes, field::holders) - 1);
            set(bytes, field::unk_FE4, 0);
        }
        break;
    case 0x91:
        change_runtime_weapon_state(weapon, 3); // 0x0014A290
        break;
    case 0x95:
        release_runtime_bob_weapon_holder(weapon); // 0x000C6250, EAX actor
        break;
    case 0x9D:
        change_runtime_weapon_state(weapon, 7); // 0x0014A290
        break;
    default:
        break;
    }
    dispatch_runtime_weapon_message(weapon, message); // 0x001463D0
}

// Xbox 0x000C61D0
u32 test_runtime_bob_contact(RuntimeWeaponActor& weapon, RuntimeSegmentContactActor& object, const Vec3& point,
    GhoulySpawnPool* contacts, const Vec3& velocity, u16 word)
{
    auto bytes = weapon.bytes;
    if (report_runtime_weapon_contact(weapon, weapon_holder(weapon.actor), object, point, contacts, velocity, word,
                                      get(bytes, weapon_field::hit_kind), get(bytes, weapon_field::hit_strength),
                                      get(bytes, weapon_field::hit_style),
                                      get(bytes, field::hits) == 0) != 0) // 0x00141890
    {
        set(bytes, field::hits, get(bytes, field::hits) + 1);
    }
    return get(bytes, weapon_field::mask) == 0 ? 1u : 0u;
}

// Xbox 0x000C6250, EAX actor
void release_runtime_bob_weapon_holder(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    set(bytes, field::holders, get(bytes, field::holders) - 1);
    if (get(bytes, field::holders) == 0)
    {
        block_runtime_weapon_spawn(weapon); // 0x00147860, EBX actor
        set_runtime_weapon_mask(weapon, 0); // The inlined 0x00146230: +0x93C cleared, the +0xD88 voice stopped.
        return;
    }
    if (runtime_weapon_boost_active() != 0) // DAT_00512278.
    {
        set(bytes, field::unk_FE4, 1);
    }
}

// Xbox 0x000C62C0, EAX actor
void finish_runtime_bob_weapon_rest(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    if (read_u32(bytes, field::animation.offset) != 0x5F646961u) // "aid_"
    {
        return;
    }
    std::string events;
    const AnimationMix mix = bob_mix(bytes, events);
    start_actor_animation_mix(weapon.view.animation, 0, mix, field_data(bytes, field::animation), weapon.registry,
        weapon.error, weapon.random); // 0x0003ADA0, EBX actor, EDI the mix
    dispatch_callouts(weapon, 0xD);
    award_runtime_weapon_points(weapon, 1); // 0x001456D0, ESI actor
}

// Xbox 0x000C63A0, EAX actor
void start_runtime_bob_weapon_rest(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    if (read_u32(bytes, field::animation.offset) != 0x5F646961u) // "aid_"
    {
        return;
    }
    std::string events;
    AnimationMix mix = bob_mix(bytes, events);
    AnimationController* controller = weapon.view.animation.controller.get(); // Actor +0x50.
    if (controller != nullptr && !controller->slots.empty() && controller->slots[0].player != nullptr)
    {
        mix.position = controller->slots[0].player->position;
    }
    start_actor_animation_mix(weapon.view.animation, 0, mix, field_data(bytes, field::animation), weapon.registry,
        weapon.error, weapon.random); // 0x0003ADA0, EBX actor, EDI the mix
    controller = weapon.view.animation.controller.get();
    if (controller != nullptr)
    {
        if (!controller->slots.empty() && controller->slots[0].player != nullptr)
        {
            MinaPlayer& player = *controller->slots[0].player;
            player.speed = -engine_absolute(player.speed); // 0x00152BB0, FCHS: played backwards
        }
        controller->dirty = true; // +0x5C.
    }
    set(bytes, field::holders, get(bytes, field::holders) + 1);
    set(bytes, field::unk_FE4, 1);
    dispatch_callouts(weapon, 0x4B);
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
        if (weapon.actor.spawn_owner == nullptr)
        {
            throw Error("0x000C6100 reads a missing +0xD8C owner's tag through address zero");
        }
        tally_runtime_script_kind(6, weapon.actor.spawn_owner->class_key); // 0x0012ADC0, ECX 6, DX owner +6
        return;
    case 0xE:
        start_runtime_bob_weapon_rest(weapon); // 0x000C63A0, EAX actor
        return;
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
            set(bytes, weapon_field::next_state, 0x10);
        }
        return;
    }
    case 7:
    {
        if (get(bytes, weapon_field::mask) != 0)
        {
            set(bytes, weapon_field::next_state, 0x10);
            return;
        }
        if (weapon.blocked_players > -1) // DAT_004E1C34.
        {
            set(bytes, field::state_7_timer, 1.f); // DAT_0032B0F8.
            return;
        }
        float timer = get(bytes, field::state_7_timer);
        runtime_frame_timer_elapsed(timer, weapon.actor, weapon.frame); // 0x00042360
        set(bytes, field::state_7_timer, timer);
        if (timer != 0.f)
        {
            return;
        }
        finish_runtime_bob_weapon_rest(weapon); // 0x000C62C0, EAX actor
        set(bytes, weapon_field::next_state, 0x10);
        return;
    }
    case 0xE:
        if (actor_animation_completed(weapon.view.animation, 0)) // 0x0003B290
        {
            set_spawn_blocked(weapon, weapon.actor.spawn_blocked - 1);
            if (weapon.actor.spawn_blocked == 0)
            {
                register_actor_model_anchors(weapon.actor, weapon.view.anchors); // 0x0003EEB0
            }
            set(bytes, weapon_field::mask, 1);
            weapon.actor.mask_flag = i32((u32(weapon.actor.mask_flag) & ~0xFFu) | 1u); // Native alias of +0x93C.
            set(bytes, weapon_field::next_state, 0x10);
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
