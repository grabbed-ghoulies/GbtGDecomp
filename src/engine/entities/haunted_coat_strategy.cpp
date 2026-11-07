#include "haunted_coat_strategy.h"
#include "mind_fields.h"
#include "strategy_fields.h"
#include "player_mind_targets.h"
#include "actor_clock.h"
#include "actor_live_attachments.h"
#include "actor_transform.h"
#include "body_message_helpers.h"
#include "player_children.h"
#include "player_messages.h"
#include "player_mind_events.h"
#include "player_mind_goals.h"
#include "player_mind_requests.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "../navigation/mind_navigation_angles.h"
#include "../navigation/navigation.h"
#include "../navigation/player_avoidance.h"
#include "../scripts/script_vm.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include "player_fields.h"
#include <array>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
HauntedCoatStrategyState& fields_of(std::span<u8> strategy)
{
    return strategy_view<HauntedCoatStrategyState>(strategy);
}

const HauntedCoatStrategyState& fields_of(std::span<const u8> strategy)
{
    return strategy_view<HauntedCoatStrategyState>(strategy);
}

namespace parameter = haunted_coat_parameter;

RuntimePlayerStrategyGoals& goals_of(RuntimeHauntedCoatStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeHauntedCoatStrategy& context)
{
    return goals_of(context).entry.strategy;
}

std::span<u8> body_of(RuntimeHauntedCoatStrategy& context)
{
    return goals_of(context).entry.body;
}

Actor& actor_of(RuntimeHauntedCoatStrategy& context)
{
    return goals_of(context).entry.actor;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeHauntedCoatStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeHauntedCoatStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* goal, RuntimeHauntedCoatStrategy& context)
{
    load_runtime_mind_goal(goal, goals_of(context).entry.goals); // 0x0002A8E0
}

RuntimeMindTarget* select_target(bool& found, RuntimeHauntedCoatStrategy& context)
{
    RuntimeMindTarget* target = nullptr;
    found = select_runtime_strategy_target(target, 0, goals_of(context)); // 0x00139AA0
    return target;
}

std::span<const u8> target_bytes(const RuntimeMindTarget* target, const char* reader)
{
    if (target == nullptr)
    {
        throw Error(std::string(reader) + " reads the haunted coat's target record through address zero");
    }
    return target->bytes;
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    if (offset + 4 > bytes.size())
    {
        throw Error("haunted coat mind request field lies outside the request");
    }
    std::memcpy(bytes.data() + offset, &value, 4);
}

