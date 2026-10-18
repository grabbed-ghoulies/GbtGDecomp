#include "body_pickup_state.h"
#include "actor_attachment_events.h"
#include "actor_spawn_eligibility.h"
#include "actor_spawn_query.h"
#include "actor_transform.h"
#include "player_mind_events.h"
#include "../camera/camera_mind_rig.h"
#include "../scripts/script_conditions.h"
#include "gbtg/anim.h"

#include <array>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void adjust_body_counters(std::span<u8> body, i32 step)
{
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + step);
    }
}

// Each record on the actor's +0x8F4 list (actor +0x8F8 is the first) gains or drops a +0x15 hold.
void adjust_live_attachment_holds(entity_detail::ListHead& live_attachments, i32 step)
{
    for (auto* live = static_cast<ActorLiveAttachment*>(live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->retained_15 = u8(live->retained_15 + step);
    }
}

const MinaPlayer* first_player(const ActorAnimationState& animation)
{
    const AnimationController* controller = animation.controller.get();
    return controller == nullptr ? nullptr : controller->slots.at(0).player.get();
}

// The inlined test of 0x0009DF90: the animation passed `grab` since the last step (+0x10C the previous position).
bool runtime_pickup_grab_crossed(const MinaPlayer& player, float grab)
{
    if (player.position == player.previous)
    {
        return false;
    }
    float from = player.previous;
    float to = player.position;
    if (0.f > player.speed)
    {
        from = player.position;
        to = player.previous;
    }
    if (to > from)
    {
        return grab >= from && to > grab;
    }
    return grab >= from || to > grab;
}

// The inlined weapon row lookup: its +0xC value for the holder's class (actor +6), or zero.
float runtime_pickup_grab_point(std::span<const u8> weapon, u16 host_class)
{
    const i32 count = i16(read_u16(weapon, 0x944));
    for (i32 index = 0; index < count; ++index)
    {
        const size_t offset = 0x948 + size_t(index) * 0x10;
        if (read_u16(weapon, offset) == host_class)
        {
            return read_f32(weapon, offset + 0xC);
        }
    }
    return 0.f;
}

// Body +0x48's weapon leaves the pick-up: broadcast {0x74, actor, weapon}, drop its +0xAE4 count and, for a
// controlled player, queue {0xAD, weapon tag} for the script.
void release_runtime_pickup_weapon(RuntimePickupStateContext& context)
{
    const auto body = context.body;
    const u32 weapon = read_u32(body, 0x48);
    if (weapon == 0)
    {
        return;
    }
    RuntimeBodyAttachedActor* held = context.attachment.ownership.attached;
    if (held == nullptr || held->address != weapon)
    {
        throw Error("pick-up state body +0x48 is not the attachment owner's weapon");
    }
    const std::array<u32, 3> pulse{0x74, read_u32(body, 8), weapon};
    broadcast_actor_pulse_message(reinterpret_cast<const i32*>(pulse.data())); // 0x00122C70
    const u16 count = u16(read_u16(held->bytes, 0xAE4) - 1);
    std::memcpy(held->bytes.data() + 0xAE4, &count, 2);
    if (read_u16(context.actor_bytes, 0xCDC) == 2 || read_u32(context.actor_bytes, 0x93C) != 0)
    {
        const std::array<u32, 2> event{0xAD, read_u16(held->bytes, 6)};
        script_queue_event(context.game,
            {reinterpret_cast<const u8*>(event.data()), sizeof(event)}); // 0x00116810
    }
}
}

// Xbox 0x0009DD80
void pickup_state_object_sizes(ObjectSizes& output)
{
    output.instance = 0x2C;
    output.parameters = 0x10;
}

// Xbox 0x0009DE00
bool initialize_runtime_pickup_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimePickupStateContext& context)
{
    const auto body = context.body;
    body[0x38] = 1;
    body[0x39] = 1;
    context.bits.bytes[5] |= 4;
    adjust_live_attachment_holds(context.live_attachments, 1);
    body[0x6D] = u8(body[0x6D] + 1);
    adjust_body_counters(body, 1);
    const Vec3 start = actor_world_position(context.actor); // 0x000412A0
    std::memcpy(state.data() + 0x1C, &start, sizeof(start));
    put_word(state, 0x18, 0);
    put_word(state, 0x28, read_u32(parameters, 0xC));
    return true;
}

// Xbox 0x0009DEA0
void destroy_runtime_pickup_state(std::span<u8>, RuntimePickupStateContext& context)
{
    const auto body = context.body;
    context.bits.bytes[5] &= 0xFB;
    adjust_body_counters(body, -1);
    adjust_live_attachment_holds(context.live_attachments, -1);
    body[0x6D] = u8(body[0x6D] - 1);
    release_runtime_pickup_weapon(context);
}

