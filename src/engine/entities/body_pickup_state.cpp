#include "body_pickup_state.h"
#include "mind_fields.h"
#include "actor_attachment_events.h"
#include "actor_spawn_eligibility.h"
#include "actor_spawn_query.h"
#include "actor_transform.h"
#include "player_mind_events.h"
#include "player_goal_cleanup.h"
#include "../camera/camera_mind_rig.h"
#include "../scripts/script_conditions.h"
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
        live->hold = u8(live->hold + step);
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
    const u32 weapon = get(body, body_field::attached);
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
    if (get(context.actor_bytes, player_field::mode) == 2 || get(context.actor_bytes, player_field::case_link) != 0)
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
    set(state, pickup_state_field::start_position, start);
    set(state, pickup_state_field::taken, 0);
    set(state, pickup_state_field::release_point, get(parameters, pickup_state_parameter::release_point));
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
    const Vec3 position = get(body, body_field::position);
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
        const Vec3 start = get(state, pickup_state_field::start_position);
        put_float(body, 0x18, (point.x - start.x) * ratio + start.x);
        put_float(body, 0x1C, position.y);
        put_float(body, 0x20, (point.z - start.z) * ratio + start.z);
    }
}

// Xbox 0x0009DF90
void tick_runtime_pickup_state(std::span<u8> state, RuntimePickupStateContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    const auto mind = context.mind;
    const u32 weapon_address = mind.empty() ? 0 : get(mind, mind_field::action_actor);
    set(state, body_state_field::elapsed,
        runtime_actor_step(actor, context.clock) + get(state, body_state_field::elapsed));
    put_float(body, 0x734, 0.f);
    put_float(body, 0x72C, 0.f);
    if (weapon_address != 0)
    {
        auto& weapon = context.weapon_at(weapon_address);
        const float grab = runtime_pickup_grab_point(weapon.object.bytes, read_u16(context.actor_bytes, 6));
        if (get(state, pickup_state_field::taken) == 0)
        {
            approach_runtime_pickup(state, weapon, grab, context); // 0x0009E2B0
        }
        const MinaPlayer* player = first_player(context.animation);
        if (player != nullptr && runtime_pickup_grab_crossed(*player, grab))
        {
            set(state, pickup_state_field::taken, 1);
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
                set(mind, mind_field::action_actor, 0);
            }
        }
    }
    const MinaPlayer* player = first_player(context.animation);
    const float position = player == nullptr ? 0.f : player->position;
    const float end = get(state, pickup_state_field::release_point);
    const bool taken_empty = get(state, pickup_state_field::taken) != 0 && get(body, body_field::attached) == 0;
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
    if (end <= position && get(state, pickup_state_field::taken) != 0 && pushed)
    {
        const u16 key = context.body_slot_key(0x2C);
        set(state, body_state_field::next_key, key);
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
    set(state, body_state_field::next_key, key);
}

// Xbox 0x0009E430
void lift_state_object_sizes(ObjectSizes& output)
{
    output.instance = 0x30;
    output.parameters = 0x1C;
}

// Xbox 0x0009E4B0
bool initialize_runtime_lift_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimePickupStateContext& context)
{
    const auto body = context.body;
    set(state, lift_state_field::stage, 1);
    set(state, lift_state_field::ask_point, get(parameters, lift_state_parameter::ask_point));
    set(state, lift_state_field::hands_point, get(parameters, lift_state_parameter::hands_point));
    set(state, lift_state_field::release_point, get(parameters, lift_state_parameter::release_point));
    set(state, lift_state_field::hold_height, get(parameters, lift_state_parameter::hold_height));
    body[0x38] = 1;
    body[0x39] = 1;
    context.bits.bytes[5] |= 4;
    adjust_live_attachment_holds(context.live_attachments, 1);
    body[0x6D] = u8(body[0x6D] + 1);
    if (context.latches == nullptr)
    {
        throw Error("lift state has no DAT_00511AA4 latch view for 0x0001FC40");
    }
    increment_runtime_body_latches(body, context.actor, *context.latches); // 0x0001FC40, ECX body
    adjust_body_counters(body, 1);
    return true;
}

