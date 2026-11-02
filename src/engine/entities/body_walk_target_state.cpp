#include "body_walk_target_state.h"
#include "mind_fields.h"
#include "actor_transform.h"
#include "body_hit_state.h"
#include "body_orientation.h"
#include "player_mind_events.h"
#include "../audio/body_sound_commands.h"
#include "gbtg/anim.h"
#include "body_fields.h"

#include <bit>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
// Walk-target row records by state, the host side of state +0x1C + 0x10 * row.
std::unordered_map<const u8*, std::array<const AnimationRecord*, 4>> walk_target_records;
// Fly-alert hover records (+0x78), by state.
std::unordered_map<const u8*, const AnimationRecord*> fly_alert_hovers;

const float tau = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
const float pi = std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
const float negative_pi = std::bit_cast<float>(0xC0490FDBu); // DAT_0032B29C.
const float half_pi = std::bit_cast<float>(0x3FC90FDBu); // DAT_0032B140.

namespace field = walk_target_field;
namespace rows_field = walk_target_rows_field;
namespace row_field = walk_target_row_field;
namespace parameter = walk_target_parameter;

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

// The inlined 0x00152BE0 wrap into [0, 2pi): the x87 remainder is compared, its float kept.
float wrap_positive(float value)
{
    const long double remainder = animation_remainder(value, tau);
    float result = float(remainder);
    if (0.0L > remainder) // DAT_0032B100.
    {
        result += tau;
    }
    return result;
}

// The inlined 0x00152BE0 wrap into [-pi, pi).
float wrap_signed(float value)
{
    const long double remainder = animation_remainder(value, tau);
    float result = float(remainder);
    if (static_cast<long double>(negative_pi) > remainder)
    {
        result += tau;
    }
    if (result >= pi)
    {
        result -= tau;
    }
    return result;
}

// The inlined surface-mode scale: DAT_00217EC0[mode] when body +0x878 has the mode's bit, otherwise 1.
float surface_mode_scale(std::span<const u8> body, std::span<const float> mode_scales)
{
    const u8 mode = get(body, body_field::surface_override_frames) == 0 ? get(body, body_field::surface)
                                                                        : get(body, body_field::surface_override);
    if ((get(body, body_field::scaled_surfaces) & (1u << (mode & 0x1F))) != 0)
    {
        if (mode >= mode_scales.size())
        {
            throw Error("surface mode lies outside the DAT_00217EC0 view");
        }
        return mode_scales[mode];
    }
    return 1.f; // DAT_0032B0F8.
}

// Actor +0x958 -> +0xC: the mind's target actor, when the mind is alert (+0x20).
u32 alert_mind_target(std::span<const u8> mind)
{
    if (mind.empty() || get(mind, mind_field::target_mode) == 0)
    {
        return 0;
    }
    return get(mind, mind_field::target);
}

void store_target_position(std::span<u8> body, const Vec3& position)
{
    put_float(body, 0x3C, position.x);
    put_float(body, 0x40, position.y);
    put_float(body, 0x44, position.z);
}

// Actor +0x3C/+0x44: the actor's horizontal speed (0x00152B30 of x*x + z*z).
float actor_flat_speed(const Actor& actor)
{
    const float x = actor.anchor.x;
    const float z = actor.anchor.z;
    return float(retail_square_root(x * x + z * z));
}
}

// Xbox 0x001009A0
bool runtime_angle_in_arc(float angle, float start, float end)
{
    const float a = wrap_positive(angle);
    const float b = wrap_positive(start);
    float c = end;
    if (!(end == tau))
    {
        c = wrap_positive(end);
    }
    if (!(c >= b))
    {
        if (a >= b)
        {
            return true;
        }
        return c > a;
    }
    if (!(a >= b))
    {
        return false;
    }
    return c > a;
}

// Xbox 0x00100B80
float runtime_nearest_arc_bound(float angle, float lower, float upper)
{
    const float to_lower = wrap_signed(lower - angle);
    const float to_upper = wrap_signed(upper - angle);
    const float upper_turn = animation_absolute(to_upper);
    const float lower_turn = animation_absolute(to_lower);
    if (upper_turn > lower_turn)
    {
        return lower;
    }
    return upper;
}

