#pragma once
#include "dialogue_widget_construction.h"
#include "dialogue_widget_bat.h"
#include "dialogue_page_setup.h"
#include "input_activation.h"
#include "../audio/listener_settings.h"
#include "dialogue_widget_update.h"

namespace gbtg
{
struct CutsceneRuntimeContext;
struct RuntimeCutsceneInstance;
struct AudioEventVoices;
struct RuntimeDialogueQueueEntry
{
    std::array<char16_t, 0x800> text; // DAT_004E61D0, stride 0x1002
    u8 flag; // DAT_004E71D0
};
struct RuntimeDialogueActivationState
{
    std::array<RuntimeDialogueQueueEntry, 8> entries;
    u32 read; // DAT_004EE1E0
    u32 write; // DAT_004EE1E4
    u32 count; // DAT_004EE1E8
    i32 mode; // DAT_004EE1EC
    float duration; // DAT_004EE1F0
    float elapsed; // DAT_004EE1F4
    std::span<u8> input; // DAT_004EE1F8
    u32 input_address; // Native pointer mapping for DAT_004EE1F8
    i32 button; // DAT_004EE1FC
    u32 visible; // DAT_004EE200
    float progress; // DAT_004EE204
    RuntimeCutsceneInstance* cutscene; // DAT_004EE22C
    EntityCamNode camera; // DAT_004EE230
    u32 camera_selected; // DAT_004EE2A0
    u32 completion; // DAT_004EE2A4
    RuntimeDialogueWidget* widget; // DAT_004EE2A8
    i32 saved_reverb; // DAT_004EE2AC
    u32 map_retained; // DAT_004EE2B4
    float speaker_timer; // DAT_004E60BC
};
struct RuntimeDialogueActivationContext
{
    RuntimeDialogueActivationState& state;
    u32& scene_flags;
    u32& pause_count; // DAT_00510CEC
    u32& frontend_pause_count; // DAT_004E173C
    u32& random;
    u8 mode_flags;
    u16 viewport_width;
    float viewport_bottom;
    float wrap_width; // DAT_0050973C
    u32 first_category;
    u32 second_category;
    const char* camera_name; // DAT_00323C2C
    const char* page_names; // DAT_002FEAB8, stride 0x80
    const u8* prompt_parameters; // DAT_002FEB50
    char* replacement_source;
    char* replacement_target;
    DialogueWidgetAllocationContext& allocation;
    DialogueWidgetBatFactoryContext& bats;
    DialoguePageLayout& page;
    DialoguePageSettings& settings;
    DialogueMeasurementContext& measurement;
    RuntimeInputActivationContext& input;
    RuntimeInputReleaseContext& input_release;
    AudioListenerSettings& listener;
    AudioEventVoices& voices;
    CutsceneRuntimeContext& cutscenes;
    DialogueWidgetUpdateContext& widget_update;
    std::unordered_map<u32, std::span<u8>>& input_addresses;
    std::unordered_map<u32, RuntimeDialogueInputDevice>& input_views;
    u32 frame; // DAT_004DEA58
    float freeze; // DAT_004312DC
    float dt; // DAT_005096C0
    float scene_scale; // DAT_004312C4
};
void set_runtime_dialogue_mode(i32 mode, RuntimeDialogueActivationContext& context); // Xbox 0x000584D0
u32 enqueue_runtime_dialogue(const char16_t* text, u8 flag,
    RuntimeDialogueActivationContext& context); // Xbox 0x000582E0
u32 enqueue_runtime_dialogue_key(const char* key, u8 flag,
    RuntimeDialogueActivationContext& context); // Xbox 0x000582A0
void close_runtime_dialogue(RuntimeDialogueActivationContext& context); // Xbox 0x000583A0
void accept_runtime_dialogue_button(RuntimeDialogueActivationState& state); // Xbox 0x00058B80
void advance_runtime_dialogue_page(RuntimeDialogueActivationContext& context); // Xbox 0x00058AE0
void update_runtime_dialogue(RuntimeDialogueActivationContext& context); // Xbox 0x00057E60
u32 advance_runtime_dialogue_timer(float& timer, float freeze, float dt,
    float scale); // Xbox 0x0012B2A0
void update_runtime_dialogue_speaker(DialogueOwnedResources& state, float& timer,
    u32& random, float freeze, float dt, float scale); // Xbox 0x00057450
}
