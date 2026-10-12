#include "camera_mind_rig.h"
#include "display_setup_math.h"
#include "display_view.h"
#include "../entities/actor_transform.h"
#include "../entities/body_damage_motion.h"
#include "../entities/body_gaze.h"
#include "../entities/entity_collision.h"
#include "../entities/player_mind_events.h"
#include "../entities/shape_copy.h"
#include "../navigation/cell_reach.h"
#include "../navigation/navigation.h"
#include "gbtg/sound_runtime.h"
#include "gbtg/anim.h"
#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
constexpr float kTurn = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
constexpr float kPi = std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
constexpr float kNegativePi = std::bit_cast<float>(0xC0490FDBu); // DAT_0032B29C.
constexpr float kHalfPi = std::bit_cast<float>(0x3FC90FDBu); // DAT_0032B140.
constexpr float kOne = 1.f; // DAT_0032B0F8.
constexpr float kNegativeOne = -1.f; // DAT_0032B120.
constexpr float kHalf = 0.5f; // DAT_0032B0F4.
constexpr float kRadians = std::bit_cast<float>(0x3C8EFA35u); // DAT_0032B0FC.
constexpr float kHundredth = std::bit_cast<float>(0x3C23D70Au); // DAT_0032B124.
constexpr float kDeadZone = std::bit_cast<float>(0x3E4CCCCDu); // DAT_0032B1AC.
constexpr float kTurnGain = 1.25f; // DAT_0032B114.
constexpr float kFloorMargin = 2.f; // DAT_0032B110.
constexpr float kReach = 100.f; // DAT_0032B12C.

float real(std::span<const u8> bytes, size_t offset)
{
    return read_f32(bytes, offset);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

Vec3 record_position(std::span<const u8> record)
{
    return Vec3{real(record, 4), real(record, 8), real(record, 0xC)};
}

// The inline wrap after 0x00152BE0: a remainder below -pi (compared in x87) gains a turn; one at or
// above pi loses a turn.
float wrap_angle(float value)
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

const RuntimeDialogueInputDevice* actor_input_device(const RuntimeDirectorActor& actor,
    const RuntimeCameraRigContext& context)
{
    const u32 address = read_u32(actor.bytes, 0x960);
    if (address == 0)
    {
        return nullptr;
    }
    return context.input_device(address);
}

const EntityCamNode& active_camera()
{
    if (g_cam_active[0] == nullptr)
    {
        throw Error("camera rig reads the active camera DAT_004E16A0 through address zero");
    }
    return *g_cam_active[0];
}
}

float RuntimeCameraRigTunables::real(u32 address) const
{
    float value;
    if (address >= 0x004274B8 && address + 4 <= 0x00427540)
    {
        std::memcpy(&value, rig.data() + (address - 0x004274B8), 4);
        return value;
    }
    if (address >= 0x00434008 && address + 4 <= 0x00434034)
    {
        std::memcpy(&value, view.data() + (address - 0x00434008), 4);
        return value;
    }
    throw Error("camera rig tunable outside DAT_004274B8 and DAT_00434008");
}

u32 RuntimeCameraRigTunables::word(u32 address) const
{
    return std::bit_cast<u32>(real(address));
}

// Xbox 0x0013DC40
float camera_input_turn(const RuntimeDirectorActor& actor, const RuntimeCameraRigContext& context)
{
    const RuntimeDialogueInputDevice* device = actor_input_device(actor, context);
    if (device == nullptr || read_u32(device->record, 4) == 0)
    {
        return 0.f;
    }
    const float first = runtime_input_first_analog_level(*device, context.input);
    const float second = runtime_input_second_analog_level(*device, context.input);
    float turn;
    if (first > kDeadZone)
    {
        turn = second > kDeadZone ? 0.f : 0.f - first;
    }
    else
    {
        turn = second > kDeadZone ? second : 0.f;
    }
    const float magnitude = float(engine_absolute(turn));
    float sign = kOne;
    if (!(turn >= 0.f))
    {
        sign = kNegativeOne;
    }
    if (magnitude > kDeadZone)
    {
        return (magnitude - kDeadZone) * kTurnGain * sign;
    }
    return 0.f;
}

// Xbox 0x0013DD00
float camera_input_turn_step(const RuntimeDirectorActor& actor, const Actor& camera,
    const RuntimeCameraRigContext& context)
{
    float turn = camera_input_turn(actor, context);
    if ((context.view_flags & 2) == 0)
    {
        turn = turn * kNegativeOne;
    }
    if (context.invert != 0)
    {
        turn = turn * kNegativeOne;
    }
    const float step = runtime_frame_step(context.clock);
    return step * runtime_actor_rate(camera) * turn;
}

