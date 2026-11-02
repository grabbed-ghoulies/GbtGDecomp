#include "extinguisher_weapon.h"
#include "actor_shape_bounds.h"
#include "entity.h"
#include "entity_collision.h"
#include "entity_heap.h"
#include "segment_records.h"
#include "weapon_actor_detail.h"
#include "../audio/listener_settings.h"
#include "../graphics/billboard_queue.h"
#include "../graphics/vector_normalize.h"
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include <bit>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
using namespace weapon_detail;

namespace
{
namespace field = extinguisher_weapon_field;
namespace parameter = extinguisher_weapon_parameter;
namespace gun = gun_weapon_field;

// The spray block pools (retail +0xF20 and the eight +0xF00 words), by the weapon's bytes.
struct ExtinguisherBlocks
{
    std::span<const u8> spray; // +0xEFC, the spray AID's bytes.
    GhoulySpawnPool* active = nullptr; // +0xF20.
    std::array<GhoulySpawnPool*, 8> trails{}; // +0xF00.
};
std::unordered_map<const u8*, ExtinguisherBlocks> extinguisher_blocks;

constexpr float kEmitInterval = 0.03333333507180214f; // DAT_0032B448, 1/30 s.

ExtinguisherBlocks& blocks_of(const RuntimeWeaponActor& weapon)
{
    return extinguisher_blocks[weapon.bytes.data()];
}

// The +0xF20 / +0xF00 words follow the native pools.
void sync_blocks(RuntimeWeaponActor& weapon)
{
    const ExtinguisherBlocks& blocks = blocks_of(weapon);
    set(weapon.bytes, field::active, blocks.active != nullptr ? 1u : 0u);
    for (u32 index = 0; index < 8; ++index)
    {
        store32(weapon.bytes, field::kTrails + index * 4, blocks.trails[index] != nullptr ? 1u : 0u);
    }
}

float weapon_step(const RuntimeWeaponActor& weapon)
{
    return frame_rate(weapon.actor) * frame_step(weapon.frame);
}

float spray_word(const RuntimeWeaponActor& weapon)
{
    return std::bit_cast<float>(get(weapon.bytes, gun::unk_EE8));
}

// A value between start and end by age over life, clamped between them; the end when life is zero.
float sample_spray(float age, float life, float start, float end)
{
    if (life == 0.f)
    {
        return end;
    }
    const float value = (age / life) * (end - start) + start;
    if (end > start)
    {
        if (value > end)
        {
            return end;
        }
        if (start > value)
        {
            return start;
        }
        return value;
    }
    if (end > value)
    {
        return end;
    }
    if (value > start)
    {
        return start;
    }
    return value;
}

u8 truncate_byte(long double value)
{
    return u8(truncate_runtime_float(value)); // 0x001673D8
}

std::span<RuntimeSprayParticle> particles(const GhoulySpawnPool& block)
{
    return {reinterpret_cast<RuntimeSprayParticle*>(block.begin),
        size_t(block.cursor - block.begin) / sizeof(RuntimeSprayParticle)};
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

// The spheres' radii (3, 4, 5, 6, 7 times the spread, DAT_0032B260 + i * DAT_0032B1AC * DAT_0032B184, clamped to
// DAT_0032B26C).
float sphere_scale(i32 index)
{
    float scale = float(index) * 0.2f * 5.f + 3.f;
    if (scale > 8.f)
    {
        return 8.f;
    }
    if (3.f > scale)
    {
        return 3.f;
    }
    return scale;
}
}

// Xbox 0x000C7B50
void extinguisher_weapon_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = field::kSize;
    sizes.parameters = parameter::kSize;
}

