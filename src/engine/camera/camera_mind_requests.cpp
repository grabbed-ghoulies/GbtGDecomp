#include "camera_mind_requests.h"
#include "../entities/player_fields.h"
#include "../entities/body_fields.h"
#include "../entities/mind_fields.h"
#include "../scripts/script_table_lookup.h"
#include "../entities/actor_shape_push.h"
#include "entity_camera.h"
#include "display_view.h"
#include "../entities/bounded_channels.h"
#include "../entities/actor_clock.h"
#include "../navigation/navigation.h"
#include "../navigation/path_routes.h"
#include "../navigation/floor_tracking.h"
#include "../entities/ghouly_spawn_record.h"
#include <cstring>
#include "gbtg/anim.h"
#include "gbtg/sound_runtime.h"
#include "../frontend/dialogue_input.h"
#include <cmath>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace field = camera_mind_field;
namespace
{
float mind_real(std::span<const u8> mind, size_t offset)
{
    return read_f32(mind, offset);
}

// The frame step: 0 while DAT_004312DC is above 0, else DAT_005096C0 * DAT_004312C4.
float frame_step(const RuntimeActorClock& clock)
{
    return clock.freeze > 0.f ? 0.f : clock.dt * clock.scale;
}
}

// Xbox 0x00079E80
u32 track_camera_follow_point(std::span<u8> mind, std::span<const u8> request, const Actor& camera, Vec3& point,
    float& near_distance, float& far_distance, const RuntimeCameraRigContext& context)
{
    const u32 target = read_u32(request, 0x10);
    if (target == 0)
    {
        point = {0.f, 10.f, 0.f}; // DAT_0032B10C
        return 0;
    }
    const Actor* actor = context.actor_at(target).actor;
    if (actor == nullptr)
    {
        throw Error("0x00079E80 follows a request +0x10 that is not a scene actor");
    }
    Vec3 found{};
    float y;
    switch (context.tunables.word(0x00427540))
    {
    case 0:
        found = actor_world_position(*actor); // 0x000412A0, ESI the actor
        y = found.y;
        break;
    case 1:
    {
        float height = 0.f;
        float radius = 0.f;
        runtime_actor_cylinder_metrics(*actor, found, height, radius); // 0x0003DD50, ECX the actor
        y = context.tunables.real(0x00427544) * height + found.y;
        break;
    }
    default:
        throw Error("0x00079E80 reads its point from an uninitialised stack slot (DAT_00427540 above 1)");
    }
    near_distance = context.tunables.real(0x00427550);
    far_distance = context.tunables.real(0x00427554);
    if (get(mind, field::focus_actor) != 0)
    {
        set(mind, field::blend_from, get(mind, field::follow_point));
        set(mind, field::blend, 1.f); // DAT_0032B0F8
    }
    point = {found.x, y, found.z};
    const float step = frame_step(context.clock);
    const float rate = camera.animation_pause > 0.f ? 0.f : camera.animation_rate; // +0x930, +0x92C
    if (get(mind, field::blend) > 0.f)
    {
        const float left = get(mind, field::blend) - rate * step;
        set(mind, field::blend, left);
        if (!(0.f < left))
        {
            set(mind, field::blend, 0.f);
        }
    }
    if (get(mind, field::blend) > 0.f)
    {
        // (sin(t * pi - pi/2) + 1) * 0.5 (DAT_0032B148, DAT_0032B140, DAT_0032B0F8, DAT_0032B0F4), on the x87 stack.
        const float angle = float(static_cast<long double>(get(mind, field::blend)) * std::bit_cast<float>(0x40490FDBu)
            - std::bit_cast<float>(0x3FC90FDBu));
        const long double share = (retail_sin(angle) + 1.0L) * 0.5L; // 0x00153320
        const Vec3 from = get(mind, field::blend_from);
        point.x = float((static_cast<long double>(from.x) - point.x) * share + point.x);
        point.y = float((static_cast<long double>(from.y) - point.y) * share + point.y);
        point.z = float((static_cast<long double>(from.z) - point.z) * share + point.z);
    }
    set(mind, field::follow_point, point);
    return 0;
}

