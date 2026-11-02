#include "baron_strategy.h"
#include "mind_fields.h"
#include "actor_clock.h"
#include "actor_transform.h"
#include "body_action_selection.h"
#include "body_animation_commands.h"
#include "body_message_helpers.h"
#include "body_timers.h"
#include "bounded_channels.h"
#include "entity.h"
#include "pirate_strategy.h"
#include "player_goal_cleanup.h"
#include "player_messages.h"
#include "player_mind_feedback.h"
#include "player_mind_goals.h"
#include "player_mind_targets.h"
#include "player_parent_lifecycle.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "player_strategy_goals.h"
#include "gbtg/anim.h"
#include <bit>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace field = baron_strategy_field;
namespace parameter = baron_parameter;

// The host halves of the three timeline words (+0xF0, +0xF8, +0x100), by the strategy's bytes.
std::unordered_map<const u8*, std::array<std::span<const u8>, 3>> baron_timelines;

// The tier rows 0x000A8870 returns: DAT_00302A60, DAT_00302A74 and DAT_00302A88.
constexpr std::array<std::array<float, 5>, 3> kTierRows{{
    {0.f, 0.5f, 0.5f, 10.f, 20.f},
    {0.f, 0.75f, 0.75f, 40.f, 50.f},
    {0.f, 1.f, 1.f, 70.f, 80.f},
}};

// The angry goal's name, written by sprintf into DAT_00508EB8.
char angry_goal[0x40];

void put_u32(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_f32(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

RuntimePlayerStrategyGoals& goals_of(RuntimeBaronStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeBaronStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// Actor +0x958.
std::span<u8> mind_of(RuntimeBaronStrategy& context)
{
    return goals_of(context).entry.events.mind;
}

// Actor +0x95C.
std::span<u8> body_of(RuntimeBaronStrategy& context)
{
    return goals_of(context).entry.body;
}

Actor& actor_of(RuntimeBaronStrategy& context)
{
    return goals_of(context).entry.actor;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeBaronStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeBaronStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* name, RuntimeBaronStrategy& context)
{
    load_runtime_mind_goal(name, goals_of(context).entry.goals); // 0x0002A8E0
}

RuntimeMindTarget* selected_target(RuntimeBaronStrategy& context, bool& found)
{
    RuntimeMindTarget* target = nullptr;
    found = select_runtime_strategy_target(target, 0, goals_of(context)); // 0x00139AA0
    return target;
}

bool can_resume(RuntimeBaronStrategy& context)
{
    return runtime_player_can_resume_strategy(goals_of(context).entry.actor_bytes, goals_of(context).resume)
        != 0; // 0x00028D50, EAX actor
}

bool weapon_free(RuntimeBaronStrategy& context)
{
    return runtime_strategy_weapon_free(strategy_of(context), context.views); // 0x000BEB80, EAX strategy
}

u32 timeline_word(u32 tier, u32 offset, RuntimeBaronStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    return read_u32(strategy, field::timeline_0.offset + tier * field::kTimelineStride + offset);
}

// The timeline row at `row` (a retail address inside the tier's timeline): +0 on, +4 seconds.
std::span<const u8> timeline_row(u32 row, RuntimeBaronStrategy& context)
{
    const u32 tier = get(strategy_of(context), field::tier);
    const std::span<const u8> timeline = baron_timelines[strategy_of(context).data()][tier];
    const u32 start = timeline_word(tier, 0, context);
    if (timeline.data() == nullptr || start == 0 || row < start || row - start + 8 > timeline.size())
    {
        throw Error("the Baron's timeline row is outside the tier's loaded timeline");
    }
    return timeline.subspan(row - start, 8);
}

// The inlined effect start of 0x000A80B0 and 0x000A7880 (+0x58 2): the tier's timeline from its first row.
void begin_timeline(RuntimeBaronStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    set(strategy, field::effect_mode, 2u);
    const u32 tier = runtime_baron_tier(body_of(context));
    set(strategy, field::tier, tier);
    const u32 row = timeline_word(tier, 0, context);
    set(strategy, field::row, row);
    set(strategy, field::row_time, 0.f);
    if (read_u32(timeline_row(row, context), 0) == 0)
    {
        end_runtime_baron_effect(context); // 0x000A75B0
        return;
    }
    start_runtime_baron_effect(context); // 0x000A76C0
}

// The inlined +0x58 1 of 0x000A80B0.
void turn_effect_on(RuntimeBaronStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::effect_mode) == 1)
    {
        return;
    }
    set(strategy, field::effect_mode, 1u);
    start_runtime_baron_effect(context); // 0x000A76C0
}

// The inlined mode end of 0x000A8590: the effect lingers for 0.25 s (DAT_0032B104).
bool turn_effect_off(RuntimeBaronStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::effect_mode) == 0)
    {
        return false;
    }
    set(strategy, field::effect_mode, 0u);
    if (get(strategy, field::effect) == 0)
    {
        return false;
    }
    set(strategy, field::linger, 0.25f);
    return true;
}