// Xbox 0x000C7BE0
bool initialize_runtime_extinguisher_weapon(RuntimeWeaponActor& weapon, std::span<const u8> parameters)
{
    auto bytes = weapon.bytes;
    const char* name = field_data(parameters, parameter::spray);
    std::span<const u8> spray{};
    if (read_u32(parameters, parameter::spray.offset) == kAidPrefix)
    {
        spray = acquire_animation_resource(weapon.registry, name); // 0x00035E90
        if (spray.data() == nullptr)
        {
            resource_error(weapon.error, 2); // 0x00032700
        }
    }
    blocks_of(weapon) = ExtinguisherBlocks{spray};
    set(bytes, gun::recharge, 0.075f); // DAT_0032B44C.
    set(bytes, gun::drain, 0.15f); // DAT_0032B168.
    set(bytes, field::spray, spray.data() != nullptr ? 1u : 0u);
    set(bytes, field::reach, 0.01f); // DAT_0032B124.
    sync_blocks(weapon);
    return true;
}

// Xbox 0x000C7C60
void destroy_runtime_extinguisher_weapon(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    ExtinguisherBlocks& blocks = blocks_of(weapon);
    if (get(bytes, field::spray) != 0)
    {
        release_animation_resource(weapon.registry, blocks.spray); // 0x00035C00
        set(bytes, field::spray, 0u);
    }
    if (blocks.active != nullptr)
    {
        engine_heap_free(blocks.active); // 0x000663E0
        blocks.active = nullptr;
    }
    for (GhoulySpawnPool*& trail : blocks.trails)
    {
        if (trail != nullptr)
        {
            engine_heap_free(trail); // HeapFree on the block's heap (0x000F7100).
            trail = nullptr;
        }
    }
    extinguisher_blocks.erase(bytes.data());
}

// Xbox 0x000C7D10
void start_runtime_extinguisher_spray(RuntimeWeaponActor& weapon)
{
    blocks_of(weapon).active = allocate_runtime_shape_pool(sizeof(RuntimeSprayParticle), 0x20); // The inlined 0x00066260.
    set(weapon.bytes, field::emission, 0.f);
    sync_blocks(weapon);
}

// Xbox 0x000C7D50
void end_runtime_extinguisher_spray(RuntimeWeaponActor& weapon)
{
    retire_runtime_extinguisher_spray(weapon); // 0x000C8D30, ESI actor
}

