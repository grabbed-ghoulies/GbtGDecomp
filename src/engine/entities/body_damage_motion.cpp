#include "body_damage_motion.h"
#include "mind_fields.h"
#include <cstddef>
#include "body_fields.h"
#include "player_mind_targets.h"
#include "player_mind_events.h"
#include "body_damage_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
float wrap_target_remainder(long double remainder)
{
    float relative = float(remainder);
    const float turn = std::bit_cast<float>(0x40C90FDBu);
    if (static_cast<long double>(std::bit_cast<float>(0xC0490FDBu)) > remainder)
    {
        relative += turn;
    }
    if (relative >= std::bit_cast<float>(0x40490FDBu))
    {
        relative -= turn;
    }
    return relative;
}
}

// Xbox 0x0001CC10
u32 runtime_body_target_direction(const Actor& actor, std::span<const u8> target, float heading)
{
    const Vec3 position = actor_world_position(actor);
    const Vec3 direction{mind_target_record(target).shape.position.x - position.x,
        mind_target_record(target).shape.position.y - position.y, mind_target_record(target).shape.position.z
            - position.z};
    float angle;
    runtime_direction_angle(direction, angle);
    const float reference = heading + std::bit_cast<float>(0x3F490FDBu);
    const float relative = wrap_target_remainder(animation_remainder(angle - reference,
        std::bit_cast<float>(0x40C90FDBu)));
    if (relative > std::bit_cast<float>(0x3FC90FDBu))
    {
        return 3;
    }
    if (relative >= 0.f)
    {
        return 0;
    }
    if (std::bit_cast<float>(0xBFC90FDBu) > relative)
    {
        return 2;
    }
    return 1;
}

// Xbox 0x00026830
i32 runtime_body_target_turn_direction(const Actor& actor,
    std::span<const u8> target, u32* within_angle)
{
    const Vec3 position = actor_world_position(actor);
    const Vec3 direction{mind_target_record(target).shape.position.x - position.x,
        mind_target_record(target).shape.position.y - position.y, mind_target_record(target).shape.position.z
            - position.z};
    float angle;
    runtime_direction_angle(direction, angle);
    const float difference = float(static_cast<long double>(angle) - actor.euler.y);
    const float relative = wrap_target_remainder(animation_remainder(difference,
        std::bit_cast<float>(0x40C90FDBu)));
    if (within_angle != nullptr)
    {
        *within_angle = static_cast<long double>(std::bit_cast<float>(0x43652EE1u))
            > animation_absolute(relative) ? 1u : 0u;
    }
    return relative > 0.f ? 1 : -1;
}

// Xbox 0x00041970
float runtime_actor_bound_angle(std::span<const u8> bound, const Actor& origin)
{
    const Vec3 position = actor_world_position(origin);
    const Vec3 direction{read_f32(bound, 0x20) - position.x,
        read_f32(bound, 0x24) - position.y, read_f32(bound, 0x28) - position.z};
    float angle;
    runtime_direction_angle(direction, angle);
    return angle;
}

// Xbox 0x001060A0
void runtime_horizontal_direction(Vec3& direction, float angle, float length)
{
    direction = {0.f, 0.f, 0.f};
    const float sine = float(retail_sin(angle));
    const long double cosine = retail_cos(angle);
    direction.x = float(static_cast<long double>(sine) * length + static_cast<long double>(direction.x));
    direction.z = float(cosine * length + static_cast<long double>(direction.z));
}

// Xbox 0x001060F0
void offset_runtime_horizontal_point(Vec3& output, const Vec3& source, float angle, float length)
{
    output = source;
    const float sine = float(retail_sin(angle));
    const long double cosine = retail_cos(angle);
    output.x = float(static_cast<long double>(sine) * length + output.x);
    output.z = float(cosine * length + output.z);
}

// Xbox 0x0001D540
float runtime_body_hit_angle(std::span<const u8> hit)
{
    if (hit.data() == nullptr)
    {
        return 0.f;
    }
    const float angle = read_f32(hit, 0x14) * std::bit_cast<float>(u32(0x3C8EFA35));
    if (angle == 0.f)
    {
        switch (hit[6])
        {
        case 0:
            return std::bit_cast<float>(u32(0x3FC90FDB));
        case 1:
            return 0.f;
        case 2:
            return std::bit_cast<float>(u32(0xBFC90FDB));
        case 3:
            return std::bit_cast<float>(u32(0x40490FDB));
        }
    }
    return angle;
}

