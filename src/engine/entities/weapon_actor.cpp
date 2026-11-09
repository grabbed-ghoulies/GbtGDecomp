#include "weapon_actor.h"
#include "mind_fields.h"
#include "gbtg/model_nodes.h"
#include "actor_fields.h"
#include "weapon_fields.h"
#include "player_fields.h"
#include <cstddef>
#include "powerup_flags.h"
#include "thrown_projectile.h"
#include "homing_projectile.h"
#include "weapon_actor_detail.h"
#include "projectile_weapon.h"
#include "bob_weapon.h"
#include "candle_weapon.h"
#include "container_weapon.h"
#include "extinguisher_weapon.h"
#include "../audio/core_source.h"
#include "euler_wrapping.h"
#include "gbtg/sound_runtime.h"
#include "segment_records.h"
#include "../frontend/player_health_display.h"
#include "../frontend/scoreboard_update.h"
#include "../camera/camera_body_slots.h"
#include "player_mind_events.h"
#include "../camera/entity_camera.h"
#include "actor_render_registration.h"
#include "../navigation/navigation.h"
#include "../navigation/spawn_candidates.h"
#include "gbtg/actor_spawn_factory.h"
#include "entity_collision.h"
#include "actor_template_creation.h"
#include "body_damage_tables.h"
#include "weapon_breakage.h"
#include "weapon_motion.h"
#include "actor_live_attachments.h"
#include "actor_model_anchors.h"
#include "actor_model_pieces.h"
#include "actor_runtime_queries.h"
#include "actor_spawn_eligibility.h"
#include "actor_spawn_query.h"
#include "actor_shape_bounds.h"
#include "actor_scare_requests.h"
#include "actor_transform.h"
#include "actor_world_lists.h"
#include "ghouly_spawn_trigger.h"
#include "shape_copy.h"
#include "entity.h"
#include "shape_transform.h"
#include "../graphics/billboard_queue.h"
#include "../scene/ambient.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"
#include "body_fields.h"

#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace gbtg
{
using namespace weapon_detail;


namespace
{
ChairWeaponState& chair_of(std::span<u8> bytes)
{
    return actor_record_view<ChairWeaponState>(bytes);
}

// The retail byte loops copy a C string with its terminator into a fixed field.
void copy_string(std::span<u8> bytes, size_t offset, size_t capacity, const char* text)
{
    const size_t length = std::strlen(text);
    if (length >= capacity || offset + length >= bytes.size())
    {
        throw Error("weapon string does not fit its field");
    }
    std::memcpy(bytes.data() + offset, text, length + 1);
}

const char* parameter_string(std::span<const u8> parameters, size_t offset, size_t capacity)
{
    const auto* text = reinterpret_cast<const char*>(parameters.data() + offset);
    if (offset + capacity > parameters.size() || std::memchr(text, 0, capacity) == nullptr)
    {
        throw Error("weapon parameter string has no terminator");
    }
    return text;
}

// 0x00035E90 with error 2 (0x00032700) when the AID is missing.
std::span<const u8> acquire_resource(RuntimeWeaponActor& weapon, const char* name)
{
    const auto resource = acquire_animation_resource(weapon.registry, name);
    if (resource.data() == nullptr)
    {
        resource_error(weapon.error, 2);
    }
    return resource;
}

// The spawn-model walk keeps each actor field and its retail word together.
void set_spawn_blocked(RuntimeWeaponActor& weapon, i32 value)
{
    weapon.actor.spawn_blocked = value;
    weapon_base(weapon.bytes).spawn_blocked = u32(value);
}

// Xbox 0x00144A80, EAX actor
void copy_runtime_weapon_classes(RuntimeWeaponActor& weapon, std::span<const u8> parameters)
{
    const i32 count = i32(get(parameters, weapon_parameter::accepted_class_count));
    weapon_base(weapon.bytes).accepted_class_count = u16(get(parameters, weapon_parameter::accepted_class_count));
    weapon_base(weapon.bytes).accepted_class_row = 0;
    weapon.actor.accepted_class_count = i16(u16(get(parameters, weapon_parameter::accepted_class_count)));
    weapon.actor.accepted_classes = reinterpret_cast<const ActorAcceptedClass*>(weapon_base(weapon.bytes).accepted_classes.data());
    if (count <= 0)
    {
        return;
    }
    if (count > i32(weapon.accepted_resources.size()))
    {
        throw Error("weapon accepted class rows overflow actor +0x948");
    }
    const auto rows = weapon.executable(get(parameters, weapon_parameter::accepted_classes), size_t(count) * 0x8C);
    for (i32 index = 0; index < count; ++index)
    {
        const auto source = rows.subspan(size_t(index) * 0x8C, 0x8C);
        const size_t row = 0x948 + size_t(index) * 0x10;
        if (read_u32(source, 0xC) == kAidPrefix)
        {
            // Row +8 holds the AID; the native span stands in for the retail pointer.
            weapon.accepted_resources[size_t(index)] = acquire_resource(weapon,
                parameter_string(source, 0xC, 0x80));
        }
        store16(weapon.bytes, row + 4, read_u16(source, 4));
        store16(weapon.bytes, row, read_u16(source, 0));
        store32(weapon.bytes, row + 0xC, read_u32(source, 8));
        store16(weapon.bytes, row + 2, read_u16(source, 2));
    }
}

// Xbox 0x00145150, ESI actor, EDI parameters
void initialize_runtime_weapon_names(RuntimeWeaponActor& weapon, std::span<const u8> parameters)
{
    copy_string(weapon.bytes, offsetof(WeaponBase, name_9A8), sizeof(WeaponBase::name_9A8),
                parameter_string(parameters, weapon_parameter::name_9A8.offset, 0x80));
    copy_string(weapon.bytes, offsetof(WeaponBase, name_A28), sizeof(WeaponBase::name_A28),
                parameter_string(parameters, weapon_parameter::name_A28.offset, 0x80));
    weapon_base(weapon.bytes).break_category = 1;
    weapon_base(weapon.bytes).unk_D9C = 0;
    weapon_base(weapon.bytes).unk_9A4 = get(parameters, weapon_parameter::awards_points) == 0 ? 1u : 0u;
    weapon.pieces.clear(); // +0x2A4 = 0.
    build_actor_model_pieces(weapon.actor, weapon.pieces, weapon.random); // 0x0003F170
}

// Xbox 0x00146D10
void collect_runtime_weapon_grab_shapes(RuntimeWeaponActor& weapon)
{
    const Actor& actor = weapon.actor;
    weapon_base(weapon.bytes).prompt_pulse = 0.7f; // DAT_0032B264.
    weapon.hand_texture = acquire_resource(weapon, "aid_texture_ghoulies_weapon_interacthand");
    weapon_base(weapon.bytes).prompt_phase = 0;
    weapon_base(weapon.bytes).prompt_alpha = 0;
    weapon.grab_shapes.clear();
    weapon_base(weapon.bytes).grab_shape_count = 0;
    // Block 2 holds a count and 0x34-byte records; each pass resumes after the record it kept.
    const u8* cursor = nullptr;
    while (true)
    {
        const u8* block = runtime_actor_model_block(actor, 2);
        if (block == nullptr)
        {
            break;
        }
        const u8* end;
        u32 records;
        std::memcpy(&records, block, 4);
        if (cursor == nullptr)
        {
            cursor = block + 4;
            end = cursor + size_t(records) * 0x34;
        }
        else
        {
            cursor += 0x34;
            end = block + size_t(records) * 0x34 + 4;
        }
        bool found = false;
        while (cursor < end)
        {
            u16 kind;
            std::memcpy(&kind, cursor, 2);
            if (kind == 1)
            {
                found = true;
                break;
            }
            cursor += 0x34;
        }
        if (!found)
        {
            break;
        }
        if (weapon.grab_shapes.size() == 2)
        {
            throw Error("weapon grab shapes overflow actor +0xAB8");
        }
        weapon.grab_shapes.push_back(cursor);
        weapon_base(weapon.bytes).grab_shape_count = u32(weapon.grab_shapes.size());
    }
    weapon.spawn_points.clear();
    for (const u8* record = find_runtime_actor_shape(actor, 0x400, 0, -1); record != nullptr;
        record = next_runtime_actor_shape(actor, record, 0x400, 0, -1))
    {
        weapon.spawn_points.push_back(reinterpret_cast<const EntityShapeNode*>(record));
    }
    weapon.actor.spawn_points = weapon.spawn_points.data();
    weapon.actor.spawn_point_count = i32(weapon.spawn_points.size());
    weapon_base(weapon.bytes).spawn_point_count = u32(weapon.spawn_points.size());
    const EntityBonePose* pose = nullptr;
    if (actor.model != nullptr)
    {
        pose = actor.model->alternate_shape_pose;
        if (pose == nullptr)
        {
            pose = actor.model->primary_shape_pose;
        }
    }
    for (size_t index = 0; index < weapon.grab_shapes.size(); ++index)
    {
        const u8* shape = weapon.grab_shapes[index];
        float x;
        float y;
        float z;
        i16 joint;
        std::memcpy(&x, shape + 0x10, 4);
        std::memcpy(&y, shape + 0x20, 4);
        std::memcpy(&z, shape + 0x30, 4);
        std::memcpy(&joint, shape + 2, 2);
        if (joint >= 0)
        {
            if (pose == nullptr)
            {
                throw Error("weapon grab shape names a joint but the model instance has no shape pose");
            }
            const float* m = shape_bone_matrix(*pose, joint);
            const float bx = m[2] * z + m[1] * y + m[0] * x + m[3];
            const float by = m[4] * x + m[6] * z + m[5] * y + m[7];
            const float bz = m[8] * x + m[10] * z + m[9] * y + m[11];
            x = bx;
            y = by;
            z = bz;
        }
        const float* a = actor.xform;
        const Vec3 query{a[2] * z + a[1] * y + a[0] * x + a[3], a[4] * x + a[6] * z + a[5] * y + a[7],
            a[8] * x + a[10] * z + a[9] * y + a[11]};
        Vec3 point;
        float radius;
        i32 nearest = 0;
        u8 kept = 0xFF;
        if (select_actor_spawn_point(actor, query, point, radius, nearest)) // 0x001476D0
        {
            kept = u8(nearest);
        }
        weapon_base(weapon.bytes).kept_spawn_points.at(size_t(index)) = kept;
    }
}

// Xbox 0x00147E10
void collect_runtime_weapon_flagged_markers(RuntimeWeaponActor& weapon)
{
    const u8* block = runtime_actor_model_block(weapon.actor, 4);
    if (block == nullptr)
    {
        return;
    }
    const u8* first = block + 8;
    u16 count = weapon_base(weapon.bytes).flagged_marker_count;
    for (const u8* record = first; record != nullptr; record += record[1])
    {
        if ((record[4] & 0x10) != 0 && record[0] == 0)
        {
            ++count;
        }
        if (record[1] == 0)
        {
            break;
        }
    }
    weapon_base(weapon.bytes).flagged_marker_count = count;
    if (count != 0)
    {
        weapon.flagged_markers.assign(count, nullptr); // 0x000F72D0, cleared.
        size_t slot = 0;
        const u8* record = first;
        do
        {
            if ((record[4] & 0x10) != 0 && record[0] == 0)
            {
                weapon.flagged_markers.at(slot++) = record;
            }
        }
        while (record[1] != 0 && (record += record[1]) != nullptr);
    }
}

// Xbox 0x00147F00, ESI parameters, EDX actor
void pick_runtime_weapon_variant(RuntimeWeaponActor& weapon, std::span<const u8> parameters)
{
    const float roll = next_unit(weapon.random) - 1.f; // DAT_0032B0F8.
    if (roll <= get(parameters, weapon_parameter::variant_chance))
    {
        copy_string(weapon.bytes, offsetof(WeaponBase, variant_name), sizeof(WeaponBase::variant_name),
                    parameter_string(parameters, weapon_parameter::variant_name.offset, 0x80));
        copy_string(weapon.bytes, offsetof(WeaponBase, variant_attributes), sizeof(WeaponBase::variant_attributes),
                    parameter_string(parameters, weapon_parameter::variant_attributes.offset, 0x80));
        weapon_base(weapon.bytes).trigger_radius = get(parameters, weapon_parameter::trigger_radius);
        weapon_base(weapon.bytes).trigger_chance = get(parameters, weapon_parameter::trigger_chance);
        weapon_base(weapon.bytes).unk_BF8 = get(parameters, weapon_parameter::unk_BF8);
    }
    weapon_base(weapon.bytes).spawn_group = get(parameters, weapon_parameter::spawn_group);
    weapon_base(weapon.bytes).unk_BFC = get(parameters, weapon_parameter::unk_BFC);
    weapon_base(weapon.bytes).unk_C00 = 0;
}

}

namespace weapon_detail
{
// Vtable +0x44 of the weapon's class.
void update_state(RuntimeWeaponActor& weapon, u16 state)
{
    if (weapon.type == 0xAA)
    {
        update_runtime_chair_weapon_state(weapon, state);
        return;
    }
    if (weapon.type == 0xA9)
    {
        update_runtime_large_projectile_state(weapon, state);
        return;
    }
    if (weapon.type == 0xA7)
    {
        update_runtime_thrown_projectile_state(weapon, state);
        return;
    }
    if (weapon.type == 0xA8)
    {
        update_runtime_homing_projectile_state(weapon, state); // 0x000CC970
        return;
    }
    if (weapon.type == 0xA0)
    {
        update_runtime_bob_weapon_state(weapon, state);
        return;
    }
    if (weapon.type == 0xA1)
    {
        update_runtime_candle_weapon_state(weapon, state); // 0x000C6800
        return;
    }
    if (weapon.type == 0xA2)
    {
        update_runtime_container_weapon_state(weapon, state); // 0x000C76A0
        return;
    }
    if (weapon.type == 0xA4 || weapon.type == 0xA3 || weapon.type == 0xA5 || weapon.type == 0xA6)
    {
        update_runtime_gun_weapon_state(weapon, state); // The gun's 0x000CAA80
        return;
    }
    throw Error("vtable +0x44 of factory type " + std::to_string(weapon.type) + ", which is no weapon class (0xA0..0xAA)");
}

// Vtable +0x48 of the weapon's class: the chair and both projectiles keep the empty base slot.
void late_update_state(RuntimeWeaponActor& weapon, u16 state)
{
    if (weapon.type == 0xAA || weapon.type == 0xA9 || weapon.type == 0xA8 || weapon.type == 0xA7 || weapon.type == 0xA0
        || weapon.type == 0xA1 || weapon.type == 0xA2)
    {
        return;
    }
    if (weapon.type == 0xA4 || weapon.type == 0xA3 || weapon.type == 0xA5 || weapon.type == 0xA6)
    {
        late_update_runtime_gun_weapon_state(weapon, state); // The gun's 0x000CAF10
        return;
    }
    throw Error("vtable +0x48 of factory type " + std::to_string(weapon.type) + ", which is no weapon class (0xA0..0xAA)");
}

// Vtable +0x40 of the weapon's class.
void leave_state(RuntimeWeaponActor& weapon, u16 state, u16 next)
{
    if (weapon.type == 0xAA)
    {
        leave_runtime_chair_weapon_state(weapon, state, next);
        return;
    }
    if (weapon.type == 0xA9)
    {
        leave_runtime_large_projectile_state(weapon, state, next);
        return;
    }
    if (weapon.type == 0xA7)
    {
        leave_runtime_thrown_projectile_state(weapon, state, next);
        return;
    }
    if (weapon.type == 0xA8)
    {
        leave_runtime_homing_projectile_state(weapon, state, next); // 0x000CCF60
        return;
    }
    if (weapon.type == 0xA0)
    {
        leave_runtime_bob_weapon_state(weapon, state, next);
        return;
    }
    if (weapon.type == 0xA1)
    {
        leave_runtime_candle_weapon_state(weapon, state, next); // 0x000C6BC0
        return;
    }
    if (weapon.type == 0xA2)
    {
        leave_runtime_container_weapon_state(weapon, state, next); // 0x000C7990
        return;
    }
    if (weapon.type == 0xA4 || weapon.type == 0xA3 || weapon.type == 0xA5 || weapon.type == 0xA6)
    {
        leave_runtime_gun_weapon_state(weapon, state); // The gun's 0x000CAEB0
        return;
    }
    throw Error("vtable +0x40 of factory type " + std::to_string(weapon.type) + ", which is no weapon class (0xA0..0xAA)");
}

// Vtable +0x3C of the weapon's class.
void enter_state(RuntimeWeaponActor& weapon, u16 state, u16 previous)
{
    if (weapon.type == 0xAA)
    {
        enter_runtime_chair_weapon_state(weapon, state, previous);
        return;
    }
    if (weapon.type == 0xA9)
    {
        enter_runtime_large_projectile_state(weapon, state, previous);
        return;
    }
    if (weapon.type == 0xA7)
    {
        enter_runtime_thrown_projectile_state(weapon, state, previous);
        return;
    }
    if (weapon.type == 0xA8)
    {
        enter_runtime_homing_projectile_state(weapon, state, previous); // 0x000CCE00
        return;
    }
    if (weapon.type == 0xA0)
    {
        enter_runtime_bob_weapon_state(weapon, state, previous);
        return;
    }
    if (weapon.type == 0xA1)
    {
        enter_runtime_weapon_state(weapon, state, previous); // 0x000C6BB0 jumps to 0x0014A120.
        return;
    }
    if (weapon.type == 0xA2)
    {
        enter_runtime_container_weapon_state(weapon, state, previous); // 0x000C7870
        return;
    }
    if (weapon.type == 0xA4 || weapon.type == 0xA3 || weapon.type == 0xA5 || weapon.type == 0xA6)
    {
        enter_runtime_gun_weapon_state(weapon, state); // The gun's 0x000CAE90
        return;
    }
    throw Error("vtable +0x3C of factory type " + std::to_string(weapon.type) + ", which is no weapon class (0xA0..0xAA)");
}
// Actor +0x99C, the class's contact test: the chair sets 0x000CF5F0.
RuntimeBodyContactCallback contact_test(RuntimeWeaponActor& weapon)
{
    if (weapon_base(weapon.bytes).contact_test == 0x000CF5F0)
    {
        return [&weapon](Actor&, RuntimeSegmentContactActor& object, const Vec3& point, GhoulySpawnPool* contacts,
            const RuntimeSegmentContactRecord&, const Vec3& velocity, u16 word) -> u32
        {
            return test_runtime_chair_contact(weapon, object, point, contacts, velocity, word); // 0x000CF5F0
        };
    }
    if (weapon_base(weapon.bytes).contact_test == 0x000CB040)
    {
        return [&weapon](Actor&, RuntimeSegmentContactActor& object, const Vec3& point, GhoulySpawnPool* contacts,
            const RuntimeSegmentContactRecord&, const Vec3& velocity, u16 word) -> u32
        {
            return test_runtime_gun_contact(weapon, object, point, contacts, velocity, word); // 0x000CB040
        };
    }
    if (weapon_base(weapon.bytes).contact_test == 0x000C6F10)
    {
        return [&weapon](Actor&, RuntimeSegmentContactActor& object, const Vec3& point, GhoulySpawnPool* contacts,
            const RuntimeSegmentContactRecord&, const Vec3& velocity, u16 word) -> u32
        {
            return test_runtime_candle_contact(weapon, object, point, contacts, velocity, word); // 0x000C6F10
        };
    }
    if (weapon_base(weapon.bytes).contact_test == 0x000C61D0)
    {
        return [&weapon](Actor&, RuntimeSegmentContactActor& object, const Vec3& point, GhoulySpawnPool* contacts,
            const RuntimeSegmentContactRecord&, const Vec3& velocity, u16 word) -> u32
        {
            return test_runtime_bob_contact(weapon, object, point, contacts, velocity, word); // 0x000C61D0
        };
    }
    if (weapon_base(weapon.bytes).contact_test == 0x000CE5D0)
    {
        return [&weapon](Actor&, RuntimeSegmentContactActor& object, const Vec3& point, GhoulySpawnPool* contacts,
            const RuntimeSegmentContactRecord&, const Vec3& velocity, u16 word) -> u32
        {
            return test_runtime_projectile_contact(weapon, object, point, contacts, velocity, word); // 0x000CE5D0
        };
    }
    if (weapon_base(weapon.bytes).contact_test == 0x000CD1B0)
    {
        return [&weapon](Actor&, RuntimeSegmentContactActor& object, const Vec3& point, GhoulySpawnPool* contacts,
            const RuntimeSegmentContactRecord&, const Vec3& velocity, u16 word) -> u32
        {
            return test_runtime_homing_projectile_contact(weapon, object, point, contacts, velocity, word); // 0x000CD1B0
        };
    }
    if (weapon_base(weapon.bytes).contact_test == 0x000CBF80)
    {
        return [&weapon](Actor&, RuntimeSegmentContactActor& object, const Vec3& point, GhoulySpawnPool* contacts,
            const RuntimeSegmentContactRecord&, const Vec3& velocity, u16 word) -> u32
        {
            return test_runtime_thrown_projectile_contact(weapon, object, point, contacts, velocity, word); // 0x000CBF80
        };
    }
    if (weapon_base(weapon.bytes).contact_test == 0x000CB040)
    {
        return [&weapon](Actor&, RuntimeSegmentContactActor& object, const Vec3& point, GhoulySpawnPool* contacts,
            const RuntimeSegmentContactRecord&, const Vec3& velocity, u16 word) -> u32
        {
            return test_runtime_gun_contact(weapon, object, point, contacts, velocity, word); // 0x000CB040
        };
    }
    throw Error("weapon contact test +0x99C holds " + std::to_string(weapon_base(weapon.bytes).contact_test)
        + ", which no weapon class sets");
}
}

// Xbox 0x001458E0
void weapon_actor_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = 0xE54;
    sizes.parameters = 0x5F8;
}