// The inlined "0x6E / 0x74" gate: reactions are on (strategy +0x1C clear, the actor's pause group and decay
// entry, and an idle goal ring).
bool reacts(RuntimeBaronStrategy& context)
{
    return runtime_strategy_reactions_enabled(goals_of(context));
}

u32 queue_notice(const char* name, RuntimeBaronStrategy& context)
{
    RuntimeIntroNotices notices = context.intro();
    return queue_runtime_intro_notice(name, notices); // 0x0013FF00, ESI name
}
}

// Xbox 0x000A77E0
void baron_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x000A73A0
void initialize_runtime_baron_strategy(std::span<const u8> parameters, RuntimeBaronStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const char* effect = field_data(parameters, parameter::effect);
    std::memcpy(strategy.data() + field::effect_name.offset, effect, std::strlen(effect) + 1);
    auto& timelines = baron_timelines[strategy.data()];
    for (u32 tier = 0; tier < 3; ++tier)
    {
        const char* name = reinterpret_cast<const char*>(parameters.data() + parameter::timeline_0.offset
            + tier * parameter::kTimelineStride);
        timelines[tier] = {};
        u32 prefix;
        std::memcpy(&prefix, name, 4);
        if (prefix == 0x5F646961) // "aid_"
        {
            timelines[tier] = acquire_animation_resource(context.registry, name); // 0x00035E90
            if (timelines[tier].data() == nullptr)
            {
                resource_error(context.error, 2); // 0x00032700
            }
        }
        const u32 start = timelines[tier].data() == nullptr ? 0u : context.address_of(timelines[tier].data());
        const u32 offset = field::timeline_0.offset + tier * field::kTimelineStride;
        put_u32(strategy, offset, start);
        if (start != 0)
        {
            const u32 size = runtime_body_resource_size(timelines[tier].data(), context.registry.records,
                context.loaded_sizes); // 0x00035D90
            put_u32(strategy, offset + 4, start + (size >> 3) * 8);
        }
    }
    set(strategy, field::row, get(strategy, field::timeline_0));
}

// Xbox 0x000A7440
void destroy_runtime_baron_strategy(RuntimeBaronStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    auto& timelines = baron_timelines[strategy.data()];
    for (u32 tier = 0; tier < 3; ++tier)
    {
        const u32 offset = field::timeline_0.offset + tier * field::kTimelineStride;
        if (read_u32(strategy, offset) != 0)
        {
            release_animation_resource(context.registry, timelines[tier]); // 0x00035C00
            timelines[tier] = {};
            put_u32(strategy, offset, 0u);
        }
    }
    baron_timelines.erase(strategy.data());
}

// Xbox 0x000A7470
void tick_runtime_baron_effect(RuntimeBaronStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u32 mode = get(strategy, field::effect_mode);
    if (mode == 0)
    {
        float linger = get(strategy, field::linger);
        const bool ended = count_down_runtime_strategy_timer(linger, goals); // The inlined 0x0002B9F0.
        set(strategy, field::linger, linger);
        if (ended)
        {
            end_runtime_baron_effect(context); // 0x000A75B0
        }
        return;
    }
    if (mode != 2)
    {
        return;
    }
    const float time = runtime_actor_rate(actor_of(context)) * runtime_frame_step(goals.clock)
        + get(strategy, field::row_time);
    set(strategy, field::row_time, time);
    if (time < read_f32(timeline_row(get(strategy, field::row), context), 4))
    {
        return;
    }
    const u32 tier = get(strategy, field::tier);
    const u32 row = get(strategy, field::row) + 8;
    set(strategy, field::row_time, 0.f);
    set(strategy, field::row, row);
    if (timeline_word(tier, 4, context) <= row)
    {
        set(strategy, field::row, timeline_word(tier, 0, context));
    }
    const u32 on = read_u32(timeline_row(get(strategy, field::row), context), 0);
    if (u32(get(strategy, field::effect) != 0) == on)
    {
        return;
    }
    if (on != 0)
    {
        start_runtime_baron_effect(context); // The inlined 0x000A76C0.
        return;
    }
    end_runtime_baron_effect(context); // 0x000A75B0
}

