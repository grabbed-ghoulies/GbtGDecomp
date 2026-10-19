#include "projectile_weapon.h"
#include "weapon_actor_detail.h"
#include "weapon_breakage.h"
#include "actor_attachment_events.h"
#include "actor_live_attachments.h"
#include "actor_model_anchors.h"
#include "actor_render_registration.h"
#include "actor_spawn_eligibility.h"
#include "actor_transform.h"
#include "actor_world_lists.h"
#include "body_animation_commands.h"
#include "body_contacts.h"
#include "body_damage_tables.h"
#include "body_resources.h"
#include "entity_heap.h"
#include "../frontend/player_health_display.h"
#include "../frontend/scoreboard_update.h"

namespace gbtg
{
using namespace weapon_detail;

namespace
{
// 0x0003C040 with the actor's +0x8C resource: its callout records for an id.
void send_callout(RuntimeWeaponActor& weapon, i32 id)
{
    dispatch_actor_resource_callout(weapon.view.startup.resource, id, nullptr, weapon.base().callouts);
}

// The scoreboard's weapon counter (DAT_00432688..DAT_004326E4) at a count.
void show_weapon_counter(RuntimeWeaponActor& weapon, i32 count)
{
    auto& hud = weapon.services.scoreboard;
    hud.damage_690 = 2.f; // DAT_0032B110.
    hud.damage_counter.target = count;
    hud.damage_counter.value = count;
    update_runtime_hud_counter_digits(hud.damage_counter, weapon.services.damage_preset); // 0x0012F990
    hud.damage_caption = 7;
    load_runtime_scoreboard_weapon_icons(hud, weapon.registry, weapon.error); // 0x0012D8A0
    hud.damage_6e0 = 2;
}

// The holder (+0xD8C) is a live player (0x00028D20 on its +0xCE0).
bool holder_is_live_player(RuntimeWeaponActor& weapon)
{
    Actor* owner = weapon.actor.spawn_owner; // +0xD8C.
    return owner != nullptr && runtime_actor_is_live_player(*owner,
        read_u32(weapon.bytes_at(weapon.services.actor_address(*owner)), 0xCE0));
}

// 0x0003B340 on slot 0: the slot's player resumes and the controller is marked dirty.
void resume_idle_slot(RuntimeWeaponActor& weapon)
{
    auto* controller = weapon.view.animation.controller.get();
    if (controller != nullptr)
    {
        if (!controller->slots.empty() && controller->slots[0].player != nullptr)
        {
            --controller->slots[0].player->paused;
        }
        controller->dirty = true;
    }
}

// 0x0003C750: actor +0x118 gains the bits and the shape probe is registered again.
void raise_model_mode(RuntimeWeaponActor& weapon, i32 bits)
{
    weapon.actor.model_mode |= bits;
    register_actor_shape_probe(weapon.actor, weapon.view.shape); // 0x0003C770
}

void set_spawn_blocked(RuntimeWeaponActor& weapon, i32 value)
{
    weapon.actor.spawn_blocked = value;
    store32(weapon.bytes, 0xAD0, u32(value));
}
}

// Xbox 0x000CD620
void large_projectile_weapon_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = 0xF08;
    sizes.parameters = 0x604;
}

// Xbox 0x000CD6B0
bool initialize_runtime_large_projectile(RuntimeWeaponActor& weapon, std::span<const u8> parameters)
{
    auto bytes = weapon.bytes;
    store32(bytes, 0x99C, 0x000CE5D0); // The class's contact test, called through +0x99C.
    store16(bytes, 0xE84, 0);
    store32(bytes, 0xED4, read_u32(parameters, 0x5F8));
    store32(bytes, 0xED8, read_u32(parameters, 0x5FC));
    store32(bytes, 0xEDC, read_u32(parameters, 0x600));
    return true;
}

// Xbox 0x000CDAA0
void destroy_runtime_large_projectile(RuntimeWeaponActor& weapon)
{
    if (weapon.collision_overrides != nullptr)
    {
        restore_runtime_body_collision_overrides(*weapon.collision_overrides); // 0x0013EE80
        engine_heap_free(weapon.collision_overrides); // 0x000663E0
        weapon.collision_overrides = nullptr;
    }
}

// Xbox 0x001454B0, ECX actor
void mark_runtime_weapon_player_category(RuntimeWeaponActor& weapon)
{
    const Actor* owner = weapon.actor.spawn_owner; // +0xD8C.
    store16(weapon.bytes, 0xAA8, owner != nullptr && runtime_actor_is_player(*owner) ? 1 : 0);
}

// Xbox 0x000CDF40, EAX actor
void start_runtime_large_projectile_idle(RuntimeWeaponActor& weapon)
{
    // The body of 0x00144D40 inlined over +0xDD0, through 0x0003ADA0.
    if (read_u32(weapon.bytes, 0xDD0) == kAidPrefix)
    {
        start_runtime_weapon_animation(weapon, reinterpret_cast<const char*>(weapon.bytes.data() + 0xDD0));
    }
}

