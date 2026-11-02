#include "body_hit_state.h"
#include "player_mind_targets.h"
#include "actor_transform.h"
#include "actor_shape_bounds.h"
#include "entity_collision.h"
#include "body_damage_motion.h"
#include "body_move_action_slots.h"
#include "body_walk_alert_tick.h"
#include "player_goal_cleanup.h"
#include "player_mind_events.h"
#include "player_mind_feedback.h"
#include "../audio/body_sound_commands.h"
#include "gbtg/anim.h"
#include "body_fields.h"

#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_half(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

void adjust_body_counters(std::span<u8> body, i32 step)
{
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + step);
    }
}

// The inline 0x00095E70: body +0x6C allows the next action once the target was hit, no event window is open and
// no 0x6F notice arrived (EAX state).
void update_runtime_hit_permission(std::span<const u8> state, std::span<u8> body)
{
    if (read_u32(state, 0x2C) != 0 && read_u32(state, 0x30) == 0 && read_u32(state, 0x60) == 0)
    {
        body[0x6C] = 1;
        return;
    }
    body[0x6C] = 0;
}

// The slot-0 MINA player of actor +0x50, or null.
const MinaPlayer* slot_player(const ActorAnimationState& animation)
{
    const AnimationController* controller = animation.controller.get();
    if (controller == nullptr)
    {
        return nullptr;
    }
    return controller->slots.at(0).player.get();
}

// Body +0x84 +0x18 when it is set and nonzero, otherwise DAT_0032B0F4.
float hit_window(std::span<const u8> hit)
{
    if (hit.data() != nullptr)
    {
        const float window = read_f32(hit, 0x18);
        if (!(window == 0.f))
        {
            return window;
        }
    }
    return 0.5f; // DAT_0032B0F4.
}

// The inline surface-mode scale: DAT_00217EC0[mode] when body +0x878 has the mode's bit, otherwise 1.
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

// 0x001085F0 over the given input word: a null word reads its kind from address zero.
void read_input_axes(const RuntimeDialogueInputDevice* input, std::array<float, 2>& axes,
    const RuntimeDialogueInputState& state, std::span<const u8> address_zero)
{
    if (input != nullptr)
    {
        runtime_input_axes(*input, axes, state);
        return;
    }
    const u32 kind = read_u32(address_zero, 0);
    if (kind - 1 <= 4)
    {
        throw Error("hit state input word is null but address zero holds an input kind");
    }
    axes = {0.f, 0.f};
}
}

// Xbox 0x001013A0
float step_runtime_angle_toward(float target, float current, float fraction)
{
    const float tau = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
    const long double first = animation_remainder(target - current, tau);
    float turn = float(first);
    if (static_cast<long double>(std::bit_cast<float>(0xC0490FDBu)) > first) // DAT_0032B29C.
    {
        turn += tau;
    }
    if (turn >= std::bit_cast<float>(0x40490FDBu)) // DAT_0032B148.
    {
        turn -= tau;
    }
    const long double second = animation_remainder(turn * fraction + current, tau);
    float result = float(second);
    if (0.0L > second) // DAT_0032B100.
    {
        result += tau;
    }
    return result;
}

// Xbox 0x00095720
void hit_state_object_sizes(ObjectSizes& output)
{
    output.instance = 0x70;
    output.parameters = 0x28;
}

