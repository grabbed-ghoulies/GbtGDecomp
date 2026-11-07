#include "trigger_actor.h"
#include "actor_fields.h"
#include "actor_transform.h"
#include "player_mind_events.h"
#include "ghouly_spawn_trigger.h"
#include "ghouly_box_decoder.h"
#include "actor_template_creation.h"
#include "actor_lookup.h"
#include "player_strategy_goals.h"
#include "../navigation/spawn_candidates.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
TriggerState& fields_of(std::span<u8> bytes)
{
    return actor_record_view<TriggerState>(bytes);
}

const TriggerState& fields_of(std::span<const u8> bytes)
{
    return actor_record_view<TriggerState>(bytes);
}

void word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

// Xbox 0x000280D0
void copy_runtime_trigger_attributes(u8* destination, const u8* source)
{
    size_t remaining = 0x200;
    if (destination != source)
    {
        if ((reinterpret_cast<std::uintptr_t>(destination) & 7) == 0 &&
            (reinterpret_cast<std::uintptr_t>(source) & 7) == 0)
        {
            i32 count = 0x40;
            do
            {
                std::memcpy(destination, source, 4);
                std::memcpy(destination + 4, source + 4, 4);
                destination += 8;
                source += 8;
                remaining -= 8;
                --count;
            }
            while (count != 0);
        }
        if ((reinterpret_cast<std::uintptr_t>(destination) & 3) == 0 &&
            (reinterpret_cast<std::uintptr_t>(source) & 3) == 0 && remaining >= 4)
        {
            size_t count = remaining >> 2;
            do
            {
                std::memcpy(destination, source, 4);
                destination += 4;
                source += 4;
                remaining -= 4;
                --count;
            }
            while (count != 0);
        }
        if (remaining != 0)
        {
            do
            {
                *destination++ = *source++;
                --remaining;
            }
            while (remaining != 0);
        }
    }
}

// Xbox 0x00069600
void runtime_trigger_storage_sizes(ObjectSizes& sizes)
{
    sizes = {sizeof(TriggerState), trigger_parameter::kSize};
}

// Xbox 0x00069660
bool initialize_runtime_trigger_actor(RuntimeTriggerActorContext& context,
    std::span<const u8> parameters)
{
    const u32 key = get(parameters, trigger_parameter::controller_key);
    if (key != 0)
    {
        if (g_ghouly_controllers.size() == g_ghouly_controllers.capacity())
        {
            g_ghouly_controllers.reserve(g_ghouly_controllers.size() + 10);
        }
        g_ghouly_controllers.push_back({key, &context.controller});
    }
    fields_of(context.bytes).min_radius = get(parameters, trigger_parameter::min_radius);
    fields_of(context.bytes).max_radius = get(parameters, trigger_parameter::max_radius);
    fields_of(context.bytes).goal = get(parameters, trigger_parameter::goal);
    copy_runtime_trigger_attributes(fields_of(context.bytes).goal_names.data(),
        field_data(parameters, trigger_parameter::goal_names));
    fields_of(context.bytes).anchor = 0;
    fields_of(context.bytes).facing = get(parameters, trigger_parameter::facing);
    fields_of(context.bytes).unk_B4C = get(parameters, trigger_parameter::unk_548);
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    const float random = std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
    if (random <= get(parameters, trigger_parameter::box_chance))
    {
        std::strcpy(fields_of(context.bytes).box_name.data(),
                    field_data(parameters, trigger_parameter::box_name));
        std::strcpy(fields_of(context.bytes).box_attributes.data(),
                    field_data(parameters, trigger_parameter::box_attributes));
        fields_of(context.bytes).trigger_radius = get(parameters, trigger_parameter::trigger_radius);
        fields_of(context.bytes).trigger_chance = get(parameters, trigger_parameter::trigger_chance);
    }
    fields_of(context.bytes).slot_count = get(parameters, trigger_parameter::slot_count);
    return true;
}

// Xbox 0x00069910
bool find_runtime_trigger_spawn_slot(std::span<u8> bytes, i32& slot)
{
    const i32 count = i32(fields_of(bytes).slot_count);
    if (count == 0)
    {
        fields_of(bytes).slots.data()[0] = 0;
        slot = 0;
        return true;
    }
    i32 index = 0;
    if (count > 0)
    {
        do
        {
            if (fields_of(bytes).slots.data()[index] == 0)
            {
                slot = index;
                return true;
            }
            ++index;
        }
        while (index < i32(fields_of(bytes).slot_count));
    }
    return false;
}