// Xbox 0x000CEEC0, EAX actor
void show_runtime_large_projectile_holder(RuntimeWeaponActor& weapon)
{
    const ActorLink* link = weapon.actor.link; // +0x918.
    if (link == nullptr || link->head == nullptr || link->owner == nullptr)
    {
        return;
    }
    const Actor& holder = *link->owner;
    if (read_u32(weapon.bytes_at(weapon.services.actor_address(holder)), 0xCE0) == 0
        || !runtime_actor_is_player(holder))
    {
        return;
    }
    show_weapon_counter(weapon, 1);
}

// Xbox 0x000CDBE0
void dispatch_runtime_large_projectile_message(RuntimeWeaponActor& weapon, std::span<u32> message)
{
    auto bytes = weapon.bytes;
    switch (message[0])
    {
    case 0x49:
        if (message[0xB] == 0)
        {
            break;
        }
        send_callout(weapon, 0x35); // 0x0003C040
        if (message[0xD] == 0 || read_f32(bytes, 0xE74) != 0.f) // DAT_0032B100.
        {
            break;
        }
        set_runtime_weapon_mask(weapon, 0); // 0x00146230
        change_runtime_weapon_state(weapon, 4); // 0x0014A290
        break;
    case 0x7C:
        if (weapon.collision_overrides != nullptr)
        {
            remove_runtime_body_contact_override(weapon.collision_overrides,
                *weapon.actor_at(message[1])); // 0x0013EDD0
        }
        break;
    case 0x91:
        change_runtime_weapon_state(weapon, 3); // 0x0014A290
        break;
    case 0x97:
        if (holder_is_live_player(weapon))
        {
            show_weapon_counter(weapon, 0);
        }
        change_runtime_weapon_state(weapon, 6); // 0x0014A290
        return;
    case 0x9C:
        change_runtime_weapon_state(weapon, 7); // 0x0014A290
        break;
    case 0x9D:
        untranslated(0x000CE660, "the large projectile's message 0x9D (0x000CDBE0)");
    case 0xA0:
    {
        if (read_u16(bytes, 0xC0C) != 0x10)
        {
            break;
        }
        store32(bytes, 0xEC0, message[3]);
        store32(bytes, 0xEC4, message[4]);
        store32(bytes, 0xEC8, message[5]);
        const float offset = read_f32(bytes, 0xED8) + read_f32(bytes, 0xEC0);
        store32(bytes, 0xEC0, std::bit_cast<u32>(offset));
        store32(bytes, 0xECC, 0);
        store32(bytes, 0xED0, message[6]);
        store32(bytes, 0xEA4, message[2]);
        const Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
        std::memcpy(bytes.data() + 0xEB4, &position, 12);
        const Vec3 euler = actor_world_euler(weapon.actor); // 0x00041820
        std::memcpy(bytes.data() + 0xEA8, &euler, 12);
        change_runtime_weapon_state(weapon, 0xF); // 0x0014A290
        store32(weapon.bytes_at(message[2]), 0x314, weapon.self_address());
        message[1] = 1;
        break;
    }
    case 0xA4:
        insert_runtime_body_contact_override(weapon.collision_overrides, weapon.contact_actor_at(message[1]), 0,
            weapon.players); // 0x0013EBE0
        break;
    case 0xA5:
        if (holder_is_live_player(weapon))
        {
            show_weapon_counter(weapon, 0);
        }
        store32(bytes, 0xE54, message[1]);
        store32(bytes, 0xE58, message[2]);
        store32(bytes, 0xE5C, message[3]);
        change_runtime_weapon_state(weapon, 2); // 0x0014A290
        send_callout(weapon, 0x4F); // 0x0003C040
        break;
    default:
        break;
    }
    dispatch_runtime_weapon_message(weapon, message); // 0x001463D0
}