// Xbox 0x000CEF50
void chair_weapon_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = 0xF08;
    sizes.parameters = 0x684;
}

// Xbox 0x00146230, ESI actor, EAX value
void set_runtime_weapon_mask(RuntimeWeaponActor& weapon, u8 value)
{
    weapon_base(weapon.bytes).mask = value;
    weapon.actor.mask_flag = i32((u32(weapon.actor.mask_flag) & ~0xFFu) | value);
    if (value == 0 && weapon_base(weapon.bytes).voice_name.data()[0] != 0)
    {
        weapon_base(weapon.bytes).voice_name.data()[0] = 0;
        const u16 voice = weapon_base(weapon.bytes).voice;
        if (voice != 0xFFFF)
        {
            stop_actor_sound_voice(i16(voice), weapon.services.sounds); // 0x00039690
            weapon_base(weapon.bytes).voice = 0xFFFF;
        }
    }
}

// Xbox 0x00145BC0, EAX actor
void register_runtime_weapon_cells(RuntimeWeaponActor& weapon)
{
    if (entity_detail::g_nav_grids[0] != nullptr) // DAT_0050E240.
    {
        if (weapon.view.live_attachments.next != nullptr) // Actor +0x8F8.
        {
            update_runtime_weapon_cells(weapon, -1); // 0x00145C30
            // 0x001104E0 registers the cells around the attachment's volume (+0x20 centre, +0x38 radius).
            const auto* attachment = static_cast<const ActorLiveAttachment*>(weapon.view.live_attachments.next);
            Vec3 center;
            std::memcpy(&center, attachment->shape.data() + 4, 12);
            float radius;
            std::memcpy(&radius, attachment->shape.data() + 0x1C, 4);
            entity_detail::register_navigation_cells(weapon.cells, center, i32(weapon_base(weapon.bytes).floor),
                                                     radius,
                                                     0); // 0x001104E0
            return;
        }
        if (weapon.cells != nullptr)
        {
            entity_detail::release_navigation_cell_registration(weapon.cells,
                                                                weapon_base(weapon.bytes).floor); // 0x001108B0
        }
    }
}

// Xbox 0x00144D00, ESI actor
void start_runtime_weapon_idle(RuntimeWeaponActor& weapon)
{
    if (read_u32(weapon.bytes, offsetof(WeaponBase, animation)) == kAidPrefix)
    {
        start_runtime_weapon_animation(weapon, weapon_base(weapon.bytes).animation.data());
        return;
    }
    // 0x0003AAA0 finds "initial" in the actor's table; 0x0003A9E0 plays it.
    const AnimationRecord* record = find_actor_animation(weapon.view.animation.table, "initial", nullptr,
        weapon.random);
    if (record != nullptr)
    {
        start_actor_animation(weapon.view.animation, *record, weapon.registry, weapon.error, weapon.random);
    }
}

// Xbox 0x00144D40, EAX actor
bool start_runtime_weapon_animation(RuntimeWeaponActor& weapon, const char* name)
{
    auto& animation = weapon.view.animation;
    AnimationMix mix;
    mix.duration = 10.f; // DAT_0032B10C.
    mix.weight = -10.f; // DAT_0032B1FC.
    mix.blend_mode = 0x21;
    mix.fade_duration = 0.2f; // DAT_0032B1AC.
    mix.position = 0.f;
    mix.mode = 0;
    mix.priority = 0x31;
    mix.old_pose_duration = 0.2f;
    mix.events = {};
    mix.borrowed_animation = {};
    mix.borrowed_events = {};
    mix.record = nullptr;
    if (animation.controller == nullptr)
    {
        animation.controller = make_animation_controller(4); // 0x00030740
        animation.controller->event_callback = [](std::span<const u8> event, u32 index, float position,
            void* owner, int phase)
        {
            animation_actor_event(event, index, position, *static_cast<ActorAnimationState*>(owner), phase); // 0x0003B3C0
        };
        animation.controller->event_owner = &animation;
    }
    if (!start_controller_animation(*animation.controller, 0, {}, name, mix, weapon.registry, weapon.error,
        weapon.random)) // 0x00030880
    {
        clear_actor_animation(animation, weapon.actor.model != nullptr ? &weapon.view.pose : nullptr,
            weapon.registry); // 0x0003AC20
        return false;
    }
    if (animation.controller != nullptr)
    {
        if (!animation.controller->slots.empty() && animation.controller->slots[0].player != nullptr)
        {
            ++animation.controller->slots[0].player->paused;
        }
        animation.controller->dirty = true;
    }
    return true;
}

// Xbox 0x00146310, EAX actor
void attach_runtime_weapon_volume(RuntimeWeaponActor& weapon)
{
    if (weapon.view.live_attachments.next != nullptr) // Actor +0x8F8.
    {
        return;
    }
    Actor& actor = weapon.actor;
    if ((runtime_actor_physical_shape_flags(actor) & 0x100) == 0) // 0x00042500
    {
        return;
    }
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const float radius = float(actor_model_bound_radius(actor)); // 0x0003DB30
    std::array<u8, 0x20> shape{};
    shape[0] = 4;
    shape[1] = 0x20;
    shape[2] = 1;
    std::memcpy(shape.data() + 4, &position, 12);
    const float height = float(actor_model_height(actor)); // 0x0003DB90
    std::memcpy(shape.data() + 0x14, &height, 4);
    std::memcpy(shape.data() + 0x1C, &radius, 4);
    allocate_actor_live_attachment(actor, weapon.view.live_attachments, weapon.global_attachments, 2,
        u8(weapon_base(weapon.bytes).floor), 0, shape.data(), actor_transform_origin); // 0x0012AFE0
    weapon_base(weapon.bytes).was_held = 1;
}

// Xbox 0x0014A290, ESI actor, EDI state
void change_runtime_weapon_state(RuntimeWeaponActor& weapon, u16 state)
{
    weapon_base(weapon.bytes).next_state = 0;
    leave_state(weapon, weapon_base(weapon.bytes).state, state); // Vtable +0x40.
    const u16 previous = weapon_base(weapon.bytes).state;
    weapon_base(weapon.bytes).state = state;
    weapon_base(weapon.bytes).state_time = 0.f;
    enter_state(weapon, state, previous); // Vtable +0x3C.
}

// Xbox 0x00144FD0, EAX actor
void start_runtime_weapon_scene(RuntimeWeaponActor& weapon)
{
    if (weapon_base(weapon.bytes).starts_scene == 0)
    {
        return;
    }
    const Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
    if (!weapon.services.backgrounds)
    {
        throw Error("weapon scene start 0x00144FD0: the background list for 0x0013B980 is not bound");
    }
    const std::vector<BackgroundListEntry> backgrounds = weapon.services.backgrounds();
    u32 background = weapon_base(weapon.bytes).scene_background;
    i32 part = i32(weapon_base(weapon.bytes).scene_part);
    const bool found = find_background_part_near(position, backgrounds, background, part); // 0x0013B980, EBX position
    weapon_base(weapon.bytes).scene_background = background;
    weapon_base(weapon.bytes).scene_part = u32(part);
    if (!found)
    {
        weapon_base(weapon.bytes).starts_scene = 0;
        return;
    }
    // Vtable +0xC with {2}; words 1..3 are the position's stack slots.
    std::array<u32, 0x42> message{};
    message[0] = 2;
    message[1] = std::bit_cast<u32>(position.x);
    message[2] = std::bit_cast<u32>(position.y);
    message[3] = std::bit_cast<u32>(position.z);
    weapon.message(message);
}

// Xbox 0x00148600, ESI actor
void bind_runtime_weapon_group(RuntimeWeaponActor& weapon)
{
    const u16 group = actor_marker_next(weapon.bytes);
    if (group == 0)
    {
        return;
    }
    u32 owner = 0;
    for (const auto* node = entity_detail::g_world.latch; node != nullptr; node = node->next) // DAT_00511AA4.
    {
        const u32 address = weapon.latch_owner(*node);
        if (actor_marker_id(weapon.bytes_at(address)) == group)
        {
            owner = address;
            break;
        }
    }
    weapon_base(weapon.bytes).group_owner = owner;
}

// Xbox 0x001462B0, ESI weapon, EAX actor
void release_runtime_weapon_holder(RuntimeWeaponActor& weapon, u32 actor)
{
    if (weapon.markers != nullptr)
    {
        remove_runtime_body_contact_actor(*weapon.markers, *weapon.actor_at(actor)); // 0x0013F720
    }
    if (weapon.actor.spawn_owner != nullptr && weapon.services.actor_address(*weapon.actor.spawn_owner) == actor)
    {
        weapon.actor.spawn_owner = nullptr; // +0xD8C's native view.
        weapon_base(weapon.bytes).spawn_owner = 0;
    }
    if (weapon_base(weapon.bytes).group_owner == actor)
    {
        weapon_base(weapon.bytes).group_owner = 0;
    }
}

