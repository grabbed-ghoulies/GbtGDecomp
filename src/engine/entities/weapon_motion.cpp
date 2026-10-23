#include "weapon_motion.h"
#include "weapon_actor_detail.h"
#include "actor_shape_bounds.h"
#include "actor_transform.h"
#include "entity.h"
#include "entity_collision.h"
#include "entity_heap.h"
#include "segment_records.h"
#include "../navigation/floor_tracking.h"

namespace gbtg
{
using namespace weapon_detail;

namespace
{
// The weapon's +0x8C callout records for an id (0x0003BE20 with the actor's address, as 0x0003C040 sends it).
void send_weapon_callout(RuntimeWeaponActor& weapon, i32 id)
{
    dispatch_actor_resource_callout(weapon.view.startup.resource, id, nullptr, weapon.base().callouts);
}

// 0x00148F20 / 0x00149270 over their blocks: start +0, target +0xC, timer +0x18, bounces +0x1C.
void update_hop_block(RuntimeWeaponActor& weapon, size_t block)
{
    Actor& actor = weapon.actor;
    auto bytes = weapon.bytes;
    const float step = frame_step(weapon.frame);
    const float rate = frame_rate(actor);
    if (read_u32(bytes, block + 0x1C) == 0)
    {
        bool done = false;
        const float timer = read_f32(bytes, block + 0x18);
        if (timer > 0.f)
        {
            const float remaining = timer - rate * step;
            std::memcpy(bytes.data() + block + 0x18, &remaining, 4);
            if (!(0.f < remaining))
            {
                const float zero = 0.f;
                std::memcpy(bytes.data() + block + 0x18, &zero, 4);
                done = true;
            }
        }
        const float progress = 1.f - read_f32(bytes, block + 0x18) * std::bit_cast<float>(0x3FAAAAABu); // DAT_0032B3C0.
        const float start_x = read_f32(bytes, block);
        const float start_y = read_f32(bytes, block + 4);
        const float start_z = read_f32(bytes, block + 8);
        Vec3 position{(read_f32(bytes, block + 0xC) - start_x) * progress + start_x,
            (read_f32(bytes, block + 0x10) - start_y) * progress + start_y,
            (read_f32(bytes, block + 0x14) - start_z) * progress + start_z};
        const float angle = progress * std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
        const float lift = float(retail_sin(angle)); // 0x00153320
        static_cast<void>(retail_cos(angle)); // 0x00153230, its result dropped.
        position.y = lift * 8.f + position.y; // DAT_0032B26C.
        actor_set_world_position(actor, position); // 0x00041730
        if (done)
        {
            send_weapon_callout(weapon, 0xC); // 0x0003BE20
            actor.anchor = Vec3{0.f, 15.f, 0.f}; // DAT_0032B214.
            store32(bytes, block + 0x1C, read_u32(bytes, block + 0x1C) + 1);
        }
        return;
    }
    bool done = false;
    const float dt = rate * step;
    Vec3 velocity{actor.anchor.x, actor.anchor.y - dt * 50.f, actor.anchor.z}; // DAT_0032B1E0.
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    Vec3 next{velocity.x * dt + position.x, velocity.y * dt + position.y, velocity.z * dt + position.z};
    const float floor = read_f32(bytes, block + 0x10);
    if (floor > next.y && 0.f > velocity.y)
    {
        const u32 bounces = read_u32(bytes, block + 0x1C) + 1;
        next.y = floor;
        store32(bytes, block + 0x1C, bounces);
        if (i32(bounces) >= 3)
        {
            velocity = Vec3{0.f, 0.f, 0.f};
            done = true;
        }
        else
        {
            velocity.y = velocity.y * std::bit_cast<float>(0xBECCCCCDu); // DAT_0032B658.
        }
    }
    actor.anchor = velocity;
    actor_set_world_position(actor, next); // 0x00041730
    if (done)
    {
        store16(bytes, 0xC0E, 0x10);
    }
}

// 0x00149230 / 0x00149580 over their blocks.
void start_hop_block(RuntimeWeaponActor& weapon, size_t block, const Vec3& target)
{
    const Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
    std::memcpy(weapon.bytes.data() + block, &position, 12);
    std::memcpy(weapon.bytes.data() + block + 0xC, &target, 12);
    store32(weapon.bytes, block + 0x1C, 0);
    const float timer = 0.75f; // DAT_0032B2AC.
    std::memcpy(weapon.bytes.data() + block + 0x18, &timer, 4);
}
}

// Xbox 0x001487C0, EAX actor
void enter_runtime_weapon_flight(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    weapon.actor.anchor = actor_transform_origin; // DAT_0050DD80.
    std::memcpy(bytes.data() + 0xC34, &weapon.default_euler, 12); // DAT_0050DD74.
    store32(bytes, 0xC30, 0);
    store32(bytes, 0xC24, 1);
    store32(bytes, 0xC28, 0);
    store32(bytes, 0xC2C, 1);
    store32(bytes, 0xC40, 0x000C4000u);
    store32(bytes, 0xC44, 0);
}

// Xbox 0x001488D0
void update_runtime_weapon_flight(RuntimeWeaponActor& weapon)
{
    Actor& actor = weapon.actor;
    auto bytes = weapon.bytes;
    Vec3 velocity = actor.anchor; // +0x3C.
    if (read_u32(bytes, 0xC44) != 0)
    {
        std::array<u32, 0x42> message{};
        message[0] = 0x93;
        entity_copy_match_block(message.data() + 1, bytes.data() + 0xC48); // 0x000FCD70
        // Word 0x16 is the address of the stack copy of +0x3C, the velocity; a thrown projectile's landing
        // (0x000CC340) passes it on to what it spawns (a spider web's 0x7A).
        message[0x16] = weapon.services.bind_address(&velocity, 12);
        weapon.message(message);
        weapon.services.unbind_address(&velocity);
        return;
    }
    const float dt = frame_rate(actor) * frame_step(weapon.frame);
    velocity.y = read_f32(bytes, 0xC30) * dt + velocity.y;
    Vec3 position = actor_world_position(actor); // 0x000412A0
    Vec3 target{position.x + velocity.x * dt, position.y + velocity.y * dt, position.z + velocity.z * dt};
    Actor* owner = actor.spawn_owner; // +0xD8C keeps no collision filter during the sweep.
    u32 filter = 0;
    if (owner != nullptr)
    {
        filter = owner->model_flags;
        owner->model_flags = 0;
    }
    const float radius = float(actor_model_sweep_radius(actor)); // 0x0003D9A0
    const u32 mask = read_u32(bytes, 0xC40);
    if (read_u32(bytes, 0xC24) != 0 && entity_collect_sphere_contacts(position, radius, 0xC, mask) > 0) // 0x001227E0
    {
        auto contacts = entity_merge_contact_records(); // 0x00122310
        Vec3 separated;
        separate_runtime_segment_contacts(contacts, position, radius, separated, actor_transform_origin); // 0x000FC3F0
        engine_heap_free(contacts.block); // 0x000663E0
        position = separated;
        actor_set_world_position(actor, position); // 0x00041730
        target = Vec3{velocity.x * dt + position.x, velocity.y * dt + position.y, velocity.z * dt + position.z};
    }
    bool hit = false;
    i32 pass = 0;
    while (true)
    {
        Vec3 start = target;
        const float length = (velocity.x * velocity.x + velocity.z * velocity.z) + velocity.y * velocity.y;
        Vec3 offset = velocity;
        if (length != 0.f)
        {
            const long double scale = static_cast<long double>(engine_reciprocal_sqrt(length)) * radius; // 0x001534C0
            offset = Vec3{float(velocity.x * scale), float(velocity.y * scale), float(velocity.z * scale)};
        }
        start = Vec3{offset.x + start.x, offset.y + start.y, offset.z + start.z};
        hit = entity_record_sweep_probe(start, position, 0xC, mask) == 1; // 0x00122480
        if (!hit || read_u32(bytes, 0xC28) == 0)
        {
            target = start;
            break;
        }
        const u8* record = entity_detail::g_segment_slots[0].pool.begin; // DAT_00511A20 + 4.
        const float normal_x = read_f32({record, 0x54}, 0xC);
        const float normal_y = read_f32({record, 0x54}, 0x10);
        const float normal_z = read_f32({record, 0x54}, 0x14);
        if (!(std::bit_cast<float>(0x3F666666u) > normal_y)) // DAT_0032B174.
        {
            target = start;
            break;
        }
        const float along_x = (target.x - position.x) - (start.x - position.x);
        const float along_y = (target.y - position.y) - (start.y - position.y);
        const float along_z = (target.z - position.z) - (start.z - position.z);
        float push = 0.f - ((along_z * normal_z + along_x * normal_x) + along_y * normal_y);
        constexpr float minimum = std::bit_cast<float>(0x3D4CCCCDu); // DAT_0032B250.
        bool apply = true;
        if (minimum > push)
        {
            push = minimum;
        }
        else if (0.f > push)
        {
            apply = false;
        }
        if (apply)
        {
            target.x = push * normal_x + target.x;
            target.y = push * normal_y + target.y;
            target.z = normal_z * push + target.z;
        }
        ++pass;
        if (pass >= 3)
        {
            break;
        }
    }
    if (hit && read_u32(bytes, 0xC28) != 0 && pass == 3)
    {
        target = position;
        velocity.z = 0.f;
        velocity.x = 0.f;
        hit = false;
    }
    if (owner != nullptr)
    {
        owner->model_flags = filter;
    }
    actor_set_world_position(actor, target); // 0x00041730
    store32(bytes, 0xC24, 0);
    if (hit)
    {
        store32(bytes, 0xC44, 1);
        entity_copy_match_block(bytes.data() + 0xC48, entity_detail::g_segment_slots[0].pool.begin); // 0x000FCD70
        if (weapon.markers != nullptr)
        {
            for (u32 index = 0; index < weapon.markers->count; ++index)
            {
                const float scale = 2.f; // DAT_0032B110.
                std::memcpy(weapon.markers->records[index].bytes.data() + 0x18, &scale, 4);
            }
        }
    }
    actor.anchor = velocity;
}

// Xbox 0x00148850, ESI actor
void leave_runtime_weapon_flight(RuntimeWeaponActor& weapon)
{
    if (read_u32(weapon.bytes, 0xC2C) == 0 && weapon.markers != nullptr)
    {
        deactivate_all_runtime_body_contact_shapes(*weapon.markers); // 0x0013F660
        std::array<u32, 0x42> message{};
        message[0] = 0x92;
        message[1] = 0;
        weapon.message(message); // Vtable +0xC.
    }
}

// Xbox 0x001495C0
void enter_runtime_weapon_follow(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    store32(bytes, 0xCEC, 0);
    store32(bytes, 0xCF0, 0);
    store32(bytes, 0xCF4, 0);
    store32(bytes, 0xCF8, 0);
    store32(bytes, 0xCDC, 1);
    weapon.actor.mask_mode = 1; // Native alias of the +0xCDC word.
    weapon.actor.anchor = actor_transform_origin; // DAT_0050DD80.
    if (weapon.markers != nullptr)
    {
        activate_all_runtime_body_contact_shapes(weapon.actor, *weapon.markers, 0xFF); // 0x0013F5B0
        std::array<u32, 0x42> message{};
        message[0] = 0x92;
        message[1] = 1;
        weapon.message(message); // Vtable +0xC.
    }
    const entity_detail::NavGrid* grid = entity_detail::g_nav_grids[read_u32(bytes, 0xD00)]; // DAT_0050E240.
    if (grid != nullptr)
    {
        if (weapon.navigation_nodes == nullptr)
        {
            throw Error("weapon state 10 (0x001495C0) needs the navigation nodes DAT_0050E24C, whose loader is not native");
        }
        const u32 count = u32(grid->depth * grid->width);
        auto* search = static_cast<entity_detail::NavigationPathSearch*>(engine_heap_allocate(0x34)); // 0x000F72D0
        if (search == nullptr)
        {
            weapon.path_search = nullptr; // +0xD04.
            return;
        }
        std::memset(search, 0, 0x34);
        entity_detail::reset_navigation_path_search(*search, weapon.navigation_nodes, grid, count); // 0x00036AF0
        weapon.path_search = search;
    }
}

// Xbox 0x001496D0, ESI actor
void leave_runtime_weapon_follow(RuntimeWeaponActor& weapon)
{
    if (weapon.markers != nullptr)
    {
        deactivate_all_runtime_body_contact_shapes(*weapon.markers); // 0x0013F660
        std::array<u32, 0x42> message{};
        message[0] = 0x92;
        message[1] = 0;
        weapon.message(message); // Vtable +0xC.
    }
    if (weapon.path_search != nullptr)
    {
        // 0x00036B60: the search's scratch block, then the search itself.
        if (weapon.path_search->scratch != nullptr)
        {
            engine_heap_free(weapon.path_search->scratch);
            weapon.path_search->scratch = nullptr;
        }
        engine_heap_free(weapon.path_search);
        weapon.path_search = nullptr;
    }
}

// Xbox 0x00149230, EAX actor, EBX target
void start_runtime_weapon_hop(RuntimeWeaponActor& weapon, const Vec3& target)
{
    start_hop_block(weapon, 0xC9C, target);
}

// Xbox 0x00149580, EAX actor, EBX target
void start_runtime_weapon_drop(RuntimeWeaponActor& weapon, const Vec3& target)
{
    start_hop_block(weapon, 0xCBC, target);
}

// Xbox 0x00148F20, EBX actor
void update_runtime_weapon_hop(RuntimeWeaponActor& weapon)
{
    update_hop_block(weapon, 0xC9C);
}

// Xbox 0x00149270, EBX actor
void update_runtime_weapon_drop(RuntimeWeaponActor& weapon)
{
    update_hop_block(weapon, 0xCBC);
}
}
