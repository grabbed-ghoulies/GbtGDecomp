#include "warlock_strategy.h"
#include "actor_clock.h"
#include "actor_live_attachments.h"
#include "actor_model_anchors.h"
#include "actor_render_registration.h"
#include "actor_transform.h"
#include "body_animation_commands.h"
#include "body_damage_notices.h"
#include "body_message_helpers.h"
#include "body_timers.h"
#include "entity.h"
#include "giant_hand_strategy.h"
#include "player_children.h"
#include "player_goal_cleanup.h"
#include "player_messages.h"
#include "player_mind_goals.h"
#include "player_state.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "../audio/listener_settings.h"
#include "../camera/display_view.h"
#include "../camera/entity_camera.h"
#include "../camera/view_initialization.h"
#include "../navigation/occupancy.h"
#include "../navigation/spawn_candidates.h"

#include <array>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = warlock_strategy_field;
namespace parameter = warlock_strategy_parameter;

constexpr u32 kRecordAddress = 0x003034F8;
// The record at 0x003034F8: +0 .. +0xC the attacks allowed at full, three quarters, half and a quarter energy, +0x10 ..
// +0x1C the body's +0x80 with each, then the modes' seconds, the stretch and the entrance.
constexpr std::array<i32, 4> kAttackLimits{1, 2, 3, 4}; // 0x003034F8 +0 .. +0xC.
constexpr std::array<float, 4> kAttackRates{30.f, 30.f, 30.f, 30.f}; // 0x003034F8 +0x10 .. +0x1C.
constexpr float kGoneTime = 2.f; // +0x20, mode 3.
constexpr float kEnteringHoldTime = 0.f; // +0x24, mode 1 on the first appearance.
constexpr float kVanishTime = 0.5f; // +0x28, modes 2 and 4.
constexpr float kHoldTime = 3.5f; // +0x2C, mode 1.
constexpr float kHitVanishTime = 1.f; // +0x30, mode 2 after a hit.
constexpr float kHitHoldTime = 0.f; // +0x34, mode 1 after a hit.
constexpr float kNoticeTime = 3.72f; // +0x38, until callout 0x53.
constexpr float kHitNoticeTime = 1.72f; // +0x3C, after a hit.
constexpr float kStretchShare = 0.6f; // +0x40, the share of a vanish spent stretching.
constexpr float kSquashWidth = 2.f; // +0x44, the X and Z scale between the squash and the stretch.
constexpr float kSquashHeight = 0.5f; // +0x48, the Y scale there.
constexpr float kStretchLift = 25.f; // +0x4C, how far the stretch rises.
constexpr float kEntranceTime = 1.5f; // +0x50.
constexpr float kEntranceLookHeight = 1.f; // +0x54, of the body's +0x884 height.
constexpr float kScareRadius = 250.f; // +0x58, 0x0013B660's radius.
constexpr float kStretchHeight = 5.f; // DAT_0032B184.
constexpr float kRunOut = 0.001f; // DAT_0032B1A4, also the stretch's thinnest X and Z scale.
constexpr float kOne = 1.f; // DAT_0032B0F8.
constexpr float kOpaque = 255.f; // DAT_0032B158.
constexpr float kPi = 3.1415927f; // DAT_0032B148.
constexpr float kContactChance = 0.5f; // DAT_0032B0F4.
constexpr const char* kEntranceCamera = "warlock entrance"; // 0x00324328.