// Xbox 0x0013D110
float camera_floor_height(const RuntimeDirectorActor& actor, const Vec3& point, const Vec3& reference,
    float margin)
{
    float result = point.y;
    const i32 floor = i32(read_u32(actor.bytes, 0xCEC));
    const i32 cell = navigation_cell_index(floor, point);
    if (cell == -1)
    {
        return result;
    }
    const auto& cells = entity_detail::g_nav_grids[floor]->cells;
    const float bottom = float(cells.at(size_t(cell)).bottom);
    const i32 upper = navigation_cell_index(floor, point);
    float top;
    if (upper == -1)
    {
        top = bottom - kOne;
    }
    else
    {
        top = float(cells.at(size_t(upper)).top);
        if (reference.y > top)
        {
            top = bottom - kOne;
        }
    }
    if (top > bottom && margin * kFloorMargin > top - bottom)
    {
        return (top + bottom) * kHalf;
    }
    if (margin > point.y - bottom)
    {
        result = bottom + margin;
    }
    if (top > bottom && margin > top - point.y)
    {
        result = top - margin;
    }
    return result;
}

// Xbox 0x001062A0
void offset_camera_point(Vec3& point, float angle, float distance)
{
    const float sine = float(retail_sin(angle));
    const long double cosine = retail_cos(angle);
    point.x = float(double(sine) * distance + point.x);
    point.z = float(cosine * distance + point.z);
}

// Xbox 0x00101550
float clamp_camera_heading(float heading, float first, float second)
{
    const float to_first = wrap_angle(first - heading);
    const float to_second = wrap_angle(first - second);
    if (to_second > 0.f)
    {
        if (0.f > to_first)
        {
            return first;
        }
        if (to_first > to_second)
        {
            return second;
        }
        return heading;
    }
    if (to_first > 0.f)
    {
        return first;
    }
    if (to_second > to_first)
    {
        return second;
    }
    return heading;
}

// Xbox 0x0013CE80
void aim_camera_offset(Vec3& output, const Vec3& from, const Vec3& to, u32 actor, float range, float near_distance,
    float far_distance, const RuntimeCameraRigContext& context)
{
    const double dx = double(from.x) - to.x;
    const double dy = double(from.y) - to.y;
    const double dz = double(from.z) - to.z;
    const float span = float(std::sqrt(dy * dy + dx * dx + dz * dz));
    output.x = to.x - from.x;
    output.y = to.y - from.y;
    output.z = to.z - from.z;
    const float boost = context.tunables.real(0x00434028);
    float scaled_range = range;
    float low = near_distance;
    float high = far_distance;
    if (boost > 0.f && actor != 0)
    {
        const RuntimeDirectorActor view = context.actor_at(actor);
        if ((view.body_bits->bytes[4] & 0x10) != 0)
        {
            scaled_range = boost * range;
            low = boost * near_distance;
            high = boost * far_distance;
        }
    }
    const float lift = context.tunables.real(0x00434024);
    if (lift > 0.f)
    {
        output.y = output.y * lift;
    }
    float length;
    if (scaled_range == 0.f)
    {
        length = high;
    }
    else
    {
        const float blend = span / scaled_range * (high - low) + low;
        if (high > low)
        {
            length = blend > high ? high : (low > blend ? low : blend);
        }
        else
        {
            length = high > blend ? high : (blend > low ? low : blend);
        }
    }
    const float squared = output.x * output.x + output.z * output.z + output.y * output.y;
    if (squared != 0.f)
    {
        const long double factor = retail_reciprocal_sqrt(squared) * length;
        output.x = float(factor * output.x);
        output.y = float(factor * output.y);
        output.z = float(factor * output.z);
    }
}

// Xbox 0x000783A0
u32 frame_camera_target(const Vec3& from, std::span<const u8, 0x20> record, Vec3& point, float& heading,
    float& distance, float& rise, const RuntimeCameraRigContext& context)
{
    const Vec3 position = record_position(record);
    const Vec3 offset{from.x - position.x, from.y - position.y, from.z - position.z};
    if (!runtime_direction_angle(offset, heading))
    {
        return 0;
    }
    const float half_height = real(record, 0x14) * kHalf;
    rise = half_height;
    float vertical = 0.f;
    const i32 vertical_pixels = i32(context.tunables.word(0x00434010));
    if (vertical_pixels != 0)
    {
        if (context.render_height == 0)
        {
            throw Error("camera rig needs the display records 0x000F5900 initializes (DAT_0054DD80 is 0)");
        }
        vertical = active_camera().focal_pixels * half_height
            / (float(vertical_pixels) * kHundredth * float(context.render_height / 2));
    }
    float horizontal = 0.f;
    const i32 horizontal_pixels = i32(context.tunables.word(0x004274E4));
    if (horizontal_pixels != 0)
    {
        if (context.render_width == 0)
        {
            throw Error("camera rig needs the display records 0x000F5900 initializes (DAT_0054DD84 is 0)");
        }
        horizontal = active_camera().focal_pixels * active_camera().pixel_aspect * real(record, 0x1C)
            / (float(horizontal_pixels) * kHundredth * float(context.render_width / 2));
    }
    if (!(horizontal >= vertical))
    {
        horizontal = vertical;
    }
    distance = horizontal;
    point = position;
    point.y = point.y + half_height;
    return 1;
}

