#include "background_morphs.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "../runtime/rare_replay.h"
#endif
#include "../graphics/background_cloth.h"
#include "../graphics/vector_normalize.h"

#include <cmath>

namespace gbtg
{
namespace
{

PointGraphPoint& point_of(RuntimeBackgroundCloth& cloth, u32 index)
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
void pull_handle(PointGraphPoint& handle, const PointGraphPoint& rail, const Vec3& pull, float share, bool zx_first)
{
    const float dx = handle.position.x - rail.position.x;
    const float dy = handle.position.y - rail.position.y;
    const float dz = handle.position.z - rail.position.z;
    const float distance = zx_first ? float(std::sqrt(double(dz * dz + dx * dx + dy * dy)))
                                    : float(std::sqrt(double(dy * dy + dz * dz + dx * dx)));
    handle.position.x = pull.x * share + handle.position.x;
    handle.position.y = handle.position.y + pull.y * share;
    handle.position.z = handle.position.z + pull.z * share;
    Vec3 offset{handle.position.x - rail.position.x, handle.position.y - rail.position.y, handle.position.z - rail.position.z};
    normalize_runtime_graphics_vector(offset, offset); // 0x00184F99
    offset = {offset.x * distance, offset.y * distance, offset.z * distance};
    handle.position.x = rail.position.x + offset.x;
    handle.position.y = offset.y + rail.position.y;
    handle.position.z = rail.position.z + offset.z;
    handle.previous.x = handle.position.x;
    handle.previous.y = handle.position.y;
    handle.previous.z = handle.position.z;
}

// One pass over the rows: the second half's handles move by `second_pull` (rows - u - 2 shares), the first half's by
// `first_pull` (u shares).
void pull_rows(RuntimeBackgroundMorph& morph, const Vec3& first_pull, const Vec3& second_pull, bool closing)
{
    RuntimeBackgroundCloth& first = *morph.first;
    RuntimeBackgroundCloth& second = *morph.second;
    if (cloth_header(first.bytes).rows == 0)
    {
        return;
    }
    u32 row = 0;
    u32 next;
    do
    {
        next = row + 1;
        const u16 second_rows = cloth_header(second.bytes).rows;
        if (next < second_rows)
        {
            pull_handle(point_of(second, morph.second_handles[row]), point_of(second, morph.second_rail[row]),
                second_pull, row_share(i32(second_rows - row) - 2, second_rows), closing);
        }
        if (row != 0)
        {
            pull_handle(point_of(first, morph.first_handles[row]), point_of(first, morph.first_rail[row]),
                first_pull, row_share(i32(row), cloth_header(first.bytes).rows), false);
        }
        row = next;
    } while (next < cloth_header(first.bytes).rows);
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
    morph.first_handles.resize(cloth_header(first.bytes).rows);
    morph.second_handles.resize(cloth_header(second.bytes).rows);
    morph.first_rail.resize(cloth_header(first.bytes).rows);
    morph.second_rail.resize(cloth_header(second.bytes).rows);
    settle(first); // 0x0004C0B0, EDI first
    settle(second); // 0x0004C0B0, EDI second
    cloth_header(first.bytes).sorted = u8(0);
    cloth_header(second.bytes).sorted = u8(0);
    for (u32 row = 0; row < cloth_header(first.bytes).rows; ++row)
    {
        const u32 columns = cloth_header(first.bytes).columns;
        morph.first_handles[row] = (columns * 2) / 3 + columns * row;
        morph.first_rail[row] = columns * row;
        first.point(morph.first_handles[row]).pinned = 1;
    }
    for (u32 row = 0; row < cloth_header(second.bytes).rows; ++row)
    {
        const u32 columns = cloth_header(second.bytes).columns;
        morph.second_handles[row] = (columns * 2) / 3 + columns * row;
        morph.second_rail[row] = columns * row;
        second.point(morph.second_handles[row]).pinned = 1;
    }
    prepare_runtime_point_graph_edges(first.graph); // 0x001024A0, EAX first
    prepare_runtime_point_graph_edges(second.graph); // 0x001024A0, EAX second
    const Vec3 a = cloth_header(second.bytes).center;
    const Vec3 b = cloth_header(first.bytes).center;
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
#if !defined(GBTG_RETAIL_GAME_ONLY)
    // Rare Replay: each call pulls a fixed step and counts it, so the morphs step on retail ticks.
    if (!rare_replay_retail_tick())
    {
        return;
    }
#endif
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
            cloth_header(morph.first->bytes).awake = u8(1);
            cloth_header(morph.second->bytes).awake = u8(1);
            const u16 rows = cloth_header(morph.first->bytes).rows;
            const PointGraphPoint& top = morph.first->point(morph.first_rail[0]);
            const PointGraphPoint& bottom = morph.first->point(morph.first_rail[rows - 1]);
            const Vec3 first_pull{(top.position.x - bottom.position.x) * kRate, (top.position.y - bottom.position.y) * kRate,
                (top.position.z - bottom.position.z) * kRate};
            const Vec3 second_pull{(bottom.position.x - top.position.x) * kRate, (bottom.position.y - top.position.y) * kRate,
                (bottom.position.z - top.position.z) * kRate};
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
            cloth_header(morph.first->bytes).awake = u8(1);
            cloth_header(morph.second->bytes).awake = u8(1);
            const u16 rows = cloth_header(morph.first->bytes).rows;
            const PointGraphPoint& bottom = morph.first->point(morph.first_rail[rows - 1]);
            const PointGraphPoint& top = morph.first->point(morph.first_rail[0]);
            const Vec3 first_pull{(bottom.position.x - top.position.x) * kRate, (bottom.position.y - top.position.y) * kRate,
                (bottom.position.z - top.position.z) * kRate};
            const Vec3 second_pull{(top.position.x - bottom.position.x) * kRate, (top.position.y - bottom.position.y) * kRate,
                (top.position.z - bottom.position.z) * kRate};
            --morph.steps;
            pull_rows(morph, first_pull, second_pull, true);
        }
    }
}
}
