#include "warlock_strategy.h"
#include <cstddef>
#include "strategy_fields.h"
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
#include "body_fields.h"

#include <array>
#include <cstring>

namespace gbtg
{
namespace
{
WarlockStrategyState& fields_of(std::span<u8> strategy)
{
    return strategy_view<WarlockStrategyState>(strategy);
}

const WarlockStrategyState& fields_of(std::span<const u8> strategy)
{
    return strategy_view<WarlockStrategyState>(strategy);
}

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
    return strategy_base(strategy_of(context)).actor;
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
    if (strategy_base(strategy_of(context)).state == 0x401)
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
    if (fields_of(strategy).shown != 0)
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
    fields_of(strategy).shown = 1u;
}

// Xbox 0x000C33E0, ESI strategy
void hide(RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).shown == 0 || (body_bits(context).bytes[3] & 8) != 0)
    {
        return;
    }
    hide_runtime_actor(context.actor_view, body_bits(context)); // 0x001364B0, EBX actor
    fields_of(strategy).shown = 0u;
}

// Xbox 0x000C3410, EDI strategy
void leave_world(RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).solid == 0 || (body_bits(context).bytes[3] & 8) != 0)
    {
        return;
    }
    RuntimeBodyStateContext body = body_state(context);
    remove_runtime_actor_collision(context.actor_view, body); // 0x00136540, ESI actor
    fields_of(strategy).solid = 0u;
}

// Xbox 0x000C3490, EDI strategy: back in the world's collision, and the body hears 0x4A.
void enter_world(RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).solid != 0)
    {
        return;
    }
    RuntimeBodyStateContext body = body_state(context);
    restore_runtime_actor_collision(fields_of(strategy).model_flags, fields_of(strategy).model_mode,
        u8(fields_of(strategy).shape_mode), context.actor_view, body); // 0x001365C0, ESI actor, EAX flags
    tell_body(0x4A, context); // Body vtable +0xC.
    fields_of(strategy).solid = 1u;
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
    fields_of(strategy).entrance_timer = kEntranceTime; // Record +0x50.
    if (context.view == nullptr)
    {
        throw Error("0x000C4170 reads the display view through address zero");
    }
    fields_of(strategy).eye = context.view->position; // DAT_005077A8 +0.
    initialize_camera_view(context.entrance_view, kEntranceCamera, 1); // 0x000496C0, EDX +0x80, CL 1
    link_camera_view(0, context.entrance_view); // 0x00048C20, ESI 0
    camera_bind_named_view(0, kEntranceCamera); // 0x00048CF0, EBX name
    context.entrance_view.position = fields_of(strategy).eye; // +0x88.
    change_runtime_actor_pause(u8(0xFFFF), 0, context.pause()); // 0x00117400
    fields_of(strategy).entrance = 1u;
}

// Xbox 0x000C4440, EDI strategy
void end_entrance(RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).entrance == 0)
    {
        return;
    }
    Actor& actor = actor_of(context);
    decrement_runtime_body_latches(body_of(context), actor, context.latches); // The inlined 0x0001FD20.
    actor.model_flags = 2; // Actor +0x1F0.
    camera_unlink_view(&context.entrance_view); // 0x00048C70, EAX +0x80
    change_runtime_actor_pause(u8(0xFFFF), 1, context.pause()); // 0x00117400
    fields_of(strategy).entrance = 0u;
    fields_of(strategy).entering = 1u;
}

// Xbox 0x000C42F0, EBX strategy: the entrance runs out, or the camera turns from its eye toward the warlock.
void update_entrance(RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).entrance == 0)
    {
        return;
    }
    float timer = fields_of(strategy).entrance_timer;
    count_down(timer, context);
    fields_of(strategy).entrance_timer = timer;
    if (kRunOut > fields_of(strategy).entrance_timer) // COMISS; JBE keeps it.
    {
        end_entrance(context); // 0x000C4440, EDI strategy
        return;
    }
    const Actor& actor = actor_of(context);
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const Vec3 eye = fields_of(strategy).eye;
    // The look height is the body's actor (+8) Y scale times the body's +0x884 and the record's +0x54; the actor's
    // own height is not added.
    const float dy = actor.radius_y * body_record(body_of(context)).height_scale * kEntranceLookHeight - eye.y;
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
    std::memcpy(body_of(context).data() + offsetof(BodyRecord, position.x), &position, 12);
    tell_body(0x4A, context); // Body vtable +0xC.
}