// Xbox 0x00100A80
float runtime_clamp_angle_to_arc(float angle, float lower, float upper)
{
    const float a = wrap_positive(angle);
    const float low = wrap_positive(lower);
    const float high = wrap_positive(upper);
    if (!(high >= low))
    {
        if (!(a > high))
        {
            return a;
        }
        if (!(low > a))
        {
            return a;
        }
    }
    else if (!(low > a) && !(a > high))
    {
        return a;
    }
    // Tail jump into 0x00100B80 with the wrapped values.
    return runtime_nearest_arc_bound(a, low, high);
}

// Xbox 0x00092120
float runtime_turn_within_arc(float heading, float target, float limit)
{
    float turn = wrap_positive(heading - target + half_pi);
    if (turn >= pi)
    {
        turn = wrap_positive(turn + pi);
    }
    return runtime_clamp_angle_to_arc(turn - half_pi, 0.f - limit, limit) + target;
}

// Xbox 0x000921E0
float runtime_facing_weight(float heading, float target)
{
    float turn = wrap_positive(heading - target);
    if (turn >= pi)
    {
        turn = tau - turn;
    }
    if (turn >= half_pi)
    {
        turn = pi - turn;
    }
    turn = (turn - std::bit_cast<float>(0x3EB2B8C3u)) * std::bit_cast<float>(0x3FD189F2u); // DAT_0032B160, DAT_0032B2B4.
    const float weight = 1.f - turn;
    if (0.f > weight)
    {
        return 0.f;
    }
    if (weight > 1.f)
    {
        return 1.f;
    }
    return weight;
}

// Xbox 0x00091A90
void walk_target_object_sizes(ObjectSizes& output)
{
    output.instance = 0x7C;
    output.parameters = 0x60;
}

// Xbox 0x00091820
void find_runtime_walk_target_records(std::span<u8> rows, RuntimeBodyStateContext& context)
{
    auto& records = walk_target_records[rows.data() - field::rows.offset]; // The state that holds the rows.
    for (size_t row = 0; row < 4; ++row)
    {
        const u32 name_address = get(record(rows, rows_field::rows, row), row_field::name);
        const char* name = name_address == 0 ? nullptr
            : reinterpret_cast<const char*>(context.resolve(name_address, 0).data());
        // The 0x0003AAA0 pair: the held actor's +0xDB0 variant first, then the plain name.
        const AnimationRecord* animation = find_runtime_body_animation(name, context.actions);
        records[row] = animation;
        set(record(rows, rows_field::rows, row), row_field::found, animation != nullptr ? 1 : 0);
    }
}

// Xbox 0x000A6380
bool initialize_runtime_walk_target(std::span<u8> state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context)
{
    const auto body = context.body;
    const auto rows = record(state, field::rows);
    const auto source = record(parameters, parameter::rows);
    std::memcpy(rows.data(), source.data(), rows.size());
    find_runtime_walk_target_records(rows, context);
    set(state, field::arrived_key, get(parameters, parameter::arrived_key));
    set(state, field::stopped_key, get(parameters, parameter::stopped_key));
    set(state, field::settle_turn, get(parameters, parameter::settle_turn));
    set(state, field::direction, 0xFFFFFFFFu);
    set(state, field::band, 0);
    put_float(body, 0x71C, std::bit_cast<float>(0x3F0EFA35u)); // DAT_0032B220.
    put_float(body, 0x720, std::bit_cast<float>(0x40F5BE0Cu)); // DAT_0032B554.
    put_float(body, 0x724, 4.f); // DAT_0032B1B0.
    if (const u32 target = alert_mind_target(context.mind); target != 0)
    {
        if (!context.actor_position)
        {
            throw Error("walk-target state has no actor position view for the mind target");
        }
        store_target_position(body, context.actor_position(target)); // 0x000412A0, EDI body +0x3C.
    }
    body[0x38] = 1;
    body[0x39] = 1;
    context.bits->bytes[6] |= 0x80;
    set(state, field::arrival_delay, 1.f); // DAT_0032B0F8.
    return true;
}

