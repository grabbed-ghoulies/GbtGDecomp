#include "water_pistol_weapon.h"
#include "actor_shape_bounds.h"
#include "entity.h"
#include "entity_collision.h"
#include "entity_heap.h"
#include "extinguisher_weapon.h"
#include "segment_records.h"
#include "weapon_actor_detail.h"
#include "../camera/entity_camera.h"
#include "../graphics/actor_effect_nodes.h"
#include "../graphics/actor_effect_stream.h"
#include "../graphics/vector_normalize.h"
#include "../graphics/world_sprite_passes.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/resource_error.h"
#include <bit>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
using namespace weapon_detail;

namespace
{
WaterPistolWeaponState& fields_of(std::span<u8> bytes)
{
    return actor_record_view<WaterPistolWeaponState>(bytes);
}

const WaterPistolWeaponState& fields_of(std::span<const u8> bytes)
{
    return actor_record_view<WaterPistolWeaponState>(bytes);
}

namespace parameter = water_pistol_weapon_parameter;

// The droplet block pools (retail +0xF34 and the twelve +0xF04 words), the texture (+0xF00), the muzzle effect node
// (+0xEFC) and the draw's point list (+0xF3C), by the weapon's bytes.
struct WaterPistolBlocks
{
    std::span<const u8> texture;
    ActorEffectNode* muzzle = nullptr;
    GhoulySpawnPool* active = nullptr;
    std::array<GhoulySpawnPool*, 12> trails{};
    GhoulySpawnPool* strip = nullptr;
};
std::unordered_map<const u8*, WaterPistolBlocks> water_pistol_blocks;

constexpr float kEmitInterval = 0.03333333507180214f; // DAT_0032B448, 1/30 s.

WaterPistolBlocks& blocks_of(const RuntimeWeaponActor& weapon)
{
    return water_pistol_blocks[weapon.bytes.data()];
}

// The +0xF34 / +0xF04 / +0xEFC / +0xF3C words follow the native views.
void sync_blocks(RuntimeWeaponActor& weapon)
{
    const WaterPistolBlocks& blocks = blocks_of(weapon);
    fields_of(weapon.bytes).active = blocks.active != nullptr ? 1u : 0u;
    for (u32 index = 0; index < 12; ++index)
    {
        fields_of(weapon.bytes).trails[index] = blocks.trails[index] != nullptr ? 1u : 0u;
    }
    fields_of(weapon.bytes).muzzle_effect = blocks.muzzle != nullptr ? 1u : 0u;
    fields_of(weapon.bytes).strip = blocks.strip != nullptr ? 1u : 0u;
}

float fire_word(const RuntimeWeaponActor& weapon)
{
    return std::bit_cast<float>(actor_record_view<GunWeaponState>(weapon.bytes).unk_EE8);
}

// The droplets' speed: 150 x +0xEE8 + 50 (DAT_0032B444, DAT_0032B1E0).
float droplet_speed(const RuntimeWeaponActor& weapon)
{
    return fire_word(weapon) * 150.f + 50.f;
}

i32 droplet_count(const GhoulySpawnPool& block)
{
    return i32(block.cursor - block.begin) / i32(block.stride);
}

RuntimeWaterDroplet* droplet_at(u8* address)
{
    return reinterpret_cast<RuntimeWaterDroplet*>(address);
}

const char* name_at(const RuntimeWeaponActor& weapon, size_t offset)
{
    return reinterpret_cast<const char*>(weapon.bytes.data() + offset);
}

// The block's next record, grown by its +2 count when full (the inlined 0x000662A0).
template <typename Record>
Record& append(GhoulySpawnPool*& pool)
{
    if (pool->cursor == pool->limit)
    {
        pool = grow_runtime_shape_pool(pool); // 0x000662A0
    }
    auto* record = reinterpret_cast<Record*>(pool->cursor);
    pool->cursor += pool->stride;
    return *record;
}

// The first record is removed and the rest slide down (0x00165AF0).
void remove_first(GhoulySpawnPool& block)
{
    u8* last = block.cursor - block.stride;
    if (block.begin < last)
    {
        std::memmove(block.begin, block.begin + block.stride, size_t(last - block.begin));
    }
    block.cursor -= block.stride;
}

// The effect named at `offset` at the point (g_actor_creation_options, flag 1; 4 adds the euler), through 0x00053170
// (or its inlined 0x00035E90 / 0x00053060 / 0x00035C00 walk): the node created last, or null.
ActorEffectNode* spawn_effect(RuntimeWeaponActor& weapon, size_t offset, const Vec3& point, const Vec3* euler)
{
    if (weapon.services.effects == nullptr)
    {
        throw Error("water pistol effect without the scene's effect stream context");
    }
    g_actor_creation_options.flags = 0;
    g_actor_creation_options.position = point;
    g_actor_creation_options.flags = 1;
    if (euler != nullptr)
    {
        g_actor_creation_options.euler = *euler;
        g_actor_creation_options.flags |= 4;
    }
    if (load_actor_effect_stream(name_at(weapon, offset), &g_actor_creation_options, *weapon.services.effects) == 0)
    {
        return nullptr;
    }
    const i32 slot = g_last_actor_effect;
    g_last_actor_effect = -1;
    if (i16(slot) == -1)
    {
        return nullptr;
    }
    return g_actor_effect_nodes.at(size_t(i16(slot)));
}

// A splash: the effect ends on its own (flag 8).
void splash(RuntimeWeaponActor& weapon, size_t offset, const Vec3& point)
{
    if (ActorEffectNode* effect = spawn_effect(weapon, offset, point, nullptr); effect != nullptr)
    {
        effect->bytes[0x18] |= 8;
    }
}

// The muzzle effect ends (flag 8, its +8 life -1, DAT_0032B120).
void end_muzzle_effect(RuntimeWeaponActor& weapon)
{
    WaterPistolBlocks& blocks = blocks_of(weapon);
    if (blocks.muzzle == nullptr)
    {
        return;
    }
    blocks.muzzle->bytes[0x18] |= 8;
    const float ended = -1.f;
    std::memcpy(blocks.muzzle->bytes.data() + 8, &ended, 4);
    blocks.muzzle = nullptr;
}

// Xbox 0x000CA500, ESI from: "trail" effects every 5 units from `from` toward `to`, at least one, their velocities
// (+0x8C) blended from one droplet's to the other's.
void trail_between(RuntimeWeaponActor& weapon, const RuntimeWaterDroplet& from, const RuntimeWaterDroplet& to)
{
    float dx = to.position.x - from.position.x;
    float dy = to.position.y - from.position.y;
    float dz = to.position.z - from.position.z;
    const float squared = dz * dz + dy * dy + dx * dx;
    const long double ex = (long double)from.position.x - to.position.x;
    const long double ey = (long double)from.position.y - to.position.y;
    const long double ez = (long double)from.position.z - to.position.z;
    const float distance = float(std::sqrt(ex * ex + ez * ez + ey * ey));
    if (squared != 0.f) // DAT_0032B100.
    {
        const float scale = engine_reciprocal_sqrt(squared); // 0x001534C0
        dx = dx * scale;
        dy = dy * scale;
        dz = dz * scale;
    }
    i32 count = 1;
    if (i32(distance * 0.2f) > 1) // DAT_0032B1AC.
    {
        count = i32(distance * 0.2f);
    }
    Vec3 point = from.position;
    const Vec3 advance{dx * 5.f, dy * 5.f, dz * 5.f}; // DAT_0032B184.
    for (i32 index = 0; index < count; ++index)
    {
        float t = 1.f; // DAT_0032B0F8.
        const float fraction = float(index) / float(count);
        if (float(count) != 0.f && fraction <= 1.f)
        {
            t = 0.f;
            if (0.f <= fraction)
            {
                t = fraction;
            }
        }
        const Vec3 velocity{(to.velocity.x - from.velocity.x) * t + from.velocity.x,
            (to.velocity.y - from.velocity.y) * t + from.velocity.y,
            (to.velocity.z - from.velocity.z) * t + from.velocity.z};
        if (ActorEffectNode* effect = spawn_effect(weapon, offsetof(WaterPistolWeaponState, trail_effect), point, nullptr);
            effect != nullptr)
        {
            std::memcpy(effect->bytes.data() + 0x8C, &velocity, 12);
            effect->bytes[0x18] |= 8;
        }
        point = Vec3{advance.x + point.x, advance.y + point.y, advance.z + point.z};
    }
}

// Xbox 0x000CA480, EAX the droplet, EDI block: the droplets before `droplet` (all of them when one or none would be
// left) are removed oldest first, trails left between each and the next.
void drop_before(RuntimeWeaponActor& weapon, GhoulySpawnPool& block, const u8* droplet)
{
    i32 removed = i32(droplet - block.begin) / 0x28;
    const i32 count = droplet_count(block);
    if (u32(count - removed) <= 1)
    {
        removed = count;
    }
    for (; removed > 0; --removed)
    {
        if (removed > 1)
        {
            trail_between(weapon, *droplet_at(block.begin), *droplet_at(block.begin + 0x28)); // 0x000CA500
        }
        remove_first(block);
    }
}

// Xbox 0x000C9D40, EAX hits, ECX to: the move from `from` to `to` (the droplet's point) against the world; a hit
// counts the probe's actor (when its record is flagged 2), splashes and drops the droplets behind the hit.
bool sweep_droplet(RuntimeWeaponActor& weapon, GhoulySpawnPool& block, u8* droplet, Vec3& to, Vec3 from,
    GhoulySpawnPool*& hits)
{
    const bool swept = entity_record_sweep_probe(to, from, 0xC, 0xC4000) != 0; // 0x00122480, EBX to
    const i32 segment = entity_record_model_segment_probe(to, from, 0x20102); // 0x001228F0, EBX to
    const auto& found = entity_detail::g_segment_slots[0]; // DAT_00511A1C / DAT_00511A20.
    if (segment != 0 && (read_u32({found.pool.begin, 0x54}, 0x20) & 2) != 0)
    {
        if (hits == nullptr)
        {
            hits = allocate_runtime_shape_pool(sizeof(RuntimeSprayHit), 4); // The inlined 0x00066260.
        }
        auto& hit = append<RuntimeSprayHit>(hits);
        hit.object = found.object;
        hit.contacts = copy_segment_records(found.pool); // 0x000FCC90
        hit.point = to;
        hit.velocity = droplet_at(droplet)->velocity;
    }
    if (!swept && segment == 0)
    {
        return false;
    }
    // The first droplet from this one on that lies behind the hit along the move, less one.
    const Vec3 move{to.x - from.x, to.y - from.y, to.z - from.z};
    u8* behind = droplet;
    for (; behind < block.cursor; behind += 0x28)
    {
        const Vec3& point = droplet_at(behind)->position;
        const float along = (point.x - to.x) * move.x + (point.y - to.y) * move.y + (point.z - to.z) * move.z;
        if (0.f > along)
        {
            behind -= 0x28;
            break;
        }
    }
    splash(weapon, offsetof(WaterPistolWeaponState, hit_effect), to);
    drop_before(weapon, block, behind); // 0x000CA480
    return true;
}

// Xbox 0x000C9C30, EBX block: the newest droplet's move (from the muzzle for the firing block), then older droplets
// three at a time back to the first, until one hits.
GhoulySpawnPool* query_block(RuntimeWeaponActor& weapon, GhoulySpawnPool& block, const Vec3& point, bool firing)
{
    GhoulySpawnPool* hits = nullptr;
    if (droplet_count(block) == 0)
    {
        return hits;
    }
    u8* droplet = block.cursor - 0x28;
    Vec3 to = droplet_at(droplet)->position;
    const Vec3 from = firing ? point : droplet_at(droplet)->previous;
    if (sweep_droplet(weapon, block, droplet, to, from, hits)) // 0x000C9D40
    {
        return hits;
    }
    const auto back = [&block](u8* at)
    {
        const i32 skipped = at < block.begin + 0x78 ? i32(at - block.begin) / 0x28 : 3;
        return at - skipped * 0x28;
    };
    u8* previous = droplet;
    droplet = back(droplet);
    while (droplet >= block.begin)
    {
        if (sweep_droplet(weapon, block, droplet, droplet_at(droplet)->position, droplet_at(previous)->position,
                hits)) // 0x000C9D40, ECX the droplet
        {
            break;
        }
        if (droplet == block.begin)
        {
            break;
        }
        previous = droplet;
        droplet = back(droplet);
    }
    return hits;
}

// Xbox 0x000C9FA0, EAX point, ECX direction: the actors within 3 of the muzzle (records flagged 2), a splash there,
// and the firing block's droplets dropped behind its newest. Whether any probe was found.
bool query_muzzle(RuntimeWeaponActor& weapon, GhoulySpawnPool& block, Vec3 point, const Vec3& direction,
    float radius, GhoulySpawnPool*& hits)
{
    const i32 count = entity_collect_contact_probes(point, radius, 0x20102); // 0x00122B00
    if (count == 0)
    {
        return false;
    }
    const float speed = droplet_speed(weapon);
    const Vec3 velocity{direction.x * speed, direction.y * speed, direction.z * speed};
    for (i32 slot = 0; slot < count; ++slot)
    {
        const auto& found = entity_detail::g_segment_slots[slot]; // DAT_00511A1C + 8n.
        if ((read_u32({found.pool.begin, 0x54}, 0x20) & 2) == 0)
        {
            continue;
        }
        if (hits == nullptr)
        {
            hits = allocate_runtime_shape_pool(sizeof(RuntimeSprayHit), 4); // The inlined 0x00066260.
        }
        auto& hit = append<RuntimeSprayHit>(hits);
        hit.object = found.object;
        hit.contacts = copy_segment_records(found.pool); // 0x000FCC90
        hit.point = point;
        hit.velocity = velocity;
    }
    splash(weapon, offsetof(WaterPistolWeaponState, hit_effect), point);
    if (droplet_count(block) != 0)
    {
        drop_before(weapon, block, block.cursor - 0x28); // 0x000CA480
    }
    return count > 0;
}

// Xbox 0x000CA830, EAX source: the source's hit records are appended to `hits` and the source is freed.
void merge_hits(GhoulySpawnPool* source, GhoulySpawnPool*& hits)
{
    if (source == nullptr)
    {
        return;
    }
    if (hits == nullptr)
    {
        hits = source;
        return;
    }
    for (u8* record = source->begin; record < source->cursor; record += sizeof(RuntimeSprayHit))
    {
        std::memcpy(&append<RuntimeSprayHit>(hits), record, sizeof(RuntimeSprayHit));
    }
    engine_heap_free(source); // HeapFree on the block's heap (0x000F7100).
}

// Xbox 0x000CA240, EAX block: the muzzle point (for the firing block) and the droplets newest first, drawn as a strip
// (0x000659F0) with the texture, width 2, 1.0 and colour 0x80FFFFFF.
void draw_block(RuntimeWeaponActor& weapon, const GhoulySpawnPool& block, const Vec3& point, bool firing)
{
    if (droplet_count(block) == 0)
    {
        return;
    }
    WaterPistolBlocks& blocks = blocks_of(weapon);
    if (blocks.strip == nullptr)
    {
        blocks.strip = allocate_runtime_shape_pool(sizeof(Vec3), 0x20); // The inlined 0x00066260.
    }
    else
    {
        blocks.strip->cursor = blocks.strip->begin;
    }
    if (firing)
    {
        append<Vec3>(blocks.strip) = point;
    }
    for (u8* droplet = block.cursor - 0x28; droplet >= block.begin; droplet -= 0x28)
    {
        append<Vec3>(blocks.strip) = droplet_at(droplet)->position;
    }
    sync_blocks(weapon);
    if (!weapon.services.display)
    {
        throw Error("0x000CA240 draws the droplet strip without the scene's display setup");
    }
    draw_world_line_strip(reinterpret_cast<const Vec3*>(blocks.strip->begin),
        i32(blocks.strip->cursor - blocks.strip->begin) / i32(sizeof(Vec3)), const_cast<u8*>(blocks.texture.data()), 2,
        1.f, 0x80FFFFFFu, weapon.services.display()); // 0x000659F0, ECX +0xF00
}
}

