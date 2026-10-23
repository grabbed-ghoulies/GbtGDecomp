#include "background_morphs.h"
#include "../graphics/background_cloth.h"
#include "../graphics/vector_normalize.h"

#include <cmath>

namespace gbtg
{
namespace
{
namespace field = background_cloth_field;

float* point_of(RuntimeBackgroundCloth& cloth, u32 index)
{
    return cloth.point(index);
}

// The rows' share: (rows - u - 2) or u over the rows, with the CRT's unsigned correction for a negative count.
float row_share(i32 numerator, u16 rows)
{
    float share = float(numerator);
    if (numerator < 0)
    {
        share = share + 4294967296.f; // DAT_0032B144
    }
    return share / float(rows);
}

// Moves the handle by `pull` scaled by `share`, then puts it back at its old distance from its rail point, with no
// velocity. `zx_first` sums the distance z, x, y (the closing pass of the second half), else y, z, x.
void pull_handle(float* handle, const float* rail, const Vec3& pull, float share, bool zx_first)
{
    const float dx = handle[0] - rail[0];
    const float dy = handle[1] - rail[1];
    const float dz = handle[2] - rail[2];
    const float distance = zx_first ? float(std::sqrt(double(dz * dz + dx * dx + dy * dy)))
                                    : float(std::sqrt(double(dy * dy + dz * dz + dx * dx)));
    handle[0] = pull.x * share + handle[0];
    handle[1] = handle[1] + pull.y * share;
    handle[2] = handle[2] + pull.z * share;
    Vec3 offset{handle[0] - rail[0], handle[1] - rail[1], handle[2] - rail[2]};
    normalize_runtime_graphics_vector(offset, offset); // 0x00184F99
    offset = {offset.x * distance, offset.y * distance, offset.z * distance};
    handle[0] = rail[0] + offset.x;
    handle[1] = offset.y + rail[1];
    handle[2] = rail[2] + offset.z;
    handle[3] = handle[0];
    handle[4] = handle[1];
    handle[5] = handle[2];
}

// One pass over the rows: the second half's handles move by `second_pull` (rows - u - 2 shares), the first half's by
// `first_pull` (u shares).
void pull_rows(RuntimeBackgroundMorph& morph, const Vec3& first_pull, const Vec3& second_pull, bool closing)
{
    RuntimeBackgroundCloth& first = *morph.first;
    RuntimeBackgroundCloth& second = *morph.second;
    if (get(first.bytes, field::rows) == 0)
    {
        return;
    }
    u32 row = 0;
    u32 next;
    do
    {
        next = row + 1;
        const u16 second_rows = get(second.bytes, field::rows);
        if (next < second_rows)
        {
            pull_handle(point_of(second, morph.second_handles[row]), point_of(second, morph.second_rail[row]),
                second_pull, row_share(i32(second_rows - row) - 2, second_rows), closing);
        }
        if (row != 0)
        {
            pull_handle(point_of(first, morph.first_handles[row]), point_of(first, morph.first_rail[row]),
                first_pull, row_share(i32(row), get(first.bytes, field::rows)), false);
        }
        row = next;
    } while (next < get(first.bytes, field::rows));
}
}

// Xbox 0x00114050
void set_runtime_background_morphs_playing(RuntimeBackgroundMorphs* morphs, u8 playing)
{
    if (morphs == nullptr)
    {
        return;
    }
    for (auto& record : morphs->records)
    {
        record.playing = playing;
    }
}

// Xbox 0x00113620
void add_runtime_background_morph(RuntimeBackgroundMorphs* morphs, RuntimeBackgroundCloth& first,
    RuntimeBackgroundCloth& second, const std::function<void(RuntimeBackgroundCloth&)>& settle)
{
    if (morphs == nullptr)
    {
        return;
    }
    auto& morph = morphs->records.emplace_back();
    morph.first = &first;
    morph.second = &second;
    morph.playing = 0;
    morph.first_handles.resize(get(first.bytes, field::rows));
    morph.second_handles.resize(get(second.bytes, field::rows));
    morph.first_rail.resize(get(first.bytes, field::rows));
    morph.second_rail.resize(get(second.bytes, field::rows));
    settle(first); // 0x0004C0B0, EDI first
    settle(second); // 0x0004C0B0, EDI second
    set(first.bytes, field::sorted, u8(0));
    set(second.bytes, field::sorted, u8(0));
    for (u32 row = 0; row < get(first.bytes, field::rows); ++row)
    {
        const u32 columns = get(first.bytes, field::columns);
        morph.first_handles[row] = (columns * 2) / 3 + columns * row;
        morph.first_rail[row] = columns * row;
        reinterpret_cast<u8*>(first.point(morph.first_handles[row]))[0x28] = 1;
    }
    for (u32 row = 0; row < get(second.bytes, field::rows); ++row)
    {
        const u32 columns = get(second.bytes, field::columns);
        morph.second_handles[row] = (columns * 2) / 3 + columns * row;
        morph.second_rail[row] = columns * row;
        reinterpret_cast<u8*>(second.point(morph.second_handles[row]))[0x28] = 1;
    }
    prepare_runtime_point_graph_edges(first.graph); // 0x001024A0, EAX first
    prepare_runtime_point_graph_edges(second.graph); // 0x001024A0, EAX second
    const Vec3 a = get(second.bytes, field::center);
    const Vec3 b = get(first.bytes, field::center);
    morph.center = {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f}; // DAT_0032B0F4
    morph.steps = 0;
}

// Xbox 0x001137E0
void step_runtime_background_morphs(RuntimeBackgroundMorphs* morphs)
{
    if (morphs == nullptr)
    {
        return;
    }
    constexpr float kRate = 0.008f; // DAT_0032B344
    for (auto& morph : morphs->records)
    {
        if (morph.playing == 1)
        {
            if (morph.steps >= 100)
            {
                morph.playing = 0;
                continue;
            }
            set(morph.first->bytes, field::awake, u8(1));
            set(morph.second->bytes, field::awake, u8(1));
            const u16 rows = get(morph.first->bytes, field::rows);
            const float* top = morph.first->point(morph.first_rail[0]);
            const float* bottom = morph.first->point(morph.first_rail[rows - 1]);
            const Vec3 first_pull{(top[0] - bottom[0]) * kRate, (top[1] - bottom[1]) * kRate,
                (top[2] - bottom[2]) * kRate};
            const Vec3 second_pull{(bottom[0] - top[0]) * kRate, (bottom[1] - top[1]) * kRate,
                (bottom[2] - top[2]) * kRate};
            ++morph.steps;
            pull_rows(morph, first_pull, second_pull, false);
        }
        else if (morph.playing == 2)
        {
            if (morph.steps < 1)
            {
                morph.playing = 0;
                continue;
            }
            set(morph.first->bytes, field::awake, u8(1));
            set(morph.second->bytes, field::awake, u8(1));
            const u16 rows = get(morph.first->bytes, field::rows);
            const float* bottom = morph.first->point(morph.first_rail[rows - 1]);
            const float* top = morph.first->point(morph.first_rail[0]);
            const Vec3 first_pull{(bottom[0] - top[0]) * kRate, (bottom[1] - top[1]) * kRate,
                (bottom[2] - top[2]) * kRate};
            const Vec3 second_pull{(top[0] - bottom[0]) * kRate, (top[1] - bottom[1]) * kRate,
                (top[2] - bottom[2]) * kRate};
            --morph.steps;
            pull_rows(morph, first_pull, second_pull, true);
        }
    }
}
}
