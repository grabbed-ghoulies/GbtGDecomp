#include "haunted_door_strategy.h"
#include "strategy_fields.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "actor_live_attachments.h"
#include "actor_transform.h"
#include "player_children.h"
#include "player_messages.h"
#include "player_mind_events.h"
#include "player_mind_goals.h"
#include "player_mind_requests.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "../graphics/actor_effect_stream.h"
#include "../navigation/navigation.h"
#include "../scripts/script_vm.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
HauntedDoorStrategyState& fields_of(std::span<u8> strategy)
{
    return strategy_view<HauntedDoorStrategyState>(strategy);
}

const HauntedDoorStrategyState& fields_of(std::span<const u8> strategy)
{
    return strategy_view<HauntedDoorStrategyState>(strategy);
}

namespace parameter = haunted_door_parameter;

// The host halves of the strategy's resource words: the overlay's record (+0x4C) and the timeline (+0x5C), by the
// strategy's bytes.
struct HauntedDoorResources
{
    const AnimationRecord* overlay = nullptr;
    std::span<const u8> timeline{};
};
std::unordered_map<const u8*, HauntedDoorResources> door_resources;

RuntimePlayerStrategyGoals& goals_of(RuntimeHauntedDoorStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeHauntedDoorStrategy& context)
{
    return goals_of(context).entry.strategy;
}

std::span<u8> body_of(RuntimeHauntedDoorStrategy& context)
{
    return goals_of(context).entry.body;
}

Actor& actor_of(RuntimeHauntedDoorStrategy& context)
{
    return goals_of(context).entry.actor;
}

HauntedDoorResources& resources_of(RuntimeHauntedDoorStrategy& context)
{
    return door_resources[strategy_of(context).data()];
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeHauntedDoorStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeHauntedDoorStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* goal, RuntimeHauntedDoorStrategy& context)
{
    load_runtime_mind_goal(goal, goals_of(context).entry.goals); // 0x0002A8E0
}

RuntimeMindTarget* select_target(bool& found, RuntimeHauntedDoorStrategy& context)
{
    RuntimeMindTarget* target = nullptr;
    found = select_runtime_strategy_target(target, 0, goals_of(context)); // 0x00139AA0
    return target;
}

std::span<const u8> target_bytes(const RuntimeMindTarget* target, const char* reader)
{
    if (target == nullptr)
    {
        throw Error(std::string(reader) + " reads the haunted door's target record through address zero");
    }
    return target->bytes;
}

bool join_queue(RuntimeMindTarget* target, RuntimeHauntedDoorStrategy& context)
{
    return join_runtime_strategy_target_queue(context.base.link, target == nullptr ? nullptr : target->record,
        &actor_of(context), context.queue); // 0x0002B910
}

// Body +0x30 clear and +0x34 zero: nothing moves the body.
bool body_settled(RuntimeHauntedDoorStrategy& context)
{
    const std::span<const u8> body = body_of(context);
    return body_record(body).move_holds == 0 && body_record(body).move_hold_time == 0.f;
}

const AnimationRecord& overlay_record(RuntimeHauntedDoorStrategy& context, const char* reader)
{
    const AnimationRecord* record = resources_of(context).overlay;
    if (record == nullptr)
    {
        throw Error(std::string(reader) + " reads +0x117 of the knob attack overlay, which 0x0003AAA0 did not find");
    }
    return *record;
}

// Each of the actor's live attachment records (+0x8F8) holds `step` more (0x00042090 / 0x000420B0).
void adjust_live_holds(i32 step, RuntimeHauntedDoorStrategy& context)
{
    for (auto* live = static_cast<ActorLiveAttachment*>(context.live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->hold = u8(live->hold + step);
    }
}
}

// Xbox 0x000AFD10
void haunted_door_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(HauntedDoorStrategyState);
    output.parameters = parameter::kSize;
}

