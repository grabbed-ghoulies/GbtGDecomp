#include "player_construction.h"
#include <cstddef>
#include "body_fields.h"
#include "player_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00027F40
void runtime_player_storage_sizes(std::span<u32, 2> sizes)
{
    sizes[0] = 0xCF8;
    sizes[1] = 0x63C;
}
namespace
{
void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
void word(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}
void vector(std::span<u8> bytes, size_t offset, const Vec3& value)
{
    put(bytes, offset, std::bit_cast<u32>(value.x));
    put(bytes, offset + 4, std::bit_cast<u32>(value.y));
    put(bytes, offset + 8, std::bit_cast<u32>(value.z));
}
}

// Xbox 0x00028830
bool initialize_runtime_player(std::span<u8> actor, std::span<const u8> parameters,
    RuntimePlayerConstruction& context)
{
    const u16 mode = read_u16(parameters, 0x334);
    word(actor, offsetof(PlayerRecord, mode), mode);
    context.actor.mask_mode = i16(mode);
    word(actor, offsetof(PlayerRecord, owner_class), read_u16(parameters, 0x336));
    const u16 tag = read_u16(parameters, 0x338);
    word(actor, offsetof(PlayerRecord, tag), tag);
    if (tag != 0)
    {
        const u16 index = find_runtime_player_tag_index(tag, context.tag_rows, context.tag_names);
        word(actor, offsetof(PlayerRecord, damage_class), index);
        context.actor.damage_class = index; // Native alias of the actor +0xCF4 word.
        word(actor, offsetof(PlayerRecord, tag), tag);
    }
    player_record(actor).start_position = actor_world_position(context.actor);
    player_record(actor).start_euler = actor_world_euler(context.actor);
    word(actor, offsetof(PlayerRecord, goal_latch), read_u16(parameters, 0x63A));
    auto& random = context.attributes.random;
    random = random * 1664525u + 1013904223u;
    const float sampled = std::bit_cast<float>(0x3F800000u | (random & 0x7FFFFFu));
    put(actor, offsetof(PlayerRecord, goal_delay), std::bit_cast<u32>((sampled - 1.f) * 0.5f));
    initialize_runtime_player_attributes(actor, parameters, context.attributes);
    set_runtime_player_spawn_controller(context.spawn, parameters.data(), context.attributes.registry,
        context.attributes.error, random);
    auto& child = context.strategy_setup.child;
    set_runtime_player_body(context.body, reinterpret_cast<const char*>(parameters.data() + 0x43A), child);
    if (read_u32(parameters, 0x3BA) == 0x5F646961)
    {
        set_runtime_player_mind(context.mind, reinterpret_cast<const char*>(parameters.data() + 0x3BA), child);
        // 0x00028E50 stores the created mind at actor +0x958; its readers (the cheer state's target among them)
        // take a zero word for no mind.
        player_record(actor).mind = context.mind.bytes == nullptr ? 0u : context.address_of(context.mind.bytes.get());
    }
    context.input = create_runtime_actor_base_input(context.input_release);
    player_record(actor).input_device = context.address_of(context.input.data());
    if (read_u32(parameters, 0x33A) == 0x5F646961)
    {
        set_runtime_player_strategy(context.strategy, reinterpret_cast<const char*>(parameters.data() + 0x33A),
            context.strategy_setup);
    }
    const std::span<u8> body{context.body.bytes.get(), context.body.size};
    if (body_record(body).unk_FC != 0)
    {
        body_record(body).channel_keys = 2;
        body_record(body).channel_key = 0;
        body_record(body).unk_92 = 2;
        put(body, offsetof(BodyRecord, channel_timer), context.initial_channel_word);
        random = random * 1664525u + 1013904223u;
        put(body, offsetof(BodyRecord, channel_timer),
            std::bit_cast<u32>(std::bit_cast<float>(0x3F800000u | (random & 0x7FFFFFu)) - 1.f));
    }
    FactoryObject attached;
    if (read_u32(parameters, 0x4BA) == 0x5F646961)
    {
        attached = create_runtime_player_attachment(reinterpret_cast<const char*>(parameters.data() + 0x4BA),
            0, context.attachments);
    }
    if (player_record(actor).mode == 1)
    {
        ++context.player_count;
        if (attached.bytes != nullptr && (context.story_flags & 0x10) != 0)
        {
            auto& attached_actor = context.attachments.view_of(attached);
            put(attached_actor.object.bytes, 0xDA0, 1);
        }
    }
    actor[0x298] = 1;
    refresh_actor_model_matrices(context.actor, context.matrices, 0, context.matrix_boundaries);
    context.created_attachment = std::move(attached);
    return true;
}
}