// Xbox 0x0014A0D0, ESI actor
void reset_runtime_weapon_state(RuntimeWeaponActor& weapon)
{
    weapon_base(weapon.bytes).next_state = 0;
    leave_state(weapon, weapon_base(weapon.bytes).state, 0); // Vtable +0x40.
    const u16 previous = weapon_base(weapon.bytes).state;
    weapon_base(weapon.bytes).state = 0;
    weapon_base(weapon.bytes).state_time = 0.f;
    enter_state(weapon, 0, previous); // Vtable +0x3C.
    weapon_base(weapon.bytes).next_state = 0;
}

// Xbox 0x00145E80
void destroy_runtime_weapon_actor(RuntimeWeaponActor& weapon)
{
    auto& link = weapon.spawn_link; // +0x934 / +0x938.
    if (link.previous != nullptr)
    {
        link.previous->next = link.next;
    }
    if (link.next != nullptr)
    {
        link.next->previous = link.previous;
    }
    link.previous = nullptr;
    link.next = nullptr;
    if (weapon.hand_texture.data() != nullptr)
    {
        release_animation_resource(weapon.registry, weapon.hand_texture); // 0x00035C00
        weapon.hand_texture = {};
    }
    if (weapon.markers != nullptr)
    {
        destroy_runtime_body_collision_state(weapon.markers); // 0x0013F240
        weapon.markers = nullptr;
    }
    // 0x00144B30: each accepted class row's AID.
    for (i16 index = 0; index < weapon.actor.accepted_class_count; ++index)
    {
        auto& resource = weapon.accepted_resources.at(size_t(index));
        if (resource.data() != nullptr)
        {
            release_animation_resource(weapon.registry, resource); // 0x00035C00
            resource = {};
        }
    }
    if (weapon.spawn_record != nullptr) // +0xC04.
    {
        release_ghouly_spawn_record(*weapon.spawn_record); // 0x000F3A00
        weapon.spawn_record = nullptr;
    }
    reset_runtime_weapon_state(weapon); // 0x0014A0D0
    if (!weapon.flagged_markers.empty())
    {
        weapon.flagged_markers.clear(); // 0x000663E0.
        weapon_base(weapon.bytes).flagged_marker_count = 0;
    }
    if (weapon_base(weapon.bytes).unk_D9C != 0)
    {
        // Every retail write of +0xD9C (0x00145150, 0x00145820, 0x00145E80) stores zero, so 0x001450A0's piece
        // blocks never exist.
        throw Error("weapon destructor 0x00145E80 finds +0xD9C set, which no retail writer does");
    }
    weapon.pieces.clear(); // +0x2A4, 0x000663E0.
    if (weapon.cells != nullptr)
    {
        entity_detail::release_navigation_cell_registration(weapon.cells,
                                                            weapon_base(weapon.bytes).floor); // 0x001108B0
    }
    if (weapon_base(weapon.bytes).voice != 0xFFFF)
    {
        stop_actor_sound_voice(i16(weapon_base(weapon.bytes).voice), weapon.services.sounds); // 0x00039690
        weapon_base(weapon.bytes).voice = 0xFFFF;
    }
    if (weapon_base(weapon.bytes).unk_D98 != 0)
    {
        // Every retail write of +0xD98 (0x00145820, 0x00145C80, 0x00145E80) stores zero, so there is never an
        // effect node for 0x000ED2E0 to release.
        throw Error("weapon destructor 0x00145E80 finds +0xD98 set, which no retail writer does");
    }
    remove_ghouly_controller(&weapon.controller); // DAT_00512B74's entry, as 0x0012ACA0 removes it.
}

// Xbox 0x00146A60, ESI actor, EBX message
void break_runtime_weapon_by_player(RuntimeWeaponActor& weapon, std::span<u32> message)
{
    auto bytes = weapon.bytes;
    if (weapon.actor.spawn_model_blocked != 0 || !weapon_has_pieces(weapon.actor)) // +0x9A0.
    {
        return;
    }
    weapon_base(bytes).break_category = 0;
    if (message[4] != 0)
    {
        // The player's body +0x270 / +0x274 name its two partners; a live one picks the category.
        const auto body = weapon.services.body_of(message[4]);
        const Actor* first = weapon.actor_or_null(body_record(body).attacker);
        const Actor* second = weapon.actor_or_null(body_record(body).unk_274);
        if (first != nullptr && (first->mask_mode == 2 || first->mask_flag != 0))
        {
            weapon_base(bytes).break_category = 2;
        }
        else if (second != nullptr && (second->mask_mode == 2 || second->mask_flag != 0))
        {
            weapon_base(bytes).break_category = 3;
        }
        else
        {
            weapon_base(bytes).break_category = 0;
        }
    }
    break_runtime_weapon(weapon); // 0x001454F0
    change_runtime_weapon_state(weapon, 9); // As 0x0014A290.
    message[1] = 1;
}

// Xbox 0x00146B80, ESI actor, EDI attacker
u32 break_runtime_weapon_by_attacker(RuntimeWeaponActor& weapon, Actor* attacker, u16 category)
{
    auto bytes = weapon.bytes;
    if (weapon.actor.spawn_model_blocked != 0 || !weapon_has_pieces(weapon.actor)) // +0x9A0.
    {
        return 0;
    }
    if (attacker != nullptr && (attacker->mask_mode == 2 || attacker->mask_flag != 0))
    {
        weapon_base(bytes).break_category = category;
    }
    else
    {
        weapon_base(bytes).break_category = 0;
    }
    break_runtime_weapon(weapon); // 0x001454F0
    change_runtime_weapon_state(weapon, 9); // As 0x0014A290.
    if (attacker != nullptr)
    {
        std::array<u32, 0x42> notice{};
        notice[0] = 0x50;
        notice[1] = 0;
        weapon.send(weapon.services.actor_address(*attacker), notice); // Vtable +0xC.
    }
    return 1;
}

namespace
{
// 0x001463D0 case 0x97 with a nonzero +4: the weapon drops beside its owner (+0xD8C), or where it is.
void drop_runtime_weapon(RuntimeWeaponActor& weapon)
{
    Actor& actor = weapon.actor;
    float minimum = 10.f; // DAT_0032B10C.
    float maximum = 30.f; // DAT_0032B108.
    float lift = 4.f; // DAT_0032B1B0.
    Vec3 position;
    i32 floor;
    if (actor.spawn_owner != nullptr)
    {
        Actor& owner = *actor.spawn_owner;
        position = actor_world_position(owner); // 0x000412A0
        floor = i32(player_record(weapon.bytes_at(weapon.services.actor_address(owner))).floor);
        // Message 0x59 asks the owner for the drop: +0x10 set gives the radii (+0xC / +0x10), a turn (+0x14)
        // and the lift (+0x18).
        std::array<u32, 0x42> query{};
        query[0] = 0x59;
        query[1] = weapon.self_address();
        weapon.send(weapon.services.actor_address(owner), query); // Vtable +0xC.
        if (query[4] != 0)
        {
            minimum = std::bit_cast<float>(query[3]);
            maximum = std::bit_cast<float>(query[4]);
            Vec3 euler = actor_world_euler(actor); // 0x00041820
            euler.z = euler.z + std::bit_cast<float>(query[5]);
            wrap_runtime_euler(euler, actor.euler); // 0x00105AA0 into +0x14
            actor.xform_valid = 0; // +0x2A8.
            lift = std::bit_cast<float>(query[6]);
        }
    }
    else
    {
        position = actor_world_position(actor); // 0x000412A0
        floor = entity_detail::navigation_floor_index(position, -1, 0); // 0x00110BF0
    }
    Vec3 target;
    if (!entity_detail::select_navigation_spawn_position(floor, nullptr, position, minimum, maximum, nullptr,
        nullptr, 0x19, 0, target, weapon.random, weapon.services.occupancy_initial)) // 0x00111420
    {
        target = position;
    }
    entity_detail::navigation_cell_height(floor, target, target.y); // 0x00110BB0
    target.y = target.y + lift;
    change_runtime_weapon_state(weapon, 8); // 0x0014A290
    start_runtime_weapon_drop(weapon, target); // 0x00149580
}

// 0x001463D0 case 0x9B: the weapon comes to rest on the floor below it.
void settle_runtime_weapon(RuntimeWeaponActor& weapon)
{
    Actor& actor = weapon.actor;
    actor_set_uniform_scale(actor, 1.f); // 0x00041B90
    change_runtime_weapon_state(weapon, 0x10); // 0x0014A290
    Vec3 position = actor_world_position(actor); // 0x000412A0
    if (actor.spawn_owner == nullptr)
    {
        throw Error("0x001463D0 case 0x9B reads the floor of an owner at +0xD8C that is not set");
    }
    const i32 floor = i32(player_record(weapon.bytes_at(weapon.services.actor_address(*actor.spawn_owner))).floor);
    float height = 0.f;
    if (!entity_detail::navigation_cell_height(floor, position, height)) // 0x00110BB0
    {
        return;
    }
    // A downward probe through the floor height finds the surface; otherwise the cell height stands.
    Vec3 above{position.x, height + 10.f, position.z}; // DAT_0032B10C.
    Vec3 below{position.x, height - 10.f, position.z};
    float surface = height;
    if (entity_record_sweep_probe(below, above, 8, 0) != 0) // 0x00122480
    {
        surface = below.y;
    }
    position.y = surface + 2.f; // DAT_0032B110.
    actor_set_world_position(actor, position); // 0x00041730
}
}

// Xbox 0x00147FC0
u32 spawn_runtime_weapon_body(RuntimeWeaponActor& weapon, const char* name, const char* third, const char* second,
    const char* fourth, bool face_view)
{
    Actor& actor = weapon.actor;
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    Vec3 euler;
    if (face_view)
    {
        const auto* view = weapon.services.view;
        if (view == nullptr)
        {
            throw Error("0x00147FC0 faces the view DAT_005077A8 before one is set");
        }
        const Vec3 direction{view->position.x - position.x, view->position.y - position.y,
            view->position.z - position.z};
        runtime_direction_angle(direction, euler.y); // 0x00105F90
        euler.z = 0.f;
        euler.x = 0.f;
    }
    else
    {
        euler = actor_world_euler(actor); // 0x00041820
    }
    u32 head;
    std::memcpy(&head, name, 4);
    if (head != kAidPrefix)
    {
        return 0;
    }
    const auto named = [](const char* text) -> const char*
    {
        u32 word;
        std::memcpy(&word, text, 4);
        return word == kAidPrefix ? text : nullptr;
    };
    auto& objects = weapon.services.objects;
    FactoryObject* body = weapon.services.keep(create_spawn_actor(objects.factory, objects.resources, objects.records,
        name, position, euler, 1.f, nullptr, nullptr, named(second), named(fourth), named(third), 0)); // 0x001347C0
    if (body == nullptr || body->bytes == nullptr)
    {
        return 0;
    }
    const u32 body_address = weapon.object_address(*body);
    const u32 group = weapon_base(weapon.bytes).spawn_group;
    if (group != 0)
    {
        // 0xA6 asks the group's owner (+0xC08) for the pose; 0x4D gives it to the new body.
        std::array<u32, 0x42> ask{};
        ask[0] = 0xA6;
        std::memcpy(ask.data() + 1, &position, 12);
        std::memcpy(ask.data() + 4, &euler, 12);
        const u32 owner = weapon_base(weapon.bytes).group_owner;
        if (owner != 0)
        {
            weapon.send(owner, ask); // Vtable +0xC.
        }
        std::array<u32, 0x42> place{};
        place[0] = 0x4D;
        place[1] = group;
        if (ask[14] != 0)
        {
            std::copy_n(ask.data() + 8, 6, place.data() + 2);
        }
        else
        {
            std::memcpy(place.data() + 2, &position, 12);
            std::memcpy(place.data() + 5, &euler, 12);
        }
        weapon.send(body_address, place);
    }
    std::array<u32, 0x42> notice{};
    notice[0] = 0xA4;
    notice[1] = body_address;
    weapon.message(notice); // Vtable +0xC.
    return body_address;
}

// Xbox 0x00148270, EAX actor
u32 spawn_runtime_weapon_template(RuntimeWeaponActor& weapon, const char* name)
{
    const Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
    const Vec3 euler = actor_world_euler(weapon.actor); // 0x00041820
    auto& objects = weapon.services.objects;
    auto parameters = clone_executable_resource(objects.resources, name); // 0x00107720
    if (parameters.bytes == nullptr)
    {
        return 0;
    }
    std::memcpy(parameters.bytes.get() + 8, &position, 12);
    std::memcpy(parameters.bytes.get() + 0x14, &euler, 12);
    FactoryObject* created = weapon.services.keep(create_factory_object(objects.factory,
        {parameters.bytes.get(), parameters.size})); // 0x00107520
    parameters.bytes.reset(); // 0x000663E0.
    if (created == nullptr || created->bytes == nullptr)
    {
        return 0;
    }
    return weapon.object_address(*created);
}

// Xbox 0x00148300, EAX actor
u32 spawn_runtime_weapon_hopper(RuntimeWeaponActor& weapon, const char* name)
{
    const Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
    Vec3 euler = actor_world_euler(weapon.actor); // 0x00041820
    euler.z = 0.f;
    euler.x = 0.f;
    const i32 floor = entity_detail::navigation_floor_index(position, -1, 0); // 0x00110BF0
    Vec3 target;
    if (!entity_detail::select_navigation_spawn_position(floor, nullptr, position, 10.f, 40.f, nullptr, nullptr,
        0x19, 0, target, weapon.random, weapon.services.occupancy_initial) &&
        !entity_detail::select_navigation_spawn_position(floor, nullptr, position, 30.f, 60.f, nullptr, nullptr,
            0x19, 0, target, weapon.random, weapon.services.occupancy_initial)) // 0x00111420
    {
        return 0;
    }
    auto& objects = weapon.services.objects;
    FactoryObject* created = weapon.services.keep(create_runtime_actor_at(name, &position, &euler, 1.f,
        objects.resources, objects.factory, actor_transform_origin, weapon.services.default_euler)); // 0x00112C50
    if (created == nullptr || created->bytes == nullptr)
    {
        return 0;
    }
    const u32 address = weapon.object_address(*created);
    std::array<u32, 0x42> hop{};
    hop[0] = 0x96;
    std::memcpy(hop.data() + 1, &target, 12);
    std::memcpy(hop.data() + 4, &euler, 12);
    weapon.send(address, hop); // Vtable +0xC.
    return address;
}