// Xbox 0x000AFD80
bool initialize_runtime_haunted_door_strategy(std::span<const u8> parameters, RuntimeHauntedDoorStrategy& context)
{
    // 0x000AF920: the overlay's record (the inlined 0x0003AAA0 pair) and the timeline.
    const std::span<u8> strategy = strategy_of(context);
    HauntedDoorResources& resources = resources_of(context);
    resources = HauntedDoorResources{};
    resources.overlay = find_runtime_body_animation(context.overlay_name, context.actions);
    fields_of(strategy).overlay = resources.overlay != nullptr ? 1u : 0u;
    const char* name = field_data(parameters, parameter::timeline);
    u32 prefix;
    std::memcpy(&prefix, name, 4);
    if (prefix == 0x5F646961) // "aid_"
    {
        resources.timeline = acquire_animation_resource(context.registry, name); // 0x00035E90
        if (resources.timeline.data() == nullptr)
        {
            resource_error(context.error, 2); // 0x00032700
        }
    }
    const u32 timeline = resources.timeline.data() == nullptr ? 0u : context.address_of(resources.timeline.data());
    fields_of(strategy).timeline = timeline;
    if (timeline != 0)
    {
        const u32 size = runtime_body_resource_size(resources.timeline.data(), context.registry.records,
            context.loaded_sizes); // 0x00035D90
        fields_of(strategy).timeline_end = timeline + (size >> 2) * 4;
        fields_of(strategy).timeline_row = timeline;
    }
    return true;
}

// Xbox 0x000AFDA0
void destroy_runtime_haunted_door_strategy(RuntimeHauntedDoorStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    end_runtime_haunted_door_attack(context); // 0x000AFB90, EBX strategy
    fields_of(strategy).overlay_on = 0u;
    fields_of(strategy).overlay_wanted = 0u;
    if (fields_of(strategy).timeline != 0)
    {
        HauntedDoorResources& resources = resources_of(context);
        release_animation_resource(context.registry, resources.timeline); // 0x00035C00
        resources.timeline = {};
    }
    fields_of(strategy).timeline = 0u;
    fields_of(strategy).timeline_end = 0u;
    fields_of(strategy).timeline_row = 0u;
}

// Xbox 0x000AF9D0, EAX strategy
void drive_runtime_haunted_door_overlay(RuntimeHauntedDoorStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).overlay_on != 0)
    {
        if (!body_settled(context))
        {
            end_runtime_haunted_door_attack(context); // 0x000AFB90, EBX strategy
            fields_of(strategy).overlay_on = 0u;
            fields_of(strategy).overlay_wanted = 1u;
            return;
        }
        const AnimationRecord& record = overlay_record(context, "0x000AF9D0");
        if (!runtime_actor_animation_slot_complete(context.actions.animation, u32(i32(record.slot)))) // 0x0003B290
        {
            return;
        }
        if (!step_runtime_haunted_door_timeline(context) || !runtime_haunted_door_sees_target(context))
        {
            return;
        }
        start_runtime_body_animation_record(record, context.actions); // 0x0001AFA0, ESI body
        return;
    }
    if (fields_of(strategy).overlay_wanted == 0 || !body_settled(context))
    {
        return;
    }
    start_runtime_haunted_door_overlay(context); // The tail jump to 0x000AFA80.
}

// Xbox 0x000AFA80, EAX strategy
void start_runtime_haunted_door_overlay(RuntimeHauntedDoorStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).overlay_on != 0)
    {
        return;
    }
    fields_of(strategy).overlay_wanted = 1u;
    if (!body_settled(context))
    {
        return;
    }
    fields_of(strategy).overlay_on = 1u;
    begin_runtime_haunted_door_attack(context); // 0x000AFB20, EDI strategy
}

// Xbox 0x000AFAC0, EAX message
void call_out_runtime_haunted_door_hit(std::span<const u32> message, RuntimeHauntedDoorStrategy& context)
{
    RuntimeActorCreationOptions& options = g_actor_creation_options; // DAT_004E1C5C.
    options.flags = 0;
    options.position = Vec3{std::bit_cast<float>(message[7]), std::bit_cast<float>(message[8]),
        std::bit_cast<float>(message[9])};
    options.flags = 1;
    const u32 actor = message[5];
    if (actor + 0x8Cu == 0)
    {
        return;
    }
    context.actor_callout(actor, 0x34, &options); // The inlined 0x0003C040.
}