// Xbox 0x000784B0
float camera_stick_turn(const RuntimeDirectorActor& target, std::span<const u8> mind,
    const RuntimeCameraRigContext& context)
{
    float speed = context.tunables.real(0x00434030);
    if (!(speed >= 0.f) || target.mind.empty() || target.mind[0x20] == 0)
    {
        speed = context.tunables.real(0x0043402C);
    }
    if (!(speed > 0.f))
    {
        return 0.f;
    }
    const RuntimeDialogueInputDevice* device = context.input_device(read_u32(target.bytes, 0x960));
    if (read_u32(device->record, 4) == 0 || (target.body_bits->bytes[6] & 0x80) == 0)
    {
        return 0.f;
    }
    std::array<float, 2> axes{};
    runtime_input_axes(*device, axes, context.input);
    const float tilt = float(retail_square_root(axes[1] * axes[1] + axes[0] * axes[0]));
    const Actor& actor = *target.actor;
    const long double motion = retail_square_root(actor.anchor.x * actor.anchor.x + actor.anchor.z * actor.anchor.z);
    if (kHundredth > motion)
    {
        speed = 0.f;
    }
    float angle = 0.f;
    if (axes[0] != 0.f || axes[1] != 0.f)
    {
        angle = float(retail_atan2(axes[1], axes[0]));
    }
    // ECX is the camera actor at mind +8; its +0x18 is the yaw.
    const float yaw = context.actor_at(read_u32(mind, 8)).actor->euler.y;
    angle = runtime_relative_angle(angle, yaw);
    if (angle > kHalfPi)
    {
        angle = kPi - angle;
    }
    else if (std::bit_cast<float>(0xBFC90FDBu) > angle) // DAT_0032B13C.
    {
        angle = kNegativePi - angle;
    }
    return 0.f - angle * tilt * speed;
}

// Xbox 0x00078630
void ease_camera_heading(std::span<u8> mind, const Actor& camera, float& heading, const RuntimeDirectorActor& target,
    const RuntimeCameraRigContext& context)
{
    if (read_u32(mind, 0xAC) != 0)
    {
        const float step = runtime_frame_step(context.clock);
        const float rate = runtime_actor_rate(camera);
        const float limit = float(double(real(mind, 0xD0)) * kRadians * rate * step);
        put_float(mind, 0xB8, step_runtime_damage_angle(heading, real(mind, 0xB4), limit));
        const float offset = wrap_angle(float(double(real(mind, 0xB4)) - heading));
        bool settled = kRadians > engine_absolute(offset);
        if (!settled)
        {
            const float hold = real(mind, 0xBC);
            if (hold > 0.f)
            {
                const float remaining = hold - runtime_actor_rate(camera) * runtime_frame_step(context.clock);
                put_float(mind, 0xBC, remaining);
                if (!(0.f < remaining))
                {
                    put_float(mind, 0xBC, 0.f);
                    settled = true;
                }
            }
        }
        if (settled)
        {
            put(mind, 0xAC, 0);
            put(mind, 0xB0, 1);
        }
        heading = clamp_camera_heading(heading, real(mind, 0xB4), real(mind, 0xB8));
        return;
    }
    if (0.f > context.tunables.real(0x00434030) && !target.mind.empty() && target.mind[0x20] != 0)
    {
        if (read_u32(mind, 0xC4) == 0)
        {
            put_float(mind, 0xB8, heading);
        }
        else
        {
            heading = real(mind, 0xB8);
        }
        put(mind, 0xC4, 1);
        return;
    }
    put(mind, 0xC4, 0);
}

