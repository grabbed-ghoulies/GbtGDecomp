#include "player_messages.h"
#include <cstddef>
#include "actor_transform.h"
#include "actor_live_attachments.h"
#include "body_fields.h"
#include "player_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
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

// Shared 0xA7/0xA9/0xAA gate: a name already starting with "aid_", or a busy +0xB9E word, rejects.
bool spawn_slot_available(std::span<const u8> actor)
{
    return read_u32(actor, offsetof(PlayerRecord, ghouly_box)) != 0x5F646961 && player_record(actor).ghouly_count == 0;
}

void copy_string(std::span<u8> actor, size_t offset, const char* text)
{
    size_t index = 0;
    char value;
    do
    {
        value = text[index];
        actor[offset + index] = u8(value);
        ++index;
    } while (value != '\0');
}

// Inline insertion used for DAT_00511B34, DAT_00511B44 and DAT_00511B4C: follow word 0
// to its end, then insert after that node.
entity_detail::ListHead* insert_after_first(entity_detail::ListHead& head, entity_detail::ListHead& node)
{
    entity_detail::ListHead* at = &head;
    if (head.prev != nullptr)
    {
        do
        {
            at = static_cast<entity_detail::ListHead*>(at->prev);
        } while (at->prev != nullptr);
    }
    auto* following = static_cast<entity_detail::ListHead*>(at->next);
    at->next = &node;
    node.prev = at;
    node.next = following;
    if (following != nullptr)
    {
        following->prev = &node;
    }
    return at;
}

u16 first_goal_kind(const RuntimePlayerGoalRing* ring, std::span<const u8> address_zero)
{
    if (ring->tail == ring->capacity)
    {
        return read_u16(address_zero, 0);
    }
    return read_u16(ring->rows, size_t(ring->stride) * ring->head);
}

bool player_or_mode_four(std::span<const u8> actor)
{
    const i16 mode = i16(player_record(actor).mode);
    return mode == 1 || mode == 4;
}
}

// Xbox 0x00019150
u32 fade_runtime_player_animation_slot(AnimationController* controller, const ActorAnimationMessage& message)
{
    if (read_u16(message.event, 0xC) != 5)
    {
        return 0;
    }
    const u32 slot = message.slot;
    float duration;
    if (controller == nullptr || controller->slots[slot].player == nullptr)
    {
        duration = 0.0f;
    }
    else
    {
        duration = controller->slots[slot].player->duration;
    }
    if (controller != nullptr && slot != 0)
    {
        auto& entry = controller->slots[slot];
        if (entry.player != nullptr)
        {
            entry.fade_duration = (1.0f - message.position) * duration;
            entry.fade_mode = 3;
        }
        controller->dirty = true;
    }
    return 0;
}

// Xbox 0x00019660
void reset_runtime_player_attribute_timer(RuntimeBodyAttachedActor& actor, u8 value)
{
    player_record(actor.bytes).unk_96D = value;
    player_record(actor.bytes).unk_96F = 0;
    player_record(actor.bytes).unk_970 = 0;
    if (player_record(actor.bytes).countdown_55 > 0.0f)
    {
        put(actor.bytes, offsetof(PlayerRecord, countdown_55), 0);
        // Retail reuses its argument slot as the message, writing only the id.
        const std::array<u32, 1> message{0x55};
        actor.message(message);
    }
}

// Xbox 0x000196D0
u32 runtime_player_attribute_word_28(std::span<const u8> attributes)
{
    if (attributes.data() != nullptr)
    {
        return read_u32(attributes, 0x28);
    }
    return 0;
}

// Xbox 0x000294F0
void relay_runtime_player_contact(const RuntimePlayerContactMessage& message, u32 actor_address)
{
    std::array<u32, 9> reply{};
    reply[0] = 0xF;
    reply[1] = actor_address;
    for (size_t index = 0; index < 6; ++index)
    {
        reply[2 + index] = message.words[index];
    }
    reply[8] = message.extra; // The upper half of the +0x20 word is zero.
    message.sender->message(reply);
}

