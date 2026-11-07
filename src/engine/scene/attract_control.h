#pragma once
#include <array>
#include "scene_control_fields.h"
#include "cutscene_scene_runtime.h"
#include "gbtg/object_factory.h"
#include "gbtg/resource_error.h"
#include "../frontend/dialogue_input.h"
#include "../frontend/dialogue_text_pool.h"
#include "../frontend/input_activation.h"
#include "../../gbtg/text/localtext_resources.h"

namespace gbtg
{
struct GhoulySpawnPool;
struct RuntimeTextMetrics;
struct RuntimeModelInstance;
// The attract banner's input mode: the axes DAT_00546180..DAT_0054619C, the keyboard device DAT_005461A0 (pushed
// as "gameselect", 0x003241E0) and the port DAT_004340E8 that confirmed Start.
struct RuntimeAttractInput
{
    std::array<float, 8> axes{}; // DAT_00546180
    std::span<u8> device; // DAT_005461A0
    u32 address = 0; // Native pointer mapping for DAT_005461A0.
    i32 port = 0; // DAT_004340E8
};
extern RuntimeAttractInput g_attract_input;
// What the attract control reaches outside its object: the scene's resources, text, input, cutscene stack and
// actors. SceneActorServices binds it when the native scene starts.
struct AttractControlServices
{
    AnimationResourceRegistry& resources;
    ResourceErrorState& error;
    LocalTextResourceContext& text;
    GhoulySpawnPool*& text_pools; // The 0x0005F390 registry.
    const RuntimeTextMetrics& metrics;
    float viewport_width;
    i32& next_text_id;
    u32& random; // DAT_0054DD7C
    const float& banner_x; // DAT_00509730
    const float& banner_bottom; // DAT_0050972C
    std::span<const u8> font; // DAT_00504444; +0x10 is its cell height.
    i32& pause_count; // DAT_00510CEC
    RuntimeInputActivationContext& input;
    RuntimeInputReleaseContext& input_release;
    std::unordered_map<u32, std::span<u8>>& input_addresses;
    std::unordered_map<u32, RuntimeDialogueInputDevice>& input_views;
    RuntimeDialogueInputState& input_state;
    const u32& frame; // DAT_004DEA58
    SceneCutsceneRuntime& cutscenes; // DAT_004E1B40.. and the clips' track views and actor rows.
    ModelNodeFlagCache& node_cache; // DAT_005078D8
    std::function<RuntimeModelInstance*(Actor&)> secondary_model; // Actor +0x310.
    std::function<void(const char*)> request_script; // 0x0011EB30, mode 1.
    std::function<void()> start_laugh; // 0x0013BAF0
    i32 language; // DAT_0042A710
    std::span<const u8> bubble_nodes; // DAT_004275DC, stride 4
    const float& freeze; // DAT_004312DC
    const float& dt; // DAT_005096C0
    const float& scene_scale; // DAT_004312C4
};
// Native state of an attract control (factory type 0x9B) whose retail words are pointers.
struct AttractControlState
{
    u32 clip = 0; // +0x24, the playing row of clips
    std::array<char16_t, 0x100> banner_text{}; // +0x28, the localized "StartBanner"
    RuntimeDialogueTextPool* banner_pool = nullptr; // +0x228
    u32 banner = 0; // +0x22C, the text pool's id for the banner
    EntityCamNode camera{}; // +0x230
    float banner_time = 0.f; // +0x290, seconds until the banner shows again
    u32 started = 0; // +0x294, the start press was taken
    std::span<const u8> clips; // +0x298, the aidlist resource: rows of 0x80-byte cutscene names
    RuntimeCutsceneInstance* cutscene = nullptr; // +0x29C
    u32 clip_count = 0; // +0x2A0
    std::array<char, 0x80> next_script{}; // +0x2A4, the parameters' +0x108: requested when the clips run out
};
void attract_control_object_sizes(ObjectSizes& output); // Xbox 0x00080950
bool construct_runtime_attract_control(std::span<const u8> parameters,
    AttractControlState& control, AttractControlServices& services); // Xbox 0x00080A30
void destroy_runtime_attract_control(AttractControlState& control,
    AttractControlServices& services); // Xbox 0x00080BB0
void tick_runtime_attract_control(SceneControlBase& base, AttractControlState& control,
    AttractControlServices& services); // Xbox 0x00080C10
void show_runtime_attract_banner(AttractControlState& control,
    AttractControlServices& services); // Xbox 0x000809C0, ESI state
void accept_runtime_attract_start(AttractControlState& control, AttractControlServices& services); // Xbox 0x00080CB0
void advance_runtime_attract_clip(AttractControlState& control,
    AttractControlServices& services); // Xbox 0x00080D20, ESI state
void show_runtime_attract_bubbles(AttractControlState& control,
    AttractControlServices& services); // Xbox 0x00080DA0
u32 push_runtime_attract_input(AttractControlServices& services); // Xbox 0x00143070
void pop_runtime_attract_input(AttractControlServices& services); // Xbox 0x001430E0
bool runtime_attract_start_pressed(AttractControlServices& services); // Xbox 0x001431B0
}