// Xbox 0x0013D640
void merge_camera_targets(std::span<const u8> records, i32 count, const Vec3& eye, float yaw,
    std::span<u8, 0x20> output)
{
    if (count == 1)
    {
        copy_actor_shape(output.data(), records.data());
        return;
    }
    const size_t end = size_t(count) * 0x20;
    if (records.size() < end)
    {
        throw Error("camera target records are shorter than their count");
    }
    const auto position = [&records](size_t offset)
    {
        return record_position(records.subspan(offset, 0x20));
    };
    float base = 0.f;
    float angle = 0.f;
    const Vec3 first = position(0);
    if (runtime_direction_angle(Vec3{eye.x - first.x, eye.y - first.y, eye.z - first.z}, angle))
    {
        base = wrap_angle(angle - yaw);
    }
    float low_angle = base;
    float high_angle = base;
    float low_sine = float(retail_sin(base));
    float high_sine = low_sine;
    size_t low_record = 0;
    size_t high_record = 0;
    for (size_t offset = 0x20; offset < end; offset += 0x20)
    {
        const Vec3 at = position(offset);
        if (!runtime_direction_angle(Vec3{eye.x - at.x, eye.y - at.y, eye.z - at.z}, angle))
        {
            continue;
        }
        const float relative = wrap_angle(angle - yaw);
        const long double sine = retail_sin(relative);
        if (low_sine > sine)
        {
            low_angle = relative;
            low_sine = float(sine);
            low_record = offset;
        }
        else if (float(sine) > high_sine)
        {
            high_angle = relative;
            high_sine = float(sine);
            high_record = offset;
        }
    }
    const float middle = (high_angle + low_angle) * kHalf + yaw;
    const float half_spread = wrap_angle(high_angle - low_angle) * kHalf;
    const float back = float(double(middle) - kPi);
    const float near_x = float(retail_sin(back) * kReach + eye.x);
    const float near_z = float(retail_cos(back) * kReach + eye.z);
    const Vec3 low = position(low_record);
    const Vec3 high = position(high_record);
    const float cross = high.x * low.z - low.x * high.z;
    const float across_x = low.x - high.x;
    const float across_z = high.z - low.z;
    const float toward_x = eye.x - near_x;
    const float toward_z = near_z - eye.z;
    const float determinant = toward_x * across_z - toward_z * across_x;
    const float line = float(double(eye.z) * near_x - double(near_z) * eye.x);
    if (determinant == 0.f)
    {
        copy_actor_shape(output.data(), records.data() + low_record);
        return;
    }
    const float inverse = kOne / determinant;
    const float meet_z = (toward_z * cross - line * across_z) * inverse;
    const float meet_x = (line * across_x - toward_x * cross) * inverse;
    const float back_z = eye.z - meet_z;
    const float back_x = eye.x - meet_x;
    const float reach = float(retail_square_root(back_z * back_z + back_x * back_x));
    float width = float(display_tangent(half_spread) * reach);
    if (0.f > width)
    {
        width = 0.f - width;
    }
    const auto elevation = [&records, &eye](size_t offset, float& top, float& bottom)
    {
        const std::span<const u8> record = records.subspan(offset, 0x20);
        const float dz = real(record, 0xC) - eye.z;
        const float dx = real(record, 4) - eye.x;
        const float flat = float(retail_square_root(dz * dz + dx * dx));
        top = float(retail_atan2(real(record, 0x14) + real(record, 8) - eye.y, flat));
        bottom = float(retail_atan2(real(record, 8) - eye.y, flat));
    };
    float top_angle = 0.f;
    float bottom_angle = 0.f;
    elevation(0, top_angle, bottom_angle);
    for (size_t offset = 0x20; offset < end; offset += 0x20)
    {
        float top = 0.f;
        float bottom = 0.f;
        elevation(offset, top, bottom);
        if (top > top_angle)
        {
            top_angle = top;
        }
        if (bottom_angle > bottom)
        {
            bottom_angle = bottom;
        }
    }
    const float top_height = float(display_tangent(top_angle) * reach);
    const float bottom_height = float(display_tangent(bottom_angle) * reach);
    output[0] = 4;
    output[1] = 0x20;
    output[2] = 1;
    const std::span<u8> bytes{output.data(), 0x20};
    put_float(bytes, 4, meet_x);
    put_float(bytes, 8, eye.y + bottom_height);
    put_float(bytes, 0xC, meet_z);
    put_float(bytes, 0x10, 0.f);
    put_float(bytes, 0x14, float(double(top_height) - bottom_height));
    put_float(bytes, 0x18, 0.f);
    put_float(bytes, 0x1C, width);
}

