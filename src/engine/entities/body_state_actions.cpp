#include "body_state_actions.h"
#include "body_message_helpers.h"
#include "body_move_actions.h"
#include "actor_world_lists.h"
#include "body_hit_state.h"
#include "body_dizzy_state.h"
#include "body_knockdown_state.h"
#include "body_die_state.h"
#include "body_anim_state.h"
#include "body_pickup_state.h"
#include "body_struggle_state.h"
#include "body_player_death_state.h"
#include "body_grabbed_state.h"
#include "body_walk_target_state.h"
#include "body_hold_state.h"
#include "body_throw_state.h"
#include "gbtg/entity.h"
#include "body_fields.h"
#include "body_state_fields.h"
#include <bit>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace gbtg
{
namespace stand_parameter = stand_alert_parameter;
namespace walk_parameter = walk_alert_parameter;
namespace fly_parameter = fly_state_parameter;
namespace row = walk_alert_row_field;

namespace
{
std::function<RuntimeBodyStateContext(u32)> body_context_of;
std::function<RuntimeHitStateLifecycle(u32)> hit_context_of;
std::function<RuntimeDizzyStateContext&(u32)> dizzy_context_of;
std::function<RuntimeKnockdownContext&(u32)> knockdown_context_of;
std::function<RuntimeDieStateContext&(u32)> die_context_of;
std::function<RuntimePlayerDeathContext&(u32)> player_death_context_of;
std::function<RuntimeGrabbedStateContext&(u32)> grabbed_context_of;
std::function<RuntimeThrowStateContext&(u32)> throw_context_of;
std::function<RuntimeAnimStateContext&(u32)> anim_context_of;
std::function<RuntimePickupStateContext&(u32)> pickup_context_of;
// Walk-alert (and fly) speed-band records by state, the host side of the state's rows.
std::unordered_map<const u8*, std::vector<const AnimationRecord*>> walk_alert_records;
// The fly state's looping animation record (`loop_found`), by the state's bytes.
std::unordered_map<const u8*, const AnimationRecord*> fly_loop_records;

// Actor +0x50 controller: the first slot's player pause count (+0x58 -> +0 -> +0x125) and dirty flag (+0x5C).
void adjust_animation_pause(AnimationController* controller, i32 step)
{
    if (controller != nullptr)
    {
        if (controller->slots.at(0).player != nullptr)
        {
            controller->slots[0].player->paused = u8(controller->slots[0].player->paused + step);
        }
        controller->dirty = true;
    }
}

void adjust_body_counters(std::span<u8> body, i32 step)
{
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + step);
    }
}

// Xbox 0x001628B0
void destroy_empty_state(FactoryObject&)
{
}

bool construct_stand_alert_view(FactoryObject& object, std::span<const u8> parameters)
{
    StandAlertState& state = body_state_view<StandAlertState>({object.bytes.get(), object.size});
    auto context = body_context_of(state.base.body);
    return initialize_runtime_stand_alert(state, parameters, context);
}

void destroy_stand_alert_view(FactoryObject& object)
{
    StandAlertState& state = body_state_view<StandAlertState>({object.bytes.get(), object.size});
    auto context = body_context_of(state.base.body);
    destroy_runtime_stand_alert(state, context);
}

bool construct_walk_alert_view(FactoryObject& object, std::span<const u8> parameters)
{
    WalkAlertState& state = body_state_view<WalkAlertState>({object.bytes.get(), object.size});
    auto context = body_context_of(state.base.body);
    return initialize_runtime_walk_alert(state, parameters, context);
}

void destroy_walk_alert_view(FactoryObject& object)
{
    WalkAlertState& state = body_state_view<WalkAlertState>({object.bytes.get(), object.size});
    auto context = body_context_of(state.base.body);
    destroy_runtime_walk_alert(state, context);
}

bool construct_fly_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    FlyState& state = body_state_view<FlyState>({object.bytes.get(), object.size});
    auto context = body_context_of(state.base.body);
    return initialize_runtime_fly_state(state, parameters, context);
}

