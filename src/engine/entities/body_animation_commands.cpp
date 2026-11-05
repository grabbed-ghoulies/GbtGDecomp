#include "body_animation_commands.h"
#include "weapon_fields.h"
#include <cstddef>
#include "actor_model_anchors.h"
#include "../runtime/name_lookup.h"
#include "actor_render_registration.h"
#include "player_mind_events.h"
#include "gbtg/sound_runtime.h"
#include "body_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x0003B1D0
float runtime_actor_animation_duration(const ActorAnimationState& animation, u32 slot)
{
    if (animation.controller != nullptr)
    {
        auto* player = animation.controller->slots[slot].player.get();
        if (player != nullptr)
        {
            return player->duration;
        }
    }
    return 0.f;
}

// Xbox 0x0001AC50
void command_runtime_body_breath(Actor& actor, const EntityBitBlock& flags,
    const ActorAnimationState& animation, u32 phase, u32 slot,
    std::span<const u8> command, u32 effect_flags, ActorEffectStreamContext& effects)
{
    if (phase == 0 && ((effect_flags & 1) != 0 || (flags.bytes[1] & 0x40) != 0))
    {
        const u8* record = find_actor_model_binding(actor, 5);
        if (record != nullptr)
        {
            g_actor_effect_binding.mode = 1;
            g_actor_effect_binding.actor = &actor;
            g_actor_effect_binding.index = (g_actor_effect_binding.index & ~0xFFFF) | 5;
            ModelBoneBinding binding;
            std::memcpy(&binding.kind, record, 2);
            std::memcpy(&binding.joint, record + 2, 2);
            std::memcpy(binding.matrix.data(), record + 4, 48);
            Vec3 position;
            Vec3 euler;
            actor_binding_world_position(actor, binding, position);
            actor_binding_world_euler(actor, binding, euler);
            g_actor_creation_options.position = position;
            g_actor_creation_options.euler = euler;
            g_actor_creation_options.user = &g_actor_effect_binding;
            g_actor_creation_options.flags = 0xD;
            if (load_actor_effect_stream("aid_fxemitter_ghoulies_actor_common_breath", &g_actor_creation_options, effects) != 0)
            {
                const i16 index = i16(g_last_actor_effect);
                g_last_actor_effect = -1;
                if (index != -1)
                {
                    auto& effect = *g_actor_effect_nodes[index];
                    effect.bytes[0x18] |= 8;
                    if (read_f32(command, 8) != 0.f)
                    {
                        const float duration = runtime_actor_animation_duration(animation, slot);
                        const float value = duration * (read_f32(command, 8) - read_f32(command, 4));
                        std::memcpy(effect.bytes.data() + 0x10, &value, 4);
                    }
                }
            }
        }
    }
}

// Xbox 0x00122D80
void broadcast_runtime_actor_proximity(RuntimeActorProximityNode* first,
    std::span<const u32> message, const Vec3& position, float radius)
{
    const float squared_radius = radius * radius;
    auto* node = first;
    while (node != nullptr)
    {
        auto* current = node;
        node = node->next;
        const Vec3 point = actor_world_position(current->actor);
        const float z = point.z - position.z;
        const float x = point.x - position.x;
        const float y = point.y - position.y;
        const float squared_distance = (z * z + x * x) + y * y;
        if (squared_radius > squared_distance)
        {
            current->message(message);
        }
    }
}

// Xbox 0x0001ADB0
void command_runtime_body_proximity(Actor& actor, std::span<const u8> command,
    RuntimeActorProximityNode* first)
{
    const Vec3 position = actor_world_position(actor);
    const std::array<u32, 4> message{0x37, std::bit_cast<u32>(position.x),
        std::bit_cast<u32>(position.y), std::bit_cast<u32>(position.z)};
    broadcast_runtime_actor_proximity(first, message, position, read_f32(command, 0x10));
}

// Xbox 0x0001B5D0
u32 teleport_runtime_body_secondary(std::span<u8> body, RuntimeBodyAttachedActor* primary,
    RuntimeBodyAttachedActor*& secondary, RuntimeBodyAnimationReceiver*& receiver,
    const Vec3& position)
{
    const bool same = secondary == primary;
    if (same)
    {
        if (weapon_base(secondary->bytes).unk_93D == 3)
        {
            --body_record(body).unk_30B;
        }
    }
    auto* attached = detach_runtime_body_secondary(body, secondary, receiver);
    const std::array<u32, 4> message{0xA5, std::bit_cast<u32>(position.x),
        std::bit_cast<u32>(position.y), std::bit_cast<u32>(position.z)};
    attached->message(message);
    if (same)
    {
        const std::array<u32, 3> notice{0x73, read_u32(body, 8), attached->address};
        broadcast_actor_pulse_message(reinterpret_cast<const i32*>(notice.data()));
    }
    return 1;
}

