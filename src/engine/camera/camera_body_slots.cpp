#include "camera_body_slots.h"
#include <cstddef>
#include "../entities/body_fields.h"
#include "entity_camera.h"
#include "../entities/actor_transform.h"
#include "../entities/entity_collision.h"
#include "gbtg/anim.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
constexpr float kTurn = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
constexpr float kPi = std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
constexpr float kNegativePi = std::bit_cast<float>(0xC0490FDBu); // DAT_0032B29C.
constexpr float kRadians = std::bit_cast<float>(0x3C8EFA35u); // DAT_0032B0FC.
constexpr float kMinimumPush = std::bit_cast<float>(0x3D4CCCCDu); // DAT_0032B250.

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_vec3(std::span<u8> bytes, size_t offset, const Vec3& value)
{
    put_float(bytes, offset, value.x);
    put_float(bytes, offset + 4, value.y);
    put_float(bytes, offset + 8, value.z);
}

float wrap_component(float value)
{
    const long double remainder = animation_remainder(value, kTurn);
    float result = float(remainder);
    if (kNegativePi > remainder)
    {
        result = result + kTurn;
    }
    if (result >= kPi)
    {
        result = result - kTurn;
    }
    return result;
}

// Inline normalization: a nonzero square length scales each component by the x87 reciprocal square root.
void normalize_record_normal(entity_detail::SlideRecord& record, float squared)
{
    if (squared != 0.f)
    {
        const long double factor = retail_reciprocal_sqrt(squared);
        record.x = float(factor * record.x);
        record.y = float(factor * record.y);
        record.z = float(factor * record.z);
    }
}

void set_bit(EntityBitBlock& bits, size_t index, u8 mask, bool set)
{
    bits.bytes[index] = u8(set ? (bits.bytes[index] | mask) : (bits.bytes[index] & ~mask));
}
}

// Xbox 0x00105AA0
void wrap_runtime_euler(const Vec3& angles, Vec3& output)
{
    output.x = wrap_component(angles.x);
    output.y = wrap_component(angles.y);
    output.z = wrap_component(angles.z);
}

// Xbox 0x00072BA0
void aim_camera_body(RuntimeCameraBodySlots& camera)
{
    Actor& actor = camera.actor;
    const Vec3 world = actor_world_position(actor);
    if (body_record(camera.body).look_point_set == 0)
    {
        return;
    }
    float lift = 0.f;
    if (camera.look_profile != 4)
    {
        lift = camera.tunables.real(0x00434020);
    }
    const Vec3 aim = body_record(camera.body).look_point;
    float yaw = 0.f;
    float pitch = 0.f;
    direction_to_yaw_pitch(aim.x - world.x, lift + aim.y - world.y, aim.z - world.z, yaw, pitch);
    const Vec3 angles{0.f - (camera.tunables.real(0x0043401C) * kRadians + pitch), yaw + kPi,
        camera_body_record(camera.body).unk_8F4};
    wrap_runtime_euler(angles, actor.euler);
    actor.xform_valid = 0;
   body_record(camera.body).euler = actor_world_euler(actor);
}

// Xbox 0x00072B10
void capture_camera_body_pose(RuntimeCameraBodySlots& camera, RuntimeBodyApplyContext& apply)
{
    aim_camera_body(camera);
    const Vec3 position = actor_world_position(camera.actor);
    const Vec3 euler = actor_world_euler(camera.actor);
    camera_body_record(camera.body).view_position = position;
    camera_body_record(camera.body).view_euler = euler;
    // The native camera node is the record embedded at body +0x894.
    camera.view.position = position;
    camera.view.rotation.x = euler.x;
    camera.view.rotation.y = euler.y;
    camera.view.rotation.z = euler.z;
    apply_runtime_body(camera.body, apply);
}

// Xbox 0x00072CB0
u32 probe_camera_body_support(RuntimeCameraBodySlots& camera)
{
    Actor& actor = camera.actor;
    EntityBitBlock& bits = camera.bits;
    const bool saved = ((bits.bytes[2] >> 5) & 1) != 0;
    bits.bytes[1] = u8(bits.bytes[1] & 0xDF);
    Vec3 world = actor_world_position(actor);
    Vec3 slide = body_record(camera.body).position;
    Vec3 snap{};
    u32 seen = 0;
    u32 passed;
    i32 limit;
    if (camera.tunables.word(0x004274C8) != 0)
    {
        passed = 1;
        limit = 0;
        snap = slide;
    }
    else
    {
        passed = 0;
        limit = 3;
    }
    std::array<entity_detail::SlideRecord, 5> slots{};
    bool fold = false;
    for (i32 index = 0; index < limit; ++index)
    {
        entity_detail::SlideRecord& slot = slots[size_t(index)];
        snap = slide;
        const EntityMaskHost host{actor.mask_flag, actor.goal_ring, actor.mask_mode};
        if (entity_record_movement_probe(snap, world, camera.tunables.word(0x004274B8), 4,
                camera_collision_mask(host)) == 0)
        {
            passed = 1;
            fold = seen != 0;
            break;
        }
        entity_copy_match_block(&slot, entity_detail::g_segment_slots[0].pool.begin);
        seen = 1;
        if (index == 2 && entity_match_blocks_equal(slots[0].match, slots[2].match) != 0)
        {
            slot.x = slot.x + slots[1].x;
            slot.y = slot.y + slots[1].y;
            slot.z = slot.z + slots[1].z;
            normalize_record_normal(slot, slot.x * slot.x + slot.y * slot.y + slot.z * slot.z);
            set_bit(bits, 1, 0x20, true);
            limit = 5;
        }
        entity_copy_match_block(camera.body.data() + offsetof(BodyRecord, last_contact), &slot);
        if (index == 0 && 1.f > engine_absolute(slot.y)) // DAT_0032B0F8.
        {
            slot.y = 0.f;
            normalize_record_normal(slot, slot.x * slot.x + 0.f * 0.f + slot.z * slot.z);
        }
        const float along = slot.x * ((slide.x - world.x) - (snap.x - world.x))
            + ((slide.z - world.z) - (snap.z - world.z)) * slot.z
            + ((slide.y - world.y) - (snap.y - world.y)) * slot.y;
        float push = 0.f - along;
        if (kMinimumPush > push)
        {
            push = kMinimumPush;
        }
        else if (0.f > push)
        {
            continue;
        }
        slide.x = slot.x * push + slide.x;
        slide.y = push * slot.y + slide.y;
        slide.z = push * slot.z + slide.z;
    }
    if (fold)
    {
        const Vec3 start = body_record(camera.body).position;
        const float separation = (snap.z - world.z) * (start.z - world.z)
            + (snap.y - world.y) * (start.y - world.y)
            + (start.x - world.x) * (snap.x - world.x);
        if (0.f > separation || 0.f > camera_body_record(camera.body).separation)
        {
            passed = 0;
        }
        camera_body_record(camera.body).separation = separation;
    }
    if (passed == 0)
    {
        set_bit(bits, 2, 0x20, saved);
        return passed;
    }
    set_bit(bits, 2, 0x20, seen != 0);
    actor_set_world_position(actor, snap);
    // Actor +0x95C is this body; its +8 is the actor.
   body_record(camera.body).position = actor_world_position(actor);
    return passed;
}
}