// Xbox 0x00079C40
void place_camera_follow_eye(const Vec3& point, Vec3& eye, const Actor& camera, float near_distance,
    float far_distance, float low_pitch, float high_pitch, float low_yaw, float high_yaw, u32 curve,
    const RuntimeCameraRigContext& context)
{
    constexpr float kDegrees = std::bit_cast<float>(0x42652EE0u); // DAT_0032B280
    constexpr float kRadians = std::bit_cast<float>(0x3C8EFA35u); // DAT_0032B0FC
    const float dx = point.x - eye.x;
    const float dy = point.y - eye.y;
    const float dz = point.z - eye.z;
    const float length = float(std::sqrt(static_cast<long double>(dy) * dy + static_cast<long double>(dx) * dx
        + static_cast<long double>(dz) * dz)); // FSQRT
    float yaw = 0.f;
    float pitch = 0.f;
    direction_to_yaw_pitch(eye.x - point.x, eye.y - point.y, eye.z - point.z, yaw, pitch); // 0x00105E20
    const float pitch_degrees = pitch * kDegrees;
    float yaw_degrees = yaw * kDegrees;
    const float far_limit = near_distance > far_distance ? near_distance : far_distance;
    float distance = length;
    if (near_distance > length)
    {
        distance = near_distance;
    }
    else if (length > far_limit)
    {
        distance = far_limit;
    }
    const float high_pitch_limit = low_pitch > high_pitch ? low_pitch : high_pitch;
    float clamped_pitch = pitch_degrees;
    if (low_pitch > pitch_degrees)
    {
        clamped_pitch = low_pitch;
    }
    else if (pitch_degrees > high_pitch_limit)
    {
        clamped_pitch = high_pitch_limit;
    }
    if (low_yaw != 0.f || high_yaw != std::bit_cast<float>(0x40C90FDBu)) // DAT_0032B298
    {
        const float high_yaw_limit = low_yaw > high_yaw ? low_yaw : high_yaw;
        if (low_yaw > yaw_degrees)
        {
            yaw_degrees = low_yaw;
        }
        else if (yaw_degrees > high_yaw_limit)
        {
            yaw_degrees = high_yaw_limit;
        }
    }
    if (curve != 0)
    {
        // The input device's analog difference (0x00108D20, EAX the device) turns the yaw by 100 a second.
        const float rate = camera.animation_pause > 0.f ? 0.f : camera.animation_rate; // +0x930, +0x92C
        const float step = rate * context.clock.dt * context.clock.scale; // DAT_005096C0, DAT_004312C4
        const RuntimeDialogueInputDevice* device = context.input_device(curve);
        if (device == nullptr)
        {
            throw Error("0x00079C40 turns by an input device that is not bound");
        }
        yaw_degrees = runtime_input_analog_difference(*device, context.input) * std::bit_cast<float>(0x42C80000u)
            * step + yaw_degrees; // DAT_0032B12C
    }
    Vec3 placed = point;
    add_sound_direction(placed, clamped_pitch * kRadians, yaw_degrees * kRadians, distance); // 0x00106220, ESI placed
    constexpr float kLow = std::bit_cast<float>(0xBA83126Fu); // DAT_0032B490
    constexpr float kHigh = std::bit_cast<float>(0x3A83126Fu); // DAT_0032B1A4
    const auto outside = [](float delta)
    {
        return kLow > delta || delta > kHigh;
    };
    if (outside(placed.x - eye.x) || outside(placed.y - eye.y) || outside(placed.z - eye.z))
    {
        eye = placed;
    }
}

