#pragma once
#include "player_children.h"
#include "../scripts/script_table_lookup.h"
#include "gbtg/entity.h"
#include "gbtg/sound_runtime.h"

namespace gbtg
{
struct RuntimePlayerIntroSound
{
    u32& enabled; // DAT_00433BE8.
    float& scale; // DAT_00433BEC.
    i16& voice; // DAT_00433BF0.
    const u8& configuration; // **(DAT_00506C6C+8).
    RuntimeActorSoundQueues& sound;
};
void reset_runtime_player_intro_sound(RuntimePlayerIntroSound& state); // Xbox 0x001307D0
u32 runtime_actor_case_class(std::span<const u8> link, u16 key); // Xbox 0x00145FF0
struct RuntimePlayerCameraSource
{
    Actor& actor; // body +8.
    std::span<const u8> body; // actor +0x95C.
    std::span<const u8> case_link; // body +0x48; empty maps null.
};
struct RuntimePlayerCameraContext
{
    ObjectFactoryRegistry& factory;
    std::span<ExecutableResourcePool> resources;
    const char* parameters; // string at 0x00314E50.
    std::span<const u8> offsets; // table at 0x00314EB0.
    const u32& runtime_flags; // DAT_0050EA9C.
    const i32& player_count; // DAT_00545AF0.
    // Required genuine camera vtable +0xC mapping. Payload: id, zero, actor token.
    std::function<void(FactoryObject&, const std::array<u32, 3>&)> message;
};
void build_runtime_player_camera_target(std::span<u8, 0x20> output,
    RuntimePlayerCameraSource& source, RuntimePlayerCameraContext& context); // Xbox 0x0013D000
FactoryObject create_runtime_player_camera(RuntimePlayerCameraSource* source,
    u32 actor_address, RuntimePlayerCameraContext& context); // Xbox 0x0013CCD0
struct RuntimePlayerMindSetup
{
    Actor& actor; // mind +8, same actor used by camera source.
    std::span<u8> actor_bytes;
    std::span<u8> body_bytes; // actor +0x95C.
    const RuntimeScriptLookupTable* animation_table; // body +0x73C.
    u32 actor_address;
    const u8& scene_flags; // **(DAT_00506C74+8).
    const u32& runtime_flags; // DAT_0050EA9C.
    const u32& story_flags; // DAT_0050EA98.
    const i32& health; // DAT_00430D10.
    const i32& chapter_mode; // DAT_00430D0C.
    const char* scene_name; // DAT_00510E58.
    RuntimePlayerIntroSound& intro_sound;
    RuntimePlayerCameraSource& camera_source;
    RuntimePlayerCameraContext& camera_context;
    // Retains the created object and its real scene registration until teardown.
    std::vector<FactoryObject>& cameras;
};
bool initialize_runtime_player_mind(std::span<u8> mind, std::span<const u8> parameters,
    RuntimePlayerMindSetup& context); // Xbox 0x0007EF10
}
