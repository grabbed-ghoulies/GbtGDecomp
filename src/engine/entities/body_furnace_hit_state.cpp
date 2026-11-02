#include "body_furnace_hit_state.h"
#include "actor_transform.h"
#include "body_damage_notices.h"
#include "body_hit_state.h"
#include "body_message_helpers.h"
#include "bounded_channels.h"
#include "entity.h"
#include "player_attachment.h"
#include "player_goal_cleanup.h"
#include "player_mind_events.h"
#include "../camera/camera_body_slots.h"
#include "body_fields.h"

#include <array>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = furnace_hit_state_field;

std::function<RuntimeFurnaceHitStateContext&(u32)> furnace_hit_context_of;

u32 body_address_of(std::span<const u8> state)
{
    return read_u32(state, 8);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

bool construct_furnace_hit_state_view(FactoryObject& object, std::span<const u8>)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_furnace_hit_state(state, furnace_hit_context_of(body_address_of(state)));
}

void destroy_furnace_hit_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_furnace_hit_state(state, furnace_hit_context_of(body_address_of(state)));
}

const ObjectFactoryVtable* furnace_hit_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505980, furnace_hit_state_object_sizes,
        construct_furnace_hit_state_view, destroy_furnace_hit_state_view};
    return &table;
}
}

// Xbox 0x00096A30
void furnace_hit_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = 0xC;
}

// Xbox 0x00096AB0
bool initialize_runtime_furnace_hit_state(std::span<u8> state, RuntimeFurnaceHitStateContext& context)
{
    RuntimeKnockdownContext& knockdown = context.knockdown;
    const std::span<u8> body = knockdown.body;
    Actor& actor = knockdown.actor;
    body[0x6B] = u8(body[0x6B] + 1);
    body[0x69] = u8(body[0x69] + 1);
    increment_runtime_body_latch_mask(body, 0x3F, actor, context.latches); // The inlined 0x0001F3F0, {0x47, actor}.
    set(body, body_field::holds, get(body, body_field::holds) + 1);
    body[0x309] = u8(body[0x309] + 1);
    body[0x30A] = u8(body[0x30A] + 1);
    body[0x30B] = u8(body[0x30B] + 1);
    body[0x30C] = u8(body[0x30C] + 1);
    set(body, body_field::move_holds, u16(get(body, body_field::move_holds) + 1));
    body[0x38] = 1;
    body[0x39] = 1;
    release_runtime_player_attachment(0, 0, knockdown.attachment); // 0x0001B810, EAX body
    if (RuntimeBodyAttachedActor* attached = context.state.attached; attached != nullptr) // Body +0x4C.
    {
        detach_runtime_body_secondary(body, context.state.attached, context.state.receiver); // 0x0001B570, ESI body
        const std::array<u32, 2> release{0x97, 0};
        attached->message(release); // Vtable +0xC.
    }
    knockdown.bits.bytes[3] |= 8;
    notify_runtime_body_death_once(body, knockdown.actor_address, knockdown.game); // 0x00024910, ESI body
    if (context.state.secondary != nullptr) // Actor +0x310.
    {
        context.state.secondary_mode = 0; // Actor +0x30C.
    }
    set(state, field::start, actor_world_position(actor)); // 0x000412A0
    set(state, field::start_euler, actor_world_euler(actor)); // 0x00041820
    const u32 furnace = read_u32(body, 0x26C);
    set(state, field::furnace, knockdown.actor_position(furnace)); // 0x000412A0, ESI body +0x26C
    const Vec3 toward = knockdown.actor_position(furnace);
    const Vec3 at = actor_world_position(actor);
    float yaw = 0.f;
    runtime_direction_angle(Vec3{toward.x - at.x, toward.y - at.y, toward.z - at.z}, yaw); // 0x00105F90
    set(state, field::facing, Vec3{0.f, yaw + 3.1415927f, 0.f}); // DAT_0032B148.
    const Vec3 start = get(state, field::start);
    const Vec3 end = get(state, field::furnace);
    float heading = 0.f;
    runtime_direction_angle(Vec3{end.x - start.x, end.y - start.y, end.z - start.z}, heading); // 0x00105F90
    // x87: the remainder stays on the stack for the first test; the float stored beside it is what moves.
    const long double remainder = runtime_float_remainder(heading - actor.euler.y, 6.2831855f); // 0x00152BE0
    float turn = float(remainder);
    if (-3.1415927f > remainder) // DAT_0032B29C.
    {
        turn = turn + 6.2831855f; // DAT_0032B298.
    }
    if (!(turn < 3.1415927f)) // DAT_0032B148.
    {
        turn = turn - 6.2831855f;
    }
    if (1.5707964f > engine_absolute(turn)) // 0x00152BB0, DAT_0032B140
    {
        Vec3 facing = get(state, field::facing);
        facing.y = facing.y - 3.1415927f;
        set(state, field::facing, facing);
    }
    set(state, field::remaining, 1.f); // DAT_0032B0F8.
    actor.anchor = knockdown.default_position; // Actor +0x3C, DAT_0050DD80.
    set(body, body_field::velocity, knockdown.default_position);
    knockdown.callouts(0x2C, nullptr); // 0x0003BE20 over actor +0x8C, options 0
    return true;
}