RuntimePlayerStrategyGoals& goals_of(RuntimeWarlockStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeWarlockStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// Actor +0x95C.
std::span<u8> body_of(RuntimeWarlockStrategy& context)
{
    return goals_of(context).entry.body;
}

Actor& actor_of(RuntimeWarlockStrategy& context)
{
    return goals_of(context).entry.actor;
}

u32 actor_address(RuntimeWarlockStrategy& context)
{
    return read_u32(strategy_of(context), 8);
}

EntityBitBlock& body_bits(RuntimeWarlockStrategy& context)
{
    EntityBitBlock* bits = goals_of(context).body_bits; // Body +0x15C.
    if (bits == nullptr)
    {
        throw Error("warlock strategy reads the body +0x15C bit block of a body with none");
    }
    return *bits;
}

RuntimeBodyStateContext body_state(RuntimeWarlockStrategy& context)
{
    if (!context.body_state)
    {
        throw Error("warlock strategy: 0x0001FEB0's body views are not bound");
    }
    return context.body_state();
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeWarlockStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeWarlockStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* goal, RuntimeWarlockStrategy& context)
{
    load_runtime_mind_goal(goal, goals_of(context).entry.goals); // 0x0002A8E0
}

bool can_resume(RuntimeWarlockStrategy& context)
{
    return runtime_player_can_resume_strategy(goals_of(context).entry.actor_bytes, goals_of(context).resume)
        != 0; // 0x00028D50, EAX actor
}

// 0x0003BE20 (or 0x0003C040, ECX actor, EDX id) over the actor's +0x8C callouts, with no creation options.
void callout(u32 id, RuntimeWarlockStrategy& context)
{
    if (!context.actor_callouts)
    {
        throw Error("warlock strategy: 0x0003BE20 over the actor's callouts is not bound");
    }
    context.actor_callouts(actor_address(context), id, nullptr);
}

// A one-word message (0x4A) through the body's vtable +0xC.
void tell_body(u32 id, RuntimeWarlockStrategy& context)
{
    std::array<u32, 0x42> message{id};
    context.object_message(goals_of(context).address_of(body_of(context).data()), message);
}

// The inlined 0x0002B9F0 on a strategy timer: the frame step (0 while DAT_004312DC is above 0) times the actor's
// rate (0 while actor +0x930 is above 0, else +0x92C), stopping at 0.
void count_down(float& timer, RuntimeWarlockStrategy& context)
{
    const float step = runtime_frame_step(goals_of(context).clock);
    const float rate = runtime_actor_rate(actor_of(context));
    if (0.f < timer)
    {
        timer = timer - rate * step;
        if (!(0.f < timer)) // COMISS; JC keeps it.
        {
            timer = 0.f;
        }
    }
}

// Xbox 0x0002B410, ESI actor: the actor's world height.
float actor_height(const Actor& actor)
{
    return actor_world_position(actor).y; // 0x000412A0
}

// Xbox 0x000C4130, EAX strategy: not holding, and the body bits leave it free to react (byte 4 bits 0x10 and 0x20,
// byte 3 bit 8 clear).
bool can_react(RuntimeWarlockStrategy& context)
{
    if (read_u16(strategy_of(context), 0xE) == 0x401)
    {
        return false;
    }
    const EntityBitBlock& bits = body_bits(context);
    if ((bits.bytes[4] & 0x10) != 0 || (bits.bytes[3] & 8) != 0)
    {
        return false;
    }
    return ((u8(~bits.bytes[4]) >> 5) & 1) != 0;
}

// Xbox 0x000C3440, ESI strategy
void show(RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::shown) != 0)
    {
        return;
    }
    NativeActorFactoryInstance& view = context.actor_view;
    register_actor_render_bins(view.render); // 0x0003CAE0
    if (auto* record = static_cast<ActorLiveAttachment*>(view.live_attachments.next); record != nullptr) // +0x8F8.
    {
        suspend_actor_live_attachment(*record, false); // Its +0x18 count one lower.
    }
    register_actor_model_anchors(actor_of(context), view.anchors); // 0x0003EEB0
    body_bits(context).bytes[7] &= 0xEF;
    set(strategy, field::shown, 1u);
}

// Xbox 0x000C33E0, ESI strategy
void hide(RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::shown) == 0 || (body_bits(context).bytes[3] & 8) != 0)
    {
        return;
    }
    hide_runtime_actor(context.actor_view, body_bits(context)); // 0x001364B0, EBX actor
    set(strategy, field::shown, 0u);
}

// Xbox 0x000C3410, EDI strategy
void leave_world(RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::solid) == 0 || (body_bits(context).bytes[3] & 8) != 0)
    {
        return;
    }
    RuntimeBodyStateContext body = body_state(context);
    remove_runtime_actor_collision(context.actor_view, body); // 0x00136540, ESI actor
    set(strategy, field::solid, 0u);
}

