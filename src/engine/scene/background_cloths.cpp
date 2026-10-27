#include "background_cloths.h"
#include "../audio/event_voices.h"
#include "../camera/view_frustum.h"
#include "../entities/entity_heap.h"
#include "../entities/mesh_sphere_geometry.h"
#include "../graphics/vector_normalize.h"
#include "gbtg/anim.h"

#include <cstdio>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = background_cloth_field;

// The ramp's grey for texel `index` of 256 (DAT_0032B430.. thresholds, DAT_0032B414 scale).
u8 ramp_level(u32 index)
{
    const float position = float(i32(index));
    float level;
    if (25.6f > position) // DAT_0032B430
    {
        level = 0.3f; // DAT_0032B268
    }
    else if (51.2f > position) // DAT_0032B42C
    {
        level = 0.35f; // DAT_0032B354
    }
    else if (76.8f > position) // DAT_0032B428
    {
        level = 0.5f; // DAT_0032B0F4
    }
    else if (102.4f > position) // DAT_0032B424
    {
        level = 0.6f; // DAT_0032B248
    }
    else if (179.2f > position) // DAT_0032B420
    {
        level = 0.65f; // DAT_0032B1EC
    }
    else if (204.8f > position) // DAT_0032B41C
    {
        level = 0.7f; // DAT_0032B264
    }
    else
    {
        level = 230.4f > position ? 0.75f : 0.85f; // DAT_0032B418, DAT_0032B2AC, DAT_0032B17C
    }
    i32 value = i32(level * 256.f); // DAT_0032B414, CVTTSS2SI
    if (value < 0)
    {
        value = 0;
    }
    else if (value >= 0x100)
    {
        value = 0xFF;
    }
    return u8(value);
}

std::span<const u8> part_table_of(u32 model, RuntimeBackgroundClothScene& scene)
{
    const u32 record = read_u32(scene.cloth.resolve(model, 0x14), 0x10);
    const u32 table = read_u32(scene.cloth.resolve(record, 0x10), 0xC);
    if (table == 0)
    {
        return {};
    }
    return scene.cloth.resolve(table, 0x10);
}

void kind_not_translated(u8 kind, u32 address)
{
    char text[96];
    std::snprintf(text, sizeof(text), "background part kind %u's cloth builder 0x%08X is not translated", kind,
        address);
    throw Error(text);
}
}

// Xbox 0x00114180
void create_runtime_background_cloth_pools(RuntimeBackgroundClothScene& scene)
{
    auto& cloths = scene.cloths;
    cloths.list.clear();
    cloths.list.reserve(2);
    cloths.planes.clear();
    cloths.planes.reserve(2);
    cloths.morphs.records.clear();
    cloths.morphs.records.reserve(2);
    cloths.created = true;
    scene.morphs = &cloths.morphs;
    const u32 texture = scene.create_texture(0x100, 1, 1, 0, 6, 3); // D3DDevice_CreateTexture2
    cloths.ramp = texture;
    u8* texels = scene.lock_texture(texture); // D3DTexture_LockRect
    auto* steps = static_cast<u8*>(engine_heap_allocate(0x400));
    for (u32 index = 0; index < 0x100; ++index)
    {
        const u8 level = ramp_level(index);
        steps[index * 4] = level;
        steps[index * 4 + 1] = level;
        steps[index * 4 + 2] = level;
        steps[index * 4 + 3] = 0xFF;
    }
    // XGSwizzleRect (0x0018DCC3) of one 256-texel row: the swizzled order of a single row is the linear one.
    std::memcpy(texels, steps, 0x400);
    engine_heap_free(steps); // 0x0014D360
    scene.ramp_texture = cloths.ramp;
}

// Xbox 0x001134A0
void release_runtime_background_morphs(RuntimeBackgroundClothScene& scene)
{
    if (scene.morphs == nullptr)
    {
        return;
    }
    scene.cloths.morphs.records.clear();
    scene.morphs = nullptr;
}

// Xbox 0x001143A0
void release_runtime_background_cloths(RuntimeBackgroundClothScene& scene)
{
    release_runtime_background_morphs(scene); // 0x001134A0
    auto& cloths = scene.cloths;
    if (cloths.created)
    {
        for (auto* cloth : cloths.list)
        {
            release_runtime_background_cloth(cloth); // 0x0004B770
        }
        cloths.list.clear();
        cloths.planes.clear();
        cloths.created = false;
    }
    if (cloths.ramp != 0)
    {
        scene.release_texture(cloths.ramp); // D3DResource_Release
        cloths.ramp = 0;
        scene.ramp_texture = 0;
    }
}

