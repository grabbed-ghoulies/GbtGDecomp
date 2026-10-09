#include "ghouly_box_messages.h"
#include "actor_spawn_eligibility.h"
#include "actor_spawn_query.h"

#include <cstring>

namespace gbtg
{
// Xbox 0x0006BDF0
void dispatch_ghouly_box_message(GhoulyBoxController& controller, void* message,
    GhoulyBoxMessageContext& context)
{
    u32 id;
    std::memcpy(&id, message, 4);
    auto& events = context.base.events;
    switch (id)
    {
    case 0x7C:
    {
        void* value = static_cast<ActorReferenceMessage*>(message)->value;
        if (value == controller.source_93c)
        {
            controller.source_93c = nullptr;
        }
        if (value == controller.anchor_934)
        {
            controller.anchor_934 = nullptr;
        }
        break;
    }
    case 0x7D:
    {
        if (events.state.setting_1fe != 0)
        {
            auto* anchor = find_actor_by_id(events.state.setting_1fe);
            controller.anchor_934 = anchor == nullptr ? nullptr : context.controllers.at(anchor);
        }
        const Vec3 position = actor_world_position(controller.actor);
        auto* node = find_actor_by_group(events.state.setting_1fc);
        Actor* source;
        if (node == nullptr)
        {
            float distance;
            source = find_nearest_spawn_actor(position, nullptr, &distance, 4);
        }
        else
        {
            source = context.actors.at(node);
        }
        controller.source_93c = source;
        break;
    }
    case 0xA6:
    {
        auto& pose = *static_cast<GhoulyPoseMessage*>(message);
        auto& source = *static_cast<Actor*>(controller.source_93c);
        pose.output_position = actor_world_position(source);
        pose.output_euler = actor_world_euler(source);
        pose.result = 1;
        break;
    }
    case 0xA7:
    {
        auto& spawn = *static_cast<GhoulySpawnMessage*>(message);
        auto& source = *static_cast<Actor*>(controller.source_93c);
        if (!actor_can_supply_ghoulybox(source))
        {
            spawn.result = -2;
        }
        else
        {
            Vec3 source_position;
            Vec3 source_euler;
            Vec3 position;
            Vec3 euler;
            if (query_ghouly_spawn_pose(controller, source_position, source_euler, position, euler))
            {
                const char* attributes = std::memcmp(spawn.attributes, "aid_", 4) == 0 ? spawn.attributes : nullptr;
                const char* second = std::memcmp(spawn.second_override, "aid_", 4) == 0 ? spawn.second_override : nullptr;
                const char* variant = std::memcmp(spawn.variant, "aid_", 4) == 0 ? spawn.variant : nullptr;
                auto object = create_spawn_actor_unit_scale(events.scares.factory, events.scares.resources,
                    context.scene_records, spawn.name, position, euler, variant, attributes, second, nullptr, nullptr);
                spawn.result = object.bytes != nullptr;
                if (object.bytes != nullptr)
                {
                    events.scares.objects.push_back(std::move(object));
                    events.scares.flags |= 2;
                    activate_actor_bind_tree(source);
                    controller.spawned_940 = 1;
                }
            }
        }
        break;
    }
    case 0xA8:
    {
        auto& request = *static_cast<GhoulyRequest*>(message);
        auto* source = static_cast<Actor*>(controller.source_93c);
        if (source != nullptr && actor_can_supply_ghoulybox(*source) && controller.load_938 == nullptr)
        {
            auto* record = load_ghoulybox(request.box, events.effects.models.registry,
                events.effects.models.error, events.effects.random_state);
            controller.load_938 = record;
            if (record != nullptr)
            {
                record->controller_24 = &controller;
                record->flags_2c = 0;
                std::strcpy(record->attributes_30, request.attributes);
                link_actor_load_record(&g_side_list, *record);
                request.result = 1;
            }
        }
        break;
    }
    default:
        break;
    }
    dispatch_base_actor_message(message, context.base);
}
}