// Xbox 0x00041900
float runtime_actor_angle_toward(const Actor& target, const Actor& origin)
{
    const Vec3 first = actor_world_position(target);
    const Vec3 second = actor_world_position(origin);
    const Vec3 direction{first.x - second.x, first.y - second.y, first.z - second.z};
    float angle;
    runtime_direction_angle(direction, angle);
    return angle;
}

// Xbox 0x00100FE0
float step_runtime_damage_angle(float current, float target, float step)
{
    const long double remainder = animation_remainder(target - current, std::bit_cast<float>(0x40C90FDBu));
    float difference = float(remainder);
    if (remainder < static_cast<long double>(std::bit_cast<float>(0xC0490FDBu)))
    {
        difference += std::bit_cast<float>(0x40C90FDBu);
    }
    if (difference >= std::bit_cast<float>(0x40490FDBu))
    {
        difference -= std::bit_cast<float>(0x40C90FDBu);
    }
    const float absolute = animation_absolute(difference);
    if (!(step > absolute))
    {
        if (0.f > difference)
        {
            target = current - step;
        }
        else
        {
            target = current + step;
        }
    }
    const long double result = animation_remainder(target, std::bit_cast<float>(0x40C90FDBu));
    float value = float(result);
    if (result < 0.L)
    {
        value += std::bit_cast<float>(0x40C90FDBu);
    }
    return value;
}

// Xbox 0x000246D0
void turn_runtime_body_toward_damage(Actor& actor, const Actor& source, std::span<const u8> attributes)
{
    if ((attributes.data() == nullptr || attributes[0x198] != 0) && !(actor.animation_pause > 0.f))
    {
        Vec3 euler = actor_world_euler(actor);
        const float angle = runtime_actor_angle_toward(source, actor);
        euler.y = step_runtime_damage_angle(euler.y, angle, std::bit_cast<float>(0x3F32B8C3u));
        normalize_euler_angles(actor.euler, euler);
        actor.xform_valid = 0;
    }
}

// Xbox 0x00022400
void push_runtime_body_from_damage(Actor& actor, u32 actor_address,
    const BodyDamage& damage, std::span<const u8> attributes, u32 key,
    RuntimeBodyAttachedActor* first_source, RuntimeBodyAttachedActor* second_source,
    const RuntimeBodyDamageTables& tables, std::span<const u8> keys)
{
    const float value =
        runtime_body_damage_push_amount(actor, damage.source_class, key, tables, keys);
    const float factor = attributes.data() == nullptr ? 1.f : read_f32(attributes, 0x19C);
    const float amount = factor * value;
    if (amount != 0.f)
    {
        const Vec3 direction = damage.direction;
        float angle;
        runtime_direction_angle(direction, angle);
        const float x = float(retail_sin(angle) * static_cast<long double>(amount));
        const float z = float(retail_cos(angle) * static_cast<long double>(amount));
        actor.anchor = {x, 0.f, z};
        if ((attributes.data() == nullptr || attributes[0x1A2] == 0) &&
            second_source != nullptr && second_source == first_source)
        {
            const std::array<u32, 5> message{0x60, actor_address, std::bit_cast<u32>(x), 0, std::bit_cast<u32>(z)};
            second_source->message(message);
        }
    }
}

// Xbox 0x00024830
void set_runtime_body_damage_motion(std::span<u8> body, const Vec3& direction,
    float first, float second, float third, float fourth)
{
    const u16 count = u16(body_record(body).unk_160 + 1);
    body_record(body).unk_160 = count;
    const u16 secondary = u16(body_record(body).unk_162 + 1);
    body_record(body).unk_162 = secondary;
    body_record(body).unk_220 = first;
    body_record(body).unk_224 = second;
    body_record(body).unk_228 = third;
    body_record(body).unk_22C = fourth;
    std::memcpy(body.data() + offsetof(BodyRecord, unk_230.x), &direction, 12);
}
}