// Xbox 0x000C91F0
void water_pistol_weapon_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = sizeof(WaterPistolWeaponState);
    sizes.parameters = parameter::kSize;
}

// Xbox 0x000C9280
bool initialize_runtime_water_pistol_weapon(RuntimeWeaponActor& weapon, std::span<const u8> parameters)
{
    auto bytes = weapon.bytes;
    std::span<const u8> texture{};
    if (read_u32(parameters, parameter::texture.offset) == kAidPrefix)
    {
        texture = acquire_animation_resource(weapon.registry, field_data(parameters, parameter::texture)); // 0x00035E90
        if (texture.data() == nullptr)
        {
            resource_error(weapon.error, 2); // 0x00032700
        }
    }
    blocks_of(weapon) = WaterPistolBlocks{texture};
    fields_of(bytes).texture = texture.data() != nullptr ? 1u : 0u;
    for (u32 index = 0; index < 4; ++index)
    {
        const char* name = reinterpret_cast<const char*>(parameters.data() + parameter::hit.offset + index * 0x80);
        std::memcpy(bytes.data() + offsetof(WaterPistolWeaponState, hit_effect) + index * 0x80, name, std::strlen(name) + 1);
    }
    actor_record_view<GunWeaponState>(bytes).recharge = 0.075f; // DAT_0032B44C.
    actor_record_view<GunWeaponState>(bytes).drain = 0.15f; // DAT_0032B168.
    sync_blocks(weapon);
    return true;
}