// Xbox 0x0001A750
void command_runtime_body_teleport(std::span<u8> body, Actor& actor,
    std::span<const u8> command, RuntimeBodyAttachedActor* primary,
    RuntimeBodyAttachedActor*& secondary, RuntimeBodyAnimationReceiver*& receiver)
{
    const Vec3 position = actor_world_position(actor);
    Vec3 offset{0.f, 0.f, 0.f};
    add_sound_direction(offset, read_f32(command, 0x10), actor.euler.y, read_f32(command, 0x14));
    teleport_runtime_body_secondary(body, primary, secondary, receiver,
        Vec3{position.x + offset.x, position.y + offset.y, position.z + offset.z});
}

// Xbox 0x00041A30
float runtime_actor_relative_target_angle(const Actor& actor, const Vec3& target_position)
{
    const Vec3 position = actor_world_position(actor);
    const Vec3 direction{target_position.x - position.x, target_position.y - position.y,
        target_position.z - position.z};
    float angle;
    runtime_direction_angle(direction, angle);
    const float difference = float(static_cast<long double>(angle) - static_cast<long double>(actor.euler.y));
    const long double remainder = animation_remainder(difference, std::bit_cast<float>(0x40C90FDBu));
    float wrapped = float(remainder);
    if (remainder < static_cast<long double>(std::bit_cast<float>(0xC0490FDBu)))
    {
        wrapped += std::bit_cast<float>(0x40C90FDBu);
    }
    if (wrapped >= std::bit_cast<float>(0x40490FDBu))
    {
        wrapped -= std::bit_cast<float>(0x40C90FDBu);
    }
    return wrapped;
}

// Xbox 0x000199F0
void command_runtime_body_target_push(Actor& actor, const EntityBitBlock& flags,
    const RuntimeBodyCommandTarget* mind, std::span<const u8> command)
{
    if ((flags.bytes[7] & 0x20) == 0)
    {
        float angle = 0.f;
        if (mind != nullptr && mind->position != nullptr)
        {
            angle = runtime_actor_relative_target_angle(actor, *mind->position);
        }
        add_runtime_body_direction_push(actor, std::bit_cast<i32>(read_u32(command, 0x10)),
            actor.euler.y + angle);
    }
}

// Xbox 0x0013B8B0
void record_runtime_body_attachment_event(const u8* record,
    const RuntimeActorCreationOptions* options, ActorAttachmentEventContext& context, u32 value)
{
    auto output = context.command_attachments;
    if (read_u32(output, 0) != 8)
    {
        const auto bytes = std::span<const u8>(record, 0xD4);
        const u16 kind = read_u16(bytes, 4);
        u16 token;
        if (kind == 4)
        {
            const i16 joint = read_u32(bytes, 0xC8) == 0 ? -1 : i16(read_u16(bytes, 0xCC));
            token = request_actor_joint_attachment(context.actor, context.attachments,
                reinterpret_cast<const char*>(record + 0x48), read_u32(bytes, 0xD0),
                joint, options, context.effects);
        }
        else if (kind == 5)
        {
            token = request_actor_binding_attachment(context.actor, context.attachments,
                reinterpret_cast<const char*>(record + 0x48), read_u32(bytes, 0xCC),
                i16(read_u16(bytes, 0xC8)), options, context.effects);
        }
        else
        {
            if (kind != 6)
            {
                dispatch_actor_attachment_event(record, options, context, value);
                return;
            }
            token = request_attachment_index_event(record, options, context);
        }
        if (token != 0)
        {
            u32 count = read_u32(output, 0);
            const u32 stored = token;
            std::memcpy(output.data() + size_t(count + 1) * 4, &stored, 4);
            ++count;
            std::memcpy(output.data(), &count, 4);
        }
    }
}

// Xbox 0x0013B7E0
void request_runtime_player_attachment_events(const u8* record,
    ActorAttachmentEventContext& context, i32 id, u32 output_address)
{
    if (context.actor_address != 0xFFFFFF74u)
    {
        dispatch_actor_attachment_records(record, id, record_runtime_body_attachment_event,
            nullptr, context, output_address);
    }
}

// Xbox 0x0013B810
void clear_runtime_player_attachment_events(std::span<u8> events,
    RuntimeBodyAttachmentLists& lists)
{
    i32 index = 0;
    while (index < std::bit_cast<i32>(read_u32(events, 0)))
    {
        remove_runtime_body_attachment(u16(read_u32(events, size_t(index + 1) * 4)), lists);
        ++index;
    }
    const u32 zero = 0;
    std::memcpy(events.data(), &zero, 4);
}