// Xbox 0x000A64D0
void destroy_runtime_walk_target(std::span<u8> state, RuntimeBodyStateContext& context)
{
    const auto body = context.body;
    put_float(body, 0x71C, std::bit_cast<float>(0x3F0EFA35u)); // DAT_0032B220.
    put_float(body, 0x720, std::bit_cast<float>(0x41490FDBu)); // DAT_0032B21C.
    put_float(body, 0x724, 6.f); // DAT_0032B218.
    context.bits->bytes[6] &= 0x7F;
    walk_target_records.erase(state.data());
}

const AnimationRecord* runtime_walk_target_record(std::span<const u8> state, u32 index)
{
    const auto found = walk_target_records.find(state.data());
    if (found == walk_target_records.end() || index >= found->second.size())
    {
        throw Error("walk-target state has no record for its row");
    }
    return found->second[index];
}

// Xbox 0x00019840
void steer_runtime_body_by_stick(RuntimeWalkTargetTick& context)
{
    const auto body = context.body;
    std::array<float, 2> axes{};
    if (context.input == nullptr || get(body, body_field::move_holds) != 0 ||
        get(body, body_field::move_hold_time) != 0.f)
    {
        axes = {0.f, 0.f};
    }
    else
    {
        runtime_input_axes(*context.input, axes, context.input_state); // 0x001085F0, EAX actor +0x960.
    }
    const long double root = retail_square_root(axes[1] * axes[1] + axes[0] * axes[0]);
    float magnitude = float(root);
    if (!(1.0L > root)) // DAT_0032B0F8.
    {
        magnitude = 1.f;
    }
    float angle;
    if (axes[0] == 0.f && axes[1] == 0.f)
    {
        angle = 0.f;
    }
    else
    {
        angle = float(retail_atan2(axes[1], axes[0]));
    }
    float side = 0.f;
    float forward = 0.f;
    if (magnitude > 0.f)
    {
        put_float(body, 0x33C, angle);
        const float scale = surface_mode_scale(body, context.mode_scales);
        const float speed = runtime_body_step_speed(body, context.actor, context.tables) * scale * magnitude;
        side = float(retail_sin(angle) * speed);
        forward = float(retail_cos(angle) * speed);
    }
    put_float(body, 0x318, side);
    put_float(body, 0x31C, 0.f);
    put_float(body, 0x320, forward);
}

// Xbox 0x00091A00
i32 runtime_walk_target_band(std::span<const u8> rows, RuntimeWalkTargetTick& context)
{
    const float speed = actor_flat_speed(context.actor);
    float fraction = speed / runtime_body_step_speed(context.body, context.actor, context.tables);
    if (!(1.f > fraction)) // DAT_0032B0F8.
    {
        fraction = 1.f;
    }
    for (i32 band = 0; band < 2; ++band)
    {
        const auto limits = get(rows, rows_field::band_limits);
        if (fraction > limits.at(size_t(band)) && limits.at(size_t(band) + 1) >= fraction)
        {
            return band;
        }
    }
    return 0;
}

// Xbox 0x000A6140
u32 runtime_walk_target_side(float turn)
{
    // DAT_00428700: {pi/2, -pi/2}.
    static const std::array<float, 2> bounds{half_pi, -half_pi};
    for (u32 side = 0; side < 2; ++side)
    {
        if (runtime_angle_in_arc(turn, bounds[(side + 1) & 1], bounds[side]))
        {
            return side;
        }
    }
    return 0;
}

// Xbox 0x000A61B0
u32 runtime_walk_target_direction(RuntimeWalkTargetTick& context)
{
    // x87: body +0x33C - actor +0x18, stored as a float.
    const float turn = float(static_cast<long double>(read_f32(context.body, 0x33C))
        - static_cast<long double>(context.actor.euler.y));
    return runtime_walk_target_side(wrap_signed(turn));
}

// Xbox 0x000A6950
long double runtime_walk_target_distance(RuntimeWalkTargetTick& context)
{
    const auto body = context.body;
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0, ESI actor.
    const float dx = position.x - read_f32(body, 0x3C);
    const float dy = position.y - read_f32(body, 0x40);
    const float dz = position.z - read_f32(body, 0x44);
    const long double sum = (static_cast<long double>(dz) * dz + static_cast<long double>(dy) * dy)
        + static_cast<long double>(dx) * dx;
    return std::sqrt(sum);
}