// Xbox 0x001153A0
void build_runtime_background_cloths(RuntimeBackgroundClothScene& scene)
{
    const u32 model = scene.background_model();
    if (model == 0)
    {
        return;
    }
    const Vec3 room{(scene.room_high[0] + scene.room_low[0]) * 0.5f, (scene.room_high[1] + scene.room_low[1]) * 0.5f,
        (scene.room_high[2] + scene.room_low[2]) * 0.5f}; // DAT_0050EA04..DAT_0050EA18, DAT_0032B0F4
    const auto table = part_table_of(model, scene);
    if (table.empty())
    {
        return;
    }
    const u32 count = read_u32(table, 0);
    const auto parts = scene.cloth.resolve(read_u32(table, 4), size_t(count) * 0x34);
    for (u32 index = 0; index < count; ++index)
    {
        const auto part = parts.subspan(size_t(index) * 0x34, 0x34);
        const u8 kind = part[0x30];
        if (kind == 1 || kind == 2 || kind == 3 || kind == 8)
        {
            RuntimeBackgroundCloth* cloth = build_runtime_background_flag(part, part_table_of(model, scene),
                scene.cloth); // 0x00050880
            if (cloth == nullptr)
            {
                continue;
            }
            auto bytes = cloth->bytes;
            set(bytes, field::constrain_hook, 0x00114910u);
            set(bytes, field::constrain_enabled, 1u);
            set(bytes, field::awake_hook, 0x001151F0u);
            set(bytes, field::asleep_hook, 0x001152C0u);
            const Vec3 center = get(bytes, field::center);
            Vec3 light{center.x - room.x, center.y - room.y, center.z - room.z};
            const float squared = (light.z * light.z + light.y * light.y) + light.x * light.x;
            if (squared != 0.f) // DAT_0032B100
            {
                const long double scale = retail_reciprocal_sqrt(squared); // 0x001534C0
                light = {float(light.x * scale), float(light.y * scale), float(light.z * scale)};
            }
            set(bytes, field::faces_room, u8(1));
            set(bytes, field::light, light);
            if (part[0x30] == 8)
            {
                set(bytes, field::casts_shadow, u8(0));
            }
            register_runtime_background_cloth(*cloth, scene); // 0x00115A40
        }
        else if (kind == 7)
        {
            RuntimeBackgroundCloth* cloth = build_runtime_background_water(part, part_table_of(model, scene),
                scene.cloth); // 0x00050940
            if (cloth != nullptr)
            {
                auto bytes = cloth->bytes;
                set(bytes, field::casts_shadow, u8(0));
                set(bytes, field::constrain_hook, 0x001145F0u);
                set(bytes, field::constrain_enabled, 1u);
                set(bytes, field::before_hook, 0x00114580u);
                set(bytes, field::own_shaders, u8(0));
                set(bytes, field::second_draw, u8(1));
                set(bytes, field::damping, 0.01f); // DAT_0032B124
                set(bytes, field::layer, u8(1));
                set(bytes, field::specular, u8(1));
                register_runtime_background_cloth(*cloth, scene); // 0x00115A40
            }
        }
        else if (kind == 6)
        {
            kind_not_translated(kind, 0x000509E0);
        }
        else if (kind == 5)
        {
            RuntimeBackgroundCloth* first = nullptr;
            RuntimeBackgroundCloth* second = nullptr;
            build_runtime_background_curtains(part, part_table_of(model, scene), first, second,
                scene.cloth); // 0x00050CC0
            settle_runtime_background_cloth(*first, scene.cloth); // 0x0004C0B0
            settle_runtime_background_cloth(*second, scene.cloth); // 0x0004C0B0
            register_runtime_background_cloth(*first, scene); // 0x00115A40
            register_runtime_background_cloth(*second, scene); // 0x00115A40
            set(first->bytes, field::awake_hook, 0x001151F0u);
            set(second->bytes, field::awake_hook, 0x001151F0u);
            set(first->bytes, field::asleep_hook, 0x001152C0u);
            set(second->bytes, field::asleep_hook, 0x001152C0u);
            set(first->bytes, field::own_shaders, u8(0));
            set(second->bytes, field::own_shaders, u8(0));
            add_runtime_background_morph(scene.morphs, *first, *second, [&scene](RuntimeBackgroundCloth& cloth)
                {
                    settle_runtime_background_cloth(cloth, scene.cloth); // 0x0004C0B0
                }); // 0x00113620, EBX second
        }
        else if (kind == 4 && scene.cloths.created)
        {
            auto& plane = scene.cloths.planes.emplace_back(); // 0x000662F0
            std::memcpy(plane.data() + 4, part.data(), 0x30);
            const float* corner = plane.data() + 4;
            const float ax = corner[3] - corner[0];
            const float ay = corner[4] - corner[1];
            const float az = corner[5] - corner[2];
            const float bx = corner[6] - corner[0];
            const float by = corner[7] - corner[1];
            const float bz = corner[8] - corner[2];
            Vec3 normal{bz * ay - by * az, az * bx - bz * ax, by * ax - ay * bx};
            normalize_runtime_graphics_vector(normal, normal); // 0x00184F99
            plane[0] = normal.x;
            plane[1] = normal.y;
            plane[2] = normal.z;
            plane[3] = 0.f - (corner[1] * normal.y + corner[2] * normal.z + corner[0] * normal.x);
        }
    }
    if (!scene.cloths.created)
    {
        return;
    }
    for (const auto& plane : scene.cloths.planes)
    {
        const Vec3 p0{plane[4], plane[5], plane[6]};
        const Vec3 p1{plane[7], plane[8], plane[9]};
        const Vec3 p2{plane[10], plane[11], plane[12]};
        const Vec3 p3{plane[13], plane[14], plane[15]};
        for (auto* cloth : scene.cloths.list)
        {
            const Vec3 center = get(cloth->bytes, field::center);
            const float radius = get(cloth->bytes, field::radius);
            if (!runtime_sphere_intersects_triangle(center, radius, p0, p1, p2) // 0x0015A440
                && !runtime_sphere_intersects_triangle(center, radius, p1, p2, p3))
            {
                continue;
            }
            auto& copy = cloth->planes.emplace_back(plane);
            if (0.f > center.y * copy[1] + center.x * copy[0] + center.z * copy[2] + copy[3])
            {
                copy[0] = 0.f - copy[0];
                copy[1] = 0.f - copy[1];
                copy[2] = 0.f - copy[2];
                copy[3] = 0.f - copy[3];
            }
        }
    }
}