// Xbox 0x0001AE00
void command_runtime_body_attachment_list(std::span<u8> body, u32 body_address,
    bool clear, const u8* record, ActorAttachmentEventContext& events,
    RuntimeBodyAttachmentLists& lists)
{
    if (clear)
    {
        clear_runtime_body_command_attachments(body, lists);
        return;
    }
    body_record(body).unk_2D2 = 1;
    if (events.actor_address != 0xFFFFFF74u)
    {
        events.command_attachments = body.subspan(offsetof(BodyRecord, command_tokens), 36);
        dispatch_actor_attachment_records(record, 0x40, record_runtime_body_attachment_event,
            nullptr, events, body_address + 0x2D8);
    }
}

// Xbox 0x00019A50
void command_runtime_body_facing_push(Actor& actor, const EntityBitBlock& flags,
    std::span<const u8> command)
{
    if ((flags.bytes[7] & 0x20) == 0)
    {
        const float angle = read_f32(command, 0x14) * std::bit_cast<float>(0x3C8EFA35u) + actor.euler.y;
        add_runtime_body_direction_push(actor, std::bit_cast<i32>(read_u32(command, 0x10)), angle);
    }
}

// Xbox 0x0001A690
void command_runtime_body_detach_actors(std::span<u8> body,
    std::span<const u8> command, RuntimeBodyActorAttachment& primary,
    RuntimeBodyAttachedActor*& secondary, RuntimeBodyAnimationReceiver*& receiver)
{
    if (read_u32(command, 0x10) != 0 && primary.attached != nullptr)
    {
        RuntimeBodyAttachedActor* attached = detach_runtime_body_actor(body, primary);
        activate_actor_bind_tree(*attached->actor);
    }
    if (read_u32(command, 0x14) != 0 && secondary != nullptr)
    {
        RuntimeBodyAttachedActor* attached = detach_runtime_body_secondary(body, secondary, receiver);
        activate_actor_bind_tree(*attached->actor);
    }
}

// Xbox 0x0001A720
void command_runtime_body_attachment_event(RuntimeBodyCommandCallout* attached)
{
    if (attached != nullptr && attached->actor_address != 0xFFFFFF74u)
    {
        dispatch_actor_attachment_records(attached->record, 0x55, nullptr, nullptr,
            attached->events, 0);
    }
}

namespace
{
void command_attachment_callout(RuntimeBodyCommandCallout* attached,
    const char* name, std::span<const char* const, 0x55> names)
{
    if (attached != nullptr)
    {
        const char* end = name;
        do
        {
            ++end;
        } while (end[-1] != '\0');
        i32 id;
        if (end == name + 1)
        {
            id = 0;
        }
        else
        {
            id = binary_search_names(0x55, names.data(), name);
            if (id == -1)
            {
                id = 0;
            }
            else
            {
                ++id;
            }
        }
        if (attached->actor_address != 0xFFFFFF74u)
        {
            dispatch_actor_attachment_records(attached->record, id, nullptr, nullptr,
                attached->events, 0);
        }
    }
}
}

// Xbox 0x0001A810
void command_runtime_body_primary_callout(RuntimeBodyCommandCallout* attached,
    const char* name, std::span<const char* const, 0x55> names)
{
    command_attachment_callout(attached, name, names);
}

// Xbox 0x0001A880
void command_runtime_body_secondary_callout(RuntimeBodyCommandCallout* attached,
    const char* name, std::span<const char* const, 0x55> names)
{
    command_attachment_callout(attached, name, names);
}

// Xbox 0x0001EF70
u32 runtime_body_flag_name_index(const char* name, std::span<const char* const, 0x41> names)
{
    u32 index = 0;
    do
    {
        const char* current = name;
        const char* entry = names[index];
        u8 first, second;
        while (true)
        {
            first = u8(*current);
            if (first > 0x20)
            {
                first &= 0xDF;
            }
            second = u8(*entry);
            if (second > 0x20)
            {
                second &= 0xDF;
            }
            if (first != second)
            {
                break;
            }
            ++current;
            ++entry;
            if (first == 0)
            {
                return index;
            }
        }
        if (first == second)
        {
            return index;
        }
        ++index;
    } while (index < 0x41);
    return 0;
}

// Xbox 0x00144E90
void activate_runtime_attached_contact_shapes(RuntimeBodyCommandAttachment& attached,
    u16 tag, u8 flags, float scale)
{
    if (attached.collision != nullptr)
    {
        activate_runtime_body_contact_shapes(*attached.collision, *attached.actor.actor, tag, flags, scale);
        const std::array<u32, 2> message{0x92, 1};
        attached.actor.message(message);
    }
}

// Xbox 0x00144EE0
void deactivate_runtime_attached_contact_shapes(RuntimeBodyCommandAttachment& attached, u16 tag)
{
    if (attached.collision != nullptr)
    {
        deactivate_runtime_body_contact_shapes(*attached.collision, tag);
        const std::array<u32, 2> message{0x92, 0};
        attached.actor.message(message);
    }
}