// Each of the actor's live attachment records (+0x8F8) holds `step` more (0x00042090 / 0x000420B0).
void adjust_live_holds(i32 step, RuntimeHauntedCoatStrategy& context)
{
    for (auto* live = static_cast<ActorLiveAttachment*>(context.live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->hold = u8(live->hold + step);
    }
}

// value * 2 pi held within [0, 2 pi] (DAT_0032B298), as the sway and bob phases feed 0x00153320.
float phase_angle(float value)
{
    const float tau = std::bit_cast<float>(0x40C90FDBu);
    if (value > tau)
    {
        return tau;
    }
    float angle = 0.f;
    if (!(0.f > value))
    {
        angle = value;
    }
    return angle;
}

RuntimePlayerMindRequest& allocate_request(u16 kind, RuntimeHauntedCoatStrategy& context)
{
    RuntimeMindGoalContext& mind = goals_of(context).entry.goals;
    return *allocate_runtime_mind_request(mind.anchor, kind, mind.sizes); // 0x00029D90, EBX kind
}

// Xbox 0x000AEEC0, EAX target: the target's floor cell (target +0x19, its point +0x20) is missing, blocked (+7 bits 8
// or 2), or a region (+0xC low 12 bits) flagged 0x1000.
bool runtime_haunted_coat_target_cell_closed(std::span<const u8> target)
{
    const i32 floor = target[0x19];
    const Vec3 point{mind_target_record(target).shape.position.x, mind_target_record(target).shape.position.y,
        mind_target_record(target).shape.position.z};
    const i32 cell = navigation_cell_index(floor, point); // 0x00110EF0
    if (cell == -1)
    {
        return true;
    }
    const NavCell& record = g_nav_grids[floor]->cells.at(size_t(cell));
    if ((record.flags_7 & 8) != 0 || (record.flags_7 & 2) != 0)
    {
        return true;
    }
    if ((record.region & 0xFFF) == 0 || (record.region & 0x1000) == 0)
    {
        return false;
    }
    return true;
}

// Xbox 0x000AEF10, ECX actor, EAX target: a free point 32 to 35.5 out along the actor's heading, turned toward the
// target by the strategy's turn fraction (+0x4C) and swayed by sin(+0x44 * pi) * pi / 3.
bool search_runtime_haunted_coat_heading(std::span<const u8> target, Vec3& output, RuntimeHauntedCoatStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const Actor& actor = actor_of(context);
    const Vec3 origin = actor_world_position(actor); // 0x000412A0
    const float tau = std::bit_cast<float>(0x40C90FDBu);
    const float sway_angle = phase_angle(fields_of(strategy).sway * 0.5f * tau); // DAT_0032B0F4
    const float sway = float(retail_sin(sway_angle) * static_cast<long double>(std::bit_cast<float>(0x3F860A92u)));
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const Vec3 direction{mind_target_record(target).shape.position.x - position.x,
        mind_target_record(target).shape.position.y - position.y,
        mind_target_record(target).shape.position.z - position.z};
    float angle = 0.f; // Retail leaves the stack slot as it was for a zero direction.
    runtime_direction_angle(direction, angle); // 0x00105F90
    const float heading = interpolate_runtime_mind_angle(angle, actor.euler.y, fields_of(strategy).turn) // 0x00100E90
        + sway;
    RuntimeNavigationHeadingSearch search{origin,       i32(player_record(goals.entry.actor_bytes).floor),
                                          32.f,         35.5f,
                                          nullptr,      goals.entry.allowed,
                                          &output,      0,
                                          0x1C,         i16(read_u16(goals.entry.actor_bytes, 6)),
                                          goals.random, context.occupancy_initial};
    return search_runtime_navigation_headings(search, heading, std::bit_cast<float>(0x3E9C61AAu), 10); // 0x0013AA50
}

// Xbox 0x000AEBF0, ESI strategy, EDI mind, EAX target: the phases step on; past 2 s the coat bobs (0x11) and turns
// in place (0x27) when its target's cell is closed, else it moves (0x10) to a point the heading search finds.
bool sway_runtime_haunted_coat(std::span<const u8> target, RuntimeHauntedCoatStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const float step = runtime_actor_step(actor_of(context), goals.clock);
    const float turn = step + fields_of(strategy).turn;
    fields_of(strategy).turn = turn;
    if (turn > 1.f) // DAT_0032B0F8.
    {
        fields_of(strategy).turn = 1.f;
    }
    float sway = fields_of(strategy).sway + step;
    const float closing = fields_of(strategy).closing + step;
    fields_of(strategy).sway = sway;
    fields_of(strategy).closing = closing;
    if (sway > 2.f) // DAT_0032B110.
    {
        do
        {
            sway -= 2.f;
        } while (sway > 2.f);
        fields_of(strategy).sway = sway;
    }
    float bob = fields_of(strategy).bob + step;
    fields_of(strategy).bob = bob;
    const float bob_period = std::bit_cast<float>(0x400CCCCDu); // DAT_0032B5A4, 2.2.
    if (bob > bob_period)
    {
        do
        {
            bob -= bob_period;
        } while (bob > bob_period);
        fields_of(strategy).bob = bob;
    }
    const float tau = std::bit_cast<float>(0x40C90FDBu);
    const float bob_scale = std::bit_cast<float>(0x3EE8BA2Eu); // DAT_0032B5A0, 1 / 2.2.
    if (runtime_haunted_coat_target_cell_closed(target) && closing > 2.f) // 0x000AEEC0
    {
        clear_runtime_player_mind_requests(*goals.setup.mind); // 0x00029E80, EBX mind
        const float lift = float(retail_sin(phase_angle(fields_of(strategy).bob * bob_scale * tau)) * 8.0L);
        RuntimePlayerMindRequest& hover = allocate_request(0x11, context);
        put_float(hover.bytes, 0x10, 1.f);
        put_float(hover.bytes, 0x14, 1.f);
        put_float(hover.bytes, 0x1C, 1.f);
        put_float(hover.bytes, 0x24, 30.f); // DAT_0032B108.
        put_float(hover.bytes, 0x28, .25f); // DAT_0032B104.
        put_float(hover.bytes, 0x18, 10.f); // DAT_0032B10C.
        put_float(hover.bytes, 0x24, 0.f);
        put_float(hover.bytes, 0x18, lift);
        RuntimePlayerMindRequest& face = allocate_request(0x27, context);
        put_float(face.bytes, 0x10, 40.f); // DAT_0032B1B8.
        return true;
    }
    Vec3 point{};
    if (!search_runtime_haunted_coat_heading(target, point, context)) // 0x000AEF10
    {
        return false;
    }
    clear_runtime_player_mind_requests(*goals.setup.mind); // 0x00029E80, EBX mind
    const float lift = float(retail_sin(phase_angle(fields_of(strategy).bob * bob_scale * tau)) * 8.0L);
    RuntimePlayerMindRequest& move = allocate_request(0x10, context);
    set(move.bytes, vertical_point_request::speed_low, 1.f);
    set(move.bytes, vertical_point_request::speed_high, 1.f);
    set(move.bytes, vertical_point_request::height, 10.f);
    set(move.bytes, vertical_point_request::tolerance, 1.f);
    set(move.bytes, vertical_point_request::approach, 30.f);
    set(move.bytes, vertical_point_request::vertical_speed, .25f);
    set(move.bytes, vertical_point_request::point, point);
    set(move.bytes, vertical_point_request::approach, 0.f);
    set(move.bytes, vertical_point_request::height, lift);
    set(move.bytes, vertical_point_request::vertical_speed, .25f);
    return true;
}

// Xbox 0x000AF750, ECX body, EAX target: with the body settled, the target near (0x00134350) and within 60 degrees
// in front (0x00137F50), after 1 s closing in the coat queues on its target (0x0002B910); otherwise it leaves the
// queue (0x00112A20).
bool runtime_haunted_coat_attack_ready(RuntimeMindTarget* target, RuntimeHauntedCoatStrategy& context)
{
    const std::span<const u8> target_record = target_bytes(target, "0x000AF750");
    const bool near = context.point_is_near(target_record); // 0x00134350
    const std::span<const u8> body = body_of(context);
    bool settled;
    if (body_record(body).unk_6B != 0)
    {
        settled = false;
    }
    else if (body_record(body).unk_6A != 0)
    {
        settled = false;
    }
    else if (body_record(body).unk_70 > 0.f)
    {
        settled = false;
    }
    else if (body_record(body).move_holds != 0 || !(body_record(body).move_hold_time == 0.f))
    {
        settled = false;
    }
    else
    {
        const EntityBitBlock& bits = context.bits();
        if ((bits.bytes[3] & 8) != 0)
        {
            settled = false;
        }
        else if (runtime_body_bit_set(bits, 0x2C)) // 0x0001EF10
        {
            settled = false;
        }
        else
        {
            settled = !runtime_body_bit_set(bits, 0x2A); // 0x0001EF10
        }
    }
    const Vec3 point{mind_target_record(target_record).shape.position.x,
        mind_target_record(target_record).shape.position.y, mind_target_record(target_record).shape.position.z};
    const bool facing = runtime_actor_faces_point(actor_of(context), point, std::bit_cast<float>(0x3F860A92u));
    if (settled && near && facing && fields_of(strategy_of(context)).closing > 1.f) // DAT_0032B0F8.
    {
        return join_runtime_strategy_target_queue(context.base.link, target->record, &actor_of(context),
            context.queue); // 0x0002B910, EBX strategy, EDI target
    }
    detach_runtime_strategy_link(context.base.link); // 0x00112A20, EAX strategy +0x20
    return false;
}

// Xbox 0x000AF870, EAX strategy: the coat wakes once. The script hears {0xB4, the actor's tag}; a coat on the wall
// lets go of its body hold (+0x258, message 0x4A on the last), and `release` drops the live holds message 0x7D raised.
void wake_runtime_haunted_coat(u32 release, RuntimeHauntedCoatStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    if (fields_of(strategy).awake != 0)
    {
        return;
    }
    fields_of(strategy).awake = 1u;
    strategy_base(strategy).unk_18 = 0x416;
    strategy_base(strategy).after_wait_state = 0x416;
    strategy_base(strategy).unk_1B = 1;
    const u32 tag = read_u16(goals.entry.actor_bytes, 6);
    const std::array<u32, 2> event{0xB4, tag};
    script_queue_event(goals.game, {reinterpret_cast<const u8*>(event.data()), sizeof(event)}); // 0x00116810, EDI
    if (fields_of(strategy).on_wall != 0)
    {
        const u32 count = body_record(body).holds - 1;
        body_record(body).holds = count;
        if (count == 0)
        {
            const std::array<u32, 2> message{0x4A, tag}; // Word 1 is the event's stack slot.
            context.body_message(message); // Body vtable +0xC.
        }
    }
    if (release != 0 && fields_of(strategy).latched != 0)
    {
        adjust_live_holds(-1, context); // 0x000420B0, EAX actor
        fields_of(strategy).latched = 0u;
    }
    restore_runtime_player_script_overrides(context.self, context.overrides); // 0x00138030, ESI actor
}
}