// Xbox 0x00079B80
i32 update_camera_follow_request(RuntimeCameraMindView& view, std::span<const u8> request,
    const RuntimeCameraRigContext& context)
{
    Vec3 eye = actor_world_position(view.camera); // 0x000412A0, EDI the eye
    Vec3 point{};
    float near_distance = 0.f;
    float far_distance = 0.f;
    const u32 curve = track_camera_follow_point(view.mind, request, view.camera, point, near_distance,
        far_distance, context); // 0x00079E80
    place_camera_follow_eye(point, eye, view.camera, near_distance, far_distance, context.tunables.real(0x00427558),
        context.tunables.real(0x0042755C), 0.f, std::bit_cast<float>(0x40C90FDBu), curve, context); // 0x00079C40
   body_record(view.body).position.x = eye.x;
   body_record(view.body).position.y = eye.y;
   body_record(view.body).position.z = eye.z;
   body_record(view.body).look_point.x = point.x;
   body_record(view.body).look_point.y = point.y;
   body_record(view.body).look_point.z = point.z;
    body_record(view.body).look_point_set = 1;
    return 0;
}

// Xbox 0x0007A160
void camera_actor_point(const RuntimeDirectorActor& actor, Vec3& point, float* heading,
    const RuntimeCameraRigContext& context)
{
    if (actor.actor == nullptr)
    {
        throw Error("0x0007A160 frames an address that is not a scene actor");
    }
    switch (context.tunables.word(0x00427560))
    {
    case 0:
        point = actor_world_position(*actor.actor); // 0x000412A0, ESI the actor, EDI the point
        point.y = actor.actor->radius_y * body_record(actor.body).height_scale * context.tunables.real(0x00427564)
            + point.y;
        break;
    case 1:
    {
        float height = 0.f;
        float radius = 0.f;
        runtime_actor_cylinder_metrics(*actor.actor, point, height, radius); // 0x0003DD50, ECX the actor
        point.y = context.tunables.real(0x00427564) * height + point.y;
        break;
    }
    default:
        break;
    }
    if (heading != nullptr)
    {
        *heading = actor.actor->euler.y * std::bit_cast<float>(0x42652EE1u); // +0x18, DAT_0032B440
    }
}

// Xbox 0x0007A550
u32 track_camera_pair_point(std::span<u8> mind, const Actor& camera, Vec3& point, float& near_distance,
    float& far_distance, float& heading, const RuntimeCameraRigContext& context)
{
    const u32 target_address = get(mind, mind_field::target);
    if (target_address == 0)
    {
        point = {0.f, 0.f, 0.f};
        return 0;
    }
    const char* name = g_cam_active[0] == nullptr ? "" : g_cam_active[0]->name; // DAT_004E16A0 +0x34, or 0x0030C9D2
    if (std::strcmp(name, "cameron") != 0) // 0x00323E30
    {
        point = {0.f, 0.f, 0.f};
        return 0;
    }
    const RuntimeDirectorActor target = context.actor_at(target_address);
    Vec3 first{};
    camera_actor_point(target, first, &heading, context); // 0x0007A160, ECX the target
    // The target's mind (+0x958) +0xC, kept while its +0x20 is set.
    u32 second_address = target.mind.empty() ? 0u : get(target.mind, mind_field::target);
    if (second_address != 0 && get(target.mind, mind_field::target_mode) == 0)
    {
        second_address = 0;
    }
    near_distance = context.tunables.real(0x00427570);
    far_distance = context.tunables.real(0x00427574);
    if (second_address == 0)
    {
        if (get(mind, field::focus_actor) != 0)
        {
            set(mind, field::blend_from, get(mind, field::follow_point));
            set(mind, field::blend, 1.f); // DAT_0032B0F8
        }
        point = first;
    }
    else
    {
        const RuntimeDirectorActor second = context.actor_at(second_address);
        Vec3 other{};
        camera_actor_point(second, other, nullptr, context); // 0x0007A160, ECX the second
        if (context.tunables.word(0x00427568) != 0)
        {
            other.y = first.y;
        }
        point.y = (first.y - other.y) * 0.5f + other.y; // DAT_0032B0F4
        point.x = (first.x - other.x) * 0.5f + other.x;
        point.z = (first.z - other.z) * 0.5f + other.z;
        if (context.tunables.word(0x0042756C) != 0)
        {
            if (second.actor == nullptr || target.actor == nullptr)
            {
                throw Error("0x0007A550 fits the two-player camera to an address that is not a scene actor");
            }
            const long double dx = static_cast<long double>(other.x) - first.x;
            const long double dy = static_cast<long double>(other.y) - first.y;
            const long double dz = static_cast<long double>(other.z) - first.z;
            const long double gap = std::sqrt(dz * dz + dy * dy + dx * dx); // FSQRT
            const float reach = float(gap + static_cast<long double>(second.actor->radius_xz)
                * body_record(second.body).radius_scale
                + static_cast<long double>(target.actor->radius_xz) * body_record(target.body).radius_scale);
            float fit = second.actor->radius_y * body_record(second.body).height_scale
                + target.actor->radius_y * body_record(target.body).height_scale;
            if (!(reach < fit))
            {
                fit = reach;
            }
            near_distance = fit;
            far_distance = fit * std::bit_cast<float>(0x3DCCCCCDu) + fit; // DAT_0032B254
        }
        if (get(mind, field::focus_actor) == 0 || get(mind, field::focus_actor) != second_address)
        {
            set(mind, field::blend_from, get(mind, field::follow_point));
            set(mind, field::blend, 1.f); // DAT_0032B0F8
        }
    }
    set(mind, field::focus_actor, second_address);
    const float step = frame_step(context.clock);
    const float rate = camera.animation_pause > 0.f ? 0.f : camera.animation_rate; // +0x930, +0x92C
    if (get(mind, field::blend) > 0.f)
    {
        const float left = get(mind, field::blend) - rate * step;
        set(mind, field::blend, left);
        if (!(0.f < left))
        {
            set(mind, field::blend, 0.f);
        }
    }
    if (get(mind, field::blend) > 0.f)
    {
        const float angle = float(static_cast<long double>(get(mind, field::blend)) * std::bit_cast<float>(0x40490FDBu)
            - std::bit_cast<float>(0x3FC90FDBu)); // DAT_0032B148, DAT_0032B140
        const long double share = (retail_sin(angle) + 1.0L) * 0.5L; // 0x00153320
        const Vec3 from = get(mind, field::blend_from);
        point.x = float((static_cast<long double>(from.x) - point.x) * share + point.x);
        point.y = float((static_cast<long double>(from.y) - point.y) * share + point.y);
        point.z = float((static_cast<long double>(from.z) - point.z) * share + point.z);
    }
    set(mind, field::follow_point, point);
    return player_record(target.bytes).input_device;
}