// Xbox 0x000A67C0
void aim_runtime_walk_target(std::span<const u8> state, RuntimeWalkTargetTick& context)
{
    const auto body = context.body;
    const u8 alert = context.mind.empty() ? 0 : get(context.mind, mind_field::target_mode);
    const Vec3 target{read_f32(body, 0x3C), read_f32(body, 0x40), read_f32(body, 0x44)};
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0, ESI actor.
    const Vec3 direction{target.x - position.x, target.y - position.y, target.z - position.z};
    float angle;
    runtime_direction_angle(direction, angle); // 0x00105F90, EAX direction, EBX angle.
    const float heading = read_f32(body, 0x33C);
    const float turned = step_runtime_angle_toward(angle, heading, float(alert != 0 ? 1 : 0));
    const long double distance = runtime_walk_target_distance(context); // Kept in x87.
    float limit = float((distance - 12.0L) // DAT_0032B470.
        * static_cast<long double>(std::bit_cast<float>(0x3F490FDBu)) // DAT_0032B2D0.
        + static_cast<long double>(std::bit_cast<float>(0x3E32B8C2u))); // DAT_0032B46C.
    const float upper = std::bit_cast<float>(0x3F75BE0Bu); // DAT_0032B468.
    const float lower = std::bit_cast<float>(0x3E32B8C2u); // DAT_0032B46C.
    if (limit > upper)
    {
        limit = upper;
    }
    else if (lower > limit)
    {
        limit = lower;
    }
    const float facing = runtime_turn_within_arc(heading, turned, limit);
    put_float(body, 0x734, 0.f);
    put_float(body, 0x72C, 0.f);
    put_float(body, 0x730, facing);
    if (get(state, field::settle_turn) != 0)
    {
        settle_runtime_body_turn(body, context.actor); // 0x00134720, ESI body.
    }
    static_cast<void>(runtime_facing_weight(heading, turned)); // The result is discarded.
}

// Xbox 0x000A6280
void select_runtime_walk_target_row(std::span<u8> state, RuntimeWalkTargetTick& context)
{
    const i32 band = runtime_walk_target_band(record(state, field::rows), context);
    const u32 direction = runtime_walk_target_direction(context);
    if (get(state, field::band) != u32(band) || get(state, field::direction) != direction)
    {
        const u32 row = direction + u32(band) * 2;
        const AnimationRecord* record = runtime_walk_target_record(state, row); // State +0x1C + row * 0x10.
        context.actions.animation.retain_position = 1; // Actor +0x84.
        if (record == nullptr)
        {
            throw Error("walk-target row record is null; 0x0001AFA0 reads its +0x117");
        }
        start_runtime_body_animation_record(*record, context.actions);
    }
    set(state, field::band, u32(band));
    set(state, field::direction, direction);
}

// Xbox 0x000918C0
float runtime_walk_target_rate(std::span<const u8> rows, u32 direction, u32 band, float speed,
    RuntimeWalkTargetTick& context)
{
    const auto row = record(rows, rows_field::rows, size_t(direction + band * 2));
    const float lower_rate = get(row, row_field::lower_rate);
    const float upper_rate = get(row, row_field::upper_rate);
    const float scale = surface_mode_scale(context.body, context.mode_scales);
    const float base = runtime_body_step_speed(context.body, context.actor, context.tables) * scale;
    const auto limits = get(rows, rows_field::band_limits);
    const float lower = limits.at(band) * base;
    const float upper = limits.at(size_t(band) + 1) * base;
    if (upper == lower)
    {
        return upper_rate;
    }
    const float value = (speed - lower) / (upper - lower) * (upper_rate - lower_rate) + lower_rate;
    if (upper_rate > lower_rate)
    {
        if (value > upper_rate)
        {
            return upper_rate;
        }
        if (!(lower_rate > value))
        {
            return value;
        }
        return lower_rate;
    }
    if (upper_rate > value)
    {
        return upper_rate;
    }
    if (!(value > lower_rate))
    {
        return value;
    }
    return lower_rate;
}