// Xbox 0x000AF030
void haunted_coat_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(HauntedCoatStrategyState);
    output.parameters = parameter::kSize;
}

// Xbox 0x000AF0A0
bool initialize_runtime_haunted_coat_strategy(RuntimeHauntedCoatStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    const Vec3 position = actor_world_position(actor_of(context)); // 0x000412A0
    const i32 floor = navigation_floor_index(position, -1, 0); // 0x00110BF0
    const i32 cell = navigation_cell_index(floor, position); // 0x00110EF0, EBX point, ESI floor
    if (cell != -1 && position.y >= float(g_nav_grids[floor]->cells.at(size_t(cell)).bottom) + 3.f) // DAT_0032B260
    {
        fields_of(strategy).on_wall = 1u;
    }
    const u16 key = u16((fields_of(strategy).on_wall != 0 ? 1 : 0) + 0xF2);
   body_record(body).default_action_key = key;
    select_runtime_body_action(body, key, context.actions); // The inlined 0x00027650 over body +0x73C.
    if (fields_of(strategy).on_wall != 0)
    {
        body_record(body).holds = body_record(body).holds + 1;
    }
    return true;
}

// Xbox 0x000AF180
void update_runtime_haunted_coat_strategy(RuntimeHauntedCoatStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u16 state = strategy_base(strategy).state;
    update_runtime_base_strategy(goals); // 0x0002C130
    bool found = false;
    switch (state)
    {
    case 0x3ED:
        select_target(found, context);
        if (found && !requests_pending(context))
        {
            change_state(0x40C, context);
        }
        return;
    case 0x40C:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (!found)
        {
            return;
        }
        sway_runtime_haunted_coat(target_bytes(target, "0x000AEBF0"), context); // 0x000AEBF0
        if (runtime_haunted_coat_attack_ready(target, context)) // 0x000AF750
        {
            change_state(0x3ED, context); // 0x000AF830, the inlined 0x0002B8D0.
        }
        return;
    }
    case 0x413:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (!found)
        {
            return;
        }
        const float distance = runtime_squared_target_distance(actor_of(context),
            target_bytes(target, "0x000AF263")); // 0x000415A0
        if (distance <= 3600.f) // DAT_0032B410.
        {
            goals.room = nullptr; // Actor +0xCA0.
            change_state(0x417, context);
        }
        return;
    }
    case 0x416:
        select_target(found, context);
        if (found)
        {
            change_state(0x40C, context);
        }
        return;
    case 0x417:
        select_target(found, context);
        if (requests_pending(context))
        {
            return;
        }
        change_state(runtime_player_strategy_decay_enabled(goals.entry.actor_bytes, goals.resume) != 0 ? 0x40C
                                                                                                          : 0x3E8,
            context); // 0x0002B7F0
        return;
    default:
        return;
    }
}