// Xbox 0x0007A420
void turn_camera_yaw_by_input(const Actor& camera, float& yaw, u32 device, float heading,
    const RuntimeCameraRigContext& context)
{
    constexpr float kTurn = std::bit_cast<float>(0x43B40000u); // 360, DAT_0032B1E4
    constexpr float kLow = std::bit_cast<float>(0xC3340000u); // -180, DAT_0032B294
    constexpr float kHigh = std::bit_cast<float>(0x43340000u); // 180, DAT_0032B290
    const auto wrap = [](long double remainder)
    {
        float angle = float(remainder);
        if (static_cast<long double>(kLow) > remainder)
        {
            angle = angle + kTurn;
        }
        if (!(angle < kHigh))
        {
            angle = angle - kTurn;
        }
        return angle;
    };
    const float rate = camera.animation_pause > 0.f ? 0.f : camera.animation_rate; // +0x930, +0x92C
    const float step = context.clock.dt * context.clock.scale * rate; // DAT_005096C0, DAT_004312C4
    const float offset = wrap(runtime_float_remainder(float(static_cast<long double>(yaw) - heading),
        kTurn)); // 0x00152BE0
    const RuntimeDialogueInputDevice* input = context.input_device(device);
    if (input == nullptr)
    {
        throw Error("0x0007A420 reads an input device that is not bound");
    }
    float level = runtime_input_first_analog_level(*input, context.input); // 0x00108DE0, EAX the device
    if (0.f > offset)
    {
        level = 0.f - level;
    }
    const float turned = level * std::bit_cast<float>(0x42C80000u) * step + offset; // DAT_0032B12C
    if (kLow > turned || turned > kHigh)
    {
        yaw = wrap(runtime_float_remainder(heading + kHigh, kTurn)); // 0x00152BE0
        level = 0.f;
    }
    yaw = level * std::bit_cast<float>(0x42C80000u) * step + yaw;
}