// Xbox 0x000A62D0
void retime_runtime_walk_target(std::span<const u8> state, RuntimeWalkTargetTick& context)
{
    const float speed = actor_flat_speed(context.actor);
    float duration;
    if (!runtime_body_stride_duration(speed, context.actor, context.actions.animation, duration))
    {
        const float rate = runtime_walk_target_rate(record(state, field::rows), get(state, field::direction),
            get(state, field::band), speed, context);
        duration = rate * runtime_body_turn_ratio(context.body, context.actor, 0, context.tables,
            context.mode_scales);
    }
    if (context.actions.animation.controller != nullptr)
    {
        // EDI actor +0x50, EAX 0: controller slot 0.
        auto& controller = *context.actions.animation.controller;
        retime_mina_playback(controller.slots.at(0).player.get(), duration, controller.dirty);
    }
}

// Xbox 0x000A6640
void settle_runtime_walk_target(std::span<u8> state, RuntimeWalkTargetTick& context)
{
    const auto body = context.body;
    const float x = context.actor.anchor.x;
    const float z = context.actor.anchor.z;
    const float vx = get(body, body_field::velocity).x;
    const float vy = get(body, body_field::velocity).y;
    const float vz = get(body, body_field::velocity).z;
    const bool alert = !context.mind.empty() && get(context.mind, mind_field::target_mode) != 0;
    if (!alert && get(state, field::arrival_delay) == 0.f) // DAT_0032B100.
    {
        // x87: body +0x730 - actor +0x18, stored as a float.
        const float turn = float(static_cast<long double>(get(body, body_field::euler).y)
            - static_cast<long double>(context.actor.euler.y));
        const float magnitude = animation_absolute(wrap_signed(turn));
        if (std::bit_cast<float>(0x3E860A92u) >= magnitude) // DAT_0032B340.
        {
            set(state, field::arrived, 1);
            set(state, body_state_field::next_key, get(state, field::arrived_key));
        }
        return;
    }
    const float actor_speed = z * z + x * x;
    const float desired = (vz * vz + vy * vy) + vx * vx;
    const float scale = surface_mode_scale(body, context.mode_scales);
    const float threshold = runtime_body_step_speed(body, context.actor, context.tables) * scale
        * std::bit_cast<float>(0x3C23D70Au); // DAT_0032B124.
    if (desired == 0.f && threshold * threshold >= actor_speed)
    {
        set(state, body_state_field::next_key, get(state, field::stopped_key));
    }
}

// Xbox 0x000A6520
void tick_runtime_walk_target(std::span<u8> state, RuntimeWalkTargetTick& context)
{
    const float step = runtime_actor_rate(context.actor) * runtime_frame_step(context.clock);
    set(state, body_state_field::elapsed, step + get(state, body_state_field::elapsed));
    const float timer = get(state, field::arrival_delay);
    if (timer > 0.f)
    {
        const float left = timer - runtime_actor_rate(context.actor) * runtime_frame_step(context.clock);
        set(state, field::arrival_delay, left);
        if (0.f >= left)
        {
            set(state, field::arrival_delay, 0.f);
        }
    }
    if (const u32 target = alert_mind_target(context.mind); target != 0)
    {
        store_target_position(context.body, context.actor_position(target)); // 0x000412A0, EDI body +0x3C.
    }
    steer_runtime_body_by_stick(context);
    aim_runtime_walk_target(state, context);
    select_runtime_walk_target_row(state, context);
    retime_runtime_walk_target(state, context);
    settle_runtime_walk_target(state, context);
}

