#include "projectile_weapon.h"
#include "powerup_flags.h"
#include "weapon_actor_detail.h"
#include "weapon_breakage.h"
#include "actor_attachment_events.h"
#include "actor_live_attachments.h"
#include "actor_model_anchors.h"
#include "actor_render_registration.h"
#include "actor_spawn_eligibility.h"
#include "actor_transform.h"
#include "actor_shape_bounds.h"
#include "gbtg/actor_runtime.h"
#include "body_hit_state.h"
#include "actor_sound_pose.h"
#include "actor_trail_requests.h"
#include "body_animation_commands.h"
#include "entity.h"
#include "entity_collision.h"
#include "segment_records.h"
#include "gbtg/sound_runtime.h"
#include <cmath>
#include "actor_world_lists.h"
#include "body_animation_commands.h"
#include "body_contacts.h"
#include "body_damage_tables.h"
#include "body_resources.h"
#include "entity_heap.h"
#include "../frontend/player_health_display.h"
#include "../frontend/scoreboard_update.h"
#include "body_fields.h"

namespace gbtg
{
using namespace weapon_detail;

namespace
{
namespace field = large_projectile_field;
namespace parameter = large_projectile_parameter;

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
    set(bytes, field::bounces_left, i16(0));
    set(bytes, field::hold_turn, get(parameters, parameter::hold_turn));
    set(bytes, field::hold_forward, get(parameters, parameter::hold_forward));
    set(bytes, field::hold_up, get(parameters, parameter::hold_up));
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
        if (message[0xD] == 0 || get(bytes, field::shatter_timer) != 0.f) // DAT_0032B100.
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
        record_runtime_large_projectile_root(weapon,
            {reinterpret_cast<u8*>(field_data(bytes, field::rest_euler)), field::rest_euler.size},
            {reinterpret_cast<u8*>(field_data(bytes, field::rest_position)), field::rest_position.size}); // 0x000CE660
        break;
    case 0xA0:
    {
        if (read_u16(bytes, 0xC0C) != 0x10)
        {
            break;
        }
        Vec3 hold{std::bit_cast<float>(message[3]), std::bit_cast<float>(message[4]),
            std::bit_cast<float>(message[5])};
        hold.x = get(bytes, field::hold_forward) + hold.x;
        set(bytes, field::hold_offset, hold);
        set(bytes, field::rise_elapsed, 0.f);
        set(bytes, field::rise_duration, std::bit_cast<float>(message[6]));
        set(bytes, field::catcher, message[2]);
        const Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
        set(bytes, field::catch_position, position);
        const Vec3 euler = actor_world_euler(weapon.actor); // 0x00041820
        set(bytes, field::catch_euler, euler);
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
        set(bytes, field::throw_target, Vec3{std::bit_cast<float>(message[1]), std::bit_cast<float>(message[2]),
            std::bit_cast<float>(message[3])});
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
            bounce_runtime_large_projectile(weapon); // 0x000CE1F0, EBX actor
            send_callout(weapon, 0xC); // 0x0003C040, EDX 0xC
            return;
        }
        clear_actor_animation(weapon.view.animation, weapon.actor.model != nullptr ? &weapon.view.pose : nullptr,
            weapon.registry); // 0x0003AC20, EAX actor
        launch_runtime_large_projectile(weapon); // 0x000CE020
        if (weapon.markers != nullptr) // 0x00144F30 with ECX 0xFF: every contact shape on, then {0x92, 1}.
        {
            activate_all_runtime_body_contact_shapes(weapon.actor, *weapon.markers, 0xFF); // 0x0013F5B0
            std::array<u32, 0x42> shapes{};
            shapes[0] = 0x92;
            shapes[1] = 1;
            weapon.message(shapes);
        }
        set_runtime_weapon_mask(weapon, 0); // 0x00146230
        {
            // 0x00041F90 with actor +0x208: a second instance of the actor's own model.
            auto& startup = weapon.services.startup();
            auto& models = weapon.view.startup.models;
            set_runtime_actor_secondary_model(models, models.name.data(), startup.registry, startup.error,
                startup.models, startup.deferred, startup.fence, startup.types, startup.shaders, startup.scratch,
                startup.heap_free);
            if (models.secondary != nullptr) // +0x310.
            {
                weapon.view.startup.secondary_mode = 1; // +0x30C.
            }
        }
        register_actor_wide_list(weapon.view.render); // 0x0003E8C0, the tail jump
        return;
    case 3:
        if (Actor* owner = weapon.actor.spawn_owner; owner != nullptr) // +0xD8C.
        {
            // Holder +0x95C -> +0x4C and +0x14: this weapon.
            weapon.services.hold_secondary(*owner, weapon.self_address());
        }
        return;
    case 4:
        if (actor_has_spawn_model(weapon.actor)) // 0x00145470
        {
            mark_runtime_weapon_player_category(weapon); // 0x001454B0
            break_runtime_weapon(weapon); // 0x001454F0
        }
        start_runtime_large_projectile_shatter(weapon); // 0x000CEBD0
        return;
    case 6:
        if (actor_has_spawn_model(weapon.actor)) // 0x00145470
        {
            mark_runtime_weapon_player_category(weapon); // 0x001454B0
            break_runtime_weapon(weapon); // 0x001454F0
        }
        set_runtime_weapon_mask(weapon, 0); // 0x00146230
        return;
    case 7:
        restore_runtime_large_projectile_root(weapon); // 0x000CE7E0
        return;
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
            unregister_actor_wide_list(weapon.view.render); // 0x0003E900, EAX actor
        }
        return;
    case 3:
    {
        Actor* owner = weapon.actor.spawn_owner; // +0xD8C.
        if (owner == nullptr)
        {
            return;
        }
        // Holder +0x95C -> +0x4C and +0x14 cleared.
        weapon.services.hold_secondary(*owner, 0);
        const auto body = weapon.services.body_of(weapon.services.actor_address(*owner));
        if (get(body, body_field::attached) == weapon.self_address())
        {
            weapon.services.empty_hand(*owner); // 0x0001B680, ECX the holder's body
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

// Xbox 0x001058E0, ESI degrees, EDI the output
Vec3 runtime_degrees_to_turns(const Vec3& degrees)
{
    const float tau = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
    const float radians = std::bit_cast<float>(0x3C8EFA35u); // DAT_0032B0FC.
    const auto wrap = [&](float value)
    {
        // x87: the product is stored as a float before 0x00152BE0.
        const long double remainder = animation_remainder(float(static_cast<long double>(value) * radians), tau);
        float result = float(remainder);
        if (0.0L > remainder) // DAT_0032B100.
        {
            result += tau;
        }
        return result;
    };
    return Vec3{wrap(degrees.x), wrap(degrees.y), wrap(degrees.z)};
}

// Xbox 0x000CE020
void launch_runtime_large_projectile(RuntimeWeaponActor& weapon)
{
    const auto bytes = weapon.bytes;
    Actor& actor = weapon.actor;
    const Vec3 spin = runtime_degrees_to_turns(Vec3{267.f, 0.f, 0.f}); // 0x003037F8
    set(bytes, field::fall_speed, -437.f); // DAT_0032B65C, the fall.
    set(bytes, field::spin, spin);
    set(bytes, field::bounces_left, i16(3)); // Bounces left.
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const Vec3 target = get(bytes, field::throw_target);
    const Vec3 direction{target.x - position.x, target.y - position.y, target.z - position.z};
    float yaw = get(bytes, field::heading);
    runtime_direction_angle(direction, yaw); // 0x00105F90, EBX +0xE64
    set(bytes, field::heading, yaw);
    Vec3 velocity{0.f, 0.f, 0.f};
    add_sound_direction(velocity, 0.f, yaw, 167.f); // 0x00106220
    actor.anchor = velocity; // +0x3C..+0x44.
    // 0x0003BE20 over actor +0x8C, id 0x4A, with 0x0013B8B0 recording the attachments at +0xEE0.
    ActorAttachmentEventContext events = weapon.base().callouts;
    events.command_attachments = record(bytes, field::command_attachments);
    dispatch_actor_attachment_records(weapon.view.startup.resource.resource.data(), 0x4A,
        record_runtime_body_attachment_event, nullptr, events,
        weapon.self_address() + field::command_attachments.offset);
}

// Xbox 0x000CE150, EAX the bounce record, EDI actor
void bounce_runtime_large_projectile_velocity(RuntimeWeaponActor& weapon, float pitch_degrees, float keep)
{
    Actor& actor = weapon.actor;
    const Vec3 v = actor.anchor;
    // x87: the speed, then times the record's +0x1C, stored as a float.
    const long double speed = std::sqrt((static_cast<long double>(v.z) * v.z + static_cast<long double>(v.y) * v.y)
        + static_cast<long double>(v.x) * v.x);
    const float length = float(speed * keep);
    Vec3 velocity{0.f, 0.f, 0.f};
    add_sound_direction(velocity, pitch_degrees * std::bit_cast<float>(0x3C8EFA35u), get(weapon.bytes, field::heading),
        length); // 0x00106220, DAT_0032B0FC
    actor.anchor = velocity;
}

// Xbox 0x000CE1F0, EBX actor
void bounce_runtime_large_projectile(RuntimeWeaponActor& weapon)
{
    const auto bytes = weapon.bytes;
    Vec3 spin = get(bytes, field::spin);
    spin.x = spin.x * 0.8f; // DAT_0032B244.
    const Vec3 kick = runtime_degrees_to_turns(Vec3{0.f, 73.f, 0.f}); // 0x00303818
    spin.x = kick.x + spin.x;
    spin.y = spin.y + kick.y;
    spin.z = spin.z + kick.z;
    set(bytes, field::spin, spin);
    bounce_runtime_large_projectile_velocity(weapon, 320.f, 0.8f); // 0x000CE150 with the 0x003037E8 record
}

// Xbox 0x000CE510, EBX actor
void spin_runtime_large_projectile(RuntimeWeaponActor& weapon)
{
    const auto bytes = weapon.bytes;
    Actor& actor = weapon.actor;
    const float step = runtime_actor_step(actor, weapon.clock);
    Vec3 euler = actor_world_euler(actor); // 0x00041820, ESI actor
    const Vec3 spin = get(bytes, field::spin);
    euler.x = spin.x * step + euler.x;
    euler.y = spin.y * step + euler.y;
    euler.z = spin.z * step + euler.z;
    normalize_euler_angles(actor.euler, euler); // 0x00105AA0
    actor.xform_valid = 0; // +0x2A8.
}

// Xbox 0x000CE290, EAX actor, ECX the last position
i32 collide_runtime_large_projectile(RuntimeWeaponActor& weapon, Vec3& last, Vec3& next, float radius)
{
    const auto bytes = weapon.bytes;
    const u8 settled = get(bytes, field::settled);
    set(bytes, field::settled, 1);
    if (settled == 0)
    {
        if (Actor* holder = weapon.actor.spawn_owner; holder != nullptr) // +0xD8C.
        {
            // From the holder's position at the last height to the last position; the result is not tested.
            Vec3 from = actor_world_position(*holder); // 0x000412A0
            from.y = last.y;
            entity_record_sweep_probe(last, from, 0xC, 0x000C4000u); // 0x00122480, EBX the last position
        }
        if (entity_collect_sphere_contacts(last, radius, 0xC, 0x000C4000u) > 0) // 0x001227E0
        {
            auto contacts = entity_merge_contact_records(); // 0x00122310
            separate_runtime_segment_contacts(contacts, last, radius, next, actor_transform_origin); // 0x000FC3F0
            engine_heap_free(contacts.block); // 0x000663E0
            return 3;
        }
    }
    if (entity_record_movement_probe(next, last, std::bit_cast<u32>(radius), 0xC, 0x000C4000u) != 1) // 0x001226C0
    {
        return 1;
    }
    // The hit's normal y against the sine of 50 degrees: a wall breaks it, a floor bounces it.
    const float rise = read_f32({entity_detail::g_segment_slots[0].pool.begin, 0x54}, 0x10);
    if (retail_sin(std::bit_cast<float>(0x3F5F66F3u)) > static_cast<long double>(rise)) // 0x00153320
    {
        return 3;
    }
    return 2;
}

// Xbox 0x000CE380, ECX actor, EAX set on the last bounce
i32 step_runtime_large_projectile(RuntimeWeaponActor& weapon, bool last_bounce)
{
    const auto bytes = weapon.bytes;
    Actor& actor = weapon.actor;
    const float radius = last_bounce ? 1.f : 5.f; // DAT_0032B0F8, DAT_0032B184.
    Vec3 next = actor_world_position(actor); // 0x000412A0
    Vec3 last = next;
    const float step = runtime_actor_rate(actor) * runtime_frame_step(weapon.clock);
    const float drift = step * 0.f;
    const float half = step * 0.5f; // DAT_0032B0F4.
    const Vec3 v = actor.anchor;
    const float vy = get(bytes, field::fall_speed) * step + v.y;
    const float vx = drift + v.x;
    const float vz = drift + v.z;
    next.x = (v.x + vx) * half + next.x;
    next.y = (v.y + vy) * half + next.y;
    next.z = (v.z + vz) * half + next.z;
    actor.anchor = Vec3{vx, vy, vz};
    spin_runtime_large_projectile(weapon); // 0x000CE510
    Actor* holder = actor.spawn_owner; // +0xD8C keeps no +0x1F0 flags during the probes.
    u32 flags = 0;
    if (holder != nullptr)
    {
        flags = holder->model_flags;
        holder->model_flags = 0;
    }
    const i32 result = collide_runtime_large_projectile(weapon, last, next, radius); // 0x000CE290
    if (holder != nullptr)
    {
        holder->model_flags = flags;
    }
    actor_set_world_position(actor, next); // 0x00041730
    return result;
}

// Xbox 0x000CDAD0, EAX actor
void fly_runtime_large_projectile(RuntimeWeaponActor& weapon)
{
    const auto bytes = weapon.bytes;
    const float most = std::bit_cast<float>(0x3D6147AEu); // DAT_0032B5D0.
    // DAT_0032B3D4.
    const float trail = float(i32(get(bytes, field::bounces_left))) * std::bit_cast<float>(0x3EAAAAABu) * most;
    float scale;
    if (trail > most)
    {
        scale = most;
    }
    else
    {
        scale = 0.f;
        if (!(0.f > trail))
        {
            scale = trail;
        }
    }
    if (weapon.services.pal == nullptr || weapon.services.pal_override == nullptr || !weapon.services.trail)
    {
        throw Error("large projectile flight has no trail or PAL views");
    }
    const i32 rate = *weapon.services.pal == 0 || *weapon.services.pal_override != 0 ? 0x3C : 0x32;
    auto trails = weapon.services.trail(weapon.self_address());
    float timer = get(bytes, field::trail_timer);
    request_actor_trail(weapon.actor, scale, timer, rate / 2, trails); // 0x0013B030
    set(bytes, field::trail_timer, timer);
    const i32 result = step_runtime_large_projectile(weapon, u16(get(bytes, field::bounces_left)) == 0); // 0x000CE380
    if (result == 2)
    {
        const i16 left = i16(u16(get(bytes, field::bounces_left)));
        if (left < 1)
        {
            change_runtime_weapon_state(weapon, 4); // 0x0014A290
            return;
        }
        set(bytes, field::bounces_left, i16(u16(left - 1)));
        change_runtime_weapon_state(weapon, 2); // 0x0014A290: bounce
    }
    else if (result == 3)
    {
        // Vtable +0x40 / +0x3C straight to state 4.
        store16(bytes, 0xC0E, 0);
        leave_state(weapon, read_u16(bytes, 0xC0C), 4);
        const u16 previous = read_u16(bytes, 0xC0C);
        store16(bytes, 0xC0C, 4);
        store32(bytes, 0xC14, 0);
        enter_state(weapon, 4, previous);
    }
}

// Xbox 0x000CEBD0, ESI actor
void start_runtime_large_projectile_shatter(RuntimeWeaponActor& weapon)
{
    const auto bytes = weapon.bytes;
    weapon.actor.anchor = Vec3{0.f, 0.f, 0.f}; // DAT_0050DD80.
    float scale = 1.f; // DAT_0032B0F8.
    if (Actor* holder = weapon.actor.spawn_owner; holder != nullptr) // +0xD8C.
    {
        const u32 address = weapon.services.actor_address(*holder);
        insert_runtime_body_contact_override(weapon.collision_overrides, weapon.contact_actor_at(address), 0,
            weapon.players); // 0x0013EBE0, +0xE7C
        const auto holder_bytes = weapon.bytes_at(address);
        if (read_u32(holder_bytes, 0xCE0) != 0
            && (read_u16(holder_bytes, 0xCDC) == 2 || read_u32(holder_bytes, 0x93C) != 0) && runtime_weapon_boost_active() != 0) // DAT_00512278
        {
            scale = 2.f; // DAT_0032B110.
        }
    }
    set(bytes, field::shatter_scale, scale);
    set(bytes, field::shatter_timer, 0.6f); // DAT_0032B248.
}

// Xbox 0x000CEB10, EAX actor
void show_runtime_large_projectile_shatter(RuntimeWeaponActor& weapon)
{
    const auto bytes = weapon.bytes;
    Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
    // x87: the sweep radius times 0 (DAT_0032B100) plus the height, stored as a float.
    position.y = float(actor_model_sweep_radius(weapon.actor) * 0.0L + static_cast<long double>(position.y));
    const float scale = get(bytes, field::shatter_scale);
    const i32 id = scale > 1.f ? 0xB : 0xA; // DAT_0032B0F8.
    g_actor_creation_options.position = position;
    g_actor_creation_options.scale = scale;
    g_actor_creation_options.flags = 0x41;
    if (weapon.self_address() + 0x8C != 0)
    {
        dispatch_actor_attachment_records(weapon.view.startup.resource.resource.data(), id, nullptr,
            &g_actor_creation_options, weapon.base().callouts, 0); // 0x0003BE20
    }
    set(bytes, field::shatter_shown, 1);
}

// Xbox 0x000CEC70, EAX actor
bool update_runtime_large_projectile_shatter(RuntimeWeaponActor& weapon)
{
    const auto bytes = weapon.bytes;
    // DAT_0032B108, DAT_0032B118, DAT_0032B300.
    float reach = 30.f - get(bytes, field::shatter_timer) * 1.6666666f * 22.f;
    if (8.f > reach) // DAT_0032B26C.
    {
        reach = 8.f;
    }
    else if (reach > 30.f)
    {
        reach = 30.f;
    }
    const float radius = get(bytes, field::shatter_scale) * reach;
    // DAT_0032B248, DAT_0032B254.
    if (0.6f - get(bytes, field::shatter_timer) > 0.1f && get(bytes, field::shatter_shown) == 0)
    {
        show_runtime_large_projectile_shatter(weapon); // 0x000CEB10
    }
    Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
    if (weapon.collision_overrides != nullptr)
    {
        suspend_runtime_body_contact_overrides(*weapon.collision_overrides); // 0x0013EE10
    }
    const i32 count = entity_collect_contact_probes(position, radius, 0x00220142u); // 0x00122B00
    if (weapon.collision_overrides != nullptr)
    {
        restore_runtime_body_collision_overrides(*weapon.collision_overrides); // 0x0013EE80
    }
    std::vector<std::pair<entity_detail::SegmentProbe*, GhoulySpawnPool*>> found;
    for (i32 index = 0; index < count; ++index)
    {
        auto& slot = entity_detail::g_segment_slots[index];
        found.emplace_back(slot.object, copy_segment_records(slot.pool)); // 0x000FCC90
    }
    const Vec3 still{0.f, 0.f, 0.f}; // DAT_0050DD80.
    for (auto& [probe, records] : found)
    {
        RuntimeSegmentContactActor& object = weapon.contact_actor(*probe);
        if (report_runtime_projectile_contact(weapon, weapon.actor.spawn_owner, object, position, records, still, 0,
                bytes[0xDAC], bytes[0xDAD], bytes[0xDAE], get(bytes, field::hits) == 0, 0) != 0) // 0x00141A30
        {
            set(bytes, field::hits, get(bytes, field::hits) + 1);
        }
        insert_runtime_body_contact_override(weapon.collision_overrides, object, 0, weapon.players); // 0x0013EBE0
        engine_heap_free(records); // 0x0014D360 on the block's heap.
    }
    const float timer = get(bytes, field::shatter_timer);
    if (timer > 0.f)
    {
        const float left = timer - runtime_actor_rate(weapon.actor) * runtime_frame_step(weapon.clock);
        set(bytes, field::shatter_timer, left);
        if (left <= 0.f) // COMISS 0, left; JC keeps it.
        {
            set(bytes, field::shatter_timer, 0.f);
            return true;
        }
    }
    return false;
}

// Xbox 0x000CE660, EAX actor
void record_runtime_large_projectile_root(RuntimeWeaponActor& weapon, std::span<u8> euler, std::span<u8> offset)
{
    const Actor& actor = weapon.actor;
    Vec3 root{};
    actor_joint_world_euler(actor, 0, root); // 0x0003B050, EDX actor
    const Vec3 position = actor_world_position(actor); // 0x000412A0, ESI actor
    std::memcpy(euler.data(), &root, 12);
    const ModelPoseBuffer* pose = nullptr;
    if (actor.model != nullptr)
    {
        pose = actor.model->runtime->alternate; // Model +0x168, else +0x164.
        if (pose == nullptr)
        {
            pose = actor.model->runtime->primary;
        }
    }
    if (pose == nullptr)
    {
        throw Error("0x000CE660 reads joint 0 of a projectile without a model pose");
    }
    const AnimationMatrix& m = model_pose_matrix(*pose, 0); // 0x00031530, EAX 0
    const Vec3& o = actor_transform_origin; // DAT_0050DD80.
    const Vec3 joint{((m[1] * o.y + m[2] * o.z) + m[0] * o.x) + m[3],
        ((m[5] * o.y + m[6] * o.z) + m[4] * o.x) + m[7],
        ((m[9] * o.y + m[10] * o.z) + m[8] * o.x) + m[11]};
    Vec3 turned = joint;
    rotate_euler_vector(root, joint, turned); // 0x00106770, EAX euler, ESI output
    const Vec3 rest{position.x - turned.x, position.y - turned.y, position.z - turned.z};
    std::memcpy(offset.data(), &rest, 12);
}

// Xbox 0x000CE7E0, EBX actor
void restore_runtime_large_projectile_root(RuntimeWeaponActor& weapon)
{
    Vec3 position;
    position = get(weapon.bytes, field::rest_position);
    actor_set_world_position(weapon.actor, position); // 0x00041730, EDI actor, ESI +0xE98
    Vec3 euler;
    euler = get(weapon.bytes, field::rest_euler);
    normalize_euler_angles(weapon.actor.euler, euler); // 0x00105AA0, EDI actor +0x14, ESI +0xE8C
    weapon.actor.xform_valid = 0; // +0x2A8.
}

// Xbox 0x000CE810, EAX actor
void lift_runtime_large_projectile(RuntimeWeaponActor& weapon)
{
    const auto bytes = weapon.bytes;
    Actor& actor = weapon.actor;
    const u32 holder_address = get(bytes, field::catcher);
    const Actor& holder = *weapon.actor_at(holder_address);
    actor.animation_rate = runtime_actor_rate(holder); // +0x92C from the holder's +0x92C / +0x930.
    const Vec3 hold = get(bytes, field::hold_offset);
    const float x = hold.x;
    const float y = hold.y;
    const float z = hold.z;
    const float* m = holder.xform; // Holder +0x2AC.
    const Vec3 target{((m[2] * z + m[1] * y) + m[0] * x) + m[3],
        get(bytes, field::hold_up) + (((m[6] * z + m[4] * x) + m[5] * y) + m[7]),
        ((m[10] * z + m[8] * x) + m[9] * y) + m[11]};
    Vec3 facing = actor_world_euler(holder); // 0x00041820, ESI holder
    facing.z = get(bytes, field::hold_turn) + facing.z;
    float elapsed = runtime_actor_step(actor, weapon.clock) + get(bytes, field::rise_elapsed);
    set(bytes, field::rise_elapsed, elapsed);
    const bool arrived = elapsed >= get(bytes, field::rise_duration);
    if (arrived)
    {
        set(bytes, field::rise_elapsed, get(bytes, field::rise_duration));
    }
    const float fraction = get(bytes, field::rise_elapsed) / get(bytes, field::rise_duration);
    const Vec3 caught = get(bytes, field::catch_position);
    Vec3 position{(target.x - caught.x) * fraction + caught.x, (target.y - caught.y) * fraction + caught.y,
        (target.z - caught.z) * fraction + caught.z};
    const float arc = fraction * std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
    const float rise = float(retail_sin(arc)); // 0x00153320
    static_cast<void>(retail_cos(arc)); // 0x00153230, popped unused.
    position.y = rise * 15.f + position.y; // DAT_0032B214.
    actor_set_world_position(actor, position); // 0x00041730, EDI actor, ESI position
    const Vec3 caught_euler = get(bytes, field::catch_euler);
    const Vec3 turned{step_runtime_angle_toward(facing.x, caught_euler.x, fraction),
        step_runtime_angle_toward(facing.y, caught_euler.y, fraction),
        step_runtime_angle_toward(facing.z, caught_euler.z, fraction)}; // 0x001013A0
    normalize_euler_angles(actor.euler, turned); // 0x00105AA0, EDI actor +0x14
    actor.xform_valid = 0; // +0x2A8.
    if (arrived)
    {
        std::array<u32, 2> taken{0x5C, weapon.self_address()};
        weapon.send(holder_address, taken); // Holder vtable +0xC.
    }
}

// Xbox 0x000CDA10
void update_runtime_large_projectile_state(RuntimeWeaponActor& weapon, u16 state)
{
    show_runtime_large_projectile_holder(weapon); // 0x000CEEC0
    switch (state)
    {
    case 2:
        fly_runtime_large_projectile(weapon); // 0x000CDAD0
        return;
    case 3:
    case 7:
    case 0xB:
    case 0xC:
        return;
    case 4:
        if (!update_runtime_large_projectile_shatter(weapon)) // 0x000CEC70
        {
            return;
        }
        [[fallthrough]];
    case 6:
        store16(weapon.bytes, 0xC0E, 0xB);
        return;
    case 0xF:
        lift_runtime_large_projectile(weapon); // 0x000CE810
        return;
    case 0x10:
        notify_runtime_weapon_grab_points(weapon); // 0x00147430
        return;
    default:
        update_runtime_weapon_state(weapon, state); // 0x0014A220
        return;
    }
}
}