// Xbox 0x00069E00
i32 query_runtime_trigger_pose(RuntimeTriggerActorContext& context, i32 select,
    Vec3& position, Vec3& euler)
{
    const Vec3 origin = actor_world_position(context.actor);
    if (select == 0)
    {
        runtime_random_annulus_position(position, origin, fields_of(context.bytes).max_radius,
            fields_of(context.bytes).min_radius, context.random);
        select = 1;
    }
    else
    {
        const i32 floor = entity_detail::navigation_floor_index(origin, -1, 0);
        select = entity_detail::select_navigation_spawn_position(floor, nullptr, origin,
            fields_of(context.bytes).min_radius, fields_of(context.bytes).max_radius, nullptr,
            nullptr, 0, 0, position, context.random, context.occupancy_initial) ? 1 : 0;
        if (select == 0)
        {
            return 0;
        }
    }
    euler = actor_world_euler(context.actor);
    const Vec3 direction{origin.x - position.x, origin.y - position.y, origin.z - position.z};
    float angle;
    if (runtime_direction_angle(direction, angle))
    {
        const i32 mode = i32(fields_of(context.bytes).facing);
        if (mode != 1)
        {
            if (mode != 2)
            {
                return select;
            }
            angle -= std::bit_cast<float>(0x40490FDBu);
        }
        euler = {0.f, angle, 0.f};
    }
    return select;
}

// Xbox 0x000697D0
bool query_runtime_trigger_spawn_pose(RuntimeTriggerActorContext& context,
    Vec3& source_position, Vec3& source_euler, Vec3& position, Vec3& euler)
{
    const i32 select =
        fields_of(context.bytes).goal == 0 && fields_of(context.bytes).unk_B4C == 0 ? 1 : 0;
    if (query_runtime_trigger_pose(context, select, source_position, source_euler) != 0)
    {
        const u32 anchor = fields_of(context.bytes).anchor;
        if (anchor == 0)
        {
            position = source_position;
            euler = source_euler;
            return true;
        }
        std::array<u32, 15> message{};
        message[0] = 0xA6;
        std::memcpy(message.data() + 1, &source_position, 12);
        std::memcpy(message.data() + 4, &source_euler, 12);
        context.message(anchor, message);
        if (message[14] != 0)
        {
            std::memcpy(&position, message.data() + 8, 12);
            std::memcpy(&euler, message.data() + 11, 12);
            return true;
        }
    }
    return false;
}

// Xbox 0x000697A0
void destroy_runtime_trigger_actor(RuntimeTriggerActorContext& context)
{
    remove_ghouly_controller(&context.controller);
    if (context.load != nullptr)
    {
        release_ghouly_spawn_record(*context.load);
        context.load = nullptr;
        fields_of(context.bytes).box_record = 0;
    }
}

// Xbox 0x00069D20
void update_runtime_trigger_actor(RuntimeTriggerActorContext& context)
{
    const Vec3 position = actor_world_position(context.actor);
    if (check_ghouly_spawn_trigger(position, fields_of(context.bytes).trigger_radius,
        fields_of(context.bytes).trigger_chance, context.random) == 1)
    {
        std::array<u32, 0x42> message{};
        message[0] = 0xA8;
        std::strcpy(reinterpret_cast<char*>(message.data() + 1),
            fields_of(context.bytes).box_name.data());
        std::strcpy(reinterpret_cast<char*>(message.data() + 0x21),
            fields_of(context.bytes).box_attributes.data());
        context.self_message(message);
        fields_of(context.bytes).trigger_radius = 0.f;
        fields_of(context.bytes).trigger_chance = 0.f;
    }
    if (context.load != nullptr && context.load->remaining_c == 0)
    {
        release_ghouly_spawn_record(*context.load);
        context.load = nullptr;
        fields_of(context.bytes).box_record = 0;
    }
}