// Xbox 0x00091C10
bool initialize_runtime_fly_alert(std::span<u8> state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context)
{
    namespace fly = fly_alert_field;
    namespace fly_parameter = fly_alert_parameter;
    const auto body = context.body;
    const auto rows = record(state, fly::rows);
    const auto source = record(parameters, fly_parameter::rows);
    std::memcpy(rows.data(), source.data(), rows.size());
    find_runtime_walk_target_records(rows, context); // 0x00091820
    set(state, fly::arrived_key, get(parameters, fly_parameter::arrived_key));
    set(state, fly::stopped_key, get(parameters, fly_parameter::stopped_key));
    set(state, fly::direction, 0xFFFFFFFFu);
    set(state, fly::band, 0);
    put_float(body, 0x71C, std::bit_cast<float>(0x3F0EFA35u)); // DAT_0032B220.
    put_float(body, 0x720, std::bit_cast<float>(0x4075BE0Cu)); // DAT_0032B640.
    put_float(body, 0x724, 4.f); // DAT_0032B1B0.
    if (const u32 target = alert_mind_target(context.mind); target != 0)
    {
        if (!context.actor_position)
        {
            throw Error("fly-alert state has no actor position view for the mind target");
        }
        store_target_position(body, context.actor_position(target)); // 0x000412A0, EDI body +0x3C.
    }
    body[0x38] = 1;
    body[0x39] = 1;
    set(state, fly::arrival_delay, 1.f); // DAT_0032B0F8.
    // The inlined 0x0003AAA0 pair: the held actor's +0xDB0 variant first, then the plain name.
    const u32 name_address = get(parameters, fly_parameter::hover);
    const char* name = name_address == 0 ? nullptr
        : reinterpret_cast<const char*>(context.resolve(name_address, 0).data());
    const AnimationRecord* hover = find_runtime_body_animation(name, context.actions);
    fly_alert_hovers[state.data()] = hover;
    set(state, fly::hover, hover != nullptr ? 1u : 0u);
    if (hover != nullptr)
    {
        start_runtime_body_animation_record(*hover, context.actions); // 0x0001AFA0, ESI body
    }
    return true;
}

// Xbox 0x00091D90
void destroy_runtime_fly_alert(std::span<const u8> state, RuntimeBodyStateContext& context)
{
    const auto body = context.body;
    put_float(body, 0x71C, std::bit_cast<float>(0x3F0EFA35u)); // DAT_0032B220.
    put_float(body, 0x720, std::bit_cast<float>(0x41490FDBu)); // DAT_0032B21C.
    put_float(body, 0x724, 6.f); // DAT_0032B218.
    if (get(state, fly_alert_field::hover) != 0)
    {
        const AnimationRecord* hover = fly_alert_hovers[state.data()];
        AnimationController* controller = context.actions.animation.controller.get(); // Actor +0x50.
        const i32 slot = hover == nullptr ? 0 : i32(hover->slot); // Record +0x117.
        if (controller != nullptr && slot != 0)
        {
            AnimationControllerSlot& entry = controller->slots.at(size_t(slot));
            if (entry.player != nullptr)
            {
                entry.fade_duration = 0.25f; // +0x1C, DAT_0032B104.
                entry.fade_mode = 3; // +0x18.
            }
            controller->dirty = true; // +0x5C.
        }
    }
    fly_alert_hovers.erase(state.data());
    walk_target_records.erase(state.data());
}

// Xbox 0x000917B0
u32 runtime_fly_alert_side(float turn)
{
    // DAT_00428674: {pi/2, -pi/2}.
    static const std::array<float, 2> bounds{half_pi, -half_pi};
    for (u32 side = 0; side < 2; ++side)
    {
        if (runtime_angle_in_arc(turn, bounds[(side + 1) & 1], bounds[side])) // 0x001009A0
        {
            return side;
        }
    }
    return 0;
}

// Xbox 0x00091990, EAX body
u32 runtime_fly_alert_direction(RuntimeWalkTargetTick& context)
{
    // x87: body +0x33C - actor +0x18, stored as a float.
    const float turn = float(static_cast<long double>(read_f32(context.body, 0x33C))
        - static_cast<long double>(context.actor.euler.y));
    return runtime_fly_alert_side(wrap_signed(turn));
}

