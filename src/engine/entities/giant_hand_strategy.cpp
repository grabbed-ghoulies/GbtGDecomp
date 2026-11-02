#include "giant_hand_strategy.h"
#include "mind_fields.h"
#include "actor_live_attachments.h"
#include "actor_model_anchors.h"
#include "actor_render_registration.h"
#include "actor_transform.h"
#include "actor_world_lists.h"
#include "entity.h"
#include "player_children.h"
#include "player_messages.h"
#include "player_mind_goals.h"
#include "player_mind_targets.h"
#include "player_strategy_entry.h"
#include "strategy_target_selection.h"
#include "../navigation/occupancy.h"
#include "../scene/trophy_heads_control.h"
#include "../scripts/script_actor_query.h"

#include <array>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace field = giant_hand_strategy_field;
namespace
{
RuntimePlayerStrategyGoals& goals_of(RuntimeGiantHandStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeGiantHandStrategy& context)
{
    return goals_of(context).entry.strategy;
}

Actor& actor_of(RuntimeGiantHandStrategy& context)
{
    return goals_of(context).entry.actor;
}

EntityBitBlock& body_bits(RuntimeGiantHandStrategy& context)
{
    EntityBitBlock* bits = goals_of(context).body_bits; // Body +0x15C.
    if (bits == nullptr)
    {
        throw Error("giant hand strategy reads the body +0x15C bit block of a body with none");
    }
    return *bits;
}

// The mind's target record (actor +0x958 -> +0x10), or null.
const RuntimeMindTarget* mind_target(RuntimeGiantHandStrategy& context)
{
    const std::span<u8> mind = goals_of(context).entry.events.mind;
    if (mind.data() == nullptr)
    {
        return nullptr;
    }
    const u32 target = get(mind, mind_field::target_record);
    return target == 0 ? nullptr : goals_of(context).target_at(target);
}

Vec3 target_position(const RuntimeMindTarget& target)
{
    return Vec3{get(target.bytes, mind_target_field::shape_position).x, get(target.bytes, mind_target_field::shape_position).y, get(target.bytes, mind_target_field::shape_position).z};
}

// A one-word notice to every active actor (0x00122C70, EDI the message) and to the script (0x00116810).
void announce(u32 id, RuntimeGiantHandStrategy& context)
{
    std::array<i32, 0x42> message{i32(id)};
    broadcast_actor_pulse_message(message.data()); // 0x00122C70
    context.queue_event({reinterpret_cast<const u8*>(message.data()), 4}); // 0x00116810
}

RuntimeBodyStateContext body_state(RuntimeGiantHandStrategy& context)
{
    if (!context.body_state)
    {
        throw Error("giant hand strategy: 0x0001FEB0's body views are not bound");
    }
    return context.body_state();
}

// Xbox 0x000ACC40, ESI strategy
void hide(RuntimeGiantHandStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::shown) != 0)
    {
        hide_runtime_actor(context.actor_view, body_bits(context)); // 0x001364B0, EBX actor
        set(strategy, field::shown, 0u);
    }
}

// Xbox 0x000ACC60, EDI strategy
void leave_world(RuntimeGiantHandStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::solid) != 0)
    {
        Actor& actor = actor_of(context);
        set(strategy, field::model_flags, actor.model_flags); // Actor +0x1F0.
        set(strategy, field::model_mode, actor.model_mode); // Actor +0x118.
        RuntimeBodyStateContext body = body_state(context);
        remove_runtime_actor_collision(context.actor_view, body); // 0x00136540, ESI actor
        set(strategy, field::solid, 0u);
    }
}

// Xbox 0x000ACCA0, ESI strategy
void show(RuntimeGiantHandStrategy& context)
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

// Xbox 0x000AC590, EAX strategy
bool target_can_be_grabbed(RuntimeGiantHandStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::held) != 0 || get(strategy, field::other_grabbing) != 0
        || (context.runtime_flags & 0x400) != 0)
    {
        return false;
    }
    const RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        return false;
    }
    const RuntimeScriptMindQuery& body = target->body; // Target +0x10 -> +0x95C.
    if (body.energy == 0)
    {
        return false;
    }
    return !script_mind_test_bit(body, 0x24) && !script_mind_test_bit(body, 0x25)
        && !script_mind_test_bit(body, 0x1F); // 0x0001EF10, EAX body
}

// Xbox 0x000AC610, ECX actor: how far the target stands outside its room (actor +0xCA0, else the nearest record of
// mask 0x20), -1 with neither, 0 with no target.
float target_room_distance(RuntimeGiantHandStrategy& context)
{
    const RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        return 0.f;
    }
    const Vec3 point = target_position(*target);
    const OccupancyRecord* room = goals_of(context).room; // Actor +0xCA0.
    if (room == nullptr)
    {
        room = find_nearest_occupancy_record(point, 0x20); // 0x00124760
        if (room == nullptr)
        {
            return -1.f; // DAT_0032B120.
        }
    }
    return occupancy_record_distance(*room, point); // 0x00124B90, ECX record, EAX point
}