void destroy_fly_state_view(FactoryObject& object)
{
    FlyState& state = body_state_view<FlyState>({object.bytes.get(), object.size});
    auto context = body_context_of(state.base.body);
    destroy_runtime_fly_state(state, context);
}

// Body +0x15C: the bit block whose +8 bytes the furnace state sets.
RuntimeBodyBitStorage& bits_of(const RuntimeBodyStateContext& context)
{
    if (context.bits == nullptr)
    {
        throw Error("body state has no body +0x15C bit block");
    }
    return *context.bits;
}

bool construct_hit_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    HitState& state = body_state_view<HitState>({object.bytes.get(), object.size});
    auto context = hit_context_of(state.base.body);
    return initialize_runtime_hit_state(state, parameters, context);
}

void destroy_hit_state_view(FactoryObject& object)
{
    HitState& state = body_state_view<HitState>({object.bytes.get(), object.size});
    auto context = hit_context_of(state.base.body);
    destroy_runtime_hit_state(state, context);
}

const ObjectFactoryVtable* hit_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505940, hit_state_object_sizes, construct_hit_state_view,
        destroy_hit_state_view};
    return &table;
}

bool construct_hit_furnace_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    HitFurnaceState& state = body_state_view<HitFurnaceState>({object.bytes.get(), object.size});
    auto context = body_context_of(state.base.body);
    return initialize_runtime_hit_furnace_state(state, parameters, context.body, bits_of(context));
}

void destroy_hit_furnace_state_view(FactoryObject& object)
{
    HitFurnaceState& state = body_state_view<HitFurnaceState>({object.bytes.get(), object.size});
    auto context = body_context_of(state.base.body);
    destroy_runtime_hit_furnace_state(state, context.body, bits_of(context));
}

const ObjectFactoryVtable* hit_furnace_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505960, hit_furnace_state_object_sizes,
        construct_hit_furnace_state_view, destroy_hit_furnace_state_view};
    return &table;
}

bool construct_dizzy_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    DizzyState& state = body_state_view<DizzyState>({object.bytes.get(), object.size});
    return initialize_runtime_dizzy_state(state, parameters, dizzy_context_of(state.base.body));
}

void destroy_dizzy_state_view(FactoryObject& object)
{
    DizzyState& state = body_state_view<DizzyState>({object.bytes.get(), object.size});
    destroy_runtime_dizzy_state(state, dizzy_context_of(state.base.body));
}

const ObjectFactoryVtable* dizzy_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505760, dizzy_state_object_sizes, construct_dizzy_state_view,
        destroy_dizzy_state_view};
    return &table;
}

bool construct_anim_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    AnimState& state = body_state_view<AnimState>({object.bytes.get(), object.size});
    return initialize_runtime_anim_state(state, parameters, anim_context_of(state.base.body));
}

void destroy_anim_state_view(FactoryObject& object)
{
    AnimState& state = body_state_view<AnimState>({object.bytes.get(), object.size});
    destroy_runtime_anim_state(state, anim_context_of(state.base.body));
}

const ObjectFactoryVtable* anim_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x005056A0, anim_state_object_sizes, construct_anim_state_view,
        destroy_anim_state_view};
    return &table;
}

bool construct_die_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    DieState& state = body_state_view<DieState>({object.bytes.get(), object.size});
    return initialize_runtime_die_state(state, parameters, die_context_of(state.base.body));
}

void destroy_die_state_view(FactoryObject& object)
{
    DieState& state = body_state_view<DieState>({object.bytes.get(), object.size});
    destroy_runtime_die_state(state, die_context_of(state.base.body));
}

const ObjectFactoryVtable* die_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505720, die_state_object_sizes, construct_die_state_view,
        destroy_die_state_view};
    return &table;
}

bool construct_knockdown_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    KnockdownState& state = body_state_view<KnockdownState>({object.bytes.get(), object.size});
    return initialize_runtime_knockdown_state(state, parameters, knockdown_context_of(state.base.body));
}

void destroy_knockdown_state_view(FactoryObject& object)
{
    KnockdownState& state = body_state_view<KnockdownState>({object.bytes.get(), object.size});
    destroy_runtime_knockdown_state(state, knockdown_context_of(state.base.body));
}