// Xbox 0x0009E560
void destroy_runtime_lift_state(std::span<u8> state, RuntimePickupStateContext& context)
{
    const auto body = context.body;
    context.bits.bytes[5] &= 0xFB;
    context.bits.bytes[5] &= 0xBF;
    adjust_body_counters(body, -1);
    adjust_live_attachment_holds(context.live_attachments, -1);
    body[0x6D] = u8(body[0x6D] - 1);
    if (context.latches == nullptr)
    {
        throw Error("lift state has no DAT_00511AA4 latch view for 0x0001FD20");
    }
    decrement_runtime_body_latches(body, context.actor, *context.latches); // 0x0001FD20, ECX body
    if (get(body, body_field::attached) != 0)
    {
        release_runtime_pickup_weapon(context); // Inlined as in 0x0009DEA0.
        return;
    }
    const u32 weapon = get(state, lift_state_field::weapon);
    if (get(state, lift_state_field::stage) == 3 && weapon != 0)
    {
        const Actor& lifted = context.weapon_at(weapon).actor;
        // Weapon +0x918: no bind node, an unbound node, or a node of no actor.
        if (lifted.link == nullptr || lifted.link->head == nullptr || lifted.link->actor == nullptr)
        {
            // 0x00146280, EAX the weapon, ECX 0: weapon message {0x97, 0}.
            if (!context.weapon_message)
            {
                throw Error("lift state has no weapon message view for 0x00146280");
            }
            const std::array<u32, 2> drop{0x97, 0};
            context.weapon_message(weapon, drop);
        }
    }
}

// Xbox 0x0009EA70
void face_runtime_lift_weapon(u32 weapon, RuntimePickupStateContext& context)
{
    const auto body = context.body;
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0, ESI actor
    Vec3 point;
    float radius = 0.f;
    i32 index = 0;
    // EBX the weapon, EDI the position, ESI the point; the result is not tested.
    if (!select_actor_spawn_point(context.weapon_at(weapon).actor, position, point, radius, index)) // 0x001476D0
    {
        throw Error("0x0009EA70 reads the pick-up point of a weapon without pick-up spheres");
    }
    const Vec3 direction{point.x - position.x, point.y - position.y, point.z - position.z};
    float angle = 0.f;
    if (runtime_direction_angle(direction, angle)) // 0x00105F90
    {
        put_float(body, 0x734, 0.f);
        put_float(body, 0x72C, 0.f);
        put_float(body, 0x730, angle);
    }
}