// Xbox 0x0001A290
void command_runtime_attached_contact_shapes(RuntimeBodyCommandAttachment* attached,
    std::span<const u8> command, u32 phase)
{
    if (attached != nullptr)
    {
        if (phase == 0)
        {
            activate_runtime_attached_contact_shapes(*attached, read_u16(command, 0x10),
                u8(read_u16(command, 0x12)), read_f32(command, 0x14));
            return;
        }
        deactivate_runtime_attached_contact_shapes(*attached, read_u16(command, 0x10));
    }
}

// Xbox 0x0001AAF0
void command_runtime_body_contact_shapes(std::span<u8> body, Actor& actor,
    RuntimeBodyCollisionState* state, std::span<const u8> command, u32 phase)
{
    if (phase == 0)
    {
        if (state != nullptr)
        {
            activate_runtime_body_contact_shapes(*state, actor, read_u16(command, 0x10),
                u8(read_u16(command, 0x12)), read_f32(command, 0x14));
            const u32 zero = 0;
            body_record(body).contact_count = zero;
        }
    }
    else if (state != nullptr)
    {
        deactivate_runtime_body_contact_shapes(*state, read_u16(command, 0x10));
    }
}

// Xbox 0x0001AB50
void command_runtime_body_side_counters(std::span<u8> body, u32 mask, u32 phase)
{
    if (phase == 0)
    {
        if ((mask & 1) != 0)
        {
            ++body_record(body).unk_309;
        }
        if ((mask & 2) != 0)
        {
            ++body_record(body).unk_30A;
        }
        if ((mask & 4) != 0)
        {
            ++body_record(body).unk_30B;
        }
        if ((mask & 8) != 0)
        {
            ++body_record(body).unk_30C;
            return;
        }
    }
    else
    {
        if ((mask & 1) != 0)
        {
            --body_record(body).unk_309;
        }
        if ((mask & 2) != 0)
        {
            --body_record(body).unk_30A;
        }
        if ((mask & 4) != 0)
        {
            --body_record(body).unk_30B;
        }
        if ((mask & 8) != 0)
        {
            --body_record(body).unk_30C;
        }
    }
}

// Xbox 0x0001ABC0
void command_runtime_body_latch_mask(std::span<u8> body, Actor& actor,
    entity_detail::LatchNode*& first, u8 mask, u32 phase)
{
    if (phase == 0)
    {
        increment_runtime_body_latch_mask(body, mask, actor, first);
        return;
    }
    decrement_runtime_body_latch_mask(body, mask, actor, first);
}

// Xbox 0x0001ABF0
void command_runtime_body_flag(EntityBitBlock& flags, const char* name,
    std::span<const char* const, 0x41> names, u32 phase)
{
    const u32 index = runtime_body_flag_name_index(name, names);
    if (phase == 0)
    {
        set_runtime_body_bit(flags, index);
        return;
    }
    clear_runtime_body_bit(flags, index);
}

// Xbox 0x0001A7E0
void command_runtime_body_release_actor(std::span<u8> body, RuntimeBodyActorAttachment& attachment)
{
    if (attachment.attached != nullptr)
    {
        RuntimeBodyAttachedActor* actor = detach_runtime_body_actor(body, attachment);
        const std::array<u32, 1> message{0x9B};
        actor->message(message);
    }
}

// Xbox 0x000191C0
float runtime_body_random_range(u32& random, float low, float high)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return (high - low) * (std::bit_cast<float>((random & 0x7FFFFF) | 0x3F800000u) - 1.f) + low;
}

// Xbox 0x0001AE50
void command_runtime_body_random_timer(std::span<u8> body, float low, float high, u32& random)
{
    const float value = runtime_body_random_range(random, low, high);
    if (body_record(body).move_hold_time <= value)
    {
        body_record(body).move_hold_time = value;
        return;
    }
    const u32 retained = read_u32(body, offsetof(BodyRecord, move_hold_time));
    std::memcpy(body.data() + offsetof(BodyRecord, move_hold_time), &retained, 4);
}

// Xbox 0x00019A90
void add_runtime_body_direction_push(Actor& actor, i32 speed, float angle)
{
    if (speed == 0)
    {
        speed = 40;
    }
    const float amount = float(speed);
    const Vec3 anchor = actor.anchor;
    const long double sine = retail_sin(angle);
    const long double cosine = retail_cos(angle);
    actor.anchor.x = anchor.x + float(sine * static_cast<long double>(amount));
    actor.anchor.y = anchor.y;
    actor.anchor.z = float(cosine * static_cast<long double>(amount)) + anchor.z;
}
}