// Xbox 0x000923B0
void steer_runtime_fly_alert(RuntimeWalkTargetTick& context)
{
    const auto body = context.body;
    const bool settled = get(body, body_field::move_holds) == 0 && get(body, body_field::move_hold_time) == 0.f;
    std::array<float, 2> axes{};
    if (context.input != nullptr && settled)
    {
        runtime_input_axes(*context.input, axes, context.input_state); // 0x001085F0, EAX input
    }
    const long double root = retail_square_root(axes[1] * axes[1] + axes[0] * axes[0]); // 0x00152B30
    float magnitude = float(root);
    if (!(1.0L > root)) // DAT_0032B0F8.
    {
        magnitude = 1.f;
    }
    float heading = 0.f;
    if (axes[0] != 0.f || axes[1] != 0.f)
    {
        heading = float(retail_atan2(axes[1], axes[0])); // 0x001530E0
    }
    float lift = 0.f;
    if (settled)
    {
        if (context.input == nullptr)
        {
            throw Error("0x000923B0 reads the third input pair through address zero");
        }
        std::array<float, 2> third{};
        runtime_input_tertiary_axes(*context.input, third, context.input_state); // 0x00108AB0
        lift = third[1];
    }
    float side = 0.f;
    float forward = 0.f;
    if (magnitude > 0.f)
    {
        const float scale = surface_mode_scale(body, context.mode_scales);
        const float speed = runtime_body_step_speed(body, context.actor, context.tables) * scale * magnitude; // 0x000259C0
        side = float(retail_sin(heading) * speed); // 0x00153320
        forward = float(retail_cos(heading) * speed); // 0x00153230
    }
    const float scale = surface_mode_scale(body, context.mode_scales);
    const float rise = runtime_body_step_speed(body, context.actor, context.tables) * scale * lift; // 0x000259C0
    put_float(body, 0x318, side);
    put_float(body, 0x31C, rise);
    put_float(body, 0x320, forward);
}

// Xbox 0x00092280, EBX body
void aim_runtime_fly_alert(RuntimeWalkTargetTick& context)
{
    const auto body = context.body;
    if (const u32 target = alert_mind_target(context.mind); target != 0)
    {
        store_target_position(body, context.actor_position(target)); // 0x000412A0, EDI body +0x3C.
        const float yaw = context.actor.euler.y; // Actor +0x18.
        const float heading = runtime_body_point_heading(body, context.actor); // 0x000197B0, EAX body
        static_cast<void>(animation_remainder(heading - yaw, tau)); // 0x00152BE0; the result is discarded.
        put_float(body, 0x734, 0.f);
        put_float(body, 0x72C, 0.f);
        put_float(body, 0x730, heading);
        float limit = float((runtime_walk_target_distance(context) - 12.0L) // 0x000A6950, DAT_0032B470.
            * static_cast<long double>(std::bit_cast<float>(0x3F490FDBu)) // DAT_0032B2D0.
            + static_cast<long double>(std::bit_cast<float>(0x3E32B8C2u))); // DAT_0032B46C.
        const float upper = std::bit_cast<float>(0x3F75BE0Bu); // DAT_0032B468.
        const float lower = std::bit_cast<float>(0x3E32B8C2u); // DAT_0032B46C.
        if (limit > upper)
        {
            limit = upper;
        }
        else if (lower > limit)
        {
            limit = lower;
        }
        const float facing = read_f32(body, 0x33C);
        static_cast<void>(runtime_turn_within_arc(facing, heading, limit)); // 0x00092120; the result is discarded.
        static_cast<void>(runtime_facing_weight(facing, heading)); // 0x000921E0; the result is discarded.
    }
    put_float(body, 0x734, 0.f);
    put_float(body, 0x72C, 0.f);
}

// Xbox 0x00091B10, EAX body, EDI state
void select_runtime_fly_alert_row(std::span<u8> state, RuntimeWalkTargetTick& context)
{
    namespace fly = fly_alert_field;
    const i32 band = runtime_walk_target_band(record(state, fly::rows), context); // 0x00091A00
    const u32 direction = runtime_fly_alert_direction(context); // 0x00091990
    if (get(state, fly::band) != u32(band) || get(state, fly::direction) != direction)
    {
        const AnimationRecord* row = runtime_walk_target_record(state, direction + u32(band) * 2); // State +0x1C.
        context.actions.animation.retain_position = 1; // Actor +0x84.
        if (row == nullptr)
        {
            throw Error("fly-alert row record is null; 0x0001AFA0 reads its +0x117");
        }
        start_runtime_body_animation_record(*row, context.actions); // 0x0001AFA0
    }
    set(state, fly::band, u32(band));
    set(state, fly::direction, direction);
}