// Xbox 0x00144B80, EAX message
void animate_runtime_weapon_with_holder(RuntimeWeaponActor& weapon, std::span<const u32> message)
{
    // The holder row (+0x946) of the +0x948 class rows: its +8 is the weapon's animation table, which native keeps
    // in accepted_resources.
    const u16 row = weapon_base(weapon.bytes).accepted_class_row;
    const std::span<const u8> resource = weapon.accepted_resources.at(row);
    if (resource.data() == nullptr)
    {
        return;
    }
    const AnimationTable& table = *weapon.animation_tables->tables.at(resource.data());
    // Message +8 is the holder's slot-0 record.
    const auto* holder = static_cast<const AnimationRecord*>(weapon.services.bound_at(message[2]));
    if (holder == nullptr)
    {
        throw Error("0x00144B80 reads a null holder record at message +8");
    }
    const AnimationAlias* alias = find_animation_alias(table, holder->name); // 0x00032210
    const AnimationRecord* record = find_animation_record(table, alias != nullptr ? alias->target : holder->name); // 0x00032180
    if (record == nullptr)
    {
        alias = find_animation_alias(table, "default"); // 0x0031C470
        record = find_animation_record(table, alias != nullptr ? alias->target : "default");
        if (record == nullptr)
        {
            return;
        }
    }
    AnimationMix mix = animation_mix(*holder, false); // 0x000322A0
    mix.events = record->events; // +0x84 into mix +0x24.
    mix.position = std::bit_cast<float>(message[4]);
    mix.old_pose_duration = record->old_pose_duration;
    if (holder->old_pose_duration != record->old_pose_duration
        && record->old_pose_duration == std::bit_cast<float>(0x3E4CCCCDu)) // DAT_0032B1AC.
    {
        mix.old_pose_duration = holder->old_pose_duration;
    }
    start_actor_animation_mix(weapon.view.animation, message[3], mix, record->animation.data(), weapon.registry,
        weapon.error, weapon.random); // 0x0003ADA0
}

// Xbox 0x001463D0
void dispatch_runtime_weapon_message(RuntimeWeaponActor& weapon, std::span<u32> message)
{
    weapon.base_message(message); // 0x0003EF70
    switch (message[0])
    {
    case 1:
        change_runtime_weapon_state(weapon, 0x10); // 0x0014A290
        return;
    case 2:
        change_runtime_weapon_state(weapon, 0xC);
        return;
    case 6:
        // 0x00144FB0: the actor at message +4 joins the marker walk's overrides, unsuspended.
        if (weapon.markers != nullptr)
        {
            insert_runtime_body_contact_override(weapon.markers->overrides, weapon.contact_actor_at(message[1]), 0,
                weapon.players); // 0x0013EBE0
        }
        return;
    case 8:
        break_runtime_weapon_by_player(weapon, message); // 0x00146A60
        return;
    case 0x13:
        if (actor_has_spawn_model(weapon.actor)) // 0x00145470
        {
            weapon_base(weapon.bytes).break_category = 0;
            break_runtime_weapon(weapon); // 0x001454F0
            change_runtime_weapon_state(weapon, 9); // 0x0014A290
        }
        return;
    case 0x15:
        message[1] = break_runtime_weapon_by_attacker(weapon, weapon.actor_or_null(message[2]), 1); // 0x00146B80
        return;
    case 0x19:
    {
        // The attacker is the owner (+0xD8C) of the actor at message +0x1C.
        Actor* source = weapon.actor_or_null(message[7]);
        message[4] = break_runtime_weapon_by_attacker(weapon, source != nullptr ? source->spawn_owner : nullptr, 5);
        message[6] = 0;
        return;
    }
    case 0x1D:
    {
        // 0x00145FD0: the holder of the actor at message +0x14.
        Actor* source = weapon.actor_or_null(message[5]);
        message[4] = break_runtime_weapon_by_attacker(weapon, source != nullptr ? weapon_holder(*source) : nullptr, 4);
        return;
    }
    case 0x20:
        message[1] = u32(weapon.actor.spawn_model_blocked); // +0x9A0.
        return;
    case 0x23:
    {
        if (weapon.spawn_link.previous == nullptr)
        {
            // 0x0002CBD0: +0x934 goes after the root of DAT_00511B24.
            auto* root = &entity_detail::g_world.body_b;
            while (root->prev != nullptr)
            {
                root = static_cast<entity_detail::ListHead*>(root->prev);
            }
            auto* next = static_cast<ActorSpawnLink*>(root->next);
            root->next = &weapon.spawn_link;
            weapon.spawn_link.previous = reinterpret_cast<ActorSpawnLink*>(root);
            weapon.spawn_link.next = next;
            if (next != nullptr)
            {
                next->previous = &weapon.spawn_link;
            }
        }
        weapon.actor.model_flags &= ~0x40u;
        if (weapon.actor.spawn_blocked != 0)
        {
            clear_actor_model_anchors(weapon.view.anchors); // 0x0003EF40
        }
        const Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
        weapon_base(weapon.bytes).floor = u32(entity_detail::navigation_floor_index(position, -1, 0)); // 0x00110BF0
        return;
    }
    case 0x28:
        animate_runtime_weapon_with_holder(weapon, message); // 0x00144B80
        return;
    case 0x47:
        if (weapon.markers != nullptr)
        {
            remove_runtime_body_contact_actor(*weapon.markers, *weapon.actor_at(message[1])); // 0x0013F720
        }
        return;
    case 0x7C:
        release_runtime_weapon_holder(weapon, message[1]); // 0x001462B0
        return;
    case 0x7D:
        start_runtime_weapon_scene(weapon); // 0x00144FD0
        bind_runtime_weapon_group(weapon); // 0x00148600
        return;
    case 0x8D:
        // 0x001462F0: the holder record at +0xE50 leaves when it names message +4.
        if (weapon_base(weapon.bytes).target_record == message[1])
        {
            weapon_base(weapon.bytes).target_record = 0;
        }
        return;
    case 0x93:
        weapon_base(weapon.bytes).next_state = 9;
        return;
    case 0x96:
    {
        change_runtime_weapon_state(weapon, 5); // 0x0014A290
        Vec3 target;
        std::memcpy(&target, message.data() + 1, 12);
        start_runtime_weapon_hop(weapon, target); // 0x00149230
        return;
    }
    case 0x97:
        if (message[1] != 0)
        {
            drop_runtime_weapon(weapon);
        }
        return;
    case 0x98:
        if (weapon.actor.spawn_owner != nullptr && runtime_actor_is_player(*weapon.actor.spawn_owner)) // 0x00028D00
        {
            award_runtime_weapon_points(weapon, 1); // 0x001456D0
        }
        activate_actor_bind_tree(weapon.actor); // 0x00112CC0
        return;
    case 0x9B:
        settle_runtime_weapon(weapon);
        return;
    case 0xA7:
    {
        const auto text = [&](size_t word)
        {
            return reinterpret_cast<const char*>(message.data() + word);
        };
        message[0x81] = spawn_runtime_weapon_body(weapon, text(1), text(0x41), text(0x21), text(0x61),
            weapon_base(weapon.bytes).unk_C00 == 0) != 0 ? 1u : 0u; // 0x00147FC0
        return;
    }
    case 0xA8:
        message[0x41] = request_runtime_weapon_variant(weapon, reinterpret_cast<const char*>(message.data() + 1),
            reinterpret_cast<const char*>(message.data() + 0x21)) ? 1u : 0u; // 0x00148430
        return;
    case 0xA9:
        message[0x21] = spawn_runtime_weapon_template(weapon, reinterpret_cast<const char*>(message.data() + 1)) != 0
            ? 1u : 0u; // 0x00148270
        return;
    case 0xAA:
        message[0x21] = spawn_runtime_weapon_hopper(weapon, reinterpret_cast<const char*>(message.data() + 1)) != 0
            ? 1u : u32(-2); // 0x00148300
        return;
    case 0xAB:
        // 0x00145030: the scene-start wait at +0xC18 ends when +0xC1C / +0xC20 match; the weapon takes message 1
        // and the actor at message +0xC is told 6.
        if (weapon_base(weapon.bytes).starts_scene != 0 &&
            weapon_base(weapon.bytes).scene_background == message[1] &&
            weapon_base(weapon.bytes).scene_part == message[2])
        {
            std::array<u32, 0x42> start{};
            start[0] = 1;
            weapon.message(start); // Vtable +0xC.
            if (message[3] != 0)
            {
                std::array<u32, 0x42> notice{};
                notice[0] = 6;
                notice[1] = weapon.self_address();
                weapon.send(message[3], notice);
            }
        }
        break;
    default:
        return;
    }
}

// Xbox 0x00141890, EAX velocity, EBX point
u32 report_runtime_weapon_contact(RuntimeWeaponActor& weapon, Actor* holder, RuntimeSegmentContactActor& object,
    const Vec3& point, GhoulySpawnPool* contacts, const Vec3& velocity, u16 word, u8 kind, u8 strength, u8 style,
    bool count)
{
    RuntimeWeaponHitNotice notice{0x1D, kind, strength, style, 0, &weapon.actor, velocity, contacts, point};
    object.message(&notice); // Vtable +0xC of the actor touched.
    if (holder != nullptr)
    {
        auto& services = weapon.services;
        std::array<u32, 0x42> report{};
        report[0] = 0x5A;
        report[1] = weapon.self_address();
        report[2] = u32(i32(i16(word)));
        std::memcpy(report.data() + 3, &velocity, 12);
        report[6] = services.actor_address(object.actor);
        report[7] = contacts != nullptr ? services.bind_address(contacts, sizeof(GhoulySpawnPool)) : 0;
        std::memcpy(report.data() + 8, &point, 12);
        report[0xB] = notice.reply;
        const u32 holder_address = services.actor_address(*holder);
        weapon.send(holder_address, report); // The holder's vtable +0xC.
        weapon.message(report); // This weapon's.
        if (contacts != nullptr)
        {
            services.unbind_address(contacts);
        }
        if (count && notice.reply != 0)
        {
            // DAT_0053E528 and DAT_00524E2C: the holder's tag row and the totals at row 0.
            const u16 tag = read_u16(weapon.bytes_at(holder_address), 6);
            ++services.contact_totals[size_t(tag) * 0x3D];
            ++services.contact_totals[0];
            ++services.contact_session[size_t(tag) * 0x3D];
            ++services.contact_session[0];
        }
    }
    return notice.reply;
}

// Xbox 0x00141C00, EAX velocity, EBX point, EDX style
u32 report_runtime_weapon_spray_contact(RuntimeWeaponActor& weapon, Actor* holder, RuntimeSegmentContactActor& object,
    const Vec3& point, GhoulySpawnPool* contacts, const Vec3& velocity, u8 kind, u8 strength, u8 style, bool count)
{
    RuntimeWeaponHitNotice notice{0x18, kind, strength, style, 0, &weapon.actor, velocity, contacts, point};
    object.message(&notice); // Vtable +0xC of the actor touched.
    if (holder != nullptr)
    {
        auto& services = weapon.services;
        std::array<u32, 0x42> report{};
        report[0] = 0x3F;
        report[1] = weapon.self_address();
        std::memcpy(report.data() + 2, &velocity, 12);
        report[5] = services.actor_address(object.actor);
        report[6] = contacts != nullptr ? services.bind_address(contacts, sizeof(GhoulySpawnPool)) : 0;
        std::memcpy(report.data() + 7, &point, 12);
        report[0xA] = notice.reply;
        const u32 holder_address = services.actor_address(*holder);
        weapon.send(holder_address, report); // The holder's vtable +0xC.
        weapon.message(report); // This weapon's.
        if (contacts != nullptr)
        {
            services.unbind_address(contacts);
        }
        if (count && notice.reply != 0)
        {
            // DAT_0053E528 and DAT_00524E2C: the holder's tag row and the totals at row 0.
            const u16 tag = read_u16(weapon.bytes_at(holder_address), 6);
            ++services.contact_totals[size_t(tag) * 0x3D];
            ++services.contact_totals[0];
            ++services.contact_session[size_t(tag) * 0x3D];
            ++services.contact_session[0];
        }
    }
    return notice.reply;
}

// Xbox 0x00141A30, EAX velocity, EBX point, EDX style
u32 report_runtime_projectile_contact(RuntimeWeaponActor& weapon, Actor* holder, RuntimeSegmentContactActor& object,
    const Vec3& point, GhoulySpawnPool* contacts, const Vec3& velocity, u16 word, u8 kind, u8 strength, u8 style,
    bool count, u32 tail)
{
    RuntimeProjectileHitNotice notice{0x19, kind, strength, style, 0, 0, 0, &weapon.actor, velocity, contacts, point,
        tail};
    object.message(&notice); // Vtable +0xC of the actor touched.
    if (holder != nullptr)
    {
        auto& services = weapon.services;
        std::array<u32, 0x42> report{};
        report[0] = 0x49;
        report[1] = weapon.self_address();
        report[2] = u32(i32(i16(word)));
        std::memcpy(report.data() + 3, &velocity, 12);
        report[6] = services.actor_address(object.actor);
        report[7] = contacts != nullptr ? services.bind_address(contacts, sizeof(GhoulySpawnPool)) : 0;
        std::memcpy(report.data() + 8, &point, 12);
        report[0xB] = notice.reply;
        report[0xC] = notice.result;
        report[0xD] = notice.standing;
        const u32 holder_address = services.actor_address(*holder);
        weapon.send(holder_address, report); // The holder's vtable +0xC.
        weapon.message(report); // This weapon's.
        if (contacts != nullptr)
        {
            services.unbind_address(contacts);
        }
        if (count && notice.reply != 0)
        {
            // DAT_0053E528 and DAT_00524E2C: the holder's tag row and the totals at row 0.
            const u16 tag = read_u16(weapon.bytes_at(holder_address), 6);
            ++services.contact_totals[size_t(tag) * 0x3D];
            ++services.contact_totals[0];
            ++services.contact_session[size_t(tag) * 0x3D];
            ++services.contact_session[0];
        }
    }
    return notice.reply;
}

// Xbox 0x000CE5D0
u32 test_runtime_projectile_contact(RuntimeWeaponActor& weapon, RuntimeSegmentContactActor& object, const Vec3& point,
    GhoulySpawnPool* contacts, const Vec3& velocity, u16 word)
{
    auto bytes = weapon.bytes;
    if (report_runtime_projectile_contact(weapon, weapon_holder(weapon.actor), object, point, contacts, velocity, word,
                                          weapon_base(bytes).hit_kind, weapon_base(bytes).hit_strength,
                                          weapon_base(bytes).hit_style, actor_record_view<LargeProjectileState>(bytes).hits == 0,
                                          0) != 0) // 0x00141A30
    {
        actor_record_view<LargeProjectileState>(bytes).hits = actor_record_view<LargeProjectileState>(bytes).hits + 1;
    }
    insert_runtime_body_contact_override(weapon.collision_overrides, object, 0, weapon.players); // 0x0013EBE0, +0xE7C
    return weapon_base(bytes).mask == 0 ? 1u : 0u;
}

// Xbox 0x000CF5F0
u32 test_runtime_chair_contact(RuntimeWeaponActor& weapon, RuntimeSegmentContactActor& object, const Vec3& point,
    GhoulySpawnPool* contacts, const Vec3& velocity, u16 word)
{
    auto bytes = weapon.bytes;
    report_runtime_weapon_contact(weapon, weapon_holder(weapon.actor), object, point, contacts, velocity, word,
                                  weapon_base(bytes).hit_kind, weapon_base(bytes).hit_strength,
                                  weapon_base(bytes).hit_style, chair_of(bytes).struck == 0); // 0x00141890
    return weapon_base(bytes).mask == 0 ? 1u : 0u;
}