// Xbox 0x000C34E0, ECX strategy: the actor's alpha (+0x19B) is the time left's share of the mode's time out of 255,
// fading out while vanishing and in (255 less it) while reappearing.
void fade(bool vanishing, RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const float duration = fields_of(strategy).duration;
    float level = kOpaque;
    if (duration != 0.f) // UCOMISS; JNP takes a zero duration as opaque.
    {
        const float value = fields_of(strategy).timer / duration * kOpaque;
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
    const float left = fields_of(strategy).timer;
    const float duration = fields_of(strategy).duration;
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
        position.y = fields_of(strategy).height + lift;
        actor_set_world_position(actor, position); // 0x00041730, ESI point, EDI actor
        const Vec3 placed = actor_world_position(actor); // 0x000412A0, ESI body +8, EDI body +0x18
        std::memcpy(body_of(context).data() + offsetof(BodyRecord, position.x), &placed, 12);
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
    fields_of(strategy).model_flags = actor.model_flags; // Actor +0x1F0.
    fields_of(strategy).model_mode = actor.model_mode; // Actor +0x118.
    fields_of(strategy).shape_mode = u32(body_record(body_of(context)).push_mode);
    fields_of(strategy).noticed = 0u;
    fields_of(strategy).height = actor_height(actor); // 0x0002B410, ESI actor
    if (fields_of(strategy).entering != 0)
    {
        fields_of(strategy).timer = kEnteringHoldTime; // Record +0x24.
    }
    else if (fields_of(strategy).hit != 0)
    {
        fields_of(strategy).timer = kHitHoldTime; // Record +0x34.
        leave_world(context); // 0x000C3410, EDI strategy
    }
    else
    {
        fields_of(strategy).timer = kHoldTime; // Record +0x2C.
    }
    fields_of(strategy).notice_timer = fields_of(strategy).hit != 0 ? kHitNoticeTime : kNoticeTime; // +0x3C / +0x38
    fields_of(strategy).duration = fields_of(strategy).timer;
}

// Xbox 0x000C3160, ECX strategy, EAX mode
void set_mode(u32 mode, RuntimeWarlockStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).mode == mode)
    {
        return;
    }
    fields_of(strategy).mode = mode;
    switch (mode)
    {
    case 1:
        begin_waiting(context);
        return;
    case 2:
    {
        fields_of(strategy).timer = fields_of(strategy).hit != 0 ? kHitVanishTime : kVanishTime; // +0x30 / +0x28
        leave_world(context); // 0x000C3410, EDI strategy
        fields_of(strategy).duration = fields_of(strategy).timer;
        callout(0x52, context); // 0x0003C040, ECX actor, EDX 0x52
        const auto name = fields_of(strategy).effect_name;
        fields_of(strategy).effect = context.create_effect(name.data(), actor_address(context)); // 0x0003DF40
        return;
    }
    case 3:
    {
        end_entrance(context); // 0x000C4440, EDI strategy
        hide(context); // 0x000C33E0, ESI strategy
        move_away(context); // 0x000C3370
        fields_of(strategy).timer = kGoneTime; // Record +0x20.
        fields_of(strategy).duration = kGoneTime;
        strategy_base(strategy).attacking = 0;
        clear_runtime_player_attachment_events(retail_bytes(fields_of(strategy).first_events),
            context.lists); // 0x0013B810, EBX +0xF0
        clear_runtime_player_attachment_events(retail_bytes(fields_of(strategy).second_events),
            context.lists); // 0x0013B810, EBX +0x114
        fields_of(strategy).noticed = 1u;
        fields_of(strategy).hit = 0u;
        if (fields_of(strategy).entering != 0)
        {
            fields_of(strategy).entering = 0u;
            decrement_runtime_body_latch_mask(body_of(context), 0x3F, actor_of(context), context.latches); // 0x0001F2F0
        }
        return;
    }
    case 4:
        load_goal(context.goal_reappear, context);
        show(context); // 0x000C3440, ESI strategy
        fields_of(strategy).timer = kVanishTime; // Record +0x28.
        fields_of(strategy).duration = kVanishTime;
        strategy_base(strategy).attacking = 1;
        callout(0x51, context); // 0x0003C040, ECX actor, EDX 0x51
        return;
    case 5:
        if (fields_of(strategy).effect != 0)
        {
            RuntimeBodyTimerRecipient recipient{[&context, effect = fields_of(strategy).effect](
                                                    std::span<const u32> message)
                {
                    context.object_message(effect, message);
                }};
            expire_runtime_body_timer_recipient(recipient); // 0x0013B240, EAX the object
            fields_of(strategy).effect = 0u;
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
    fields_of(strategy).attacks = 0;
    size_t row = 0;
    const u16 full = body_record(body).max_energy;
    if (full != 0)
    {
        const float share = float(body_record(body).energy) / float(full);
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
    fields_of(strategy).attack_limit = kAttackLimits[row];
    std::memcpy(body.data() + offsetof(BodyRecord, unk_80), &kAttackRates[row], 4);
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
        fields_of(strategy).attacks = 0;
        fields_of(strategy).attack_limit = 0;
        return;
    }
    goals.room = nullptr; // Actor +0xCA0.
    pick_attack_limit(context); // 0x000C3BE0, EAX strategy
    load_goal(context.goal_attack_prepare, context);
    strategy_base(strategy).attacking = 1;
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
        const u16 previous = strategy_base(strategy).state;
        goals.setup.leaving(goals.strategy, previous, 0x401); // Vtable +0x18.
        strategy_base(strategy).state = u16(0x401);
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
    const i32 attacks = fields_of(strategy).attacks;
    const i32 limit = fields_of(strategy).attack_limit;
    if (attacks < limit)
    {
        load_goal(context.goal_attack, context);
        fields_of(strategy).attacks = attacks + 1;
        return;
    }
    if (attacks == limit)
    {
        load_goal(context.goal_attack_finish, context);
        fields_of(strategy).attacks = attacks + 1;
        return;
    }
    change_state(0x401, context); // 0x0002B8D0, EDI 0x401
}

// Xbox 0x000C2ED0, ECX strategy, EAX actor: the notice runs out once (callout 0x53), then the mode moves on.
void update_hold(RuntimeWarlockStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).noticed == 0)
    {
        float timer = fields_of(strategy).notice_timer;
        count_down(timer, context);
        fields_of(strategy).notice_timer = timer;
        if (kRunOut > fields_of(strategy).notice_timer) // COMISS; JBE keeps it.
        {
            // LEA EAX,[EDI + 0x8C] is never null, so the callout always runs.
            callout(0x53, context); // 0x0003BE20, EAX actor +0x8C
            fields_of(strategy).noticed = 1u;
        }
    }
    switch (fields_of(strategy).mode)
    {
    case 1:
    {
        RuntimeMindTarget* target = nullptr;
        if (select_runtime_strategy_target(target, 0, goals)) // 0x00139AA0, EAX strategy
        {
            notice_runtime_strategy_target(0x1D, context.checks); // 0x0013A430, EAX strategy
        }
        float timer = fields_of(strategy).timer;
        count_down_runtime_strategy_timer(timer, goals); // 0x0002B9F0, ECX +0x40
        fields_of(strategy).timer = timer;
        if (kRunOut > fields_of(strategy).timer && !runtime_body_bit_set(body_bits(context), 0x24) // 0x0001EF10
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
        float timer = fields_of(strategy).timer;
        const bool done = count_down_runtime_strategy_timer(timer, goals); // 0x0002B9F0, ECX +0x40
        fields_of(strategy).timer = timer;
        if (done)
        {
            set_mode(3, context); // 0x000C3160, EAX 3
        }
        return;
    }
    case 3:
        if (can_resume(context))
        {
            float timer = fields_of(strategy).timer;
            count_down_runtime_strategy_timer(timer, goals); // 0x0002B9F0, ECX +0x40
            fields_of(strategy).timer = timer;
        }
        if (kRunOut > fields_of(strategy).timer && fields_of(strategy).mode != 4)
        {
            set_mode(4, context); // The inlined 0x000C3160, EAX 4.
        }
        return;
    case 4:
    {
        fade(false, context); // 0x000C34E0, 0
        stretch(false, context); // 0x000C3570, EAX 0
        float timer = fields_of(strategy).timer;
        const bool done = count_down_runtime_strategy_timer(timer, goals); // 0x0002B9F0, ECX +0x40
        fields_of(strategy).timer = timer;
        if (done && fields_of(strategy).mode != 5)
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
    if (const u32 effect = fields_of(strategy).effect; effect != 0)
    {
        const std::array<u32, 2> message{5, 0};
        context.object_message(effect, message); // Object vtable +0xC.
        fields_of(strategy).effect = 0u;
    }
    Actor& actor = actor_of(context);
    if (kOne != actor.scale_x || kOne != actor.radius_y || kOne != actor.radius_xz) // UCOMISS; JP sets.
    {
        actor.scale_x = kOne;
        actor.radius_y = kOne;
        actor.radius_xz = kOne;
        actor.xform_valid = 0; // +0x2A8.
    }
    remove_events(offsetof(WarlockStrategyState, first_events), context); // 0x0003CFD0 over +0xF0, EAX actor
    remove_events(offsetof(WarlockStrategyState, second_events), context); // 0x0003CFD0 over +0x114, EAX actor
    if (fields_of(strategy).mode != 0)
    {
        fields_of(strategy).mode = 0u;
    }
}
}

// Xbox 0x000C3CA0
void warlock_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(WarlockStrategyState);
    output.parameters = parameter::kSize;
}

// Xbox 0x000C3D10
bool initialize_runtime_warlock_strategy(std::span<u8> strategy, std::span<const u8> parameters,
    RuntimeWarlockStrategy& context)
{
    fields_of(strategy).record = kRecordAddress;
    fields_of(strategy).hit = 0u;
    fields_of(strategy).entering = 1u;
    increment_runtime_body_latches(body_of(context), actor_of(context), context.latches); // The inlined 0x0001FC40.
    fields_of(strategy).solid = 1u;
    fields_of(strategy).shown = 1u;
    const auto source = get(parameters, parameter::effect_name);
    std::array<char, 0x80> name{};
    std::strncpy(name.data(), source.data(), name.size() - 1); // The inlined strcpy.
    fields_of(strategy).effect_name = name;
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
        if (fields_of(strategy).entering != 0)
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
            fields_of(strategy).hit = 1u;
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
    const u16 state = strategy_base(strategy_of(context)).state;
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
        --strategy_base(strategy).pause_count;
    }
    switch (state)
    {
    case 0x3EE:
        strategy_base(strategy).attacking = 0;
        return;
    case 0x401:
        stop_holding(context); // 0x000C2DB0, EDI strategy
        return;
    default:
        return;
    }
}
}