// Xbox 0x0009E2B0
void approach_runtime_pickup(std::span<const u8> state, RuntimePlayerAttachmentActor& weapon, float grab,
    RuntimePickupStateContext& context)
{
    const auto body = context.body;
    const Vec3 position{read_f32(body, 0x18), read_f32(body, 0x1C), read_f32(body, 0x20)};
    Vec3 point;
    float radius = 0.f;
    i32 index = 0;
    // EBX the weapon: its nearest pick-up sphere to the body's position. The result is not tested; a weapon
    // without spheres would leave the point unset.
    if (!select_actor_spawn_point(weapon.actor, position, point, radius, index)) // 0x001476D0
    {
        throw Error("0x0009E2B0 reads the pick-up point of a weapon without pick-up spheres");
    }
    const Vec3 direction{point.x - position.x, point.y - position.y, point.z - position.z};
    float angle = 0.f;
    if (runtime_direction_angle(direction, angle)) // 0x00105F90
    {
        put_float(body, 0x734, 0.f);
        put_float(body, 0x72C, 0.f);
        put_float(body, 0x730, angle);
    }
    const MinaPlayer* player = first_player(context.animation);
    const float position_now = player == nullptr ? 0.f : player->position;
    if (grab >= position_now && grab > 0.f)
    {
        const float ratio = position_now / grab;
        if (radius > 0.f)
        {
            offset_camera_point(point, angle + std::bit_cast<float>(0x40490FDBu), radius); // 0x001062A0, DAT_0032B148
        }
        put_float(body, 0x18, (point.x - read_f32(state, 0x1C)) * ratio + read_f32(state, 0x1C));
        put_float(body, 0x1C, position.y);
        put_float(body, 0x20, (point.z - read_f32(state, 0x24)) * ratio + read_f32(state, 0x24));
    }
}

// Xbox 0x0009DF90
void tick_runtime_pickup_state(std::span<u8> state, RuntimePickupStateContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    const auto mind = context.mind;
    const u32 weapon_address = mind.empty() ? 0 : read_u32(mind, 0x18);
    put_float(state, 0x10, runtime_actor_step(actor, context.clock) + read_f32(state, 0x10));
    put_float(body, 0x734, 0.f);
    put_float(body, 0x72C, 0.f);
    if (weapon_address != 0)
    {
        auto& weapon = context.weapon_at(weapon_address);
        const float grab = runtime_pickup_grab_point(weapon.object.bytes, read_u16(context.actor_bytes, 6));
        if (read_u32(state, 0x18) == 0)
        {
            approach_runtime_pickup(state, weapon, grab, context); // 0x0009E2B0
        }
        const MinaPlayer* player = first_player(context.animation);
        if (player != nullptr && runtime_pickup_grab_crossed(*player, grab))
        {
            put_word(state, 0x18, 1);
            if (actor_can_supply_ghoulybox(weapon.actor)) // 0x00146150, ECX weapon
            {
                if (attach_runtime_player_weapon(weapon, 0, context.attachment, context.default_position,
                        context.default_euler)) // 0x0001B190
                {
                    const u16 count = u16(read_u16(weapon.object.bytes, 0xAE4) + 1);
                    std::memcpy(weapon.object.bytes.data() + 0xAE4, &count, 2);
                    context.weapon_callouts(weapon_address, 0x48); // 0x0003BE20 over weapon +0x8C
                    const u32 message[]{0x48};
                    context.actor_message(message);
                }
            }
            if (!mind.empty())
            {
                put_word(mind, 0x18, 0);
            }
        }
    }
    const MinaPlayer* player = first_player(context.animation);
    const float position = player == nullptr ? 0.f : player->position;
    const float end = read_f32(state, 0x28);
    const bool taken_empty = read_u32(state, 0x18) != 0 && read_u32(body, 0x48) == 0;
    if (context.input == nullptr || context.input_state == nullptr)
    {
        throw Error("pick-up state stick check without the tick's input device");
    }
    std::array<float, 2> axes{};
    runtime_input_axes(*context.input, axes, *context.input_state); // 0x001085F0
    bool pushed = axes[1] * axes[1] + axes[0] * axes[0] > context.stick_threshold;
    if (!pushed)
    {
        runtime_input_secondary_axes(*context.input, axes, *context.input_state); // 0x001088D0
        pushed = axes[1] * axes[1] + axes[0] * axes[0] > context.stick_threshold;
    }
    if (end <= position && read_u32(state, 0x18) != 0 && pushed)
    {
        const u16 key = context.body_slot_key(0x2C);
        std::memcpy(state.data() + 0xC, &key, 2);
        return;
    }
    if (!taken_empty && player != nullptr)
    {
        bool done = false;
        if (player->mode == 1)
        {
            if (0.f > player->speed)
            {
                done = !(0.f < player->position);
            }
            else
            {
                done = !(player->position < 1.f); // DAT_0032B0F8.
            }
        }
        if (!done)
        {
            return;
        }
    }
    const u16 key = context.body_slot_key(0x20);
    std::memcpy(state.data() + 0xC, &key, 2);
}
}
