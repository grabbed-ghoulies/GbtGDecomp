#include "weapon_motion.h"
#include <cstddef>
#include "body_fields.h"
#include "mind_fields.h"
#include "weapon_actor_detail.h"
#include "actor_shape_bounds.h"
#include "actor_transform.h"
#include "entity.h"
#include "entity_collision.h"
#include "entity_heap.h"
#include "segment_records.h"
#include "../navigation/floor_tracking.h"
#include "../navigation/navigation.h"
#include "../navigation/path_routes.h"
#include "../camera/camera.h"
#include "../camera/entity_camera.h"
#include "../scene/ambient.h"
#include "actor_clock.h"
#include "actor_render_registration.h"
#include "body_damage_motion.h"
#include "player_mind_targets.h"
#include "gbtg/sound_runtime.h"
#include <string>

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
        weapon_base(bytes).next_state = 0x10;
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
    weapon_base(bytes).motion_euler = weapon.default_euler; // DAT_0050DD74.
    weapon_base(bytes).gravity = 0.f;
    weapon_base(bytes).probes_contacts = 1;
    weapon_base(bytes).slides = 0;
    weapon_base(bytes).quiet = 1;
    weapon_base(bytes).contact_mask = 0x000C4000u;
    weapon_base(bytes).hit = 0;
}

