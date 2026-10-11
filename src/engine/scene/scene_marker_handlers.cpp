#include "scene_marker_handlers.h"
#include "../entities/actor_shape_bounds.h"
#include "../entities/entity_heap.h"
#include "../runtime/name_lookup.h"
#include "../entities/actor_trail_requests.h"
#include "gbtg/actor_spawn_factory.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
u32 word_at(const u8* bytes, size_t offset)
{
    u32 value;
    std::memcpy(&value, bytes + offset, 4);
    return value;
}

u16 half_at(const u8* bytes, size_t offset)
{
    u16 value;
    std::memcpy(&value, bytes + offset, 2);
    return value;
}

float float_at(const u8* bytes, size_t offset)
{
    return std::bit_cast<float>(word_at(bytes, offset));
}

Vec3 vector_at(const u8* bytes, size_t offset)
{
    return {float_at(bytes, offset), float_at(bytes, offset + 4), float_at(bytes, offset + 8)};
}

u16 marker_tag(const u8* record, size_t offset, const char* const* tags)
{
    const i32 index = binary_search_names(0xE9, tags, reinterpret_cast<const char*>(record + offset));
    return index == -1 ? 0 : u16(index + 1);
}

// Row of DAT_0051108C with the given id; a miss is read through address zero, as retail does.
const u8* chain_row(const u8* records, std::span<const u8> address_zero, u16 id)
{
    const u8* found = find_runtime_marker(records, i16(id));
    return found != nullptr ? found : address_zero.data();
}
}

// Xbox 0x001246A0
OccupancyRecord& add_runtime_occupancy_record(u16 kind, u8 value, u16 id, u32 first, u32 second, u32 third,
    u32 fourth, u32 mask, u16 group)
{
    auto* pool = static_cast<GhoulySpawnPool*>(g_512344);
    if (pool == nullptr)
    {
        pool = static_cast<GhoulySpawnPool*>(engine_heap_allocate(i32(sizeof(GhoulySpawnPool)) + 6 * 0x38));
        pool->begin = reinterpret_cast<u8*>(pool + 1);
        pool->cursor = pool->begin;
        pool->stride = 0x38;
        pool->grow = 6;
        pool->limit = pool->begin + 6 * 0x38;
        g_512344 = pool;
    }
    if (pool->cursor == pool->limit)
    {
        pool = grow_runtime_shape_pool(pool);
        g_512344 = pool;
    }
    auto* record = reinterpret_cast<OccupancyRecord*>(pool->cursor);
    pool->cursor += pool->stride;
    record->kind = i16(kind);
    record->value_0 = value;
    std::memcpy(record->values_a, &id, 2);
    std::memcpy(record->values_a + 2, &first, 4);
    std::memcpy(record->values_a + 6, &second, 4);
    record->mask = mask;
    std::memcpy(record->values_a + 0xA, &third, 4);
    std::memcpy(record->values_a + 0xE, &fourth, 4);
    record->group = group < 2 ? 0 : i16(group);
    g_512348 = i32(u32(g_512348) | mask);
    return *record;
}

// Xbox 0x00100210
void add_runtime_marker_sphere(const u8* record, const char* const* tags)
{
    const u16 group = marker_tag(record, 0x30, tags);
    const float radius = float(i32(word_at(record, 0x2C)));
    auto& occupancy = add_runtime_occupancy_record(0, record[0x28], half_at(record, 6), word_at(record, 0x70),
        word_at(record, 0x74), word_at(record, 0x78), word_at(record, 0x7C), record[0x29], group);
    occupancy.center = vector_at(record, 0xC);
    occupancy.extent_x = radius;
    occupancy.extent_z = radius * radius;
}