// Xbox 0x000CD700
void enter_runtime_large_projectile_state(RuntimeWeaponActor& weapon, u16 state, u16 previous)
{
    switch (state)
    {
    case 0:
    case 0xB:
    case 0xF:
        return;
    case 2:
        if (previous == 2)
        {
            untranslated(0x000CE1F0, "the large projectile entering state 2 again (0x000CD700)");
        }
        untranslated(0x000CE020, "the large projectile entering state 2 (0x000CD700)");
    case 3:
        if (Actor* owner = weapon.actor.spawn_owner; owner != nullptr) // +0xD8C.
        {
            const std::span<u8> body = weapon.bytes_at(read_u32(weapon.bytes_at(weapon.services.actor_address(*owner)),
                0x95C));
            const u32 self = weapon.self_address();
            store32(body, 0x4C, self);
            store32(body, 0x14, self);
        }
        return;
    case 4:
        if (actor_has_spawn_model(weapon.actor)) // 0x00145470
        {
            mark_runtime_weapon_player_category(weapon); // 0x001454B0
            break_runtime_weapon(weapon); // 0x001454F0
        }
        untranslated(0x000CEBD0, "the large projectile entering state 4 (0x000CD700)");
    case 6:
        if (actor_has_spawn_model(weapon.actor)) // 0x00145470
        {
            mark_runtime_weapon_player_category(weapon); // 0x001454B0
            break_runtime_weapon(weapon); // 0x001454F0
        }
        set_runtime_weapon_mask(weapon, 0); // 0x00146230
        return;
    case 7:
        untranslated(0x000CE7E0, "the large projectile entering state 7 (0x000CD700)");
    case 0xC:
        block_runtime_weapon_spawn(weapon); // 0x00147860
        raise_model_mode(weapon, 4); // 0x0003C750
        start_runtime_large_projectile_idle(weapon); // 0x000CDF40
        return;
    case 0x10:
        raise_model_mode(weapon, 4); // 0x0003C750
        weapon.actor.model_flags |= 0x100u;
        start_runtime_large_projectile_idle(weapon); // 0x000CDF40
        if (read_u32(weapon.bytes, 0xDA8) == 0)
        {
            attach_runtime_weapon_volume(weapon); // 0x00146310
        }
        return;
    default:
        enter_runtime_weapon_state(weapon, state, previous); // 0x0014A120
        return;
    }
}

// Xbox 0x000CD8A0
void leave_runtime_large_projectile_state(RuntimeWeaponActor& weapon, u16 state, u16 next)
{
    auto bytes = weapon.bytes;
    switch (state)
    {
    case 0:
        weapon.actor.model_mode &= ~4;
        register_actor_shape_probe(weapon.actor, weapon.view.shape); // 0x0003C770
        return;
    case 2:
        if (next != 2)
        {
            untranslated(0x0003E900, "the large projectile leaving state 2 (0x000CD8A0)");
        }
        return;
    case 3:
    {
        Actor* owner = weapon.actor.spawn_owner; // +0xD8C.
        if (owner == nullptr)
        {
            return;
        }
        const std::span<u8> body = weapon.bytes_at(read_u32(weapon.bytes_at(weapon.services.actor_address(*owner)),
            0x95C));
        store32(body, 0x4C, 0);
        store32(body, 0x14, 0);
        if (read_u32(body, 0x48) == weapon.self_address())
        {
            untranslated(0x0001B680, "the large projectile leaving its holder's hand (0x000CD8A0)");
        }
        return;
    }
    case 4:
        if (weapon.collision_overrides != nullptr)
        {
            restore_runtime_body_collision_overrides(*weapon.collision_overrides); // 0x0013EE80
            engine_heap_free(weapon.collision_overrides); // 0x000663E0
            weapon.collision_overrides = nullptr;
        }
        return;
    case 0xC:
        if (read_u32(bytes, 0xDD0) == kAidPrefix)
        {
            resume_idle_slot(weapon); // 0x0003B340
        }
        set_spawn_blocked(weapon, weapon.actor.spawn_blocked - 1);
        if (weapon.actor.spawn_blocked == 0)
        {
            register_actor_model_anchors(weapon.actor, weapon.view.anchors); // 0x0003EEB0
        }
        return;
    case 0xF:
        return;
    case 0x10:
        if (read_u32(bytes, 0xDD0) == kAidPrefix)
        {
            resume_idle_slot(weapon); // 0x0003B340
        }
        weapon.actor.model_mode &= ~4;
        register_actor_shape_probe(weapon.actor, weapon.view.shape); // 0x0003C770
        weapon.actor.model_flags &= ~0x100u;
        clear_actor_live_attachments(weapon.view.live_attachments); // 0x00042040
        return;
    default:
        leave_runtime_weapon_state(weapon, state, next); // 0x0014A180
        return;
    }
}

// Xbox 0x000CDA10
void update_runtime_large_projectile_state(RuntimeWeaponActor& weapon, u16 state)
{
    show_runtime_large_projectile_holder(weapon); // 0x000CEEC0
    switch (state)
    {
    case 2:
        untranslated(0x000CDAD0, "the large projectile's state 2 update (0x000CDA10)");
    case 3:
    case 7:
    case 0xB:
    case 0xC:
        return;
    case 4:
        untranslated(0x000CEC70, "the large projectile's state 4 update (0x000CDA10)");
    case 6:
        store16(weapon.bytes, 0xC0E, 0xB);
        return;
    case 0xF:
        untranslated(0x000CE810, "the large projectile's state 15 update (0x000CDA10)");
    case 0x10:
        notify_runtime_weapon_grab_points(weapon); // 0x00147430
        return;
    default:
        update_runtime_weapon_state(weapon, state); // 0x0014A220
        return;
    }
}
}