// Xbox 0x00029590
void acknowledge_runtime_player_contact(RuntimeBodyAttachedActor& actor, const RuntimePlayerContactMessage& message)
{
    std::array<u32, 3> notice{0x2D, actor.address, message.sender->address};
    actor.message(notice);
    // The same stack record is reused with its owner and sender words swapped.
    notice[1] = message.sender->address;
    notice[2] = actor.address;
    message.sender->message(notice);
}

// Xbox 0x00028700
void accept_runtime_player_spawn(std::span<u8> actor, GhoulySpawnMessage& message, RuntimePlayerSpawnController& spawn)
{
    if (spawn_slot_available(actor))
    {
        copy_string(actor, offsetof(PlayerRecord, ghouly_box), message.name);
        copy_string(actor, offsetof(PlayerRecord, ghouly_attributes), message.attributes);
        spawn.active = actor[offsetof(PlayerRecord, ghouly_box)]; // Native alias of the first name byte.
        player_record(actor).ghouly_kind = 0;
        message.result = 1;
        return;
    }
    message.result = -1;
}

// Xbox 0x00028770
void accept_runtime_player_variant(std::span<u8> actor, GhoulyVariantMessage& message, RuntimePlayerSpawnController& spawn)
{
    if (spawn_slot_available(actor))
    {
        copy_string(actor, offsetof(PlayerRecord, ghouly_box), message.name);
        spawn.active = actor[offsetof(PlayerRecord, ghouly_box)];
        actor[offsetof(PlayerRecord, ghouly_attributes)] = 0;
        player_record(actor).ghouly_kind = 1;
        message.result = 1;
        return;
    }
    message.result = -1;
}

// Xbox 0x000287C0
void accept_runtime_player_alternate_variant(std::span<u8> actor, GhoulyVariantMessage& message,
    RuntimePlayerSpawnController& spawn)
{
    if (spawn_slot_available(actor))
    {
        copy_string(actor, offsetof(PlayerRecord, ghouly_box), message.name);
        spawn.active = actor[offsetof(PlayerRecord, ghouly_box)];
        actor[offsetof(PlayerRecord, ghouly_attributes)] = 0;
        player_record(actor).ghouly_kind = 2;
        message.result = 1;
        return;
    }
    message.result = -1;
}

// Xbox 0x00028D50
u8 runtime_player_can_resume_strategy(std::span<const u8> actor, const RuntimePlayerStrategyResume& context)
{
    const size_t group = (i16(player_record(actor).mode) == 2 || player_record(actor).case_link != 0) ? 1 : 0;
    if (context.references[group] != 0)
    {
        return 0;
    }
    return context.decay_enabled[context.decay_index];
}

// Xbox 0x0002B7F0
u8 runtime_player_strategy_decay_enabled(std::span<const u8> actor,
    const RuntimePlayerStrategyResume& context)
{
    const size_t group = (player_record(actor).mode == 2 || player_record(actor).case_link != 0) ? 1 : 0;
    if (context.references[group] != 0)
    {
        return 0;
    }
    return context.decay_enabled[context.decay_index];
}

// Xbox 0x0002B8D0
void change_runtime_player_strategy_state(FactoryObject& strategy, u16 state, RuntimePlayerStrategySetup& context)
{
    std::span<u8> bytes{strategy.bytes.get(), strategy.size};
    context.leaving(strategy, read_u16(bytes, 0xE), state);
    const u16 previous = read_u16(bytes, 0xE);
    put_u16(bytes, 0xE, state);
    clear_runtime_player_mind_requests(*context.mind);
    context.entering(strategy, state, previous);
}

// Xbox 0x00028210
void drain_runtime_player_goals(RuntimePlayerGoalCleanup& goals)
{
    while (goals.ring != nullptr)
    {
        remove_runtime_player_goal(goals);
    }
}

