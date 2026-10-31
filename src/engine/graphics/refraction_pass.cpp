#include "refraction_pass.h"
#include "../audio/listener_settings.h"
#include "gbtg/texture.h"

#include <bit>
#include <cmath>
#include <vector>

namespace gbtg
{
RuntimeSceneRefraction g_scene_refraction{};
u32 g_crt_random_seed = 1;

// Xbox 0x00166C29
i32 runtime_crt_rand(u32& seed)
{
    seed = seed * 214013u + 2531011u;
    return i32((seed >> 16) & 0x7FFF);
}

// Xbox 0x0011B7A0
const u8* find_runtime_refraction_mesh(std::span<const u8> model)
{
    if (model.empty())
    {
        return nullptr;
    }
    const u32 count = read_u32(model, 4);
    const u32 table = read_u32(model, 0);
    for (u32 index = 0; index < count; ++index)
    {
        if (read_u32(model, table + index * 8) == 0x13)
        {
            const u32 offset = read_u32(model, table + index * 8 + 4);
            return offset != 0 ? model.data() + offset : nullptr;
        }
    }
    return nullptr;
}

// Xbox 0x0011B7F0
void build_runtime_refraction_quad(RuntimeSceneRefraction& refraction, u16 width, u16 height)
{
    constexpr float tenth = std::bit_cast<float>(0x3DCCCCCDu); // DAT_0032B254
    const float wide = float(u32(width)) * tenth;
    const float high = float(u32(height)) * tenth;
    for (u32 corner = 0; corner < 4; ++corner)
    {
        // Corners 0..3: (0, 0), (1, 0), (1, 1), (0, 1).
        const float across = corner == 0 || corner == 3 ? 0.f : 1.f; // DAT_0032B0F8
        const float down = corner == 0 || corner == 1 ? 0.f : 1.f;
        const float x = across * float(u32(width));
        const float y = down * float(u32(height));
        float* record = refraction.quad.data() + corner * 12;
        record[0] = x;
        record[1] = y;
        record[2] = 0.f;
        record[3] = 1.f;
        record[4] = refraction.drift_u + across;
        record[5] = refraction.drift_v + down;
        record[6] = wide;
        record[7] = 0.f;
        record[8] = x;
        record[9] = 0.f;
        record[10] = high;
        record[11] = y;
    }
}

// Xbox 0x0011B8F0
void create_runtime_refraction_noise(RuntimeSceneRefraction& refraction,
    const std::function<u32(u32, u32, u32, u32, u32, u32)>& create_texture,
    const std::function<std::span<u8>(u32)>& texels, u32& seed)
{
    refraction.noise = create_texture(0x80, 0x80, 1, 0, 6, 3); // D3DDevice_CreateTexture2, A8R8G8B8
    std::vector<u8> linear(0x10000); // 0x000F72D0(0x10000)
    u8* pixel = linear.data();
    for (i32 row = 0; row < 0x80; ++row)
    {
        const long double angle = (long double)row * std::bit_cast<float>(0x3C000000u)
            * std::bit_cast<float>(0x427B53D2u); // DAT_0032B32C, DAT_0032B328
        const u8 sine = u8(truncate_runtime_float(std::sin(angle) * -1.5L)); // DAT_0032B320, 0x001673D8
        const u8 cosine = u8(truncate_runtime_float(std::cos(angle) * -5.0L)); // DAT_0032B318
        for (u32 column = 0; column < 0x80; ++column)
        {
            pixel[0] = 0xFF;
            pixel[1] = 0x7F;
            pixel[2] = 0x7F;
            pixel[3] = 0x7F;
            const u8 first = u8(runtime_crt_rand(seed) % 3); // 0x00166C29
            pixel[1] = u8(u8(pixel[1] + u8(first - 1)) - sine);
            const u8 second = u8(runtime_crt_rand(seed) % 3);
            pixel[2] = u8(pixel[2] + u8(u8(second - cosine) - 1));
            pixel += 4;
        }
    }
    // D3DTexture_LockRect, then XGSwizzleRect of the 128 x 128 32-bit rows into level 0.
    swizzle_nv2a_into(texels(refraction.noise), linear, 0x80, 0x80, 4);
}
}