// Xbox 0x000A75B0
void end_runtime_baron_effect(RuntimeBaronStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const u32 effect = get(strategy, field::effect);
    if (effect == 0)
    {
        return;
    }
    const std::array<u32, 2> message{5, 0};
    context.object_message(effect, message); // Vtable +0xC.
    decrement_runtime_body_latches(body_of(context), actor_of(context), context.latches); // The inlined 0x0001FD20.
    set(strategy, field::effect, 0u);
}

// Xbox 0x000A76C0
void start_runtime_baron_effect(RuntimeBaronStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::effect) != 0)
    {
        return;
    }
    const std::span<u8> body = body_of(context);
    const char* name = field_data(std::span<const u8>{strategy}, field::effect_name);
    set(strategy, field::effect, context.create_effect(name, read_u32(body, 8))); // The inlined 0x0003DF40.
    increment_runtime_body_latches(body, actor_of(context), context.latches); // The inlined 0x0001FC40.
}

// Xbox 0x000A8870
std::span<const float, 5> runtime_baron_tier_row(std::span<const u8> body)
{
    return kTierRows[runtime_baron_tier(body)];
}

// Xbox 0x000A88A0
u32 runtime_baron_tier(std::span<const u8> body)
{
    const u16 energy = read_u16(body, 0x94);
    if (energy > 0xB4)
    {
        return 0;
    }
    return energy < 0xA1 ? 2u : 1u;
}

// Xbox 0x000A8700
bool runtime_baron_target_holds_cane(const RuntimeMindTarget& target, RuntimeBaronStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const u32 cane = get(strategy, field::cane);
    if (cane == 0)
    {
        return false;
    }
    if (context.views.actor_bytes_at(cane)[0x93C] == 0)
    {
        set(strategy, field::cane, 0u);
        return false;
    }
    if (get(target.bytes, mind_target_field::kind) != 0)
    {
        return false;
    }
    return read_u32(context.views.body_of(target.actor).bytes, 0x48) == cane;
}

// Xbox 0x000A8750 (0x3EA), 0x000A87E0 (0x3EE)
void fetch_runtime_baron_cane_for(u16 state, RuntimeBaronStrategy& context)
{
    if (read_u32(body_of(context), 0x48) != 0)
    {
        change_state(state, context);
        return;
    }
    set(strategy_of(context), field::after_fetch, state);
    change_state(0x402, context);
}

// Xbox 0x00134430
bool runtime_target_in_view(const Actor& actor, std::span<const u8> target, float half_angle, float radius)
{
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const float dz = get(target, mind_target_field::shape_position).z - position.z;
    const float dx = get(target, mind_target_field::shape_position).x - position.x;
    if (dz * dz + dx * dx > radius * radius)
    {
        return false;
    }
    const Vec3 point{get(target, mind_target_field::shape_position).x, get(target, mind_target_field::shape_position).y, get(target, mind_target_field::shape_position).z};
    const float turn = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
    float angle = float(runtime_float_remainder(runtime_actor_relative_target_angle(actor, point),
        turn)); // 0x00041A30, 0x00152BE0
    if (angle < -std::bit_cast<float>(0x40490FDBu)) // DAT_0032B29C.
    {
        angle = angle + turn;
    }
    if (std::bit_cast<float>(0x40490FDBu) <= angle) // DAT_0032B148.
    {
        angle = angle - turn;
    }
    return engine_absolute(angle) <= half_angle; // 0x00152BB0
}