// Xbox 0x00078840
void place_camera_rig(std::span<u8> mind, const Actor& camera, std::span<const u8> request, const Vec3& eye,
    Vec3& position, Vec3& focus, u32 target, float& distance, const RuntimeCameraRigContext& context)
{
    const Vec3 euler = actor_world_euler(camera);
    const i32 count = i32(read_u32(request, 0x294));
    if (count < 1)
    {
        position = eye;
        return;
    }
    std::array<u8, 0x20> merged{};
    merge_camera_targets(request.subspan(0x14), count, eye, euler.y, merged);
    const RuntimeDirectorActor watched = context.actor_at(target);
    float heading = 0.f;
    float rise = 0.f;
    Vec3 probe;
    if (frame_camera_target(eye, merged, focus, heading, distance, rise, context) != 0)
    {
        const float dz = eye.z - focus.z;
        const float dx = eye.x - focus.x;
        const float planar = float(retail_square_root(dz * dz + dx * dx));
        if (read_u32(mind, 0xB0) != 0)
        {
            put_float(mind, 0xB8, heading);
            put(mind, 0xB0, 0);
        }
        heading = camera_stick_turn(watched, mind, context) + heading;
        ease_camera_heading(mind, camera, heading, watched, context);
        if (distance > real(mind, 0xC8) && distance > planar)
        {
            distance = planar > real(mind, 0xC8) ? planar : real(mind, 0xC8);
            if (context.tunables.real(0x0042753C) > distance)
            {
                distance = context.tunables.real(0x0042753C);
            }
        }
        else
        {
            put_float(mind, 0xC8, distance);
        }
        position = focus;
        const float reach = distance;
        const float sine = float(retail_sin(heading));
        const float cosine = float(retail_cos(heading));
        position.x = sine * reach + position.x;
        position.z = cosine * reach + position.z;
        if (context.tunables.word(0x004274E8) != 0)
        {
            if (context.tunables.word(0x004274EC) != 0)
            {
                float pitch = context.tunables.real(0x004274D8);
                if (context.director.skip_candidates != 0 || count > 1)
                {
                    pitch = context.tunables.real(0x00434008);
                }
                position.y = float(display_tangent(pitch * kRadians) * distance + position.y);
            }
            else
            {
                position.y = position.y + context.tunables.real(0x00434014);
            }
        }
        if (context.tunables.real(0x0042753C) > planar)
        {
            const float floor = focus.y + rise;
            if (context.tunables.real(0x004274F0) > position.y - floor)
            {
                position.y = context.tunables.real(0x004274F0) + floor;
            }
        }
        probe = position;
        const float frame = runtime_actor_rate(camera) * runtime_frame_step(context.clock);
        if (frame > 0.f)
        {
            const float turn = camera_input_turn_step(watched, camera, context) / frame;
            const float magnitude = engine_absolute(turn);
            float sign = kOne;
            if (!(turn >= 0.f))
            {
                sign = kNegativeOne;
            }
            if (context.tunables.real(0x00427518) > 0.f)
            {
                offset_camera_point(position, sign * kHalfPi + heading,
                    context.tunables.real(0x00427518) * magnitude);
            }
        }
        put(mind, 0xB0, 1);
    }
    else
    {
        position = eye;
        probe = position;
    }
    i32 last_cell = 0;
    if (!navigation_path_clear(eye, probe, 1, 0, 0, 0, nullptr, &last_cell, 0))
    {
        probe.x = eye.x;
        probe.z = eye.z;
    }
    const float floor = camera_floor_height(watched, probe, focus, context.tunables.real(0x004274F0));
    put(mind, 0xD8, position.y > floor ? 1 : 0);
    position.y = floor;
}

// Xbox 0x000791A0
void set_camera_heading_goal(std::span<u8> mind, float heading, float hold, u32 keep_side, float speed, u32 tag)
{
    const float current = wrap_angle(float(double(real(mind, 0xB4)) - real(mind, 0xB8)));
    const float wanted = wrap_angle(float(double(heading) - real(mind, 0xB8)));
    const u32 active = read_u32(mind, 0xAC);
    if (keep_side != 0 && active != 0)
    {
        const float current_side = current >= 0.f ? kOne : kNegativeOne;
        const float wanted_side = wanted >= 0.f ? kOne : kNegativeOne;
        if (current_side != wanted_side)
        {
            return;
        }
        if (!(engine_absolute(current) > engine_absolute(wanted)))
        {
            return;
        }
    }
    put_float(mind, 0xB4, heading);
    put_float(mind, 0xBC, hold);
    put(mind, 0xAC, 1);
    put_float(mind, 0xD0, speed);
    put(mind, 0xB0, active == 0 ? 1 : 0);
    put(mind, 0xD4, tag);
}

namespace
{
u32 camera_body_mask(const Actor& camera)
{
    const EntityMaskHost host{camera.mask_flag, camera.mask_span, camera.mask_mode};
    return camera_collision_mask(host);
}

Vec3 first_contact_normal()
{
    const u8* record = entity_detail::g_segment_slots[0].pool.begin;
    return Vec3{read_f32({record, 0x18}, 0xC), read_f32({record, 0x18}, 0x10), read_f32({record, 0x18}, 0x14)};
}
}