// Xbox 0x00027FB0
u32 handle_runtime_player_pause_message(std::span<u8> actor, u32 id, RuntimePlayerPause& context)
{
    switch (id)
    {
    case 9:
    {
        const i32 count = i32(player_record(actor).pause_count) + 1;
        player_record(actor).pause_count = u32(count);
        context.actor.parked_gate = count;
        if (count == 1)
        {
            if (context.strategy.bytes != nullptr)
            {
                change_runtime_player_strategy_state(context.strategy, 0x3E8, context.setup);
            }
            if (script_actor_current_mask(context.actor, context.query) != 0)
            {
                drain_runtime_player_goals(context.goals);
                return 1;
            }
        }
        break;
    }
    case 10:
    {
        const i32 count = i32(player_record(actor).pause_count) - 1;
        player_record(actor).pause_count = u32(count);
        context.actor.parked_gate = count;
        if (count == 0 && context.strategy.bytes != nullptr && runtime_player_can_resume_strategy(actor, context.resume) != 0)
        {
            change_runtime_player_strategy_state(context.strategy,
                read_u16({context.strategy.bytes.get(), context.strategy.size}, 0xC), context.setup);
        }
        break;
    }
    case 0xB:
    case 0xC:
    case 0xD:
        break;
    default:
        return 0;
    }
    return 1;
}

// Xbox 0x00029490
void register_runtime_player_navigation(std::span<const u8> actor, RuntimePlayerNavigationRegistration& context)
{
    const float radius = context.body_owner.radius_xz * body_record(context.body).radius_scale;
    if (radius != 0.0f)
    {
        const Vec3 center = player_record(actor).start_position;
        entity_detail::register_navigation_cells(context.records, center, i32(player_record(actor).floor), radius,
            i32(player_record(actor).outer_cells));
    }
}

// Xbox 0x00029270
void activate_runtime_player(RuntimePlayerActivation& context)
{
    auto actor = context.self.bytes;
    insert_after_first(context.world.mark, context.links[0]);
    const i16 mode = i16(player_record(actor).mode);
    if (mode == 1)
    {
        insert_after_first(context.world.alt, context.links[2]);
    }
    else if (mode == 2)
    {
        insert_after_first(context.world.slot, context.links[3]);
        entity_detail::ListHead* tail = &context.world.case_head;
        while (tail->next != nullptr)
        {
            tail = static_cast<entity_detail::ListHead*>(tail->next);
        }
        tail->next = &context.links[1];
        context.links[1].prev = tail;
        context.links[1].next = nullptr;
        // Raw +0x93C word and its native alias hold the predecessor's retail address.
        const u32 previous = context.address_of(tail);
        player_record(actor).case_link = previous;
        context.actor.mask_flag = i32(previous);
    }
    const Vec3 position = actor_world_position(context.actor);
    player_record(actor).start_position = position;
    context.actor.parked_pos = position;
    const Vec3 euler = actor_world_euler(context.actor);
    player_record(actor).start_euler = euler;
    context.actor.parked_euler = euler;
    if (read_u16(actor, 0x1FC) != 0 && player_or_mode_four(actor))
    {
        const Vec3 point = actor_world_position(context.actor);
        const OccupancyRecord* record = find_occupancy_record(point, 0.0f, 2, nullptr, 0);
        if (record != nullptr)
        {
            context.room = record;
        }
    }
    if (player_or_mode_four(actor))
    {
        const Vec3 point = actor_world_position(context.actor);
        const OccupancyRecord* record = find_occupancy_record(point, 0.0f, 4, nullptr, i16(read_u16(actor, 6)));
        if (record != nullptr)
        {
            context.allowed = record;
        }
    }
    const u16 tag = read_u16(actor, 6);
    for (i32 index = 0; index < context.excluded_count; ++index)
    {
        if (tag == context.excluded_tags[index])
        {
            // Inline copy of 0x00019660 with value 1.
            player_record(actor).unk_96D = 1;
            player_record(actor).unk_96F = 0;
            player_record(actor).unk_970 = 0;
            if (player_record(actor).countdown_55 > 0.0f)
            {
                put(actor, offsetof(PlayerRecord, countdown_55), 0);
                const std::array<u32, 1> message{0x55};
                context.self.message(message);
            }
            break;
        }
    }
    if (context.ring != nullptr && first_goal_kind(context.ring, context.address_zero) != 0)
    {
        player_record(actor).unk_98B = 1;
    }
    else
    {
        restore_runtime_player_script_overrides(context.self, context.overrides);
    }
    const Vec3 point = actor_world_position(context.actor);
    player_record(actor).floor = u32(entity_detail::navigation_floor_index(point, -1, 0));
}