// Xbox 0x000A7880
void update_runtime_baron_strategy(RuntimeBaronStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    const u16 state = get(strategy, strategy_field::state);
    alert_runtime_goal_listener(goals); // 0x0013D400
    update_runtime_base_strategy(goals); // 0x0002C130
    tick_runtime_baron_effect(context); // 0x000A7470
    float recovery = get(strategy, field::recovery);
    count_down_runtime_strategy_live_timer(recovery, goals); // The inlined 0x0002BA60 over +0x48.
    set(strategy, field::recovery, recovery);
    bool found;
    RuntimeMindTarget* target = nullptr;
    switch (state)
    {
    case 0x3EA:
    {
        float aloof = get(strategy, field::aloof_time);
        tick_runtime_body_frame_timer(aloof, goals.clock); // 0x0012B2A0, EAX +0x44
        set(strategy, field::aloof_time, aloof);
        target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        const bool in_view = runtime_target_in_view(actor_of(context), target->bytes,
            std::bit_cast<float>(0x40060A92u), 50.f); // 0x00134430, EAX body
        if (!in_view)
        {
            set(strategy, field::aloof_time, 0.f);
        }
        if (get(strategy, field::aloof_time) == 0.f && in_view) // DAT_0032B100.
        {
            fetch_runtime_baron_cane_for(0x3EE, context); // 0x000A87E0
            return;
        }
        if (!requests_pending(context))
        {
            load_goal(context.goal_aloof, context);
        }
        return;
    }
    case 0x3EB:
        target = selected_target(context, found);
        if (!found || requests_pending(context))
        {
            return;
        }
        if (runtime_squared_target_distance(actor_of(context), target->bytes) > 6400.f) // 0x000415A0, DAT_0032B6C8
        {
            fetch_runtime_baron_cane_for(0x3EA, context); // 0x000A8750
            return;
        }
        set(strategy, field::angry, 1u);
        fetch_runtime_baron_cane_for(0x3EE, context); // 0x000A87E0
        return;
    case 0x3EE:
        target = selected_target(context, found);
        if (!found || requests_pending(context))
        {
            return;
        }
        change_state(get(strategy, field::hit_target) != 0 ? 0x40E : 0x3EA, context);
        return;
    case 0x3F4:
        put_f32(body, 0x32C, runtime_baron_tier_row(body)[1]); // 0x000A8870
        target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        if (!runtime_baron_target_holds_cane(*target, context)) // 0x000A8700
        {
            fetch_runtime_baron_cane_for(0x3EA, context); // 0x000A8750
        }
        return;
    case 0x3F8:
        if (!requests_pending(context))
        {
            fetch_runtime_baron_cane_for(0x3EA, context); // 0x000A8750
        }
        return;
    case 0x401:
        target = selected_target(context, found);
        if (found)
        {
            if (!runtime_baron_target_holds_cane(*target, context)) // 0x000A8700
            {
                goals.room = nullptr; // Actor +0xCA0.
                fetch_runtime_baron_cane_for(0x3EE, context); // 0x000A87E0
                return;
            }
            const Vec3 position = actor_world_position(actor_of(context)); // 0x00041440's 0x000412A0
            const float dx = get(target->bytes, mind_target_field::shape_position).x - position.x;
            const float dz = get(target->bytes, mind_target_field::shape_position).z - position.z;
            if (dz * dz + dx * dx <= 6400.f) // DAT_0032B6C8.
            {
                change_state(0x3F4, context);
                return;
            }
        }
        if (!requests_pending(context))
        {
            load_goal(context.goal_far, context);
        }
        return;
    case 0x402:
        if (!requests_pending(context))
        {
            change_state(get(strategy, field::after_fetch), context);
        }
        return;
    case 0x405:
        put_f32(body, 0x32C, runtime_baron_tier_row(body)[2]); // 0x000A8870
        if (!weapon_free(context)) // 0x000BEB80
        {
            change_state(0x401, context);
            return;
        }
        if (requests_pending(context))
        {
            return;
        }
        change_state(read_u32(body, 0x48) != 0 ? 0x3EA : 0x401, context);
        return;
    case 0x409:
        target = selected_target(context, found);
        if (!found || requests_pending(context) || !can_resume(context)
            || get(strategy, field::recovery) != 0.f) // DAT_0032B100.
        {
            return;
        }
        if (weapon_free(context)) // 0x000BEB80
        {
            change_state(0x405, context);
            return;
        }
        if (runtime_baron_target_holds_cane(*target, context)) // 0x000A8700
        {
            change_state(0x3F4, context);
            return;
        }
        fetch_runtime_baron_cane_for(0x3EA, context); // 0x000A8750
        return;
    case 0x40E:
        target = selected_target(context, found);
        if (!found || requests_pending(context))
        {
            return;
        }
        fetch_runtime_baron_cane_for(0x3EA, context); // 0x000A8750
        return;
    case 0x411:
        if (requests_pending(context) || !can_resume(context) || get(strategy, field::recovery) != 0.f)
        {
            return;
        }
        change_state(0x401, context);
        return;
    default:
        return;
    }
}