// Xbox 0x000AF340
void dispatch_runtime_haunted_coat_message(std::span<const u32> message, RuntimeHauntedCoatStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u32 actor = strategy_base(strategy).actor;
    // LAB 0x000AF392: a hostile target (0x00139C70, EDX 0) wakes it.
    const auto wake_on_hostile = [&]()
    {
        RuntimeMindTarget* target = nullptr;
        if (select_runtime_strategy_hostile_target(target, 0, goals)) // 0x00139C70
        {
            goals.room = nullptr; // Actor +0xCA0.
            change_state(0x417, context);
        }
    };
    switch (message[0])
    {
    case 1:
        if (strategy_base(strategy).state == 0x3E9)
        {
            if (fields_of(strategy).awake != 0)
            {
                return;
            }
            wake_on_hostile();
            return;
        }
        if (strategy_base(strategy).state == 0x413)
        {
            wake_on_hostile();
            return;
        }
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        return;
    case 0x27:
        if (strategy_base(strategy).state == 0x417)
        {
            return;
        }
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        return;
    case 0x30:
        if (fields_of(strategy).awake == 0)
        {
            wake_runtime_haunted_coat(1, context); // 0x000AF870
        }
        return;
    case 0x31:
        if (fields_of(strategy).awake != 0)
        {
            react_runtime_strategy_watched_actor(message, context.checks, context.views); // 0x001397B0
        }
        return;
    case 0x36:
        notice_runtime_strategy_hostile_target(0xC, context.checks); // 0x0013A4F0, ESI strategy
        return;
    case 0x6E:
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        if (check_runtime_player_knockout(goals) != 0 || message[1] != actor) // 0x0013A490
        {
            return;
        }
        if (strategy_base(strategy).state == 0x413)
        {
            change_state(0x416, context);
        }
        if (fields_of(strategy).awake != 0
            && retaliate_runtime_strategy_contact(message, -1, context.checks, context.views) == 0) // 0x00139390
        {
            react_runtime_strategy_contact(message, -1.f, context.checks); // 0x001395C0, DAT_0032B120
        }
        return;
    case 0x70:
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        if (strategy_base(strategy).state == 0x413
            && adopt_runtime_strategy_message_attacker(message, 1, context.checks, context.views) != 0) // 0x00139550
        {
            goals.room = nullptr; // Actor +0xCA0.
            change_state(0x417, context);
        }
        return;
    case 0x7D:
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        if (fields_of(strategy).awake == 0)
        {
            fields_of(strategy).latched = 1u;
            adjust_live_holds(1, context); // The tail jump to 0x00042090, EAX actor.
        }
        return;
    default:
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        return;
    }
}

