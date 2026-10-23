#include "container_weapon.h"
#include "weapon_actor_detail.h"
#include "actor_live_attachments.h"
#include "actor_model_anchors.h"
#include "actor_render_registration.h"
#include "actor_spawn_eligibility.h"
#include "actor_world_lists.h"
#include "body_contacts.h"
#include "body_damage_tables.h"
#include "weapon_breakage.h"
#include "../navigation/cell_registration.h"
#include "gbtg/sound_runtime.h"
#include <cstring>

namespace gbtg
{
using namespace weapon_detail;

namespace
{
namespace field = container_weapon_field;
namespace parameter = container_weapon_parameter;
constexpr u32 kScriptHold = 0; // DAT_00512278, BSS that no retail code writes.

// 0x00145FD0, EAX actor: the holder whose link actor +0x918 names, while that link is set.
Actor* holder_of(const RuntimeWeaponActor& weapon)
{
    const ActorLink* link = weapon.actor.link;
    return link != nullptr && link->head != nullptr ? link->owner : nullptr;
}

// A live player (+0xCE0 set, 0x00028D20) in the scripted modes (+0xCDC 2, or +0x93C set).
bool live_scripted_player(RuntimeWeaponActor& weapon, const Actor& actor)
{
    return read_u32(weapon.bytes_at(weapon.services.actor_address(actor)), 0xCE0) != 0
        && (actor.mask_mode == 2 || actor.mask_flag != 0);
}

// 0x0003B340 on slot 0: the slot's player resumes and the controller is marked dirty.
void resume_idle_slot(RuntimeWeaponActor& weapon)
{
    auto* controller = weapon.view.animation.controller.get(); // +0x50.
    if (controller == nullptr)
    {
        return;
    }
    if (!controller->slots.empty() && controller->slots[0].player != nullptr)
    {
        --controller->slots[0].player->paused;
    }
    controller->dirty = true;
}
}

// Xbox 0x000C75B0
void container_weapon_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = field::kSize;
    sizes.parameters = parameter::kSize;
}

// Xbox 0x000C7640
bool initialize_runtime_container_weapon(RuntimeWeaponActor& weapon, std::span<const u8> parameters)
{
    const char* contents = field_data(parameters, parameter::contents);
    std::strcpy(field_data(weapon.bytes, field::contents), contents);
    set(weapon.bytes, field::uses, get(parameters, parameter::uses));
    set(weapon.bytes, field::left, get(parameters, parameter::uses));
    set(weapon.bytes, field::caption, get(parameters, parameter::caption));
    weapon.bytes[0x942] = 1;
    return true;
}

// Xbox 0x000C7AF0, ECX actor
void show_runtime_container_uses(RuntimeWeaponActor& weapon)
{
    const Actor* holder = holder_of(weapon);
    if (holder != nullptr && live_scripted_player(weapon, *holder))
    {
        show_runtime_weapon_uses(weapon, get(weapon.bytes, field::left), get(weapon.bytes, field::caption)); // 0x0012DC80
    }
}

// Xbox 0x000C7A50, ESI actor
void drop_runtime_container_weapon(RuntimeWeaponActor& weapon)
{
    Actor& actor = weapon.actor;
    auto bytes = weapon.bytes;
    if (weapon.markers != nullptr)
    {
        deactivate_all_runtime_body_contact_shapes(*weapon.markers); // 0x0013F660, EBX +0x998
        std::array<u32, 0x42> message{};
        message[0] = 0x92;
        message[1] = 0;
        weapon.message(message); // Vtable +0xC.
    }
    bytes[0x93C] = 0;
    actor.mask_flag = i32(u32(actor.mask_flag) & ~0xFFu);
    if (bytes[0xD08] != 0)
    {
        const u16 voice = read_u16(bytes, 0xD88);
        bytes[0xD08] = 0;
        if (voice != 0xFFFF)
        {
            stop_actor_sound_voice(i16(voice), weapon.services.sounds); // 0x00039690
            store16(bytes, 0xD88, 0xFFFF);
        }
    }
    actor.model_mode = 0; // +0x118.
    register_actor_shape_probe(actor, weapon.view.shape); // 0x0003C770
    break_runtime_weapon(weapon); // 0x001454F0
}