// Xbox 0x001488D0
void update_runtime_weapon_flight(RuntimeWeaponActor& weapon)
{
    Actor& actor = weapon.actor;
    auto bytes = weapon.bytes;
    Vec3 velocity = actor.anchor; // +0x3C.
    if (weapon_base(bytes).hit != 0)
    {
        std::array<u32, 0x42> message{};
        message[0] = 0x93;
        entity_copy_match_block(message.data() + 1, weapon_base(bytes).contact_match.data()); // 0x000FCD70
        // Word 0x16 is the address of the stack copy of +0x3C, the velocity; a thrown projectile's landing
        // (0x000CC340) passes it on to what it spawns (a spider web's 0x7A).
        message[0x16] = weapon.services.bind_address(&velocity, 12);
        weapon.message(message);
        weapon.services.unbind_address(&velocity);
        return;
    }
    const float dt = frame_rate(actor) * frame_step(weapon.frame);
    velocity.y = weapon_base(bytes).gravity * dt + velocity.y;
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
    const u32 mask = weapon_base(bytes).contact_mask;
    if (weapon_base(bytes).probes_contacts != 0 &&
        entity_collect_sphere_contacts(position, radius, 0xC, mask) > 0) // 0x001227E0
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
        if (!hit || weapon_base(bytes).slides == 0)
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
    if (hit && weapon_base(bytes).slides != 0 && pass == 3)
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
    weapon_base(bytes).probes_contacts = 0;
    if (hit)
    {
        weapon_base(bytes).hit = 1;
        entity_copy_match_block(weapon_base(bytes).contact_match.data(), entity_detail::g_segment_slots[0].pool.begin); // 0x000FCD70
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
    if (weapon_base(weapon.bytes).quiet == 0 && weapon.markers != nullptr)
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
    weapon_base(bytes).slack = 0.f;
    weapon_base(bytes).damping = 0.f;
    weapon_base(bytes).rise = 0.f;
    weapon_base(bytes).ghost = 0;
    weapon_base(bytes).starting = 1;
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
    const entity_detail::NavGrid* grid = entity_detail::g_nav_grids[weapon_base(bytes).floor]; // DAT_0050E240.
    if (grid != nullptr)
    {
        if (weapon.navigation_nodes == nullptr)
        {
            throw Error("weapon state 10 (0x001495C0) needs the navigation nodes DAT_0050E24C, whose loader is not native");
        }
        const u32 count = u32(grid->depth * grid->width);
        auto* search = static_cast<entity_detail::NavigationPathSearch*>(engine_heap_allocate(
            sizeof(entity_detail::NavigationPathSearch))); // 0x000F72D0 with 0x34: the host search holds 64-bit pointers
        if (search == nullptr)
        {
            weapon.path_search = nullptr; // +0xD04.
            return;
        }
        std::memset(search, 0, sizeof(entity_detail::NavigationPathSearch));
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
    start_hop_block(weapon, offsetof(WeaponBase, hop), target);
}

// Xbox 0x00149580, EAX actor, EBX target
void start_runtime_weapon_drop(RuntimeWeaponActor& weapon, const Vec3& target)
{
    start_hop_block(weapon, offsetof(WeaponBase, drop), target);
}

// Xbox 0x00148F20, EBX actor
void update_runtime_weapon_hop(RuntimeWeaponActor& weapon)
{
    update_hop_block(weapon, offsetof(WeaponBase, hop));
}

// Xbox 0x00149270, EBX actor
void update_runtime_weapon_drop(RuntimeWeaponActor& weapon)
{
    update_hop_block(weapon, offsetof(WeaponBase, drop));
}

// The head 0x00149750 and 0x000CD2A0 inline before their state change: +0x93C clears and the sound +0xD08 holds
// (its voice at +0xD88) stops.
void stop_runtime_weapon_hold_sound(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    weapon_base(bytes).mask = 0;
    if (weapon_base(bytes).voice_name.data()[0] == 0)
    {
        return;
    }
    weapon_base(bytes).voice_name.data()[0] = 0;
    if (weapon_base(bytes).voice == 0xFFFF)
    {
        return;
    }
    stop_actor_sound_voice(i16(weapon_base(bytes).voice),
                           weapon.services.sounds); // 0x00039690 on DAT_004E0CE8[voice]
    weapon_base(bytes).voice = 0xFFFF;
}

// Xbox 0x00149C30, EAX actor
RuntimeMindTarget* runtime_weapon_holder_target(RuntimeWeaponActor& weapon)
{
    const Actor* holder = weapon.actor.spawn_owner; // +0xD8C.
    if (holder == nullptr)
    {
        throw Error("0x00149C30 reads the mind of a weapon with no holder (+0xD8C)");
    }
    const u32 mind = read_u32(weapon.bytes_at(weapon.services.actor_address(*holder)), 0x958);
    if (mind == 0)
    {
        return nullptr;
    }
    const u32 record = get(weapon.bytes_at(mind), mind_field::target_record);
    return record == 0 ? nullptr : weapon.services.target_at(record);
}

namespace
{
// The weapon's follow target: the actor of its +0xE50 record (record +0x10).
Actor& follow_target(RuntimeWeaponActor& weapon, const char* reader)
{
    RuntimeMindTarget* record = weapon.services.target_at(weapon_base(weapon.bytes).target_record);
    if (record == nullptr)
    {
        throw Error(std::string(reader) + " reads a weapon follow record (+0xE50) with no actor");
    }
    return record->actor;
}

// Joint 4 of the target's model at its rest point (0x00052FC0, then 0x0003AEB0 with EBX DAT_0050DD80).
Vec3 follow_target_joint(const Actor& target, const char* reader)
{
    if (target.model != nullptr && target.model->cpu.data() != nullptr)
    {
        const u32 joint = actor_lookup_joint_index(target.model->cpu, 4) & 0xFFu; // 0x00052FC0
        if (joint != 0xFFu && joint != 0xFFFFFFFFu)
        {
            Vec3 point{};
            actor_transform_joint_point(target, u16(joint), actor_transform_origin, point); // 0x0003AEB0
            return point;
        }
    }
    // Retail reads its uninitialised joint local here.
    throw Error(std::string(reader) + " reads joint 4 of a follow target whose model has none");
}

// The +2 height (bottom) of a floor cell (DAT_0050E240[floor] +0x10, 0x12-byte cells).
float cell_height(i32 floor, i32 cell)
{
    return float(entity_detail::g_nav_grids[floor]->cells[cell].bottom);
}

// Xbox 0x00103330, EAX point, ECX normal, EDX kept, ESI start, the stack the probe's start: the point moves out
// along the normal by how far the probe held it back (at least DAT_0032B250).
void push_runtime_weapon_off_contact(Vec3& point, const Vec3& normal, const Vec3& kept, const Vec3& start,
    const Vec3& probe_start)
{
    const float depth = 0.f - (normal.z * ((point.z - probe_start.z) - (kept.z - start.z))
        + normal.y * ((point.y - probe_start.y) - (kept.y - start.y))
        + normal.x * ((point.x - probe_start.x) - (kept.x - start.x)));
    float push = 0.05f; // DAT_0032B250.
    if (!(depth < 0.05f))
    {
        if (!(0.f <= depth))
        {
            return;
        }
        push = depth;
    }
    point.x = normal.x * push + point.x;
    point.y = normal.y * push + point.y;
    point.z = normal.z * push + point.z;
}

// Xbox 0x00105B90, EDI current, ESI target, EBX output: each axis turns by rate x step (0x00100FE0).
Vec3 turn_runtime_euler_toward(const Vec3& current, const Vec3& target, float rate, float step)
{
    const float turn = rate * step;
    return Vec3{step_runtime_damage_angle(current.x, target.x, turn),
        step_runtime_damage_angle(current.y, target.y, turn), step_runtime_damage_angle(current.z, target.z, turn)};
}
}

// Xbox 0x00149C50: the follow height, joint 4 of the target above its feet, over the cell under the weapon.
float runtime_weapon_follow_height(RuntimeWeaponActor& weapon)
{
    if (weapon_base(weapon.bytes).target_record == 0)
    {
        return 0.f;
    }
    const Actor& target = follow_target(weapon, "0x00149C50");
    const Vec3 joint = follow_target_joint(target, "0x00149C50");
    const Vec3 target_position = actor_world_position(target); // 0x000412A0
    const Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
    const i32 floor = entity_detail::navigation_floor_index(position, -1, 0); // 0x00110BF0
    const i32 cell = entity_detail::navigation_cell_index(floor, position); // 0x00110EF0
    if (cell == -1)
    {
        // Retail reads the stack word that holds this weapon's address as the height.
        throw Error("0x00149C50 reads a follow height off the floor grid");
    }
    return (joint.y - target_position.y) + cell_height(floor, cell);
}

// Xbox 0x00149D20: the velocity toward the follow target along the floor grid's path search (+0xD04), scaled by
// +0xCF0, rising or falling by +0xCF4 to hold +0xCEC about the follow height.
bool steer_runtime_weapon_follow(RuntimeWeaponActor& weapon, Vec3& velocity)
{
    auto bytes = weapon.bytes;
    velocity = Vec3{};
    Actor* holder = weapon.actor.spawn_owner; // +0xD8C.
    entity_detail::NavigationPathSearch* search = weapon.path_search; // +0xD04.
    if (search == nullptr || holder == nullptr || weapon_base(bytes).target_record == 0)
    {
        return false;
    }
    const Actor& target = follow_target(weapon, "0x00149D20");
    const Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
    Vec3 target_position = actor_world_position(target); // 0x000412A0
    Vec3 goal = follow_target_joint(target, "0x00149D20");
    goal.y = runtime_weapon_follow_height(weapon); // 0x00149C50
    const i32 target_floor = entity_detail::navigation_floor_index(target_position, -1, 0); // 0x00110BF0
    update_runtime_weapon_cells(weapon, target_floor); // 0x00145C30
    const i32 floor = i32(weapon_base(bytes).floor);
    const i32 cell = entity_detail::navigation_cell_index(floor, position); // 0x00110EF0
    i32 target_cell = entity_detail::navigation_cell_index(target_floor, goal); // 0x00110EF0
    if (cell == -1 || target_cell == -1)
    {
        return false;
    }
    if (floor != target_floor)
    {
        if (entity_detail::navigation_floor_connection_point(floor, target_floor, goal, target_position) == 0)
        {
            return false; // 0x00110D00, EBX the weapon's floor
        }
        goal.x = target_position.x;
        goal.z = target_position.z;
        target_cell = entity_detail::navigation_cell_index(target_floor, target_position); // 0x00110EF0
    }
    GhoulySpawnPool* route = static_cast<GhoulySpawnPool*>(search->scratch); // +0x14.
    if (route != nullptr && cell == search->last && target_cell == search->first
        && entity_detail::navigation_path_search_route_is_open(*search) != 0) // 0x00036E90
    {
        entity_detail::set_navigation_route_endpoints(*route, position, goal); // 0x000376F0, EBX start, EDI finish
    }
    else
    {
        if (entity_detail::navigation_cell_has_no_open_neighbor(*search->grid, u32(cell), 0)) // 0x00036600
        {
            Vec3 open{};
            if (entity_detail::navigation_nearest_open_cell(u32(cell), floor, &open) == -1) // 0x00111660
            {
                return false;
            }
            velocity = Vec3{open.x - position.x, open.y - position.y, open.z - position.z};
            return true;
        }
        const u32 holder_address = weapon.services.actor_address(*holder);
        entity_detail::NavigationCellRegistration* held = *weapon.services.player_cells(holder_address); // +0xCE8.
        const u32 held_grid = read_u32(weapon.bytes_at(holder_address), 0xCEC);
        if (held != nullptr)
        {
            entity_detail::decrement_navigation_cell_counts(*held, held_grid); // 0x00110470
        }
        entity_detail::begin_navigation_path_search(*search, cell, target_cell); // 0x00036C80
        const float elapsed = runtime_actor_step(weapon.actor, weapon.clock); // 0x00042320
        const u32 mask = body_record(weapon.services.body_of(holder_address)).scaled_surfaces; // Holder body +0x878.
        entity_detail::update_navigation_path_search(*search, mask, elapsed); // 0x00036CA0
        if (held != nullptr)
        {
            entity_detail::increment_navigation_cell_counts(*held, held_grid); // 0x00110400
        }
        route = nullptr;
        if (entity_detail::navigation_path_search_has_result(*search) != 0) // 0x00036C60
        {
            route = entity_detail::take_navigation_search_route(*search, position, goal); // 0x00036E50
        }
    }
    if (route != nullptr)
    {
        const float reach = float(search->grid->cell) * 0.5f; // Grid +8, DAT_0032B0F4.
        entity_detail::navigation_route_direction(*route, velocity, &target_position, reach); // 0x00037D50
        velocity.x = weapon_base(bytes).damping * velocity.x;
        velocity.z = weapon_base(bytes).damping * velocity.z;
    }
    const float height = goal.y;
    const float slack = weapon_base(bytes).slack;
    const float rise = weapon_base(bytes).rise;
    const float floor_height = cell_height(floor, cell);
    const float target_height = cell_height(target_floor, target_cell);
    if (height - slack > position.y || height - target_height > position.y - floor_height)
    {
        velocity.y = rise;
        return true;
    }
    if (position.y > slack + height)
    {
        velocity.y = 0.f - rise;
    }
    return true;
}

// Xbox 0x00149750: weapon state 10, led along the floor grid toward the follow target. A weapon with no holder, or
// none to steer by, breaks (state 9).
void update_runtime_weapon_follow(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    Actor& actor = weapon.actor;
    Actor* holder = actor.spawn_owner; // +0xD8C.
    if (holder == nullptr)
    {
        stop_runtime_weapon_hold_sound(weapon);
        change_runtime_weapon_state(weapon, 9); // The inlined 0x0014A290.
        return;
    }
    const float step = frame_rate(actor) * frame_step(weapon.frame);
    Vec3 velocity{};
    if (!steer_runtime_weapon_follow(weapon, velocity)) // 0x00149D20
    {
        stop_runtime_weapon_hold_sound(weapon);
        change_runtime_weapon_state(weapon, 9); // 0x0014A290, EDI 9
        return;
    }
    Vec3 position = actor_world_position(actor); // 0x000412A0
    const Vec3 delta{velocity.x * step, velocity.y * step, velocity.z * step};
    Vec3 next{position.x + delta.x, position.y + delta.y, position.z + delta.z};
    // The holder keeps no +0x1F0 flags during the probes.
    const u32 flags = holder->model_flags;
    holder->model_flags = 0;
    const float radius = float(actor_model_sweep_radius(actor)); // 0x0003D9A0, EDI actor
    if (weapon_base(bytes).starting != 0)
    {
        // The first step out of anything the weapon starts inside, unless it ignores the world (+0xCF8).
        if (weapon_base(bytes).ghost == 0 &&
            entity_collect_sphere_contacts(position, radius, 0xC, 0x000C4000u) > 0)
        {
            auto contacts = entity_merge_contact_records(); // 0x00122310
            Vec3 separated{};
            separate_runtime_segment_contacts(contacts, position, radius, separated, actor_transform_origin); // 0x000FC3F0
            engine_heap_free(contacts.block); // 0x000663E0
            position = separated;
            actor_set_world_position(actor, position); // 0x00041730
            next = Vec3{delta.x + position.x, delta.y + position.y, delta.z + position.z};
        }
        weapon_base(bytes).starting = 0;
        actor.mask_mode = 0; // Native alias of the +0xCDC word.
    }
    if (weapon_base(bytes).ghost == 0)
    {
        Vec3 kept = next;
        if (entity_record_movement_probe(kept, position, std::bit_cast<u32>(radius), 0xC, 0x000C4000u) == 1) // 0x001226C0
        {
            const std::span<const u8> hit{entity_detail::g_segment_slots[0].pool.begin, 0x54}; // DAT_00511A20 +4.
            const Vec3 normal{read_f32(hit, 0xC), read_f32(hit, 0x10), read_f32(hit, 0x14)};
            if (0.9f > normal.y) // DAT_0032B174.
            {
                push_runtime_weapon_off_contact(next, normal, kept, position, position); // 0x00103330
            }
            else
            {
                next = kept;
            }
        }
        else
        {
            next = kept;
        }
    }
    holder->model_flags = flags;
    float yaw = 0.f;
    float pitch = 0.f;
    if (direction_to_yaw_pitch(velocity.x, velocity.y, velocity.z, yaw, pitch) != 0) // 0x00105E20, ECX +0xCE4
    {
        weapon_base(bytes).aim = Vec3{pitch, yaw, 0.f};
    }
    else
    {
        weapon_base(bytes).aim = Vec3{0.f, 0.f, 0.f};
    }
    const Vec3 aim = weapon_base(bytes).aim;
    const Vec3 euler = turn_runtime_euler_toward(actor_world_euler(actor), aim, std::bit_cast<float>(0x40490FDBu),
        step); // 0x00041820, then 0x00105B90 at pi a second
    normalize_euler_angles(actor.euler, euler); // 0x00105AA0, EDI actor +0x14
    actor.xform_valid = 0; // +0x2A8.
    if (g_amb_volume_enabled != 0 // DAT_0050EA00..DAT_0050EA18.
        && (g_amb_color_lo[0] > next.x || next.x >= g_amb_color_hi[0] || g_amb_color_lo[1] > next.y
            || next.y >= g_amb_color_hi[1] || g_amb_color_lo[2] > next.z || next.z >= g_amb_color_hi[2]))
    {
        activate_actor_bind_tree(actor); // 0x00112CC0, ESI actor
    }
    const bool decay = weapon.clock.decay_enabled[weapon.clock.decay_index] != 0; // DAT_0030F144[DAT_0051248C].
    if (scene_detail::g_pause_references[1] == 0 && decay && scene_detail::g_pause_references[0] == 0 && decay)
    {
        actor.anchor = velocity; // +0x3C.
        actor_set_world_position(actor, next); // 0x00041730
        return;
    }
    actor.anchor = Vec3{}; // +0x3C.
}
}