// Xbox 0x000A7D30
void dispatch_runtime_baron_message(std::span<u32> message, RuntimeBaronStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    const u32 actor = read_u32(strategy, 8);
    const u16 state = get(strategy, strategy_field::state);
    if ((message[0] == 0x26 || message[0] == 0x27) && (state == 0x409 || state == 0x411))
    {
        return;
    }
    dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
    switch (message[0])
    {
    case 0x14:
        if (get(strategy, field::effect) == message[1])
        {
            set(strategy, field::effect, 0u);
        }
        return;
    case 0x31:
        react_runtime_strategy_watched_actor(message, context.checks, context.views); // 0x001397B0
        return;
    case 0x36:
        notice_runtime_strategy_hostile_target(8, context.checks); // 0x0013A4F0
        return;
    case 0x59:
    {
        // The drop of a weapon he owned: the tier's radii, a quarter turn back (DAT_0032B13C) and no lift.
        const std::span<const float, 5> row = runtime_baron_tier_row(body); // 0x000A8870
        message[2] = 1;
        message[3] = std::bit_cast<u32>(row[3]);
        message[4] = std::bit_cast<u32>(row[4]);
        message[5] = std::bit_cast<u32>(-1.5707964f);
        message[6] = std::bit_cast<u32>(0.f);
        return;
    }
    case 0x5E:
        set(strategy, field::cane, message[1]);
        return;
    case 0x6E:
        if (check_runtime_player_knockout(goals) != 0) // 0x0013A490, EBX strategy
        {
            return;
        }
        if (message[1] == actor && reacts(context))
        {
            if (message[4] != 0)
            {
                change_state(0x409, context);
                if (get(strategy, field::knockdown_notice) == 0)
                {
                    set(strategy, field::knockdown_notice,
                        queue_notice("chapter4_baron_firstknockdown", context)); // 0x003242AC
                    set(strategy, field::recovery, 1.f); // DAT_0032B0F8.
                }
            }
            else
            {
                const EntityBitBlock* bits = goals.body_bits; // Body +0x15C.
                if (bits == nullptr)
                {
                    throw Error("Baron strategy 0x000A7D30 reads the body +0x15C bit block of a body with none");
                }
                change_state(runtime_body_target_state_flagged(body, *bits) ? 0x3F8 : 0x3EB, context); // 0x00134620
            }
        }
        if (runtime_message_from_mind_target(message, actor, mind_of(context), goals.target_at) // 0x001408E0
            && message[4] != 0)
        {
            set(strategy, field::hit_target, 1u);
        }
        return;
    case 0x74:
        if (message[2] != get(strategy, field::cane) || message[1] == actor)
        {
            return;
        }
        if (!reacts(context) && state != 0x409)
        {
            return;
        }
        change_state(0x411, context);
        if (get(strategy, field::cane_notice) == 0
            && queue_notice("chapter4_baron_canepickedup", context) != 0) // 0x00324290
        {
            set(strategy, field::cane_notice, 1u);
            set(strategy, field::recovery, 1.f); // DAT_0032B0F8.
        }
        return;
    case 0x7C:
        // The tail jump to 0x00140A40 over strategy +0x38.
        clear_runtime_player_mind_reference(strategy, field::cane.offset, message[1]);
        return;
    case 0x7D:
        set(strategy, field::cane, read_u32(body, 0x48));
        return;
    default:
        return;
    }
}