// Xbox 0x000AFB20, EDI strategy
void begin_runtime_haunted_door_attack(RuntimeHauntedDoorStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).attacking != 0)
    {
        return;
    }
    const std::span<u8> body = body_of(context);
    const u8* row = nullptr;
    if (context.attack_resource.data() != nullptr) // Body +0x5C.
    {
        const RuntimeAttachmentRequestTables& tables = *context.attack_tables.at(context.attack_resource.data());
        if (tables.second.rows != nullptr) // +8.
        {
            row = find_runtime_attachment_second_request(tables, 0xCF); // 0x0001C5A0, EBX 0xCF
        }
    }
    body_record(body).hit_record = row == nullptr ? 0u : context.address_of(row);
   body_record(body).unk_88 = 0;
    ++body_record(body).unk_6A;
    start_runtime_body_animation_record(overlay_record(context, "0x000AFB20"), context.actions); // 0x0001AFA0
   body_record(body).unk_74C = 1;
    fields_of(strategy).timeline_row = fields_of(strategy).timeline;
    fields_of(strategy).row_time = 0.f;
    fields_of(strategy).attacking = 1u;
}

// Xbox 0x000AFB90, EBX strategy
void end_runtime_haunted_door_attack(RuntimeHauntedDoorStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).attacking == 0)
    {
        return;
    }
    const std::span<u8> body = body_of(context);
    body_record(body).hit_record = 0;
   body_record(body).unk_88 = 0;
    --body_record(body).unk_6A;
    const i32 slot = i32(overlay_record(context, "0x000AFB90").slot);
    AnimationController* controller = context.actions.animation.controller.get(); // Actor +0x50.
    if (controller != nullptr && slot != 0)
    {
        if (controller->slots.at(size_t(slot)).player != nullptr)
        {
            clear_animation_controller_slot(*controller, u32(slot), true, context.registry); // 0x00031240
        }
        controller->dirty = true; // +0x5C.
    }
   body_record(body).unk_74C = 0;
    fields_of(strategy).attacking = 0u;
}

// Xbox 0x000AFC00, EAX strategy
bool runtime_haunted_door_sees_target(RuntimeHauntedDoorStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<const u8> mind = goals.entry.events.mind; // Actor +0x958.
    if (mind.data() == nullptr || mind_record(mind).target_record == 0)
    {
        return true;
    }
    const std::span<const u8> target = goals.target_at(mind_record(mind).target_record)->bytes;
    const Vec3 point{mind_target_record(target).shape.position.x, mind_target_record(target).shape.position.y,
        mind_target_record(target).shape.position.z};
    const Vec3 position = actor_world_position(actor_of(context)); // 0x000412A0
    i32 last_cell = 0;
    return navigation_path_clear(position, point, 0, 0, 0, 0, nullptr, &last_cell,
        i16(read_u16(goals.entry.actor_bytes, 6))); // 0x0012CC50
}

// Xbox 0x000AFC80, ESI strategy, EBX actor
bool step_runtime_haunted_door_timeline(RuntimeHauntedDoorStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const std::span<const u8> timeline = resources_of(context).timeline;
    const u32 row = fields_of(strategy).timeline_row;
    if (timeline.data() == nullptr)
    {
        throw Error("0x000AFC80 reads the haunted door's timeline through address zero");
    }
    const float wait = read_f32(timeline, row - fields_of(strategy).timeline);
    // DAT_0032B398 (0.6) raised to the hits taken (0x00167CB8), times the row's wait.
    const float limit = float(std::pow(static_cast<long double>(0.6000000238418579),
        static_cast<long double>(fields_of(strategy).hits)) * wait);
    const float time = runtime_actor_rate(actor_of(context)) * runtime_frame_step(goals_of(context).clock)
        + fields_of(strategy).row_time;
    fields_of(strategy).row_time = time;
    if (time < limit)
    {
        return false;
    }
    fields_of(strategy).row_time = 0.f;
    fields_of(strategy).timeline_row = row + 4;
    if (!(row + 4 < fields_of(strategy).timeline_end))
    {
        fields_of(strategy).timeline_row = fields_of(strategy).timeline;
    }
    return true;
}