// Xbox 0x0009E6B0
void tick_runtime_lift_state(std::span<u8> state, RuntimePickupStateContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    set(state, body_state_field::elapsed,
        runtime_actor_step(actor, context.clock) + get(state, body_state_field::elapsed));
    put_float(body, 0x734, 0.f);
    put_float(body, 0x72C, 0.f);
    const u32 stage = get(state, lift_state_field::stage);
    u32 weapon = 0;
    if (stage == 1)
    {
        weapon = context.mind.empty() ? 0 : get(context.mind, mind_field::action_actor);
    }
    else if (stage == 3)
    {
        weapon = get(state, lift_state_field::weapon);
    }
    if (stage == 1)
    {
        face_runtime_lift_weapon(weapon, context); // 0x0009EA70
    }
    const MinaPlayer* player = first_player(context.animation);
    const bool crossed = player != nullptr
        && runtime_pickup_grab_crossed(*player, get(state, lift_state_field::ask_point));
    const float position = player == nullptr ? 0.f : player->position;
    const bool past_end = position >= get(state, lift_state_field::release_point);
    if (crossed)
    {
        set(state, lift_state_field::stage, 2);
        if (weapon != 0 && actor_can_supply_ghoulybox(context.weapon_at(weapon).actor)) // 0x00146150, ECX weapon
        {
            // {0xA0, reply, the lifting actor, the hold offset (0, actor +0x38 * state +0x24, 0), the lift time}.
            const float duration = player == nullptr ? 0.f : player->duration; // Player +0x20.
            std::array<u32, 7> ask{};
            ask[0] = 0xA0;
            ask[2] = read_u32(body, 8);
            ask[3] = std::bit_cast<u32>(0.f);
            ask[4] = std::bit_cast<u32>(actor.radius_xz * get(state, lift_state_field::hold_height)); // Actor +0x38.
            ask[5] = std::bit_cast<u32>(0.f);
            ask[6] = std::bit_cast<u32>((get(state, lift_state_field::hands_point)
                - get(state, lift_state_field::ask_point)) * duration);
            if (!context.weapon_message)
            {
                throw Error("lift state has no weapon message view for message 0xA0");
            }
            context.weapon_message(weapon, ask); // Weapon vtable +0xC; ask[1] is its reply.
            if (ask[1] != 0)
            {
                set(state, lift_state_field::weapon, weapon);
                set(state, lift_state_field::stage, 3);
                context.bits.bytes[5] |= 0x40;
                const u32 lift[]{0x48};
                context.actor_message(lift);
            }
        }
    }
    const bool asked = get(state, lift_state_field::stage) == 2;
    // EAX the tick's input word: a null word reads its kind from address zero, and a kind outside 1..5 gives no
    // stick in both 0x001085F0 and 0x001088D0.
    std::array<float, 2> axes{};
    bool pushed = false;
    if (context.input != nullptr)
    {
        if (context.input_state == nullptr)
        {
            throw Error("lift state stick check without the input state");
        }
        runtime_input_axes(*context.input, axes, *context.input_state); // 0x001085F0
        pushed = axes[1] * axes[1] + axes[0] * axes[0] > context.stick_threshold; // DAT_0032B22C.
        if (!pushed)
        {
            runtime_input_secondary_axes(*context.input, axes, *context.input_state); // 0x001088D0
            pushed = axes[1] * axes[1] + axes[0] * axes[0] > context.stick_threshold;
        }
    }
    else
    {
        const u32 kind = context.address_zero.empty() ? 0 : read_u32(context.address_zero, 0);
        if (kind - 1 <= 4)
        {
            throw Error("lift state input word is null but address zero holds an input kind");
        }
    }
    if (past_end && asked)
    {
        const u16 key = context.body_slot_key(pushed ? 0x2C : 0x20);
        set(state, body_state_field::next_key, key);
        return;
    }
    if (player != nullptr)
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
    set(state, body_state_field::next_key, key);
}

// Xbox 0x0009E9F0
void message_runtime_lift_state(std::span<u8> state, std::span<const u32> message,
    RuntimePickupStateContext& context)
{
    if (message[0] == 0x5C)
    {
        // The weapon reached the hands: attach it (0x0001B190 with 0) and count the holder.
        auto& weapon = context.weapon_at(message[1]);
        attach_runtime_player_weapon(weapon, 0, context.attachment, context.default_position,
            context.default_euler); // 0x0001B190, EAX the weapon
        const u16 count = u16(read_u16(weapon.object.bytes, 0xAE4) + 1);
        std::memcpy(weapon.object.bytes.data() + 0xAE4, &count, 2);
        context.bits.bytes[5] &= 0xBF;
        put_word(context.actor_words, 0x314, 0); // The weapon 0xA0 stored there.
        const u32 lifted[]{0x48};
        context.actor_message(lifted);
        return;
    }
    if (message[0] == 0x7C && message[1] == get(state, lift_state_field::weapon))
    {
        set(state, lift_state_field::weapon, 0);
    }
}
}