// Xbox 0x00078C70
i32 slide_camera_eye(RuntimeCameraMindView& view, const Vec3& normal, const Vec3& start, const Vec3& eye,
    Vec3& center, float& heading, Vec3& slid, const RuntimeCameraRigContext& context)
{
    Vec3 direction{start.x - eye.x, start.y - eye.y, start.z - eye.z};
    const float squared = direction.z * direction.z + direction.y * direction.y + direction.x * direction.x;
    if (squared != 0.f)
    {
        const long double factor = retail_reciprocal_sqrt(squared);
        direction.x = float(direction.x * factor);
        direction.y = float(direction.y * factor);
        direction.z = float(direction.z * factor);
    }
    const float facing = normal.z * direction.z + normal.y * direction.y + normal.x * direction.x;
    if (!(facing > 0.f))
    {
        return 0;
    }
    Vec3 moved = eye;
    Vec3 push = normal;
    if (1.f > engine_absolute(normal.y)) // DAT_0032B0F8.
    {
        const float flat = push.z * push.z + push.x * push.x;
        push.y = 0.f;
        if (flat != 0.f)
        {
            const long double factor = retail_reciprocal_sqrt(flat);
            push.x = float(push.x * factor);
            push.y = float(0.0 * factor);
            push.z = float(push.z * factor);
        }
    }
    const float along = ((moved.z - center.z) - (start.z - center.z)) * push.z
        + ((moved.x - center.x) - (start.x - center.x)) * push.x
        + ((moved.y - center.y) - (start.y - center.y)) * push.y;
    float amount = 0.f - along;
    constexpr float kMinimumPush = std::bit_cast<float>(0x3D4CCCCDu); // DAT_0032B250.
    bool apply = true;
    if (kMinimumPush > amount)
    {
        amount = kMinimumPush;
    }
    else if (0.f > amount)
    {
        apply = false;
    }
    if (apply)
    {
        moved.x = amount * push.x + moved.x;
        moved.y = amount * push.y + moved.y;
        moved.z = push.z * amount + moved.z;
    }
    const Vec3 travel{moved.x - start.x, moved.y - start.y, moved.z - start.z};
    if (!runtime_direction_angle(travel, heading))
    {
        return 0;
    }
    float toward = 0.f;
    if (runtime_direction_angle(Vec3{eye.x - center.x, eye.y - center.y, eye.z - center.z}, toward))
    {
        const float side = runtime_relative_angle(heading, toward) >= 0.f ? kOne : kNegativeOne;
        heading = context.tunables.real(0x00427534) * kRadians * side + heading;
    }
    const float dz = eye.z - center.z;
    const float dx = eye.x - center.x;
    const float reach = float(retail_square_root(dz * dz + dx * dx));
    offset_runtime_horizontal_point(slid, center, heading, reach);
    i32 last_cell = 0;
    u32 first;
    if (context.tunables.word(0x00427528) != 0)
    {
        first = entity_record_movement_probe(slid, center, context.tunables.word(0x004274B8), 4,
            camera_body_mask(view.camera)) == 0 ? 1 : 0;
    }
    else
    {
        first = navigation_path_clear(center, slid, i32(context.tunables.word(0x00427524)), 0, 0, 0, nullptr,
            &last_cell, 0) ? 1 : 0;
    }
    u32 second = first;
    if (context.tunables.word(0x00427530) != 0)
    {
        second = navigation_path_clear(eye, slid, i32(context.tunables.word(0x0042752C)), 0, 0, 0, nullptr,
            &last_cell, 0) ? 1 : 0;
    }
    if (first != 0)
    {
        return second != 0 ? 1 : 0;
    }
    return second != 0 ? 0 : -1;
}

// Xbox 0x000790B0
u32 sweep_camera_eye(RuntimeCameraMindView& view, const Vec3& eye, Vec3& start, Vec3& center, float& heading,
    i32 passes, const RuntimeCameraRigContext& context)
{
    Vec3 slid = start;
    if (passes <= 0)
    {
        return 0;
    }
    i32 pass = 0;
    do
    {
        if (entity_record_sweep_probe(slid, center, 4, camera_body_mask(view.camera)) == 0)
        {
            return 0;
        }
        if (pass == 0)
        {
            start = slid;
        }
        const i32 result = slide_camera_eye(view, first_contact_normal(), start, eye, center, heading, slid, context);
        if (result == 1)
        {
            return 1;
        }
        if (result == 0)
        {
            return 0;
        }
        ++pass;
    } while (pass < passes);
    return 0;
}