// Xbox 0x000C3490, EDI strategy: back in the world's collision, and the body hears 0x4A.
void enter_world(RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::solid) != 0)
    {
        return;
    }
    RuntimeBodyStateContext body = body_state(context);
    restore_runtime_actor_collision(get(strategy, field::model_flags), get(strategy, field::model_mode),
        u8(get(strategy, field::shape_mode)), context.actor_view, body); // 0x001365C0, ESI actor, EAX flags
    tell_body(0x4A, context); // Body vtable +0xC.
    set(strategy, field::solid, 1u);
}

// Xbox 0x000C4170, EDI strategy: with story bit 0x1D (byte 3 bit 0x20) the entrance camera looks at the warlock from
// where the display view stands, with the scene paused and the body latched.
void start_entrance(RuntimeWarlockStrategy& context)
{
    if ((context.story_bits()[3] & 0x20) == 0)
    {
        return;
    }
    const std::span<u8> strategy = strategy_of(context);
    Actor& actor = actor_of(context);
    increment_runtime_body_latches(body_of(context), actor, context.latches); // The inlined 0x0001FC40.
    actor.model_flags &= 0xFFFFFFFDu; // Actor +0x1F0.
    set(strategy, field::entrance_timer, kEntranceTime); // Record +0x50.
    if (context.view == nullptr)
    {
        throw Error("0x000C4170 reads the display view through address zero");
    }
    set(strategy, field::eye, context.view->position); // DAT_005077A8 +0.
    initialize_camera_view(context.entrance_view, kEntranceCamera, 1); // 0x000496C0, EDX +0x80, CL 1
    link_camera_view(0, context.entrance_view); // 0x00048C20, ESI 0
    camera_bind_named_view(0, kEntranceCamera); // 0x00048CF0, EBX name
    context.entrance_view.position = get(strategy, field::eye); // +0x88.
    change_runtime_actor_pause(u8(0xFFFF), 0, context.pause()); // 0x00117400
    set(strategy, field::entrance, 1u);
}

// Xbox 0x000C4440, EDI strategy
void end_entrance(RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::entrance) == 0)
    {
        return;
    }
    Actor& actor = actor_of(context);
    decrement_runtime_body_latches(body_of(context), actor, context.latches); // The inlined 0x0001FD20.
    actor.model_flags = 2; // Actor +0x1F0.
    camera_unlink_view(&context.entrance_view); // 0x00048C70, EAX +0x80
    change_runtime_actor_pause(u8(0xFFFF), 1, context.pause()); // 0x00117400
    set(strategy, field::entrance, 0u);
    set(strategy, field::entering, 1u);
}

// Xbox 0x000C42F0, EBX strategy: the entrance runs out, or the camera turns from its eye toward the warlock.
void update_entrance(RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::entrance) == 0)
    {
        return;
    }
    float timer = get(strategy, field::entrance_timer);
    count_down(timer, context);
    set(strategy, field::entrance_timer, timer);
    if (kRunOut > get(strategy, field::entrance_timer)) // COMISS; JBE keeps it.
    {
        end_entrance(context); // 0x000C4440, EDI strategy
        return;
    }
    const Actor& actor = actor_of(context);
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const Vec3 eye = get(strategy, field::eye);
    // The look height is the body's actor (+8) Y scale times the body's +0x884 and the record's +0x54; the actor's
    // own height is not added.
    const float dy = actor.radius_y * read_f32(body_of(context), 0x884) * kEntranceLookHeight - eye.y;
    float yaw = 0.f;
    float pitch = 0.f;
    direction_to_yaw_pitch(position.x - eye.x, dy, position.z - eye.z, yaw, pitch); // 0x00105E20, EAX direction
    context.entrance_view.rotation.x = 0.f - pitch; // +0x94.
    context.entrance_view.rotation.y = yaw + kPi; // +0x98.
    context.entrance_view.rotation.z = 0.f; // +0x9C.
}

