#include "player_strategy_goals.h"
#include "strategy_fields.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "actor_pose_updates.h"
#include "actor_render_registration.h"
#include "actor_transform.h"
#include "body_action_ids.h"
#include "body_message_helpers.h"
#include "body_damage_tables.h"
#include "entity_heap.h"
#include "../navigation/spawn_candidates.h"
#include "body_fields.h"
#include "player_fields.h"
#include <bit>
#include <cstring>
#include <new>

namespace gbtg
{
bool actor_tick_timer(const Actor& actor, float& timer, float freeze, float dt,
    float scene_scale); // Xbox 0x00042360

namespace
{
void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_u16(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

std::span<u8> strategy_bytes(RuntimePlayerStrategyGoals& context)
{
    return context.entry.strategy;
}

// First goal kind; a null ring reads 0 and an empty ring reads the word at address zero.
u16 current_goal_kind(const RuntimePlayerStrategyGoals& context)
{
    const RuntimePlayerGoalRing* ring = context.entry.ring;
    if (ring == nullptr)
    {
        return 0;
    }
    if (ring->tail == ring->capacity)
    {
        return read_u16(context.entry.address_zero, 0);
    }
    return read_u16(ring->rows, size_t(ring->stride) * ring->head);
}

bool mind_requests_pending(const RuntimePlayerStrategyGoals& context)
{
    return context.setup.mind->first != nullptr;
}

// Inline strategy transition (leaving, store, request clear, entering) used by several callers.
void move_strategy_state(RuntimePlayerStrategyGoals& context, u16 state)
{
    std::span<u8> strategy = strategy_bytes(context);
    context.setup.leaving(context.strategy, strategy_base(strategy).state, state);
    const u16 previous = strategy_base(strategy).state;
    strategy_base(strategy).state = state;
    clear_runtime_player_mind_requests(*context.setup.mind);
    context.setup.entering(context.strategy, state, previous);
}

u32 select_action(u16 key, RuntimePlayerStrategyGoals& context)
{
    return select_runtime_body_action(context.entry.body, key, context.actions) ? 1 : 0;
}

void refresh_pose(RuntimePlayerAttachmentActor& pose, RuntimePlayerStrategyGoals& context)
{
    update_actor_animation_pose(pose.actor, pose.animation, pose.model, pose.bone,
        context.actions.registry, pose.callbacks, pose.cache, pose.reset);
}
}

// Xbox 0x0002B540
void record_runtime_player_free_position(RuntimePlayerStrategyGoals& context)
{
    std::span<u8> actor = context.entry.actor_bytes;
    put(actor, 0xCB8, 0);
    const Vec3 position = actor_world_position(context.entry.actor);
    put_float(actor, 0xCAC, position.x);
    put_float(actor, 0xCB0, position.y);
    put_float(actor, 0xCB4, position.z);
}

// Xbox 0x000285B0
u32 tick_runtime_player_goal_delay(RuntimePlayerStrategyGoals& context)
{
    std::span<u8> actor = context.entry.actor_bytes;
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(context.entry.actor);
    const float delay = get(actor, player_field::goal_delay);
    if (delay > 0.0f)
    {
        const float remaining = delay - rate * step;
        set(actor, player_field::goal_delay, remaining);
        if (remaining <= 0.0f)
        {
            set(actor, player_field::goal_delay, 0.0f);
        }
    }
    return get(actor, player_field::goal_delay) == 0.0f ? 1 : 0;
}

// Xbox 0x00028580
void clear_runtime_player_goal_volume(RuntimePlayerStrategyGoals& context)
{
    std::span<u8> actor = context.entry.actor_bytes;
    put(actor, 0x98C, 0);
    context.goal_volume = nullptr;
    if (actor[0x98A] != 0)
    {
        decrement_runtime_body_latches(context.cleanup.body, context.cleanup.body_owner, context.cleanup.first);
        actor[0x98A] = 0;
    }
}

// Xbox 0x00028500
void set_runtime_player_goal_volume(const OccupancyRecord* volume, RuntimePlayerStrategyGoals& context)
{
    std::span<u8> actor = context.entry.actor_bytes;
    put(actor, 0x98C, volume == nullptr ? 0 : context.address_of(volume));
    context.goal_volume = volume;
    if (volume == nullptr)
    {
        if (actor[0x98A] != 0)
        {
            decrement_runtime_body_latches(context.cleanup.body, context.cleanup.body_owner, context.cleanup.first);
            actor[0x98A] = 0;
        }
        return;
    }
    if (current_goal_kind(context) == 3 && actor[0x98A] == 0)
    {
        increment_runtime_body_latches(context.cleanup.body, context.cleanup.body_owner, context.cleanup.first);
        actor[0x98A] = 1;
    }
}

// Xbox 0x00124800
const OccupancyRecord* find_runtime_scripted_occupancy(i16 value, const void* records)
{
    if (g_occupancy_pool == nullptr)
    {
        return nullptr;
    }
    const auto& pool = *static_cast<const OccupancyPool*>(g_occupancy_pool);
    const OccupancyRecord* record = pool.begin;
    if (record < pool.cursor)
    {
        do
        {
            const auto* table = static_cast<const u8*>(records);
            u32 count;
            std::memcpy(&count, table, 4);
            const u8* row = table + 8;
            u16 id;
            std::memcpy(&id, reinterpret_cast<const u8*>(record) + 0xA, 2);
            i16 found = 0;
            for (u32 index = 0; index < count; ++index)
            {
                u16 key;
                std::memcpy(&key, row + 6, 2);
                if (key == id)
                {
                    std::memcpy(&found, row + 8, 2);
                    break;
                }
                u32 size;
                std::memcpy(&size, row, 4);
                row += size;
            }
            if (found == value)
            {
                return record;
            }
            ++record;
        } while (record < pool.cursor);
    }
    return nullptr;
}

// Xbox 0x0003AC90
void release_runtime_actor_controller(ActorAnimationState& animation, bool send_end,
    AnimationResourceRegistry& registry)
{
    if (animation.controller != nullptr)
    {
        release_animation_controller(animation.controller, send_end, registry);
        animation.controller = nullptr;
    }
}

// Xbox 0x0002A080
void set_runtime_mind_alert_event(RuntimeMindEvents& events, u32 target)
{
    if (target != 0)
    {
        RuntimeMindEvent* event = allocate_runtime_mind_event(events, 3, 0.5f);
        put(event->bytes, 0x20, target);
        return;
    }
    for (LiveScript* node = events.anchor.next; node != nullptr; node = node->next)
    {
        if (u16(node->type) == 3)
        {
            script_release_event(node);
            return;
        }
    }
}

// Xbox 0x0013E6B0
void load_runtime_goal_effect(const char* name, const Vec3& position, const Vec3& euler,
    ActorEffectStreamContext& effects)
{
    g_actor_creation_options.flags = 0;
    g_actor_creation_options.position = position;
    g_actor_creation_options.flags = 1;
    g_actor_creation_options.euler = euler;
    g_actor_creation_options.flags = 5;
    if (load_actor_effect_stream(name, &g_actor_creation_options, effects) != 0)
    {
        const i32 last = g_last_actor_effect;
        g_last_actor_effect = -1;
        if (last != -1)
        {
            g_actor_effect_nodes[last]->bytes[0x18] |= 8;
        }
    }
}

// Xbox 0x00028150
void play_runtime_player_goal_effects(RuntimePlayerStrategyGoals& context)
{
    play_runtime_actor_goal_effects(context.entry.actor, context.entry.actor_bytes, context.effects);
}

// Xbox 0x00028150
void play_runtime_actor_goal_effects(Actor& actor, std::span<u8> bytes, ActorEffectStreamContext& effects)
{
    const Vec3 position = actor_world_position(actor);
    const Vec3 euler = actor_world_euler(actor);
    for (const size_t offset : {size_t(0x998), size_t(0xA18), size_t(0xA98)})
    {
        if (read_u32(bytes, offset) == 0x5F646961)
        {
            load_runtime_goal_effect(reinterpret_cast<const char*>(bytes.data() + offset), position, euler,
                effects);
        }
    }
    if (bytes[0xB18] != 0 && bytes[0xB18] != 0)
    {
        queue_actor_sound(actor.sound_slots[0], 0, &actor, 0, nullptr, nullptr, 1.f,
            reinterpret_cast<const char*>(bytes.data() + 0xB18), effects.sounds);
    }
}

// Xbox 0x0013D400
void alert_runtime_goal_listener(RuntimePlayerStrategyGoals& context)
{
    entity_detail::LatchNode* node = context.cleanup.first;
    if (node == nullptr)
    {
        return;
    }
    while (node->tag != 6)
    {
        node = node->next;
        if (node == nullptr)
        {
            return;
        }
    }
    if (node != nullptr)
    {
        std::span<const u8> body = context.cleanup.body;
        const Actor& owner = context.cleanup.body_owner;
        Message message;
        message.id = 0x63;
        message.arg_i = 0x12004; // Bytes 4, 0x20, 1 and a zero fourth byte at message +4.
        message.hit_center = actor_world_position(owner);
        message.hit_extent_y = owner.radius_y * get(body, body_field::height_scale);
        message.hit_radius = owner.radius_xz * get(body, body_field::radius_scale);
        node->handle_message(*node, message);
    }
}

// Xbox 0x00135420
u32 runtime_player_goal_ready(RuntimePlayerStrategyGoals& context)
{
    const EntityBitBlock& bits = *context.body_bits;
    switch (current_goal_kind(context))
    {
    case 0:
    case 0x13:
    case 0x14:
    case 0x24:
        return 1;
    case 1:
    case 8:
    case 0xA:
    case 0x15:
    case 0x22:
        if (context.setup.mind != nullptr && mind_requests_pending(context))
        {
            return 0;
        }
        return 1;
    case 2:
    case 3:
    case 4:
    case 0x1F:
        return (bits.bytes[6] & 1) != 0 ? 1 : 0;
    case 6:
        return ~context.runtime_flags & 1;
    case 7:
        return get(context.entry.actor_bytes, player_field::unk_984);
    case 9:
        return ~(context.runtime_flags >> 2) & 1;
    case 0x12:
        return runtime_body_bit_set(bits, 0x30) ? 1 : 0;
    case 0x1A:
        return runtime_body_bit_set(bits, 0xB) ? 0 : 1;
    case 0x1C:
    case 0x1D:
        return runtime_body_bit_set(bits, 0x30) ? 1 : 0;
    default:
        return 0;
    }
}

// Xbox 0x00139E90
bool find_runtime_goal_volume_target(const OccupancyRecord& volume, u32& output, RuntimePlayerStrategyGoals& context)
{
    RuntimeMindEvents& events = context.entry.events;
    std::span<u8> mind = events.mind;
    if (get(mind, mind_field::target_record) != 0)
    {
        output = get(mind, mind_field::target_record);
        return true;
    }
    float distance;
    RuntimeMindTarget* target = find_runtime_mind_target(context.entry.actor, context.entry.allowed, 0, 0,
        &distance, get(mind, mind_field::target_categories), (get(mind, mind_field::target_flags) & ~0x20u) | 8u, context.entry.targets);
    if (target != nullptr)
    {
        Vec3 position;
        std::memcpy(&position, target->bytes.data() + 0x20, sizeof(position));
        bool inside;
        if (volume.kind == 0)
        {
            inside = occupancy_sphere_contains(volume, position, distance);
        }
        else if (volume.kind == 1)
        {
            inside = occupancy_box_contains(volume, position, distance);
        }
        else
        {
            inside = false;
        }
        if (!inside)
        {
            target = nullptr;
        }
    }
    const u32 address = target == nullptr ? 0 : target->address;
    set_runtime_mind_target(events, address);
    set_runtime_mind_alert_event(events, address);
    if (target == nullptr)
    {
        set(mind, mind_field::target, 0);
    }
    else if (target->bytes[0x14] == 0)
    {
        set(mind, mind_field::target, get(target->bytes, mind_target_field::actor));
    }
    output = address;
    return target != nullptr;
}

// Xbox 0x0013A0F0
u32 seek_runtime_player_free_position(RuntimePlayerStrategyGoals& context)
{
    std::span<u8> actor = context.entry.actor_bytes;
    const Vec3 saved = get(actor, player_field::saved_position);
    const i16 tag = i16(read_u16(actor, 6));
    // Retail leaves this stack point unwritten when no cell is selected; the path check below
    // still reads it.
    Vec3 candidate{};
    const bool found = entity_detail::select_navigation_spawn_position(i32(get(actor, player_field::floor)), nullptr,
        saved, 0.0f, 20.0f, nullptr, context.entry.allowed, 4, tag, candidate, context.random,
        0.0f); // Flag 0x10 is clear, so the retained occupancy scratch is not read.
    if (context.current == nullptr)
    {
        const OccupancyRecord* allowed = context.entry.allowed;
        i32 last_cell; // Retail writes this over its copy of the allowed record.
        const Vec3 position = actor_world_position(context.entry.actor);
        if (entity_detail::navigation_path_clear(position, candidate, 0, 0, 1, 0, allowed, &last_cell, tag))
        {
            return 1;
        }
    }
    if (found)
    {
        clear_runtime_player_mind_requests(*context.setup.mind);
        std::span<u8> mind = context.entry.events.mind;
        set(mind, mind_field::goal_point, candidate);
        RuntimeMindGoalContext& goals = context.entry.goals;
        if (get(actor, player_field::owner_class) == 1)
        {
            RuntimePlayerMindRequest* request = allocate_runtime_mind_request(goals.anchor, 0xF, goals.sizes);
            put_float(request->bytes, 0x10, 1.0f); // DAT_0032B0F8.
            put_float(request->bytes, 0x14, 1.0f);
            put_float(request->bytes, 0x1C, 1.0f);
            put_float(request->bytes, 0x24, 30.0f); // DAT_0032B108.
            put_float(request->bytes, 0x28, 0.25f); // DAT_0032B104.
            put_float(request->bytes, 0x18, 10.0f); // DAT_0032B10C.
            put_float(request->bytes, 0x24, 0.0f);
            return 0;
        }
        RuntimePlayerMindRequest* request = allocate_runtime_mind_request(goals.anchor, 0x2D, goals.sizes);
        put_float(request->bytes, 0x10, 1.0f);
        put_float(request->bytes, 0x14, 1.0f);
        put_float(request->bytes, 0x1C, 30.0f);
        put_float(request->bytes, 0x1C, 0.0f);
    }
    return 0;
}

// Xbox 0x00135B40
u32 select_runtime_body_slot_action(u32 slot, RuntimePlayerStrategyGoals& context)
{
    const u16 key = context.body_action_key(slot);
    if (key != 0)
    {
        return select_action(key, context);
    }
    return 0;
}

// Xbox 0x00135AB0
u32 select_runtime_carry_action(RuntimePlayerStrategyGoals& context)
{
    RuntimeBodyAttachedActor* attached = context.actions.attached;
    const u8 state = attached == nullptr ? 0 : attached->bytes[0x93D];
    u16 key;
    if (state == 1)
    {
        const u16 kind = attached == nullptr ? 0
            : classify_runtime_attached_actor(attached->bytes, read_u16(context.entry.actor_bytes, 6));
        if (kind != 1 || !runtime_attached_actor_allows_range(*attached))
        {
            return 0;
        }
        key = 0xC0;
    }
    else if (state == 4)
    {
        key = 0xBF;
    }
    else
    {
        key = 0xBE;
    }
    if (context.actions.table != nullptr)
    {
        u32 parameters;
        if (find_runtime_script_table_row(context.actions.table, key, parameters) != nullptr
            && get(context.entry.body, body_field::action_key) != key)
        {
            return select_action(key, context);
        }
    }
    return 0;
}

// Xbox 0x00135790
u32 select_runtime_goal_kind_action(u16 kind, u32& output, RuntimePlayerStrategyGoals& context)
{
    output = 0;
    u16 key;
    switch (kind)
    {
    case 0:
    case 5:
    case 6:
    case 8:
    case 9:
    case 0xA:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x24:
        return 1;
    case 1:
    case 0x22:
        output = select_runtime_carry_action(context);
        return 1;
    case 2:
        key = 0x13;
        break;
    case 3:
        key = context.body_action_key(0x58);
        break;
    case 4:
        key = 0x14;
        break;
    case 7:
    {
        u32 marked = 0;
        u32 index = 0;
        const u8 bit = context.body_bits->bytes[3] & 0x40;
        do
        {
            if (bit != 0)
            {
                marked = 1;
                break;
            }
            ++index;
        } while (index < 3);
        if (context.entry.body[0x6A] != 0)
        {
            return 1;
        }
        if (marked != 0)
        {
            return marked;
        }
        const u32 result = select_runtime_body_slot_action(0x24, context);
        output = result;
        return result;
    }
    case 0xB:
        key = 0x52;
        break;
    case 0xC:
        key = 0x4A;
        break;
    case 0xD:
    case 0x1E:
        key = 0x4C;
        break;
    case 0xE:
        key = 0x3F;
        break;
    case 0xF:
        key = 0x40;
        break;
    case 0x10:
        key = 0x4D;
        break;
    case 0x11:
        key = 0x4B;
        break;
    case 0x12:
    {
        release_runtime_actor_controller(context.pose.animation, false, context.actions.registry);
        const u32 result = select_action(0x79, context);
        output = result;
        refresh_pose(context.pose, context);
        RuntimeBodyAttachedActor* attached = context.actions.attached;
        if (attached != nullptr)
        {
            refresh_pose(*context.attached_poses.at(attached), context);
        }
        return result;
    }
    case 0x16:
        key = 0x4E;
        break;
    case 0x17:
        key = 0x4F;
        break;
    case 0x18:
        key = 0x50;
        break;
    case 0x19:
        key = 0x54;
        break;
    case 0x1A:
        key = 0x3C;
        break;
    case 0x1B:
        key = 0x3D;
        break;
    case 0x1C:
        key = context.body_action_key(0x38);
        break;
    case 0x1D:
        key = context.body_action_key(0x3C);
        break;
    case 0x1F:
        key = 0x12;
        break;
    case 0x20:
        key = 0x48;
        break;
    case 0x21:
        key = 0x49;
        break;
    case 0x23:
    case 0x25:
        output = 0;
        return 1;
    default:
        return 0;
    }
    const u32 result = select_action(key, context);
    output = result;
    return result;
}

// Xbox 0x00135540
u32 select_runtime_player_goal_action(u32& output, RuntimePlayerStrategyGoals& context)
{
    static constexpr u16 keys[0x16]{0, 0x3E, 0x53, 0x52, 0x4A, 0x4C, 0, 0x3F, 0x40, 0x4D, 0x5D, 0x5E, 0x4B, 0,
        0x51, 0x4E, 0x4F, 0x50, 0x54, 0x48, 0x49, 0x55};
    const u16 mode = get(context.entry.actor_bytes, player_field::goal_latch);
    if (mode > 0x15)
    {
        output = 0;
        return 0;
    }
    u16 key;
    switch (mode)
    {
    case 0:
        key = context.body_action_key(0x50);
        break;
    case 6:
        key = context.body_action_key(0x54);
        break;
    case 0xD:
        output = 0;
        return 1;
    default:
        key = keys[mode];
        break;
    }
    const u32 result = select_action(key, context);
    output = result;
    return result;
}

// Xbox 0x001391D0
u32 enter_runtime_player_goal_action(RuntimePlayerStrategyGoals& context)
{
    u32 selected; // Retail overwrites this output with the mind pointer before using it.
    if (select_runtime_goal_kind_action(current_goal_kind(context), selected, context) != 0)
    {
        move_strategy_state(context, 0x3FB);
        if (current_goal_kind(context) == 9)
        {
            load_runtime_mind_goal(context.goal_intro, context.entry.goals);
        }
        return 1;
    }
    while (context.entry.ring != nullptr)
    {
        remove_runtime_player_goal(context.cleanup);
    }
    return 0;
}

// Xbox 0x00138F60
u32 begin_runtime_player_goal(RuntimePlayerStrategyGoals& context)
{
    std::span<u8> actor = context.entry.actor_bytes;
    const OccupancyRecord* volume;
    if (actor[0x988] == 0 && read_u16(actor, 0x1FC) != 0)
    {
        volume = find_runtime_scripted_occupancy(i16(read_u16(actor, 0x1FC)), context.script_records);
    }
    else
    {
        volume = nullptr;
    }
    actor[0x988] = 1;
    set_runtime_player_goal_volume(volume, context);
    if (volume != nullptr)
    {
        actor[0x989] = 1;
        u32 selected;
        select_runtime_player_goal_action(selected, context);
        move_strategy_state(context, 0x3FB);
        return 1;
    }
    actor[0x989] = 0;
    return enter_runtime_player_goal_action(context);
}

// Xbox 0x00139010
u32 complete_runtime_player_goal_step(RuntimePlayerStrategyGoals& context)
{
    std::span<u8> actor = context.entry.actor_bytes;
    const u16 kind = current_goal_kind(context);
    const RuntimePlayerGoalRing* ring = context.entry.ring;
    if (ring != nullptr && ring->tail != ring->capacity)
    {
        u32 count;
        if (ring->tail < ring->head)
        {
            count = (u32(ring->capacity) - ring->head) + ring->tail + 1;
        }
        else
        {
            count = (u32(ring->tail) - ring->head) + 1;
        }
        if (count == 1 && runtime_player_strategy_decay_enabled(actor, context.resume) == 0 && kind != 0x15)
        {
            put(actor, 0x984, 1);
            return 0;
        }
    }
    remove_runtime_player_goal(context.cleanup);
    if (kind == 0x15 && (context.entry.ring == nullptr || current_goal_kind(context) != 0x15))
    {
        activate_actor_bind_tree(context.entry.actor);
    }
    if (context.entry.ring != nullptr && current_goal_kind(context) != 0)
    {
        begin_runtime_player_goal(context);
        return 0;
    }
    put(actor, 0xCA0, 0);
    context.room = nullptr;
    if (runtime_player_strategy_decay_enabled(actor, context.resume) != 0)
    {
        std::span<u8> strategy = strategy_bytes(context);
        const StrategyBase& base = strategy_base(strategy);
        const u16 next = base.wait_timer > 0.0f ? u16(0x415) : base.after_wait_state;
        move_strategy_state(context, next);
        return 1;
    }
    move_strategy_state(context, 1000);
    return 1;
}

// Xbox 0x00138A30
u32 update_runtime_player_goal_state(RuntimePlayerStrategyGoals& context)
{
    std::span<u8> actor = context.entry.actor_bytes;
    if (get(actor, player_field::goal_volume) != 0)
    {
        alert_runtime_goal_listener(context);
        u32 target;
        if (find_runtime_goal_volume_target(*context.goal_volume, target, context)
            && tick_runtime_player_goal_delay(context) != 0)
        {
            const bool controlled = runtime_mind_target_is_controlled_player(*context.target_at(target));
            finish_runtime_player_goal(context.cleanup, context.body_bits, context.actions,
                context.primary_action_key, context.game, controlled ? 1 : 0);
            clear_runtime_player_goal_volume(context);
            enter_runtime_player_goal_action(context);
            play_runtime_player_goal_effects(context);
        }
        return 0;
    }
    if (runtime_player_goal_ready(context) != 0)
    {
        return complete_runtime_player_goal_step(context);
    }
    switch (current_goal_kind(context))
    {
    case 6:
        if (!mind_requests_pending(context))
        {
            load_runtime_mind_goal(context.goal_wait_for_scare, context.entry.goals);
        }
        break;
    case 0x23:
    case 0x25:
        if (!mind_requests_pending(context))
        {
            load_runtime_mind_goal(context.entry.goal_default, context.entry.goals);
        }
        break;
    default:
        break;
    }
    return 0;
}

// Xbox 0x0002C130
void update_runtime_base_strategy(RuntimePlayerStrategyGoals& context)
{
    std::span<u8> strategy = strategy_bytes(context);
    std::span<u8> actor = context.entry.actor_bytes;
    switch (strategy_base(strategy).state)
    {
    case 1000:
        reset_runtime_input_device(context.input, context.input_release);
        return;
    case 0x3E9:
    case 0x3F4:
        if (!mind_requests_pending(context))
        {
            change_runtime_player_strategy_state(context.strategy, strategy_base(strategy).unk_18, context.setup);
        }
        return;
    case 0x3F2:
    case 0x418:
        if (!mind_requests_pending(context))
        {
            change_runtime_player_strategy_state(context.strategy, strategy_base(strategy).unk_10, context.setup);
        }
        return;
    case 0x3F3:
        if (!mind_requests_pending(context))
        {
            load_runtime_mind_goal(context.entry.goal_default, context.entry.goals);
        }
        return;
    case 0x3FB:
        update_runtime_player_goal_state(context);
        return;
    case 0x3FF:
    {
        std::span<u8> mind = context.entry.events.mind;
        if (get(mind, mind_field::target_record) != 0
            && runtime_mind_target_occupancy_differs(*context.target_at(get(mind, mind_field::target_record)),
                read_u16(actor, 6), context.entry.allowed))
        {
            set(mind, mind_field::target_record, 0);
        }
        u32 reached;
        if (!mind_requests_pending(context))
        {
            reached = seek_runtime_player_free_position(context);
        }
        else
        {
            reached = runtime_player_mind_saved_path_clear(context.entry.actor, mind, context.current,
                context.entry.allowed, read_u16(actor, 6)) ? 1 : 0;
        }
        if (reached != 0)
        {
            record_runtime_player_free_position(context);
            if (runtime_player_can_resume_strategy(actor, context.resume) != 0)
            {
                change_runtime_player_strategy_state(context.strategy, strategy_base(strategy).unk_18, context.setup);
                return;
            }
            change_runtime_player_strategy_state(context.strategy, 1000, context.setup);
        }
        return;
    }
    case 0x415:
    {
        float timer = strategy_base(strategy).wait_timer;
        actor_tick_timer(context.entry.actor, timer, context.clock.freeze, context.clock.dt, context.clock.scale);
        strategy_base(strategy).wait_timer = timer;
        if (timer == 0.0f)
        {
            change_runtime_player_strategy_state(context.strategy, strategy_base(strategy).after_wait_state,
                context.setup);
        }
        return;
    }
    default:
        return;
    }
}

namespace
{
// Shared retail gate: no strategy hold (+0x1C), the decay group free and enabled, and no pending
// goal kind (a null ring passes; an empty ring reads the word at address zero).
bool strategy_goal_gate(RuntimePlayerStrategyGoals& context)
{
    std::span<const u8> actor = context.entry.actor_bytes;
    if (strategy_base(context.entry.strategy).pause_count != 0)
    {
        return false;
    }
    const size_t group = (get(actor, player_field::mode) == 2 || get(actor, player_field::case_link) != 0) ? 1 : 0;
    if (context.resume.references[group] != 0 || context.resume.decay_enabled[context.resume.decay_index] == 0)
    {
        return false;
    }
    return context.entry.ring == nullptr || current_goal_kind(context) == 0;
}

void adopt_message_target(RuntimeMindTarget& target, RuntimePlayerStrategyGoals& context)
{
    set_runtime_mind_target(context.entry.events, target.address);
    if (get(target.bytes, mind_target_field::kind) == 0)
    {
        set(context.entry.events.mind, mind_field::target, get(target.bytes, mind_target_field::actor));
    }
}
}

// Xbox 0x00028C30
u32 runtime_player_mind_target(RuntimePlayerStrategyGoals& context)
{
    if (context.setup.mind == nullptr)
    {
        return 0;
    }
    return get(context.entry.events.mind, mind_field::target_record);
}

// Xbox 0x00042070
RuntimeMindTarget* runtime_actor_active_target(const RuntimeStrategyMessageActor& actor)
{
    RuntimeMindTarget* target = actor.target;
    if (target == nullptr)
    {
        return nullptr;
    }
    return target->bytes[0x18] == 0 ? target : nullptr;
}

// Xbox 0x00028420
void append_runtime_player_goal(u16 kind, u32 value, std::span<const u32, 3> position,
    std::span<const u32, 3> second, RuntimePlayerStrategyGoals& context)
{
    RuntimePlayerGoalRing*& ring = context.entry.ring;
    if (ring == nullptr)
    {
        void* block = engine_heap_allocate(i32(sizeof(RuntimePlayerGoalRing) + 0x180));
        u8* rows = static_cast<u8*>(block) + sizeof(RuntimePlayerGoalRing);
        ring = new (block) RuntimePlayerGoalRing{0x20, 0xC, 0, 0xC, std::span<const u8>(rows, 0x180)};
        if (runtime_goal_requires_latches(kind))
        {
            increment_runtime_body_latches(context.cleanup.body, context.cleanup.body_owner, context.cleanup.first);
        }
    }
    RuntimePlayerGoalRing* current = ring;
    if (current->tail == current->capacity)
    {
        current->head = 0;
        current->tail = 0;
    }
    else
    {
        current->tail = u16(i32(current->tail + 1) % i32(current->capacity));
    }
    std::span<u8> row{const_cast<u8*>(current->rows.data()) + size_t(current->stride) * current->tail, 0x20};
    put_u16(row, 0, kind);
    put(row, 0x1C, value);
    put(row, 4, position[0]);
    put(row, 8, position[1]);
    put(row, 0xC, position[2]);
    put(row, 0x10, second[0]);
    put(row, 0x14, second[1]);
    put(row, 0x18, second[2]);
    put(context.entry.actor_bytes, 0x984, 0);
}

// Xbox 0x001387F0
u32 queue_runtime_player_goal(std::span<const u32> message, RuntimePlayerStrategyGoals& context)
{
    const u16 before = current_goal_kind(context);
    append_runtime_player_goal(u16(message[1]), message[8], message.subspan<2, 3>(), message.subspan<5, 3>(),
        context);
    if (before != 0)
    {
        return 1;
    }
    return begin_runtime_player_goal(context);
}

// Xbox 0x00138C80
void resolve_runtime_goal_alert(RuntimePlayerStrategyGoals& context)
{
    strategy_base(context.entry.strategy).wait_timer = 0.0f;
    if (context.entry.ring == nullptr || current_goal_kind(context) == 0)
    {
        return;
    }
    std::span<u8> mind = context.entry.events.mind;
    std::span<const u8> actor = context.entry.actor_bytes;
    u32 flags = get(mind, mind_field::target_flags);
    const u32 category = get(mind, mind_field::target_categories);
    const auto alerted = [&actor]()
    {
        return get(actor, player_field::countdown_56) > 0.0f || get(actor, player_field::provoked) != 0;
    };
    if (alerted())
    {
        flags = (flags & ~4u) | 0x10u;
    }
    flags = (flags & ~0x20u) | 8u;
    u16 exclude = 0;
    if (alerted())
    {
        exclude = read_u16(actor, 0x97E);
        if (exclude <= 1)
        {
            exclude = 0;
        }
    }
    u16 include = 0;
    if (alerted())
    {
        include = read_u16(actor, 0x97C);
        if (include <= 1)
        {
            include = 0;
        }
    }
    float distance;
    RuntimeMindTarget* target = find_runtime_mind_target(context.entry.actor, context.entry.allowed, include, exclude,
        &distance, category, flags, context.entry.targets);
    if (target == nullptr)
    {
        if (!alerted())
        {
            return;
        }
        target = find_runtime_mind_target(context.entry.actor, context.entry.allowed, 0, 0, &distance,
            get(mind, mind_field::target_categories), (get(mind, mind_field::target_flags) & ~0x20u) | 8u, context.entry.targets);
        if (target == nullptr)
        {
            return;
        }
    }
    adopt_message_target(*target, context);
    const bool volume = get(actor, player_field::goal_volume) != 0;
    const bool controlled = runtime_mind_target_is_controlled_player(*target);
    finish_runtime_player_goal(context.cleanup, context.body_bits, context.actions, context.primary_action_key,
        context.game, controlled ? 1 : 0);
    clear_runtime_player_goal_volume(context);
    if (volume)
    {
        enter_runtime_player_goal_action(context);
        play_runtime_player_goal_effects(context);
        return;
    }
    complete_runtime_player_goal_step(context);
}

// Xbox 0x00138B40
void deliver_runtime_goal_item(std::span<const u32> message, RuntimeBaseStrategyMessages& context)
{
    RuntimePlayerStrategyGoals& goals = context.goals;
    std::span<const u8> actor = goals.entry.actor_bytes;
    if (goals.entry.ring == nullptr || current_goal_kind(goals) == 0 || get(actor, player_field::goal_volume) == 0)
    {
        return;
    }
    const Vec3 position = actor_world_position(goals.entry.actor);
    const float dz = position.z - std::bit_cast<float>(message[4]);
    const float dy = position.y - std::bit_cast<float>(message[3]);
    const float dx = position.x - std::bit_cast<float>(message[2]);
    if (dz * dz + dy * dy + dx * dx > 900.0f) // DAT_0032B4EC.
    {
        return;
    }
    RuntimeStrategyMessageActor* holder = context.message_actor(message[1])->holder;
    const u32 success = holder == nullptr ? 0 : (runtime_actor_is_player(*holder->actor) ? 1 : 0);
    if (holder != nullptr)
    {
        RuntimeMindTarget* target = runtime_actor_active_target(*holder);
        if (target != nullptr)
        {
            adopt_message_target(*target, goals);
        }
    }
    finish_runtime_player_goal(goals.cleanup, goals.body_bits, goals.actions, goals.primary_action_key, goals.game,
        success);
    clear_runtime_player_goal_volume(goals);
    complete_runtime_player_goal_step(goals);
}

// Xbox 0x001392D0
void complete_runtime_goal_contact(u32 first, u32 second, RuntimeBaseStrategyMessages& context)
{
    RuntimePlayerStrategyGoals& goals = context.goals;
    std::span<const u8> actor = goals.entry.actor_bytes;
    if (first != goals.cleanup.actor.address || second == 0 || goals.entry.ring == nullptr
        || current_goal_kind(goals) == 0 || get(actor, player_field::goal_volume) == 0)
    {
        return;
    }
    if (context.carried != nullptr && context.carried[0x199] == 0)
    {
        return;
    }
    RuntimeStrategyMessageActor& other = *context.message_actor(second);
    if (get(actor, player_field::goal_latch) == 0xC || other.target == nullptr)
    {
        return;
    }
    const bool controlled = runtime_mind_target_is_controlled_player(*other.target);
    finish_runtime_player_goal(goals.cleanup, goals.body_bits, goals.actions, goals.primary_action_key, goals.game,
        controlled ? 1 : 0);
    RuntimeMindTarget* target = runtime_actor_active_target(other);
    if (target != nullptr)
    {
        set_runtime_mind_target(goals.entry.events, target->address);
        set(goals.entry.events.mind, mind_field::target, second);
    }
    clear_runtime_player_goal_volume(goals);
    enter_runtime_player_goal_action(goals);
}

// Xbox 0x00138E40
void complete_runtime_goal_handoff(std::span<const u32> message, RuntimeBaseStrategyMessages& context)
{
    RuntimePlayerStrategyGoals& goals = context.goals;
    std::span<const u8> actor = goals.entry.actor_bytes;
    if (goals.entry.ring == nullptr || current_goal_kind(goals) == 0)
    {
        return;
    }
    const u16 group = read_u16(actor, 0x1FC);
    if (group == 0)
    {
        return;
    }
    const RuntimeStrategyMessageActor& other = *context.message_actor(message[2]);
    if (read_u16(other.bytes, 0x1FC) == 0 || group != read_u16(other.bytes, 0x1FE))
    {
        return;
    }
    RuntimeMindTarget* target = runtime_actor_active_target(*context.message_actor(message[1]));
    if (target == nullptr)
    {
        return;
    }
    adopt_message_target(*target, goals);
    const bool controlled = runtime_mind_target_is_controlled_player(*target);
    finish_runtime_player_goal(goals.cleanup, goals.body_bits, goals.actions, goals.primary_action_key, goals.game,
        controlled ? 1 : 0);
    clear_runtime_player_goal_volume(goals);
    complete_runtime_player_goal_step(goals);
}

// Xbox 0x00138EF0
void complete_runtime_goal_script(std::span<const u32> message, RuntimeBaseStrategyMessages& context)
{
    RuntimePlayerStrategyGoals& goals = context.goals;
    std::span<const u8> actor = goals.entry.actor_bytes;
    if (goals.entry.ring == nullptr || current_goal_kind(goals) == 0)
    {
        return;
    }
    const u16 group = read_u16(actor, 0x1FC);
    if (group == 0)
    {
        return;
    }
    const RuntimeStrategyMessageActor& other = *context.message_actor(message[1]);
    if (read_u16(other.bytes, 0x1FC) == 0 || group != read_u16(other.bytes, 0x1FE))
    {
        return;
    }
    finish_runtime_player_goal(goals.cleanup, goals.body_bits, goals.actions, goals.primary_action_key, goals.game,
        1);
    clear_runtime_player_goal_volume(goals);
    complete_runtime_player_goal_step(goals);
}

// Xbox 0x0002BC10
void dispatch_runtime_base_strategy_message(std::span<const u32> message, RuntimeBaseStrategyMessages& context)
{
    RuntimePlayerStrategyGoals& goals = context.goals;
    std::span<u8> strategy = goals.entry.strategy;
    std::span<const u8> actor = goals.entry.actor_bytes;
    switch (message[0])
    {
    case 1:
        resolve_runtime_goal_alert(goals);
        return;
    case 0x24:
        if (get(actor, player_field::mode) == 1 && strategy_goal_gate(goals) &&
            get(goals.entry.events.mind, mind_field::target_record) == 0)
        {
            clear_runtime_player_mind_requests(*goals.setup.mind);
        }
        return;
    case 0x26:
        if (runtime_player_strategy_decay_enabled(actor, goals.resume) == 0)
        {
            return;
        }
        if (script_actor_current_mask(goals.entry.actor, context.query) != 0 && get(actor, player_field::unk_984) == 0)
        {
            return;
        }
        if (script_actor_current_mask(goals.entry.actor, context.query) != 0)
        {
            drain_runtime_player_goals(goals.cleanup);
            const StrategyBase& base = strategy_base(strategy);
            const u16 state = base.wait_timer > 0.0f ? u16(0x415) : base.after_wait_state;
            change_runtime_player_strategy_state(goals.strategy, state, goals.setup);
            return;
        }
        if (runtime_player_mind_target(goals) != 0 && get(actor, player_field::mode) == 1)
        {
            change_runtime_player_strategy_state(goals.strategy, 0x3E9, goals.setup);
            return;
        }
        change_runtime_player_strategy_state(goals.strategy, strategy_base(strategy).unk_18, goals.setup);
        return;
    case 0x27:
        if (script_actor_current_mask(goals.entry.actor, context.query) == 0 && strategy_base(strategy).state != 0x3F3)
        {
            change_runtime_player_strategy_state(goals.strategy, 1000, goals.setup);
        }
        return;
    case 0x32:
        complete_runtime_player_goal_step(goals);
        return;
    case 0x4B:
        change_runtime_player_strategy_state(goals.strategy, strategy_base(strategy).unk_0C, goals.setup);
        return;
    case 0x4D:
        queue_runtime_player_goal(message, goals);
        return;
    case 0x5D:
        if (strategy_goal_gate(goals) && runtime_player_can_resume_strategy(actor, goals.resume) != 0)
        {
            change_runtime_player_strategy_state(goals.strategy, 0x3FF, goals.setup);
        }
        return;
    case 0x64:
        if (runtime_player_can_resume_strategy(actor, goals.resume) != 0 && strategy_goal_gate(goals))
        {
            const StrategyBase& base = strategy_base(strategy);
            const u16 state = message[1] != 0 ? base.flagged_resume_state : base.resume_state;
            change_runtime_player_strategy_state(goals.strategy, state, goals.setup);
        }
        return;
    case 0x65:
        if (strategy_goal_gate(goals))
        {
            change_runtime_player_strategy_state(goals.strategy, u16(message[1]), goals.setup);
        }
        return;
    case 0x6E:
        if (message[4] != 0 && message[1] == goals.cleanup.actor.address)
        {
            detach_runtime_strategy_link(context.link);
        }
        [[fallthrough]];
    case 0x70:
        complete_runtime_goal_contact(message[1], message[2], context);
        return;
    case 0x72:
        deliver_runtime_goal_item(message, context);
        return;
    case 0x74:
        complete_runtime_goal_handoff(message, context);
        return;
    case 0x7D:
        if (runtime_player_strategy_decay_enabled(actor, goals.resume) == 0)
        {
            const u32 notice[1] = {0x27};
            goals.cleanup.actor.message(notice);
        }
        return;
    case 0x8F:
        complete_runtime_goal_script(message, context);
        return;
    default:
        return;
    }
}
}