// Xbox 0x00115A40, ESI cloth
void register_runtime_background_cloth(RuntimeBackgroundCloth& cloth, RuntimeBackgroundClothScene& scene)
{
    if (!scene.cloths.created)
    {
        return;
    }
    scene.cloths.list.push_back(&cloth);
    step_runtime_background_cloth(cloth, 0.1f, scene.cloth); // 0x0004C000
    measure_runtime_background_cloth(cloth); // 0x0004C0E0
}

// Xbox 0x00115BB0
void tick_runtime_background_cloths(float step, RuntimeBackgroundClothScene& scene)
{
    if (!scene.cloths.created || scene.cloths.list.empty())
    {
        return;
    }
    for (auto* cloth : scene.cloths.list)
    {
        auto bytes = cloth->bytes;
        if (get(bytes, field::always_stepped) == 0
            && classify_runtime_box_frustum(scene.frustum(), get(bytes, field::low), get(bytes, field::high),
                scene.frustum_hint()) == 0) // 0x00159BA0
        {
            continue;
        }
        step_runtime_background_cloth(*cloth, step, scene.cloth); // 0x0004C000
        if (get(bytes, field::awake) == 0)
        {
            continue;
        }
        measure_runtime_background_cloth(*cloth); // 0x0004C0E0
        if (get(bytes, field::rustles) == 0)
        {
            continue;
        }
        if (0.f < get(bytes, field::sound_wait))
        {
            set(bytes, field::sound_wait, get(bytes, field::sound_wait) - step);
            continue;
        }
        if (!(runtime_background_cloth_motion(*cloth) > 1.f)) // 0x0004C1B0
        {
            continue;
        }
        set(bytes, field::sound_wait, 5.f); // DAT_0032B184
        const Vec3 low = get(bytes, field::low);
        const Vec3 high = get(bytes, field::high);
        const Vec3 middle{(high.x + low.x) * 0.5f, (high.y + low.y) * 0.5f, (high.z + low.z) * 0.5f};
        const i16 voice = request_positioned_audio_event_voice(field_data(bytes, field::sound), 1, middle,
            scene.voices); // 0x00039580
        set(bytes, field::sound_voice, u16(voice));
    }
    step_runtime_background_morphs(scene.morphs); // 0x001137E0
}

}