// Xbox 0x000C9330
void destroy_runtime_water_pistol_weapon(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    WaterPistolBlocks& blocks = blocks_of(weapon);
    if (fields_of(bytes).texture != 0)
    {
        release_animation_resource(weapon.registry, blocks.texture); // 0x00035C00
        fields_of(bytes).texture = 0u;
    }
    if (blocks.active != nullptr)
    {
        engine_heap_free(blocks.active); // 0x000663E0
        blocks.active = nullptr;
    }
    if (blocks.strip != nullptr)
    {
        engine_heap_free(blocks.strip); // 0x000663E0
        blocks.strip = nullptr;
    }
    for (GhoulySpawnPool*& trail : blocks.trails)
    {
        if (trail != nullptr)
        {
            engine_heap_free(trail); // HeapFree on the block's heap (0x000F7100).
            trail = nullptr;
        }
    }
    end_muzzle_effect(weapon);
    water_pistol_blocks.erase(bytes.data());
}

// Xbox 0x000C9410
void start_runtime_water_pistol_fire(RuntimeWeaponActor& weapon, const Vec3& point, const Vec3* direction)
{
    WaterPistolBlocks& blocks = blocks_of(weapon);
    blocks.active = allocate_runtime_shape_pool(sizeof(RuntimeWaterDroplet), 0x20); // The inlined 0x00066260.
    fields_of(weapon.bytes).emission = 0.f;
    Vec3 euler{};
    if (direction != nullptr)
    {
        float yaw = 0.f;
        float pitch = 0.f;
        direction_to_yaw_pitch(direction->x, direction->y, direction->z, yaw, pitch); // 0x00105E20
        euler = Vec3{pitch, yaw, 0.f};
    }
    blocks.muzzle = spawn_effect(weapon, offsetof(WaterPistolWeaponState, muzzle_name), point, direction != nullptr ? &euler : nullptr);
    if (blocks.muzzle != nullptr)
    {
        blocks.muzzle->bytes[0x18] &= 0xF7;
    }
    sync_blocks(weapon);
}