// Xbox 0x00069960
void dispatch_runtime_trigger_message(RuntimeTriggerMessages& context, std::span<u32> message)
{
    auto& trigger = context.trigger;
    const auto bytes = trigger.bytes;
    switch (message[0])
    {
    case 0x7C:
        if (message[1] == fields_of(bytes).anchor)
        {
            fields_of(bytes).anchor = 0;
        }
        for (size_t row = 0; row < 4; ++row)
        {
            if (message[1] == fields_of(bytes).slots.data()[row])
            {
                fields_of(bytes).slots.data()[row] = 0;
            }
        }
        break;
    case 0x7D:
        if (actor_marker_next(bytes) != 0)
        {
            fields_of(bytes).anchor = find_runtime_actor_by_id(actor_marker_next(bytes), context.latch_owner,
                context.bytes_at); // 0x00129ED0
        }
        break;
    case 0xA6:
        {
            Vec3 position, euler;
            std::memcpy(&position, message.data() + 8, 12);
            std::memcpy(&euler, message.data() + 11, 12);
            message[14] = u32(query_runtime_trigger_pose(trigger, 1, position, euler));
            std::memcpy(message.data() + 8, &position, 12);
            std::memcpy(message.data() + 11, &euler, 12);
        }
        break;
    case 0xA7:
    case 0xA9:
    case 0xAA:
        {
            i32 slot;
            if (!find_runtime_trigger_spawn_slot(bytes, slot))
            {
                break;
            }
            Vec3 source_position, source_euler, position, euler;
            if (!query_runtime_trigger_spawn_pose(trigger, source_position, source_euler, position, euler))
            {
                break;
            }
            const char* name = reinterpret_cast<const char*>(message.data() + 1);
            FactoryObject* object;
            if (message[0] == 0xA7)
            {
                const u32 anchor = fields_of(bytes).anchor;
                const u32 chained = anchor != 0 && actor_marker_next(context.bytes_at(anchor)) != 0 ? 1 : 0;
                const auto aid = [&](size_t index) -> const char*
                {
                    return message[index] == 0x5F646961 ?
                        reinterpret_cast<const char*>(message.data() + index) : nullptr;
                };
                object = create_runtime_marker_actor(name, source_position, source_euler, 1.f,
                    aid(0x21), aid(0x61), aid(0x41), fields_of(bytes).goal, 0, // EDX +0x184, ECX +0x104.
                    position, euler, fields_of(bytes).goal_names.data(), chained, 0, context.objects);
                if (chained != 0)
                {
                    send_runtime_marker_actor_goals(actor_marker_next(context.bytes_at(anchor)),
                        fields_of(bytes).goal, *object, context.objects);
                }
                if (object != nullptr)
                {
                    play_runtime_actor_goal_effects(context.actor_of(*object),
                        {object->bytes.get(), object->size}, context.effects);
                    fields_of(bytes).slots.data()[slot] = context.address_of(object->bytes.get());
                    message[0x81] = 1;
                }
            }
            else
            {
                if (message[0] == 0xA9)
                {
                    object = context.keep(create_runtime_actor_pose(name, source_position, source_euler,
                        context.objects.resources, context.objects.factory));
                }
                else
                {
                    object = context.keep(create_runtime_actor_at(name, &source_position, &source_euler,
                        1.f, context.objects.resources, context.objects.factory, source_position, source_euler));
                }
                if (object != nullptr)
                {
                    Actor& actor = context.actor_of(*object);
                    actor_set_world_position(actor, position);
                    normalize_euler_angles(actor.euler, euler);
                    word({object->bytes.get(), object->size}, 0x2A8, 0);
                    actor.xform_valid = 0;
                    fields_of(bytes).slots.data()[slot] = context.address_of(object->bytes.get());
                    message[0x21] = 1;
                }
                else if (message[0] == 0xAA)
                {
                    message[0x21] = 1;
                }
            }
        }
        break;
    case 0xA8:
        if (trigger.load == nullptr)
        {
            trigger.load = load_ghoulybox(reinterpret_cast<const char*>(message.data() + 1),
                context.objects.registry, context.objects.error, trigger.random);
            fields_of(bytes).box_record = context.address_of(trigger.load);
            if (trigger.load != nullptr)
            {
                trigger.load->controller_24 = &trigger.controller;
                trigger.load->flags_2c = 0;
                std::strcpy(trigger.load->attributes_30, reinterpret_cast<const char*>(message.data() + 0x21));
                link_actor_load_record(&g_side_list, *trigger.load);
                message[0x41] = 1;
            }
        }
        break;
    default:
        break;
    }
    context.base_message(context.base, message);
}
}
