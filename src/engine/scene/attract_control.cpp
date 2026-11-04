#include "attract_control.h"
#include "gbtg/model_runtime.h"
#include "../camera/view_initialization.h"
#include "../cutscenes/actor_tracks.h"

#include <bit>
#include <cstring>

namespace gbtg
{
RuntimeAttractInput g_attract_input;

namespace
{
constexpr u32 kAid = 0x5F646961u; // "aid_"

u32 word(std::span<const u8> bytes, size_t offset)
{
    return read_u32(bytes, offset);
}

// The frame step the tick uses twice: nothing while frozen (DAT_004312DC > 0), else DAT_005096C0 * DAT_004312C4.
float frame_step(const AttractControlServices& services)
{
    if (services.freeze <= 0.f)
    {
        return services.dt * services.scene_scale;
    }
    return 0.f;
}

RuntimeDialogueInputDevice attract_device()
{
    return {g_attract_input.device, {nullptr, nullptr, nullptr, nullptr}};
}
}

// Xbox 0x00080950
void attract_control_object_sizes(ObjectSizes& output)
{
    output.instance = 0x324;
    output.parameters = 0x188;
}

// Xbox 0x00080A30
bool construct_runtime_attract_control(std::span<const u8> parameters,
    AttractControlState& control, AttractControlServices& services)
{
    std::span<const u8> clips;
    if (word(parameters, 8) == kAid)
    {
        clips = acquire_animation_resource(services.resources, reinterpret_cast<const char*>(parameters.data() + 8));
        if (clips.data() == nullptr)
        {
            resource_error(services.error, 2);
        }
    }
    i32 count = 0;
    control.clips = clips;
    size_t row = 0;
    while (clips.data() != nullptr && word(clips, row) == kAid)
    {
        row += 0x80;
        ++count;
        if (row + 4 > clips.size())
        {
            throw Error("attract clip list runs past its aidlist resource");
        }
        if (word(clips, row) != kAid)
        {
            clips = {};
        }
    }
    control.clip_count = u32(count);
    size_t at = 0;
    char copied;
    do
    {
        copied = char(parameters[0x108 + at]);
        control.next_script[at] = copied;
        ++at;
    }
    while (copied != 0);
    if (word(parameters, 0x88) != kAid)
    {
        return false;
    }
    const auto text = acquire_animation_resource(services.resources,
        reinterpret_cast<const char*>(parameters.data() + 0x88));
    if (text.data() == nullptr)
    {
        resource_error(services.error, 2);
        return false;
    }
    if (lookup_borrowed_localtext(text, "attract", "StartBanner", control.banner_text.data(),
        services.text) == 0)
    {
        return false;
    }
    release_animation_resource(services.resources, text);
    allocate_dialogue_text_pool(control.banner_pool, 0, 1, 0x180, services.text_pools);
    show_runtime_attract_banner(control, services);
    initialize_camera_view(control.camera, "attract", 0);
    link_camera_view(0, control.camera);
    ++services.pause_count;
    if (push_runtime_attract_input(services) == 0)
    {
        return false;
    }
    control.clip = 0;
    control.cutscene = start_scene_cutscene(reinterpret_cast<const char*>(control.clips.data()), 1, 1,
        services.cutscenes);
    show_runtime_attract_bubbles(control, services);
    control.started = 0;
    return true;
}

// Xbox 0x00080BB0
void destroy_runtime_attract_control(AttractControlState& control,
    AttractControlServices& services)
{
    release_dialogue_text_pool(control.banner_pool, *services.text_pools);
    control.banner_pool = nullptr;
    camera_unlink_view(&control.camera);
    --services.pause_count;
    pop_runtime_attract_input(services);
    release_scene_cutscenes(services.cutscenes);
    if (control.clips.data() != nullptr)
    {
        release_animation_resource(services.resources, control.clips);
        control.clips = {};
    }
}

// Xbox 0x00080C10
void tick_runtime_attract_control(SceneControlBase& base, AttractControlState& control,
    AttractControlServices& services)
{
    base.elapsed = base.elapsed + frame_step(services);
    advance_runtime_attract_clip(control, services);
    if (control.started == 0)
    {
        accept_runtime_attract_start(control, services);
    }
    const float step = frame_step(services);
    if (0.f < control.banner_time)
    {
        const float left = control.banner_time - step;
        control.banner_time = left;
        if (left <= 0.f)
        {
            control.banner_time = 0.f;
            show_runtime_attract_banner(control, services);
        }
    }
}

// Xbox 0x000809C0
void show_runtime_attract_banner(AttractControlState& control,
    AttractControlServices& services)
{
    const float cell = float(i32(read_u32(services.font, 0x10)));
    const float y = services.banner_bottom - cell * 1.125f; // DAT_0032B5B4
    const u32 id = create_dialogue_text_record(*control.banner_pool,
        control.banner_text.data(), services.banner_x, y, 1.125f, 1.125f, 4.f, 1, 0, 0,
        services.metrics, services.viewport_width, services.next_text_id, services.random);
    control.banner = id;
    control.banner_time = 7.f; // DAT_0032B284
}

// Xbox 0x00080CB0
void accept_runtime_attract_start(AttractControlState& control, AttractControlServices& services)
{
    char script[0x80]{};
    std::memcpy(script, "aid_script_ghoulies_gameselect_gameselect", 42); // 0x00322B8C
    activate_dialogue_input(services.input_state);
    refresh_runtime_input_device(g_attract_input.address, services.frame, services.input, services.input_release);
    if (runtime_attract_start_pressed(services))
    {
        services.request_script(script);
        services.start_laugh();
        control.started = 1;
    }
}

// Xbox 0x00080D20
void advance_runtime_attract_clip(AttractControlState& control,
    AttractControlServices& services)
{
    auto& cutscenes = services.cutscenes;
    if (-1 < cutscenes.index)
    {
        const RuntimeCutsceneInstance& current = *cutscenes.instances[size_t(cutscenes.index)];
        if (current.duration <= current.time)
        {
            release_scene_cutscenes(cutscenes);
            const u32 clip = control.clip + 1;
            control.clip = clip;
            if (control.clip_count <= clip)
            {
                services.request_script(control.next_script.data());
                control.clip = 0;
            }
            control.cutscene = start_scene_cutscene(
                reinterpret_cast<const char*>(control.clips.data()) + size_t(control.clip) * 0x80, 1, 1,
                cutscenes);
            show_runtime_attract_bubbles(control, services);
        }
    }
}

// Xbox 0x00080DA0
void show_runtime_attract_bubbles(AttractControlState& control, AttractControlServices& services)
{
    const i32 language = services.language;
    // Copied from 0x00323EB0, 0x00323E74 and 0x00323E38 into 0x80-byte rows.
    constexpr std::array<const char*, 3> bubbles{
        "aid_model_ghoulies_cutscene_attractmodespeechbubble",
        "aid_model_ghoulies_cutscene_attractmodespeechbubbleaction",
        "aid_model_ghoulies_cutscene_attractmodespeechbubblebaddies",
    };
    for (const char* name : bubbles)
    {
        if (control.cutscene == nullptr)
        {
            throw Error("attract speech bubbles have no cutscene (0x0004EA50 reads +0x29C)");
        }
        auto& runtime = services.cutscenes.bindings.runtime;
        const auto& view = runtime.views.at(control.cutscene->resource);
        auto& rows = runtime.instances.at(control.cutscene);
        Actor* actor = find_cutscene_actor_by_model(*control.cutscene, view.tracks.data(), rows.actors.data(), name);
        if (actor != nullptr)
        {
            const u8 node = services.bubble_nodes[size_t(language) * 4];
            if (actor->model != nullptr)
            {
                set_runtime_model_node_state(*actor->model->runtime, node, 1, services.node_cache);
            }
            if (RuntimeModelInstance* secondary = services.secondary_model(*actor); secondary != nullptr)
            {
                set_runtime_model_node_state(*secondary, node, 1, services.node_cache);
            }
        }
    }
}

// Xbox 0x00143070
u32 push_runtime_attract_input(AttractControlServices& services)
{
    g_attract_input.axes = {};
    g_attract_input.device = create_runtime_keyboard_input(services.input_release);
    // Native pointer mapping; retail stores the allocated address in DAT_005461A0.
    g_attract_input.address = static_cast<u32>(reinterpret_cast<std::uintptr_t>(g_attract_input.device.data()));
    services.input_addresses[g_attract_input.address] = g_attract_input.device;
    services.input_views[g_attract_input.address] = attract_device();
    push_runtime_input_entry(g_attract_input.address, "gameselect", services.input); // 0x003241E0
    return 1;
}

// Xbox 0x001430E0
void pop_runtime_attract_input(AttractControlServices& services)
{
    g_attract_input.axes = {};
    auto device = g_attract_input.device;
    switch (word(device, 0))
    {
    case 1:
    case 2:
        clear_runtime_input_states(device);
        break;
    case 4:
        reset_runtime_composite_input(device, services.input_release);
        break;
    case 5:
        clear_runtime_actor_input_states(device);
        break;
    }
    pop_runtime_input_entry(services.input);
    switch (word(device, 0))
    {
    case 1:
    case 2:
    case 3:
        release_runtime_simple_input(device);
        break;
    case 4:
        release_runtime_composite_input(device, services.input_release);
        break;
    case 5:
        release_runtime_actor_input(device, services.input_release);
        break;
    }
    services.input_views.erase(g_attract_input.address);
    services.input_addresses.erase(g_attract_input.address);
    g_attract_input.device = {};
    g_attract_input.address = 0;
}

// Xbox 0x001431B0
bool runtime_attract_start_pressed(AttractControlServices& services)
{
    const auto device = attract_device();
    const u32 accept = runtime_input_button_edge(device, 0x100, services.input_state);
    const u32 start = runtime_input_button_edge(device, 0x10, services.input_state);
    u16 mask;
    if (accept == 0)
    {
        if (start == 0)
        {
            return false;
        }
        mask = 0x10;
    }
    else
    {
        if (start != 0)
        {
            return false;
        }
        mask = 0x100;
    }
    switch (word(g_attract_input.device, 0))
    {
    case 3:
    case 4:
        g_attract_input.port = runtime_input_new_edge_port(mask, services.input);
        return g_attract_input.port != -1;
    }
    g_attract_input.port = 0;
    return true;
}
}