// Xbox 0x000C9540
void end_runtime_water_pistol_fire(RuntimeWeaponActor& weapon, const Vec3& point)
{
    retire_runtime_water_pistol_block(weapon); // 0x000CA3D0, ESI actor
    splash(weapon, offsetof(WaterPistolWeaponState, expire_effect), point);
    end_muzzle_effect(weapon);
    sync_blocks(weapon);
}

// Xbox 0x000CA3D0, ESI actor
void retire_runtime_water_pistol_block(RuntimeWeaponActor& weapon)
{
    WaterPistolBlocks& blocks = blocks_of(weapon);
    weapon_base(weapon.bytes).firing = 0;
    GhoulySpawnPool* block = blocks.active;
    if (block == nullptr)
    {
        return;
    }
    if (u32(droplet_count(*block)) <= 1)
    {
        engine_heap_free(block); // 0x000663E0
        blocks.active = nullptr;
        sync_blocks(weapon);
        return;
    }
    for (GhoulySpawnPool*& trail : blocks.trails)
    {
        if (trail == nullptr)
        {
            trail = block;
            blocks.active = nullptr;
            sync_blocks(weapon);
            return;
        }
    }
    engine_heap_free(block); // HeapFree on the block's heap (0x000F7100).
    blocks.active = nullptr;
    sync_blocks(weapon);
}