// Xbox 0x001451C0, ECX direction, EDX actor, EAX velocity
void chip_runtime_weapon(RuntimeWeaponActor& weapon, const Vec3& direction, const Vec3& velocity, float fraction)
{
    // The direction and velocity feed products whose results retail drops; only the random steps remain.
    static_cast<void>(direction);
    static_cast<void>(velocity);
    Actor& actor = weapon.actor;
    auto bytes = weapon.bytes;
    const auto groups = [&]() -> u32
    {
        if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
        {
            return 0;
        }
        const auto cpu = actor.model->cpu;
        const u32 header = runtime_model_piece_header(cpu);
        return header == 0 ? 0u : cpu[header];
    };
    const auto remaining = [&]()
    {
        return 1.f - float(i32(weapon_base(bytes).broken_groups)) / float(i32(groups()) - 1); // DAT_0032B0F8.
    };
    if (weapon.matrices.matrices.count != 0 && i32(groups()) > 1)
    {
        if (!(remaining() > fraction))
        {
            return;
        }
        for (i32 step = 0; step < 9; ++step)
        {
            weapon.random = weapon.random * 0x19660Du + 0x3C6EF35Fu; // DAT_0054DD7C.
        }
        while (remaining() > fraction)
        {
            const auto cpu = actor.model->cpu;
            const u32 header = runtime_model_piece_header(cpu);
            const u32 group = weapon_base(bytes).broken_groups;
            bytes[0x298] = 0;
            RuntimeActorMatrixSampling sampling{actor_matrix_records(weapon.matrices), weapon.matrices.matrices.node,
                weapon.services.sampling_scratch, weapon.services.sampling_active, actor_transform_origin,
                weapon.model_source};
            launch_actor_model_piece_group(actor, i32(group), weapon.pieces, weapon.matrices, sampling,
                weapon.model_source, weapon.random); // 0x0003F820
            weapon_base(bytes).broken_pieces = weapon_base(bytes).broken_pieces + cpu[header + 0x16 + group * 0xC];
            weapon_base(bytes).broken_groups = group + 1;
        }
    }
    set_runtime_weapon_piece_pairs(weapon, fraction); // 0x001457A0
}

// Xbox 0x000CF670, ESI actor
void drop_runtime_chair_weapon(RuntimeWeaponActor& weapon)
{
    Actor& actor = weapon.actor;
    auto bytes = weapon.bytes;
    if (weapon.markers != nullptr)
    {
        deactivate_all_runtime_body_contact_shapes(*weapon.markers); // 0x0013F660
        std::array<u32, 0x42> message{};
        message[0] = 0x92;
        message[1] = 0;
        weapon.message(message); // Vtable +0xC.
    }
    weapon_base(bytes).mask = 0;
    actor.mask_flag = i32(u32(actor.mask_flag) & ~0xFFu);
    if (weapon_base(bytes).voice_name.data()[0] != 0)
    {
        const u16 voice = weapon_base(bytes).voice;
        weapon_base(bytes).voice_name.data()[0] = 0;
        if (voice != 0xFFFF)
        {
            stop_actor_sound_voice(i16(voice), weapon.services.sounds); // 0x00039690
            weapon_base(bytes).voice = 0xFFFF;
        }
    }
    actor.model_mode = 0; // +0x118.
    register_actor_shape_probe(actor, weapon.view.shape); // 0x0003C770
    const Actor* owner = actor.spawn_owner; // +0xD8C.
    weapon_base(bytes).break_category = owner != nullptr && (owner->mask_mode == 2 || owner->mask_flag != 0) ? 1 : 0;
    break_runtime_weapon(weapon); // 0x001454F0
}

// Xbox 0x00144CC0, EDI actor
bool start_runtime_weapon_prewait(RuntimeWeaponActor& weapon)
{
    auto& animation = weapon.view.animation;
    if (animation.table == nullptr) // +0x48.
    {
        return false;
    }
    const AnimationRecord* record = find_actor_animation(animation.table, "prewait", nullptr, weapon.random); // 0x0003AAA0
    if (record == nullptr)
    {
        return false;
    }
    start_actor_animation(animation, *record, weapon.registry, weapon.error, weapon.random); // 0x0003A9E0
    return true;
}

// Xbox 0x00147B30, EAX actor
void push_runtime_weapon_holder(RuntimeWeaponActor& weapon, Actor& holder)
{
    Actor& actor = weapon.actor;
    if (weapon_base(weapon.bytes).pickup_count != 0 ||
        weapon_base(weapon.bytes).flagged_marker_count == 0)
    {
        return;
    }
    actor_refresh_transform(actor); // 0x00040DE0
    Vec3 push{0.f, 0.f, 0.f};
    const EntityBonePose* pose = nullptr;
    if (actor.model != nullptr)
    {
        pose = actor.model->alternate_shape_pose;
        if (pose == nullptr)
        {
            pose = actor.model->primary_shape_pose;
        }
    }
    for (const u8* record : weapon.flagged_markers) // +0xAE8.
    {
        u32 mask = 0xFFFFFFFFu;
        if (actor.model != nullptr)
        {
            i16 joint;
            std::memcpy(&joint, record + 2, 2);
            mask = actor.model->runtime->shader_indices.at(size_t(joint)); // Model +0x16C by joint.
        }
        u32 shape_mask;
        std::memcpy(&shape_mask, record + 4, 4);
        if ((shape_mask & mask & 0x10) == 0 || record[0] != 0)
        {
            continue;
        }
        Vec3 center;
        const float radius = shape_sphere_metrics(*reinterpret_cast<const EntityShapeNode*>(record), actor.xform,
            pose, center); // 0x00056330
        std::array<u8, 0x14> shape{};
        shape[0] = 1;
        shape[1] = 0x14;
        shape[2] = 1;
        std::memcpy(shape.data() + 4, &center, 12);
        std::memcpy(shape.data() + 0x10, &radius, 4);
        push_runtime_weapon_marker(weapon, holder, shape, push); // 0x00147CD0
    }
    if (push.x != 0.f || push.z != 0.f)
    {
        std::array<u32, 0x42> message{};
        message[0] = 0x9E;
        message[1] = std::bit_cast<u32>(push.x);
        message[2] = 0;
        message[3] = std::bit_cast<u32>(push.z);
        weapon.send(weapon.services.actor_address(holder), message); // The holder's vtable +0xC.
    }
}

// Xbox 0x00147CD0, EAX holder
void push_runtime_weapon_marker(RuntimeWeaponActor& weapon, Actor& holder, std::span<const u8, 0x14> shape, Vec3& push)
{
    float radius;
    std::memcpy(&radius, shape.data() + 0x10, 4);
    Vec3 probe;
    std::memcpy(&probe, shape.data() + 4, 12);
    probe = Vec3{probe.x + push.x, probe.y + push.y, probe.z + push.z};
    if (entity_collect_sphere_contacts(probe, radius, 4, 0x001C4000u) <= 0) // 0x001227E0
    {
        return;
    }
    auto contacts = entity_merge_contact_records(); // 0x00122310
    Vec3 eye = actor_world_position(holder); // 0x000412A0
    const auto body = weapon.services.body_of(weapon.services.actor_address(holder)); // Holder +0x95C.
    eye.y = holder.radius_y * body_record(body).height_scale * 0.5f + eye.y; // DAT_0032B0F4.
    cull_back_facing_segment_contacts(contacts, eye); // 0x000FC840
    // Without rows 0x000FC3F0 does not run and the push gains nothing here.
    Vec3 separated = probe;
    if ((contacts.cursor - contacts.begin) / i32(contacts.stride) != 0)
    {
        separate_runtime_segment_contacts(contacts, probe, radius, separated, actor_transform_origin); // 0x000FC3F0
    }
    engine_heap_free(contacts.block); // 0x000663E0
    push.y = push.y + (separated.y - probe.y);
    push.x = push.x + (separated.x - probe.x);
    push.z = push.z + (separated.z - probe.z);
}

// Xbox 0x0012DC80, EAX uses
void show_runtime_weapon_uses(RuntimeWeaponActor& weapon, i32 uses, u32 caption)
{
    auto& hud = weapon.services.scoreboard;
    hud.damage_690 = 2.f; // DAT_0032B110.
    if (uses < 0)
    {
        uses = 0;
    }
    hud.damage_counter.value = uses;
    hud.damage_counter.target = uses;
    update_runtime_hud_counter_digits(hud.damage_counter, weapon.services.damage_preset); // 0x0012F990
    hud.damage_caption = i32(caption);
    load_runtime_scoreboard_weapon_icons(hud, weapon.registry, weapon.error); // 0x0012D8A0
    hud.damage_6e0 = 2;
}

// Xbox 0x00145C30, EAX actor
void update_runtime_weapon_cells(RuntimeWeaponActor& weapon, i32 preferred)
{
    if (entity_detail::g_nav_grids[0] == nullptr) // DAT_0050E240.
    {
        return;
    }
    const Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
    auto& floor = weapon_base(weapon.bytes).floor;
    entity_detail::update_navigation_floor(position, floor, weapon.cells, weapon.path_search, preferred, 1); // 0x00110C90
}

// Xbox 0x0003DE30, ECX actor, ESI object
bool release_runtime_actor_frame_object(RuntimeWeaponActor& weapon, u32 object)
{
    for (auto* update = weapon.view.frame.updates; update != nullptr; update = update->next) // Actor +0x1D8.
    {
        FactoryObject* owned = weapon.services.object_of(weapon.services.frame_objects.at(update));
        if (owned == nullptr)
        {
            throw Error("0x0003DE30 reads a frame object the scene does not hold");
        }
        if (weapon.object_address(*owned) != object)
        {
            continue;
        }
        std::array<u32, 0x42> notice{};
        notice[0] = 0x14;
        notice[1] = object;
        weapon.message(notice); // Vtable +0xC.
        release_factory_object(weapon.services.objects.factory, *owned); // 0x001075C0
        return true;
    }
    return false;
}

// Xbox 0x000CF0D0
void dispatch_runtime_chair_weapon_message(RuntimeWeaponActor& weapon, std::span<u32> message)
{
    auto bytes = weapon.bytes;
    switch (message[0])
    {
    case 0x14:
        if (chair_of(bytes).effect == message[1])
        {
            chair_of(bytes).effect = 0;
        }
        break;
    case 0x5A:
    {
        // A hit the chair took part in (word +0x2C); each counts one use while the holder is not a scripted player.
        if (message[0xB] == 0)
        {
            break;
        }
        const Actor* holder = weapon_holder(weapon.actor); // 0x00145FD0
        if (holder != nullptr && runtime_actor_is_player(*holder) && runtime_weapon_boost_active() != 0) // 0x00028D00; DAT_00512278
        {
            break;
        }
        if (chair_of(bytes).struck != 0)
        {
            break;
        }
        chair_of(bytes).struck = 1;
        i32 uses = i32(chair_of(bytes).uses);
        if (uses <= 0)
        {
            break;
        }
        --uses;
        chair_of(bytes).uses = u32(uses);
        if (uses < 1)
        {
            chair_of(bytes).spent = 1;
            break;
        }
        auto* contacts = static_cast<GhoulySpawnPool*>(weapon.services.bound_at(message[7]));
        entity_detail::SegmentPool pool;
        pool.stride = contacts->stride;
        pool.grow = contacts->grow;
        pool.begin = contacts->begin;
        pool.cursor = contacts->cursor;
        pool.limit = contacts->limit;
        Vec3 direction;
        normalized_segment_contact_direction(pool, direction); // 0x000FCA60
        Vec3 velocity;
        std::memcpy(&velocity, message.data() + 3, 12);
        chip_runtime_weapon(weapon, direction, velocity,
            float(i32(chair_of(bytes).uses)) / float(i32(chair_of(bytes).total_uses))); // 0x001451C0
        break;
    }
    case 0x91:
    {
        change_runtime_weapon_state(weapon, 3); // 0x0014A290
        // Picked up: a player gets the second use count.
        const Actor* taker = weapon.actor_or_null(message[1]);
        const u32 uses = taker != nullptr && runtime_actor_is_player(*taker) // 0x00028D00
            ? chair_of(bytes).player_uses : chair_of(bytes).taker_uses;
        chair_of(bytes).uses = uses;
        chair_of(bytes).total_uses = uses;
        break;
    }
    case 0x92:
    {
        if (message[1] != 0)
        {
            chair_of(bytes).struck = 0;
            break;
        }
        if (chair_of(bytes).spent == 0)
        {
            break;
        }
        Actor* holder = weapon_holder(weapon.actor); // 0x00145FD0
        if (holder == nullptr)
        {
            break;
        }
        // Spent: the chair leaves the holder's hand.
        set_runtime_weapon_mask(weapon, 0); // 0x00146230
        weapon.services.detach_from_holder(weapon.services.actor_address(*holder)); // 0x0001B4C0
        change_runtime_weapon_state(weapon, 6); // 0x0014A290
        break;
    }
    case 0x97:
        if (message[1] == 0)
        {
            Actor* owner = weapon.actor.spawn_owner; // +0xD8C.
            if (owner != nullptr && runtime_actor_is_live_player(*owner,
                player_record(weapon.bytes_at(weapon.services.actor_address(*owner))).live)) // 0x00028D20
            {
                // The scoreboard's weapon-only counter.
                auto& hud = weapon.services.scoreboard;
                hud.damage_690 = 2.f; // DAT_0032B110.
                hud.damage_counter.target = 0;
                hud.damage_counter.value = 0;
                update_runtime_hud_counter_digits(hud.damage_counter, weapon.services.damage_preset); // 0x0012F990
                hud.damage_caption = 7;
                load_runtime_scoreboard_weapon_icons(hud, weapon.registry, weapon.error); // 0x0012D8A0
                hud.damage_6e0 = 2;
            }
            change_runtime_weapon_state(weapon, 6); // 0x0014A290
            return;
        }
        break;
    case 0x9C:
        change_runtime_weapon_state(weapon, 7); // 0x0014A290
        break;
    default:
        break;
    }
    dispatch_runtime_weapon_message(weapon, message); // 0x001463D0
}

// Xbox 0x00042360, EAX timer, ECX actor
bool runtime_frame_timer_elapsed(float& timer, const Actor& actor, const ActorFrameContext& frame)
{
    const float step = frame_step(frame);
    const float rate = frame_rate(actor);
    if (timer > 0.f)
    {
        const float remaining = timer - rate * step;
        timer = remaining;
        if (!(0.f < remaining))
        {
            timer = 0.f;
            return true;
        }
    }
    return false;
}

// Xbox 0x001292F0, EAX point, ECX flags
Actor* find_nearest_runtime_case_actor(const Vec3* point, Actor& requester, u32 flags, float* distance,
    const std::vector<RuntimeScriptActorQueryView>& candidates, const RuntimeScriptActorQueryContext& context)
{
    Vec3 origin;
    if (point == nullptr)
    {
        origin = actor_world_position(requester); // 0x000412A0
    }
    else
    {
        origin = *point;
    }
    Actor* found = nullptr;
    float best = 0.f;
    for (const auto& entry : candidates) // DAT_00511B40, node - 0x93C.
    {
        auto candidate = entry;
        if (matches_runtime_script_actor_query(candidate, &requester, flags, context) == 0) // 0x00129420
        {
            continue;
        }
        const Vec3 position = actor_world_position(candidate.actor);
        const float dy = origin.y - position.y;
        const float dx = origin.x - position.x;
        const float dz = origin.z - position.z;
        const float measured = (dz * dz + dy * dy) + dx * dx;
        if (found == nullptr || best > measured)
        {
            if (runtime_script_actor_path_open(flags, &requester, candidate.actor)) // 0x001295E0
            {
                found = &candidate.actor;
                best = measured;
            }
        }
    }
    if (found != nullptr && distance != nullptr)
    {
        *distance = float(retail_square_root(best)); // 0x00152B30
    }
    return found;
}