// Xbox 0x0007A1F0
void place_camera_pair_eye(const Vec3& point, Vec3& eye, const Actor& camera, float near_distance, float far_distance,
    float low_pitch, float high_pitch, float low_yaw, float high_yaw, float heading, u32 device,
    const RuntimeCameraRigContext& context)
{
    constexpr float kDegrees = std::bit_cast<float>(0x42652EE0u); // DAT_0032B280
    constexpr float kRadians = std::bit_cast<float>(0x3C8EFA35u); // DAT_0032B0FC
    const float dx = point.x - eye.x;
    const float dy = point.y - eye.y;
    const float dz = point.z - eye.z;
    const float length = float(std::sqrt(static_cast<long double>(dy) * dy + static_cast<long double>(dx) * dx
        + static_cast<long double>(dz) * dz)); // FSQRT
    float yaw = 0.f;
    float pitch = 0.f;
    direction_to_yaw_pitch(eye.x - point.x, eye.y - point.y, eye.z - point.z, yaw, pitch); // 0x00105E20
    yaw = yaw * kDegrees;
    const float pitch_degrees = pitch * kDegrees;
    const float far_limit = near_distance > far_distance ? near_distance : far_distance;
    float distance = length;
    if (near_distance > length)
    {
        distance = near_distance;
    }
    else if (length > far_limit)
    {
        distance = far_limit;
    }
    const float high_pitch_limit = low_pitch > high_pitch ? low_pitch : high_pitch;
    float clamped_pitch = pitch_degrees;
    if (low_pitch > pitch_degrees)
    {
        clamped_pitch = low_pitch;
    }
    else if (pitch_degrees > high_pitch_limit)
    {
        clamped_pitch = high_pitch_limit;
    }
    if (low_yaw != 0.f || high_yaw != std::bit_cast<float>(0x43B40000u)) // DAT_0032B100, DAT_0032B1E4
    {
        const float high_yaw_limit = low_yaw > high_yaw ? low_yaw : high_yaw;
        if (low_yaw > yaw)
        {
            yaw = low_yaw;
        }
        else if (yaw > high_yaw_limit)
        {
            yaw = high_yaw_limit;
        }
    }
    if (device != 0)
    {
        turn_camera_yaw_by_input(camera, yaw, device, heading, context); // 0x0007A420, ESI the yaw
    }
    Vec3 placed = point;
    add_sound_direction(placed, clamped_pitch * kRadians, yaw * kRadians, distance); // 0x00106220, ESI placed
    constexpr float kLowDelta = std::bit_cast<float>(0xBA83126Fu); // DAT_0032B490
    constexpr float kHighDelta = std::bit_cast<float>(0x3A83126Fu); // DAT_0032B1A4
    const auto outside = [](float delta)
    {
        return kLowDelta > delta || delta > kHighDelta;
    };
    if (outside(placed.x - eye.x) || outside(placed.y - eye.y) || outside(placed.z - eye.z))
    {
        eye = placed;
    }
}

// Xbox 0x0007A080
i32 update_camera_pair_request(RuntimeCameraMindView& view, const RuntimeCameraRigContext& context)
{
    Vec3 eye = actor_world_position(view.camera); // 0x000412A0, EDI the eye
    Vec3 point{};
    float near_distance = 0.f;
    float far_distance = 0.f;
    float heading = 0.f;
    const u32 device = track_camera_pair_point(view.mind, view.camera, point, near_distance, far_distance, heading,
        context); // 0x0007A550
    if (device != 0)
    {
        place_camera_pair_eye(point, eye, view.camera, near_distance, far_distance, context.tunables.real(0x00427578),
            context.tunables.real(0x0042757C), 0.f, std::bit_cast<float>(0x43B40000u), heading, device,
            context); // 0x0007A1F0
       body_record(view.body).position.x = eye.x;
       body_record(view.body).position.y = eye.y;
       body_record(view.body).position.z = eye.z;
       body_record(view.body).look_point.x = point.x;
       body_record(view.body).look_point.y = point.y;
       body_record(view.body).look_point.z = point.z;
        body_record(view.body).look_point_set = 1;
    }
    return 0;
}

