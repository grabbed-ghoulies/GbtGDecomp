#include "body_arc_state.h"
#include <cstddef>
#include "body_fields.h"
#include "actor_transform.h"
#include "body_action_selection.h"
#include "body_animation_commands.h"
#include "body_hit_state.h"
#include "player_goal_cleanup.h"
#include "player_mind_requests.h"
#include "../camera/camera_body_slots.h"
#include "gbtg/model_runtime.h"
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace parameter = arc_state_parameter;

// State +0x1C / +0x20: the flight and landing records 0x0008D990 found. The records live on the host, so the
// native words only say whether one was found.
struct ArcRecords
{
    const AnimationRecord* flight = nullptr;
    const AnimationRecord* landing = nullptr;
};
std::unordered_map<const u8*, ArcRecords> arc_records;

// The record 0x0001AFA0 starts (ECX state +0x1C or +0x20); retail reads a missing one through address zero.
const AnimationRecord* record_of(const ArcState& state, bool flight)
{
    const auto found = arc_records.find(body_state_key(state));
    const AnimationRecord* record = found == arc_records.end() ? nullptr
        : flight ? found->second.flight : found->second.landing;
    if (record == nullptr)
    {
        throw Error("arc state starts a missing animation record through address zero");
    }
    return record;
}

const char* name_at(u32 address, RuntimeArcStateContext& context)
{
    return context.image_string(address);
}

std::function<RuntimeArcStateContext&(u32)> arc_context_of;

bool construct_arc_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    ArcState& state = body_state_view<ArcState>({object.bytes.get(), object.size});
    return initialize_runtime_arc_state(state, parameters, arc_context_of(state.base.body));
}

void destroy_arc_state_view(FactoryObject& object)
{
    ArcState& state = body_state_view<ArcState>({object.bytes.get(), object.size});
    destroy_runtime_arc_state(state, arc_context_of(state.base.body));
}

const ObjectFactoryVtable* arc_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x005057A0, arc_state_object_sizes, construct_arc_state_view,
        destroy_arc_state_view};
    return &table;
}

void send_word(const std::function<void(std::span<const u32>)>& send, u32 id)
{
    const std::array<u32, 1> message{id};
    send(message);
}

// The body +0x258 count the state holds; the body hears 0x4A when it drops to zero.
void release_count(ArcState& state, RuntimeArcStateContext& context)
{
    if (state.counted == 0)
    {
        return;
    }
    i32 count;
    std::memcpy(&count, context.body.data() + offsetof(BodyRecord, holds), 4);
    --count;
    std::memcpy(context.body.data() + offsetof(BodyRecord, holds), &count, 4);
    if (count == 0)
    {
        send_word(context.body_message, 0x4A);
    }
    state.counted = 0;
}

// Xbox 0x0008DD70
void fly_runtime_arc_state(ArcState& state, RuntimeArcStateContext& context)
{
    const Vec3 start = state.start;
    const Vec3 target = state.target;
    const float dx = start.x - target.x;
    const float dy = start.y - target.y;
    const float dz = start.z - target.z;
    const float height = float(std::sqrt(double(dz * dz + dy * dy + dx * dx)))
        * std::bit_cast<float>(0x3E4CCCCDu); // DAT_0032B1AC, 0.2.
    float time = runtime_actor_step(context.actor, context.clock) + state.time;
    state.time = time;
    const bool landed = time >= state.duration; // COMISS / JC: unordered keeps flying
    if (landed)
    {
        state.time = state.duration;
    }
    const float fraction = state.time / state.duration;
    Vec3 position{(target.x - start.x) * fraction + start.x, (target.y - start.y) * fraction + start.y,
        (target.z - start.z) * fraction + start.z};
    const float angle = fraction * std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
    const float lift = float(std::sin(double(angle))); // 0x00153320
    (void)std::cos(double(angle)); // 0x00153230, its result dropped.
    position.y = lift * height + position.y;
    actor_set_world_position(context.actor, position); // 0x00041730, EDI actor
   body_record(context.body).position = actor_world_position(context.actor); // 0x000412A0 into body +0x18
    const Vec3 from = state.start_euler;
    const Vec3 to = state.target_euler;
    const Vec3 euler{step_runtime_angle_toward(to.x, from.x, fraction),
        step_runtime_angle_toward(to.y, from.y, fraction),
        step_runtime_angle_toward(to.z, from.z, fraction)}; // 0x001013A0
    wrap_runtime_euler(euler, context.actor.euler); // 0x00105AA0
    context.actor.xform_valid = 0; // Actor +0x2A8.
   body_record(context.body).euler = actor_world_euler(context.actor); // 0x00041820 into body +0x72C
    if (!landed)
    {
        return;
    }
    release_count(state, context);
    if (state.latched != 0 && state.keep_latches == 0)
    {
        decrement_runtime_body_latches(context.body, context.actor, context.latches); // 0x0001FD20
        state.latched = 0;
    }
    clear_runtime_player_attachment_events(state.events, context.lists); // 0x0013B810
    if (state.landing != 0)
    {
        start_runtime_body_animation_record(*record_of(state, false), context.actions); // 0x0001AFA0
        state.phase = 3;
        return;
    }
    u16 key = state.end_key;
    if (key == 0)
    {
        key = context.body_slot_key(0x20);
    }
    state.base.next_key = key;
    state.phase = 0;
}
}

// Xbox 0x0008D910
void arc_state_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(ArcState);
    output.parameters = parameter::kSize;
}