// Xbox 0x000C95F0
void update_runtime_water_pistol(RuntimeWeaponActor& weapon, float step)
{
    auto bytes = weapon.bytes;
    actor_record_view<GunWeaponState>(bytes).drain = (fire_word(weapon) + 1.f) * 0.075f; // DAT_0032B0F8, DAT_0032B44C.
    WaterPistolBlocks& blocks = blocks_of(weapon);
    if (blocks.active != nullptr && !step_runtime_water_block(weapon, *blocks.active, step) // 0x000C96E0
        && actor_record_view<GunWeaponState>(bytes).firing == 0)
    {
        engine_heap_free(blocks.active); // 0x000663E0
        blocks.active = nullptr;
    }
    for (GhoulySpawnPool*& trail : blocks.trails)
    {
        if (trail != nullptr && !step_runtime_water_block(weapon, *trail, step)) // 0x000C96E0
        {
            engine_heap_free(trail); // HeapFree on the block's heap (0x000F7100).
            trail = nullptr;
        }
    }
    sync_blocks(weapon);
}

// Xbox 0x000C96E0
bool step_runtime_water_block(RuntimeWeaponActor& weapon, GhoulySpawnPool& block, float step)
{
    const float fall = step * -100.f; // DAT_0032B594.
    for (u8* at = block.begin; at < block.cursor; at += 0x28)
    {
        RuntimeWaterDroplet& droplet = *droplet_at(at);
        droplet.age = step + droplet.age;
        droplet.velocity.y = fall + droplet.velocity.y;
        droplet.previous = droplet.position;
        droplet.position.x = droplet.velocity.x * step + droplet.position.x;
        droplet.position.y = step * droplet.velocity.y + droplet.position.y;
        droplet.position.z = droplet.velocity.z * step + droplet.position.z;
        if (!(droplet.age > 2.f)) // DAT_0032B110.
        {
            continue;
        }
        splash(weapon, offsetof(WaterPistolWeaponState, expire_effect), droplet.position);
        const u8* last = block.cursor - block.stride;
        if (at < last)
        {
            std::memmove(at, at + block.stride, size_t(last - at)); // 0x00165AF0
        }
        block.cursor -= block.stride;
        at -= 0x28;
    }
    return 1 < u32(droplet_count(block));
}

