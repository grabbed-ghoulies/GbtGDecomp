#include "ghouly_cutscene_setup.h"
#include "actor_lookup.h"
#include "actor_template_creation.h"
#include "actor_transform.h"
#include "ghouly_box_decoder.h"
#include "../camera/view_initialization.h"
#include "../cutscenes/event_pause.h"
#include "../cutscenes/event_runtime.h"
#include "../cutscenes/instance_construction.h"
#include "../navigation/navigation.h"
#include "../navigation/spawn_candidates.h"
#include "../scene/actor_loading.h"
#include <cstdio>
#include <cstring>

namespace gbtg
{
namespace
{
GhoulyCutsceneState& fields_of(std::span<u8> bytes)
{
    return actor_record_view<GhoulyCutsceneState>(bytes);
}

const GhoulyCutsceneState& fields_of(std::span<const u8> bytes)
{
    return actor_record_view<GhoulyCutsceneState>(bytes);
}

namespace parameter = ghouly_cutscene_parameter;

CutsceneEventPauseContext pause_context(RuntimeGhoulyCutsceneSetup& context, RuntimeCutsceneInstance& instance)
{
    CutsceneRuntimeContext& runtime = context.cutscenes();
    return CutsceneEventPauseContext{runtime.instances.at(&instance).actors.data(), runtime.animations,
        context.voices, runtime.resume_callbacks};
}

const char* name_at(std::span<const u32> message, size_t index)
{
    return reinterpret_cast<const char*>(message.data() + index);
}
}

// Xbox 0x0006B0F0
void ghouly_cutscene_setup_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = 0x9DC;
    sizes.parameters = 0x3C4;
}

// Xbox 0x0006B150
bool initialize_runtime_ghouly_cutscene_setup(RuntimeGhoulyCutsceneSetup& context, std::span<const u8> parameters)
{
    const auto bytes = context.bytes;
    const u32 key = get(parameters, parameter::controller_key);
    if (key != 0)
    {
        if (g_ghouly_controllers.size() == g_ghouly_controllers.capacity())
        {
            g_ghouly_controllers.reserve(g_ghouly_controllers.size() + 10); // 0x000662A0
        }
        g_ghouly_controllers.push_back({key, &context.controller});
    }
    fields_of(bytes).spawn_tag = get(parameters, parameter::spawn_tag);
    fields_of(bytes).hold_last_frame = get(parameters, parameter::hold_last_frame);
    fields_of(bytes).camera_mode = get(parameters, parameter::camera_mode);
    initialize_camera_view(context.camera, fields_of(bytes).camera_name.data(), 0); // 0x000496C0
    CutsceneRuntimeContext& runtime = context.cutscenes();
    context.cutscene = load_runtime_cutscene(field_data(parameters, parameter::cutscene), 2,
        &context.camera, runtime); // 0x0004D680
    RuntimeCutsceneInstance& instance = *context.cutscene;
    instance.pending[0] = 0x0006BA60; // +0x150, the spawn event callback.
    instance.pending[1] = context.self_address(); // +0x154.
    fields_of(bytes).running = 0;
    fields_of(bytes).ended = 0;
    // The body of 0x0004E640 with one pause, inlined.
    CutsceneEventPauseContext pause = pause_context(context, instance);
    pause_cutscene_events(instance, 1, pause);
    if (fields_of(bytes).camera_mode != 0)
    {
        std::snprintf(fields_of(bytes).camera_name.data(), sizeof(GhoulyCutsceneState::camera_name), "spawnId%d",
            i32(key)); // 0x00323E08
        link_camera_view(0, context.camera); // 0x00048C20, ESI 0
    }
    fields_of(bytes).anchor = 0;
    fields_of(bytes).pending_spawns = 0;
    return true;
}

// Xbox 0x0006B350
void destroy_runtime_ghouly_cutscene_setup(RuntimeGhoulyCutsceneSetup& context)
{
    remove_ghouly_controller(&context.controller); // 0x0012ACA0
    if (context.load != nullptr)
    {
        release_ghouly_spawn_record(*context.load); // 0x000F3A00
        context.load = nullptr;
    }
    if (fields_of(context.bytes).camera_mode != 0)
    {
        camera_unlink_view(&context.camera); // 0x00048C70
    }
}