const ObjectFactoryVtable* knockdown_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505A20, knockdown_state_object_sizes, construct_knockdown_state_view,
        destroy_knockdown_state_view};
    return &table;
}

bool construct_pickup_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    PickupState& state = body_state_view<PickupState>({object.bytes.get(), object.size});
    return initialize_runtime_pickup_state(state, parameters, pickup_context_of(state.base.body));
}

void destroy_pickup_state_view(FactoryObject& object)
{
    PickupState& state = body_state_view<PickupState>({object.bytes.get(), object.size});
    destroy_runtime_pickup_state(state, pickup_context_of(state.base.body));
}

const ObjectFactoryVtable* pickup_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505A60, pickup_state_object_sizes, construct_pickup_state_view,
        destroy_pickup_state_view};
    return &table;
}

bool construct_lift_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    LiftState& state = body_state_view<LiftState>({object.bytes.get(), object.size});
    return initialize_runtime_lift_state(state, parameters, pickup_context_of(state.base.body));
}

void destroy_lift_state_view(FactoryObject& object)
{
    LiftState& state = body_state_view<LiftState>({object.bytes.get(), object.size});
    destroy_runtime_lift_state(state, pickup_context_of(state.base.body));
}

const ObjectFactoryVtable* lift_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505A80, lift_state_object_sizes, construct_lift_state_view,
        destroy_lift_state_view};
    return &table;
}

const ObjectFactoryVtable* actor_move_lifecycle()
{
    static const ObjectFactoryVtable table{0x004DD72C, actor_move_object_sizes, initialize_runtime_weapon_action,
        destroy_empty_state};
    return &table;
}

const ObjectFactoryVtable* stand_alert_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505BC0, stand_alert_object_sizes, construct_stand_alert_view,
        destroy_stand_alert_view};
    return &table;
}

bool construct_grabbed_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    GrabbedState& state = body_state_view<GrabbedState>({object.bytes.get(), object.size});
    return initialize_runtime_grabbed_state(state, parameters, grabbed_context_of(state.base.body));
}

void destroy_grabbed_state_view(FactoryObject& object)
{
    GrabbedState& state = body_state_view<GrabbedState>({object.bytes.get(), object.size});
    destroy_runtime_grabbed_state(state, grabbed_context_of(state.base.body));
}

const ObjectFactoryVtable* grabbed_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x005058E0, grabbed_state_object_sizes, construct_grabbed_state_view,
        destroy_grabbed_state_view};
    return &table;
}

bool construct_player_death_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    PlayerDeathState& state = body_state_view<PlayerDeathState>({object.bytes.get(), object.size});
    return initialize_runtime_player_death_state(state, parameters, player_death_context_of(state.base.body));
}

void destroy_player_death_state_view(FactoryObject& object)
{
    PlayerDeathState& state = body_state_view<PlayerDeathState>({object.bytes.get(), object.size});
    destroy_runtime_player_death_state(state, player_death_context_of(state.base.body));
}

const ObjectFactoryVtable* player_death_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505800, player_death_state_object_sizes,
        construct_player_death_state_view, destroy_player_death_state_view};
    return &table;
}

bool construct_struggle_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    StruggleState& state = body_state_view<StruggleState>({object.bytes.get(), object.size});
    auto context = body_context_of(state.base.body);
    return initialize_runtime_struggle_state(state, parameters, context);
}

void destroy_struggle_state_view(FactoryObject& object)
{
    StruggleState& state = body_state_view<StruggleState>({object.bytes.get(), object.size});
    auto context = body_context_of(state.base.body);
    destroy_runtime_struggle_state(state, context);
}

const ObjectFactoryVtable* struggle_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505BE0, struggle_state_object_sizes, construct_struggle_state_view,
        destroy_struggle_state_view};
    return &table;
}

bool construct_walk_target_view(FactoryObject& object, std::span<const u8> parameters)
{
    WalkTargetState& state = body_state_view<WalkTargetState>({object.bytes.get(), object.size});
    auto context = body_context_of(state.base.body);
    return initialize_runtime_walk_target(state, parameters, context);
}