// Xbox 0x0007AA20
u32 camera_path_target_point(std::span<u8> mind, const RuntimeDirectorActor& target, Vec3& point, float& rise,
    const RuntimeCameraRigContext& context)
{
    if (target.address == 0)
    {
        return 0;
    }
    if (target.actor == nullptr || target.body.empty() || target.body_bits == nullptr)
    {
        throw Error("0x0007AA20 frames a target with no native actor or body");
    }
    const float half = target.actor->radius_y * body_record(target.body).height_scale * 0.5f; // DAT_0032B0F4
    point = actor_world_position(*target.actor); // 0x000412A0, ESI the target, EDI the point
    point.y = point.y + half;
    rise = half;
    if (context.tunables.word(0x0042759C) == 0)
    {
        return 1;
    }
    if ((target.body_bits->bytes[6] & 1) == 0) // Body +0x15C bit 0x30.
    {
        const u32 none = 0;
        set(mind, field::target_point_held, none);
        return 1;
    }
    if (get(mind, field::target_point_held) == 0)
    {
        set(mind, field::held_target_point, point);
        const u32 held = 1;
        set(mind, field::target_point_held, held);
        return 1;
    }
    point = get(mind, field::held_target_point);
    return 1;
}

// Xbox 0x0007AB00
void camera_path_eye(const Vec3& point, const Vec3& eye, Vec3& wanted, const RuntimeCameraRigContext& context)
{
    constexpr float kDegrees = std::bit_cast<float>(0x42652EE0u); // DAT_0032B280
    constexpr float kRadians = std::bit_cast<float>(0x3C8EFA35u); // DAT_0032B0FC
    const float dx = point.x - eye.x;
    const float dy = point.y - eye.y;
    const float dz = point.z - eye.z;
    const float length = float(std::sqrt(static_cast<long double>(dy) * dy + static_cast<long double>(dx) * dx
        + static_cast<long double>(dz) * dz)); // FSQRT
    float yaw = 0.f;
    float pitch = 0.f;
    direction_to_yaw_pitch(eye.x - point.x, eye.y - point.y, eye.z - point.z, yaw, pitch); // 0x00105E20
    const float pitch_degrees = pitch * kDegrees;
    const float yaw_degrees = yaw * kDegrees;
    const float low_distance = context.tunables.real(0x00427588);
    float high_distance = context.tunables.real(0x0042758C);
    if (low_distance > high_distance)
    {
        high_distance = low_distance;
    }
    float distance = length;
    if (low_distance > length)
    {
        distance = low_distance;
    }
    else if (length > high_distance)
    {
        distance = high_distance;
    }
    const float spread = context.tunables.real(0x00427584);
    const float centre = context.tunables.real(0x00427580);
    const float low_pitch = 0.f - (spread + centre);
    float high_pitch = 0.f - (centre - spread);
    if (low_pitch > high_pitch)
    {
        high_pitch = low_pitch;
    }
    float clamped_pitch = pitch_degrees;
    if (low_pitch > pitch_degrees)
    {
        clamped_pitch = low_pitch;
    }
    else if (pitch_degrees > high_pitch)
    {
        clamped_pitch = high_pitch;
    }
    wanted = point;
    add_sound_direction(wanted, clamped_pitch * kRadians, yaw_degrees * kRadians, distance); // 0x00106220, ESI wanted
    constexpr float kLow = std::bit_cast<float>(0xBA83126Fu); // DAT_0032B490
    constexpr float kHigh = std::bit_cast<float>(0x3A83126Fu); // DAT_0032B1A4
    const auto inside = [](float delta)
    {
        return !(kLow > delta) && !(delta > kHigh);
    };
    if (inside(wanted.x - eye.x) && inside(wanted.y - eye.y) && inside(wanted.z - eye.z))
    {
        wanted = eye;
    }
}