// Xbox 0x0006B9C0, EAX position
i32 query_runtime_ghouly_cutscene_pose(RuntimeGhoulyCutsceneSetup& context, i32 select, Vec3& position, Vec3& euler)
{
    const Vec3 origin = actor_world_position(context.actor); // 0x000412A0
    if (select != 0)
    {
        const i32 floor = entity_detail::navigation_floor_index(origin, -1, 0); // 0x00110BF0
        const i32 found = entity_detail::select_navigation_spawn_position(floor, nullptr, origin, 0.f, 0.f, nullptr,
            nullptr, 0, 0, position, context.random, context.occupancy_initial) ? 1 : 0; // 0x00111420
        if (found != 0)
        {
            euler = actor_world_euler(context.actor); // 0x00041820
        }
        return found;
    }
    position = origin;
    euler = actor_world_euler(context.actor); // 0x00041820
    return 1;
}

// Xbox 0x0006B390, EAX actor, EBX/ESI the sources
bool query_runtime_ghouly_cutscene_spawn_pose(RuntimeGhoulyCutsceneSetup& context, Vec3& source_position,
    Vec3& source_euler, Vec3& position, Vec3& euler)
{
    const i32 select = fields_of(context.bytes).spawn_tag == 0 ? 1 : 0;
    if (query_runtime_ghouly_cutscene_pose(context, select, source_position, source_euler) == 0)
    {
        return false;
    }
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
    if (message[14] == 0)
    {
        return false;
    }
    std::memcpy(&position, message.data() + 8, 12);
    std::memcpy(&euler, message.data() + 11, 12);
    return true;
}

// Xbox 0x0006BA60, cutscene event 0x13
void release_runtime_ghouly_cutscene_spawn(std::span<u8> bytes)
{
    fields_of(bytes).pending_spawns = fields_of(bytes).pending_spawns + 1;
}