// Xbox 0x000AC690, ECX actor: the target stands in its room (actor +0xCA0), or in a record of mask 0x20.
bool target_in_room(RuntimeGiantHandStrategy& context)
{
    const RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        return false;
    }
    const Vec3 point = target_position(*target);
    const OccupancyRecord* room = goals_of(context).room; // Actor +0xCA0.
    if (room == nullptr)
    {
        // XMM0 still holds the point's z, 0x001248D0's starting distance.
        return find_occupancy_record(point, point.z, 0x20, nullptr, 0) != nullptr; // 0x001248D0, EDI point
    }
    float distance = 0.f;
    if (room->kind == 0)
    {
        return occupancy_sphere_contains(*room, point, distance); // 0x00124640
    }
    if (room->kind == 1)
    {
        return occupancy_box_contains(*room, point, distance); // 0x001244D0, EAX point, EBX distance
    }
    return false;
}

// Xbox 0x000AC4C0
void wait_for_target(RuntimeGiantHandStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    RuntimeMindTarget* selected = nullptr;
    select_runtime_strategy_target(selected, 0, goals); // 0x00139AA0, EAX strategy
    leave_world(context); // The inlined 0x000ACC60.
    hide(context); // The inlined 0x000ACC40.
    if (!target_can_be_grabbed(context)) // 0x000AC590
    {
        return;
    }
    const float distance = target_room_distance(context); // 0x000AC610
    if (!(distance >= 0.f)) // DAT_0032B100; COMISS/JC skips a NaN.
    {
        return;
    }
    std::array<u32, 3> notice{0x40, goals.address_of(goals.entry.actor_bytes.data()), std::bit_cast<u32>(distance)};
    if (!context.tell_players)
    {
        throw Error("giant hand strategy: 0x00122D10 over DAT_00511B40 is not bound");
    }
    context.tell_players(notice); // 0x00122D10, EDI the notice
    if (target_in_room(context)) // 0x000AC690
    {
        change_runtime_player_strategy_state(goals.strategy, 0x3EF, goals.setup); // 0x0002B8D0, DI 0x3EF
    }
}

// Xbox 0x000AC740, EAX strategy
void start_grab(RuntimeGiantHandStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    Actor& actor = actor_of(context);
    const RuntimeMindTarget* target = mind_target(context);
    if (get(strategy, field::solid) == 0)
    {
        RuntimeBodyStateContext body = body_state(context);
        restore_runtime_actor_collision(get(strategy, field::model_flags), get(strategy, field::model_mode),
            u8(get(strategy, field::shape_mode)), context.actor_view, body); // 0x001365C0, ESI actor
        set(strategy, field::solid, 1u);
    }
    show(context); // 0x000ACCA0
    load_runtime_mind_goal(context.goal_grab, goals.entry.goals); // 0x0002A8E0
    announce(0x6C, context);
    if ((context.runtime_flags & 0x4000000) != 0)
    {
        if (target == nullptr)
        {
            throw Error("0x000AC740 places the hand on a mind target it does not have");
        }
        actor_set_world_position(actor, target_position(*target)); // 0x00041730, ESI point, EDI actor
        const Vec3 position = actor_world_position(actor); // 0x000412A0 into body +0x18
        std::memcpy(goals.entry.body.data() + 0x18, &position, 12);
    }
}

// Xbox 0x000AC820, ESI strategy, EDX actor
void start_challenge(RuntimeGiantHandStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    std::array<u8, 0x1D8> parameters{};
    build_runtime_scene_challenge_parameters(parameters,
        reinterpret_cast<const char*>(strategy.data() + field::challenge_first.offset),
        goals.address_of(goals.entry.actor_bytes.data()), -1.f, -1.f, nullptr, nullptr, 0, nullptr, 0, 2, 0,
        reinterpret_cast<const char*>(strategy.data() + field::challenge_second.offset),
        context.runtime_flags); // 0x000873A0
    if (!context.create_object)
    {
        throw Error("giant hand strategy: 0x00107520 is not bound");
    }
    context.create_object(parameters); // 0x00107520
    set(strategy, field::challenged, 1u);
}

// Xbox 0x000AC870, ESI strategy: without a challenge, the grab's end goes straight back to waiting.
void return_to_waiting(RuntimeGiantHandStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::challenged) != 0)
    {
        return;
    }
    const u16 previous = get(strategy, strategy_field::state);
    goals.setup.leaving(goals.strategy, previous, 0x401); // Vtable +0x18.
    set(strategy, strategy_field::state, u16(0x401));
    if (goals.setup.mind == nullptr)
    {
        throw Error("0x000AC870 clears the requests of a mind it does not have");
    }
    clear_runtime_player_mind_requests(*goals.setup.mind); // 0x00029E80, EBX mind
    goals.setup.entering(goals.strategy, 0x401, previous); // Vtable +0x14.
}
}