// Xbox 0x000C3370: to a random point of a random record of mask 0x20 (0x00124A10, 0x00110F50); the body's +0x18
// follows and the body hears 0x4A.
void move_away(RuntimeWarlockStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const OccupancyRecord* record = random_occupancy_record(0x20, goals.random); // 0x00124A10
    if (record == nullptr)
    {
        return;
    }
    Vec3 point{};
    if (!select_occupancy_record_point(*record, point, goals.random)) // 0x00110F50
    {
        return;
    }
    Actor& actor = actor_of(context);
    actor_set_world_position(actor, point); // 0x00041730, ESI point, EDI actor
    const Vec3 position = actor_world_position(actor); // 0x000412A0, ESI body +8, EDI body +0x18
    std::memcpy(body_of(context).data() + 0x18, &position, 12);
    tell_body(0x4A, context); // Body vtable +0xC.
}

// Xbox 0x000C34E0, ECX strategy: the actor's alpha (+0x19B) is the time left's share of the mode's time out of 255,
// fading out while vanishing and in (255 less it) while reappearing.
void fade(bool vanishing, RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const float duration = get(strategy, field::duration);
    float level = kOpaque;
    if (duration != 0.f) // UCOMISS; JNP takes a zero duration as opaque.
    {
        const float value = get(strategy, field::timer) / duration * kOpaque;
        level = value > kOpaque ? kOpaque : 0.f > value ? 0.f : value;
    }
    const u8 alpha = u8(truncate_runtime_float(vanishing ? level : kOpaque - level)); // 0x001673D8
    u32& color = context.actor_view.startup.color; // Actor +0x198.
    color = (color & 0x00FFFFFFu) | (u32(alpha) << 24);
}

// The ramp 0x000C3570 repeats: `done` of `span` taken from `from` toward `to`, held between them; a zero span is `to`.
float ramp(float done, float span, float from, float to)
{
    if (span == 0.f) // UCOMISS; JNP.
    {
        return to;
    }
    const float value = done / span * (to - from) + from;
    if (to > from)
    {
        return value > to ? to : from > value ? from : value;
    }
    return to > value ? to : value > from ? from : value;
}

// Xbox 0x000C3570, EBX strategy, EAX vanishing: the actor's scale through the vanish or the reappearance. Vanishing,
// it squashes to the record's +0x44 / +0x48 scale and then, over the record's +0x40 share of the time, stretches to 5
// high and 0.001 wide while it rises the record's +0x4C; reappearing runs the other way.
void stretch(bool vanishing, RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    Actor& actor = actor_of(context);
    const float left = get(strategy, field::timer);
    const float duration = get(strategy, field::duration);
    float height = 0.f;
    float width = 0.f;
    float lift = 0.f;
    bool rising = false;
    if (vanishing)
    {
        const float edge = kStretchShare * duration;
        if (edge > left)
        {
            height = ramp(left, edge, kStretchHeight, kSquashHeight);
            width = ramp(left, edge, kRunOut, kSquashWidth);
            lift = ramp(left, edge, kStretchLift, 0.f);
            rising = true;
        }
        else
        {
            height = ramp(left - edge, duration - edge, kSquashHeight, kOne);
            width = ramp(left - edge, duration - edge, kSquashWidth, kOne);
        }
    }
    else
    {
        const float edge = duration - kStretchShare * duration;
        if (left > edge)
        {
            height = ramp(left - edge, duration - edge, kSquashHeight, kStretchHeight);
            width = ramp(left - edge, duration - edge, kSquashWidth, kRunOut);
            lift = ramp(left - edge, duration - edge, 0.f, kStretchLift);
            rising = true;
        }
        else
        {
            height = ramp(left, edge, kOne, kSquashHeight);
            width = ramp(left, edge, kOne, kSquashWidth);
        }
    }
    if (rising)
    {
        Vec3 position = actor_world_position(actor); // 0x000412A0
        position.y = get(strategy, field::height) + lift;
        actor_set_world_position(actor, position); // 0x00041730, ESI point, EDI actor
        const Vec3 placed = actor_world_position(actor); // 0x000412A0, ESI body +8, EDI body +0x18
        std::memcpy(body_of(context).data() + 0x18, &placed, 12);
    }
    if (width != actor.scale_x || height != actor.radius_y || width != actor.radius_xz) // UCOMISS; JP sets.
    {
        actor.scale_x = width; // +0x30.
        actor.radius_y = height; // +0x34.
        actor.radius_xz = width; // +0x38.
        actor.xform_valid = 0; // +0x2A8.
    }
}