// Xbox 0x0012C9C0
bool navigation_cell_sees_point(i32 cell, i32 floor, const Vec3& point, i32 region_mode, i32 allow_tagged,
    i32 reject_marked)
{
    if (cell == -1)
    {
        return false;
    }
    const entity_detail::NavCell& centre = entity_detail::g_nav_grids[floor]->cells.at(size_t(cell)); // DAT_0050E240
    const Vec3 from{float(centre.x), float(centre.bottom), float(centre.z)}; // Grid +0x10, stride 0x12
    i32 last_cell = 0;
    return entity_detail::navigation_path_clear(from, point, region_mode, allow_tagged, 0, reject_marked, nullptr,
        &last_cell, 0); // 0x0012CC50, EAX the cell's centre
}

// Xbox 0x0007ACB0
u32 route_camera_path_eye(const Actor& camera, const RuntimeDirectorActor& target, const Vec3& eye,
    const Vec3& point, Vec3& wanted, entity_detail::NavigationPathSearch* search,
    const RuntimeCameraRigContext& context)
{
    if (search == nullptr) // Mind +0x1C.
    {
        return 0;
    }
    const i32 floor = i32(player_record(target.bytes).floor);
    const i32 start = entity_detail::navigation_cell_index(floor, eye); // 0x00110EF0, EBX the eye
    const i32 finish = entity_detail::navigation_cell_index(floor, point); // 0x00110EF0, EBX the point
    if (start == -1 || finish == -1)
    {
        return 0;
    }
    const bool retarget = context.tunables.word(0x00427598) == 0
        || entity_detail::retarget_navigation_search_route(*search, start, finish) == 0; // 0x00037980
    GhoulySpawnPool* route = static_cast<GhoulySpawnPool*>(search->scratch); // Search +0x14.
    if (route != nullptr && !retarget && entity_detail::navigation_path_search_route_is_open(*search) != 0) // 0x00036E90
    {
        entity_detail::set_navigation_route_endpoints(*route, eye, point); // 0x000376F0, EBX the eye, EDI the point
    }
    else
    {
        if (entity_detail::navigation_cell_has_no_open_neighbor(*search->grid, u32(start), 0)) // 0x00036600
        {
            return 0;
        }
        entity_detail::begin_navigation_path_search(*search, start, finish); // 0x00036C80
        const float elapsed = runtime_actor_step(camera, context.clock); // 0x00042320, EAX mind +8, the camera
        entity_detail::update_navigation_path_search(*search, 0, elapsed); // 0x00036CA0
        if (search->state != 3 || search->result_found == 0) // +0x28, +0x2C
        {
            return 0;
        }
        route = entity_detail::take_navigation_search_route(*search, eye, point); // 0x00036E50
    }
    if (route == nullptr)
    {
        return 0;
    }
    const i32 count = i32((route->cursor - route->begin) / i32(route->stride));
    for (i32 index = 0; index < count; ++index)
    {
        i32 cell;
        std::memcpy(&cell, route->begin + size_t(route->stride) * size_t(index), 4);
        if (navigation_cell_sees_point(cell, floor, point, 0, 1, 1)) // 0x0012C9C0
        {
            const entity_detail::NavCell& centre = entity_detail::g_nav_grids[floor]->cells.at(size_t(cell));
            const Vec3 from{float(centre.x), float(centre.bottom), float(centre.z)};
            camera_path_eye(point, from, wanted, context); // 0x0007AB00, EBX the point, EDI the cell
            return 1;
        }
    }
    return 0;
}

// Xbox 0x0007AE70
void place_camera_path_eye(const Actor& camera, const RuntimeDirectorActor& target, const Vec3& point,
    const Vec3& eye, Vec3& wanted, entity_detail::NavigationPathSearch* search,
    const RuntimeCameraRigContext& context)
{
    camera_path_eye(point, eye, wanted, context); // 0x0007AB00, EAX wanted, EBX the point, EDI the eye
    if (target.actor == nullptr)
    {
        throw Error("0x0007AE70 places the camera on a target with no native actor");
    }
    const Vec3 position = actor_world_position(*target.actor); // 0x000412A0, ESI the target
    i32 last_cell = 0;
    if (!entity_detail::navigation_path_clear(position, wanted, 1, 0, 0, 0, nullptr, &last_cell,
            i16(target.actor->class_key))) // 0x0012CC50, EAX the target's position
    {
        route_camera_path_eye(camera, target, eye, point, wanted, search, context); // 0x0007ACB0
    }
    camera_floor_height(target, wanted, point, context.tunables.real(0x004275A0)); // 0x0013D110, ECX wanted
}

