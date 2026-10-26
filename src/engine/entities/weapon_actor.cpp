#include "weapon_actor.h"
#include "thrown_projectile.h"
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

#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace gbtg
{
using namespace weapon_detail;

constexpr u32 kChairScriptHold = 0; // DAT_00512278, BSS that no retail code writes.

namespace
{
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
    store32(weapon.bytes, 0xAD0, u32(value));
}

// Xbox 0x00144A80, EAX actor
void copy_runtime_weapon_classes(RuntimeWeaponActor& weapon, std::span<const u8> parameters)
{
    const i32 count = i32(read_u32(parameters, 0x458));
    store16(weapon.bytes, 0x944, read_u16(parameters, 0x458));
    store16(weapon.bytes, 0x946, 0);
    weapon.actor.accepted_class_count = i16(read_u16(parameters, 0x458));
    weapon.actor.accepted_classes = reinterpret_cast<const ActorAcceptedClass*>(weapon.bytes.data() + 0x948);
    if (count <= 0)
    {
        return;
    }
    if (count > i32(weapon.accepted_resources.size()))
    {
        throw Error("weapon accepted class rows overflow actor +0x948");
    }
    const auto rows = weapon.executable(read_u32(parameters, 0x45C), size_t(count) * 0x8C);
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
    copy_string(weapon.bytes, 0x9A8, 0x80, parameter_string(parameters, 0x474, 0x80));
    copy_string(weapon.bytes, 0xA28, 0x80, parameter_string(parameters, 0x4F4, 0x80));
    store16(weapon.bytes, 0xAA8, 1);
    store32(weapon.bytes, 0xD9C, 0);
    store32(weapon.bytes, 0x9A4, read_u32(parameters, 0x464) == 0 ? 1u : 0u);
    weapon.pieces.clear(); // +0x2A4 = 0.
    build_actor_model_pieces(weapon.actor, weapon.pieces, weapon.random); // 0x0003F170
}

// Xbox 0x00146D10
void collect_runtime_weapon_grab_shapes(RuntimeWeaponActor& weapon)
{
    const Actor& actor = weapon.actor;
    store32(weapon.bytes, 0xAB0, std::bit_cast<u32>(0.7f)); // DAT_0032B264.
    weapon.hand_texture = acquire_resource(weapon, "aid_texture_ghoulies_weapon_interacthand");
    store16(weapon.bytes, 0xADC, 0);
    store8(weapon.bytes, 0xADE, 0);
    weapon.grab_shapes.clear();
    store32(weapon.bytes, 0xAB4, 0);
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
        store32(weapon.bytes, 0xAB4, u32(weapon.grab_shapes.size()));
    }
    weapon.spawn_points.clear();
    for (const u8* record = find_runtime_actor_shape(actor, 0x400, 0, -1); record != nullptr;
        record = next_runtime_actor_shape(actor, record, 0x400, 0, -1))
    {
        weapon.spawn_points.push_back(reinterpret_cast<const EntityShapeNode*>(record));
    }
    weapon.actor.spawn_points = weapon.spawn_points.data();
    weapon.actor.spawn_point_count = i32(weapon.spawn_points.size());
    store32(weapon.bytes, 0xAC4, u32(weapon.spawn_points.size()));
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
        store8(weapon.bytes, 0xAC0 + index, kept);
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
    u16 count = read_u16(weapon.bytes, 0xAE6);
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
    store16(weapon.bytes, 0xAE6, count);
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
    if (roll <= read_f32(parameters, 0x340))
    {
        copy_string(weapon.bytes, 0xAEC, 0x80, parameter_string(parameters, 0x354, 0x80));
        copy_string(weapon.bytes, 0xB6C, 0x80, parameter_string(parameters, 0x3D4, 0x80));
        store32(weapon.bytes, 0xBF0, read_u32(parameters, 0x344));
        store32(weapon.bytes, 0xBF4, read_u32(parameters, 0x348));
        store32(weapon.bytes, 0xBF8, read_u32(parameters, 0x34C));
    }
    store32(weapon.bytes, 0xBEC, read_u32(parameters, 0x454));
    store32(weapon.bytes, 0xBFC, read_u32(parameters, 0x350));
    store32(weapon.bytes, 0xC00, 0);
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
    if (weapon.type == 0xA3 || weapon.type == 0xA5 || weapon.type == 0xA6)
    {
        update_runtime_gun_weapon_state(weapon, state); // The gun's 0x000CAA80
        return;
    }
    throw Error("weapon factory type " + std::to_string(weapon.type) + " vtable +0x44 is not translated");
}