// Xbox 0x000957A0
bool initialize_runtime_hit_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeHitStateLifecycle& context)
{
    const auto body = context.body;
    body[0x38] = 1;
    body[0x39] = 1;
    put_word(body, 0x330, read_u32(body, 0x334));
    set(body, body_field::ease_rate, 7.f); // DAT_0032B284.
    set(body, body_field::ease_rate_swaps, get(body, body_field::ease_rate_swaps) + 1);
    body[0x6A] = u8(body[0x6A] + 1);
    const auto target = context.target;
    if (target.data() != nullptr && target[0x14] == 0)
    {
        put_word(context.actor_bytes, 0x11C, get(target, mind_target_field::actor));
    }
    // 0x00041820 with ESI actor and EDI state +0x38.
    const Vec3 euler = actor_world_euler(context.actor);
    std::memcpy(state.data() + 0x38, &euler, 12);
    put_word(state, 0x48, read_u32(parameters, 0xC));
    adjust_body_counters(body, 1);
    put_word(state, 0x44, get(body, body_field::attached));
    put_word(state, 0x34, read_u32(parameters, 0x10));
    put_word(state, 0x54, read_u32(parameters, 0x14));
    put_word(state, 0x58, read_u32(parameters, 0x18));
    std::memcpy(state.data() + 0x64, parameters.data() + 0x1C, 2);
    put_word(state, 0x68, read_u32(parameters, 0x20));
    put_word(state, 0x6C, read_u32(parameters, 0x24));
    put_float(state, 0x4C, 0.f);
    put_float(state, 0x50, 0.f);
    put_word(state, 0x5C, get(body, body_field::attached) != 0 ? 1 : 0);
    return true;
}

// Xbox 0x000958A0
void destroy_runtime_hit_state(std::span<u8> state, RuntimeHitStateLifecycle& context)
{
    const auto body = context.body;
    const u32 bit = read_u32(state, 0x6C);
    if (bit != 0)
    {
        context.bits->bytes[bit >> 3] &= u8(~(1u << (bit & 7)));
    }
    body[0x6A] = u8(body[0x6A] - 1);
    put_word(body, 0x334, read_u32(body, 0x330));
    body[0x6C] = 0;
    set(body, body_field::ease_rate_swaps, get(body, body_field::ease_rate_swaps) - 1);
    adjust_body_counters(body, -1);
    put_word(context.actor_bytes, 0x11C, 0);
}

// Xbox 0x00095C90
void message_runtime_hit_state(std::span<u8> state, std::span<const u8> message, RuntimeHitStateMessages& context)
{
    const auto body = context.body;
    const auto target = context.target;
    // 0x000961C0: EDX the reply, EAX the contact, ECX the hit actor, then the id and the actor to compare.
    const auto report = [&](u32 reply, u32 hit_actor, u32 id, u32 compared)
    {
        if (reply == 0)
        {
            return;
        }
        g_actor_creation_options.flags = 0;
        if (context.contact != nullptr)
        {
            g_actor_creation_options.position = *context.contact;
        }
        g_actor_creation_options.flags = context.contact != nullptr ? 1 : 0;
        if (hit_actor != 0xFFFFFF74u)
        {
            context.callouts(hit_actor, id);
        }
        if (target.data() != nullptr && compared == get(target, mind_target_field::actor))
        {
            put_word(state, 0x2C, 1);
            update_runtime_hit_permission(state, body);
        }
    };
    switch (read_u32(message, 0))
    {
    case 7:
        // 0x00095E30: an event whose +0xC is 1 opens (message +0x10 zero) or closes a window.
        if (read_u16(context.event, 0xC) == 1)
        {
            const u32 count = read_u32(message, 0x10) == 0 ? read_u32(state, 0x30) + 1 : read_u32(state, 0x30) - 1;
            put_word(state, 0x30, count);
            update_runtime_hit_permission(state, body);
        }
        return;
    case 0x41:
        report(read_u32(message, 0x28), read_u32(message, 0x14), read_u32(state, 0x34), read_u32(message, 0x14));
        return;
    case 0x5A:
        report(read_u32(message, 0x2C), context.body_attached, 0x35, read_u32(message, 0x18));
        return;
    case 0x6E:
        if (target.data() != nullptr && read_u32(message, 0x10) != 0
            && get(target, mind_target_field::actor) == read_u32(message, 4) && context.actor_address != read_u32(message, 8))
        {
            put_word(state, 0x28, 1);
        }
        return;
    case 0x70:
        // 0x00028C80 reads actor +0x958 -> +0x14 after the receiver test.
        if (read_u32(message, 8) != context.actor_address)
        {
            return;
        }
        [[fallthrough]];
    case 0x6F:
        if (target.data() != nullptr && read_u32(message, 4) == get(target, mind_target_field::actor))
        {
            put_word(state, 0x60, 1);
            update_runtime_hit_permission(state, body);
        }
        return;
    case 0x7C:
        if (read_u32(message, 4) == read_u32(state, 0x44))
        {
            put_word(state, 0x44, 0);
        }
        return;
    default:
        return;
    }
}

