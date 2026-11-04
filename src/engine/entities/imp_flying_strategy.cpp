#include "imp_flying_strategy.h"
#include "strategy_fields.h"
#include "mind_fields.h"
#include "actor_clock.h"
#include "actor_pose_updates.h"
#include "actor_transform.h"
#include "body_message_helpers.h"
#include "player_children.h"
#include "player_messages.h"
#include "player_mind_goals.h"
#include "player_mind_requests.h"
#include "player_parent_lifecycle.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "strategy_target_selection.h"
#include "../navigation/navigation.h"
#include "../navigation/spawn_candidates.h"
#include "gbtg/anim.h"
#include "gbtg/entity.h"
#include "player_fields.h"
#include <array>
#include <bit>
#include <climits>
#include <cstring>
#include "body_fields.h"

namespace gbtg
{
namespace
{
namespace field = imp_flying_strategy_field;

RuntimePlayerStrategyGoals& goals_of(RuntimeImpFlyingStrategy& context)
{
    return context.shared.base.goals;
}

std::span<u8> strategy_of(RuntimeImpFlyingStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// Actor +0x958.
std::span<u8> mind_of(RuntimeImpFlyingStrategy& context)
{
    return goals_of(context).entry.events.mind;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeImpFlyingStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeImpFlyingStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* name, RuntimeImpFlyingStrategy& context)
{
    load_runtime_mind_goal(name, goals_of(context).entry.goals); // 0x0002A8E0
}

float parameter(RuntimeImpFlyingStrategy& context, size_t offset)
{
    return read_f32(context.parameters, offset);
}

float roll(RuntimeImpFlyingStrategy& context)
{
    u32& random = goals_of(context).random; // DAT_0054DD7C.
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
}

// Actor +0x964, or no bytes without the attribute resource.
std::span<const u8> attributes(RuntimeImpFlyingStrategy& context)
{
    return context.shared.checks.attributes;
}

// The mind's target record (+0x10), or null.
RuntimeMindTarget* mind_target(RuntimeImpFlyingStrategy& context)
{
    const std::span<u8> mind = mind_of(context);
    if (mind.data() == nullptr)
    {
        return nullptr;
    }
    const u32 target = get(mind, mind_field::target_record);
    return target == 0 ? nullptr : goals_of(context).target_at(target);
}

RuntimeMindTarget* select_target(bool& found, RuntimeImpFlyingStrategy& context)
{
    RuntimeMindTarget* target = nullptr;
    found = select_runtime_strategy_target(target, 0, goals_of(context)); // 0x00139AA0
    return target;
}

bool join_queue(const RuntimeMindTarget* target, RuntimeImpFlyingStrategy& context)
{
    return join_runtime_strategy_target_queue(context.shared.base.link, target == nullptr ? nullptr : target->record,
        &goals_of(context).entry.actor, context.shared.queue); // 0x0002B910, EDI the target
}

EntityBitBlock& body_bits(RuntimeImpFlyingStrategy& context, const char* caller)
{
    EntityBitBlock* bits = goals_of(context).body_bits; // Body +0x15C.
    if (bits == nullptr)
    {
        throw Error(std::string(caller) + " reads the body +0x15C bit block of a body with none");
    }
    return *bits;
}

bool attacking(RuntimeImpFlyingStrategy& context)
{
    const u16 state = strategy_base(strategy_of(context)).state;
    return state > 0x3EC && state < 0x3F0;
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_vec3(std::span<u8> bytes, size_t offset, const Vec3& value)
{
    std::memcpy(bytes.data() + offset, &value, 12);
}

// A countdown by the actor's step that stops at zero.
void count_down(float& timer, RuntimeImpFlyingStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const float step = runtime_frame_step(goals.clock);
    const float rate = runtime_actor_rate(goals.entry.actor); // Actor +0x930, +0x92C.
    if (0.f < timer)
    {
        timer = timer - rate * step;
        if (!(0.f < timer)) // COMISS; JC keeps a positive time.
        {
            timer = 0.f;
        }
    }
}

// CVTTSS2SI: out-of-range and unordered inputs give the integer indefinite value.
i32 truncate_float(float value)
{
    if (!(value >= -2147483648.f && value < 2147483648.f))
    {
        return INT_MIN;
    }
    return i32(value);
}

// 30 units (DAT_0032B108) from the point along the heading, at its height.
Vec3 point_along(const Vec3& from, float heading)
{
    const float x = float(retail_sin(heading) * 30.L + from.x); // 0x00153320
    const float z = float(retail_cos(heading) * 30.L + from.z); // 0x00153230
    return Vec3{x, from.y, z};
}

// 0x00111420, EAX 0x1C, ESI output: a clear point within 20 of the centre.
bool clear_point(i32 floor, const Vec3& origin, const Vec3& center, Vec3& output, RuntimeImpFlyingStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    return select_navigation_spawn_position(floor, &origin, center, 0.f, 20.f, goals.room, goals.entry.allowed, 0x1C,
        i16(read_u16(goals.entry.actor_bytes, 6)), output, goals.random, context.shared.checks.occupancy_initial);
}

// Xbox 0x000B6360, ESI strategy: message 5 ends the dive's effect.
void end_dive_effect(RuntimeImpFlyingStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const u32 effect = get(strategy, field::effect);
    if (effect == 0)
    {
        return;
    }
    const std::array<u32, 2> message{5, 0};
    context.object_message(effect, message); // Effect vtable +0xC.
    set(strategy, field::effect, 0u);
}

// Xbox 0x000B6950: mind request 0xF to the imp's own point, at `height`.
void hover_at(float height, RuntimeImpFlyingStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    put_vec3(mind_of(context), 0x70, actor_world_position(goals.entry.actor)); // 0x000412A0
    set(strategy_of(context), field::high, 0u);
    RuntimeMindGoalContext& mind = goals.entry.goals;
    RuntimePlayerMindRequest* move = allocate_runtime_mind_request(mind.anchor, 0xF, mind.sizes); // 0x00029D90, EBX 0xF
    put_float(move->bytes, 0x10, 0.4f); // DAT_0032B11C, after DAT_0032B0F8.
    put_float(move->bytes, 0x14, 1.f); // DAT_0032B0F8.
    put_float(move->bytes, 0x18, height); // After DAT_0032B10C.
    put_float(move->bytes, 0x1C, 1.f);
    put_float(move->bytes, 0x20, 0.f);
    put_float(move->bytes, 0x24, 0.f); // After DAT_0032B108.
    put_float(move->bytes, 0x28, 3.f); // DAT_0032B260, after DAT_0032B104.
}

// Xbox 0x000B6A30, ECX strategy, EAX low: a hover height above the floor.
float hover_height(bool low, RuntimeImpFlyingStrategy& context)
{
    if (get(strategy_of(context), field::high) != 0)
    {
        return low ? roll(context) * 5.f + 12.5f // DAT_0032B184, DAT_0032B16C.
                   : roll(context) * 2.5f + 10.f; // DAT_0032B20C, DAT_0032B10C.
    }
    return low ? roll(context) * 5.f + 7.5f // DAT_0032B184, DAT_0032B2B0.
               : roll(context) * 2.5f + 5.f; // DAT_0032B20C, DAT_0032B184.
}

// Xbox 0x000B6630: mind requests 0x25 and 0xF to one of four clear points 30 units around the imp, at a hover height
// within 5 of its own height over the floor; nothing when none is clear.
void hover_around(bool low, RuntimeImpFlyingStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> mind = mind_of(context);
    set(strategy_of(context), field::high, !(0.5f < roll(context)) ? 1u : 0u); // DAT_0032B0F4.
    const Vec3 position = actor_world_position(goals.entry.actor); // 0x000412A0
    const i32 floor = i32(get(goals.entry.actor_bytes, player_field::floor));
    struct Candidate
    {
        Vec3 point{};
        bool clear = false;
    };
    std::array<Candidate, 4> candidates{};
    i32 count = 0;
    float heading = 0.7853981852531433f; // DAT_0032B2D0, 45 degrees.
    for (Candidate& candidate : candidates)
    {
        candidate.clear = clear_point(floor, position, point_along(position, heading), candidate.point, context);
        if (candidate.clear)
        {
            ++count;
        }
        heading = heading + 1.5707963705062866f; // DAT_0032B140.
    }
    if (count == 0)
    {
        return;
    }
    const i32 pick = i32(roll(context) * float(count));
    i32 chosen = pick;
    i32 seen = 0;
    for (i32 index = 0; index < 4; ++index)
    {
        if (!candidates[size_t(index)].clear)
        {
            continue;
        }
        if (seen == pick)
        {
            chosen = index;
            break;
        }
        ++seen;
    }
    put_vec3(mind, 0x70, candidates[size_t(chosen)].point);
    RuntimeMindGoalContext& requests = goals.entry.goals;
    RuntimePlayerMindRequest* face
        = allocate_runtime_mind_request(requests.anchor, 0x25, requests.sizes); // 0x00029D90, EBX 0x25
    put_float(face->bytes, 0x10, 10.f); // DAT_0032B10C.
    RuntimePlayerMindRequest* move
        = allocate_runtime_mind_request(requests.anchor, 0xF, requests.sizes); // 0x00029D90, EBX 0xF
    put_float(move->bytes, 0x10, 0.4f); // DAT_0032B11C, after DAT_0032B0F8.
    put_float(move->bytes, 0x14, 1.f); // DAT_0032B0F8.
    put_float(move->bytes, 0x18, 10.f); // DAT_0032B10C.
    put_float(move->bytes, 0x1C, 1.f);
    put_float(move->bytes, 0x24, 0.f); // After DAT_0032B108.
    put_float(move->bytes, 0x28, 3.f); // DAT_0032B260, after DAT_0032B104.
    // The floor's height under the imp (cell +2 on its 0x00110BF0 grid). Without a cell the float is the strategy
    // pointer's bits, the stack slot of the first argument.
    float ground = std::bit_cast<float>(goals.address_of(strategy_of(context).data()));
    const i32 grid_index = entity_detail::navigation_floor_index(position, -1, 0); // 0x00110BF0
    if (grid_index < 0 || grid_index >= entity_detail::kNavSlots)
    {
        throw Error("0x000B6630 reads DAT_0050E240 outside its floors");
    }
    const entity_detail::NavGrid* grid = entity_detail::g_nav_grids[grid_index]; // DAT_0050E240.
    if (grid != nullptr)
    {
        const i32 column = (truncate_float(position.x) - entity_detail::g_nav_origin_x) / grid->cell; // DAT_0050E264
        const i32 row = (truncate_float(position.z) - entity_detail::g_nav_origin_z) / grid->cell; // DAT_0050E26C
        if (column >= 0 && column < grid->width && row >= 0 && row < grid->depth)
        {
            const i32 index = grid->width * row + column;
            if (index != -1)
            {
                ground = float(grid->cells.at(size_t(index)).bottom);
            }
        }
    }
    const float above = position.y - ground;
    const float height = hover_height(low, context); // 0x000B6A30
    if (height > above)
    {
        const float highest = above + 5.f; // DAT_0032B184.
        put_float(move->bytes, 0x18, height > highest ? highest : height);
        return;
    }
    const float lowest = above - 5.f;
    put_float(move->bytes, 0x18, lowest > height ? lowest : height);
}

// Xbox 0x000B5E20, ESI strategy, AL step: the wing whack's step and its goal.
void set_whack_step(u32 step, RuntimeImpFlyingStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (step == get(strategy, field::whack_step))
    {
        return;
    }
    set(strategy, field::whack_step, step);
    std::span<u8> body = goals_of(context).entry.body; // Actor +0x95C.
    if (step == 1)
    {
        load_goal(context.goal_wing_whack_prepare, context);
        body[0x328] = 4;
    }
    else if (step == 2)
    {
        load_goal(context.goal_evade, context);
        body[0x328] = 3;
    }
}

// Xbox 0x000B63A0, ECX strategy, AL phase: the dive's phase and what it starts.
void set_dive_phase(u32 phase, RuntimeImpFlyingStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::dive_phase) == phase)
    {
        return;
    }
    set(strategy, field::dive_phase, phase);
    std::span<u8> body = goals_of(context).entry.body; // Actor +0x95C.
    switch (phase)
    {
    case 1:
        load_goal(context.goal_arrow_dive, context);
        body[0x328] = 6;
        set(strategy, field::lunging, 0u);
        return;
    case 2:
        end_dive_effect(context); // 0x000B6360
        set(strategy, field::hold, parameter(context, 0x10));
        return;
    case 3:
    {
        load_goal(context.goal_arrow_dive_unstuck, context);
        body[0x328] = 3;
        EntityBitBlock& bits = body_bits(context, "0x000B63A0");
        bits.bytes[4] &= 0x7F;
        bits.bytes[4] &= 0xFE;
        hover_at(parameter(context, 0xC), context); // 0x000B6950
        return;
    }
    default:
        return;
    }
}

// One axis of the lunge: from `start` toward `end` as the time runs out, kept between the two.
float lunge_axis(float start, float end, float time, float total)
{
    if (!(0.f != total)) // UCOMISS; a zero time takes the end.
    {
        return end;
    }
    const float value = (time - total) / (0.f - total) * (end - start) + start;
    if (end > start)
    {
        if (value > end)
        {
            return end;
        }
        return start > value ? start : value;
    }
    if (end > value)
    {
        return end;
    }
    return value > start ? start : value;
}

// Xbox 0x000B5CF0, EAX actor, ECX strategy: the target lives, stands and queues the actor.
bool target_open_to_attack(RuntimeImpFlyingStrategy& context)
{
    RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        return false;
    }
    if (target->body.energy == 0) // Target body +0x94.
    {
        return false;
    }
    const EntityBitBlock* bits = target->body.bits; // Target body +0x15C.
    if (bits == nullptr)
    {
        throw Error("0x000B5CF0 reads the bit block of a target with no body");
    }
    if ((bits->bytes[3] & 8) != 0)
    {
        return false;
    }
    if (runtime_strategy_abandons_target(*target, context.shared.checks) != 0) // 0x00138410, EDX target
    {
        return false;
    }
    return join_queue(target, context); // 0x0002B910
}

// Xbox 0x000B5B30, EAX actor, ECX strategy: attack a queued target, evade a near one, or hover about.
void tick_idle(RuntimeImpFlyingStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    bool found = false;
    RuntimeMindTarget* target = select_target(found, context);
    if (found && runtime_strategy_abandons_target(*target, context.shared.checks) != 0 // 0x00138410, EDX target
        && retreat_runtime_strategy_from_reached_target(context.shared.checks) != 0) // 0x001383A0
    {
        return;
    }
    std::span<u8> body = goals.entry.body; // Actor +0x95C.
    if (target_open_to_attack(context)) // 0x000B5CF0
    {
        goals.room = nullptr; // Actor +0xCA0.
        change_state(roll_runtime_actor_attack(attributes(context), goals.random) ? 0x3EF : 0x3ED, context); // 0x000196F0
        return;
    }
    if (runtime_mind_target_within(parameter(context, 0), goals)) // 0x000BABF0
    {
        body[0x328] = 3;
        clear_runtime_player_mind_requests(*goals.setup.mind); // 0x00029E80, EBX mind
        load_goal(context.goal_evade, context);
        return;
    }
    if (requests_pending(context))
    {
        return;
    }
    if (runtime_mind_target_within_second(parameter(context, 4), goals)) // 0x000B5C50
    {
        body[0x328] = 3;
        hover_around(false, context); // 0x000B6630
        return;
    }
    body[0x328] = 2;
    hover_around(true, context); // 0x000B6630
}

// Xbox 0x000B5D70, EAX actor, ECX strategy: the wing whack (0x3ED), then an evade.
void tick_wing_whack(RuntimeImpFlyingStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    bool found = false;
    RuntimeMindTarget* target = select_target(found, context);
    if (!found)
    {
        return;
    }
    if (!join_queue(target, context))
    {
        change_state(0x401, context);
    }
    switch (get(strategy, field::whack_step))
    {
    case 1:
        if (!requests_pending(context))
        {
            set_whack_step(2, context); // 0x000B5E20, AL 2
        }
        return;
    case 2:
        if (!requests_pending(context))
        {
            change_state(0x401, context);
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000B6060, EAX actor, EBX strategy: the arrow dive (0x3EF) through its phases.
void tick_arrow_dive(RuntimeImpFlyingStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    Actor& actor = goals.entry.actor;
    switch (get(strategy, field::dive_phase))
    {
    case 1:
    {
        EntityBitBlock& bits = body_bits(context, "0x000B6060");
        if ((bits.bytes[5] & 2) != 0)
        {
            set_dive_phase(2, context); // 0x000B63A0, AL 2
        }
        if (get(strategy, field::effect) == 0 && (bits.bytes[6] & 0x10) != 0)
        {
            const char* name = reinterpret_cast<const char*>(strategy.data() + field::effect_name.offset);
            set(strategy, field::effect, context.create_effect(name, goals.address_of(goals.entry.actor_bytes.data())));
        }
        break;
    }
    case 2:
    {
        float hold = get(strategy, field::hold);
        count_down(hold, context);
        set(strategy, field::hold, hold);
        if (0.001f > hold) // DAT_0032B1A4.
        {
            set_dive_phase(3, context); // 0x000B63A0, AL 3
        }
        return;
    }
    case 3:
    {
        EntityBitBlock& bits = body_bits(context, "0x000B6060");
        if ((bits.bytes[0] & 0x10) == 0)
        {
            break;
        }
        if (get(strategy, field::lunging) == 0)
        {
            // 0x0003AE60, EDX 0, writes strategy +0xCC only when the model has the marker.
            Vec3 end = get(strategy, field::lunge_end);
            actor_marker_world_position(actor, 0, end);
            set(strategy, field::lunge_end, end);
            set(strategy, field::lunge_start, actor_world_position(actor)); // 0x000412A0, EDI strategy +0xD8
            set(strategy, field::lunge, parameter(context, 0x24));
            set(strategy, field::lunging, 1u);
            break;
        }
        float lunge = get(strategy, field::lunge);
        count_down_runtime_strategy_timer(lunge, goals); // 0x0002B9F0, ECX strategy +0xE4
        set(strategy, field::lunge, lunge);
        if (0.001f > lunge) // DAT_0032B1A4.
        {
            clear_runtime_body_bit(bits, 4); // 0x0001F030, EAX body, ECX 4
        }
        const Vec3 start = get(strategy, field::lunge_start);
        const Vec3 end = get(strategy, field::lunge_end);
        const float total = parameter(context, 0x24);
        const Vec3 position{lunge_axis(start.x, end.x, lunge, total), lunge_axis(start.y, end.y, lunge, total),
            lunge_axis(start.z, end.z, lunge, total)};
        set_runtime_actor_body_position(actor, position); // 0x0002B430
        break;
    }
    default:
        return;
    }
    if (!requests_pending(context))
    {
        change_state(0x401, context);
    }
}

// Xbox 0x000B5F10, ECX actor, EAX strategy: set down (0x409) until body bit 0x31, then rise to the hover height.
void tick_landing(RuntimeImpFlyingStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (get(strategy, field::landing_step))
    {
    case 1:
        if ((body_bits(context, "0x000B5F10").bytes[6] & 1) != 0)
        {
            set(strategy, field::landing_step, 2u);
            hover_at(parameter(context, 8), context); // 0x000B6950
        }
        return;
    case 2:
        if (!requests_pending(context))
        {
            change_state(0x401, context);
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000B6560, EAX actor, ESI strategy: state 0x417 ends when its time is out or its requests are done.
void tick_rise(RuntimeImpFlyingStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    float rise = get(strategy, field::rise);
    count_down(rise, context);
    set(strategy, field::rise, rise);
    if (0.001f > rise || !requests_pending(context)) // DAT_0032B1A4.
    {
        change_state(0x401, context);
    }
}

// Xbox 0x000B6480: state 0x417 holds the imp at its hover height; already there, it goes back to 0x401.
void enter_rise(RuntimeImpFlyingStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<const u8> body = goals.entry.body; // Actor +0x95C.
    // Without body +0x19E the height is the stack slot of the first argument: the strategy pointer's bits.
    float height = std::bit_cast<float>(goals.address_of(strategy_of(context).data()));
    if (get(body, body_field::has_ground) != 0)
    {
        const float floor = get(body, body_field::ground_point).y;
        height = actor_world_position(goals.entry.actor).y - floor; // 0x000412A0 on body +8
    }
    const float hover = parameter(context, 8);
    if (!(hover < height - 5.f) && height + 5.f >= hover) // DAT_0032B184.
    {
        change_state(0x401, context);
        return;
    }
    hover_at(hover, context); // 0x000B6950
    set(strategy_of(context), field::rise, 5.f); // DAT_0032B184.
}

// Xbox 0x000B5F70, ESI strategy: the dive begins.
void enter_arrow_dive(RuntimeImpFlyingStrategy& context)
{
    body_bits(context, "0x000B5F70").bytes[7] |= 8;
    set_dive_phase(1, context); // 0x000B63A0, AL 1
    strategy_base(strategy_of(context)).attacking = 1;
}

// Xbox 0x000B5FA0, ESI strategy: the dive ends and its body bits clear.
void leave_arrow_dive(RuntimeImpFlyingStrategy& context)
{
    EntityBitBlock& bits = body_bits(context, "0x000B5FA0");
    bits.bytes[7] &= 0xF7;
    set_dive_phase(0, context); // 0x000B63A0, AL 0
    detach_runtime_strategy_link(context.shared.base.link); // 0x00112A20, EAX strategy +0x20
    end_dive_effect(context); // The same release as 0x000B6360.
    bits.bytes[4] &= 0x7F;
    bits.bytes[4] &= 0xFE;
    bits.bytes[6] &= 0xEF;
    bits.bytes[0] &= 0xEF;
    strategy_base(strategy_of(context)).attacking = 0;
}
}

// Xbox 0x000B6B50
void imp_flying_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = 0x98;
}

// Xbox 0x000B6BC0
bool initialize_runtime_imp_flying_strategy(std::span<u8> strategy, std::span<const u8> parameters)
{
    set(strategy, field::parameters, 0x00303124u);
    const char* name = reinterpret_cast<const char*>(parameters.data() + 0x18);
    std::memcpy(strategy.data() + field::effect_name.offset, name, std::strlen(name) + 1);
    return true;
}

// Xbox 0x000B6BF0
void update_runtime_imp_flying_strategy(RuntimeImpFlyingStrategy& context)
{
    const u16 state = strategy_base(strategy_of(context)).state;
    update_runtime_base_strategy(goals_of(context)); // 0x0002C130
    switch (state)
    {
    case 0x3ED:
        tick_wing_whack(context); // The tail jump to 0x000B5D70.
        return;
    case 0x3EF:
        tick_arrow_dive(context); // 0x000B6060
        return;
    case 0x401:
        tick_idle(context); // The tail jump to 0x000B5B30.
        return;
    case 0x409:
        tick_landing(context); // The tail jump to 0x000B5F10.
        return;
    case 0x417:
        tick_rise(context); // 0x000B6560
        return;
    default:
        return;
    }
}

// Xbox 0x000B6CB0
void dispatch_runtime_imp_flying_message(std::span<const u32> message, RuntimeImpFlyingStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    RuntimeSpiderStrategy& shared = context.shared;
    const std::span<u8> strategy = strategy_of(context);
    const u32 actor = strategy_base(strategy).actor;
    react_runtime_strategy_message(message, shared.checks); // 0x001384F0; its result is not read.
    switch (message[0])
    {
    case 0x31:
        if (!attacking(context))
        {
            react_runtime_strategy_watched_actor(message, shared.checks, shared.views); // 0x001397B0
        }
        return;
    case 0x36:
        notice_runtime_strategy_hostile_target(0x12, shared.checks); // 0x0013A4F0
        return;
    case 0x3B:
        if (message[2] == 1)
        {
            body_bits(context, "0x000B6CB0").bytes[4] |= 0x80;
        }
        return;
    case 0x6E:
        dispatch_runtime_base_strategy_message(message, shared.base); // 0x0002BC10
        if (check_runtime_player_knockout(goals) != 0) // 0x0013A490
        {
            return;
        }
        retaliate_runtime_strategy_contact(message, -1, shared.checks, shared.views); // 0x00139390
        if (runtime_message_lands_on_actor(message, actor)) // 0x001408A0
        {
            if (runtime_strategy_reactions_enabled(goals))
            {
                change_state(0x409, context);
            }
            return;
        }
        if (message[1] == actor)
        {
            if (runtime_strategy_reactions_enabled(goals))
            {
                change_state(0x401, context);
            }
            return;
        }
        if (!attacking(context)) // 0x000B7120
        {
            react_runtime_strategy_contact(message, 0.5f, shared.checks); // The tail jump to 0x001395C0, DAT_0032B0F4.
        }
        return;
    case 0x82:
        if (!attacking(context))
        {
            react_runtime_strategy_cower(shared.checks); // The tail jump to 0x001398A0.
        }
        return;
    case 0x84:
        if (!attacking(context))
        {
            react_runtime_strategy_startle(shared.checks); // The tail jump to 0x001399A0.
        }
        return;
    default:
        dispatch_runtime_base_strategy_message(message, shared.base); // 0x0002BC10
        return;
    }
}

// Xbox 0x000B6F80
void enter_runtime_imp_flying_state(u16 state, u16 previous, RuntimeImpFlyingStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    goals.entry.body[0x328] = 2;
    switch (state)
    {
    case 0x3ED:
        // The tail jump to 0x000B5D50.
        strategy_base(strategy).attacking = 1;
        goals.entry.body[0x328] = 4;
        set_whack_step(1, context); // 0x000B5E20, AL 1
        return;
    case 0x3EF:
        enter_arrow_dive(context); // 0x000B5F70
        return;
    case 0x401:
        body_bits(context, "0x000B6F80").bytes[7] |= 8;
        return;
    case 0x409:
        // The tail jump to 0x000B5E90.
        body_bits(context, "0x000B5E90").bytes[7] |= 8;
        if (get(strategy, field::landing_step) != 1)
        {
            set(strategy, field::landing_step, 1u);
        }
        return;
    case 0x417:
        enter_rise(context); // 0x000B6480
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state); // The tail jump to 0x0002BAF0.
        (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
        return;
    }
}

// Xbox 0x000B7050
void leave_runtime_imp_flying_state(u16 state, RuntimeImpFlyingStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (state)
    {
    case 0x3ED:
        set_whack_step(0, context); // 0x000B5E20, AL 0
        detach_runtime_strategy_link(context.shared.base.link); // 0x00112A20, EAX strategy +0x20
        strategy_base(strategy).attacking = 0;
        return;
    case 0x3EF:
        leave_arrow_dive(context); // 0x000B5FA0
        return;
    case 0x401:
        body_bits(context, "0x000B7050").bytes[7] &= 0xF7;
        return;
    case 0x409:
        // The tail jump to 0x000B5ED0.
        body_bits(context, "0x000B5ED0").bytes[7] |= 8;
        if (get(strategy, field::landing_step) != 0)
        {
            set(strategy, field::landing_step, 0u);
        }
        return;
    case 0x417:
        return;
    default:
        if (state == 0x3FC || state == 0x3FF || state == 0x415)
        {
            --strategy_base(strategy).pause_count;
        }
        return;
    }
}
}
