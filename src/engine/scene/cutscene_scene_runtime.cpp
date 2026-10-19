#include "cutscene_scene_runtime.h"
#include "scene_controls.h"
#include "../camera/view_initialization.h"
#include "../entities/actor_base_messages.h"
#include <bit>
#include <cstring>
#include <memory>

namespace gbtg
{
namespace
{
std::unordered_map<Game*, std::unique_ptr<SceneCutsceneRuntime>> scene_cutscenes;
// DAT_0050E438, the active scene controls: each control's vtable +0xC with the message. The instance pointer in
// the message's second word goes to the native controls as their reference.
void notify_scene_cutscene(const ActorReferenceMessage& message)
{
    if (g_scene_control_owner == nullptr)
    {
        return;
    }
    auto& owner = *g_scene_control_owner;
    const std::array<u32, 2> words{message.id, 0};
    RuntimeActorLink* node = owner.context.lists.active.next;
    while (node != nullptr)
    {
        RuntimeActorLink* next = node->next;
        owner.message(owner.instance_of(*node), words, message.value);
        node = next;
    }
}
void decrement_cutscene_count(i32& count)
{
    count = std::bit_cast<i32>(static_cast<u32>(count) - 1);
    if (count < 0)
    {
        count = 0;
    }
}
}

void install_scene_cutscene_runtime(Game& game, SceneCutsceneBindings bindings)
{
    if (scene_cutscenes.contains(&game))
    {
        throw Error("scene cutscene runtime already installed");
    }
    scene_cutscenes.emplace(&game, std::make_unique<SceneCutsceneRuntime>(SceneCutsceneRuntime{bindings}));
}

SceneCutsceneRuntime& scene_cutscene_runtime(Game& game)
{
    const auto found = scene_cutscenes.find(&game);
    if (found == scene_cutscenes.end())
    {
        throw Error("scene cutscene runtime is not installed: 0x0004EDA0 needs a native scene");
    }
    return *found->second;
}

SceneCutsceneRuntime* find_scene_cutscene_runtime(Game& game)
{
    const auto found = scene_cutscenes.find(&game);
    return found == scene_cutscenes.end() ? nullptr : found->second.get();
}

void remove_scene_cutscene_runtime(Game& game)
{
    release_scene_cutscenes(scene_cutscene_runtime(game));
    scene_cutscenes.erase(&game);
}

// Xbox 0x0004F050
void release_scene_cutscene_instance(RuntimeCutsceneInstance& instance,
    SceneCutsceneRuntime& state)
{
    const ActorReferenceMessage message{0x7F, &instance};
    broadcast_actor_pulse_message(reinterpret_cast<const i32*>(&message));
    notify_scene_cutscene(message);
    release_runtime_cutscene(&instance, state.bindings.runtime);
}

// Xbox 0x0004EF20, ECX position
void place_scene_cutscene(const Vec3& position, SceneCutsceneRuntime& state)
{
    auto& runtime = state.bindings.runtime;
    auto& instance = *state.instances[state.index];
    const float fraction = instance.time / instance.duration;
    instance.position = position;
    update_runtime_cutscene_actors(instance, -1.f, fraction, runtime); // 0x0004E1E0
    auto& current = *state.instances[state.index];
    update_cutscene_camera_view(current, runtime.views.at(current.resource).cameras.data(), state.camera); // 0x0004D7D0
}

// Xbox 0x0004EF80, ECX euler
void turn_scene_cutscene(const Vec3& euler, SceneCutsceneRuntime& state)
{
    auto& runtime = state.bindings.runtime;
    auto& instance = *state.instances[state.index];
    instance.euler = euler;
    update_runtime_cutscene_actors(instance, -1.f, instance.time / instance.duration, runtime); // 0x0004E1E0
    auto& current = *state.instances[state.index];
    update_cutscene_camera_view(current, runtime.views.at(current.resource).cameras.data(), state.camera); // 0x0004D7D0
}

// Xbox 0x0004EAD0
void pop_scene_cutscene(SceneCutsceneRuntime& state)
{
    if (-1 < state.index)
    {
        release_scene_cutscene_instance(*state.instances[state.index], state);
        state.instances[state.index] = nullptr;
        --state.index;
        decrement_cutscene_count(state.bindings.active_count);
    }
}

// Xbox 0x0004EB20
void push_scene_cutscene(const char* name, i32 interpolation,
    SceneCutsceneRuntime& state)
{
    state.index = std::bit_cast<i32>(static_cast<u32>(state.index) + 1);
    u32 flags = state.index == 0;
    if (interpolation != 0)
    {
        flags |= 0x20;
    }
    auto* instance = load_runtime_cutscene(name, flags, &state.camera, state.bindings.runtime);
    state.instances[state.index] = instance;
    const ActorReferenceMessage message{0x7E, state.instances[state.index]};
    broadcast_actor_pulse_message(reinterpret_cast<const i32*>(&message));
    notify_scene_cutscene(message);
    state.bindings.active_count = std::bit_cast<i32>(static_cast<u32>(state.bindings.active_count) + 1);
}

// Xbox 0x0004EDA0
RuntimeCutsceneInstance* start_scene_cutscene(const char* name, i32 view_flags,
    i32 interpolation, SceneCutsceneRuntime& state)
{
    if (state.paused == 0)
    {
        ++state.bindings.pause_count;
        ++state.bindings.transition_pause_count;
        state.paused = 1;
    }
    state.bindings.scene_flags |= 0x400000;
    pop_scene_cutscene(state);
    if (state.linked == 0)
    {
        initialize_camera_view(state.camera, "cutscene", view_flags != 0 ? 8 : 0);
    }
    push_scene_cutscene(name, interpolation, state);
    state.prefix_time = 0.f;
    state.time = 0.f;
    if (state.linked == 0)
    {
        link_camera_view(0, state.camera);
        camera_bind_named_view(0, "cutscene");
        state.linked = 1;
        auto& runtime = state.bindings.runtime;
        auto& instance = *state.instances[state.index];
        update_cutscene_camera_view(instance, runtime.views.at(instance.resource).cameras.data(), state.camera);
        return state.instances[state.index];
    }
    return state.instances[state.index];
}

// Xbox 0x0004ECE0
void release_scene_cutscenes(SceneCutsceneRuntime& state)
{
    if (-1 < state.index)
    {
        if (state.linked != 0)
        {
            camera_unlink_view(&state.camera);
            state.linked = 0;
        }
        if (-1 < state.index)
        {
            do
            {
                release_scene_cutscene_instance(*state.instances[state.index], state);
                state.instances[state.index] = nullptr;
                --state.index;
                decrement_cutscene_count(state.bindings.active_count);
            }
            while (-1 < state.index);
        }
        auto& runtime = state.bindings.runtime;
        if (runtime.camera_asset.resource.data() != nullptr)
        {
            release_animation_resource(runtime.resources, runtime.camera_asset.resource);
            runtime.camera_asset.resource = {};
        }
        if (state.paused != 0)
        {
            --state.bindings.pause_count;
            --state.bindings.transition_pause_count;
            state.paused = 0;
        }
        state.bindings.scene_flags &= 0xFFBFFFFFu;
    }
}

// Xbox 0x0004F110
void update_scene_cutscene_camera_event(float dt, SceneCutsceneRuntime& state)
{
    auto& runtime = state.bindings.runtime;
    auto& asset = runtime.camera_asset;
    if (asset.resource.data() != nullptr)
    {
        asset.duration += dt;
        const float duration = read_f32(asset.resource, 4);
        if (duration <= asset.duration)
        {
            asset.duration -= duration;
        }
        Vec3 position;
        sample_cutscene_camera_position(runtime.views.at(asset.resource.data()).cameras[0],
            asset.duration, position, 1);
        state.camera.position.x += position.x;
        state.camera.position.y += position.y;
        state.camera.position.z += position.z;
    }
}

// Xbox 0x0004EBD0
void tick_scene_cutscenes(SceneCutsceneRuntime& state)
{
    float dt = 0.f;
    if (!(state.bindings.freeze > 0.f))
    {
        dt = state.bindings.dt * state.bindings.scene_scale;
    }
    auto& runtime = state.bindings.runtime;
    if (-1 < state.index)
    {
        if (state.stop != 0)
        {
            release_scene_cutscenes(state);
            state.stop = 0;
            return;
        }
        u32 prefix;
        std::memcpy(&prefix, state.pending_name.data(), sizeof(prefix));
        if (prefix == 0x5F646961u)
        {
            state.prefix_time = state.instances[state.index]->duration + state.prefix_time;
            state.time = state.prefix_time;
            pop_scene_cutscene(state);
            push_scene_cutscene(state.pending_name.data(), state.pending_interpolation, state);
            state.pending_name[0] = 0;
        }
        state.previous_time = state.time;
        state.time += dt;
        tick_runtime_cutscene(*state.instances[state.index], dt, &state.camera, runtime);
        update_scene_cutscene_camera_event(dt, state);
    }
    else if (runtime.camera_asset.resource.data() != nullptr)
    {
        release_animation_resource(runtime.resources, runtime.camera_asset.resource);
        runtime.camera_asset.resource = {};
        return;
    }
}
}