// Xbox 0x000C98D0
void emit_runtime_water_pistol(RuntimeWeaponActor& weapon, u32 firing, const Vec3& point, const Vec3& direction)
{
    auto bytes = weapon.bytes;
    const float step = frame_rate(weapon.actor) * frame_step(weapon.frame);
    weapon_base(bytes).firing = u8(firing);
    if (firing == 0)
    {
        return;
    }
    fields_of(bytes).emission = step + fields_of(bytes).emission;
    float offset = 0.f;
    Vec3 aim{};
    normalize_runtime_graphics_vector(aim, direction); // D3DXVec3Normalize, 0x00184F99
    WaterPistolBlocks& blocks = blocks_of(weapon);
    while (fields_of(bytes).emission > kEmitInterval)
    {
        fields_of(bytes).emission = fields_of(bytes).emission - kEmitInterval;
        offset = offset + kEmitInterval;
        if (blocks.active == nullptr)
        {
            throw Error("0x000C98D0 emits into a droplet block through address zero");
        }
        RuntimeWaterDroplet& droplet = append<RuntimeWaterDroplet>(blocks.active); // The inlined 0x000F73A0 growth.
        droplet.position = Vec3{point.x + aim.x * offset, aim.y * offset + point.y, point.z + aim.z * offset};
        droplet.previous = droplet.position;
        const float speed = droplet_speed(weapon);
        droplet.velocity = Vec3{aim.x * speed, aim.y * speed, aim.z * speed};
        droplet.age = 0.f;
    }
    if (blocks.muzzle != nullptr)
    {
        float yaw = 0.f;
        float pitch = 0.f;
        direction_to_yaw_pitch(direction.x, direction.y, direction.z, yaw, pitch); // 0x00105E20
        std::memcpy(blocks.muzzle->bytes.data() + 0x1C, &point, 12);
        const Vec3 euler{pitch, yaw, 0.f};
        std::memcpy(blocks.muzzle->bytes.data() + 0x50, &euler, 12);
    }
    sync_blocks(weapon);
}