// Xbox 0x001002B0
void add_runtime_marker_box(const u8* record, const char* const* tags)
{
    const u16 group = marker_tag(record, 0x34, tags);
    const float yaw = float_at(record, 0x1C);
    const float depth = float(i32(word_at(record, 0x30)));
    const float width = float(i32(word_at(record, 0x2C)));
    auto& occupancy = add_runtime_occupancy_record(1, record[0x28], half_at(record, 6), word_at(record, 0x74),
        word_at(record, 0x78), word_at(record, 0x7C), word_at(record, 0x80), record[0x29], group);
    occupancy.center = vector_at(record, 0xC);
    occupancy.extent_x = width;
    occupancy.extent_z = depth;
    occupancy.yaw = yaw;
}

// Xbox 0x000FF880
void set_runtime_marker_camera_start(const u8* record, RuntimeMarkerCameraStart& start)
{
    if (word_at(record, 0x28) == 0)
    {
        start.position.x = float_at(record, 0xC);
        start.euler.z = float_at(record, 0x20);
        start.euler.x = 0.f - float_at(record, 0x18);
        start.euler.y = float_at(record, 0x1C) + std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
        start.position.y = float_at(record, 0x10);
        start.position.z = float_at(record, 0x14);
        start.valid = 1;
    }
}

// Xbox 0x00100110
void resolve_runtime_marker_chain_pose(const u8* record, const u8* records, std::span<const u8> address_zero,
    Vec3& position, Vec3& euler)
{
    if (half_at(record, 8) == 0)
    {
        position = vector_at(record, 0xC);
        euler = vector_at(record, 0x18);
        return;
    }
    const u8* row = record;
    do
    {
        row = chain_row(records, address_zero, half_at(row, 8));
        position = vector_at(row, 0xC);
        euler = vector_at(row, 0x18);
    } while (half_at(row, 8) != 0);
}

// Xbox 0x000FFFD0
void send_runtime_marker_chain_goals(const u8* record, const Vec3& position, const Vec3& euler, const u8* records,
    std::span<const u8> address_zero, const std::function<void(std::span<const u32>)>& message)
{
    std::array<u32, 9> goal{};
    goal[0] = 0x4D;
    if (half_at(record, 8) == 0)
    {
        goal[2] = std::bit_cast<u32>(position.x);
        goal[3] = std::bit_cast<u32>(position.y);
        goal[4] = std::bit_cast<u32>(position.z);
        goal[5] = std::bit_cast<u32>(euler.x);
        goal[6] = std::bit_cast<u32>(euler.y);
        goal[7] = std::bit_cast<u32>(euler.z);
        goal[1] = half_at(record, 0x2A);
        message(goal);
        return;
    }
    const u8* row = record;
    do
    {
        row = chain_row(records, address_zero, half_at(row, 8));
        for (size_t index = 0; index < 6; ++index)
        {
            goal[2 + index] = word_at(row, 0xC + 4 * index);
        }
        goal[1] = half_at(record, 0x2A);
        goal[8] = half_at(row, 8) != 0 ? 1 : 0;
        message(goal);
    } while (half_at(row, 8) != 0);
}

// Xbox 0x000FFEC0
void spawn_runtime_marker_player(const u8* record, RuntimeMarkerPlayerStart& context)
{
    const bool restricted = context.chapter_mode == 1 || (context.scene_flags & 8) != 0
        || (context.runtime_flags & 0x200000) != 0;
    if (word_at(record, 0x2C) != 0)
    {
        if (context.player_start_enabled != 0 && !restricted)
        {
            return;
        }
        if (half_at(record, 0x28) != 0)
        {
            return;
        }
    }
    Vec3 position;
    Vec3 euler;
    if (half_at(record, 0x2A) != 0 && restricted)
    {
        resolve_runtime_marker_chain_pose(record, context.records, context.address_zero, position, euler);
    }
    else
    {
        position = vector_at(record, 0xC);
        euler = vector_at(record, 0x18);
    }
    FactoryObject actor = create_runtime_player_actor(nullptr, position, euler, context.spawn);
    if (actor.bytes == nullptr)
    {
        return;
    }
    auto& owned = context.actors.emplace_back(std::move(actor));
    if (half_at(record, 0x2A) != 0 && !context.script_end_gate())
    {
        send_runtime_marker_chain_goals(record, position, euler, context.records, context.address_zero,
            [&](std::span<const u32> message)
            {
                context.actor_message(owned, message);
            });
    }
}