// Xbox 0x000AC8C0
void giant_hand_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = 0x118;
}

// Xbox 0x000AC930
bool initialize_runtime_giant_hand_strategy(std::span<u8> strategy, std::span<const u8> parameters,
    RuntimeGiantHandStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    Actor& actor = actor_of(context);
    const std::span<u8> body = goals.entry.body; // Actor +0x95C.
    set(strategy, field::parameters, 0x00302B78u);
    set(strategy, field::model_flags, actor.model_flags); // Actor +0x1F0.
    set(strategy, field::model_mode, actor.model_mode); // Actor +0x118.
    set(strategy, field::shape_mode, u32(body[0x244]));
    set(strategy, field::solid, 1u);
    set(strategy, field::shown, 1u);
    set(strategy, field::held, 0);
    set(strategy, field::other_grabbing, 0u);
    set(strategy, field::latched, 0u);
    set(strategy, field::challenged, 0u);
    const char* first = reinterpret_cast<const char*>(parameters.data() + 0x18);
    std::memcpy(strategy.data() + field::challenge_first.offset, first, std::strlen(first) + 1);
    const char* second = reinterpret_cast<const char*>(parameters.data() + 0x98);
    std::memcpy(strategy.data() + field::challenge_second.offset, second, std::strlen(second) + 1);
    if (context.chapter_mode == 1)
    {
        const float chance = context.attributes.data() != nullptr ? read_f32(context.attributes, 8) : 0.f;
        u32& random = goals.random; // DAT_0054DD7C.
        random = random * 0x19660Du + 0x3C6EF35Fu;
        const float roll = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
        if (!(chance >= roll))
        {
            activate_actor_bind_tree(actor); // 0x00112CC0, ESI actor
        }
    }
    return true;
}

// Xbox 0x000ACA30
void update_runtime_giant_hand_strategy(RuntimeGiantHandStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const u16 state = get(strategy, strategy_field::state);
    EntityBitBlock& bits = body_bits(context);
    const bool bit_23 = (bits.bytes[4] & 8) != 0;
    if (bit_23 && get(strategy, field::latched) == 0)
    {
        set(strategy, field::latched, 1u);
    }
    else if (!bit_23 && get(strategy, field::latched) != 0)
    {
        activate_actor_bind_tree(actor_of(context)); // 0x00112CC0, ESI actor
        return;
    }
    update_runtime_base_strategy(goals_of(context)); // 0x0002C130
    if (state == 0x3EF)
    {
        if (get(strategy, field::challenged) == 0 && (body_bits(context).bytes[5] & 0x20) != 0)
        {
            start_challenge(context); // 0x000AC820
        }
    }
    else if (state == 0x401)
    {
        wait_for_target(context); // 0x000AC4C0
    }
    bits.bytes[7] |= 0x10;
}