// Xbox 0x0003FB80
void tick_actor_model_pieces(const Actor& actor, std::span<u8> actor_bytes, std::vector<u8>& pieces, ActorMatrixSetup& matrices,
    float step)
{
    if (pieces.empty()) // +0x2A4.
    {
        return;
    }
    actor_bytes[0x298] = 0;
    if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
    {
        throw Error("0x0003FB80 reads the animation header of an actor without a model");
    }
    const auto cpu = actor.model->cpu;
    const u32 header = runtime_model_piece_header(cpu);
    u32 row = 0;
    for (u32 group = 0; group < cpu[header]; ++group)
    {
        const u8 count = runtime_model_piece_group_size(cpu, header, group);
        const u32 list = runtime_model_piece_group_list(cpu, header, group);
        for (u32 piece = 0; piece < count; ++piece)
        {
            ActorModelPiece& part = actor_model_piece(pieces, row);
            if (part.active == 0)
            {
                ++row;
                continue;
            }
            part.age = part.age + step;
            if (part.age > 6.f) // DAT_0032B218.
            {
                // The expired piece stops; retail keeps the same row for the group's next piece.
                part.active = 0;
                continue;
            }
            // The piece flies by its velocity (+0) from its position (+0x30), as a sphere of its radius (+0x48).
            Vec3 next{part.position.x + part.velocity.x, part.position.y + part.velocity.y,
                part.position.z + part.velocity.z};
            Vec3 start{part.position.x, part.position.y, part.position.z};
            if (entity_record_sphere_probe(next, start, part.radius, -1, 0) != 0) // 0x001225A0
            {
                const u8* record = entity_detail::g_segment_slots[0].pool.begin; // DAT_00511A20 + 4.
                if (record != nullptr)
                {
                    const Vec3 contact_normal = segment_contact_normal({record, 0x54});
                    float nx = contact_normal.x;
                    float ny = contact_normal.y;
                    float nz = contact_normal.z;
                    const float length = (nz * nz + ny * ny) + nx * nx;
                    if (length != 0.f)
                    {
                        const long double scale = engine_reciprocal_sqrt(length); // 0x001534C0
                        nx = float(nx * scale);
                        ny = float(ny * scale);
                        nz = float(nz * scale);
                    }
                    // The velocity bounces off the surface, then halves.
                    const float bounce = (part.velocity.y * ny * -2.f - part.velocity.z * nz * 2.f) -
                        part.velocity.x * nx * 2.f; // DAT_0032B14C, DAT_0032B110.
                    part.velocity.z = nz * bounce + part.velocity.z;
                    part.velocity.y = ny * bounce + part.velocity.y;
                    part.velocity.x = part.velocity.x + bounce * nx;
                    part.velocity.x = part.velocity.x * 0.5f; // DAT_0032B0F4.
                    part.velocity.y = part.velocity.y * 0.5f;
                    part.velocity.z = part.velocity.z * 0.5f;
                    const long double vx = part.velocity.x;
                    const long double vy = part.velocity.y;
                    const long double vz = part.velocity.z;
                    const long double speed = std::sqrt((vx * vx + vy * vy) + vz * vz);
                    u32 surface;
                    std::memcpy(&surface, record + 0x20, 4);
                    if (static_cast<long double>(std::bit_cast<float>(0x3DCCCCCDu)) > speed && // DAT_0032B254.
                        part.age > 3.f && (surface & 8) != 0) // DAT_0032B260.
                    {
                        part.active = 0;
                    }
                }
            }
            else
            {
                part.velocity.y = part.velocity.y - std::bit_cast<float>(0x3D4CCCCDu); // DAT_0032B250 gravity.
            }
            part.position.x = next.x;
            part.position.y = next.y;
            part.position.z = next.z;
            constexpr float damping = std::bit_cast<float>(0x3F733333u); // DAT_0032B2B8.
            part.spin.x = part.spin.x * damping;
            part.spin.y = part.spin.y * damping;
            part.spin.z = part.spin.z * damping;
            part.euler.x = part.turn.x + part.spin.x;
            part.euler.y = part.turn.y + part.spin.y;
            part.euler.z = part.turn.z + part.spin.z;
            // The piece's matrix record (+0x288 by its node) takes the position, lowered by the radius past the
            // extent, and the wrapped euler; it redraws.
            ActorMatrixRecord& matrix = actor_matrix_records(matrices)[size_t(cpu[list + piece])];
            const float x = part.position.x;
            const float z = part.position.z;
            const float y = part.position.y - (part.radius - part.extent);
            matrix.translation.x = x;
            matrix.translation.z = z;
            matrix.translation.y = y;
            matrix.euler = part.euler;
            Vec3 euler = matrix.euler;
            wrap_runtime_euler_angles(euler); // 0x001059B0
            matrix.euler = euler;
            matrices.progress = 0.f; // +0x29C.
            matrices.flags = 0; // +0x2A0.
            matrix.active = 1;
            ++row;
        }
    }
}

// Xbox 0x00040040, EAX actor
bool actor_model_pieces_settled(const Actor& actor, const std::vector<u8>& pieces)
{
    if (pieces.empty())
    {
        return false;
    }
    if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
    {
        throw Error("0x00040040 reads the animation header of an actor without a model");
    }
    const auto cpu = actor.model->cpu;
    const u32 header = runtime_model_piece_header(cpu);
    u32 row = 0;
    for (u32 group = 0; group < cpu[header]; ++group)
    {
        const u8 count = runtime_model_piece_group_size(cpu, header, group);
        for (u32 piece = 0; piece < count; ++piece)
        {
            if (actor_model_piece(pieces, row).active != 0)
            {
                return false;
            }
            ++row;
        }
    }
    return true;
}

namespace
{
// Xbox 0x00122E20, EAX point
void notify_runtime_marks_in_cylinder(RuntimeWeaponActor& weapon, std::span<u32> message, const Vec3& point,
    float height, float lift, float reach)
{
    const float low = point.y + lift;
    const float high = (height - lift) + low;
    for (const auto& mark : weapon.marks()) // DAT_00511B38.
    {
        // The node's +0x28 is the body; body +8 is the actor it scales.
        const Actor& actor = *mark.actor;
        const Vec3 position = actor_world_position(actor); // 0x000412A0
        const float top_extent = actor.radius_y * body_record(mark.body).height_scale;
        float radius = actor.radius_xz * body_record(mark.body).radius_scale;
        if (position.y > high)
        {
            continue;
        }
        if (low > top_extent + position.y)
        {
            continue;
        }
        radius = radius + reach;
        const float dz = position.z - point.z;
        const float dx = position.x - point.x;
        const float distance = float(retail_square_root(dz * dz + dx * dx)); // 0x00152B30
        float measured;
        if (std::bit_cast<float>(0x3727C5ACu) > distance) // DAT_0032B230.
        {
            measured = radius - std::bit_cast<float>(0x3C23D70Au); // DAT_0032B124.
        }
        else
        {
            measured = distance;
        }
        if (!(measured >= radius))
        {
            weapon.send(mark.address, message); // Vtable +0xC.
        }
    }
}

// Xbox 0x00146CE0, ECX first, EAX second
long double runtime_point_distance(const Vec3& first, const Vec3& second)
{
    const long double dy = static_cast<long double>(first.y) - second.y;
    const long double dx = static_cast<long double>(first.x) - second.x;
    const long double dz = static_cast<long double>(first.z) - second.z;
    return std::sqrt((dy * dy + dx * dx) + dz * dz);
}

// Xbox 0x001478B0, EBX actor
bool nearest_runtime_weapon_grab_point(RuntimeWeaponActor& weapon, const Vec3& query, Vec3& nearest, float& radius)
{
    if (weapon.grab_shapes.empty()) // +0xAB4.
    {
        return false;
    }
    const Actor& actor = weapon.actor;
    const ModelPoseBuffer* pose = weapon_shape_pose(actor);
    AnimationMatrix world;
    std::memcpy(world.data(), actor.xform, 48);
    const auto point_of = [&](const u8* record, Vec3& output)
    {
        const auto binding = locator_binding(record);
        if (binding.joint >= 0 && pose == nullptr)
        {
            throw Error("weapon grab shape names a joint but the model instance has no pose");
        }
        model_bone_world_point(binding, pose, &world, output); // 0x00051CE0
    };
    point_of(weapon.grab_shapes[0], nearest);
    for (size_t index = 1; index < weapon.grab_shapes.size(); ++index)
    {
        Vec3 candidate;
        point_of(weapon.grab_shapes[index], candidate);
        const float fresh = ((query.z - candidate.z) * (query.z - candidate.z) +
            (query.y - candidate.y) * (query.y - candidate.y)) + (query.x - candidate.x) * (query.x - candidate.x);
        const float kept = ((query.z - nearest.z) * (query.z - nearest.z) +
            (query.y - nearest.y) * (query.y - nearest.y)) + (query.x - nearest.x) * (query.x - nearest.x);
        if (kept > fresh)
        {
            nearest = candidate;
        }
    }
    radius = float(actor_model_bound_radius(actor)); // 0x0003DB30
    return true;
}

// Xbox 0x00117560 with EAX 1: DAT_0050E768 clear and the decay byte DAT_0030F144[DAT_0051248C] set.
bool runtime_prompt_gate(const ActorFrameContext& frame)
{
    if (frame.block_decay != 0)
    {
        return false;
    }
    return frame.decay_enabled != 0;
}
}

// Xbox 0x00147430, EAX actor
void notify_runtime_weapon_grab_points(RuntimeWeaponActor& weapon)
{
    Actor& actor = weapon.actor;
    if (actor.spawn_blocked != 0) // +0xAD0.
    {
        weapon_base(weapon.bytes).prompt_wanted = 0;
        return;
    }
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    std::array<u32, 0x42> message{};
    message[0] = 0x94;
    message[1] = weapon.self_address();
    if (actor.spawn_point_count == 0) // +0xAC4.
    {
        return;
    }
    const EntityBonePose* pose = nullptr;
    if (actor.model != nullptr)
    {
        pose = actor.model->alternate_shape_pose;
        if (pose == nullptr)
        {
            pose = actor.model->primary_shape_pose;
        }
    }
    for (i32 index = 0; index < actor.spawn_point_count; ++index)
    {
        Vec3 center;
        const float reach = shape_sphere_metrics(*actor.spawn_points[index], actor.xform, pose, center); // 0x00056330
        float height = reach + reach;
        if (!(height >= 8.f)) // DAT_0032B26C.
        {
            height = 8.f;
        }
        height = height * std::bit_cast<float>(0x3F99999Au); // DAT_0032B24C.
        message[2] = std::bit_cast<u32>(center.x);
        message[3] = std::bit_cast<u32>(position.y);
        message[4] = std::bit_cast<u32>(center.z);
        const Vec3 point{center.x, position.y, center.z};
        notify_runtime_marks_in_cylinder(weapon, message, point, height, 0.f - height, height); // 0x00122E20
    }
    weapon_base(weapon.bytes).prompt_wanted = 1;
}

// Xbox 0x0014A220
void update_runtime_weapon_state(RuntimeWeaponActor& weapon, u16 state)
{
    switch (state)
    {
    case 2:
        update_runtime_weapon_flight(weapon); // 0x001488D0
        return;
    case 5:
        update_runtime_weapon_hop(weapon); // 0x00148F20
        return;
    case 8:
        update_runtime_weapon_drop(weapon); // 0x00149270
        return;
    case 10:
        update_runtime_weapon_follow(weapon); // 0x00149750
        return;
    default:
        return;
    }
}

// Xbox 0x000CF050
void update_runtime_chair_weapon_state(RuntimeWeaponActor& weapon, u16 state)
{
    // 0x000CF750: a chair a live player holds shows its uses on the HUD (0x0012DC80).
    if (const Actor* holder = weapon_holder(weapon.actor))
    {
        if (player_record(weapon.bytes_at(weapon.services.actor_address(*holder))).live != 0 &&
            (holder->mask_mode == 2 || holder->mask_flag != 0) && chair_of(weapon.bytes).total_uses != 0)
        {
            show_runtime_weapon_uses(weapon, i32(chair_of(weapon.bytes).uses), 7); // 0x0012DC80
        }
    }
    switch (state)
    {
    case 3:
    case 6:
    case 9:
    case 0xC:
        return;
    case 7:
        weapon_base(weapon.bytes).next_state = 0x10;
        return;
    case 0xD:
        if (actor_animation_completed(weapon.view.animation, 0)) // 0x0003B290
        {
            weapon_base(weapon.bytes).next_state = 0x10;
        }
        return;
    case 0x10:
        notify_runtime_weapon_grab_points(weapon); // 0x00147430
        return;
    default:
        update_runtime_weapon_state(weapon, state); // 0x0014A220
        return;
    }
}

// Xbox 0x00145A40, EAX actor
void update_runtime_weapon_bounds(RuntimeWeaponActor& weapon)
{
    Actor& actor = weapon.actor;
    const u32 held = weapon_holder(actor) != nullptr ? 1u : 0u;
    if (weapon_base(weapon.bytes).was_held == 0 && held == 0)
    {
        return;
    }
    std::array<u8, 0x20> bound{};
    auto& shapes = reinterpret_cast<GhoulySpawnPool*&>(weapon.view.allocation_8fc); // Actor +0x8FC.
    build_runtime_actor_bound(actor, bound.data(), 0xFFFFFFFFu, false, shapes); // 0x000426F0
    const float scale_y = actor.radius_y;
    const float scale_xz = actor.radius_xz;
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const float inverse = 1.f / scale_y;
    const MindTargetShape& shape = mind_target_shape(std::span<const u8>(bound));
    const float bottom = shape.position.y;
    const float low = (bottom - position.y) * inverse;
    const float high = ((shape.height + bottom) - position.y) * inverse;
    const float radius = shape.radius / scale_xz;
    actor.model_size_y = high; // +0x1C8
    actor.model_size_z = low; // +0x1CC
    actor.model_size_x = radius; // +0x1C4
    const float extent = high >= low ? high : low;
    actor.model_extent = float(retail_square_root(extent * extent + radius * radius)); // +0x1D0, 0x00152B30
    auto* attachment = static_cast<ActorLiveAttachment*>(weapon.view.live_attachments.next); // Actor +0x8F8.
    if (attachment != nullptr)
    {
        build_runtime_actor_bound(actor, bound.data(), 0x100, true, shapes); // 0x000426F0
        const u8 floor = u8(weapon_base(weapon.bytes).floor);
        copy_actor_shape(attachment->shape.data(), bound.data()); // 0x0010AA60
        copy_actor_shape(attachment->secondary_shape.data(), bound.data());
        attachment->origin = actor_transform_origin; // DAT_0050DD80.
        attachment->flag = floor;
    }
    weapon_base(weapon.bytes).was_held = held;
    if (held == 0)
    {
        register_runtime_weapon_cells(weapon); // 0x00145BC0
    }
}