// Xbox 0x0006B4C0
void dispatch_runtime_ghouly_cutscene_setup_message(RuntimeGhoulyCutsceneSetupMessages& context,
    std::span<u32> message)
{
    auto& setup = context.setup;
    const auto bytes = setup.bytes;
    switch (message[0])
    {
    case 0x7C:
        if (message[1] == fields_of(bytes).anchor)
        {
            fields_of(bytes).anchor = 0;
        }
        break;
    case 0x7D:
        if (read_u16(bytes, 0x1FE) != 0)
        {
            fields_of(bytes).anchor = find_runtime_actor_by_id(read_u16(bytes, 0x1FE), context.latch_owner,
                context.bytes_at); // 0x00129ED0
        }
        break;
    case 0x80:
        if (setup.cutscene != nullptr)
        {
            release_runtime_cutscene(setup.cutscene, setup.cutscenes()); // 0x0004D720
            setup.cutscene = nullptr;
        }
        break;
    case 0xA6:
    {
        Vec3 position;
        Vec3 euler;
        std::memcpy(&position, message.data() + 8, 12);
        std::memcpy(&euler, message.data() + 11, 12);
        message[14] = u32(query_runtime_ghouly_cutscene_pose(setup, 1, position, euler));
        std::memcpy(message.data() + 8, &position, 12);
        std::memcpy(message.data() + 11, &euler, 12);
        break;
    }
    case 0xA7:
    {
        if (fields_of(bytes).pending_spawns == 0)
        {
            break;
        }
        Vec3 source_position, source_euler, position, euler;
        if (!query_runtime_ghouly_cutscene_spawn_pose(setup, source_position, source_euler, position, euler))
        {
            break;
        }
        const auto aid = [&](size_t index) -> const char*
        {
            return message[index] == 0x5F646961 ? name_at(message, index) : nullptr;
        };
        FactoryObject* object = create_runtime_marker_actor(name_at(message, 1), source_position, source_euler, 1.f,
            aid(0x21), aid(0x61), aid(0x41), fields_of(bytes).spawn_tag, 0, position, euler, nullptr, 0, 0,
            context.objects); // 0x001349E0, EDX +0x184, ECX +0x104
        message[0x81] = object != nullptr ? 1 : 0;
        if (object != nullptr)
        {
            fields_of(bytes).pending_spawns = fields_of(bytes).pending_spawns - 1;
        }
        break;
    }
    case 0xA8:
    {
        if (setup.load != nullptr || fields_of(bytes).running != 0)
        {
            break;
        }
        setup.load = load_ghoulybox(name_at(message, 1), context.objects.registry, context.objects.error,
            setup.random); // 0x000F39A0
        if (setup.load == nullptr)
        {
            break;
        }
        setup.load->controller_24 = &setup.controller;
        setup.load->flags_2c = 0;
        std::strcpy(setup.load->attributes_30, name_at(message, 0x21));
        link_actor_load_record(&g_side_list, *setup.load); // 0x000F3C30, DAT_00510568
        message[0x41] = 1;
        if (fields_of(bytes).ended != 0 && fields_of(bytes).hold_last_frame != 0)
        {
            break;
        }
        if (fields_of(bytes).running == 0)
        {
            fields_of(bytes).running = 1;
            CutsceneEventPauseContext pause = pause_context(setup, *setup.cutscene);
            pause_cutscene_events(*setup.cutscene, 0, pause); // 0x0004E640, EAX 0
        }
        const u32 mode = fields_of(bytes).camera_mode;
        if (mode != 0 && (mode == 2 || fields_of(bytes).ended == 0))
        {
            camera_bind_named_view(0, fields_of(bytes).camera_name.data()); // 0x00048CF0
        }
        break;
    }
    case 0xA9:
    case 0xAA:
    {
        if (fields_of(bytes).pending_spawns == 0)
        {
            break;
        }
        Vec3 source_position, source_euler, position, euler;
        if (!query_runtime_ghouly_cutscene_spawn_pose(setup, source_position, source_euler, position, euler))
        {
            break;
        }
        FactoryObject* object;
        if (message[0] == 0xA9)
        {
            object = context.keep(create_runtime_actor_pose(name_at(message, 1), source_position, source_euler,
                context.objects.resources, context.objects.factory)); // 0x00141840
        }
        else
        {
            object = context.keep(create_runtime_actor_at(name_at(message, 1), &source_position, &source_euler, 1.f,
                context.objects.resources, context.objects.factory, source_position, source_euler)); // 0x00112C50
        }
        if (object != nullptr)
        {
            Actor& actor = context.actor_of(*object);
            actor_set_world_position(actor, position); // 0x00041730
            normalize_euler_angles(actor.euler, euler); // 0x00105AA0
            const u32 stale = 0;
            std::memcpy(object->bytes.get() + 0x2A8, &stale, 4); // Actor +0x2A8, which waits for the actor's layout.
            actor.xform_valid = 0;
        }
        message[0x21] = object != nullptr ? 1 : 0;
        if (object != nullptr)
        {
            fields_of(bytes).pending_spawns = fields_of(bytes).pending_spawns - 1;
        }
        break;
    }
    default:
        break;
    }
    context.base_message(context.base, message); // 0x0003EF70
}

// Xbox 0x0006B8B0 after 0x000420D0
void update_runtime_ghouly_cutscene_setup(RuntimeGhoulyCutsceneSetup& context)
{
    const auto bytes = context.bytes;
    if (context.load != nullptr && context.load->remaining_c == 0)
    {
        release_ghouly_spawn_record(*context.load); // 0x000F3A00
        context.load = nullptr;
    }
    if (fields_of(bytes).running == 0 || context.cutscene == nullptr)
    {
        return;
    }
    EntityCamNode* camera = fields_of(bytes).camera_mode != 0 ? &context.camera : nullptr;
    CutsceneRuntimeContext& runtime = context.cutscenes();
    const float rate = context.actor.animation_pause > 0.f ? 0.f : context.actor.animation_rate;
    const float step = rate * runtime_frame_step(context.clock);
    if (!tick_runtime_cutscene(*context.cutscene, step, camera, runtime)) // 0x0004DA50
    {
        return;
    }
    fields_of(bytes).ended = 1;
    fields_of(bytes).running = 0;
    if (fields_of(bytes).hold_last_frame == 0)
    {
        seek_runtime_cutscene(*context.cutscene, 0.f, -1.f, 0, runtime); // 0x0004E4A0
        CutsceneEventPauseContext pause = pause_context(context, *context.cutscene);
        pause_cutscene_events(*context.cutscene, 1, pause); // 0x0004E640, EAX 1
    }
    if (fields_of(bytes).camera_mode != 0)
    {
        camera_rebind_view(&context.camera); // 0x00048EB0
    }
}
}
