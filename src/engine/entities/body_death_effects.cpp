#include "body_death_effects.h"
#include "actor_trail_requests.h"
#include "actor_transform.h"
#include "body_damage_notices.h"
#include "../scripts/script_conditions.h"
#include "../scene/scene.h"
#include "actor_bind_tree.h"
#include "actor_render_registration.h"
#include "player_fields.h"
#include <array>
#include <bit>
#include <cstring>
#include "body_fields.h"

namespace gbtg
{
// Xbox 0x0013B1F0, ESI name
void burst_runtime_actor_into_bits(const Actor& actor, u32 actor_address, float lifetime, const char* name,
    const char* default_name, const std::function<void(std::span<const u8>)>& create)
{
    std::array<u8, 0x55C> parameters{};
    initialize_runtime_body_death_effect(parameters, actor, actor_address, std::bit_cast<float>(0xBF800000u),
        lifetime, default_name); // 0x00068770, EBX the parameters
    if (name != nullptr)
    {
        char* output = reinterpret_cast<char*>(parameters.data() + 0x3B8);
        char character;
        do
        {
            character = *name++;
            *output++ = character;
        } while (character != 0);
    }
    create(parameters); // 0x00107520
}

namespace
{
void copy_death_effect_name(char* destination, const char* source)
{
    char character;
    do
    {
        character = *source++;
        *destination++ = character;
    } while (character != '\0');
}
}

// Xbox 0x00134AE0
void finish_runtime_body_death(u32 create_effect, u32& scene_flags,
    u32& restart_requested, RuntimeBodyDeathEffects& context)
{
    notify_runtime_body_death_classes(context.notices);
    auto& drops = context.drops;
    if (context.attributes.data() != nullptr && context.attributes[0x19A] != 0)
    {
        return;
    }
    if (player_record(drops.actor_bytes).unk_CE4 != 0)
    {
        std::array<u8, 0x14> message{};
        const u32 type = 0xB5;
        std::memcpy(message.data(), &type, 4);
        std::memcpy(message.data() + 4, &context.notices.actor_address, 4);
        const Vec3 position = actor_world_position(drops.actor);
        std::memcpy(message.data() + 8, &position, 12);
        script_queue_event(context.notices.game, message);
    }
    if (context.live != 0 && (player_record(drops.actor_bytes).mode == 2 ||
        player_record(drops.actor_bytes).case_link != 0))
    {
        if ((scene_flags & 0x8000) == 0)
        {
            request_scene_aid(reinterpret_cast<const char*>(g_scene_record), 1);
            restart_requested = 1;
        }
        return;
    }
    const char* name;
    if (context.attributes.data() == nullptr)
    {
        name = nullptr;
    }
    else
    {
        name = reinterpret_cast<const char*>(context.attributes.data() + 0x110);
    }
    if (create_effect != 0 && name != nullptr)
    {
        u32 prefix;
        std::memcpy(&prefix, name, 4);
        if (prefix == 0x5F646961)
        {
            auto clone = clone_executable_resource(drops.resources, name);
            if (clone.bytes == nullptr)
            {
                return;
            }
            release_runtime_player_attachment(1, 0, context.attachment);
            std::memcpy(clone.bytes.get() + 0x334, &context.notices.actor_address, 4);
            auto object = create_factory_object(drops.factory, {clone.bytes.get(), clone.size});
            clone.bytes.reset();
            drops.keep(std::move(object)); // The scene keeps the effect.
            spawn_runtime_body_death_drop(drops);
        }
    }
    activate_actor_bind_tree(drops.actor);
}

// Xbox 0x00134C10
void create_runtime_body_death_effect(const char* name, u32 activate,
    RuntimeBodyDeathEffects& context)
{
    notify_runtime_body_death_classes(context.notices);
    auto& drops = context.drops;
    if (context.live != 0 && (drops.actor.mask_mode == 2 || drops.actor.mask_flag != 0))
    {
        request_scene_aid(reinterpret_cast<const char*>(g_scene_record), 1);
        return;
    }
    float lifetime;
    if (context.attributes.data() == nullptr)
    {
        lifetime = std::bit_cast<float>(0xBF800000u);
        if ((context.flags & 0x10) != 0)
        {
            lifetime = std::bit_cast<float>(0x40A00000u);
        }
    }
    else
    {
        lifetime = read_f32(context.attributes, 0x194);
        if (0.f > lifetime && (context.flags & 0x10) != 0)
        {
            lifetime = std::bit_cast<float>(0x40A00000u);
        }
    }
    std::array<u8, 0x55C> parameters;
    initialize_runtime_body_death_effect(parameters, drops.actor, context.notices.actor_address,
        std::bit_cast<float>(0xBF800000u), lifetime, context.default_name);
    if (name != nullptr)
    {
        u32 prefix;
        std::memcpy(&prefix, name, 4);
        if (prefix == 0x5F646961)
        {
            copy_death_effect_name(reinterpret_cast<char*>(parameters.data() + 0x338), name);
        }
    }
    auto object = create_factory_object(drops.factory, parameters);
    drops.keep(std::move(object)); // The scene keeps the effect.
    spawn_runtime_body_death_drop(drops);
    if (activate != 0)
    {
        activate_actor_bind_tree(drops.actor);
    }
}

// Xbox 0x00134D20
void create_runtime_body_named_death_effect(const char* name, const char* variant,
    u32 activate, RuntimeBodyDeathEffects& context)
{
    notify_runtime_body_death_classes(context.notices);
    auto& drops = context.drops;
    if (context.live != 0 && (drops.actor.mask_mode == 2 || drops.actor.mask_flag != 0))
    {
        request_scene_aid(reinterpret_cast<const char*>(g_scene_record), 1);
        return;
    }
    float lifetime;
    if (context.attributes.data() == nullptr)
    {
        lifetime = std::bit_cast<float>(0xBF800000u);
        if ((context.flags & 0x10) != 0)
        {
            lifetime = std::bit_cast<float>(0x40A00000u);
        }
    }
    else
    {
        lifetime = read_f32(context.attributes, 0x194);
        if (0.f > lifetime && (context.flags & 0x10) != 0)
        {
            lifetime = std::bit_cast<float>(0x40A00000u);
        }
    }
    std::array<u8, 0x55C> parameters;
    initialize_runtime_body_death_effect(parameters, drops.actor, context.notices.actor_address,
        std::bit_cast<float>(0xBF800000u), lifetime, context.default_name);
    copy_death_effect_name(reinterpret_cast<char*>(parameters.data() + 0x3B8), variant);
    if (name != nullptr)
    {
        u32 prefix;
        std::memcpy(&prefix, name, 4);
        if (prefix == 0x5F646961)
        {
            copy_death_effect_name(reinterpret_cast<char*>(parameters.data() + 0x338), name);
        }
    }
    auto object = create_factory_object(drops.factory, parameters);
    drops.keep(std::move(object)); // The scene keeps the effect.
    spawn_runtime_body_death_drop(drops);
    if (activate != 0)
    {
        activate_actor_bind_tree(drops.actor);
    }
}

// Xbox 0x00024890
void request_runtime_body_death_effect(u32 activate, RuntimeBodyDeathEffects& context)
{
    notify_runtime_body_death_once(context.notices.body, context.notices.actor_address, context.notices.game);
    release_runtime_player_attachment(0, 0, context.attachment);
    const auto attributes = context.attributes;
    u32 kind;
    if (attributes.data() == nullptr)
    {
        kind = 0;
    }
    else
    {
        kind = read_u32(attributes, 8);
    }
    if (kind == 1)
    {
        const char* name = attributes.data() != nullptr ?
            reinterpret_cast<const char*>(attributes.data() + 0x10) : context.fallback_name;
        create_runtime_body_death_effect(name, activate, context);
    }
    else
    {
        if (kind == 2)
        {
            const char* effect_name = attributes.data() != nullptr ?
                reinterpret_cast<const char*>(attributes.data() + 0x90) : nullptr;
            const char* variant = attributes.data() != nullptr ?
                reinterpret_cast<const char*>(attributes.data() + 0x10) : context.fallback_name;
            create_runtime_body_named_death_effect(effect_name, variant, activate, context);
        }
    }
}

// Xbox 0x00134EF0
void notify_runtime_body_death_classes(RuntimeBodyDeathNotice& context)
{
    notify_runtime_body_death_once(context.body, context.actor_address, context.game);
    const std::array<u32, 3> death{0x6A, context.actor_address, body_record(context.body).unk_274};
    script_queue_event(context.game, {reinterpret_cast<const u8*>(death.data()), sizeof(death)});
    const u32 source = body_record(context.body).unk_274;
    u32 prefix;
    std::memcpy(&prefix, context.name, 4);
    if (prefix == 0x5F646961 ||
        (context.spawn_record != nullptr && context.spawn_record->remaining_c != 0))
    {
        std::array<u32, 3> message{0x6B, 0, source};
        if (prefix == 0x5F646961)
        {
            const u16 type = runtime_executable_actor_type(context.name, context.resources);
            message[1] = type;
            if (type > 1 && type < 0x3D)
            {
                script_queue_event(context.game, {reinterpret_cast<const u8*>(message.data()), sizeof(message)});
            }
        }
        if (context.spawn_record != nullptr && context.spawn_record->remaining_c != 0)
        {
            auto** owner = collect_runtime_ghouly_names(*context.spawn_record);
            u32 index = 0;
            while (true)
            {
                auto* pool = *owner;
                auto* row = pool->begin;
                u32 count = 0;
                if (!(row < pool->end))
                {
                    break;
                }
                do
                {
                    if (row->name != nullptr)
                    {
                        ++count;
                    }
                    ++row;
                } while (row < pool->end);
                if (index >= count)
                {
                    break;
                }
                const char* name = pool->begin[index].name;
                if (!resource_names_match(name, context.name))
                {
                    u16 type = 0;
                    if (name != nullptr)
                    {
                        u32 name_prefix;
                        std::memcpy(&name_prefix, name, 4);
                        if (name_prefix == 0x5F646961)
                        {
                            auto* record = find_executable_resource(context.resources, name, true);
                            if (record != nullptr && record->cpu.data() != nullptr)
                            {
                                type = read_u16(record->cpu, 2);
                            }
                        }
                    }
                    message[1] = (message[1] & 0xFFFF0000) | type;
                    if (type > 1 && type < 0x3D)
                    {
                        script_queue_event(context.game, {reinterpret_cast<const u8*>(message.data()), sizeof(message)});
                    }
                }
                ++index;
            }
            release_runtime_reference_name_owner(owner);
        }
    }
}

// Xbox 0x00068770
void initialize_runtime_body_death_effect(std::span<u8, 0x55C> parameters,
    const Actor& actor, u32 actor_address, float first, float lifetime,
    const char* default_name)
{
    initialize_actor_effect_parameters(parameters.data(), default_name);
    const u16 size = 0x55C;
    const u16 type = 0x42;
    const u16 flags = 0x12;
    std::memcpy(parameters.data(), &size, 2);
    std::memcpy(parameters.data() + 2, &type, 2);
    std::memcpy(parameters.data() + 4, &flags, 2);
    const Vec3 position = actor_world_position(actor);
    const Vec3 euler = actor_world_euler(actor);
    std::memcpy(parameters.data() + 8, &position, 12);
    std::memcpy(parameters.data() + 0x14, &euler, 12);
    std::memcpy(parameters.data() + 0x4BC, &first, 4);
    std::memcpy(parameters.data() + 0x334, &actor_address, 4);
    std::memcpy(parameters.data() + 0x4C0, &lifetime, 4);
    const u32 zero = 0;
    parameters[0x3B8] = 0;
    std::memcpy(parameters.data() + 0x558, &zero, 4);
    parameters[0x438] = 0;
    std::memcpy(parameters.data() + 0x4B8, &zero, 4);
    std::memcpy(parameters.data() + 0x544, &zero, 4);
    parameters[0x4C4] = 0;
    const char* name = "aid_fxemitter_ghoulies_weapon_bits_explodeall";
    char* destination = reinterpret_cast<char*>(parameters.data() + 0x338);
    char character;
    do
    {
        character = *name++;
        *destination++ = character;
    } while (character != '\0');
}
}