// Xbox 0x000B05B0, ESI strategy
void choose_runtime_haunted_door_attack(RuntimeHauntedDoorStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const float aggression = context.attributes.data() != nullptr ? read_f32(context.attributes, 8) : 0.f;
    goals.random = goals.random * 0x19660Du + 0x3C6EF35Fu;
    const float roll = std::bit_cast<float>((goals.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    // The inlined 0x0002B8D0.
    change_state(aggression >= roll ? 0x3EF : 0x3EE, context);
}

// Xbox 0x000AFDD0
void update_runtime_haunted_door_strategy(RuntimeHauntedDoorStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u16 state = strategy_base(strategy).state;
    update_runtime_base_strategy(goals); // 0x0002C130
    drive_runtime_haunted_door_overlay(context); // 0x000AF9D0, EAX strategy
    bool found = false;
    // LAB 0x000AFEBA: with no mind requests left, the door waits on its target (0x40D).
    const auto wait_on_target = [&context]()
    {
        if (!requests_pending(context))
        {
            change_state(0x40D, context);
        }
    };
    switch (state)
    {
    case 0x3EC:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (!found)
        {
            return;
        }
        if (join_queue(target, context))
        {
            choose_runtime_haunted_door_attack(context); // 0x000B05B0
        }
        wait_on_target();
        return;
    }
    case 0x3EE:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (!found)
        {
            return;
        }
        if (!requests_pending(context))
        {
            load_goal(context.goal_knockdown, context);
        }
        if (!join_queue(target, context))
        {
            change_state(0x40D, context);
        }
        return;
    }
    case 0x3EF:
        select_target(found, context);
        if (!found)
        {
            return;
        }
        wait_on_target();
        return;
    case 0x409:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (requests_pending(context))
        {
            return;
        }
        if (target == nullptr)
        {
            change_state(0x416, context);
            return;
        }
        if (!(40.f < runtime_target_distance(actor_of(context), target->bytes))) // 0x00041560, DAT_0032B1B8
        {
            change_state(0x3EF, context);
            return;
        }
        if (actor_event_chance(goals.random, 0.5f)) // 0x00019200, DAT_0032B0F4
        {
            change_state(0x40A, context);
            return;
        }
        change_state(actor_event_chance(goals.random, 0.5f) ? 0x3EE : 0x3EF, context); // 0x00019200
        return;
    }
    case 0x40A:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (fields_of(strategy).close_in != 0
            && !(40.f < context.surface_distance(target_bytes(target, "0x000AFF98")))) // 0x0013A5B0
        {
            change_state(0x3EF, context);
            return;
        }
        wait_on_target();
        return;
    }
    case 0x40D:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (!found)
        {
            return;
        }
        if (context.surface_distance(target_bytes(target, "0x000AFFDF")) > 80.f) // 0x0013A5B0, DAT_0032B2F0
        {
            change_state(0x3EC, context);
        }
        if (join_queue(target, context))
        {
            choose_runtime_haunted_door_attack(context); // 0x000B05B0
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
        if (!(60.f < context.surface_distance(target_bytes(target, "0x000B0036")))) // 0x0013A5B0, DAT_0032B28C
        {
            goals.room = nullptr; // Actor +0xCA0.
            change_state(0x417, context);
        }
        return;
    }
    case 0x416:
    {
        RuntimeMindTarget* target = select_target(found, context);
        float wait = fields_of(strategy).wait;
        count_down_runtime_strategy_timer(wait, goals); // 0x0002B9F0, ECX +0x44
        fields_of(strategy).wait = wait;
        if (target != nullptr && fields_of(strategy).wait == 0.f)
        {
            choose_runtime_haunted_door_attack(context); // 0x000B05B0
        }
        return;
    }
    case 0x417:
        select_target(found, context);
        if (requests_pending(context))
        {
            return;
        }
        change_state(runtime_player_strategy_decay_enabled(goals.entry.actor_bytes, goals.resume) != 0 ? 0x416
                                                                                                          : 0x3E8,
            context); // 0x0002B7F0
        return;
    default:
        return;
    }
}

