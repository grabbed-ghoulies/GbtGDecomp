#pragma once
#include "gbtg/object_factory.h"
#include "gbtg/anim.h"

namespace gbtg
{
struct RuntimePlayerSpawnContext
{
    ObjectFactoryRegistry& factory;
    std::span<ExecutableResourcePool> resources;
    const char* scene_name; // DAT_00510E58.
    std::span<const u8, 0x210> scene_overrides; // DAT_00430DA0, four rows of 0x84.
    const u8& scene_flags; // **(DAT_00506C74+8).
    const u32& runtime_flags; // DAT_0050EA9C.
    const char* fallback_override; // DAT_00430D14.
};
u32 find_player_scene_override(const char* scene,
    std::span<const u8, 0x210> overrides); // Xbox 0x00123A30
const char* select_player_parameter_name(const char* scene,
    std::span<const u8, 0x210> overrides, u8 scene_flags, u32 runtime_flags); // Xbox 0x00123A80
FactoryObject create_runtime_player_actor(const char* override_name, const Vec3& position,
    const Vec3& euler, RuntimePlayerSpawnContext& context); // Xbox 0x00134920
}