// Xbox 0x000C9B20
GhoulySpawnPool* query_runtime_water_pistol(RuntimeWeaponActor& weapon, const Vec3& point, const Vec3& direction)
{
    GhoulySpawnPool* hits = nullptr;
    WaterPistolBlocks& blocks = blocks_of(weapon);
    if (blocks.active != nullptr)
    {
        query_muzzle(weapon, *blocks.active, point, direction, 3.f, hits); // 0x000C9FA0
    }
    if (blocks.active != nullptr)
    {
        merge_hits(query_block(weapon, *blocks.active, point, true), hits); // 0x000C9C30, 0x000CA830
    }
    for (GhoulySpawnPool*& trail : blocks.trails)
    {
        if (trail == nullptr)
        {
            continue;
        }
        merge_hits(query_block(weapon, *trail, point, false), hits); // 0x000C9C30, 0x000CA830
        if (droplet_count(*trail) == 0)
        {
            engine_heap_free(trail); // HeapFree on the block's heap (0x000F7100).
            trail = nullptr;
        }
    }
    sync_blocks(weapon);
    return hits;
}

// Xbox 0x000CA1F0
void draw_runtime_water_pistol(RuntimeWeaponActor& weapon, const Vec3& point)
{
    WaterPistolBlocks& blocks = blocks_of(weapon);
    if (blocks.active != nullptr)
    {
        draw_block(weapon, *blocks.active, point, true); // 0x000CA240
    }
    for (GhoulySpawnPool* trail : blocks.trails)
    {
        if (trail != nullptr)
        {
            draw_block(weapon, *trail, point, false); // 0x000CA240
        }
    }
}
}
