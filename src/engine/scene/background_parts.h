#pragma once
#include "background_volumes.h"
#include "../graphics/actor_effect_stream.h"

namespace gbtg
{
struct BackgroundPartDispatch
{
    std::span<const u8> settings; // DAT_00546294, stride 0x22C.
    i32& setting_count; // DAT_00546298.
    i32& sound_slot; // DAT_00546290.
    std::span<const u8, 5> sound_groups; // DAT_002FD200.
    RuntimeActorSoundQueues& sounds;
    ActorEffectStreamContext& effects;
};
void dispatch_background_part_effects(Actor& actor, const char* name,
    const Vec3& position, const Vec3& source,
    BackgroundPartDispatch& context); // Xbox 0x00044D20
void advance_background_part_stage(Actor& actor, RuntimeModelInstance* secondary,
    std::span<u8> rows, i32 row, i32 part, std::span<const u8> header,
    const Vec3& position, const Vec3& source,
    std::span<u32> shared_states, BackgroundPartDispatch& context); // Xbox 0x000450A0
i32 find_active_background_part(std::span<const u8> rows, i32 part); // Xbox 0x00044EA0
bool background_part_can_be_destroyed(const Actor& actor, std::span<const u8> rows,
    i32 part, std::span<const u8> zero); // Xbox 0x00045CB0
u32 background_part_settings_value(const char* name, std::span<const u8> settings,
    i32 count); // Xbox 0x00044E70
i32 find_background_part_settings(const char* name, std::span<const u8> settings,
    i32 count); // Xbox 0x00044C20
const char* background_part_rumble(const Actor& actor, i32 part,
    std::span<const u8> settings, i32 count, const char* fallback,
    std::span<const u8> zero); // Xbox 0x00046470
void prepare_background_volume_navigation(const Actor& actor,
    std::span<BackgroundAnimationVolume> records); // Xbox 0x00045EC0
}