// Xbox 0x000C8D30, ESI actor
void retire_runtime_extinguisher_spray(RuntimeWeaponActor& weapon)
{
    ExtinguisherBlocks& blocks = blocks_of(weapon);
    GhoulySpawnPool* block = blocks.active;
    if (block == nullptr)
    {
        return;
    }
    if (particles(*block).empty())
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

// Xbox 0x000C7D60
void update_runtime_extinguisher_spray(RuntimeWeaponActor& weapon, float step)
{
    auto bytes = weapon.bytes;
    set(bytes, gun::drain, spray_word(weapon) * 0.135f + 0.015f); // DAT_0032B384, DAT_0032B380.
    ExtinguisherBlocks& blocks = blocks_of(weapon);
    if (blocks.active != nullptr && !step_runtime_spray_block(*blocks.active, step) // 0x000C7E40
        && get(bytes, gun::firing) == 0)
    {
        engine_heap_free(blocks.active); // 0x000663E0
        blocks.active = nullptr;
    }
    for (GhoulySpawnPool*& trail : blocks.trails)
    {
        if (trail != nullptr && !step_runtime_spray_block(*trail, step)) // 0x000C7E40
        {
            engine_heap_free(trail); // HeapFree on the block's heap (0x000F7100).
            trail = nullptr;
        }
    }
    sync_blocks(weapon);
}

// Xbox 0x000C7E40, EDI block
bool step_runtime_spray_block(GhoulySpawnPool& block, float step)
{
    const float fall = step * 0.f;
    const size_t stride = block.stride;
    u8* end = block.cursor;
    for (u8* at = block.begin; at < end; at += stride)
    {
        auto& particle = *reinterpret_cast<RuntimeSprayParticle*>(at);
        particle.velocity.y = fall + particle.velocity.y;
        particle.rotation = particle.spin * step + particle.rotation;
        particle.position.x = particle.position.x + particle.velocity.x * step;
        particle.position.y = particle.position.y + step * particle.velocity.y;
        particle.position.z = particle.position.z + particle.velocity.z * step;
        const float age = particle.age + step;
        particle.age = age;
        if (age < particle.life || age != age || particle.life != particle.life)
        {
            particle.size = sample_spray(age, particle.life, particle.size_start, particle.size_end);
            particle.alpha = truncate_byte(sample_spray(age, particle.life, float(i32(particle.alpha_start)),
                float(i32(particle.alpha_end))));
            continue;
        }
        u8* last = block.cursor - stride;
        if (at < last)
        {
            std::memmove(at, at + stride, size_t(last - at));
        }
        block.cursor -= stride;
        at -= stride;
        end -= stride;
    }
    return (block.cursor - block.begin) / i32(stride) != 0;
}

// Xbox 0x000C8C70
void draw_runtime_extinguisher_spray(RuntimeWeaponActor& weapon)
{
    ExtinguisherBlocks& blocks = blocks_of(weapon);
    if (blocks.active != nullptr)
    {
        draw_runtime_spray_block(weapon, *blocks.active); // 0x000C8CB0
    }
    for (GhoulySpawnPool* trail : blocks.trails)
    {
        if (trail != nullptr)
        {
            draw_runtime_spray_block(weapon, *trail); // 0x000C8CB0
        }
    }
}

// Xbox 0x000C8CB0, ECX block, EBX actor
void draw_runtime_spray_block(RuntimeWeaponActor& weapon, const GhoulySpawnPool& block)
{
    const auto list = particles(block);
    if (list.empty())
    {
        return;
    }
    const u8* sprite = blocks_of(weapon).spray.data(); // +0xEFC
    for (const RuntimeSprayParticle& particle : list)
    {
        const u32 color = 0x00FFFFFFu | (u32(particle.alpha) << 24);
        const std::array<float, 2> size{particle.size, particle.size};
        queue_billboard(g_billboard_queue, sprite, color, 0, particle.position, size.data(),
            particle.rotation); // 0x0005C1A0
    }
}

// Xbox 0x000C8030
void emit_runtime_extinguisher_spray(RuntimeWeaponActor& weapon, u32 firing, const Vec3& point,
    const Vec3& direction)
{
    auto bytes = weapon.bytes;
    set(bytes, weapon_field::firing, u8(firing));
    if (firing == 0)
    {
        const float reach = get(bytes, field::reach) - weapon_step(weapon) * 4.949999809265137f; // DAT_0032B464.
        set(bytes, field::reach, reach);
        if (0.01f > reach) // DAT_0032B124.
        {
            set(bytes, field::reach, 0.01f);
        }
        return;
    }
    const float reach = weapon_step(weapon) * 1.9800000190734863f + get(bytes, field::reach); // DAT_0032B460.
    set(bytes, field::reach, reach);
    if (reach > 1.f) // DAT_0032B0F8.
    {
        set(bytes, field::reach, 1.f);
    }
    set(bytes, field::emission, weapon_step(weapon) + get(bytes, field::emission));
    const float speed = spray_word(weapon) * 70.f + 30.f; // DAT_0032B2C4, DAT_0032B108.
    const float scale = (spray_word(weapon) + 1.f) * 0.5f; // DAT_0032B0F4.
    float delay = 0.f;
    Vec3 unit;
    normalize_runtime_graphics_vector(unit, direction); // D3DXVec3Normalize, 0x00184F99
    if (!(get(bytes, field::emission) > kEmitInterval))
    {
        return;
    }
    GhoulySpawnPool*& block = blocks_of(weapon).active;
    if (block == nullptr)
    {
        throw Error("0x000C8030 emits into the extinguisher's spray block through address zero");
    }
    do
    {
        auto& particle = append<RuntimeSprayParticle>(block);
        const float travel = delay * speed;
        particle.position = Vec3{point.x + unit.x * travel, point.y + unit.y * travel, point.z + unit.z * travel};
        particle.velocity = Vec3{unit.x * speed, unit.y * speed, unit.z * speed};
        particle.life = (next_unit(weapon.random) - 1.f) * 0.19999998807907104f + 0.25f; // DAT_0032B3A4, DAT_0032B104.
        particle.age = delay;
        particle.bounced = 0;
        particle.size_start = ((next_unit(weapon.random) - 1.f) * 0.2999999523162842f + 2.5f) * scale; // DAT_0032B45C, DAT_0032B20C.
        particle.size_end = ((next_unit(weapon.random) - 1.f) + 8.f) * scale; // DAT_0032B26C.
        particle.size = sample_spray(particle.age, particle.life, particle.size_start, particle.size_end);
        // DAT_0032B458, DAT_0032B454 and DAT_0032B450 on the x87 stack.
        particle.alpha_start = truncate_byte((static_cast<long double>(next_unit(weapon.random)) - 1.0L) * 32.0L
            + 192.0L);
        particle.alpha_end = truncate_byte((static_cast<long double>(next_unit(weapon.random)) - 1.0L) * 32.0L
            + 64.0L);
        particle.alpha = truncate_byte(sample_spray(particle.age, particle.life, float(i32(particle.alpha_start)),
            float(i32(particle.alpha_end))));
        // DAT_0032B290, DAT_0032B0FC, DAT_0032B3DC: 70 to 250 degrees a second.
        particle.spin = (next_unit(weapon.random) - 1.f) * 180.f * 0.01745329238474369f + 1.2217304706573486f;
        particle.rotation = (next_unit(weapon.random) - 1.f) * 6.2831854820251465f; // DAT_0032B298.
        set(bytes, field::emission, get(bytes, field::emission) - kEmitInterval);
        delay += kEmitInterval;
    } while (get(bytes, field::emission) > kEmitInterval);
}

// Xbox 0x000C8590
GhoulySpawnPool* query_runtime_extinguisher_spray(RuntimeWeaponActor& weapon, const Vec3& point,
    const Vec3& direction)
{
    GhoulySpawnPool* hits = nullptr;
    ExtinguisherBlocks& blocks = blocks_of(weapon);
    if (blocks.active != nullptr)
    {
        query_runtime_spray_block(weapon, *blocks.active, point, direction, hits); // 0x000C8610, EAX point
    }
    for (GhoulySpawnPool* trail : blocks.trails)
    {
        if (trail != nullptr)
        {
            query_runtime_spray_block(weapon, *trail, point, direction, hits); // 0x000C8610, EAX point
        }
    }
    return hits;
}

// Xbox 0x000C8610, EAX point
void query_runtime_spray_block(RuntimeWeaponActor& weapon, GhoulySpawnPool& block, const Vec3& point,
    const Vec3& direction, GhoulySpawnPool*& hits)
{
    auto bytes = weapon.bytes;
    const float spread = spray_word(weapon) * 0.699999988079071f + 0.30000001192092896f; // DAT_0032B264, DAT_0032B268.
    float length = 0.f;
    for (i32 index = 0; index < 5; ++index)
    {
        length = sphere_scale(index) * spread * 2.f + length; // DAT_0032B110.
    }
    // The spray's reach from 12 behind the muzzle (DAT_0032B4F4): a wall across it turns the particles back.
    const float reach = get(bytes, field::reach) * length;
    Vec3 near{direction.x * -12.f + point.x, direction.y * -12.f + point.y, direction.z * -12.f + point.z};
    Vec3 far{direction.x * reach + point.x, direction.y * reach + point.y, direction.z * reach + point.z};
    const i32 swept = entity_record_sweep_probe(far, near, 4, 0xC4000); // 0x00122480, EBX far
    Vec3 normal{};
    if (swept != 0)
    {
        std::memcpy(&normal, entity_detail::g_segment_slots[0].pool.begin + 0xC, sizeof(normal));
    }
    const bool segment = entity_record_model_segment_probe(far, near, 2) != 0; // 0x001228F0, EBX far
    if (segment)
    {
        std::memcpy(&normal, entity_detail::g_segment_slots[0].pool.begin + 0xC, sizeof(normal));
    }
    if (segment || swept != 0)
    {
        const float dy = near.y - far.y;
        const float dx = near.x - far.x;
        const float dz = near.z - far.z;
        const float wall = dz * dz + dy * dy + dx * dx;
        for (RuntimeSprayParticle& particle : particles(block))
        {
            if (particle.bounced != 0)
            {
                continue;
            }
            const float pz = near.z - particle.position.z;
            const float py = near.y - particle.position.y;
            const float px = near.x - particle.position.x;
            const float distance = pz * pz + py * py + px * px;
            if (!(distance >= wall))
            {
                continue;
            }
            // The velocity reflected about the wall's normal (DAT_0032B120, DAT_0032B110), then slowed to 0.9
            // (DAT_0032B174).
            const float vx = particle.velocity.x * -1.f;
            const float vz = particle.velocity.z * -1.f;
            const float vy = particle.velocity.y * -1.f;
            const float dot = (vz * normal.z + vy * normal.y + vx * normal.x) * 2.f;
            const float rx = dot * normal.x - vx;
            const float ry = normal.y * dot - vy;
            const float rz = normal.z * dot - vz;
            particle.velocity = Vec3{rx * 0.8999999761581421f, ry * 0.8999999761581421f, rz * 0.8999999761581421f};
            particle.bounced = 1;
        }
    }
    // Five spheres from 8 behind the muzzle (DAT_0032B4F0), each touching the last, until one meets the world or an
    // actor that counts.
    Vec3 start{direction.x * -8.f + point.x, direction.y * -8.f + point.y, direction.z * -8.f + point.z};
    const float speed = spray_word(weapon) * 70.f + 30.f; // DAT_0032B2C4, DAT_0032B108.
    const Vec3 velocity{direction.x * speed, direction.y * speed, direction.z * speed};
    float previous = 0.f;
    for (i32 index = 0; index < 5; ++index)
    {
        const float radius = get(bytes, field::reach) * sphere_scale(index) * spread;
        const float distance = radius + previous;
        Vec3 center{direction.x * distance + start.x, direction.y * distance + start.y,
            direction.z * distance + start.z};
        const bool blocked = entity_collect_sphere_contacts(center, radius, 4, 0xC4000) > 0; // 0x001227E0
        const i32 count = entity_collect_contact_probes(center, radius, 0x802); // 0x00122B00
        i32 counted = 0;
        if (count != 0)
        {
            if (hits == nullptr)
            {
                hits = allocate_runtime_shape_pool(sizeof(RuntimeSprayHit), 8); // The inlined 0x00066260 (0x0014E872).
            }
            for (i32 slot = 0; slot < count; ++slot)
            {
                auto& hit = append<RuntimeSprayHit>(hits);
                auto& found = entity_detail::g_segment_slots[slot]; // DAT_00511A1C.
                hit.object = found.object;
                hit.contacts = copy_segment_records(found.pool); // 0x000FCC90
                hit.point = center;
                hit.velocity = velocity;
                u32 flags = 0;
                for (const u8* record = hit.contacts->begin; record < hit.contacts->cursor; record += 0x54)
                {
                    flags |= read_u32({record, 0x54}, 0x20);
                }
                if ((flags & 0x800) == 0)
                {
                    ++counted;
                }
            }
        }
        if (blocked || counted != 0)
        {
            return;
        }
        previous = radius;
        start = center;
    }
}
}