// Xbox 0x00096E60
void destroy_runtime_furnace_hit_state(std::span<u8> state, RuntimeFurnaceHitStateContext& context)
{
    (void)state;
    RuntimeKnockdownContext& knockdown = context.knockdown;
    const std::span<u8> body = knockdown.body;
    set(body, body_field::move_holds, u16(get(body, body_field::move_holds) - 1));
    body[0x6B] = u8(body[0x6B] - 1);
    body[0x69] = u8(body[0x69] - 1);
    decrement_runtime_body_latch_mask(body, 0x3F, knockdown.actor, context.latches); // The inlined 0x0001F2F0.
    const u32 count = get(body, body_field::holds) - 1;
    set(body, body_field::holds, count);
    if (count == 0)
    {
        const std::array<u32, 1> release{0x4A};
        context.state.body_message(release); // The body's vtable +0xC.
    }
    body[0x309] = u8(body[0x309] - 1);
    body[0x30B] = u8(body[0x30B] - 1);
    body[0x30C] = u8(body[0x30C] - 1);
    body[0x30A] = u8(body[0x30A] - 1);
    knockdown.bits.bytes[3] &= u8(~8u);
}

// Xbox 0x00096FC0
void tick_runtime_furnace_hit_state(std::span<u8> state, RuntimeFurnaceHitStateContext& context)
{
    RuntimeKnockdownContext& knockdown = context.knockdown;
    const std::span<u8> body = knockdown.body;
    Actor& actor = knockdown.actor;
    set(state, field::elapsed, runtime_actor_step(actor, knockdown.clock) + get(state, field::elapsed));
    put_float(body, 0x734, 0.f);
    put_float(body, 0x72C, 0.f);
    const float step = runtime_actor_step(actor, knockdown.clock);
    bool done = false;
    if (get(state, field::remaining) > 0.f)
    {
        const float left = get(state, field::remaining) - step;
        set(state, field::remaining, left);
        if (!(0.f < left))
        {
            set(state, field::remaining, 0.f);
            done = true;
        }
    }
    const float behind = get(state, field::remaining) - 1.f; // DAT_0032B0F8.
    float slide = behind * -1.f; // DAT_0032B120.
    if (slide > 1.f)
    {
        slide = 1.f;
    }
    else if (0.f > slide)
    {
        slide = 0.f;
    }
    const Vec3 start = get(state, field::start);
    const Vec3 end = get(state, field::furnace);
    const Vec3 position{(end.x - start.x) * slide + start.x, (end.y - start.y) * slide + start.y,
        (end.z - start.z) * slide + start.z};
    float turn = behind * -2.f; // DAT_0032B14C.
    if (turn > 1.f)
    {
        turn = 1.f;
    }
    else if (0.f > turn)
    {
        turn = 0.f;
    }
    const Vec3 from = get(state, field::start_euler);
    const Vec3 to = get(state, field::facing);
    const Vec3 euler{step_runtime_angle_toward(to.x, from.x, turn), step_runtime_angle_toward(to.y, from.y, turn),
        step_runtime_angle_toward(to.z, from.z, turn)}; // 0x001013A0
    actor_set_world_position(actor, position); // 0x00041730, ESI the position, EDI actor
    set(body, body_field::position, actor_world_position(actor)); // 0x000412A0, EDI body +0x18
    wrap_runtime_euler(euler, actor.euler); // 0x00105AA0, EDI actor +0x14
    actor.xform_valid = 0; // Actor +0x2A8.
    set(body, body_field::euler, actor_world_euler(actor)); // 0x00041820, EDI body +0x72C
    if (done)
    {
        finish_runtime_body_death(0, knockdown.scene_flags, knockdown.restart_requested, knockdown.death); // 0x00134AE0
    }
}

void install_runtime_furnace_hit_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeFurnaceHitStateContext&(u32 body_address)> context_of)
{
    furnace_hit_context_of = std::move(context_of);
    types[0x75] = ObjectFactoryType{9, furnace_hit_state_lifecycle}; // Getter 0x00096A50.
}
}