namespace
{
bool is_aid(const u8* text)
{
    return word_at(text, 0) == 0x5F646961;
}

const char* aid_or_null(const u8* text)
{
    return is_aid(text) ? reinterpret_cast<const char*>(text) : nullptr;
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_half(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

void put_vector(std::span<u8> bytes, size_t offset, const Vec3& value)
{
    std::memcpy(bytes.data() + offset, &value.x, 4);
    std::memcpy(bytes.data() + offset + 4, &value.y, 4);
    std::memcpy(bytes.data() + offset + 8, &value.z, 4);
}

void copy_text(std::span<u8> bytes, size_t offset, const char* text)
{
    std::strcpy(reinterpret_cast<char*>(bytes.data() + offset), text);
}

FactoryObject* keep(FactoryObject object, RuntimeMarkerObjects& context)
{
    if (object.bytes == nullptr)
    {
        return nullptr;
    }
    return &context.objects.emplace_back(std::move(object));
}
}

// Xbox 0x000FF680
void create_runtime_marker_prop(const u8* record, RuntimeMarkerObjects& context)
{
    std::array<u8, 0x334> parameters{};
    initialize_actor_effect_parameters(parameters.data(), context.default_name);
    put_word(parameters, 8, word_at(record, 0xC));
    put_half(parameters, 0x2AA, half_at(record, 6));
    put_half(parameters, 0x2AC, half_at(record, 8));
    for (size_t offset = 0x10; offset <= 0x24; offset += 4)
    {
        put_word(parameters, offset - 4, word_at(record, offset));
    }
    put_half(parameters, 4, 2);
    if (is_aid(record + 0x28))
    {
        copy_text(parameters, 0x28, reinterpret_cast<const char*>(record + 0x28));
    }
    const char* name = reinterpret_cast<const char*>(record + 0xA8);
    if (is_aid(record + 0xA8))
    {
        copy_text(parameters, 0xA8, name);
    }
    FactoryObject* actor = keep(create_factory_object(context.factory, parameters), context);
    if (actor != nullptr && is_aid(record + 0x128))
    {
        char events[0x80];
        std::strcpy(events, reinterpret_cast<const char*>(record + 0x128));
        AnimationMix mix;
        mix.events = events;
        start_actor_animation_mix(context.animation_of(*actor), 0, mix, name, context.registry, context.error,
            context.random);
    }
}

// Xbox 0x00069F30
void send_runtime_marker_actor_goals(u16 first, u16 goal, FactoryObject& actor, RuntimeMarkerObjects& context)
{
    std::array<u32, 9> message{};
    message[0] = 0x4D;
    u16 id = first;
    while (id != 0)
    {
        const u8* row = chain_row(context.records, context.address_zero, id);
        for (size_t index = 0; index < 6; ++index)
        {
            message[2 + index] = word_at(row, 0xC + 4 * index);
        }
        message[1] = goal;
        message[8] = half_at(row, 8) != 0 ? 1 : 0;
        context.message(actor, message);
        id = half_at(row, 8);
    }
}

// Xbox 0x001349E0
FactoryObject* create_runtime_marker_actor(const char* name, const Vec3& position, const Vec3& euler, float scale,
    const char* second_override, const char* fourth_override, const char* fifth_override, u32 goal, u32 tag,
    const Vec3& goal_position, const Vec3& goal_euler, const u8* goal_names, u32 chained, u16 key,
    RuntimeMarkerObjects& context)
{
    FactoryObject* actor = keep(create_spawn_actor(context.factory, context.resources, context.records, name,
        position, euler, scale, nullptr, nullptr, second_override, fourth_override, fifth_override, i16(key)),
        context);
    if (actor != nullptr && goal != 0)
    {
        std::span<u8> bytes{actor->bytes.get(), actor->size};
        put_half(bytes, 0x990, u16(tag));
        if (goal_names != nullptr)
        {
            std::memcpy(bytes.data() + 0x998, goal_names, 0x200);
        }
        std::array<u32, 9> message{};
        message[0] = 0x4D;
        message[1] = goal;
        message[2] = std::bit_cast<u32>(goal_position.x);
        message[3] = std::bit_cast<u32>(goal_position.y);
        message[4] = std::bit_cast<u32>(goal_position.z);
        message[5] = std::bit_cast<u32>(goal_euler.x);
        message[6] = std::bit_cast<u32>(goal_euler.y);
        message[7] = std::bit_cast<u32>(goal_euler.z);
        message[8] = chained;
        context.message(*actor, message);
    }
    return actor;
}

// Xbox 0x000FF4F0
void create_runtime_marker_spawn_actor(const u8* record, RuntimeMarkerObjects& context)
{
    Vec3 goal_position{};
    Vec3 goal_euler{};
    const u8* self = find_runtime_marker(context.records, i16(half_at(record, 6)));
    if (self != nullptr && half_at(self, 8) != 0)
    {
        const u8* next = find_runtime_marker(context.records, i16(half_at(self, 8)));
        if (next != nullptr)
        {
            goal_position = vector_at(next, 0xC); // 0x0011F260.
        }
        if (next != nullptr)
        {
            goal_euler = vector_at(next, 0x18); // 0x0011F2A0.
        }
    }
    u32 chained = 0;
    if (word_at(record, 0x228) != 0 && half_at(record, 8) != 0)
    {
        const u8* next = chain_row(context.records, context.address_zero, half_at(record, 8));
        if (half_at(next, 8) != 0)
        {
            chained = 1;
        }
    }
    FactoryObject* actor = create_runtime_marker_actor(reinterpret_cast<const char*>(record + 0x28),
        vector_at(record, 0xC), vector_at(record, 0x18), float_at(record, 0x24), aid_or_null(record + 0xA8),
        aid_or_null(record + 0x128), aid_or_null(record + 0x1A8), word_at(record, 0x228), word_at(record, 0x22C),
        goal_position, goal_euler, record + 0x234, chained, half_at(record, 6), context);
    if (actor != nullptr && chained != 0)
    {
        const u8* next = chain_row(context.records, context.address_zero, half_at(record, 8));
        send_runtime_marker_actor_goals(half_at(next, 8), u16(word_at(record, 0x228)), *actor, context);
    }
}

// Xbox 0x001001A0
void create_runtime_marker_template(const u8* record, RuntimeMarkerObjects& context)
{
    auto clone = clone_executable_resource(context.resources, reinterpret_cast<const char*>(record + 0x28));
    if (clone.bytes == nullptr)
    {
        return;
    }
    std::span<u8> parameters{clone.bytes.get(), clone.size};
    for (size_t offset = 0xC; offset <= 0x24; offset += 4)
    {
        put_word(parameters, offset - 4, word_at(record, offset));
    }
    put_half(parameters, 0x2AA, half_at(record, 6));
    put_half(parameters, 0x2AC, half_at(record, 8));
    keep(create_factory_object(context.factory, parameters), context);
    clone.bytes.reset(); // 0x000663E0.
}

// Xbox 0x0006A010
void build_runtime_marker_trigger_parameters(std::span<u8, 0x65C> parameters, const Vec3& position, const Vec3& euler,
    u16 id, u16 next, u32 a2, u32 a3, u32 a4, u16 a5, const u8* block, u32 a7, u32 a8, const char* first_name,
    const char* second_name, float a11, u32 a12, float a13, u32 a14, const char* default_name)
{
    initialize_actor_effect_parameters(parameters.data(), default_name);
    put_vector(parameters, 8, position);
    put_vector(parameters, 0x14, euler);
    put_half(parameters, 0x2AC, next);
    put_half(parameters, 0x2AA, id);
    put_word(parameters, 0x544, a7);
    put_word(parameters, 0x334, a2);
    put_word(parameters, 0x338, a3);
    put_half(parameters, 0x340, a5);
    put_half(parameters, 0, 0x65C);
    put_half(parameters, 2, 0x6E);
    put_half(parameters, 4, 0x23);
    put_word(parameters, 0x33C, a4);
    put_word(parameters, 0x548, a8);
    copy_text(parameters, 0x54C, first_name);
    copy_text(parameters, 0x5CC, second_name);
    put_word(parameters, 0x64C, std::bit_cast<u32>(a11));
    put_word(parameters, 0x650, a12);
    put_word(parameters, 0x654, std::bit_cast<u32>(a13));
    std::memcpy(parameters.data() + 0x342, block, 0x200);
    put_word(parameters, 0x658, a14);
}

// Xbox 0x000FFAD0
void create_runtime_marker_trigger(const u8* record, RuntimeMarkerObjects& context)
{
    const float scale = std::bit_cast<float>(0x3C23D70Au); // DAT_0032B124.
    const char* first_name = reinterpret_cast<const char*>(record + 0x4C);
    const char* second_name = reinterpret_cast<const char*>(record + 0xCC);
    std::array<u8, 0x65C> parameters{};
    build_runtime_marker_trigger_parameters(parameters, vector_at(record, 0xC), vector_at(record, 0x18),
        half_at(record, 6), half_at(record, 8), word_at(record, 0x28), word_at(record, 0x2C), word_at(record, 0x30),
        half_at(record, 0x34), record + 0x14C, word_at(record, 0x38), word_at(record, 0x3C), first_name, second_name,
        float(i32(word_at(record, 0x40))) * scale, word_at(record, 0x44), float(i32(word_at(record, 0x48))) * scale,
        word_at(record, 0x34C), context.default_name);
    FactoryObject* trigger = keep(create_factory_object(context.factory, parameters), context);
    if (trigger != nullptr && float(i32(word_at(record, 0x40))) == 0.f && is_aid(record + 0x4C))
    {
        std::array<u32, 0x42> message{};
        message[0] = 0xA8;
        std::strcpy(reinterpret_cast<char*>(&message[1]), first_name);
        std::strcpy(reinterpret_cast<char*>(&message[0x21]), second_name);
        message[0x41] = 0;
        context.message(*trigger, message);
    }
}

// Xbox 0x0006E110
void build_runtime_marker_area_parameters(std::span<u8, 0x348> parameters, const Vec3& position, const Vec3& euler,
    u16 id, u16 next, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, const char* default_name)
{
    initialize_actor_effect_parameters(parameters.data(), default_name);
    put_vector(parameters, 8, position);
    put_vector(parameters, 0x14, euler);
    put_half(parameters, 0x2AA, id);
    put_half(parameters, 0x2AC, next);
    put_word(parameters, 0x33C, a4);
    put_word(parameters, 0x334, a2);
    put_half(parameters, 0, 0x348);
    put_half(parameters, 2, 0x6E);
    put_half(parameters, 4, 0x1F);
    put_word(parameters, 0x338, a3);
    put_word(parameters, 0x340, a5);
    put_word(parameters, 0x344, a6);
}

// Xbox 0x000FFDC0
void create_runtime_marker_area(const u8* record, RuntimeMarkerObjects& context)
{
    std::array<u8, 0x348> parameters{};
    build_runtime_marker_area_parameters(parameters, vector_at(record, 0xC), vector_at(record, 0x18),
        half_at(record, 6), half_at(record, 8), word_at(record, 0x28), word_at(record, 0x2C), word_at(record, 0x30),
        word_at(record, 0x34), word_at(record, 0x38), context.default_name);
    keep(create_factory_object(context.factory, parameters), context);
}
}