// Xbox 0x00144E30, EAX actor
void update_runtime_weapon_markers(RuntimeWeaponActor& weapon)
{
    if (weapon.markers == nullptr) // +0x998.
    {
        return;
    }
    // The owner at +0xD8C keeps no collision filter (+0x1F0) while the weapon's markers walk.
    Actor* owner = weapon.actor.spawn_owner;
    u32 filter = 0;
    if (owner != nullptr)
    {
        filter = owner->model_flags;
        owner->model_flags = 0;
    }
    RuntimeBodyContactWalk walk{weapon.actor, *weapon.markers, contact_test(weapon), weapon.contact_actor,
        weapon.players, weapon.clock};
    update_runtime_body_contacts(walk); // 0x0013F2C0 with +0x99C
    if (owner != nullptr)
    {
        owner->model_flags = filter;
    }
}

// Xbox 0x00148510, EAX actor
void update_runtime_weapon_variant(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    const Actor& actor = weapon.actor;
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    if (weapon_base(bytes).unk_C00 != 0)
    {
        if (weapon_holder(actor) != nullptr || weapon_base(bytes).mask == 0 || actor.spawn_blocked != 0 ||
            actor.spawn_point_count == 0)
        {
            weapon_base(bytes).trigger_radius = 0.f;
        }
    }
    if (weapon_base(bytes).mask != 0)
    {
        const i32 result = check_ghouly_spawn_trigger(position, weapon_base(bytes).trigger_radius,
                                                      weapon_base(bytes).trigger_chance,
                                                      weapon.random); // 0x0013E980
        if (result == 1)
        {
            request_runtime_weapon_variant(weapon, weapon_base(bytes).variant_name.data(),
                weapon_base(bytes).variant_attributes.data()); // 0x00148430
        }
        else if (result == -1 && weapon_base(bytes).unk_BF8 != 0)
        {
            weapon_base(bytes).trigger_radius = 0.f;
        }
    }
    if (weapon.spawn_record != nullptr && weapon.spawn_record->remaining_c == 0) // +0xC04, its +0xC.
    {
        release_ghouly_spawn_record(*weapon.spawn_record); // 0x000F3A00
        weapon.spawn_record = nullptr;
    }
}

// Xbox 0x00145820, EAX actor
void finish_runtime_weapon_pieces(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    if (weapon_base(bytes).fades_out != 0)
    {
        // 0x00107720 / 0x00107520: the fade-out with +8 this actor and +0x10 one.
        weapon.create_object("aid_objparams_ghoulies_fx_fadeout", [](std::span<u8> parameters)
        {
            const u32 one = 1;
            std::memcpy(parameters.data() + 0x10, &one, 4);
        });
        weapon_base(bytes).pieces_finished = 1;
        return;
    }
    if (weapon_base(bytes).unk_D98 != 0)
    {
        throw Error("weapon piece finish 0x00145820 finds +0xD98 set, which no retail writer does"); // 0x000ED2E0
    }
    if (weapon_base(bytes).unk_D9C != 0)
    {
        throw Error("weapon piece finish 0x00145820 finds +0xD9C set, which no retail writer does"); // 0x001450A0
    }
    weapon.pieces.clear(); // +0x2A4, 0x000663E0.
    refresh_actor_model_matrices(weapon.actor, weapon.matrices.matrices, 1, weapon.matrix_boundaries); // 0x00040200
    store8(bytes, 0x298, 1);
    refresh_actor_model_matrices(weapon.actor, weapon.matrices.matrices, 0, weapon.matrix_boundaries);
}

// Xbox 0x00145740, ESI actor
void update_runtime_weapon_pieces(RuntimeWeaponActor& weapon)
{
    if (weapon_base(weapon.bytes).pieces_finished != 0)
    {
        return;
    }
    tick_actor_model_pieces(weapon.actor, weapon.bytes, weapon.pieces, weapon.matrices, frame_step(weapon.frame)); // 0x0003FB80
    if (actor_model_pieces_settled(weapon.actor, weapon.pieces) && // 0x00040040
        weapon_base(weapon.bytes).broken_pieces == u32(weapon.matrices.matrices.count)) // +0x28C.
    {
        finish_runtime_weapon_pieces(weapon); // 0x00145820
    }
}

// Xbox 0x0014A6D0, EAX actor
void check_runtime_weapon_world_bounds(RuntimeWeaponActor& weapon)
{
    const Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
    if (g_amb_volume_enabled == 0) // DAT_0050EA00.
    {
        return;
    }
    // DAT_0050EA04..DAT_0050EA18: the minimum corner, the maximum x, the floor height and the maximum z.
    float top = g_amb_color_hi[1];
    if (!(top >= position.y))
    {
        top = position.y;
    }
    top = top + 1.f; // DAT_0032B0F8.
    if (g_amb_color_lo[0] > position.x || position.x >= g_amb_color_hi[0] || g_amb_color_lo[1] > position.y ||
        position.y >= top || g_amb_color_lo[2] > position.z || position.z >= g_amb_color_hi[2])
    {
        std::array<u32, 0x42> message{};
        message[0] = 0x98;
        weapon.message(message); // Vtable +0xC.
    }
}

// Xbox 0x00147160, EAX actor
void update_runtime_weapon_prompt(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    Actor& actor = weapon.actor;
    const float step = frame_step(weapon.frame);
    const float rate = frame_rate(actor);
    float pulse = weapon_base(bytes).prompt_pulse - rate * step;
    weapon_base(bytes).prompt_pulse = pulse;
    if (!(0.f < pulse))
    {
        pulse = pulse + std::bit_cast<float>(0x3F333333u); // DAT_0032B264.
        weapon_base(bytes).prompt_pulse = pulse;
    }
    constexpr float opaque = 192.f; // DAT_0032B454.
    float& fade = weapon_base(bytes).prompt_fade;
    const auto set_state = [&](u16 value, float time)
    {
        weapon_base(bytes).prompt_phase = value;
        weapon_base(bytes).prompt_fade = time;
    };
    switch (i16(weapon_base(bytes).prompt_phase))
    {
    case 0:
        if (weapon_base(bytes).prompt_wanted != 0 && runtime_prompt_gate(weapon.frame)) // 0x00117560
        {
            float distance = 0.f;
            Actor* player = find_nearest_runtime_case_actor(nullptr, actor, 0x1A000, &distance,
                weapon.case_actors(), weapon.query); // 0x001292F0
            if (player != nullptr)
            {
                const Vec3 position = actor_world_position(*player); // 0x000412A0
                Vec3 nearest{};
                float radius = distance;
                if (nearest_runtime_weapon_grab_point(weapon, position, nearest, radius)) // 0x001478B0
                {
                    const long double gap = runtime_point_distance(position, nearest) - radius; // 0x00146CE0
                    if (45.L > gap) // DAT_0032B578.
                    {
                        set_state(1, 0.75f); // DAT_0032B2AC.
                    }
                }
            }
        }
        break;
    case 1:
    {
        const float level = opaque - fade * std::bit_cast<float>(0x3FAAAAABu) * opaque; // DAT_0032B3C0.
        float clamped;
        if (0.f > level)
        {
            clamped = 0.f;
        }
        else if (level > opaque)
        {
            clamped = opaque;
        }
        else
        {
            clamped = level;
        }
        weapon_base(bytes).prompt_alpha = u8(i32(clamped)); // 0x001673D8.
        if (runtime_frame_timer_elapsed(fade, actor, weapon.frame)) // 0x00042360
        {
            weapon_base(bytes).prompt_phase = 2;
        }
        break;
    }
    case 2:
    {
        weapon_base(bytes).prompt_alpha = 0xC0;
        float distance = 0.f;
        Actor* player = find_nearest_runtime_case_actor(nullptr, actor, 0x1A000, &distance, weapon.case_actors(),
            weapon.query); // 0x001292F0
        if (player != nullptr)
        {
            const Vec3 position = actor_world_position(*player); // 0x000412A0
            // Retail reads the point and radius whether or not 0x001478B0 found a grab shape.
            Vec3 nearest{};
            float radius = distance;
            nearest_runtime_weapon_grab_point(weapon, position, nearest, radius); // 0x001478B0
            const long double gap = runtime_point_distance(position, nearest) - radius; // 0x00146CE0
            if (!(gap > 55.L) && weapon_base(bytes).prompt_wanted != 0 &&
                runtime_prompt_gate(weapon.frame)) // DAT_0032B528.
            {
                break;
            }
        }
        set_state(3, 1.f);
        break;
    }
    case 3:
    {
        const float level = fade * opaque;
        float clamped;
        if (level > opaque)
        {
            clamped = opaque;
        }
        else if (0.f > level)
        {
            clamped = 0.f;
        }
        else
        {
            clamped = level;
        }
        weapon_base(bytes).prompt_alpha = u8(i32(clamped)); // 0x001673D8.
        if (runtime_frame_timer_elapsed(fade, actor, weapon.frame)) // 0x00042360
        {
            weapon_base(bytes).prompt_phase = 0;
            weapon_base(bytes).prompt_alpha = 0;
        }
        break;
    }
    default:
        break;
    }
    weapon_base(bytes).prompt_visible = weapon_base(bytes).prompt_alpha != 0 ? 1 : 0;
    weapon_base(bytes).prompt_wanted = 0;
}

// Xbox 0x0014A770, EAX actor
void update_runtime_weapon_voice(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    if (weapon_base(bytes).voice_name.data()[0] == 0)
    {
        return;
    }
    AudioEventVoices& voices = weapon.services.voices;
    if (weapon_base(bytes).voice == 0xFFFF)
    {
        // The carried sound (+0xD08) starts at the weapon.
        const i16 voice = allocate_audio_event_voice(1, voices); // 0x00039830, EBX 1
        weapon_base(bytes).voice = u16(voice);
        if (voice == -1)
        {
            return;
        }
        const Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
        voices.voices[size_t(voice)].stop = 0;
        if (AudioCoreSource* source = voices.voices[size_t(voice)].source)
        {
            set_audio_core_source_position(*source, position, voices.core.sources); // 0x0015C800
        }
        prepare_audio_event_voice(voice, weapon_base(bytes).voice_name.data(), voices); // 0x00039A20
        return;
    }
    // The playing sound follows the weapon.
    const Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
    const i16 voice = i16(weapon_base(bytes).voice);
    if (AudioCoreSource* source = voices.voices[size_t(voice)].source)
    {
        set_audio_core_source_position(*source, position, voices.core.sources); // 0x0015C800
    }
}

// Xbox 0x0014A2E0
void update_runtime_weapon_actor(RuntimeWeaponActor& weapon)
{
    Actor& actor = weapon.actor;
    ActorFrameState& state = weapon.view.frame;
    ActorFrameContext& context = weapon.frame;
    float step = frame_step(context);
    float rate = frame_rate(actor);
    const float time = rate * step + weapon_base(weapon.bytes).state_time;
    weapon_base(weapon.bytes).state_time = time;
    update_state(weapon, weapon_base(weapon.bytes).state); // Vtable +0x44.
    if (Actor* holder = weapon_holder(actor))
    {
        // The held weapon's animation runs at the holder's slot 0 duration (+0x20).
        float duration = 0.f;
        auto& holder_animation = *weapon.services.animation_of(*holder);
        if (holder_animation.controller != nullptr && holder_animation.controller->slots[0].player != nullptr)
        {
            duration = holder_animation.controller->slots[0].player->duration;
        }
        if (state.animation.controller != nullptr)
        {
            retime_mina_playback(state.animation.controller->slots[0].player.get(), duration,
                state.animation.controller->dirty); // 0x00030DB0
        }
    }
    step = frame_step(context);
    rate = frame_rate(actor);
    if (state.animation.controller != nullptr)
    {
        tick_animation_controller(*state.animation.controller, rate * step, state.bone == 0,
            context.effects.models.registry); // 0x00030BA0
    }
    if (state.animation.controller != nullptr)
    {
        apply_controller_to_model(*state.animation.controller, state.model, state.bone == 0,
            context.pose_callbacks, context.node_cache, context.reset_matrix); // 0x00030C70
    }
    ActorFrameAttachment* update = state.updates; // Actor +0x1D8.
    while (update != nullptr)
    {
        ActorFrameAttachment* next = update->next;
        update->update(*update, actor);
        update = next;
    }
    update_actor_attachment_effects(actor, state.attachments, state.pending, context.effects); // 0x0003CD00
    float z = state.target.z;
    float y = state.target.y;
    float x = state.target.x;
    if (context.ease != 0) // DAT_00509B74.
    {
        constexpr float fraction = std::bit_cast<float>(0x3DCCCCCDu); // DAT_0032B254.
        x = (x - state.eased.x) * fraction + state.eased.x;
        y = (y - state.eased.y) * fraction + state.eased.y;
        z = (z - state.eased.z) * fraction + state.eased.z;
    }
    state.eased.x = x;
    state.eased.y = y;
    state.eased.z = z;
    if (actor.model != nullptr && actor.model->runtime->shader_animations != nullptr)
    {
        tick_runtime_model_shader_animations(*actor.model->runtime->shader_animations, 1.f,
            context.shader_matrices); // 0x00165A00
    }
    if (weapon_holder(actor) != nullptr)
    {
        actor_refresh_transform(actor); // 0x00040DE0
    }
    // +0x92C follows the owner at +0xD8C: its rate unless its +0x930 holds, else 1.
    if (actor.spawn_owner != nullptr)
    {
        actor.animation_rate = actor.spawn_owner->animation_pause > 0.f ? 0.f : actor.spawn_owner->animation_rate;
    }
    else
    {
        actor.animation_rate = 1.f; // DAT_0032B0F8.
    }
    late_update_state(weapon, weapon_base(weapon.bytes).state); // Vtable +0x48.
    update_runtime_weapon_bounds(weapon); // 0x00145A40
    update_runtime_weapon_markers(weapon); // 0x00144E30
    if (Actor* holder = weapon_holder(actor))
    {
        push_runtime_weapon_holder(weapon, *holder); // 0x00147B30
    }
    const u16 pending = weapon_base(weapon.bytes).next_state;
    if (pending != 0)
    {
        change_runtime_weapon_state(weapon, pending); // As 0x0014A290.
    }
    for (auto* anchor = static_cast<ActorModelAnchor*>(state.anchors.next); anchor != nullptr;
        anchor = static_cast<ActorModelAnchor*>(anchor->owner_link.next)) // Actor +0x1F8.
    {
        update_actor_model_anchor(*anchor); // 0x0011BE40
    }
    update_runtime_weapon_variant(weapon); // 0x00148510
    update_runtime_weapon_pieces(weapon); // 0x00145740
    check_runtime_weapon_world_bounds(weapon); // 0x0014A6D0
    update_runtime_weapon_prompt(weapon); // 0x00147160
    update_runtime_weapon_voice(weapon); // 0x0014A770
    if (context.block_decay == 0 && context.decay_enabled != 0)
    {
        step = frame_step(context);
        if (0.f < actor.animation_pause)
        {
            const float remaining = actor.animation_pause - step;
            actor.animation_pause = remaining;
            if (!(0.f < remaining))
            {
                actor.animation_pause = 0.f;
            }
        }
    }
    step = frame_step(context);
    rate = frame_rate(actor);
    state.elapsed = rate * step + state.elapsed;
    ++state.ticks;
}