// Xbox 0x0008D990
bool initialize_runtime_arc_state(ArcState& state, std::span<const u8> parameters,
    RuntimeArcStateContext& context)
{
    const auto body = context.body;
    u16 users;
    users = body_record(body).move_holds;
    ++users;
    body_record(body).move_holds = users;
    body_record(body).unk_38 = 0;
    body_record(body).unk_39 = 0;
    auto& records = arc_records[body_state_key(state)];
    // The inlined 0x0003AAA0 pairs: the attached actor's +0xDB0 variant first, then the plain name.
    records.flight = find_runtime_body_animation(name_at(get(parameters, parameter::flight), context),
        context.actions);
    state.flight = records.flight != nullptr ? 1 : 0;
    records.landing = find_runtime_body_animation(name_at(get(parameters, parameter::landing), context),
        context.actions);
    state.landing = records.landing != nullptr ? 1 : 0;
    state.duration = get(parameters, parameter::duration);
    state.end_key = get(parameters, parameter::end_key);
    state.keep_latches = get(parameters, parameter::keep_latches);
    if (get(parameters, parameter::take_off) != 0)
    {
        select_runtime_body_animation(name_at(get(parameters, parameter::take_off), context),
            context.actions); // 0x0001AF10
        state.phase = 1;
    }
    else
    {
        select_runtime_body_animation(name_at(get(parameters, parameter::flight), context),
            context.actions); // 0x0001AF10
        state.phase = 2;
    }
    if (const RuntimePlayerGoalRing* ring = context.ring; ring != nullptr)
    {
        if (ring->tail == ring->capacity)
        {
            throw Error("0x0008D990 reads an empty goal ring's front row through address zero");
        }
        const auto row = ring->rows.subspan(size_t(ring->head) * ring->stride);
        state.target = Vec3{read_f32(row, 4), read_f32(row, 8), read_f32(row, 0xC)};
    }
    state.start = actor_world_position(context.actor); // 0x000412A0, EDI state +0x54
    const Vec3 start = state.start;
    const Vec3 target = state.target;
    const Vec3 direction{target.x - start.x, target.y - start.y, target.z - start.z};
    float yaw = state.target_euler.y;
    runtime_direction_angle(direction, yaw); // 0x00105F90, EBX state +0x3C
    state.target_euler = Vec3{0.f, yaw, 0.f};
    state.start_euler = actor_world_euler(context.actor); // 0x00041820, EDI state +0x2C
    dispatch_actor_attachment_records(context.resource.resource.data(), i32(get(parameters, parameter::callout)),
        nullptr, nullptr, context.callouts, 0); // 0x0003BE20, EAX actor +0x8C
    i32 count;
    std::memcpy(&count, body.data() + offsetof(BodyRecord, holds), 4);
    ++count;
    std::memcpy(body.data() + offsetof(BodyRecord, holds), &count, 4);
    state.counted = 1;
    increment_runtime_body_latches(body, context.actor, context.latches); // 0x0001FC40, ECX body
    state.latched = 1;
    context.actor.anchor = Vec3{0.f, 0.f, 0.f}; // DAT_0050DD80 into actor +0x3C.
   body_record(body).velocity = Vec3{0.f, 0.f, 0.f}; // DAT_0050DD80 into body +0x318.
    send_word(context.actor_message, 0x30);
    return true;
}

// Xbox 0x0008DBF0
void destroy_runtime_arc_state(ArcState& state, RuntimeArcStateContext& context)
{
    const auto body = context.body;
    u16 users;
    users = body_record(body).move_holds;
    --users;
    body_record(body).move_holds = users;
    release_count(state, context);
    if (state.latched != 0)
    {
        decrement_runtime_body_latches(body, context.actor, context.latches); // 0x0001FD20
        state.latched = 0;
    }
    clear_runtime_player_attachment_events(state.events, context.lists); // 0x0003CFD0 loop
    arc_records.erase(body_state_key(state));
}

// Xbox 0x0008DD30
void message_runtime_arc_state(ArcState& state, std::span<const u32> message, RuntimeArcStateContext& context)
{
    if (message[0] == 0x51)
    {
        dispatch_actor_attachment_records(context.resource.resource.data(), 0x4E,
            record_runtime_body_attachment_event, nullptr, context.callouts,
            // 0x0003BE20, callback 0x0013B8B0
            context.address_of(body_state_key(state)) + u32(offsetof(ArcState, events)));
    }
}

// Xbox 0x0008DC70
void tick_runtime_arc_state(ArcState& state, RuntimeArcStateContext& context)
{
    state.base.elapsed = runtime_actor_step(context.actor, context.clock) + state.base.elapsed;
    switch (state.phase)
    {
    case 1:
        if (runtime_actor_animation_slot_complete(context.animation, 0)) // 0x0003B290, ECX 0
        {
            start_runtime_body_animation_record(*record_of(state, true), context.actions); // 0x0001AFA0
            state.phase = 2;
        }
        fly_runtime_arc_state(state, context); // 0x0008DD70
        return;
    case 2:
        fly_runtime_arc_state(state, context); // 0x0008DD70
        return;
    case 3:
        if (runtime_actor_animation_slot_complete(context.animation, 0)) // 0x0003B290, ECX 0
        {
            u16 key = state.end_key;
            if (key == 0)
            {
                key = context.body_slot_key(0x20);
            }
            state.base.next_key = key;
            state.phase = 0;
        }
        return;
    default:
        return;
    }
}

void install_runtime_arc_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeArcStateContext&(u32 body_address)> context_of)
{
    arc_context_of = std::move(context_of);
    types[0x66] = ObjectFactoryType{9, arc_state_lifecycle}; // Getter 0x0008D930.
}
}
