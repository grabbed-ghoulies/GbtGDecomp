#include "gbtg/actor_spawn_factory.h"

#include <cstring>

namespace gbtg
{
// Xbox 0x000584A0
FactoryObject create_dialogue_map_control(ObjectFactoryRegistry& factory,
    std::span<ExecutableResourcePool> resources)
{
    auto parameters = clone_executable_resource(resources,
        "aid_objparams_ghoulies_scenecontrol_mapview"); // DAT_00424880
    if (parameters.bytes != nullptr)
    {
        auto object = create_factory_object(factory, {parameters.bytes.get(), parameters.size});
        parameters.bytes.reset();
        return object;
    }
    return {};
}

// Xbox 0x0013B580
FactoryObject create_dialogue_button_bat(ObjectFactoryRegistry& factory,
    std::span<ExecutableResourcePool> resources, u8 configuration_flags,
    const std::array<float, 2>& position, float scale, float depth)
{
    const char* name = "aid_objparams_ghoulies_misc_buttonbat";
    if ((configuration_flags & 0x40) != 0)
    {
        name = "aid_objparams_ghoulies_misc_buttonbatgold";
    }
    auto parameters = clone_executable_resource(resources, name);
    std::memcpy(parameters.bytes.get() + 0x334, position.data(), sizeof(position));
    const float vertical_shift = scale * 21.f;
    const float horizontal = read_f32({parameters.bytes.get(), parameters.size}, 0x334) - scale * 2.f;
    const float vertical = read_f32({parameters.bytes.get(), parameters.size}, 0x338) - vertical_shift;
    std::memcpy(parameters.bytes.get() + 0x334, &horizontal, 4);
    std::memcpy(parameters.bytes.get() + 0x338, &vertical, 4);
    if (depth != -1.f)
    {
        std::memcpy(parameters.bytes.get() + 0x33C, &depth, 4);
    }
    auto object = create_factory_object(factory, {parameters.bytes.get(), parameters.size});
    parameters.bytes.reset();
    return object;
}

// Xbox 0x0003DF40
FactoryObject create_resource_object_with_data(ObjectFactoryRegistry& factory,
    std::span<ExecutableResourcePool> resources, const char* name, u32 data)
{
    auto parameters = clone_executable_resource(resources, name);
    if (parameters.bytes == nullptr)
    {
        return {};
    }
    std::memcpy(parameters.bytes.get() + 8, &data, 4);
    auto object = create_factory_object(factory, {parameters.bytes.get(), parameters.size});
    parameters.bytes.reset();
    return object;
}

namespace
{
// The DAT_0051108C record whose +6 word is the key; null when none matches.
const u8* find_scene_marker_record(const u8* scene_records, i16 key)
{
    u32 count;
    std::memcpy(&count, scene_records, 4);
    const u8* record = scene_records + 8;
    for (u32 index = 0; index < count; ++index)
    {
        i16 record_key;
        std::memcpy(&record_key, record + 6, 2);
        if (record_key == key)
        {
            return record;
        }
        u32 size;
        std::memcpy(&size, record, 4);
        record += size;
    }
    return nullptr;
}
}

// Xbox 0x0011F260
void read_scene_marker_position(const u8* scene_records, i16 key, Vec3& output)
{
    if (const u8* record = find_scene_marker_record(scene_records, key))
    {
        std::memcpy(&output, record + 0xC, 12);
    }
}

// Xbox 0x0011F2A0
void read_scene_marker_euler(const u8* scene_records, i16 key, Vec3& output)
{
    if (const u8* record = find_scene_marker_record(scene_records, key))
    {
        std::memcpy(&output, record + 0x18, 12);
    }
}

// Xbox 0x0011F2E0
u16 find_scene_spawn_type(const u8* scene_records, i16 key)
{
    u32 count;
    std::memcpy(&count, scene_records, 4);
    const u8* record = scene_records + 8;
    u32 index = 0;
    if (count != 0)
    {
        do
        {
            i16 record_key;
            std::memcpy(&record_key, record + 6, 2);
            if (record_key == key)
            {
                u16 type;
                std::memcpy(&type, record + 8, 2);
                return type;
            }
            ++index;
            u32 size;
            std::memcpy(&size, record, 4);
            record += size;
        }
        while (index < count);
    }
    return 0;
}

// Xbox 0x001347C0
FactoryObject create_spawn_actor(ObjectFactoryRegistry& factory,
    std::span<ExecutableResourcePool> resources, const u8* scene_records,
    const char* name, const Vec3& position, const Vec3& euler, float scale,
    const char* variant, const char* attributes, const char* second_override,
    const char* fourth_override, const char* fifth_override, i16 scene_key)
{
    if (std::memcmp(name, "aid_", 4) != 0)
    {
        return {};
    }
    auto parameters = clone_executable_resource(resources, name);
    if (parameters.bytes == nullptr)
    {
        return {};
    }
    u8* bytes = parameters.bytes.get();
    std::memcpy(bytes + 8, &position, 12);
    std::memcpy(bytes + 0x14, &euler, 12);
    std::memcpy(bytes + 0x20, &scale, 4);
    const u16 type = scene_key == 0 ? 0 : find_scene_spawn_type(scene_records, scene_key);
    std::memcpy(bytes + 0x2AC, &type, 2);
    std::memcpy(bytes + 0x2AA, &scene_key, 2);
    if (variant != nullptr)
    {
        std::strcpy(reinterpret_cast<char*>(bytes + 0x3BA), variant);
    }
    if (attributes != nullptr)
    {
        std::strcpy(reinterpret_cast<char*>(bytes + 0x33A), attributes);
    }
    if (second_override != nullptr)
    {
        std::strcpy(reinterpret_cast<char*>(bytes + 0x4BA), second_override);
    }
    if (fourth_override != nullptr)
    {
        std::strcpy(reinterpret_cast<char*>(bytes + 0x53A), fourth_override);
    }
    if (fifth_override != nullptr)
    {
        std::strcpy(reinterpret_cast<char*>(bytes + 0x5BA), fifth_override);
    }
    auto object = create_factory_object(factory, {bytes, parameters.size});
    parameters.bytes.reset();
    return object;
}

// Xbox 0x001348F0
FactoryObject create_spawn_actor_unit_scale(ObjectFactoryRegistry& factory,
    std::span<ExecutableResourcePool> resources, const u8* scene_records,
    const char* name, const Vec3& position, const Vec3& euler,
    const char* variant, const char* attributes, const char* second_override,
    const char* fourth_override, const char* fifth_override)
{
    return create_spawn_actor(factory, resources, scene_records, name, position, euler,
        1.f, fourth_override, fifth_override, variant, second_override, attributes, 0);
}
}