// Xbox 0x001459C0's holder test
bool runtime_weapon_draw_hidden(const RuntimeWeaponActor& weapon)
{
    const Actor* holder = weapon_holder(weapon.actor);
    if (holder == nullptr)
    {
        return false;
    }
    // Hidden while a live player holds it and DAT_0050EA9C bit 2 is set.
    if (player_record(weapon.bytes_at(weapon.services.actor_address(*holder))).live == 0)
    {
        return false;
    }
    if (holder->mask_mode != 2 && holder->mask_flag == 0)
    {
        return false;
    }
    return (weapon.runtime_flags & 4) != 0;
}

// Xbox 0x00147560, EAX pass, EBX actor
void draw_runtime_weapon_prompt(RuntimeWeaponActor& weapon, i32 pass)
{
    if (pass != 2 || weapon_base(weapon.bytes).prompt_visible == 0)
    {
        return;
    }
    const Actor& actor = weapon.actor;
    const ModelPoseBuffer* pose = weapon_shape_pose(actor);
    const float frames = float(i32(weapon.hand_texture[0x11]));
    const float level =
        weapon_base(weapon.bytes).prompt_pulse * std::bit_cast<float>(0x3FB6DB6Eu) * (0.f - frames) +
        frames; // DAT_0032B2E0.
    float clamped;
    if (0.f > frames)
    {
        if (level > 0.f)
        {
            clamped = 0.f;
        }
        else if (frames > level)
        {
            clamped = frames;
        }
        else
        {
            clamped = level;
        }
    }
    else
    {
        if (0.f > level)
        {
            clamped = 0.f;
        }
        else if (level > frames)
        {
            clamped = frames;
        }
        else
        {
            clamped = level;
        }
    }
    const u32 frame = u8(i32(clamped)); // 0x001673D8.
    const u32 color = (u32(weapon_base(weapon.bytes).prompt_alpha) << 24) | 0xFFFFFFu;
    const float size[2]{2.5f, 2.5f}; // DAT_0032B20C.
    AnimationMatrix world;
    std::memcpy(world.data(), actor.xform, 48);
    for (const u8* record : weapon.grab_shapes)
    {
        const auto binding = locator_binding(record);
        if (binding.joint >= 0 && pose == nullptr)
        {
            throw Error("weapon grab shape names a joint but the model instance has no pose");
        }
        Vec3 point;
        model_bone_world_point(binding, pose, &world, point); // 0x00051CE0
        queue_billboard(g_billboard_queue, weapon.hand_texture.data(), color, frame, point, size, 0.f); // 0x0005C1A0
    }
}

// Xbox 0x00147860, EBX actor
void block_runtime_weapon_spawn(RuntimeWeaponActor& weapon)
{
    if (weapon.actor.spawn_blocked == 0)
    {
        // Actor +0x1F8 is the first model anchor.
        auto* node = static_cast<entity_detail::ListHead*>(weapon.view.anchors.next);
        while (node != nullptr)
        {
            auto* next = static_cast<entity_detail::ListHead*>(node->next);
            release_actor_model_anchor(*reinterpret_cast<ActorModelAnchor*>(node)); // 0x0011BDA0
            node = next;
        }
    }
    set_spawn_blocked(weapon, weapon.actor.spawn_blocked + 1);
}

// Xbox 0x0012D050, EDI unblock
void step_runtime_weapon_spawn_blocks(i32 unblock, const std::function<RuntimeWeaponActor&(Actor&)>& weapon_of)
{
    for (auto* node = static_cast<ActorSpawnLink*>(entity_detail::g_world.body_b.next); node != nullptr;
         node = node->next) // DAT_00511B28
    {
        if (node->actor == nullptr)
        {
            throw Error("DAT_00511B24 node has no actor");
        }
        RuntimeWeaponActor& weapon = weapon_of(*node->actor); // EBX = node - 0x934
        if (unblock == 0)
        {
            block_runtime_weapon_spawn(weapon); // 0x00147860
        }
        else
        {
            set_spawn_blocked(weapon, weapon.actor.spawn_blocked - 1); // +0xAD0
            if (weapon.actor.spawn_blocked == 0)
            {
                register_actor_model_anchors(weapon.actor, weapon.view.anchors); // 0x0003EEB0
            }
        }
    }
}

// Xbox 0x00145C80
bool initialize_runtime_weapon_actor(RuntimeWeaponActor& weapon, std::span<const u8> parameters)
{
    auto bytes = weapon.bytes;
    Actor& actor = weapon.actor;
    weapon_base(bytes).mask = 1;
    actor.mask_flag = i32((u32(actor.mask_flag) & ~0xFFu) | 1u);
    weapon_base(bytes).unk_93F = get(parameters, weapon_parameter::unk_93F);
    weapon_base(bytes).unk_93D = get(parameters, weapon_parameter::unk_93D);
    actor.mask_flag = i32(read_u32(bytes, offsetof(WeaponBase, mask))); // Native alias of the +0x93C dword (0x001460C0 reads +0x93F).
    weapon_base(bytes).fades_out = get(parameters, weapon_parameter::fades_out);
    weapon_base(bytes).awards_points = get(parameters, weapon_parameter::awards_points);
    weapon_base(bytes).hit_kind = get(parameters, weapon_parameter::hit_kind);
    weapon_base(bytes).hit_strength = get(parameters, weapon_parameter::hit_strength);
    weapon_base(bytes).hit_style = get(parameters, weapon_parameter::hit_style);
    weapon_base(bytes).unk_DA8 = get(parameters, weapon_parameter::unk_DA8);
    copy_runtime_weapon_classes(weapon, parameters); // 0x00144A80
    weapon.model_nodes.clear();
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        const u32 table = read_u32(cpu, 0);
        for (u32 index = 0; index < read_u32(cpu, 4); ++index)
        {
            const u32 offset = read_u32(cpu, table + index * 8 + 4);
            weapon.model_nodes.push_back({read_u32(cpu, table + index * 8), offset == 0 ? nullptr : cpu.data() + offset});
        }
    }
    const RuntimeBodyModelNodes nodes{weapon.model_nodes};
    weapon.markers = create_runtime_body_collision_state(actor.model != nullptr && actor.model->cpu.data() != nullptr
        ? &nodes : nullptr); // 0x0013F150 into +0x998
    weapon_base(bytes).starts_scene = get(parameters, weapon_parameter::starts_scene);
    initialize_runtime_weapon_names(weapon, parameters); // 0x00145150
    collect_runtime_weapon_grab_shapes(weapon); // 0x00146D10
    collect_runtime_weapon_flagged_markers(weapon); // 0x00147E10
    pick_runtime_weapon_variant(weapon, parameters); // 0x00147F00
    weapon_base(bytes).initial_state = get(parameters, weapon_parameter::initial_state);
    weapon_base(bytes).was_held = 1;
    weapon_base(bytes).broken_groups = 0;
    weapon_base(bytes).broken_pieces = 0;
    weapon_base(bytes).unk_D98 = 0;
    weapon_base(bytes).pieces_finished = 0;
    weapon_base(bytes).voice = 0xFFFF;
    const u32 name = get(parameters, weapon_parameter::name);
    if (name != 0)
    {
        const auto text = weapon.executable(name, 0);
        if (std::memchr(text.data(), 0, text.size()) == nullptr)
        {
            throw Error("weapon parameter +0x33C name has no terminator");
        }
        copy_string(bytes, offsetof(WeaponBase, name), sizeof(WeaponBase::name),
                    reinterpret_cast<const char*>(text.data()));
    }
    copy_string(bytes, offsetof(WeaponBase, animation), sizeof(WeaponBase::animation),
                parameter_string(parameters, actor_parameter_field::animation.offset, 0x80));
    initialize_actor_model_matrices(actor, weapon.matrices, weapon.model_types,
        weapon.matrix_bindings(actor)); // 0x000400D0
    if (get(parameters, weapon_parameter::blocks_spawn) != 0)
    {
        block_runtime_weapon_spawn(weapon); // 0x00147860
    }
    if (field_data(parameters, weapon_parameter::voice_name)[0] != 0)
    {
        copy_string(bytes, offsetof(WeaponBase, voice_name), sizeof(WeaponBase::voice_name),
                    parameter_string(parameters, weapon_parameter::voice_name.offset, 0x80));
    }
    actor.shape_mode = 4; // +0x120.
    const u16 state = weapon_base(bytes).initial_state;
    weapon_base(bytes).next_state = 0;
    leave_state(weapon, weapon_base(bytes).state, state); // Vtable +0x40.
    const u16 previous = weapon_base(bytes).state;
    weapon_base(bytes).state = state;
    weapon_base(bytes).state_time = 0.f;
    enter_state(weapon, state, previous); // Vtable +0x3C.
    const u32 key = get(parameters, weapon_parameter::controller_key);
    if (key != 0)
    {
        if (g_ghouly_controllers.size() == g_ghouly_controllers.capacity())
        {
            g_ghouly_controllers.reserve(g_ghouly_controllers.size() + 10); // 0x000662A0.
        }
        g_ghouly_controllers.push_back({key, &weapon.controller});
    }
    store8(bytes, 0x298, 1);
    refresh_actor_model_matrices(actor, weapon.matrices.matrices, 0, weapon.matrix_boundaries); // 0x00040200
    return true;
}

// Xbox 0x000CEFE0
bool initialize_runtime_chair_weapon(RuntimeWeaponActor& weapon, std::span<const u8> parameters)
{
    auto bytes = weapon.bytes;
    weapon_base(bytes).contact_test = 0x000CF5F0; // The chair's grab test, called through +0x99C.
    chair_of(bytes).taker_uses = get(parameters, chair_weapon_parameter::taker_uses);
    chair_of(bytes).player_uses = get(parameters, chair_weapon_parameter::player_uses);
    if (get(parameters, chair_weapon_parameter::unk_600) != 0)
    {
        weapon_base(bytes).unk_942 = 1;
    }
    if (chair_of(bytes).player_uses == 0)
    {
        weapon_base(bytes).unk_941 = 1;
    }
    copy_string(bytes, offsetof(ChairWeaponState, effect_name), sizeof(ChairWeaponState::effect_name),
        parameter_string(parameters, 0x604, 0x80));
    return true;
}

// Xbox 0x0014A120
void enter_runtime_weapon_state(RuntimeWeaponActor& weapon, u16 state, u16 previous)
{
    (void)previous;
    switch (state)
    {
    case 2:
        enter_runtime_weapon_flight(weapon); // 0x001487C0
        return;
    case 5:
    case 8:
        weapon.actor.model_flags &= ~0x100u;
        return;
    case 10:
        enter_runtime_weapon_follow(weapon); // 0x001495C0
        return;
    default:
        return;
    }
}

// Xbox 0x0014A180
void leave_runtime_weapon_state(RuntimeWeaponActor& weapon, u16 state, u16 next)
{
    (void)next;
    switch (state)
    {
    case 2:
        leave_runtime_weapon_flight(weapon); // 0x00148850
        return;
    case 5:
        weapon.actor.model_flags |= 0x100u;
        weapon.actor.anchor = actor_transform_origin; // DAT_0050DD80..DAT_0050DD88.
        return;
    case 8:
        weapon.actor.model_flags |= 0x100u;
        return;
    case 10:
        leave_runtime_weapon_follow(weapon); // 0x001496D0
        return;
    default:
        return;
    }
}

// Xbox 0x000CF400
void enter_runtime_chair_weapon_state(RuntimeWeaponActor& weapon, u16 state, u16 previous)
{
    switch (state)
    {
    case 6:
        if (actor_has_spawn_model(weapon.actor)) // 0x00145470
        {
            drop_runtime_chair_weapon(weapon); // 0x000CF670
        }
        set_runtime_weapon_mask(weapon, 0); // 0x00146230
        return;
    case 7:
        return;
    case 0xC:
        block_runtime_weapon_spawn(weapon); // 0x00147860
        weapon.actor.model_mode |= 4;
        register_actor_shape_probe(weapon.actor, weapon.view.shape); // 0x0003C770
        start_runtime_weapon_idle(weapon); // 0x00144D00
        return;
    case 0xD:
        start_runtime_weapon_prewait(weapon); // 0x00144CC0
        return;
    case 0x10:
        register_runtime_weapon_cells(weapon); // 0x00145BC0
        weapon.actor.model_mode |= 4;
        register_actor_shape_probe(weapon.actor, weapon.view.shape); // 0x0003C770
        weapon.actor.model_flags |= 0x100u;
        start_runtime_weapon_idle(weapon); // 0x00144D00
        if (weapon_base(weapon.bytes).unk_DA8 == 0)
        {
            attach_runtime_weapon_volume(weapon); // 0x00146310
        }
        if (read_u32(retail_bytes(chair_of(weapon.bytes).effect_name), 0) == kAidPrefix)
        {
            chair_of(weapon.bytes).effect = weapon.create_object(chair_of(weapon.bytes).effect_name.data(), {}); // 0x0003DF40
        }
        return;
    default:
        enter_runtime_weapon_state(weapon, state, previous); // 0x0014A120
        return;
    }
}

// Xbox 0x000CF510
void leave_runtime_chair_weapon_state(RuntimeWeaponActor& weapon, u16 state, u16 next)
{
    auto bytes = weapon.bytes;
    if (state != 0)
    {
        if (state == 0xC)
        {
            auto* controller = weapon.view.animation.controller.get(); // +0x50.
            if (read_u32(bytes, offsetof(WeaponBase, animation)) == kAidPrefix && controller != nullptr)
            {
                if (!controller->slots.empty() && controller->slots[0].player != nullptr)
                {
                    --controller->slots[0].player->paused;
                }
                controller->dirty = true;
            }
            set_spawn_blocked(weapon, weapon.actor.spawn_blocked - 1);
            if (weapon.actor.spawn_blocked == 0)
            {
                register_actor_model_anchors(weapon.actor, weapon.view.anchors); // 0x0003EEB0
            }
            return;
        }
        if (state != 0x10)
        {
            leave_runtime_weapon_state(weapon, state, next); // 0x0014A180
            return;
        }
        if (read_u32(bytes, offsetof(WeaponBase, animation)) == kAidPrefix)
        {
            // 0x0003B340 on slot 0: the slot's player resumes and the controller is marked dirty.
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
        weapon.actor.model_flags &= ~0x100u;
        clear_actor_live_attachments(weapon.view.live_attachments); // 0x00042040
        if (chair_of(bytes).effect != 0)
        {
            release_runtime_actor_frame_object(weapon, chair_of(bytes).effect); // 0x0003DE30
            chair_of(bytes).effect = 0;
        }
    }
    if (weapon.cells != nullptr)
    {
        entity_detail::release_navigation_cell_registration(weapon.cells,
                                                            weapon_base(bytes).floor); // 0x001108B0
    }
    weapon.actor.model_mode &= ~4;
    register_actor_shape_probe(weapon.actor, weapon.view.shape); // 0x0003C770
}
}