// Xbox 0x00079320
void choose_camera_heading(RuntimeCameraMindView& view, std::span<const u8> request, const Vec3& eye,
    const RuntimeCameraRigContext& context)
{
    if (read_u32(request, 0x298) != 0)
    {
        set_camera_heading_goal(view.mind, real(request, 0x29C), context.tunables.real(0x004274FC), 0,
            context.tunables.real(0x004274F8), 1);
        return;
    }
    if (read_u32(request, 0x294) == 0)
    {
        return;
    }
    const std::span<const u8, 0x20> record{request.data() + 0x14, 0x20};
    if (navigation_record_reaches_point(record, eye, i32(context.tunables.word(0x0042751C)),
            i32(context.tunables.word(0x00427520)), 1) != 0)
    {
        put(view.mind, 0xCC, 1);
        return;
    }
    Vec3 center{real(record, 4), real(record, 0x14) * kHalf + real(record, 8), real(record, 0xC)};
    Vec3 start = eye;
    float heading = 0.f;
    if (sweep_camera_eye(view, eye, start, center, heading, i32(context.tunables.word(0x00427538)), context) != 0)
    {
        set_camera_heading_goal(view.mind, heading, context.tunables.real(0x00427514), 1,
            context.tunables.real(0x004274F4), 0);
    }
    put(view.mind, 0xCC, 0);
}

// Xbox 0x00079450
u32 settle_camera_position(RuntimeCameraMindView& view, Vec3& eye, u32 target, Vec3& position,
    Vec3& focus, std::span<u8, 0x54> contact, const RuntimeCameraRigContext& context)
{
    const u32 probe = context.tunables.word(0x004274B8);
    Vec3 kept{focus.x, position.y, focus.z};
    float height = position.y;
    u32 lifted = 0;
    if (entity_record_movement_probe(kept, focus, probe, 4, camera_body_mask(view.camera)) != 0)
    {
        height = kept.y;
        lifted = 1;
    }
    Vec3 above{focus.x, height, focus.z};
    kept = position;
    u32 result = 0;
    if (entity_record_movement_probe(kept, above, probe, 4, camera_body_mask(view.camera)) != 0)
    {
        entity_copy_match_block(contact.data(), entity_detail::g_segment_slots[0].pool.begin);
        result = 1;
        bool blocked = false;
        if (read_u32(view.mind, 0xAC) == 0 || read_u32(view.mind, 0xD4) == 0)
        {
            const RuntimeDirectorActor watched = context.actor_at(target);
            if (camera_input_turn(watched, context) == 0.f)
            {
                const i32 floor = i32(read_u32(watched.bytes, 0xCEC));
                const i32 cell = navigation_cell_index(floor, position);
                if (cell != -1
                    && !entity_detail::navigation_cell_has_no_unblocked_neighbor(*entity_detail::g_nav_grids[floor],
                        u32(cell)))
                {
                    if (entity_record_movement_probe(above, eye, probe, 4, camera_body_mask(view.camera)) != 0)
                    {
                        kept = position;
                        if (entity_record_movement_probe(kept, above, probe, 4, camera_body_mask(view.camera)) == 0)
                        {
                            result = 0;
                            blocked = true;
                        }
                        else
                        {
                            entity_copy_match_block(contact.data(), entity_detail::g_segment_slots[0].pool.begin);
                            result = 1;
                        }
                    }
                }
            }
        }
        if (!blocked)
        {
            position = kept;
        }
    }
    if (lifted != 0)
    {
        const float settled_z = position.z - focus.z;
        const float settled_x = position.x - focus.x;
        const float settled = float(retail_square_root(settled_z * settled_z + settled_x * settled_x));
        const float eye_z = eye.z - focus.z;
        const float eye_x = eye.x - focus.x;
        const float from_eye = float(retail_square_root(eye_z * eye_z + eye_x * eye_x));
        const float limit = context.tunables.real(0x0042753C);
        if (limit > settled && limit > from_eye && from_eye > settled)
        {
            position = eye;
        }
    }
    return result;
}

