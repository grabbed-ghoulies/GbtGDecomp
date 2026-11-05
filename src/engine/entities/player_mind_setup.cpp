#include "player_mind_setup.h"
#include <cstddef>
#include "body_fields.h"
#include "actor_transform.h"
#include "../scripts/script_chapter.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void write_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
void write_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

// Xbox 0x001307D0
void reset_runtime_player_intro_sound(RuntimePlayerIntroSound& state)
{
    state.enabled = 1;
    state.scale = 1.f;
    if ((state.configuration & 0x40) == 0)
    {
        state.scale = 2.f;
    }
    if (state.voice != -1)
    {
        stop_actor_sound_voice(state.voice, state.sound); // 0x00039690
        state.voice = -1;
    }
}

// Xbox 0x00145FF0
u32 runtime_actor_case_class(std::span<const u8> link, u16 key)
{
    const i16 count = std::bit_cast<i16>(read_u16(link, 0x944));
    i32 index = 0;
    size_t row = 0x948;
    if (count > 0)
    {
        do
        {
            if (read_u16(link, row) == key)
            {
                return read_u16(link, row + 4);
            }
            row += 0x10;
            ++index;
        }
        while (index < count);
    }
    if (count != 0)
    {
        return read_u16(link, 0x94C);
    }
    return 0;
}

// Xbox 0x0013D000
void build_runtime_player_camera_target(std::span<u8, 0x20> output,
    RuntimePlayerCameraSource& source, RuntimePlayerCameraContext& context)
{
    const Vec3 position = actor_world_position(source.actor);
    u32 state = 0;
    u32 kind = 0;
    if (!source.case_link.empty())
    {
        state = source.case_link[0x93D];
        kind = runtime_actor_case_class(source.case_link, source.actor.class_key);
    }
    float height = 0.f;
    float radius = 0.f;
    if (state > 0 && state < 5 && kind > 0 && kind < 4)
    {
        const size_t offset = (kind + state * 3) * 8;
        height = read_f32(context.offsets, offset);
        radius = read_f32(context.offsets, offset + 4);
    }
    if ((context.runtime_flags & 0x10) != 0 && context.player_count < 4)
    {
        height += 10.f;
    }
    output[0] = 4;
    output[1] = 0x20;
    output[2] = 1;
    write_float(output, 4, position.x);
    write_float(output, 8, position.y);
    write_float(output, 0xC, position.z);
    write_float(output, 0x10, 0.f);
    write_float(output, 0x14, source.actor.radius_y * body_record(source.body).height_scale + height);
    write_float(output, 0x18, 0.f);
    write_float(output, 0x1C, source.actor.radius_xz * body_record(source.body).radius_scale + radius);
}

// Xbox 0x0013CCD0
FactoryObject create_runtime_player_camera(RuntimePlayerCameraSource* source,
    u32 actor_address, RuntimePlayerCameraContext& context)
{
    Vec3 position;
    float pitch;
    float yaw;
    if (source == nullptr)
    {
        position = {};
        pitch = 0.f;
        yaw = 0.f;
    }
    else
    {
        std::array<u8, 0x20> target;
        build_runtime_player_camera_target(target, *source, context);
        const Vec3 euler = actor_world_euler(source->actor);
        const float target_y = read_f32(target, 8) + read_f32(target, 0x1C) * 0.5f;
        constexpr float distance = std::bit_cast<float>(0x422AC28Fu);
        position.x = static_cast<float>(static_cast<long double>(read_f32(target, 4))
            - retail_sin(euler.y) * static_cast<long double>(distance));
        position.y = target_y + std::bit_cast<float>(0x4178A3D7u);
        position.z = static_cast<float>(static_cast<long double>(read_f32(target, 0xC))
            - retail_cos(euler.y) * static_cast<long double>(distance));
        float direction_yaw;
        float direction_pitch;
        direction_to_yaw_pitch(read_f32(target, 4) - position.x,
            target_y - position.y, read_f32(target, 0xC) - position.z,
            direction_yaw, direction_pitch);
        pitch = 0.f - direction_pitch;
        yaw = direction_yaw + std::bit_cast<float>(0x40490FDBu);
    }
    auto clone = clone_executable_resource(context.resources, context.parameters);
    FactoryObject camera;
    if (clone.bytes != nullptr)
    {
        std::span<u8> parameters{clone.bytes.get(), clone.size};
        write_float(parameters, 8, position.x);
        write_float(parameters, 0xC, position.y);
        write_float(parameters, 0x10, position.z);
        write_float(parameters, 0x14, pitch);
        write_float(parameters, 0x18, yaw);
        write_float(parameters, 0x1C, 0.f);
        write_float(parameters, 0x20, 1.f);
        const u16 zero = 0;
        std::memcpy(parameters.data() + 0x2AA, &zero, 2);
        std::memcpy(parameters.data() + 0x2AC, &zero, 2);
        camera = create_factory_object(context.factory, parameters);
        clone.bytes.reset();
    }
    if (source != nullptr)
    {
        // The word after 0xBA is stored as 0, then overwritten with ESI (the source actor) before the call.
        context.message(camera, {0xBA, actor_address});
    }
    return camera;
}

// Xbox 0x0007EF10
bool initialize_runtime_player_mind(std::span<u8> mind, std::span<const u8> parameters,
    RuntimePlayerMindSetup& context)
{
    i32 health;
    if ((context.scene_flags & 8) == 0 && (context.runtime_flags & 0x200000) == 0)
    {
        health = context.health;
    }
    else
    {
        health = 10;
    }
    if (health > 0)
    {
        const u16 value = static_cast<u16>(health);
        body_record(context.body_bytes).energy = value;
        body_record(context.body_bytes).max_energy = value;
        if (script_chapter_index(context.scene_name) < 9 && (context.scene_flags & 8) == 0
            && (context.runtime_flags & 0x200000) == 0 && (context.story_flags & 0x40) == 0
            && context.chapter_mode != 1)
        {
            reset_runtime_player_intro_sound(context.intro_sound);
        }
    }
    write_word(context.actor_bytes, 0xCE0, 1);
    u32 found = 0;
    if (context.animation_table != nullptr)
    {
        u32 output; // Lookup writes the retail scratch argument for a nonnull table.
        found = find_runtime_script_table_row(context.animation_table, 0x2C, output) != nullptr;
    }
    write_word(mind, 0x144, found);
    for (size_t offset : {0x164, 0x160, 0x16C, 0x168, 0x170, 0x17C, 0x178,
        0x184, 0x180, 0x188})
    {
        write_word(mind, offset, 0);
    }
    write_word(mind, 0x174, 1);
    write_word(mind, 0x18C, 1);
    write_float(mind, 0x12C, 1.f);
    write_word(mind, 0x80, read_u32(parameters, 0xA0));
    auto camera = create_runtime_player_camera(&context.camera_source, context.actor_address,
        context.camera_context);
    context.cameras.push_back(std::move(camera));
    write_word(mind, 0x98, 1);
    for (size_t offset : {0xA0, 0xA4, 0x9C, 0x94, 0xB0, 0xA8, 0xAC})
    {
        write_word(mind, offset, 0);
    }
    std::memset(mind.data() + 0xB8, 0, 0x1A * 4);
    write_word(mind, 0xC8, context.actor_address);
    const u16 zero = 0;
    std::memcpy(mind.data() + 0xCE, &zero, 2);
    mind[0xCC] = 3;
    context.actor_bytes[0x331] = 1;
    return true;
}
}