// Xbox 0x000B0140
void dispatch_runtime_haunted_door_message(std::span<const u32> message, RuntimeHauntedDoorStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u32 actor = strategy_base(strategy).actor;
    switch (message[0])
    {
    case 0x26:
    case 0x27:
        if (strategy_base(strategy).state == 0x413 || strategy_base(strategy).state == 0x417)
        {
            return;
        }
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        return;
    case 0x31:
        if (fields_of(strategy).awake != 0)
        {
            react_runtime_strategy_watched_actor(message, context.checks, context.views); // 0x001397B0
        }
        return;
    case 0x41:
        call_out_runtime_haunted_door_hit(message, context); // The tail jump to 0x000AFAC0.
        return;
    case 0x6E:
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        if (check_runtime_player_knockout(goals) != 0 || fields_of(strategy).awake == 0) // 0x0013A490
        {
            return;
        }
        if (message[1] == actor)
        {
            if (retaliate_runtime_strategy_contact(message, -1, context.checks, context.views) == 0) // 0x00139390
            {
                react_runtime_strategy_contact(message, -1.f, context.checks); // 0x001395C0, DAT_0032B120
            }
            if (runtime_message_lands_on_actor(message, actor)) // 0x001408A0
            {
                fields_of(strategy).hits = fields_of(strategy).hits + 1;
                change_state(0x409, context);
            }
            return;
        }
        if (runtime_message_from_mind_target(message, actor, goals.entry.events.mind, goals.target_at) // 0x001408E0
            && message[4] != 0)
        {
            change_state(0x3F2, context);
            clear_runtime_player_mind_requests(*goals.setup.mind); // 0x00029E80
            load_goal(context.goal_celebrate, context);
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
        increment_runtime_body_latches(body_of(context), actor_of(context), context.latches); // 0x0001FC40
        adjust_live_holds(1, context); // 0x00042090
        fields_of(strategy).latched = 1u;
        return;
    default:
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        return;
    }
}

// Xbox 0x000B0340
void enter_runtime_haunted_door_state(u16 state, u16 previous, RuntimeHauntedDoorStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    RuntimeMindGoalContext& mind = goals.entry.goals;
    (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
    body_record(body).move_class = 2;
    strategy_base(strategy).attacking = 0;
    strategy_base(strategy).unk_1B = 0;
    switch (state)
    {
    case 0x3EC:
        body_record(body).move_class = 3;
        append_runtime_range_request(mind.anchor, mind.sizes, 40.f); // 0x00136AC0, EAX mind
        return;
    case 0x3EE:
        body_record(body).move_class = 5;
        strategy_base(strategy).attacking = 1;
        start_runtime_haunted_door_overlay(context); // 0x000AFA80
        load_goal(context.goal_knockdown, context);
        return;
    case 0x3EF:
        body_record(body).move_class = 6;
        strategy_base(strategy).attacking = 1;
        load_goal(context.goal_attack_special, context);
        return;
    case 0x409:
        load_goal(context.goal_knocked_down, context);
        return;
    case 0x40A:
        load_goal(context.goal_knocked_down_wait, context);
        fields_of(strategy).close_in = actor_event_chance(goals.random, 0.5f) ? 1u : 0u; // 0x00019200
        return;
    case 0x40D:
        allocate_runtime_mind_request(mind.anchor, 0x17, mind.sizes); // 0x00029D90, EBX 0x17
        return;
    case 0x413:
        load_goal(context.goal_sleep, context);
        ++strategy_base(strategy).pause_count;
        return;
    case 0x416:
        fields_of(strategy).wait = 1.f; // DAT_0032B0F8.
        return;
    case 0x417:
    {
        fields_of(strategy).awake = 1u;
        strategy_base(strategy).unk_18 = 0x416;
        load_goal(context.goal_wakeup, context);
        ++strategy_base(strategy).pause_count;
        strategy_base(strategy).unk_1B = 1;
        // 0x0013A400, EAX strategy: the script hears {0xB4, the actor's tag}.
        const std::array<u32, 2> event{0xB4, read_u16(goals.entry.actor_bytes, 6)};
        script_queue_event(goals.game, {reinterpret_cast<const u8*>(event.data()), sizeof(event)}); // 0x00116810
        restore_runtime_player_script_overrides(context.self, context.overrides); // 0x00138030, ESI actor
        return;
    }
    default:
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        return;
    }
}

// Xbox 0x000B0520
void leave_runtime_haunted_door_state(u16 state, RuntimeHauntedDoorStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (state)
    {
    case 0x3EE:
        end_runtime_haunted_door_attack(context); // 0x000AFB90, EBX strategy
        fields_of(strategy).overlay_on = 0u;
        fields_of(strategy).overlay_wanted = 0u;
        detach_runtime_strategy_link(context.base.link); // The tail jump to 0x00112A20, EAX strategy +0x20.
        return;
    case 0x417:
        --strategy_base(strategy).pause_count;
        if (fields_of(strategy).latched == 0)
        {
            return;
        }
        decrement_runtime_body_latches(body_of(context), actor_of(context), context.latches); // 0x0001FD20
        adjust_live_holds(-1, context); // 0x000420B0
        fields_of(strategy).latched = 0u;
        return;
    case 0x413:
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
