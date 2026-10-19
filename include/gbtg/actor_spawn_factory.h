#pragma once

#include "gbtg/anim.h"
#include "gbtg/object_factory.h"

namespace gbtg
{
FactoryObject create_dialogue_map_control(ObjectFactoryRegistry& factory,
    std::span<ExecutableResourcePool> resources); // Xbox 0x000584A0
FactoryObject create_dialogue_button_bat(ObjectFactoryRegistry& factory,
    std::span<ExecutableResourcePool> resources, u8 configuration_flags,
    const std::array<float, 2>& position, float scale, float depth); // Xbox 0x0013B580
FactoryObject create_resource_object_with_data(ObjectFactoryRegistry& factory,
    std::span<ExecutableResourcePool> resources, const char* name, u32 data); // Xbox 0x0003DF40
u16 find_scene_spawn_type(const u8* scene_records, i16 key); // Xbox 0x0011F2E0
// DI key, ESI output: the marker record's position (+0xC) or euler (+0x18); an unmatched key leaves it unchanged.
void read_scene_marker_position(const u8* scene_records, i16 key, Vec3& output); // Xbox 0x0011F260
void read_scene_marker_euler(const u8* scene_records, i16 key, Vec3& output); // Xbox 0x0011F2A0
FactoryObject create_spawn_actor(ObjectFactoryRegistry& factory,
    std::span<ExecutableResourcePool> resources, const u8* scene_records,
    const char* name, const Vec3& position, const Vec3& euler, float scale,
    const char* variant, const char* attributes, const char* second_override,
    const char* fourth_override, const char* fifth_override, i16 scene_key); // Xbox 0x001347C0
FactoryObject create_spawn_actor_unit_scale(ObjectFactoryRegistry& factory,
    std::span<ExecutableResourcePool> resources, const u8* scene_records,
    const char* name, const Vec3& position, const Vec3& euler,
    const char* variant, const char* attributes, const char* second_override,
    const char* fourth_override, const char* fifth_override); // Xbox 0x001348F0
}