// Xbox 0x000C7710
void dispatch_runtime_container_weapon_message(RuntimeWeaponActor& weapon, std::span<u32> message)
{
    auto bytes = weapon.bytes;
    switch (message[0])
    {
    case 0x91:
        change_runtime_weapon_state(weapon, 3); // 0x0014A290, EDI 3
        unregister_actor_segment_probes(weapon.view.render); // 0x0003E850, EAX actor
        break;
    case 0x97:
    {
        const Actor* owner = weapon.actor.spawn_owner; // +0xD8C.
        if (owner != nullptr && runtime_actor_is_live_player(*owner,
                read_u32(weapon.bytes_at(weapon.services.actor_address(*owner)), 0xCE0))) // 0x00028D20
        {
            show_runtime_weapon_uses(weapon, 0, get(bytes, field::caption)); // 0x0012DC80, EAX 0
        }
        change_runtime_weapon_state(weapon, 6); // 0x0014A290, EDI 6
        return;
    }
    case 0x99:
    {
        if (bytes[0x93C] == 0)
        {
            break;
        }
        // Word +4 names the asker's buffer; it gets the container's contents.
        char* output = static_cast<char*>(weapon.services.bound_at(message[1]));
        if (output == nullptr)
        {
            throw Error("container message 0x99 names a buffer that is not bound");
        }
        std::strcpy(output, field_data(bytes, field::contents));
        const Actor* holder = holder_of(weapon); // 0x00145FD0
        if (holder != nullptr && runtime_actor_is_player(*holder) && kScriptHold != 0) // 0x00028D00, DAT_00512278
        {
            break;
        }
        if (get(bytes, field::left) > 0)
        {
            set(bytes, field::left, get(bytes, field::left) - 1);
        }
        show_runtime_container_uses(weapon); // 0x000C7AF0
        if (get(bytes, field::left) > 0)
        {
            break;
        }
        const Actor* dropping = holder_of(weapon); // 0x00145FD0
        if (dropping == nullptr)
        {
            throw Error("container message 0x99 empties a container with no holder for 0x0001B4C0");
        }
        set_runtime_weapon_mask(weapon, 0); // 0x00146230, EAX 0
        weapon.services.detach_from_holder(weapon.services.actor_address(*dropping)); // 0x0001B4C0, ESI holder +0x95C
        activate_actor_bind_tree(weapon.actor); // 0x00112CC0
        break;
    }
    case 0x9D:
        change_runtime_weapon_state(weapon, 7); // 0x0014A290, EDI 7
        break;
    default:
        break;
    }
    dispatch_runtime_weapon_message(weapon, message); // 0x001463D0
}

// Xbox 0x000C76A0
void update_runtime_container_weapon_state(RuntimeWeaponActor& weapon, u16 state)
{
    show_runtime_container_uses(weapon); // 0x000C7AF0, ECX actor
    switch (state)
    {
    case 3:
    case 6:
    case 9:
    case 0xC:
        return;
    case 7:
        store16(weapon.bytes, 0xC0E, 0x10);
        return;
    case 0x10:
        notify_runtime_weapon_grab_points(weapon); // The tail jump to 0x00147430.
        return;
    default:
        update_runtime_weapon_state(weapon, state); // 0x0014A220
        return;
    }
}

// Xbox 0x000C7870
void enter_runtime_container_weapon_state(RuntimeWeaponActor& weapon, u16 state, u16 previous)
{
    auto bytes = weapon.bytes;
    switch (state)
    {
    case 6:
        if (read_u32(bytes, 0x9A0) == 0 && actor_has_spawn_model(weapon.actor)) // The inlined 0x00145470.
        {
            drop_runtime_container_weapon(weapon); // 0x000C7A50
        }
        bytes[0x93C] = 0;
        weapon.actor.mask_flag = i32(u32(weapon.actor.mask_flag) & ~0xFFu);
        if (bytes[0xD08] != 0)
        {
            const u16 voice = read_u16(bytes, 0xD88);
            bytes[0xD08] = 0;
            if (voice != 0xFFFF)
            {
                stop_actor_sound_voice(i16(voice), weapon.services.sounds); // 0x00039690
                store16(bytes, 0xD88, 0xFFFF);
            }
        }
        return;
    case 0xC:
        block_runtime_weapon_spawn(weapon); // 0x00147860
        weapon.actor.model_mode |= 4;
        register_actor_shape_probe(weapon.actor, weapon.view.shape); // 0x0003C770
        start_runtime_weapon_idle(weapon); // 0x00144D00
        return;
    case 0x10:
        register_runtime_weapon_cells(weapon); // 0x00145BC0
        weapon.actor.model_mode |= 4;
        register_actor_shape_probe(weapon.actor, weapon.view.shape); // 0x0003C770
        weapon.actor.model_flags |= 0x100u;
        start_runtime_weapon_idle(weapon); // 0x00144D00
        if (read_u32(bytes, 0xDA8) == 0)
        {
            attach_runtime_weapon_volume(weapon); // The tail jump to 0x00146310.
        }
        return;
    default:
        enter_runtime_weapon_state(weapon, state, previous); // 0x0014A120's cases, inlined.
        return;
    }
}

// Xbox 0x000C7990
void leave_runtime_container_weapon_state(RuntimeWeaponActor& weapon, u16 state, u16 next)
{
    auto bytes = weapon.bytes;
    if (state != 0)
    {
        if (state == 0xC)
        {
            if (read_u32(bytes, 0xDD0) == kAidPrefix)
            {
                resume_idle_slot(weapon); // +0x50's slot 0.
            }
            weapon.actor.spawn_blocked -= 1;
            store32(bytes, 0xAD0, u32(weapon.actor.spawn_blocked));
            if (weapon.actor.spawn_blocked == 0)
            {
                register_actor_model_anchors(weapon.actor, weapon.view.anchors); // 0x0003EEB0
            }
            return;
        }
        if (state != 0x10)
        {
            leave_runtime_weapon_state(weapon, state, next); // 0x0014A180's cases, inlined.
            return;
        }
        if (read_u32(bytes, 0xDD0) == kAidPrefix)
        {
            resume_idle_slot(weapon); // 0x0003B340
        }
        weapon.actor.model_flags &= ~0x100u;
        clear_actor_live_attachments(weapon.view.live_attachments); // 0x00042040
    }
    if (weapon.cells != nullptr)
    {
        entity_detail::release_navigation_cell_registration(weapon.cells, read_u32(bytes, 0xD00)); // 0x001108B0
    }
    weapon.actor.model_mode &= ~4;
    register_actor_shape_probe(weapon.actor, weapon.view.shape); // 0x0003C770, inlined
}
}