// Xbox 0x000A80B0
void enter_runtime_baron_state(u16 state, u16 previous, RuntimeBaronStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> mind = mind_of(context);
    const u32 target_address = get(mind, mind_field::target_record);
    const std::span<u8> body = body_of(context);
    const bool holding = read_u32(body, 0x48) != 0;
    body[0x328] = 2;
    strategy[0x1A] = 0;
    switch (state)
    {
    case 0x3EA:
        if (!holding)
        {
            load_goal(context.goal_fetch_cane, context);
        }
        load_goal(context.goal_aloof_prepare, context);
        load_goal(context.goal_aloof, context);
        set(strategy, field::aloof_time, 2.f); // DAT_0032B110.
        return;
    case 0x3EB:
        angry_goal[0] = 0;
        std::snprintf(angry_goal, sizeof(angry_goal), "angry%d", runtime_baron_tier(body) + 1); // 0x001661F9
        load_goal(angry_goal, context);
        turn_effect_on(context);
        return;
    case 0x3EE:
    {
        set(strategy, field::hit_target, 0u);
        if (get(strategy, field::angry) != 0)
        {
            turn_effect_on(context);
        }
        body[0x328] = 5;
        strategy[0x1A] = 1;
        set(strategy, strategy_field::unk_18, u16(0x401));
        if (!holding)
        {
            load_goal(context.goal_fetch_cane, context);
        }
        load_goal(context.goal_attack, context);
        const EntityBitBlock* bits = goals.body_bits; // Body +0x15C.
        if (bits == nullptr)
        {
            throw Error("Baron strategy 0x000A80B0 reads the body +0x15C bit block of a body with none");
        }
        if (runtime_body_bit_set(*bits, 0x30)) // 0x0001EF10
        {
            select_runtime_body_action(body, goals.body_action_key(0x20), goals.actions); // Vtable +0x20, 0x00027650
            return;
        }
        if (runtime_body_bit_set(*bits, 0x37)) // 0x0001EF10
        {
            select_runtime_body_action(body, goals.body_action_key(0x2C), goals.actions); // Vtable +0x2C, 0x00027650
        }
        return;
    }
    case 0x3F4:
        if (get(strategy, field::effect_mode) != 2)
        {
            begin_timeline(context);
        }
        body[0x328] = 8;
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
        return;
    case 0x3F8:
        load_goal(context.goal_dizzy, context);
        return;
    case 0x401:
        if (target_address != 0 && runtime_baron_target_holds_cane(*goals.target_at(target_address), context)
            && get(strategy, field::effect_mode) != 2) // 0x000A8700, EDX the mind's target
        {
            begin_timeline(context);
        }
        body[0x328] = 8;
        load_goal(context.goal_far, context);
        return;
    case 0x402:
        turn_effect_on(context);
        load_goal(context.goal_fetch_cane, context);
        return;
    case 0x405:
        turn_effect_on(context);
        body[0x328] = 3;
        set(strategy, strategy_field::unk_18, u16(0x405));
        load_goal(context.goal_get_weapon, context);
        set(mind, mind_field::action_actor, get(strategy, field::cane));
        return;
    case 0x409:
        if (get(strategy, field::effect_mode) != 2)
        {
            begin_timeline(context);
        }
        strategy[0x1C] = u8(strategy[0x1C] + 1);
        load_goal(context.goal_knocked_down, context);
        release_runtime_player_attachment(1, 1, context.attachment); // 0x0001B810, EAX body
        return;
    case 0x40E:
        load_goal(context.goal_celebrate, context);
        return;
    case 0x411:
        load_goal(context.goal_worried, context);
        strategy[0x1C] = u8(strategy[0x1C] + 1);
        if (get(strategy, field::effect_mode) != 2)
        {
            begin_timeline(context);
        }
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        return;
    }
}

// Xbox 0x000A8590
void leave_runtime_baron_state(u16 state, RuntimeBaronStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    switch (state)
    {
    case 0x3EE:
        detach_runtime_strategy_link(context.base.link); // 0x00112A20, EAX strategy +0x20
        if (get(strategy, field::angry) != 0)
        {
            turn_effect_off(context);
            set(strategy, field::angry, 0u);
        }
        return;
    case 0x3F4:
        turn_effect_off(context);
        put_f32(body, 0x32C, 1.f); // DAT_0032B0F8.
        return;
    case 0x405:
        turn_effect_off(context);
        set(strategy, strategy_field::unk_18, u16(0x401));
        put_f32(body, 0x32C, 1.f); // DAT_0032B0F8.
        return;
    case 0x409:
        turn_effect_off(context);
        strategy[0x1C] = u8(strategy[0x1C] - 1);
        return;
    case 0x411:
        strategy[0x1C] = u8(strategy[0x1C] - 1);
        turn_effect_off(context);
        return;
    case 0x3EB:
    case 0x401:
    case 0x402:
        turn_effect_off(context);
        return;
    case 0x3F8:
        return;
    default:
        if (state == 0x3FC || state == 0x3FF || state == 0x415)
        {
            strategy[0x1C] = u8(strategy[0x1C] - 1);
        }
        return;
    }
}
}