// Xbox 0x00091B60, ESI body
void retime_runtime_fly_alert(std::span<const u8> state, RuntimeWalkTargetTick& context)
{
    namespace fly = fly_alert_field;
    const float speed = actor_flat_speed(context.actor);
    float duration;
    if (!runtime_body_stride_duration(speed, context.actor, context.actions.animation, duration)) // 0x00133C90
    {
        const float rate = runtime_walk_target_rate(record(state, fly::rows), get(state, fly::direction),
            get(state, fly::band), speed, context); // 0x000918C0
        duration = rate * runtime_body_turn_ratio(context.body, context.actor, 0, context.tables,
            context.mode_scales); // 0x00133D50
    }
    if (context.actions.animation.controller != nullptr)
    {
        auto& controller = *context.actions.animation.controller;
        retime_mina_playback(controller.slots.at(0).player.get(), duration, controller.dirty); // 0x00030DB0, EAX 0
    }
}

// Xbox 0x00091FA0, EAX body, EDI state
void settle_runtime_fly_alert(std::span<u8> state, RuntimeWalkTargetTick& context)
{
    namespace fly = fly_alert_field;
    const auto body = context.body;
    const float x = context.actor.anchor.x;
    const float z = context.actor.anchor.z;
    const float vx = get(body, body_field::velocity).x;
    const float vy = get(body, body_field::velocity).y;
    const float vz = get(body, body_field::velocity).z;
    const bool alert = !context.mind.empty() && get(context.mind, mind_field::target_mode) != 0;
    if (!alert && get(state, fly::arrival_delay) == 0.f) // DAT_0032B100.
    {
        // x87: body +0x730 - actor +0x18, stored as a float.
        const float turn = float(static_cast<long double>(get(body, body_field::euler).y)
            - static_cast<long double>(context.actor.euler.y));
        const float magnitude = animation_absolute(wrap_signed(turn));
        if (std::bit_cast<float>(0x3E860A92u) >= magnitude) // DAT_0032B340.
        {
            set(state, fly::arrived, 1);
            set(state, body_state_field::next_key, get(state, fly::arrived_key));
        }
        return;
    }
    const float actor_speed = z * z + x * x;
    const float desired = (vz * vz + vy * vy) + vx * vx;
    const float scale = surface_mode_scale(body, context.mode_scales);
    const float threshold = runtime_body_step_speed(body, context.actor, context.tables) * scale
        * std::bit_cast<float>(0x3C23D70Au); // DAT_0032B124.
    if (desired == 0.f && threshold * threshold >= actor_speed)
    {
        set(state, body_state_field::next_key, get(state, fly::stopped_key));
    }
}

// Xbox 0x00091E10
void tick_runtime_fly_alert(std::span<u8> state, RuntimeWalkTargetTick& context, u8 ground)
{
    namespace fly = fly_alert_field;
    const float step = runtime_actor_rate(context.actor) * runtime_frame_step(context.clock);
    set(state, body_state_field::elapsed, step + get(state, body_state_field::elapsed));
    const float timer = get(state, fly::arrival_delay);
    if (timer > 0.f)
    {
        const float left = timer - runtime_actor_rate(context.actor) * runtime_frame_step(context.clock);
        set(state, fly::arrival_delay, left);
        if (0.f >= left)
        {
            set(state, fly::arrival_delay, 0.f);
        }
    }
    if (const u32 target = alert_mind_target(context.mind); target != 0)
    {
        store_target_position(context.body, context.actor_position(target)); // 0x000412A0, EDI body +0x3C.
    }
    steer_runtime_fly_alert(context); // 0x000923B0, EBX input
    if (get(context.body, body_field::move_holds) == 0 && get(context.body, body_field::move_hold_time) == 0.f &&
        (ground & 0x40) == 0)
    {
        aim_runtime_fly_alert(context); // 0x00092280
    }
    select_runtime_fly_alert_row(state, context); // 0x00091B10
    retime_runtime_fly_alert(state, context); // 0x00091B60
    settle_runtime_fly_alert(state, context); // 0x00091FA0
}

// Xbox 0x00091F70
u16 runtime_walk_target_next_key(std::span<const u8> state, ActorAnimationState& animation)
{
    const u16 key = get(state, body_state_field::next_key);
    if (key != 0 && get(state, field::arrived) != 0)
    {
        animation.retain_position = 1; // State +8 -> body +8 -> actor +0x84.
    }
    return key;
}
}