// Xbox 0x000ACAF0
void dispatch_runtime_giant_hand_message(std::span<const u32> message, RuntimeGiantHandStrategy& context)
{
    dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
    const std::span<u8> strategy = strategy_of(context);
    switch (message[0])
    {
    case 0x6C:
        set(strategy, field::other_grabbing, 1u);
        return;
    case 0x6D:
        set(strategy, field::other_grabbing, 0u);
        return;
    case 0x7D:
        hide(context); // 0x000ACC40
        leave_world(context); // 0x000ACC60
        return;
    case 0x86:
        if (message[2] == 7)
        {
            set(strategy, field::held, get(strategy, field::held) + 1);
        }
        return;
    case 0x87:
        if (message[2] == 7)
        {
            set(strategy, field::held, get(strategy, field::held) - 1);
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000ACBB0
void enter_runtime_giant_hand_state(u16 state, u16 previous, RuntimeGiantHandStrategy& context)
{
    (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
    enter_runtime_base_strategy_state(goals_of(context).entry, state); // 0x0002BAF0
    if (state == 0x3EF)
    {
        start_grab(context); // The tail jump to 0x000AC740.
    }
    else if (state == 0x3F0)
    {
        return_to_waiting(context); // 0x000AC870
    }
}

// Xbox 0x000ACBF0
void leave_runtime_giant_hand_state(u16 state, RuntimeGiantHandStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (state == 0x3FC || state == 0x3FF || state == 0x415)
    {
        --strategy[0x1C];
    }
    if (state == 0x3EF)
    {
        announce(0x6D, context);
    }
}

// Xbox 0x00136540
void remove_runtime_actor_collision(NativeActorFactoryInstance& view, RuntimeBodyStateContext& body)
{
    Actor& actor = view.bat.actor;
    unregister_actor_segment_probes(view.render); // 0x0003E850, EAX actor
    actor.model_mode = 0; // Actor +0x118.
    register_actor_shape_probe(actor, view.shape); // 0x0003C770
    // The shape probe's own +0x110 / +0x114 links, unlinked again.
    auto& world = entity_detail::g_world;
    auto* sentinel = reinterpret_cast<entity_detail::ShapeProbe*>(&world.shape_prev);
    entity_detail::ShapeProbe& probe = view.shape;
    if (probe.prev != nullptr)
    {
        auto** next = probe.prev == sentinel ? &world.shape : &probe.prev->next;
        *next = probe.next;
    }
    if (probe.next != nullptr)
    {
        probe.next->prev = probe.prev;
    }
    probe.prev = nullptr;
    probe.next = nullptr;
    set_runtime_body_shape_mode(4, body); // 0x0001FEB0, BL 4, EDI body
    actor.model_flags &= 0xFFFFFFFDu; // Actor +0x1F0.
    if (body.bits == nullptr)
    {
        throw Error("0x00136540 sets a bit in a body +0x15C block the body does not have");
    }
    body.bits->bytes[7] |= 0x40;
}

// Xbox 0x001365C0
void restore_runtime_actor_collision(u32 flags, i32 mode, u8 shape_mode, NativeActorFactoryInstance& view,
    RuntimeBodyStateContext& body)
{
    Actor& actor = view.bat.actor;
    actor.model_flags = flags; // Actor +0x1F0.
    register_actor_segment_probe(actor, view.segment); // 0x0003E810, EDX actor
    actor.model_mode = mode; // Actor +0x118.
    register_actor_shape_probe(actor, view.shape); // 0x0003C770
    register_actor_shape_probe(actor, view.shape); // 0x0003C770
    set_runtime_body_shape_mode(shape_mode, body); // 0x0001FEB0, BL the stack byte
    actor.model_flags = 2;
    if (body.bits == nullptr)
    {
        throw Error("0x001365C0 clears a bit in a body +0x15C block the body does not have");
    }
    body.bits->bytes[7] &= 0xBF;
}

// Xbox 0x001364B0
void hide_runtime_actor(NativeActorFactoryInstance& view, EntityBitBlock& bits)
{
    unregister_actor_render_bins(view.render); // 0x0003CBB0, EDI actor
    if (auto* record = static_cast<ActorLiveAttachment*>(view.live_attachments.next); record != nullptr) // +0x8F8.
    {
        suspend_actor_live_attachment(*record, true); // The inlined 0x0012B1A0: {0x8D, record, 1} and 0x00112910.
    }
    auto* node = static_cast<entity_detail::ListHead*>(view.anchors.next); // Actor +0x1F8.
    while (node != nullptr)
    {
        auto* next = static_cast<entity_detail::ListHead*>(node->next);
        release_actor_model_anchor(*reinterpret_cast<ActorModelAnchor*>(node)); // 0x0011BDA0
        node = next;
    }
    bits.bytes[7] |= 0x10;
}

// Xbox 0x00124760
const OccupancyRecord* find_nearest_occupancy_record(const Vec3& point, u32 mask)
{
    if (g_occupancy_pool == nullptr)
    {
        return nullptr;
    }
    const auto& pool = *static_cast<const OccupancyPool*>(g_occupancy_pool);
    const OccupancyRecord* nearest = nullptr;
    float nearest_distance = 0.f;
    // The stack slot both shapes write; a record of another kind compares the last value it held.
    float distance = 0.f;
    for (const OccupancyRecord* record = pool.begin; record < pool.cursor; ++record)
    {
        if ((record->mask & mask) == 0)
        {
            continue;
        }
        if (record->kind == 0 || record->kind == 1)
        {
            const bool contains = record->kind == 0
                ? occupancy_sphere_contains(*record, point, distance) // 0x00124640
                : occupancy_box_contains(*record, point, distance); // 0x001244D0, EAX point, EBX distance
            if (contains)
            {
                return record;
            }
        }
        if (nearest == nullptr || nearest_distance > distance)
        {
            nearest = record;
            nearest_distance = distance;
        }
    }
    return nearest;
}

// Xbox 0x00124B90
float occupancy_record_distance(const OccupancyRecord& record, const Vec3& point)
{
    float distance = 0.f;
    if (record.kind == 0)
    {
        occupancy_sphere_contains(record, point, distance); // 0x00124640
    }
    else if (record.kind == 1)
    {
        occupancy_box_contains(record, point, distance); // 0x001244D0, EAX point, EBX distance
    }
    return distance;
}
}