// The inlined 0x0013B810 / 0x0003CFD0 loop over a token block: each token's attachment removed, the count zeroed.
void remove_events(size_t offset, RuntimeWarlockStrategy& context)
{
    const std::span<u8> events = strategy_of(context).subspan(offset, 0x24);
    i32 index = 0;
    while (index < std::bit_cast<i32>(read_u32(events, 0)))
    {
        remove_runtime_body_attachment(u16(read_u32(events, size_t(index + 1) * 4)), context.lists); // 0x0003CFD0
        ++index;
    }
    const u32 none = 0;
    std::memcpy(events.data(), &none, 4);
}

// The mode 1 setup shared by 0x000C3160 and the enter of 0x401: the actor's collision state kept, the notice armed,
// the height taken and the hold's times picked (shorter after a hit, none on the first appearance).
void begin_waiting(RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const Actor& actor = actor_of(context);
    set(strategy, field::model_flags, actor.model_flags); // Actor +0x1F0.
    set(strategy, field::model_mode, actor.model_mode); // Actor +0x118.
    set(strategy, field::shape_mode, u32(body_of(context)[0x244]));
    set(strategy, field::noticed, 0u);
    set(strategy, field::height, actor_height(actor)); // 0x0002B410, ESI actor
    if (get(strategy, field::entering) != 0)
    {
        set(strategy, field::timer, kEnteringHoldTime); // Record +0x24.
    }
    else if (get(strategy, field::hit) != 0)
    {
        set(strategy, field::timer, kHitHoldTime); // Record +0x34.
        leave_world(context); // 0x000C3410, EDI strategy
    }
    else
    {
        set(strategy, field::timer, kHoldTime); // Record +0x2C.
    }
    set(strategy, field::notice_timer, get(strategy, field::hit) != 0 ? kHitNoticeTime : kNoticeTime); // +0x3C / +0x38
    set(strategy, field::duration, get(strategy, field::timer));
}

// Xbox 0x000C3160, ECX strategy, EAX mode
void set_mode(u32 mode, RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::mode) == mode)
    {
        return;
    }
    set(strategy, field::mode, mode);
    switch (mode)
    {
    case 1:
        begin_waiting(context);
        return;
    case 2:
    {
        set(strategy, field::timer, get(strategy, field::hit) != 0 ? kHitVanishTime : kVanishTime); // +0x30 / +0x28
        leave_world(context); // 0x000C3410, EDI strategy
        set(strategy, field::duration, get(strategy, field::timer));
        callout(0x52, context); // 0x0003C040, ECX actor, EDX 0x52
        const auto name = get(strategy, field::effect_name);
        set(strategy, field::effect, context.create_effect(name.data(), actor_address(context))); // 0x0003DF40
        return;
    }
    case 3:
    {
        end_entrance(context); // 0x000C4440, EDI strategy
        hide(context); // 0x000C33E0, ESI strategy
        move_away(context); // 0x000C3370
        set(strategy, field::timer, kGoneTime); // Record +0x20.
        set(strategy, field::duration, kGoneTime);
        strategy[0x1A] = 0;
        clear_runtime_player_attachment_events(strategy.subspan(field::first_events.offset, 0x24),
            context.lists); // 0x0013B810, EBX +0xF0
        clear_runtime_player_attachment_events(strategy.subspan(field::second_events.offset, 0x24),
            context.lists); // 0x0013B810, EBX +0x114
        set(strategy, field::noticed, 1u);
        set(strategy, field::hit, 0u);
        if (get(strategy, field::entering) != 0)
        {
            set(strategy, field::entering, 0u);
            decrement_runtime_body_latch_mask(body_of(context), 0x3F, actor_of(context), context.latches); // 0x0001F2F0
        }
        return;
    }
    case 4:
        load_goal(context.goal_reappear, context);
        show(context); // 0x000C3440, ESI strategy
        set(strategy, field::timer, kVanishTime); // Record +0x28.
        set(strategy, field::duration, kVanishTime);
        strategy[0x1A] = 1;
        callout(0x51, context); // 0x0003C040, ECX actor, EDX 0x51
        return;
    case 5:
        if (get(strategy, field::effect) != 0)
        {
            RuntimeBodyTimerRecipient recipient{[&context, effect = get(strategy, field::effect)](
                                                    std::span<const u32> message)
                {
                    context.object_message(effect, message);
                }};
            expire_runtime_body_timer_recipient(recipient); // 0x0013B240, EAX the object
            set(strategy, field::effect, 0u);
        }
        enter_world(context); // 0x000C3490, EDI strategy
        return;
    default:
        return;
    }
}

