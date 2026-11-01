#include "actor_effect_draw.h"
#include "world_sprite_passes.h"
#include "../camera/runtime_display_setup.h"
#include "../frontend/dialogue_draw_context.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace gbtg
{
namespace
{
float real(std::span<const u8> bytes, size_t offset)
{
    return std::bit_cast<float>(read_u32(bytes, offset));
}

// 0x001673D8 (_ftol): truncation toward zero; the caller keeps the low byte.
u32 truncated_byte(float value)
{
    return u32(static_cast<i32>(std::trunc(value))) & 0xFF;
}
}

// Xbox 0x000EFEA0
void draw_actor_effect_particle(ActorEffectNode& effect, size_t particle, BillboardQueue& queue)
{
    const std::span<const u8> bytes = effect.bytes;
    const size_t at = 0x390 + particle * 0x150;
    const auto field = [&bytes, at](size_t offset)
    {
        return real(bytes, at + offset);
    };
    const u32 color = truncated_byte(field(0x60)) | truncated_byte(field(0x64)) << 8
        | truncated_byte(field(0x68)) << 16 | truncated_byte(field(0x6C)) << 24;
    // Particle +0x148 is its effect node.
    const float scale = real(bytes, 0xB8) * field(0x48);
    if (scale == 0.f) // DAT_0032B100
    {
        return;
    }
    std::array<float, 2> size{scale, scale};
    const u16 flags = read_u16(bytes, at + 0x58);
    if ((flags & 4) != 0)
    {
        size = {field(0xE8) * scale, field(0xEC) * scale};
    }
    Vec3 spin{field(0xC), field(0x10), field(0x14)};
    if ((flags & 8) != 0)
    {
        spin = {spin.x + field(0x118), spin.y + field(0x12C), spin.z + field(0x140)};
    }
    const Vec3 position{field(0), field(4), field(8)};
    const u32 frame = u32(static_cast<i32>(field(0x100)));
    switch (read_u32(bytes, 0x13C))
    {
    case 0:
        queue_billboard(queue, effect.texture.data(), color, frame, position, size.data(), spin.z);
        return;
    case 2:
        throw Error("untranslated Xbox 0x0005CA10 (an effect's mode 2 sprite, 0x000EFEA0)");
    }
}

// Xbox 0x000F0010
void draw_actor_effect_sprite(ActorEffectNode& effect, size_t particle, RuntimeDisplaySetupContext& display)
{
    const std::span<const u8> bytes = effect.bytes;
    const size_t at = 0x390 + particle * 0x150;
    const auto field = [&bytes, at](size_t offset)
    {
        return real(bytes, at + offset);
    };
    const u32 color = truncated_byte(field(0x60)) | truncated_byte(field(0x64)) << 8
        | truncated_byte(field(0x68)) << 16 | truncated_byte(field(0x6C)) << 24;
    // Particle +0x148 is its effect node.
    const float scale = real(bytes, 0xB8) * field(0x48);
    if (scale == 0.f) // DAT_0032B100
    {
        return;
    }
    std::array<float, 2> size{scale, scale};
    const u16 flags = read_u16(bytes, at + 0x58);
    if ((flags & 4) != 0)
    {
        size = {field(0xE8) * scale, field(0xEC) * scale};
    }
    Vec3 euler{field(0xC), field(0x10), field(0x14)};
    if ((flags & 8) != 0)
    {
        euler = {euler.x + field(0x118), euler.y + field(0x12C), euler.z + field(0x140)};
    }
    const Vec3 position{field(0), field(4), field(8)};
    if (read_u32(bytes, 0x13C) == 2)
    {
        // CVTTSS2SI: truncation toward zero.
        const i32 frame = static_cast<i32>(field(0x100));
        draw_world_sprite_quad(frame, const_cast<u8*>(effect.texture.data()), &position, &euler, size.data(), color,
            display);
    }
}

// Xbox 0x000F8EA0
void draw_runtime_model_at(RuntimeModelInstance& model, u32 model_cpu, const Vec3* position, const Vec3* euler,
    const Vec3* scale, u32 kind, RuntimeDisplaySetupContext& display, SceneModelDrawContext& models)
{
    auto& draw = display.draw;
    // DAT_0050DD70 points at DAT_0050D7A0, reset to the identity; DAT_0050D7D0 (+12) is the scratch matrix.
    draw.current_affine = draw.affine.data();
    float* current = draw.current_affine;
    std::fill_n(current, 12, 0.f);
    current[0] = 1.f; // DAT_0032B0F8.
    current[5] = 1.f;
    current[10] = 1.f;
    if (position != nullptr)
    {
        float* translation = draw.affine.data() + 12;
        std::fill_n(translation, 12, 0.f);
        translation[0] = 1.f;
        translation[3] = position->x;
        translation[5] = 1.f;
        translation[7] = position->y;
        translation[10] = 1.f;
        translation[11] = position->z;
        multiply_affine_matrices(current, current, translation); // 0x001041B0
    }
    if (euler != nullptr)
    {
        rotate_dialogue_affine(0x79, euler->y, draw); // 0x00104160
        rotate_dialogue_affine(0x78, euler->x, draw);
        rotate_dialogue_affine(0x7A, euler->z, draw);
    }
    if (scale != nullptr)
    {
        float* scaling = draw.current_affine + 12;
        std::fill_n(scaling, 12, 0.f);
        scaling[0] = scale->x;
        scaling[5] = scale->y;
        scaling[10] = scale->z;
        multiply_affine_matrices(draw.current_affine, draw.current_affine, scaling); // 0x001041B0
    }
    // 0x000F9080 receives DAT_0050DD70 itself in EAX.
    draw_runtime_scene_model(model, model_cpu, *reinterpret_cast<const AnimationMatrix*>(draw.current_affine), kind,
        models);
}

// Xbox 0x000F8CF0, EAX position, ESI euler, EBX kind
void draw_runtime_model_scaled(RuntimeModelInstance& model, u32 model_cpu, const Vec3* position, const Vec3* euler,
    float scale, u32 kind, RuntimeDisplaySetupContext& display, SceneModelDrawContext& models)
{
    auto& draw = display.draw;
    // DAT_0050DD70 points at DAT_0050D7A0, reset to the identity; DAT_0050D7D0 (+12) is the scratch matrix.
    draw.current_affine = draw.affine.data();
    float* current = draw.current_affine;
    std::fill_n(current, 12, 0.f);
    current[0] = 1.f; // DAT_0032B0F8.
    current[5] = 1.f;
    current[10] = 1.f;
    if (position != nullptr)
    {
        float* translation = draw.affine.data() + 12;
        std::fill_n(translation, 12, 0.f);
        translation[0] = 1.f;
        translation[3] = position->x;
        translation[5] = 1.f;
        translation[7] = position->y;
        translation[10] = 1.f;
        translation[11] = position->z;
        multiply_affine_matrices(current, current, translation); // 0x001041B0
    }
    if (euler != nullptr)
    {
        rotate_dialogue_affine(0x79, euler->y, draw); // 0x00104160
        rotate_dialogue_affine(0x78, euler->x, draw);
        rotate_dialogue_affine(0x7A, euler->z, draw);
    }
    if (!(scale == 1.f)) // DAT_0032B0F8; unordered scales too.
    {
        float* scaling = draw.current_affine + 12;
        std::fill_n(scaling, 12, 0.f);
        scaling[0] = scale;
        scaling[5] = scale;
        scaling[10] = scale;
        multiply_affine_matrices(draw.current_affine, draw.current_affine, scaling); // 0x001041B0
    }
    // 0x000F9080 receives DAT_0050DD70 itself in EAX.
    draw_runtime_scene_model(model, model_cpu, *reinterpret_cast<const AnimationMatrix*>(draw.current_affine), kind,
        models);
}

// Xbox 0x000F0160
void draw_actor_effect_model(ActorEffectNode& effect, size_t particle, u32 model_cpu,
    RuntimeDisplaySetupContext& display, SceneModelDrawContext& models)
{
    const std::span<const u8> bytes = effect.bytes;
    const size_t at = 0x390 + particle * 0x150;
    const auto field = [&bytes, at](size_t offset)
    {
        return real(bytes, at + offset);
    };
    // Particle +0x148 is its effect node.
    const float size = real(bytes, 0xB8) * field(0x48);
    if (size == 0.f) // UCOMISS DAT_0032B100; JNP skips only an ordered equality.
    {
        return;
    }
    Vec3 scale{size, size, size};
    const u16 flags = read_u16(bytes, at + 0x58);
    if ((flags & 4) != 0)
    {
        scale = {field(0xE8) * size, field(0xEC) * size, field(0xF0) * size};
    }
    Vec3 euler{field(0xC), field(0x10), field(0x14)};
    if ((flags & 8) != 0)
    {
        euler = {euler.x + field(0x118), euler.y + field(0x12C), euler.z + field(0x140)};
    }
    const Vec3 position{field(0), field(4), field(8)};
    const u32 first = truncated_byte(field(0x60)) | truncated_byte(field(0x64)) << 8
        | truncated_byte(field(0x68)) << 16 | truncated_byte(field(0x6C)) << 24;
    const u32 level = truncated_byte(field(0x70));
    const u32 second = level | level << 8 | level << 16 | 0xFF000000u;
    if (effect.model == nullptr)
    {
        throw Error("0x000F0160 writes the material of an effect without its +0x158 model");
    }
    RuntimeModelInstance& model = *effect.model;
    model.state_1c0 = 1; // Model +0x1C0.
    model.state_1c4 = first; // +0x1C4.
    model.state_1c8 = second; // +0x1C8.
    draw_runtime_model_at(model, model_cpu, &position, &euler, &scale, 3, display, models); // 0x000F8EA0, EBX 3
}

// Xbox 0x000ED390
i32 draw_actor_effect_particles(ActorEffectNode& effect, BillboardQueue& queue, RuntimeDisplaySetupContext& display,
    SceneModelDrawContext& models, u32 model_cpu)
{
    const u32 type = read_u32(effect.bytes, 0x13C);
    if (type == 2)
    {
        begin_world_sprite_draw(display);
        set_world_sprite_depth_mode(read_u32(effect.bytes, 0x140), display);
    }
    i32 drawn = 0;
    const i32 count = static_cast<i16>(read_u16(effect.bytes, 2));
    u32 index = 0;
    if (0 < count)
    {
        do
        {
            if (static_cast<i16>(read_u16(effect.bytes, 0x390 + size_t(index & 0xFFFF) * 0x150 + 0x5A)) == -2)
            {
                switch (type)
                {
                case 0:
                    draw_actor_effect_particle(effect, size_t(index & 0xFFFF), queue);
                    break;
                case 2:
                    draw_actor_effect_sprite(effect, size_t(index & 0xFFFF), display);
                    break;
                case 4:
                    draw_actor_effect_model(effect, size_t(index & 0xFFFF), model_cpu, display, models); // 0x000F0160
                    break;
                }
                ++drawn;
            }
            ++index;
        }
        while (static_cast<i32>(index & 0xFFFF) < count);
    }
    if (type != 2)
    {
        return drawn;
    }
    bind_display_projection(display);
    if (--display.draw.override_count == 0)
    {
        display.draw.fog_disabled = 0;
    }
    return drawn;
}
}