// Vtable +0x48 of the weapon's class: the chair and both projectiles keep the empty base slot.
void late_update_state(RuntimeWeaponActor& weapon, u16 state)
{
    if (weapon.type == 0xAA || weapon.type == 0xA9 || weapon.type == 0xA7 || weapon.type == 0xA0 || weapon.type == 0xA1
        || weapon.type == 0xA2)
    {
        return;
    }
    if (weapon.type == 0xA3 || weapon.type == 0xA5 || weapon.type == 0xA6)
    {
        late_update_runtime_gun_weapon_state(weapon, state); // The gun's 0x000CAF10
        return;
    }
    throw Error("weapon factory type " + std::to_string(weapon.type) + " vtable +0x48 is not translated");
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
    if (weapon.type == 0xA3 || weapon.type == 0xA5 || weapon.type == 0xA6)
    {
        leave_runtime_gun_weapon_state(weapon, state); // The gun's 0x000CAEB0
        return;
    }
    throw Error("weapon factory type " + std::to_string(weapon.type) + " vtable +0x40 is not translated");
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
    if (weapon.type == 0xA3 || weapon.type == 0xA5 || weapon.type == 0xA6)
    {
        enter_runtime_gun_weapon_state(weapon, state); // The gun's 0x000CAE90
        return;
    }
    throw Error("weapon factory type " + std::to_string(weapon.type) + " vtable +0x3C is not translated");
}
// Actor +0x99C, the class's contact test: the chair sets 0x000CF5F0.
RuntimeBodyContactCallback contact_test(RuntimeWeaponActor& weapon)
{
    if (read_u32(weapon.bytes, 0x99C) == 0x000CF5F0)
    {
        return [&weapon](Actor&, RuntimeSegmentContactActor& object, const Vec3& point, GhoulySpawnPool* contacts,
            const RuntimeSegmentContactRecord&, const Vec3& velocity, u16 word) -> u32
        {
            return test_runtime_chair_contact(weapon, object, point, contacts, velocity, word); // 0x000CF5F0
        };
    }
    if (read_u32(weapon.bytes, 0x99C) == 0x000CB040)
    {
        return [&weapon](Actor&, RuntimeSegmentContactActor& object, const Vec3& point, GhoulySpawnPool* contacts,
            const RuntimeSegmentContactRecord&, const Vec3& velocity, u16 word) -> u32
        {
            return test_runtime_gun_contact(weapon, object, point, contacts, velocity, word); // 0x000CB040
        };
    }
    if (read_u32(weapon.bytes, 0x99C) == 0x000C6F10)
    {
        return [&weapon](Actor&, RuntimeSegmentContactActor& object, const Vec3& point, GhoulySpawnPool* contacts,
            const RuntimeSegmentContactRecord&, const Vec3& velocity, u16 word) -> u32
        {
            return test_runtime_candle_contact(weapon, object, point, contacts, velocity, word); // 0x000C6F10
        };
    }
    if (read_u32(weapon.bytes, 0x99C) == 0x000C61D0)
    {
        return [&weapon](Actor&, RuntimeSegmentContactActor& object, const Vec3& point, GhoulySpawnPool* contacts,
            const RuntimeSegmentContactRecord&, const Vec3& velocity, u16 word) -> u32
        {
            return test_runtime_bob_contact(weapon, object, point, contacts, velocity, word); // 0x000C61D0
        };
    }
    if (read_u32(weapon.bytes, 0x99C) == 0x000CE5D0)
    {
        return [&weapon](Actor&, RuntimeSegmentContactActor& object, const Vec3& point, GhoulySpawnPool* contacts,
            const RuntimeSegmentContactRecord&, const Vec3& velocity, u16 word) -> u32
        {
            return test_runtime_projectile_contact(weapon, object, point, contacts, velocity, word); // 0x000CE5D0
        };
    }
    if (read_u32(weapon.bytes, 0x99C) == 0x000CBF80)
    {
        return [&weapon](Actor&, RuntimeSegmentContactActor& object, const Vec3& point, GhoulySpawnPool* contacts,
            const RuntimeSegmentContactRecord&, const Vec3& velocity, u16 word) -> u32
        {
            return test_runtime_thrown_projectile_contact(weapon, object, point, contacts, velocity, word); // 0x000CBF80
        };
    }
    throw Error("weapon contact test at +0x99C is not translated");
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
    store8(weapon.bytes, 0x93C, value);
    weapon.actor.mask_flag = i32((u32(weapon.actor.mask_flag) & ~0xFFu) | value);
    if (value == 0 && weapon.bytes[0xD08] != 0)
    {
        store8(weapon.bytes, 0xD08, 0);
        const u16 voice = read_u16(weapon.bytes, 0xD88);
        if (voice != 0xFFFF)
        {
            stop_actor_sound_voice(i16(voice), weapon.services.sounds); // 0x00039690
            store16(weapon.bytes, 0xD88, 0xFFFF);
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
            entity_detail::register_navigation_cells(weapon.cells, center, i32(read_u32(weapon.bytes, 0xD00)), radius,
                0); // 0x001104E0
            return;
        }
        if (weapon.cells != nullptr)
        {
            entity_detail::release_navigation_cell_registration(weapon.cells, read_u32(weapon.bytes, 0xD00)); // 0x001108B0
        }
    }
}

// Xbox 0x00144D00, ESI actor
void start_runtime_weapon_idle(RuntimeWeaponActor& weapon)
{
    if (read_u32(weapon.bytes, 0xDD0) == kAidPrefix)
    {
        start_runtime_weapon_animation(weapon, reinterpret_cast<const char*>(weapon.bytes.data() + 0xDD0));
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
        u8(read_u32(weapon.bytes, 0xD00)), 0, shape.data(), actor_transform_origin); // 0x0012AFE0
    store32(weapon.bytes, 0xAAC, 1);
}