// Xbox 0x000C3BE0, EAX strategy: the attacks allowed (+0x78) and the body's +0x80 by the energy left (body +0x94 of
// +0x96).
void pick_attack_limit(RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    set(strategy, field::attacks, 0);
    size_t row = 0;
    const u16 full = read_u16(body, 0x96);
    if (full != 0)
    {
        const float share = float(read_u16(body, 0x94)) / float(full);
        if (0.25f > share) // DAT_0032B104.
        {
            row = 3;
        }
        else if (0.5f > share) // DAT_0032B0F4.
        {
            row = 2;
        }
        else if (0.75f > share) // DAT_0032B2AC.
        {
            row = 1;
        }
    }
    set(strategy, field::attack_limit, kAttackLimits[row]);
    std::memcpy(body.data() + 0x80, &kAttackRates[row], 4);
}

// Xbox 0x000C3AC0, ESI strategy
void enter_attack(RuntimeWarlockStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    RuntimeMindTarget* target = nullptr;
    select_runtime_strategy_target(target, 0, goals); // 0x00139AA0, EAX strategy
    if (target == nullptr)
    {
        set(strategy, field::attacks, 0);
        set(strategy, field::attack_limit, 0);
        return;
    }
    goals.room = nullptr; // Actor +0xCA0.
    pick_attack_limit(context); // 0x000C3BE0, EAX strategy
    load_goal(context.goal_attack_prepare, context);
    strategy[0x1A] = 1;
}

// Xbox 0x000C3B20, ECX strategy, EAX actor: without a target it goes back to holding; otherwise, each time the mind
// runs out of requests, another attack, the finish, then holding.
void update_attack(RuntimeWarlockStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    RuntimeMindTarget* target = nullptr;
    if (!select_runtime_strategy_target(target, 0, goals)) // 0x00139AA0, EAX strategy
    {
        const u16 previous = get(strategy, strategy_field::state);
        goals.setup.leaving(goals.strategy, previous, 0x401); // Vtable +0x18.
        set(strategy, strategy_field::state, u16(0x401));
        if (goals.setup.mind == nullptr)
        {
            throw Error("0x000C3B20 clears the requests of a mind it does not have");
        }
        clear_runtime_player_mind_requests(*goals.setup.mind); // 0x00029E80, EBX mind
        goals.setup.entering(goals.strategy, 0x401, previous); // Vtable +0x14.
        return;
    }
    if (requests_pending(context)) // Mind +0x58.
    {
        return;
    }
    const i32 attacks = get(strategy, field::attacks);
    const i32 limit = get(strategy, field::attack_limit);
    if (attacks < limit)
    {
        load_goal(context.goal_attack, context);
        set(strategy, field::attacks, attacks + 1);
        return;
    }
    if (attacks == limit)
    {
        load_goal(context.goal_attack_finish, context);
        set(strategy, field::attacks, attacks + 1);
        return;
    }
    change_state(0x401, context); // 0x0002B8D0, EDI 0x401
}