// Xbox 0x00028FC0
void handle_runtime_player_message(void* message, RuntimePlayerMessageContext& context)
{
    auto& self = context.self;
    // Message 7 arrives as the 0x0003B3C0 words (+4 position, +8 slot, +0xC event, +0x10 phase); the host
    // handlers read the decoded form.
    const auto* words = static_cast<const u32*>(message);
    ActorAnimationMessage animation{};
    if (words[0] == 7)
    {
        animation = ActorAnimationMessage{7, std::bit_cast<float>(words[1]), words[2],
            words[3] == 0 ? std::span<const u8>{} : context.event_at(words[3]), i32(words[4])};
    }
    switch (words[0])
    {
    case 7:
        fade_runtime_player_animation_slot(context.animation, animation);
        break;
    case 0xE:
        relay_runtime_player_contact(*static_cast<const RuntimePlayerContactMessage*>(message), self.address);
        break;
    case 0xF:
        acknowledge_runtime_player_contact(self, *static_cast<const RuntimePlayerContactMessage*>(message));
        break;
    case 0x23:
    {
        activate_runtime_player(context.activation);
        const std::array<i32, 2> notice{0x71, i32(self.address)};
        broadcast_actor_pulse_message(notice.data());
        break;
    }
    case 0x2D:
        context.body_bits->bytes[0] |= 2;
        break;
    case 0x6E:
    case 0x70:
    {
        const auto& owned = *static_cast<const RuntimeOwnedMessage*>(message);
        if (owned.owner == self.address && owned.present != 0 && runtime_player_attribute_word_28(context.attributes) != 0)
        {
            reset_runtime_player_attribute_timer(self, 0);
        }
        break;
    }
    case 0x7C:
        if (static_cast<const RuntimeOwnedMessage*>(message)->owner == self.address)
        {
            const std::array<i32, 2> notice{0x76, i32(self.address)};
            broadcast_actor_pulse_message(notice.data());
        }
        break;
    case 0x7D:
        register_runtime_player_navigation(self.bytes, context.navigation);
        break;
    case 0xA7:
        accept_runtime_player_spawn(self.bytes, *static_cast<GhoulySpawnMessage*>(message), context.spawn);
        break;
    case 0xA9:
        accept_runtime_player_variant(self.bytes, *static_cast<GhoulyVariantMessage*>(message), context.spawn);
        break;
    case 0xAA:
        accept_runtime_player_alternate_variant(self.bytes, *static_cast<GhoulyVariantMessage*>(message), context.spawn);
        break;
    default:
        handle_runtime_player_pause_message(self.bytes, *static_cast<const u32*>(message), context.pause);
        break;
    }
    if (words[0] == 7)
    {
        dispatch_base_actor_message(&animation, context.base);
        return;
    }
    dispatch_base_actor_message(message, context.base);
}

// Xbox 0x00028F60
void dispatch_runtime_player_message(void* message, RuntimePlayerMessageContext& context, RuntimePlayerChildMessages& children)
{
    handle_runtime_player_message(message, context);
    for (FactoryObject* child : {&children.strategy, &children.mind, &children.body})
    {
        if (child->bytes != nullptr)
        {
            children.message(*child, message);
        }
    }
}
}