// Xbox 0x000AF5A0
void enter_runtime_haunted_coat_state(u16 state, u16 previous, RuntimeHauntedCoatStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
    body_record(body).move_class = 2;
    strategy_base(strategy).attacking = 0;
    strategy_base(strategy).unk_1B = 0;
    switch (state)
    {
    case 0x3ED:
        body_record(body).move_class = 4;
        strategy_base(strategy).attacking = 1;
        load_goal(context.goal_attack, context);
        fields_of(strategy).turn = 0.f;
        fields_of(strategy).sway = 0.f;
        fields_of(strategy).closing = 0.f;
        return;
    case 0x40C:
        strategy_base(strategy).attacking = 1;
        body_record(body).move_class = 3;
        return;
    case 0x413:
        load_goal(fields_of(strategy).on_wall != 0 ? context.goal_sleep_wall : context.goal_sleep, context);
        return;
    case 0x416:
        wake_runtime_haunted_coat(0, context); // 0x000AF870
        return;
    case 0x417:
        load_goal(fields_of(strategy).on_wall != 0 ? context.goal_wakeup_wall : context.goal_wakeup, context);
        wake_runtime_haunted_coat(0, context); // 0x000AF870
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        return;
    }
}

// Xbox 0x000AF6E0
void leave_runtime_haunted_coat_state(u16 state, RuntimeHauntedCoatStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (state)
    {
    case 0x3ED:
        detach_runtime_strategy_link(context.base.link); // The inlined 0x00112A20 over strategy +0x20.
        return;
    case 0x417:
        if (fields_of(strategy).latched != 0)
        {
            fields_of(strategy).latched = 0u;
            adjust_live_holds(-1, context); // The inlined 0x000420B0.
        }
        return;
    case 0x3FC:
    case 0x3FF:
    case 0x415:
        --strategy_base(strategy).pause_count;
        return;
    default:
        return;
    }
}
}