// Xbox 0x000C2ED0, ECX strategy, EAX actor: the notice runs out once (callout 0x53), then the mode moves on.
void update_hold(RuntimeWarlockStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::noticed) == 0)
    {
        float timer = get(strategy, field::notice_timer);
        count_down(timer, context);
        set(strategy, field::notice_timer, timer);
        if (kRunOut > get(strategy, field::notice_timer)) // COMISS; JBE keeps it.
        {
            // LEA EAX,[EDI + 0x8C] is never null, so the callout always runs.
            callout(0x53, context); // 0x0003BE20, EAX actor +0x8C
            set(strategy, field::noticed, 1u);
        }
    }
    switch (get(strategy, field::mode))
    {
    case 1:
    {
        RuntimeMindTarget* target = nullptr;
        if (select_runtime_strategy_target(target, 0, goals)) // 0x00139AA0, EAX strategy
        {
            notice_runtime_strategy_target(0x1D, context.checks); // 0x0013A430, EAX strategy
        }
        float timer = get(strategy, field::timer);
        count_down_runtime_strategy_timer(timer, goals); // 0x0002B9F0, ECX +0x40
        set(strategy, field::timer, timer);
        if (kRunOut > get(strategy, field::timer) && !runtime_body_bit_set(body_bits(context), 0x24) // 0x0001EF10
            && can_resume(context))
        {
            set_mode(2, context); // 0x000C3160, EAX 2
        }
        return;
    }
    case 2:
    {
        fade(true, context); // 0x000C34E0, 1
        stretch(true, context); // 0x000C3570, EAX 1
        float timer = get(strategy, field::timer);
        const bool done = count_down_runtime_strategy_timer(timer, goals); // 0x0002B9F0, ECX +0x40
        set(strategy, field::timer, timer);
        if (done)
        {
            set_mode(3, context); // 0x000C3160, EAX 3
        }
        return;
    }
    case 3:
        if (can_resume(context))
        {
            float timer = get(strategy, field::timer);
            count_down_runtime_strategy_timer(timer, goals); // 0x0002B9F0, ECX +0x40
            set(strategy, field::timer, timer);
        }
        if (kRunOut > get(strategy, field::timer) && get(strategy, field::mode) != 4)
        {
            set_mode(4, context); // The inlined 0x000C3160, EAX 4.
        }
        return;
    case 4:
    {
        fade(false, context); // 0x000C34E0, 0
        stretch(false, context); // 0x000C3570, EAX 0
        float timer = get(strategy, field::timer);
        const bool done = count_down_runtime_strategy_timer(timer, goals); // 0x0002B9F0, ECX +0x40
        set(strategy, field::timer, timer);
        if (done && get(strategy, field::mode) != 5)
        {
            set_mode(5, context); // The inlined 0x000C3160, EAX 5.
        }
        return;
    }
    case 5:
        if (!requests_pending(context) && can_resume(context)) // Mind +0x58, 0x00028D50
        {
            change_state(0x3EE, context); // 0x0002B8D0, EDI 0x3EE
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000C2DB0, EDI strategy: leaving the hold, the warlock is shown and solid again, its object hears {5, 0}, its
// scale is 1 and its attachment tokens go.
void stop_holding(RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    show(context); // 0x000C3440, ESI strategy
    enter_world(context); // The inlined 0x000C3490.
    if (const u32 effect = get(strategy, field::effect); effect != 0)
    {
        const std::array<u32, 2> message{5, 0};
        context.object_message(effect, message); // Object vtable +0xC.
        set(strategy, field::effect, 0u);
    }
    Actor& actor = actor_of(context);
    if (kOne != actor.scale_x || kOne != actor.radius_y || kOne != actor.radius_xz) // UCOMISS; JP sets.
    {
        actor.scale_x = kOne;
        actor.radius_y = kOne;
        actor.radius_xz = kOne;
        actor.xform_valid = 0; // +0x2A8.
    }
    remove_events(field::first_events.offset, context); // 0x0003CFD0 over +0xF0, EAX actor
    remove_events(field::second_events.offset, context); // 0x0003CFD0 over +0x114, EAX actor
    if (get(strategy, field::mode) != 0)
    {
        set(strategy, field::mode, 0u);
    }
}
}

// Xbox 0x000C3CA0
void warlock_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x000C3D10
bool initialize_runtime_warlock_strategy(std::span<u8> strategy, std::span<const u8> parameters,
    RuntimeWarlockStrategy& context)
{
    set(strategy, field::record, kRecordAddress);
    set(strategy, field::hit, 0u);
    set(strategy, field::entering, 1u);
    increment_runtime_body_latches(body_of(context), actor_of(context), context.latches); // The inlined 0x0001FC40.
    set(strategy, field::solid, 1u);
    set(strategy, field::shown, 1u);
    const auto source = get(parameters, parameter::effect_name);
    std::array<char, 0x80> name{};
    std::strncpy(name.data(), source.data(), name.size() - 1); // The inlined strcpy.
    set(strategy, field::effect_name, name);
    start_entrance(context); // 0x000C4170, EDI strategy
    if (!context.scare_near)
    {
        throw Error("warlock strategy: 0x0013B660 is not bound");
    }
    context.scare_near(actor_of(context), kScareRadius); // The inlined 0x0013B660 with the record's +0x58.
    return true;
}

// Xbox 0x000C3E90
void destroy_runtime_warlock_strategy(RuntimeWarlockStrategy& context)
{
    end_entrance(context); // 0x000C4440, EDI strategy
}

// Xbox 0x000C3EF0
void dispatch_runtime_warlock_message(std::span<const u32> message, RuntimeWarlockStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    switch (message[0])
    {
    case 0x26:
    case 0x27:
        if (get(strategy, field::entering) != 0)
        {
            return;
        }
        break;
    case 0x2C:
        if (can_react(context)) // 0x000C4130, EAX strategy
        {
            change_state(0x3F2, context); // 0x0002B8D0, EDI 0x3F2
        }
        return;
    case 0x31:
        if (can_react(context))
        {
            react_runtime_strategy_watched_actor(message, context.checks, context.views); // 0x001397B0
        }
        return;
    case 0x36:
        notice_runtime_strategy_hostile_target(0x1D, context.checks); // 0x0013A4F0
        return;
    case 0x4D:
        if (message[1] == 6)
        {
            return;
        }
        break;
    case 0x6E:
        if (check_runtime_player_knockout(goals) != 0) // 0x0013A490, EBX strategy
        {
            return;
        }
        if (can_react(context)
            && react_runtime_strategy_contact(message, kContactChance, context.checks) != 0) // 0x001395C0
        {
            return;
        }
        if (runtime_message_lands_on_actor(message, actor_address(context)) // 0x001408A0, EAX message, EDX actor
            && !runtime_body_bit_set(body_bits(context), 0x1B)) // 0x0001EF10, EAX body
        {
            set(strategy, field::hit, 1u);
            change_state(0x401, context); // 0x0002B8D0, EDI 0x401
        }
        return;
    case 0x82:
        if (can_react(context))
        {
            react_runtime_strategy_cower(context.checks); // The tail jump to 0x001398A0.
        }
        return;
    case 0x84:
        if (can_react(context))
        {
            react_runtime_strategy_startle(context.checks); // The tail jump to 0x001399A0.
        }
        return;
    default:
        break;
    }
    dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
}

// Xbox 0x000C3EA0
void update_runtime_warlock_strategy(RuntimeWarlockStrategy& context)
{
    const u16 state = read_u16(strategy_of(context), 0xE);
    update_entrance(context); // 0x000C42F0, EBX strategy
    update_runtime_base_strategy(goals_of(context)); // 0x0002C130
    switch (state)
    {
    case 0x3EE:
        update_attack(context); // The tail jump to 0x000C3B20.
        return;
    case 0x401:
        update_hold(context); // The tail jump to 0x000C2ED0.
        return;
    default:
        return;
    }
}

// Xbox 0x000C40B0
void enter_runtime_warlock_state(u16 state, u16 previous, RuntimeWarlockStrategy& context)
{
    (void)previous;
    enter_runtime_base_strategy_state(goals_of(context).entry, state); // 0x0002BAF0
    switch (state)
    {
    case 0x3EE:
        enter_attack(context); // 0x000C3AC0, ESI strategy
        return;
    case 0x401:
        set_mode(1, context); // The tail jump to 0x000C3160, EAX 1.
        return;
    default:
        return;
    }
}

// Xbox 0x000C40F0
void leave_runtime_warlock_state(u16 state, RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (state == 0x3FC || state == 0x3FF || state == 0x415)
    {
        strategy[0x1C] = u8(strategy[0x1C] - 1);
    }
    switch (state)
    {
    case 0x3EE:
        strategy[0x1A] = 0;
        return;
    case 0x401:
        stop_holding(context); // 0x000C2DB0, EDI strategy
        return;
    default:
        return;
    }
}
}