void destroy_walk_target_view(FactoryObject& object)
{
    WalkTargetState& state = body_state_view<WalkTargetState>({object.bytes.get(), object.size});
    auto context = body_context_of(state.base.body);
    destroy_runtime_walk_target(state, context);
}

bool construct_fly_alert_view(FactoryObject& object, std::span<const u8> parameters)
{
    FlyAlertState& state = body_state_view<FlyAlertState>({object.bytes.get(), object.size});
    auto context = body_context_of(state.base.body);
    return initialize_runtime_fly_alert(state, parameters, context);
}

void destroy_fly_alert_view(FactoryObject& object)
{
    FlyAlertState& state = body_state_view<FlyAlertState>({object.bytes.get(), object.size});
    auto context = body_context_of(state.base.body);
    destroy_runtime_fly_alert(state, context);
}

bool construct_throw_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    ThrowState& state = body_state_view<ThrowState>({object.bytes.get(), object.size});
    return initialize_runtime_throw_state(state, parameters, throw_context_of(state.base.body));
}

void destroy_throw_state_view(FactoryObject& object)
{
    ThrowState& state = body_state_view<ThrowState>({object.bytes.get(), object.size});
    destroy_runtime_throw_state(state, throw_context_of(state.base.body));
}

const ObjectFactoryVtable* throw_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505C40, throw_state_object_sizes, construct_throw_state_view,
        destroy_throw_state_view};
    return &table;
}

bool construct_hold_state_view(FactoryObject& object, std::span<const u8>)
{
    HoldState& hold = body_state_view<HoldState>({object.bytes.get(), object.size});
    auto context = body_context_of(hold.base.body);
    return initialize_runtime_hold_state(hold, context);
}

void destroy_hold_state_view(FactoryObject& object)
{
    HoldState& hold = body_state_view<HoldState>({object.bytes.get(), object.size});
    auto context = body_context_of(hold.base.body);
    destroy_runtime_hold_state(hold, context);
}

const ObjectFactoryVtable* hold_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505C20, hold_state_object_sizes, construct_hold_state_view,
        destroy_hold_state_view};
    return &table;
}

// Getter 0x00091AB0: the walk target's sizes 0x00091A90.
const ObjectFactoryVtable* fly_alert_lifecycle()
{
    static const ObjectFactoryVtable table{0x005058A0, walk_target_object_sizes, construct_fly_alert_view,
        destroy_fly_alert_view};
    return &table;
}

const ObjectFactoryVtable* walk_target_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505CC0, walk_target_object_sizes, construct_walk_target_view,
        destroy_walk_target_view};
    return &table;
}

const ObjectFactoryVtable* fly_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505880, fly_state_object_sizes, construct_fly_state_view,
        destroy_fly_state_view};
    return &table;
}

const ObjectFactoryVtable* walk_alert_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505CA0, walk_alert_object_sizes, construct_walk_alert_view,
        destroy_walk_alert_view};
    return &table;
}
}

// Xbox 0x0001FEB0
u8 set_runtime_body_shape_mode(u8 mode, RuntimeBodyStateContext& context)
{
    Actor& actor = context.actor;
    switch (mode)
    {
    case 0:
    case 1:
        actor.model_mode = mode == 0 ? 0 : 4;
        register_actor_shape_probe(actor, context.shape);
        actor.shape_mode = context.body[0x245];
        break;
    case 2:
    case 3:
    case 4:
        actor.model_mode = mode == 4 ? 4 : 0x20002;
        register_actor_shape_probe(actor, context.shape);
        actor.shape_mode = 4;
        break;
    default:
        break;
    }
    const u8 previous = context.body[0x244];
    context.body[0x244] = mode;
    return previous;
}

// Xbox 0x00095530
void actor_move_object_sizes(ObjectSizes& output)
{
    output.instance = 0x18;
    output.parameters = 0xC;
}

// Xbox 0x000A2510
void stand_alert_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(StandAlertState);
    output.parameters = stand_parameter::kSize;
}

