#include "player_spawn.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x00123A30
u32 find_player_scene_override(const char* scene, std::span<const u8, 0x210> overrides)
{
    u32 offset = 0;
    do
    {
        if (resource_names_match(reinterpret_cast<const char*>(overrides.data() + offset), scene))
        {
            return read_u32(overrides, offset + 0x80);
        }
        offset += 0x84;
    }
    while (offset < 0x210);
    return 0;
}

// Xbox 0x00123A80
const char* select_player_parameter_name(const char* scene,
    std::span<const u8, 0x210> overrides, u8 scene_flags, u32 runtime_flags)
{
    u32 kind = find_player_scene_override(scene, overrides);
    if (kind == 0)
    {
        if ((scene_flags & 8) == 0 && (runtime_flags & 0x200000) == 0)
        {
            kind = 1;
        }
        else
        {
            kind = 2;
        }
    }
    if (kind != 1)
    {
        if (kind == 2)
        {
            return "aid_objparams_ghoulies_actor_player_girl";
        }
        if (kind == 3)
        {
            return "aid_objparams_ghoulies_actor_cookplayable";
        }
    }
    return "aid_objparams_ghoulies_actor_player_boy";
}

// Xbox 0x00134920
FactoryObject create_runtime_player_actor(const char* override_name, const Vec3& position,
    const Vec3& euler, RuntimePlayerSpawnContext& context)
{
    if (override_name == nullptr || std::memcmp(override_name, "aid_", 4) != 0)
    {
        override_name = context.fallback_override;
    }
    const char* name = select_player_parameter_name(context.scene_name, context.scene_overrides,
        context.scene_flags, context.runtime_flags);
    if (std::memcmp(name, "aid_", 4) != 0)
    {
        return {};
    }
    auto parameters = clone_executable_resource(context.resources, name);
    FactoryObject output;
    if (parameters.bytes != nullptr)
    {
        std::memcpy(parameters.bytes.get() + 8, &position.x, 4);
        std::memcpy(parameters.bytes.get() + 0xC, &position.y, 4);
        std::memcpy(parameters.bytes.get() + 0x10, &position.z, 4);
        std::memcpy(parameters.bytes.get() + 0x14, &euler.x, 4);
        std::memcpy(parameters.bytes.get() + 0x18, &euler.y, 4);
        std::memcpy(parameters.bytes.get() + 0x1C, &euler.z, 4);
        const float scale = 1.f; // DAT_0032B0F8.
        std::memcpy(parameters.bytes.get() + 0x20, &scale, 4);
        const u16 zero = 0;
        std::memcpy(parameters.bytes.get() + 0x2AA, &zero, 2);
        std::memcpy(parameters.bytes.get() + 0x2AC, &zero, 2);
        if (override_name != nullptr)
        {
            std::strcpy(reinterpret_cast<char*>(parameters.bytes.get() + 0x4BA), override_name);
        }
        output = create_factory_object(context.factory, {parameters.bytes.get(), parameters.size});
        parameters.bytes.reset(); // 0x000663E0, paired host heap release.
    }
    return output;
}
}