// Xbox 0x00079720
void track_camera_obstruction(RuntimeCameraMindView& view, Vec3& eye, Vec3& position, Vec3& focus,
    u32 target, float distance, const RuntimeCameraRigContext& context)
{
    std::array<u8, 0x54> contact{};
    if (settle_camera_position(view, eye, target, position, focus, contact, context) != 0)
    {
        const float dz = eye.z - focus.z;
        const float dx = eye.x - focus.x;
        const float planar = float(retail_square_root(dz * dz + dx * dx));
        if (context.tunables.word(0x004274C4) != 0 && target != 0)
        {
            if (camera_input_turn(context.actor_at(target), context) != 0.f)
            {
                return;
            }
        }
        if (!(planar < context.tunables.real(0x004274C0)))
        {
            return;
        }
        float angle = 0.f;
        const Vec3 wall{read_f32(contact, 0xC), read_f32(contact, 0x10), read_f32(contact, 0x14)};
        if (!runtime_direction_angle(wall, angle))
        {
            return;
        }
        const float side = runtime_relative_angle(angle, real(view.mind, 0xB8)) >= 0.f ? kOne : kNegativeOne;
        const long double swing = (double(kHalfPi) - double(context.tunables.real(0x00427510)) * kRadians) * side;
        const float heading = float(angle - swing);
        set_camera_heading_goal(view.mind, heading, context.tunables.real(0x00427514) * kFloorMargin, 1,
            context.tunables.real(0x004274F4), 1);
        return;
    }
    const Vec3 pushed{real(view.body, 0x1D8), real(view.body, 0x1DC), real(view.body, 0x1E0)};
    const Vec3 euler = actor_world_euler(view.camera);
    Vec3 forward{};
    add_sound_direction(forward, euler.x, euler.y, 1.f);
    const float squared = forward.y * forward.y + forward.z * forward.z + forward.x * forward.x;
    if (squared != 0.f)
    {
        const long double factor = retail_reciprocal_sqrt(squared);
        forward.x = float(forward.x * factor);
        forward.y = float(forward.y * factor);
        forward.z = float(forward.z * factor);
    }
    if ((view.body_bits->bytes[2] & 0x20) != 0
        && forward.z * pushed.z + forward.y * pushed.y + forward.x * pushed.x > context.tunables.real(0x0042750C))
    {
        const float step = runtime_frame_step(context.clock);
        put_float(view.mind, 0xC0, step * runtime_actor_rate(view.camera) + real(view.mind, 0xC0));
    }
    else
    {
        put_float(view.mind, 0xC0, 0.f);
    }
    if (!(real(view.mind, 0xC0) > context.tunables.real(0x00427500)))
    {
        return;
    }
    Vec3 kept = eye;
    if (entity_record_movement_probe(kept, focus, context.tunables.word(0x004274B8), 4,
            camera_body_mask(view.camera)) == 0)
    {
        return;
    }
    const float dz = focus.z - kept.z;
    const float dx = focus.x - kept.x;
    const long double gap = retail_square_root(dz * dz + dx * dx);
    if (!(gap > double(context.tunables.real(0x00427504)) * distance))
    {
        return;
    }
    position = kept;
    const float pull = context.tunables.real(0x00427508);
    const u32 message[4]{0x22,
        std::bit_cast<u32>((kept.x - focus.x) * pull + focus.x),
        std::bit_cast<u32>((kept.y - focus.y) * pull + focus.y),
        std::bit_cast<u32>((kept.z - focus.z) * pull + focus.z)};
    view.body_message(message);
    put_float(view.mind, 0xC0, 0.f);
}

// Xbox 0x000781F0
u32 update_camera_target_request(RuntimeCameraMindView& view, std::span<const u8> request, float distance,
    const RuntimeCameraRigContext& context)
{
    const u32 target = read_u32(view.mind, 0xC);
    if (target == 0)
    {
        return 0;
    }
    Vec3 eye = actor_world_position(view.camera);
    if (camera_input_turn(context.actor_at(target), context) == 0.f)
    {
        choose_camera_heading(view, request, eye, context);
    }
    else if (read_u32(view.mind, 0xAC) != 0)
    {
        put(view.mind, 0xAC, 0);
        put(view.mind, 0xB0, 1);
    }
    Vec3 position{};
    Vec3 focus{};
    place_camera_rig(view.mind, view.camera, request, eye, position, focus, target, distance, context);
    track_camera_obstruction(view, eye, position, focus, target, distance, context);
    if (read_u32(request, 0x2A0) != 0)
    {
        put_float(view.mind, 0x70, position.x);
        put_float(view.mind, 0x74, position.y);
        put_float(view.mind, 0x78, position.z);
        Vec3 aim{};
        aim_camera_offset(aim, eye, position, target, context.tunables.real(0x004274DC), 0.f,
            context.tunables.real(0x004274E0), context);
        put_float(view.body, 0x318, aim.x);
        put_float(view.body, 0x31C, aim.y);
        put_float(view.body, 0x320, aim.z);
    }
    else
    {
        put_float(view.mind, 0x70, eye.x);
        put_float(view.mind, 0x74, eye.y);
        put_float(view.mind, 0x78, eye.z);
        const u32 message[1]{3};
        view.body_message(message);
    }
    put_float(view.body, 0x2FC, focus.x);
    put_float(view.body, 0x300, focus.y);
    put_float(view.body, 0x304, focus.z);
    view.body[0x308] = 1;
    return 1;
}
}