// Xbox 0x000A2590
bool initialize_runtime_stand_alert(StandAlertState& state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context)
{
    const auto body = context.body;
    state.input_key = get(parameters, stand_parameter::input_key);
    state.alert_key = get(parameters, stand_parameter::alert_key);
    state.moving_key = get(parameters, stand_parameter::moving_key);
    state.raises_counters = get(parameters, stand_parameter::raises_counters);
    state.pauses_animation = get(parameters, stand_parameter::pauses_animation);
    state.holds = get(parameters, stand_parameter::holds);
    state.body_bit = get(parameters, stand_parameter::body_bit);
    state.node_slot = get(parameters, stand_parameter::node_slot);
    state.keeps_node = get(parameters, stand_parameter::keeps_node);
    state.detaches_secondary = get(parameters, stand_parameter::detaches_secondary);
    state.settles = get(parameters, stand_parameter::settles);
    state.clears_secondary_mode = get(parameters, stand_parameter::clears_secondary_mode);
    state.saved_secondary_mode = context.secondary_mode;
    context.bits->bytes[6] |= 1;
    body[0x38] = 1;
    body[0x39] = 1;
    std::memset(body.data() + 0x160, 0, 2);
    if (state.pauses_animation != 0)
    {
        adjust_animation_pause(context.animation, 1);
    }
    if (state.raises_counters != 0)
    {
        adjust_body_counters(body, 1);
    }
    const char* sound = field_data(parameters, stand_parameter::sound);
    if (sound[0] != 0)
    {
        if (sound[0] != 0)
        {
            queue_actor_sound(context.actor.sound_slots[4], 0, &context.actor, 4, nullptr, nullptr, 1.f, sound,
                context.sounds);
        }
        state.sound_started = 1;
    }
    const u32 bit = state.body_bit;
    if (bit != 0)
    {
        context.bits->bytes[bit >> 3] |= u8(1u << (bit & 7));
    }
    if (state.node_slot != 0xFFFFFFFF)
    {
        const u8 slot = u8(state.node_slot);
        if (context.actor.model != nullptr && context.actor.model->runtime != nullptr)
        {
            set_runtime_model_node_state(*context.actor.model->runtime, slot, 1, context.node_states);
        }
        if (context.secondary != nullptr)
        {
            set_runtime_model_node_state(*context.secondary, slot, 1, context.node_states);
        }
    }
    if (get(parameters, stand_parameter::shape_mode) == 0)
    {
        state.saved_shape_mode = body[0x244];
    }
    else
    {
        state.saved_shape_mode = set_runtime_body_shape_mode(u8(get(parameters, stand_parameter::shape_mode)), context);
    }
    if (state.holds != 0)
    {
        set(body, body_field::holds, get(body, body_field::holds) + 1);
    }
    if (state.clears_secondary_mode != 0 && context.secondary != nullptr)
    {
        context.secondary_mode = 0;
    }
    return true;
}

// Xbox 0x000A2790
void destroy_runtime_stand_alert(StandAlertState& state, RuntimeBodyStateContext& context)
{
    const auto body = context.body;
    context.bits->bytes[6] &= 0xFE;
    if (state.raises_counters != 0)
    {
        adjust_body_counters(body, -1);
    }
    if (state.pauses_animation != 0)
    {
        adjust_animation_pause(context.animation, -1);
    }
    if (state.sound_started != 0)
    {
        release_actor_sound(context.actor.sound_slots[4], context.sounds);
    }
    const u32 bit = state.body_bit;
    if (bit != 0)
    {
        context.bits->bytes[bit >> 3] &= u8(~(1u << (bit & 7)));
    }
    if (state.node_slot != 0xFFFFFFFF && state.keeps_node == 0)
    {
        const u8 slot = u8(state.node_slot);
        if (context.actor.model != nullptr && context.actor.model->runtime != nullptr)
        {
            set_runtime_model_node_state(*context.actor.model->runtime, slot, 0, context.node_states);
        }
        if (context.secondary != nullptr)
        {
            set_runtime_model_node_state(*context.secondary, slot, 0, context.node_states);
        }
    }
    set_runtime_body_shape_mode(u8(state.saved_shape_mode), context);
    if (state.holds != 0)
    {
        const u32 count = get(body, body_field::holds) - 1;
        set(body, body_field::holds, count);
        if (count == 0)
        {
            const u32 message[]{0x4A};
            context.body_message(message);
        }
    }
    if (state.detaches_secondary != 0 && context.attached != nullptr)
    {
        auto* attached = context.attached;
        detach_runtime_body_secondary(body, context.attached, context.receiver);
        const u32 message[]{0x97, 0};
        attached->message(message);
    }
    if (state.clears_secondary_mode != 0 && context.secondary != nullptr)
    {
        context.secondary_mode = state.saved_secondary_mode;
    }
}