// Xbox 0x0007A910
i32 update_camera_path_request(RuntimeCameraMindView& view, entity_detail::NavigationPathSearch* search,
    const RuntimeCameraRigContext& context)
{
    const u32 target_address = get(view.mind, mind_field::target);
    Vec3 eye = actor_world_position(view.camera); // 0x000412A0, EDI the eye
    const RuntimeDirectorActor target = target_address == 0 ? RuntimeDirectorActor{} : context.actor_at(target_address);
    Vec3 point{};
    float rise = 0.f;
    if (camera_path_target_point(view.mind, target, point, rise, context) == 0) // 0x0007AA20, ECX the target
    {
        return 0;
    }
    Vec3 wanted{};
    place_camera_path_eye(view.camera, target, point, eye, wanted, search, context); // 0x0007AE70
    set(view.mind, mind_field::goal_point, wanted);
    Vec3 aim{};
    aim_camera_offset(aim, eye, wanted, target_address, context.tunables.real(0x00427590), 0.f,
        context.tunables.real(0x00427594), context); // 0x0013CE80, ECX the eye, EAX wanted, EDX the target
   body_record(view.body).velocity.x = aim.x;
   body_record(view.body).velocity.y = aim.y;
   body_record(view.body).velocity.z = aim.z;
   body_record(view.body).look_point.x = point.x;
   body_record(view.body).look_point.y = point.y;
   body_record(view.body).look_point.z = point.z;
    body_record(view.body).look_point_set = 1;
    return 0;
}

// Xbox 0x00078170
i32 update_camera_mind_request(RuntimePlayerMindRequest& request, RuntimeCameraMindRequests& context)
{
    switch (read_u16(request.bytes, 8))
    {
    case 1:
        return i32(update_camera_target_request(context.view, request.bytes,
            std::bit_cast<float>(context.request_address(request)), context.rig));
    case 3:
        return update_camera_follow_request(context.view, request.bytes, context.rig); // 0x00079B80
    case 4:
        return update_camera_pair_request(context.view, context.rig); // 0x0007A080
    case 0x12:
        return update_camera_path_request(context.view, context.path_search, context.rig); // 0x0007A910
    default:
        return context.parent_update(request);
    }
}

// Xbox 0x0007AEF0
i32 initialize_camera_mind_request(RuntimePlayerMindRequest& request, RuntimeCameraMindRequests& context)
{
    std::span<u8> bytes = request.bytes;
    const u16 kind = read_u16(bytes, 8);
    if (kind == 0x29)
    {
        context.random = context.random * 0x19660D + 0x3C6EF35F;
        const float unit = std::bit_cast<float>((context.random & 0x7FFFFF) | 0x3F800000);
        const float low = read_f32(bytes, 0x10);
        const float high = read_f32(bytes, 0x14);
        const float value = (high - low) * (unit - 1.f) + low; // DAT_0032B0F8.
        std::memcpy(bytes.data() + 0x18, &value, 4);
    }
    u16 key;
    if (kind == 1 || kind == 0x12)
    {
        key = 0x19;
        if (body_record(context.view.body).action_key == key)
        {
            return 1;
        }
    }
    else
    {
        key = 0x1B;
    }
    if (context.actions.table != nullptr)
    {
        u32 parameters = 0;
        const u8* row = find_runtime_script_table_row(context.actions.table, key, parameters);
        if (row != nullptr)
        {
            if (parameters != 0)
            {
                throw Error("camera mind action row carries a template token the selection has no view of");
            }
            select_runtime_body_action_row(context.view.body, row, nullptr, context.actions);
        }
    }
    return 1;
}
}