// Xbox 0x0014A290, ESI actor, EDI state
void change_runtime_weapon_state(RuntimeWeaponActor& weapon, u16 state)
{
    store16(weapon.bytes, 0xC0E, 0);
    leave_state(weapon, read_u16(weapon.bytes, 0xC0C), state); // Vtable +0x40.
    const u16 previous = read_u16(weapon.bytes, 0xC0C);
    store16(weapon.bytes, 0xC0C, state);
    store32(weapon.bytes, 0xC14, 0);
    enter_state(weapon, state, previous); // Vtable +0x3C.
}

// Xbox 0x00144FD0, EAX actor
void start_runtime_weapon_scene(RuntimeWeaponActor& weapon)
{
    if (read_u32(weapon.bytes, 0xC18) != 0)
    {
        untranslated(0x0013B980, "the weapon scene start 0x00144FD0 (+0xC18 set)");
    }
}

// Xbox 0x00148600, ESI actor
void bind_runtime_weapon_group(RuntimeWeaponActor& weapon)
{
    const u16 group = read_u16(weapon.bytes, 0x1FE);
    if (group == 0)
    {
        return;
    }
    u32 owner = 0;
    for (const auto* node = entity_detail::g_world.latch; node != nullptr; node = node->next) // DAT_00511AA4.
    {
        const u32 address = weapon.latch_owner(*node);
        if (read_u16(weapon.bytes_at(address), 0x1FC) == group)
        {
            owner = address;
            break;
        }
    }
    store32(weapon.bytes, 0xC08, owner);
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
        store32(weapon.bytes, 0xD8C, 0);
    }
    if (read_u32(weapon.bytes, 0xC08) == actor)
    {
        store32(weapon.bytes, 0xC08, 0);
    }
}

// Xbox 0x0014A0D0, ESI actor
void reset_runtime_weapon_state(RuntimeWeaponActor& weapon)
{
    store16(weapon.bytes, 0xC0E, 0);
    leave_state(weapon, read_u16(weapon.bytes, 0xC0C), 0); // Vtable +0x40.
    const u16 previous = read_u16(weapon.bytes, 0xC0C);
    store16(weapon.bytes, 0xC0C, 0);
    store32(weapon.bytes, 0xC14, 0);
    enter_state(weapon, 0, previous); // Vtable +0x3C.
    store16(weapon.bytes, 0xC0E, 0);
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
        store16(weapon.bytes, 0xAE6, 0);
    }
    if (read_u32(weapon.bytes, 0xD9C) != 0)
    {
        untranslated(0x001450A0, "the weapon destructor 0x00145E80 (+0xD9C set)");
    }
    weapon.pieces.clear(); // +0x2A4, 0x000663E0.
    if (weapon.cells != nullptr)
    {
        entity_detail::release_navigation_cell_registration(weapon.cells, read_u32(weapon.bytes, 0xD00)); // 0x001108B0
    }
    if (read_u16(weapon.bytes, 0xD88) != 0xFFFF)
    {
        stop_actor_sound_voice(i16(read_u16(weapon.bytes, 0xD88)), weapon.services.sounds); // 0x00039690
        store16(weapon.bytes, 0xD88, 0xFFFF);
    }
    if (read_u32(weapon.bytes, 0xD98) != 0)
    {
        untranslated(0x000ED2E0, "the weapon destructor 0x00145E80 (+0xD98 set)");
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
    store16(bytes, 0xAA8, 0);
    if (message[4] != 0)
    {
        // The player's body +0x270 / +0x274 name its two partners; a live one picks the category.
        const auto body = weapon.services.body_of(message[4]);
        const Actor* first = weapon.actor_or_null(read_u32(body, 0x270));
        const Actor* second = weapon.actor_or_null(read_u32(body, 0x274));
        if (first != nullptr && (first->mask_mode == 2 || first->mask_flag != 0))
        {
            store16(bytes, 0xAA8, 2);
        }
        else if (second != nullptr && (second->mask_mode == 2 || second->mask_flag != 0))
        {
            store16(bytes, 0xAA8, 3);
        }
        else
        {
            store16(bytes, 0xAA8, 0);
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
        store16(bytes, 0xAA8, category);
    }
    else
    {
        store16(bytes, 0xAA8, 0);
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
        floor = i32(read_u32(weapon.bytes_at(weapon.services.actor_address(owner)), 0xCEC));
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
    const i32 floor = i32(read_u32(weapon.bytes_at(weapon.services.actor_address(*actor.spawn_owner)), 0xCEC));
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
    const u32 group = read_u32(weapon.bytes, 0xBEC);
    if (group != 0)
    {
        // 0xA6 asks the group's owner (+0xC08) for the pose; 0x4D gives it to the new body.
        std::array<u32, 0x42> ask{};
        ask[0] = 0xA6;
        std::memcpy(ask.data() + 1, &position, 12);
        std::memcpy(ask.data() + 4, &euler, 12);
        const u32 owner = read_u32(weapon.bytes, 0xC08);
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
    const u16 row = read_u16(weapon.bytes, 0x946);
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
            store16(weapon.bytes, 0xAA8, 0);
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
        store32(weapon.bytes, 0xD00, u32(entity_detail::navigation_floor_index(position, -1, 0))); // 0x00110BF0
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
        if (read_u32(weapon.bytes, 0xE50) == message[1])
        {
            store32(weapon.bytes, 0xE50, 0);
        }
        return;
    case 0x93:
        store16(weapon.bytes, 0xC0E, 9);
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
            read_u32(weapon.bytes, 0xC00) == 0) != 0 ? 1u : 0u; // 0x00147FC0
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
        if (read_u32(weapon.bytes, 0xC18) != 0 && read_u32(weapon.bytes, 0xC1C) == message[1] &&
            read_u32(weapon.bytes, 0xC20) == message[2])
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
        report[0xC] = notice.unused_14;
        report[0xD] = notice.unused_18;
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
            bytes[0xDAC], bytes[0xDAD], bytes[0xDAE], read_u32(bytes, 0xE88) == 0, 0) != 0) // 0x00141A30
    {
        store32(bytes, 0xE88, read_u32(bytes, 0xE88) + 1);
    }
    insert_runtime_body_contact_override(weapon.collision_overrides, object, 0, weapon.players); // 0x0013EBE0, +0xE7C
    return bytes[0x93C] == 0 ? 1u : 0u;
}