// Xbox 0x000A55D0
void walk_alert_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(WalkAlertState);
    output.parameters = walk_parameter::kSize;
}

// Xbox 0x000A5650
bool initialize_runtime_walk_alert(WalkAlertState& state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context)
{
    const auto body = context.body;
    const i16 count = get(parameters, walk_parameter::row_count);
    state.row_count = count;
    auto& records = walk_alert_records[body_state_key(state)];
    records.assign(count > 0 ? size_t(count) : 0, nullptr);
    for (i16 index = 0; index < count; ++index)
    {
        const auto entry = context.resolve(get(parameters, walk_parameter::rows) + u32(index) * row::kSize, row::kSize);
        const u32 name_address = get(entry, row::animation);
        const char* name = name_address == 0 ? nullptr
            : reinterpret_cast<const char*>(context.resolve(name_address, 0).data());
        // The inlined 0x0003AAA0 pair: the attached actor's +0xDB0 variant first, then the plain name.
        const AnimationRecord* record = find_runtime_body_animation(name, context.actions);
        records[size_t(index)] = record;
        WalkAlertRow& band = state.rows[size_t(index)];
        band.found = record != nullptr ? 1 : 0;
        band.lower_duration = get(entry, row::lower_duration);
        band.upper_duration = get(entry, row::upper_duration);
        band.top_speed = get(entry, row::top_speed);
    }
    state.band = 0;
    state.stop_key = get(parameters, walk_parameter::stop_key);
    state.alert_key = get(parameters, walk_parameter::alert_key);
    state.settles = get(parameters, walk_parameter::settles);
    state.stops_at_once = get(parameters, walk_parameter::stops_at_once);
    state.stick_limit = get(parameters, walk_parameter::stick_limit);
    state.holds_heading = get(parameters, walk_parameter::holds_heading);
    state.hold_heading_above = get(parameters, walk_parameter::hold_heading_above);
    state.alert_when_calm = get(parameters, walk_parameter::alert_when_calm);
    state.fixed_turn_ratio = get(parameters, walk_parameter::fixed_turn_ratio);
    state.body_bit = get(parameters, walk_parameter::body_bit);
    context.bits->bytes[6] |= 0x80;
    const char* sound = field_data(parameters, walk_parameter::sound);
    if (sound[0] != 0)
    {
        if (sound[0] != 0)
        {
            queue_actor_sound(context.actor.sound_slots[4], 0, &context.actor, 4, nullptr, nullptr, 1.f, sound,
                context.sounds);
        }
        state.sound_started = 1;
    }
    const u32 bit = state.body_bit;
    if (bit != 0)
    {
        context.bits->bytes[bit >> 3] |= u8(1u << (bit & 7));
    }
    // The inlined 0x0003A9E0 for the band (0): 0x000322A0 with actor +0x84, the +0x50 controller (0x0003ACB0
    // inline) and 0x00030880, then actor +0x84 is cleared.
    const u16 band = state.band;
    if (band >= records.size() || records[band] == nullptr)
    {
        throw Error("walk-alert band record is null; 0x000A5650 reads its +0x117");
    }
    start_actor_animation(context.actions.animation, *records[band], context.actions.registry,
        context.actions.error, context.actions.random);
    notify_runtime_body_animation(context.actions);
    body[0x38] = 1;
    body[0x39] = 1;
    return true;
}