// Xbox 0x0003CCB0
bool runtime_actor_has_render_bins(const ActorRenderRegistration& render, u8 hidden)
{
    if (hidden != 0)
    {
        return false;
    }
    for (const auto& bin : render.bins)
    {
        if (bin.prev != nullptr)
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x001346B0
void reset_runtime_body_turn(u32 brake, std::span<u8> body, RuntimeBodyTimerRecipient* first_timer)
{
    if (read_f32(body, 0x168) == 0.f) // DAT_0032B100.
    {
        put_word(body, 0x164, 0);
        if (brake != 0)
        {
            put_float(body, 0x16C, 0.99f); // DAT_0032B228.
        }
    }
    if (read_u32(body, 0x760) != 0 && read_u32(body, 0x754) == 0)
    {
        if (first_timer == nullptr)
        {
            throw Error("body +0x760 timer recipient has no native view");
        }
        const u32 message[]{5, 0};
        first_timer->message(message);
    }
    put_word(body, 0x278, 1);
}

RuntimeBodyNextAction runtime_body_next_action(const RuntimeHitStateTick& context)
{
    return RuntimeBodyNextAction{context.body, context.settings, context.table, context.first_timer,
        context.primary_key, context.secondary_key};
}

// Xbox 0x00134180
void select_runtime_body_next_action(std::span<u8> state, u32 secondary, RuntimeBodyNextAction& context)
{
    const auto body = context.body;
    if (get(body, body_field::energy) == 0 && context.table != nullptr) // Body +0x73C, held as its native view.
    {
        u32 output;
        if (find_runtime_script_table_row(context.table, 0x6A, output) != nullptr)
        {
            put_half(state, 0xC, 0x6A);
            return;
        }
    }
    const auto settings = context.settings;
    const u32 brake = settings.data() == nullptr ? 1 : (settings[0x1A4] == 0 ? 1 : 0);
    reset_runtime_body_turn(brake, body, context.first_timer);
    put_half(state, 0xC, secondary != 0 ? context.secondary_key() : context.primary_key());
}

// Xbox 0x00096260
float runtime_hit_target_gap(float& angle, RuntimeHitStateTick& context)
{
    const auto target = context.target;
    float reach;
    if (context.hit.data() == nullptr)
    {
        reach = 0.f;
    }
    else
    {
        reach = context.actor.radius_xz * read_f32(context.hit, 0x1C);
    }
    if (target.data() == nullptr || reach == 0.f)
    {
        angle = 0.f;
        return 0.f;
    }
    Vec3 aim;
    std::memcpy(&aim, target.data() + 0x20, 12);
    const Vec3 origin = actor_world_position(context.actor);
    runtime_direction_angle(Vec3{aim.x - origin.x, aim.y - origin.y, aim.z - origin.z}, angle);
    // 0x000413B0 with EBX the record +0x20: the x87 distance from the actor's world position.
    const Vec3 from = actor_world_position(context.actor);
    const long double dx = static_cast<long double>(from.x) - aim.x;
    const long double dy = static_cast<long double>(from.y) - aim.y;
    const long double dz = static_cast<long double>(from.z) - aim.z;
    const float distance = float(std::sqrt(dx * dx + dy * dy + dz * dz));
    const float radius = (get(target, mind_target_field::flags) & 0x10) != 0 ? get(target, mind_target_field::shape_radius) : get(target, mind_target_field::secondary_radius);
    const float gap = distance - radius - reach;
    if (-12.f > gap) // DAT_0032B4F4.
    {
        return -12.f;
    }
    if (gap > 12.f) // DAT_0032B470.
    {
        return 12.f;
    }
    return gap;
}

// Xbox 0x00095F30
void lunge_runtime_hit_state(std::span<u8> state, RuntimeHitStateTick& context)
{
    float angle;
    const float gap = runtime_hit_target_gap(angle, context);
    if (gap == 0.f)
    {
        return;
    }
    // A null record reads its +0x14 kind from address zero.
    const u8 kind = context.target.data() == nullptr ? context.address_zero[0x14] : context.target[0x14];
    if (kind > 0 && kind <= 3)
    {
        return;
    }
    const float window = hit_window(context.hit);
    const MinaPlayer* player = slot_player(context.animation);
    const float position = player == nullptr ? 0.f : player->position;
    const float duration = player == nullptr ? 0.f : player->duration;
    const float speed = gap / ((window - position) * duration);
    put_float(state, 0x20, 0.f);
    put_float(state, 0x1C, 0.f);
    put_float(state, 0x18, 0.f);
    const float side = float(retail_sin(angle));
    const float forward = float(retail_cos(angle));
    put_float(state, 0x18, side * speed + read_f32(state, 0x18));
    put_float(state, 0x20, forward * speed + read_f32(state, 0x20));
    put_word(state, 0x24, speed == 0.f ? 0 : 1); // DAT_0032B100.
}

// Xbox 0x0001D5B0
bool turn_runtime_body_toward(const Vec3& target, std::span<const u8> body, const Vec3& current, Vec3& euler,
    std::span<const u8> hit, const Actor& actor, const ActorAnimationState& animation)
{
    const float offset = runtime_body_hit_angle(hit);
    const float window = hit_window(hit);
    const MinaPlayer* player = slot_player(animation);
    const float position = player == nullptr ? 0.f : player->position;
    const Vec3 origin = actor_world_position(actor);
    float yaw;
    runtime_direction_angle(Vec3{target.x - origin.x, target.y - origin.y, target.z - origin.z}, yaw);
    if (position >= window)
    {
        euler = Vec3{0.f, get(body, body_field::euler).y, 0.f};
        return true;
    }
    float fraction;
    if (window == 0.f)
    {
        fraction = 1.f;
    }
    else
    {
        fraction = position / window;
        if (fraction > 1.f)
        {
            fraction = 1.f;
        }
        else if (0.f > fraction)
        {
            fraction = 0.f;
        }
    }
    euler.x = step_runtime_angle_toward(0.f, current.x, fraction);
    euler.y = step_runtime_angle_toward(yaw - offset, current.y, fraction);
    euler.z = step_runtime_angle_toward(0.f, current.z, fraction);
    return true;
}

// Xbox 0x00095EA0
void face_runtime_hit_target(std::span<const u8> state, RuntimeHitStateTick& context)
{
    if (read_u32(state, 0x54) == 0 || context.target.data() == nullptr)
    {
        return;
    }
    Vec3 target;
    std::memcpy(&target, context.target.data() + 0x20, 12);
    Vec3 euler;
    Vec3 current;
    std::memcpy(&current, state.data() + 0x38, 12);
    if (turn_runtime_body_toward(target, context.body, current, euler, context.hit, context.actor,
            context.animation)) // 0x0001D5B0 with state +0x38
    {
        if (runtime_actor_step(context.actor, context.clock) > 0.f)
        {
            // 0x0002B560 with EAX the actor and ECX the turned euler.
            set_runtime_player_actor_rotation(context.actor, context.actor, context.body, euler);
        }
    }
}

// Xbox 0x000963E0
void steer_runtime_hit_body(RuntimeHitStateTick& context)
{
    const auto body = context.body;
    std::array<float, 2> axes{};
    if (read_u32(context.actor_bytes, 0x960) == 0 || get(body, body_field::move_holds) != 0 ||
        get(body, body_field::move_hold_time) != 0.f)
    {
        axes = {0.f, 0.f};
    }
    else
    {
        if (context.own_input == nullptr)
        {
            throw Error("hit state steering has no view of actor +0x960");
        }
        runtime_input_axes(*context.own_input, axes, context.input_state);
    }
    const long double root = retail_square_root(axes[1] * axes[1] + axes[0] * axes[0]);
    float magnitude = 1.f;
    if (1.0L > root) // DAT_0032B0F8.
    {
        magnitude = float(root);
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
    if (magnitude > 1.f)
    {
        magnitude = 1.f;
    }
    else if (!(magnitude > 0.f))
    {
        put_float(body, 0x318, 0.f);
        put_float(body, 0x31C, 0.f);
        put_float(body, 0x320, 0.f);
        return;
    }
    put_float(body, 0x72C, 0.f);
    put_float(body, 0x730, angle);
    put_float(body, 0x734, 0.f);
    put_float(body, 0x33C, angle);
    const float scale = surface_mode_scale(body, context.mode_scales);
    const float speed = runtime_body_step_speed(body, context.actor, context.tables) * scale * magnitude;
    side = float(retail_sin(angle) * speed);
    forward = float(retail_cos(angle) * speed);
    put_float(body, 0x318, side);
    put_float(body, 0x31C, 0.f);
    put_float(body, 0x320, forward);
}

// Xbox 0x00096060
void move_runtime_hit_body(std::span<const u8> state, RuntimeHitStateTick& context)
{
    const auto body = context.body;
    if ((context.bits->bytes[7] & 0x20) != 0)
    {
        steer_runtime_hit_body(context);
        return;
    }
    const Vec3& velocity = context.actor.anchor; // Actor +0x3C.
    if (read_u32(state, 0x58) != 0 && read_u32(state, 0x24) == 0)
    {
        std::memcpy(body.data() + 0x318, &velocity, 12);
        return;
    }
    if (read_u32(state, 0x24) == 0
        && (read_u16(context.actor_bytes, 0xCDC) == 2 || context.actor.mask_flag != 0))
    {
        // 0x0003B260 with ECX 0: slot 0's player has a root track (player +8, set when mina +0x1A is nonzero).
        const MinaPlayer* player = slot_player(context.animation);
        if (player == nullptr || player->mina.empty() || read_u16(player->mina, 0x1A) == 0)
        {
            return;
        }
        const float step = runtime_actor_step(context.actor, context.clock);
        put_float(body, 0x18, velocity.x * step + get(body, body_field::position).x);
        put_float(body, 0x1C, velocity.y * step + get(body, body_field::position).y);
        put_float(body, 0x20, velocity.z * step + get(body, body_field::position).z);
        return;
    }
    if (context.target.data() == nullptr || read_u32(state, 0x2C) != 0 || read_u32(state, 0x24) == 0)
    {
        return;
    }
    const float step = runtime_actor_step(context.actor, context.clock);
    put_float(body, 0x18, get(body, body_field::position).x + read_f32(state, 0x18) * step);
    put_float(body, 0x1C, get(body, body_field::position).y + read_f32(state, 0x1C) * step);
    put_float(body, 0x20, get(body, body_field::position).z + read_f32(state, 0x20) * step);
}

// Xbox 0x00096390
void finish_runtime_hit_state(std::span<u8> state, u32 secondary, RuntimeHitStateTick& context)
{
    if (read_u32(state, 0x2C) == 0)
    {
        // The message is the stack slot pushed at entry: its next words are the return address into the tick
        // and the argument.
        const u32 message[]{0x29, 0x00095BE4, secondary};
        context.actor_message(message);
    }
    const u16 next = read_u16(state, 0x64);
    if (next != 0)
    {
        put_half(state, 0xC, next);
        return;
    }
    RuntimeBodyNextAction pick = runtime_body_next_action(context);
    select_runtime_body_next_action(state, secondary, pick);
}

// Xbox 0x00095940
void tick_runtime_hit_state(std::span<u8> state, RuntimeHitStateTick& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    put_float(state, 0x10, runtime_actor_rate(actor) * runtime_frame_step(context.clock) + read_f32(state, 0x10));
    if (get(body, body_field::attached) != 0 && context.attached_collision != nullptr
        && context.attached_collision->active != nullptr)
    {
        const i32 frequency = context.trails.pal == 0 || context.trails.pal_override != 0 ? 0x3C : 0x32;
        float& timer = *reinterpret_cast<float*>(state.data() + 0x50);
        request_actor_trail(actor, std::bit_cast<float>(0x3E4CCCCDu), timer, frequency / 2, context.trails);
    }
    if (read_f32(state, 0x48) > 0.f
        && runtime_actor_has_render_bins(context.render, context.actor_bytes[0x197])
        && context.collision != nullptr && context.collision->active != nullptr)
    {
        float& timer = *reinterpret_cast<float*>(state.data() + 0x4C);
        request_actor_video_rate_trail(actor, read_f32(state, 0x48), timer, context.trails);
    }
    const MinaPlayer* player = slot_player(context.animation);
    const float position = player == nullptr ? 0.f : player->position;
    const u32 bit = read_u32(state, 0x6C);
    if (bit != 0 && player != nullptr)
    {
        // The animation crossed the event position +0x68 this frame, in its play direction.
        const float mark = read_f32(state, 0x68);
        const float now = player->position;
        bool crossed = false;
        if (now != player->previous)
        {
            float upper = now;
            float lower = player->previous;
            if (0.f > player->speed)
            {
                upper = player->previous;
                lower = now;
            }
            if (upper > lower)
            {
                crossed = mark >= lower && upper > mark;
            }
            else
            {
                crossed = mark >= lower || upper > mark;
            }
        }
        if (crossed)
        {
            context.bits->bytes[bit >> 3] |= u8(1u << (bit & 7));
        }
    }
    if (hit_window(context.hit) * 0.9f > position) // DAT_0032B174.
    {
        if (read_u32(state, 0x28) == 0)
        {
            lunge_runtime_hit_state(state, context);
        }
    }
    else
    {
        Vec3 velocity;
        std::memcpy(&velocity, state.data() + 0x18, 12);
        ease_runtime_planar_velocity(velocity, context.default_position, get(body, body_field::ease_rate),
                                     context.motion_steps);
        std::memcpy(state.data() + 0x18, &velocity, 12);
    }
    face_runtime_hit_target(state, context);
    move_runtime_hit_body(state, context);
    const bool released = read_u32(state, 0x5C) != 0 && get(body, body_field::attached) == 0;
    std::array<float, 2> axes{};
    read_input_axes(context.input, axes, context.input_state, context.address_zero);
    if (axes[1] * axes[1] + axes[0] * axes[0] > 0.01f // DAT_0032B22C.
        && position > 0.75f // DAT_0032B2AC.
        && read_u32(state, 0x30) == 0)
    {
        finish_runtime_hit_state(state, 1, context);
        return;
    }
    if (player != nullptr)
    {
        bool finished = false;
        if (player->mode == 1)
        {
            if (0.f > player->speed)
            {
                finished = 0.f >= player->position;
            }
            else
            {
                finished = player->position >= 1.f; // DAT_0032B0F8.
            }
        }
        if (!finished && (!released || read_u32(state, 0x30) != 0))
        {
            return;
        }
    }
    if (read_u32(state, 0x2C) == 0)
    {
        // The message is the tick's own state argument slot, so its next word is the input argument.
        const u32 message[]{0x29, context.input_word};
        context.actor_message(message);
    }
    const u16 next = read_u16(state, 0x64);
    if (next != 0)
    {
        put_half(state, 0xC, next);
        return;
    }
    RuntimeBodyNextAction pick = runtime_body_next_action(context);
    select_runtime_body_next_action(state, 0, pick);
}

// Xbox 0x00090410
void hit_furnace_state_object_sizes(ObjectSizes& output)
{
    output.instance = 0x30;
    output.parameters = 0x14;
}

// Xbox 0x00096710
bool initialize_runtime_hit_furnace_state(std::span<u8> state, std::span<const u8> parameters, std::span<u8> body,
    RuntimeBodyBitStorage& bits)
{
    put_word(state, 0x20, 0);
    std::memcpy(state.data() + 0x18, parameters.data() + 0xC, 2);
    put_word(state, 0x1C, read_u32(parameters, 0x10));
    body[0x6B] = u8(body[0x6B] + 1);
    adjust_body_counters(body, 1);
    set(body, body_field::move_holds, u16(get(body, body_field::move_holds) + 1));
    put_word(body, 0x330, read_u32(body, 0x334));
    set(body, body_field::ease_rate_swaps, get(body, body_field::ease_rate_swaps) + 1);
    set(body, body_field::ease_rate, 7.f); // DAT_0032B284.
    body[0x38] = 1;
    body[0x39] = 1;
    bits.bytes[2] |= 2;
    if (read_u32(state, 0x1C) != 0)
    {
        bits.bytes[7] |= 1;
    }
    if (body[0xA6] != 0)
    {
        put_float(body, 0xB4, 2.f); // DAT_0032B110.
    }
    // EAX still holds the 1 stored to body +0x38.
    return true;
}

// Xbox 0x000967F0
void destroy_runtime_hit_furnace_state(std::span<const u8> state, std::span<u8> body, RuntimeBodyBitStorage& bits)
{
    set(body, body_field::move_holds, u16(get(body, body_field::move_holds) - 1));
    set(body, body_field::ease_rate_swaps, get(body, body_field::ease_rate_swaps) - 1);
    put_word(body, 0x334, read_u32(body, 0x330));
    body[0x6B] = u8(body[0x6B] - 1);
    adjust_body_counters(body, -1);
    bits.bytes[2] &= 0xFD;
    if (read_u32(state, 0x1C) != 0)
    {
        bits.bytes[7] &= 0xFE;
    }
}

// Xbox 0x00134060
void select_runtime_body_goal_action(std::span<u8> state, RuntimeBodyRecoveryAction& context)
{
    const RuntimePlayerGoalRing* ring = context.ring;
    if (ring != nullptr)
    {
        // An empty ring's front row is address zero.
        const u16 kind = ring->tail == ring->capacity ? read_u16(context.address_zero, 0)
            : read_u16(ring->rows, size_t(ring->head) * ring->stride);
        if (kind != 0 && !runtime_goal_requires_latches(kind))
        {
            if (kind < 2 || kind > 4)
            {
                // The output is the caller's ECX slot; both callees fill it.
                u32 output = 0;
                if (context.goal_latch == 0)
                {
                    context.kind_goal_action(output);
                }
                else
                {
                    context.player_goal_action(output);
                }
                if (output != 0)
                {
                    return;
                }
            }
        }
    }
    const auto settings = context.settings;
    const u32 brake = settings.data() == nullptr ? 1 : (settings[0x1A4] == 0 ? 1 : 0);
    reset_runtime_body_turn(brake, context.body, context.first_timer);
    put_half(state, 0xC, context.primary_key());
}

// Xbox 0x00133F50
void select_runtime_body_recovery_action(std::span<u8> state, u16 key, RuntimeBodyRecoveryAction& context)
{
    const auto body = context.body;
    const u16 level = get(body, body_field::energy);
    bool table_row = false;
    if (level == 0 && context.table != nullptr) // Body +0x73C, held as its native view.
    {
        u32 output;
        table_row = find_runtime_script_table_row(context.table, 0x6A, output) != nullptr;
    }
    bool secondary = false;
    if (read_f32(body, 0x23C) != 0.f)
    {
        if (level < 2 || read_f32(body, 0x174) > 0.f)
        {
            secondary = true;
        }
        else if (read_f32(body, 0x16C) == 0.f)
        {
            secondary = true;
        }
    }
    if (key != 0)
    {
        put_half(state, 0xC, key);
    }
    if (table_row)
    {
        put_half(state, 0xC, 0x6A);
        return;
    }
    if (secondary)
    {
        put_half(state, 0xC, context.secondary_id());
        return;
    }
    if (read_f32(body, 0x240) != 0.f)
    {
        put_half(state, 0xC, context.primary_id());
        return;
    }
    select_runtime_body_goal_action(state, context);
}

// Xbox 0x000965C0
void scorch_runtime_hit_furnace_contacts(std::span<u8> state, RuntimeHitFurnaceTick& context)
{
    const u32 holder = read_u32(context.body, 0x274);
    u32 player = 0;
    if (holder != 0)
    {
        const auto source = context.record_at(holder, 0xCF8);
        player = read_u16(source, 0xCDC) == 2 ? 1 : (read_u32(source, 0x93C) != 0 ? 1 : 0);
    }
    Vec3 position = actor_world_position(context.actor); // 0x000412A0
    const float radius = float(static_cast<long double>(actor_model_sweep_radius(context.actor)) // 0x0003D9A0
        * std::bit_cast<float>(0x3F2AAAABu)); // DAT_0032B204.
    const i32 found = entity_collect_contact_probes(position, radius, 0x140); // 0x00122B00
    if (found != 0)
    {
        context.probe_messages(found, player, context.actor_address); // 0x0013FC50
    }
    if (read_u32(state, 0x20) != 0)
    {
        Vec3 previous{read_f32(state, 0x24), read_f32(state, 0x28), read_f32(state, 0x2C)};
        const i32 swept = entity_record_point_probe(position, previous, radius, 0x140); // 0x001229F0, EBX position
        if (swept != 0)
        {
            context.probe_messages(swept, player, context.actor_address); // 0x0013FC50
        }
    }
    put_float(state, 0x24, position.x);
    put_float(state, 0x28, position.y);
    put_float(state, 0x2C, position.z);
    put_word(state, 0x20, 1);
}

// Xbox 0x00096890
void tick_runtime_hit_furnace_state(std::span<u8> state, RuntimeHitFurnaceTick& context)
{
    const auto body = context.body;
    put_float(state, 0x10,
        runtime_actor_rate(context.actor) * runtime_frame_step(context.clock) + read_f32(state, 0x10));
    if (context.settings.data() != nullptr && read_f32(context.settings, 0x19C) > 1.f) // DAT_0032B0F8.
    {
        scorch_runtime_hit_furnace_contacts(state, context); // 0x000965C0
    }
    put_float(body, 0x734, 0.f);
    put_float(body, 0x72C, 0.f);
    const MinaPlayer* player = slot_player(context.animation);
    const float position = player == nullptr ? 0.f : player->position;
    const auto actor = context.actor_bytes;
    bool steering = false;
    if (read_u32(actor, 0xCE0) != 0 && (read_u16(actor, 0xCDC) == 2 || read_u32(actor, 0x93C) != 0))
    {
        std::array<float, 2> axes{};
        read_input_axes(context.input, axes, context.input_state, context.address_zero);
        steering = axes[1] * axes[1] + axes[0] * axes[0] > 0.01f // DAT_0032B22C.
            && position > 0.45f; // DAT_0032B4F8.
    }
    if (player != nullptr)
    {
        bool finished = false;
        if (player->mode == 1)
        {
            if (0.f > player->speed)
            {
                finished = 0.f >= player->position;
            }
            else
            {
                finished = player->position >= 1.f; // DAT_0032B0F8.
            }
        }
        if (!finished && !steering)
        {
            return;
        }
    }
    select_runtime_body_recovery_action(state, read_u16(state, 0x18), context.recovery);
}
}