// Xbox 0x000CF5F0
u32 test_runtime_chair_contact(RuntimeWeaponActor& weapon, RuntimeSegmentContactActor& object, const Vec3& point,
    GhoulySpawnPool* contacts, const Vec3& velocity, u16 word)
{
    auto bytes = weapon.bytes;
    report_runtime_weapon_contact(weapon, weapon_holder(weapon.actor), object, point, contacts, velocity, word,
        bytes[0xDAC], bytes[0xDAD], bytes[0xDAE], read_u32(bytes, 0xE64) == 0); // 0x00141890
    return bytes[0x93C] == 0 ? 1u : 0u;
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
        const u32 header = read_u32(cpu, read_u32(cpu, 0x10) + 0x10);
        return header == 0 ? 0u : cpu[header];
    };
    const auto remaining = [&]()
    {
        return 1.f - float(i32(read_u32(bytes, 0xD90))) / float(i32(groups()) - 1); // DAT_0032B0F8.
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
            const u32 header = read_u32(cpu, read_u32(cpu, 0x10) + 0x10);
            const u32 group = read_u32(bytes, 0xD90);
            bytes[0x298] = 0;
            RuntimeActorMatrixSampling sampling{{weapon.matrices.allocation,
                size_t(weapon.matrices.matrices.count) * 0x48}, weapon.matrices.matrices.node,
                weapon.services.sampling_scratch, weapon.services.sampling_active, actor_transform_origin,
                weapon.model_source};
            launch_actor_model_piece_group(actor, i32(group), weapon.pieces, weapon.matrices, sampling,
                weapon.model_source, weapon.random); // 0x0003F820
            store32(bytes, 0xD94, read_u32(bytes, 0xD94) + cpu[header + 0x16 + group * 0xC]);
            store32(bytes, 0xD90, group + 1);
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
    const Actor* owner = actor.spawn_owner; // +0xD8C.
    store16(bytes, 0xAA8, owner != nullptr && (owner->mask_mode == 2 || owner->mask_flag != 0) ? 1 : 0);
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
    if (read_u16(weapon.bytes, 0xAE4) != 0 || read_u16(weapon.bytes, 0xAE6) == 0)
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
    eye.y = holder.radius_y * read_f32(body, 0x884) * 0.5f + eye.y; // DAT_0032B0F4.
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
    auto& floor = *reinterpret_cast<i32*>(weapon.bytes.data() + 0xD00);
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
        if (read_u32(bytes, 0xE6C) == message[1])
        {
            store32(bytes, 0xE6C, 0);
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
        if (holder != nullptr && runtime_actor_is_player(*holder) && kChairScriptHold != 0) // 0x00028D00; DAT_00512278
        {
            break;
        }
        if (read_u32(bytes, 0xE64) != 0)
        {
            break;
        }
        store32(bytes, 0xE64, 1);
        i32 uses = i32(read_u32(bytes, 0xE5C));
        if (uses <= 0)
        {
            break;
        }
        --uses;
        store32(bytes, 0xE5C, u32(uses));
        if (uses < 1)
        {
            store32(bytes, 0xE68, 1);
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
            float(i32(read_u32(bytes, 0xE5C))) / float(i32(read_u32(bytes, 0xE60)))); // 0x001451C0
        break;
    }
    case 0x91:
    {
        change_runtime_weapon_state(weapon, 3); // 0x0014A290
        // Picked up: a player gets the second use count.
        const Actor* taker = weapon.actor_or_null(message[1]);
        const u32 uses = taker != nullptr && runtime_actor_is_player(*taker) // 0x00028D00
            ? read_u32(bytes, 0xE58) : read_u32(bytes, 0xE54);
        store32(bytes, 0xE5C, uses);
        store32(bytes, 0xE60, uses);
        break;
    }
    case 0x92:
    {
        if (message[1] != 0)
        {
            store32(bytes, 0xE64, 0);
            break;
        }
        if (read_u32(bytes, 0xE68) == 0)
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
                read_u32(weapon.bytes_at(weapon.services.actor_address(*owner)), 0xCE0))) // 0x00028D20
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
    const u32 header = read_u32(cpu, read_u32(cpu, 0x10) + 0x10);
    const auto put = [&](size_t offset, float value)
    {
        std::memcpy(pieces.data() + offset, &value, 4);
    };
    const auto get = [&](size_t offset)
    {
        return read_f32(pieces, offset);
    };
    u32 row = 0;
    for (u32 group = 0; group < cpu[header]; ++group)
    {
        const u8 count = cpu[header + 0x16 + group * 0xC];
        const u32 list = header + read_u16(cpu, header + 0x14 + group * 0xC);
        for (u32 piece = 0; piece < count; ++piece)
        {
            const size_t offset = size_t(row) * 0xB0;
            if (read_u32(pieces, offset + 0x8C) == 0)
            {
                ++row;
                continue;
            }
            put(offset + 0xA8, get(offset + 0xA8) + step);
            if (get(offset + 0xA8) > 6.f) // DAT_0032B218.
            {
                // The expired piece stops; retail keeps the same row for the group's next piece.
                const u32 zero = 0;
                std::memcpy(pieces.data() + offset + 0x8C, &zero, 4);
                continue;
            }
            // The piece flies by its velocity (+0) from its position (+0x30), as a sphere of its radius (+0x48).
            Vec3 next{get(offset + 0x30) + get(offset), get(offset + 0x34) + get(offset + 4),
                get(offset + 0x38) + get(offset + 8)};
            Vec3 start{get(offset + 0x30), get(offset + 0x34), get(offset + 0x38)};
            if (entity_record_sphere_probe(next, start, get(offset + 0x48), -1, 0) != 0) // 0x001225A0
            {
                const u8* record = entity_detail::g_segment_slots[0].pool.begin; // DAT_00511A20 + 4.
                if (record != nullptr)
                {
                    float nx = read_f32({record, 0x54}, 0xC);
                    float ny = read_f32({record, 0x54}, 0x10);
                    float nz = read_f32({record, 0x54}, 0x14);
                    const float length = (nz * nz + ny * ny) + nx * nx;
                    if (length != 0.f)
                    {
                        const long double scale = engine_reciprocal_sqrt(length); // 0x001534C0
                        nx = float(nx * scale);
                        ny = float(ny * scale);
                        nz = float(nz * scale);
                    }
                    // The velocity bounces off the surface, then halves.
                    const float bounce = (get(offset + 4) * ny * -2.f - get(offset + 8) * nz * 2.f) -
                        get(offset) * nx * 2.f; // DAT_0032B14C, DAT_0032B110.
                    put(offset + 8, nz * bounce + get(offset + 8));
                    put(offset + 4, ny * bounce + get(offset + 4));
                    put(offset, get(offset) + bounce * nx);
                    put(offset, get(offset) * 0.5f); // DAT_0032B0F4.
                    put(offset + 4, get(offset + 4) * 0.5f);
                    put(offset + 8, get(offset + 8) * 0.5f);
                    const long double vx = get(offset);
                    const long double vy = get(offset + 4);
                    const long double vz = get(offset + 8);
                    const long double speed = std::sqrt((vx * vx + vy * vy) + vz * vz);
                    u32 surface;
                    std::memcpy(&surface, record + 0x20, 4);
                    if (static_cast<long double>(std::bit_cast<float>(0x3DCCCCCDu)) > speed && // DAT_0032B254.
                        get(offset + 0xA8) > 3.f && (surface & 8) != 0) // DAT_0032B260.
                    {
                        const u32 zero = 0;
                        std::memcpy(pieces.data() + offset + 0x8C, &zero, 4);
                    }
                }
            }
            else
            {
                put(offset + 4, get(offset + 4) - std::bit_cast<float>(0x3D4CCCCDu)); // DAT_0032B250 gravity.
            }
            put(offset + 0x30, next.x);
            put(offset + 0x34, next.y);
            put(offset + 0x38, next.z);
            constexpr float damping = std::bit_cast<float>(0x3F733333u); // DAT_0032B2B8.
            put(offset + 0xC, get(offset + 0xC) * damping);
            put(offset + 0x10, get(offset + 0x10) * damping);
            put(offset + 0x14, get(offset + 0x14) * damping);
            put(offset + 0x3C, get(offset + 0x18) + get(offset + 0xC));
            put(offset + 0x40, get(offset + 0x1C) + get(offset + 0x10));
            put(offset + 0x44, get(offset + 0x20) + get(offset + 0x14));
            // The piece's matrix record (+0x288 by its node) takes the position, lowered by the radius past the
            // extent, and the wrapped euler; it redraws.
            u8* matrix = matrices.allocation + size_t(cpu[list + piece]) * 0x48;
            const float x = get(offset + 0x30);
            const float z = get(offset + 0x38);
            const float y = get(offset + 0x34) - (get(offset + 0x48) - get(offset + 0xAC));
            std::memcpy(matrix + 4, &x, 4);
            std::memcpy(matrix + 0xC, &z, 4);
            std::memcpy(matrix + 8, &y, 4);
            std::memcpy(matrix + 0x10, pieces.data() + offset + 0x3C, 12);
            Vec3 euler;
            std::memcpy(&euler, matrix + 0x10, 12);
            wrap_runtime_euler_angles(euler); // 0x001059B0
            std::memcpy(matrix + 0x10, &euler, 12);
            matrices.progress = 0.f; // +0x29C.
            matrices.flags = 0; // +0x2A0.
            const u32 one = 1;
            std::memcpy(matrix + 0x44, &one, 4);
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
    const u32 header = read_u32(cpu, read_u32(cpu, 0x10) + 0x10);
    u32 row = 0;
    for (u32 group = 0; group < cpu[header]; ++group)
    {
        const u8 count = cpu[header + 0x16 + group * 0xC];
        for (u32 piece = 0; piece < count; ++piece)
        {
            if (read_u32(pieces, size_t(row) * 0xB0 + 0x8C) != 0)
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
        const float top_extent = actor.radius_y * read_f32(mark.body, 0x884);
        float radius = actor.radius_xz * read_f32(mark.body, 0x888);
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
        store32(weapon.bytes, 0xAD4, 0);
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
    store32(weapon.bytes, 0xAD4, 1);
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
        // 0x00149750 follows the owner through the path search 0x001495C0 builds, which needs DAT_0050E24C.
        untranslated(0x00149750, "the weapon state update 0x0014A220 (state 10)");
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
        if (read_u32(weapon.bytes_at(weapon.services.actor_address(*holder)), 0xCE0) != 0 &&
            (holder->mask_mode == 2 || holder->mask_flag != 0) && read_u32(weapon.bytes, 0xE60) != 0)
        {
            show_runtime_weapon_uses(weapon, i32(read_u32(weapon.bytes, 0xE5C)), 7); // 0x0012DC80
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
        store16(weapon.bytes, 0xC0E, 0x10);
        return;
    case 0xD:
        if (actor_animation_completed(weapon.view.animation, 0)) // 0x0003B290
        {
            store16(weapon.bytes, 0xC0E, 0x10);
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
    if (read_u32(weapon.bytes, 0xAAC) == 0 && held == 0)
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
    const float bottom = read_f32(bound, 8);
    const float low = (bottom - position.y) * inverse;
    const float high = ((read_f32(bound, 0x14) + bottom) - position.y) * inverse;
    const float radius = read_f32(bound, 0x1C) / scale_xz;
    std::memcpy(weapon.bytes.data() + 0x1C8, &high, 4);
    std::memcpy(weapon.bytes.data() + 0x1CC, &low, 4);
    std::memcpy(weapon.bytes.data() + 0x1C4, &radius, 4);
    const float extent = high >= low ? high : low;
    const float reach = float(retail_square_root(extent * extent + radius * radius)); // 0x00152B30
    std::memcpy(weapon.bytes.data() + 0x1D0, &reach, 4);
    auto* attachment = static_cast<ActorLiveAttachment*>(weapon.view.live_attachments.next); // Actor +0x8F8.
    if (attachment != nullptr)
    {
        build_runtime_actor_bound(actor, bound.data(), 0x100, true, shapes); // 0x000426F0
        const u8 floor = u8(read_u32(weapon.bytes, 0xD00));
        copy_actor_shape(attachment->shape.data(), bound.data()); // 0x0010AA60
        copy_actor_shape(attachment->secondary_shape.data(), bound.data());
        attachment->origin = actor_transform_origin; // DAT_0050DD80.
        attachment->flag = floor;
    }
    store32(weapon.bytes, 0xAAC, held);
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
    if (read_u32(bytes, 0xC00) != 0)
    {
        if (weapon_holder(actor) != nullptr || bytes[0x93C] == 0 || actor.spawn_blocked != 0 ||
            actor.spawn_point_count == 0)
        {
            store32(bytes, 0xBF0, 0);
        }
    }
    if (bytes[0x93C] != 0)
    {
        const i32 result = check_ghouly_spawn_trigger(position, read_f32(bytes, 0xBF0), read_f32(bytes, 0xBF4),
            weapon.random); // 0x0013E980
        if (result == 1)
        {
            request_runtime_weapon_variant(weapon, reinterpret_cast<const char*>(bytes.data() + 0xAEC),
                reinterpret_cast<const char*>(bytes.data() + 0xB6C)); // 0x00148430
        }
        else if (result == -1 && read_u32(bytes, 0xBF8) != 0)
        {
            store32(bytes, 0xBF0, 0);
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
    if (read_u32(bytes, 0xDA0) != 0)
    {
        // 0x00107720 / 0x00107520: the fade-out with +8 this actor and +0x10 one.
        weapon.create_object("aid_objparams_ghoulies_fx_fadeout", [](std::span<u8> parameters)
        {
            const u32 one = 1;
            std::memcpy(parameters.data() + 0x10, &one, 4);
        });
        store8(bytes, 0xDAF, 1);
        return;
    }
    if (read_u32(bytes, 0xD98) != 0)
    {
        untranslated(0x000ED2E0, "the weapon piece finish 0x00145820 (+0xD98 set)");
    }
    if (read_u32(bytes, 0xD9C) != 0)
    {
        untranslated(0x001450A0, "the weapon piece finish 0x00145820 (+0xD9C set)");
    }
    weapon.pieces.clear(); // +0x2A4, 0x000663E0.
    refresh_actor_model_matrices(weapon.actor, weapon.matrices.matrices, 1, weapon.matrix_boundaries); // 0x00040200
    store8(bytes, 0x298, 1);
    refresh_actor_model_matrices(weapon.actor, weapon.matrices.matrices, 0, weapon.matrix_boundaries);
}

// Xbox 0x00145740, ESI actor
void update_runtime_weapon_pieces(RuntimeWeaponActor& weapon)
{
    if (weapon.bytes[0xDAF] != 0)
    {
        return;
    }
    tick_actor_model_pieces(weapon.actor, weapon.bytes, weapon.pieces, weapon.matrices, frame_step(weapon.frame)); // 0x0003FB80
    if (actor_model_pieces_settled(weapon.actor, weapon.pieces) && // 0x00040040
        read_u32(weapon.bytes, 0xD94) == u32(weapon.matrices.matrices.count)) // +0x28C.
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
    float pulse = read_f32(bytes, 0xAB0) - rate * step;
    std::memcpy(bytes.data() + 0xAB0, &pulse, 4);
    if (!(0.f < pulse))
    {
        pulse = pulse + std::bit_cast<float>(0x3F333333u); // DAT_0032B264.
        std::memcpy(bytes.data() + 0xAB0, &pulse, 4);
    }
    constexpr float opaque = 192.f; // DAT_0032B454.
    float& fade = *reinterpret_cast<float*>(bytes.data() + 0xAE0);
    const auto set_state = [&](u16 value, float time)
    {
        store16(bytes, 0xADC, value);
        std::memcpy(bytes.data() + 0xAE0, &time, 4);
    };
    switch (i16(read_u16(bytes, 0xADC)))
    {
    case 0:
        if (read_u32(bytes, 0xAD4) != 0 && runtime_prompt_gate(weapon.frame)) // 0x00117560
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
        bytes[0xADE] = u8(i32(clamped)); // 0x001673D8.
        if (runtime_frame_timer_elapsed(fade, actor, weapon.frame)) // 0x00042360
        {
            store16(bytes, 0xADC, 2);
        }
        break;
    }
    case 2:
    {
        bytes[0xADE] = 0xC0;
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
            if (!(gap > 55.L) && read_u32(bytes, 0xAD4) != 0 && runtime_prompt_gate(weapon.frame)) // DAT_0032B528.
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
        bytes[0xADE] = u8(i32(clamped)); // 0x001673D8.
        if (runtime_frame_timer_elapsed(fade, actor, weapon.frame)) // 0x00042360
        {
            store16(bytes, 0xADC, 0);
            bytes[0xADE] = 0;
        }
        break;
    }
    default:
        break;
    }
    bytes[0xADF] = bytes[0xADE] != 0 ? 1 : 0;
    store32(bytes, 0xAD4, 0);
}

// Xbox 0x0014A770, EAX actor
void update_runtime_weapon_voice(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    if (bytes[0xD08] == 0)
    {
        return;
    }
    AudioEventVoices& voices = weapon.services.voices;
    if (read_u16(bytes, 0xD88) == 0xFFFF)
    {
        // The carried sound (+0xD08) starts at the weapon.
        const i16 voice = allocate_audio_event_voice(1, voices); // 0x00039830, EBX 1
        store16(bytes, 0xD88, u16(voice));
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
        prepare_audio_event_voice(voice, reinterpret_cast<const char*>(bytes.data() + 0xD08), voices); // 0x00039A20
        return;
    }
    // The playing sound follows the weapon.
    const Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
    const i16 voice = i16(read_u16(bytes, 0xD88));
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
    const float time = rate * step + read_f32(weapon.bytes, 0xC14);
    std::memcpy(weapon.bytes.data() + 0xC14, &time, 4);
    update_state(weapon, read_u16(weapon.bytes, 0xC0C)); // Vtable +0x44.
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
    late_update_state(weapon, read_u16(weapon.bytes, 0xC0C)); // Vtable +0x48.
    update_runtime_weapon_bounds(weapon); // 0x00145A40
    update_runtime_weapon_markers(weapon); // 0x00144E30
    if (Actor* holder = weapon_holder(actor))
    {
        push_runtime_weapon_holder(weapon, *holder); // 0x00147B30
    }
    const u16 pending = read_u16(weapon.bytes, 0xC0E);
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
    if (read_u32(weapon.bytes_at(weapon.services.actor_address(*holder)), 0xCE0) == 0)
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
    if (pass != 2 || weapon.bytes[0xADF] == 0)
    {
        return;
    }
    const Actor& actor = weapon.actor;
    const ModelPoseBuffer* pose = weapon_shape_pose(actor);
    const float frames = float(i32(weapon.hand_texture[0x11]));
    const float level = read_f32(weapon.bytes, 0xAB0) * std::bit_cast<float>(0x3FB6DB6Eu) * (0.f - frames) + frames; // DAT_0032B2E0.
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
    const u32 color = (u32(weapon.bytes[0xADE]) << 24) | 0xFFFFFFu;
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

// Xbox 0x00145C80
bool initialize_runtime_weapon_actor(RuntimeWeaponActor& weapon, std::span<const u8> parameters)
{
    auto bytes = weapon.bytes;
    Actor& actor = weapon.actor;
    store8(bytes, 0x93C, 1);
    actor.mask_flag = i32((u32(actor.mask_flag) & ~0xFFu) | 1u);
    store8(bytes, 0x93F, parameters[0x336]);
    store8(bytes, 0x93D, parameters[0x337]);
    actor.mask_flag = i32(read_u32(bytes, 0x93C)); // Native alias of the +0x93C dword (0x001460C0 reads +0x93F).
    store32(bytes, 0xDA0, read_u32(parameters, 0x460));
    store32(bytes, 0xDA4, read_u32(parameters, 0x464));
    store8(bytes, 0xDAC, parameters[0x338]);
    store8(bytes, 0xDAD, parameters[0x339]);
    store8(bytes, 0xDAE, parameters[0x33A]);
    store32(bytes, 0xDA8, read_u32(parameters, 0x46C));
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
    store32(bytes, 0xC18, read_u32(parameters, 0x574));
    initialize_runtime_weapon_names(weapon, parameters); // 0x00145150
    collect_runtime_weapon_grab_shapes(weapon); // 0x00146D10
    collect_runtime_weapon_flagged_markers(weapon); // 0x00147E10
    pick_runtime_weapon_variant(weapon, parameters); // 0x00147F00
    store16(bytes, 0xC10, read_u16(parameters, 0x334));
    store32(bytes, 0xAAC, 1);
    store32(bytes, 0xD90, 0);
    store32(bytes, 0xD94, 0);
    store32(bytes, 0xD98, 0);
    store8(bytes, 0xDAF, 0);
    store16(bytes, 0xD88, 0xFFFF);
    const u32 name = read_u32(parameters, 0x33C);
    if (name != 0)
    {
        const auto text = weapon.executable(name, 0);
        if (std::memchr(text.data(), 0, text.size()) == nullptr)
        {
            throw Error("weapon parameter +0x33C name has no terminator");
        }
        copy_string(bytes, 0xDB0, 0x20, reinterpret_cast<const char*>(text.data()));
    }
    copy_string(bytes, 0xDD0, 0x80, parameter_string(parameters, 0xA8, 0x80));
    initialize_actor_model_matrices(actor, weapon.matrices, weapon.model_types,
        weapon.matrix_bindings(actor)); // 0x000400D0
    if (read_u32(parameters, 0x468) != 0)
    {
        block_runtime_weapon_spawn(weapon); // 0x00147860
    }
    if (parameters[0x578] != 0)
    {
        copy_string(bytes, 0xD08, 0x80, parameter_string(parameters, 0x578, 0x80));
    }
    actor.shape_mode = 4; // +0x120.
    const u16 state = read_u16(bytes, 0xC10);
    store16(bytes, 0xC0E, 0);
    leave_state(weapon, read_u16(bytes, 0xC0C), state); // Vtable +0x40.
    const u16 previous = read_u16(bytes, 0xC0C);
    store16(bytes, 0xC0C, state);
    store32(bytes, 0xC14, 0);
    enter_state(weapon, state, previous); // Vtable +0x3C.
    const u32 key = read_u32(parameters, 0x470);
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
    store32(bytes, 0x99C, 0x000CF5F0); // The chair's grab test, called through +0x99C.
    store32(bytes, 0xE54, read_u32(parameters, 0x5F8));
    store32(bytes, 0xE58, read_u32(parameters, 0x5FC));
    if (read_u32(parameters, 0x600) != 0)
    {
        store8(bytes, 0x942, 1);
    }
    if (read_u32(bytes, 0xE58) == 0)
    {
        store8(bytes, 0x941, 1);
    }
    copy_string(bytes, 0xE70, 0x80, parameter_string(parameters, 0x604, 0x80));
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
        if (read_u32(weapon.bytes, 0xDA8) == 0)
        {
            attach_runtime_weapon_volume(weapon); // 0x00146310
        }
        if (read_u32(weapon.bytes, 0xE70) == kAidPrefix)
        {
            store32(weapon.bytes, 0xE6C, weapon.create_object(reinterpret_cast<const char*>(
                weapon.bytes.data() + 0xE70), {})); // 0x0003DF40
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
            if (read_u32(bytes, 0xDD0) == kAidPrefix && controller != nullptr)
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
        if (read_u32(bytes, 0xDD0) == kAidPrefix)
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
        if (read_u32(bytes, 0xE6C) != 0)
        {
            release_runtime_actor_frame_object(weapon, read_u32(bytes, 0xE6C)); // 0x0003DE30
            store32(bytes, 0xE6C, 0);
        }
    }
    if (weapon.cells != nullptr)
    {
        entity_detail::release_navigation_cell_registration(weapon.cells, read_u32(bytes, 0xD00)); // 0x001108B0
    }
    weapon.actor.model_mode &= ~4;
    register_actor_shape_probe(weapon.actor, weapon.view.shape); // 0x0003C770
}
}