// Xbox 0x000A5950
void destroy_runtime_walk_alert(WalkAlertState& state, RuntimeBodyStateContext& context)
{
    if (state.sound_started != 0)
    {
        release_actor_sound(context.actor.sound_slots[4], context.sounds);
    }
    const u32 bit = state.body_bit;
    if (bit != 0)
    {
        context.bits->bytes[bit >> 3] &= u8(~(1u << (bit & 7)));
    }
    context.bits->bytes[6] &= 0x7F;
    walk_alert_records.erase(body_state_key(state));
}

// Xbox 0x00090D10
void fly_state_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(FlyState);
    output.parameters = fly_parameter::kSize;
}

// Xbox 0x00090D90
bool initialize_runtime_fly_state(FlyState& state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context)
{
    const auto body = context.body;
    const i16 count = get(parameters, fly_parameter::row_count);
    state.row_count = count;
    auto& records = walk_alert_records[body_state_key(state)];
    records.assign(count > 0 ? size_t(count) : 0, nullptr);
    for (i16 index = 0; index < count; ++index)
    {
        const auto entry = context.resolve(get(parameters, fly_parameter::rows) + u32(index) * row::kSize, row::kSize);
        const u32 name_address = get(entry, row::animation);
        const char* name = name_address == 0 ? nullptr
            : reinterpret_cast<const char*>(context.resolve(name_address, 0).data());
        // The inlined 0x0003AAA0 pair: the attached actor's +0xDB0 variant first, then the plain name.
        const AnimationRecord* record = find_runtime_body_animation(name, context.actions);
        records[size_t(index)] = record;
        WalkAlertRow& band = state.rows[size_t(index)];
        band.found = record != nullptr ? 1 : 0;
        band.lower_duration = get(entry, row::lower_duration);
        band.upper_duration = get(entry, row::upper_duration);
        band.top_speed = get(entry, row::top_speed);
    }
    state.band = 0;
    state.stop_key = get(parameters, fly_parameter::stop_key);
    state.alert_key = get(parameters, fly_parameter::alert_key);
    state.stick_limit = get(parameters, fly_parameter::stick_limit);
    state.holds_heading = get(parameters, fly_parameter::holds_heading);
    state.hold_heading_above = get(parameters, fly_parameter::hold_heading_above);
    const char* sound = field_data(parameters, fly_parameter::sound);
    if (sound[0] != 0)
    {
        if (sound[0] != 0)
        {
            queue_actor_sound(context.actor.sound_slots[4], 0, &context.actor, 4, nullptr, nullptr, 1.f, sound,
                context.sounds); // 0x0012A260
        }
        state.sound_started = 1;
    }
    // The looping animation: the inlined 0x0003AAA0 pair, then the inlined 0x0001AFA0.
    const u32 loop_address = get(parameters, fly_parameter::loop);
    const char* loop_name = loop_address == 0 ? nullptr
        : reinterpret_cast<const char*>(context.resolve(loop_address, 0).data());
    const AnimationRecord* loop = find_runtime_body_animation(loop_name, context.actions);
    fly_loop_records[body_state_key(state)] = loop;
    state.loop_found = loop != nullptr ? 1 : 0;
    if (loop != nullptr)
    {
        start_actor_animation(context.actions.animation, *loop, context.actions.registry, context.actions.error,
            context.actions.random);
        notify_runtime_body_animation(context.actions); // 0x0001B020
    }
    // The inlined 0x0001AFA0 for the band (0).
    const u16 band = state.band;
    if (band >= records.size() || records[band] == nullptr)
    {
        throw Error("fly band record is null; 0x00090D90 reads its +0x117");
    }
    start_actor_animation(context.actions.animation, *records[band], context.actions.registry,
        context.actions.error, context.actions.random);
    notify_runtime_body_animation(context.actions); // 0x0001B020
    body[0x38] = 1;
    body[0x39] = 1;
    context.bits->bytes[6] |= 0x80;
    return true;
}

// Xbox 0x00091110
void destroy_runtime_fly_state(FlyState& state, RuntimeBodyStateContext& context)
{
    if (state.sound_started != 0)
    {
        release_actor_sound(context.actor.sound_slots[4], context.sounds); // 0x0012A3D0
    }
    if (state.loop_found != 0)
    {
        // The loop's slot fades out over 0.25 s (DAT_0032B104).
        const AnimationRecord* loop = fly_loop_records[body_state_key(state)];
        AnimationController* controller = context.animation; // Actor +0x50.
        const i32 slot = loop == nullptr ? 0 : i32(loop->slot); // Record +0x117.
        if (controller != nullptr && slot != 0)
        {
            AnimationControllerSlot& entry = controller->slots.at(size_t(slot));
            if (entry.player != nullptr)
            {
                entry.fade_duration = 0.25f; // +0x1C.
                entry.fade_mode = 3; // +0x18.
            }
            controller->dirty = true; // +0x5C.
        }
    }
    context.bits->bytes[6] &= 0x7F;
    walk_alert_records.erase(body_state_key(state));
    fly_loop_records.erase(body_state_key(state));
}

const AnimationRecord* runtime_walk_alert_record(const u8* state_key, u16 index)
{
    const auto found = walk_alert_records.find(state_key);
    if (found == walk_alert_records.end() || index >= found->second.size())
    {
        throw Error("walk-alert state has no record for its speed band");
    }
    return found->second[index];
}

void install_runtime_body_state_classes(std::span<ObjectFactoryType> types,
    std::function<RuntimeBodyStateContext(u32)> context_of, std::function<RuntimeHitStateLifecycle(u32)> hit_of,
    std::function<RuntimeDizzyStateContext&(u32)> dizzy_of, std::function<RuntimeKnockdownContext&(u32)> knockdown_of,
    std::function<RuntimeDieStateContext&(u32)> die_of, std::function<RuntimeAnimStateContext&(u32)> anim_of,
    std::function<RuntimePickupStateContext&(u32)> pickup_of,
    std::function<RuntimePlayerDeathContext&(u32)> player_death_of,
    std::function<RuntimeGrabbedStateContext&(u32)> grabbed_of,
    std::function<RuntimeThrowStateContext&(u32)> throw_of)
{
    throw_context_of = std::move(throw_of);
    if (throw_context_of)
    {
        types[0x8B] = ObjectFactoryType{9, throw_state_lifecycle};
    }
    grabbed_context_of = std::move(grabbed_of);
    types[0x70] = ObjectFactoryType{9, grabbed_state_lifecycle};
    player_death_context_of = std::move(player_death_of);
    types[0x69] = ObjectFactoryType{9, player_death_state_lifecycle};
    pickup_context_of = std::move(pickup_of);
    types[0x7C] = ObjectFactoryType{9, pickup_state_lifecycle};
    types[0x7D] = ObjectFactoryType{9, lift_state_lifecycle};
    anim_context_of = std::move(anim_of);
    types[0x5E] = ObjectFactoryType{9, anim_state_lifecycle};
    die_context_of = std::move(die_of);
    types[0x62] = ObjectFactoryType{9, die_state_lifecycle};
    knockdown_context_of = std::move(knockdown_of);
    types[0x7A] = ObjectFactoryType{9, knockdown_state_lifecycle};
    body_context_of = std::move(context_of);
    hit_context_of = std::move(hit_of);
    dizzy_context_of = std::move(dizzy_of);
    types[0x64] = ObjectFactoryType{9, dizzy_state_lifecycle};
    types[0x73] = ObjectFactoryType{9, hit_state_lifecycle};
    types[0x74] = ObjectFactoryType{9, hit_furnace_state_lifecycle};
    types[9] = ObjectFactoryType{1, actor_move_lifecycle};
    types[0x87] = ObjectFactoryType{9, stand_alert_lifecycle};
    types[0x88] = ObjectFactoryType{9, struggle_state_lifecycle};
    types[0x8E] = ObjectFactoryType{9, walk_alert_lifecycle};
    types[0x8F] = ObjectFactoryType{9, walk_target_lifecycle};
    types[0x6E] = ObjectFactoryType{9, fly_alert_lifecycle};
    types[0x6D] = ObjectFactoryType{9, fly_state_lifecycle};
    types[0x8A] = ObjectFactoryType{9, hold_state_lifecycle};
}
}
