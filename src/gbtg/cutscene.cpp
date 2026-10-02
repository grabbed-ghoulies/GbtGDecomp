#include "gbtg/cutscene.hpp"

#include <cmath>

namespace gbtg
{
namespace
{

void collect_aids(std::span<const u8> cpu, Cutscene& c)
{
    for (size_t i = 0; i < cpu.size();)
    {
        if (cpu[i] < 32 || cpu[i] >= 127)
        {
            ++i;
            continue;
        }
        size_t j = i;
        while (j < cpu.size() && cpu[j] >= 32 && cpu[j] < 127)
        {
            ++j;
        }
        if (j < cpu.size() && cpu[j] == 0 && j - i >= 8)
        {
            std::string_view s(reinterpret_cast<const char*>(cpu.data() + i), j - i);
            if (s.starts_with("aid_"))
            {
                std::string str(s);
                c.aids.push_back(str);
                if (s.starts_with("aid_camera_"))
                {
                    c.cameras.push_back(std::move(str));
                }
                else if (s.starts_with("aid_model_"))
                {
                    c.models.push_back(std::move(str));
                }
                else if (s.starts_with("aid_anim_"))
                {
                    c.anims.push_back(std::move(str));
                }
            }
            i = j + 1;
            continue;
        }
        i = j > i ? j : i + 1;
    }
}

std::string read_cstr(std::span<const u8> cpu, size_t off)
{
    if (off >= cpu.size() || cpu[off] < 32 || cpu[off] >= 127)
    {
        return {};
    }
    size_t j = off;
    while (j < cpu.size() && cpu[j] >= 32 && cpu[j] < 127)
    {
        ++j;
    }
    if (j >= cpu.size() || cpu[j] != 0 || j - off < 4)
    {
        return {};
    }
    return std::string(reinterpret_cast<const char*>(cpu.data() + off), j - off);
}

bool dense_curve(std::span<const u8> cpu, u32 data, u32 count, std::vector<float>& keys)
{
    if (count == 0 || count > 100000)
    {
        return false;
    }
    const size_t bytes = size_t(count) * 4;
    if (size_t(data) + bytes > cpu.size())
    {
        return false;
    }
    keys.resize(count);
    int sane = 0;
    for (u32 i = 0; i < count; ++i)
    {
        const float s = read_f32(cpu, size_t(data) + size_t(i) * 4);
        keys[i] = s;
        if (std::isfinite(s) && std::fabs(s) < 10000.f)
        {
            ++sane;
        }
    }
    if (sane * 10 < int(count) * 9)
    {
        keys.clear();
        return false;
    }
    return true;
}

void parse_camera_curves(std::span<const u8> cpu, Cutscene& c)
{
    if (cpu.size() < 0x18)
    {
        return;
    }
    const u32 list = read_u32(cpu, 8);
    if (size_t(list) + 8 > cpu.size())
    {
        return;
    }
    const u32 name_off = read_u32(cpu, list);
    const u32 chan = read_u32(cpu, list + 4);
    c.camera_name = read_cstr(cpu, name_off);
    if (c.keyframes == 0)
    {
        return;
    }
    // 0x4F700 adds the channel-table address to each data pointer (+8, +0x14, ...).
    size_t off = chan;
    while (off + 12 <= cpu.size() && c.camera.size() < 8)
    {
        const u32 n = read_u16(cpu, off);
        const float dur = read_f32(cpu, off + 4);
        const u32 data = read_u32(cpu, off + 8);
        if (!(dur > 0.f) || std::fabs(dur - c.duration) > 0.05f)
        {
            break;
        }
        CutsceneCurve curve;
        curve.duration = dur;
        dense_curve(cpu, chan + data, n, curve.keys);
        c.camera.push_back(std::move(curve));
        off += 12;
    }
}

void parse_actor_tracks(std::span<const u8> cpu, Cutscene& c)
{
    // Track table pointer is the u32 at +0x0C. Each row is 0x64 bytes.
    if (cpu.size() < 16 || c.tracks == 0)
    {
        return;
    }
    const u32 table = read_u32(cpu, 12);
    constexpr u32 kStride = 0x64;
    for (u8 i = 0; i < c.tracks; ++i)
    {
        const size_t off = size_t(table) + size_t(i) * kStride;
        if (off + kStride > cpu.size())
        {
            break;
        }
        CutsceneActor actor;
        actor.model = read_cstr(cpu, read_u32(cpu, off));
        actor.anim = read_cstr(cpu, read_u32(cpu, off + 4));
        if (!actor.model.starts_with("aid_model_"))
        {
            break;
        }
        size_t curve_off = off + 0x1C;
        actor.curves.reserve(6);
        for (int k = 0; k < 6; ++k)
        {
            const u32 n = read_u32(cpu, curve_off);
            const u32 data = read_u32(cpu, curve_off + 8);
            CutsceneCurve curve;
            dense_curve(cpu, data, n, curve.keys);
            actor.curves.push_back(std::move(curve));
            curve_off += 12;
        }
        const u32 mina_off = read_u32(cpu, off + 8);
        if (mina_off + 0x20 < cpu.size() && cpu[mina_off] == 'M' && cpu[mina_off + 1] == 'I' &&
            cpu[mina_off + 2] == 'N' && cpu[mina_off + 3] == 'A')
        {
            size_t end = cpu.size();
            for (size_t s = size_t(mina_off) + 4; s + 4 <= cpu.size(); ++s)
            {
                if (cpu[s] == 'M' && cpu[s + 1] == 'I' && cpu[s + 2] == 'N' && cpu[s + 3] == 'A')
                {
                    end = s;
                    break;
                }
            }
            actor.mina.assign(cpu.begin() + mina_off, cpu.begin() + end);
        }
        c.actors.push_back(std::move(actor));
    }
}

} // namespace

float sample_curve(const CutsceneCurve& curve, float time, float duration)
{
    const auto& keys = curve.keys;
    if (keys.empty())
    {
        return 0.f;
    }
    if (keys.size() == 1 || !(duration > 0.f))
    {
        return keys[0];
    }
    float u = time / duration * float(keys.size() - 1);
    if (u < 0.f)
    {
        u = 0.f;
    }
    const float last = float(keys.size() - 1);
    if (u > last)
    {
        u = last;
    }
    const size_t i = size_t(u);
    if (i >= keys.size() - 1)
    {
        return keys.back();
    }
    const float f = u - float(i);
    return keys[i] * (1.f - f) + keys[i + 1] * f;
}

// 0x514C0: u = time/duration * count; floor; lerp to the next key; past the last interval holds the last key.
float sample_channel(const CutsceneCurve& curve, float time)
{
    const auto& keys = curve.keys;
    if (keys.empty())
    {
        return 0.f;
    }
    if (keys.size() == 1 || !(curve.duration > 0.f))
    {
        return keys[0];
    }
    float u = time / curve.duration * float(keys.size());
    if (u < 0.f)
    {
        u = 0.f;
    }
    const int last = int(keys.size()) - 1;
    int i = int(u);
    if (i >= last)
    {
        return keys[size_t(last)];
    }
    if (i < 0)
    {
        i = 0;
    }
    const float frac = u - float(i);
    return keys[size_t(i)] * (1.f - frac) + keys[size_t(i) + 1] * frac;
}

Quat slerp_quat(Quat a, Quat b, float t)
{
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (dot < 0.f)
    {
        b.x = -b.x;
        b.y = -b.y;
        b.z = -b.z;
        b.w = -b.w;
        dot = -dot;
    }
    if (dot > 0.9995f)
    {
        Quat q{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
        const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
        if (len > 1e-8f)
        {
            q.x /= len;
            q.y /= len;
            q.z /= len;
            q.w /= len;
        }
        return q;
    }
    const float theta = std::acos(dot);
    const float s = std::sin(theta);
    const float w1 = std::sin((1.f - t) * theta) / s;
    const float w2 = std::sin(t * theta) / s;
    return Quat{a.x * w1 + b.x * w2, a.y * w1 + b.y * w2, a.z * w1 + b.z * w2, a.w * w1 + b.w * w2};
}

// 0x51590 gathers key[i] and key[i+1] on channels 3..6, then 0x154030 slerps them.
Quat sample_orientation(const Cutscene& cut, float time)
{
    const CutsceneCurve& lead = cut.camera[3];
    if (lead.keys.size() < 2 || !(lead.duration > 0.f))
    {
        Quat q;
        if (cut.camera.size() > 3 && !cut.camera[3].keys.empty())
        {
            q.x = cut.camera[3].keys[0];
        }
        if (cut.camera.size() > 4 && !cut.camera[4].keys.empty())
        {
            q.y = cut.camera[4].keys[0];
        }
        if (cut.camera.size() > 5 && !cut.camera[5].keys.empty())
        {
            q.z = cut.camera[5].keys[0];
        }
        if (cut.camera.size() > 6 && !cut.camera[6].keys.empty())
        {
            q.w = cut.camera[6].keys[0];
        }
        return q;
    }
    float u = time / lead.duration * float(lead.keys.size());
    if (u < 0.f)
    {
        u = 0.f;
    }
    const int last = int(lead.keys.size()) - 1;
    int i = int(u);
    float frac = 0.f;
    if (i >= last)
    {
        i = last;
        frac = 0.f;
    }
    else
    {
        frac = u - float(i);
    }
    auto key = [&](size_t channel, int index) {
        const auto& keys = cut.camera[channel].keys;
        if (keys.empty())
        {
            return 0.f;
        }
        if (index >= int(keys.size()))
        {
            return keys.back();
        }
        if (index < 0)
        {
            return keys[0];
        }
        return keys[size_t(index)];
    };
    const int j = i >= last ? last : i + 1;
    const Quat a{key(3, i), key(4, i), key(5, i), key(6, i)};
    const Quat b{key(3, j), key(4, j), key(5, j), key(6, j)};
    return slerp_quat(a, b, frac);
}

bool sample_cutscene_camera(const Cutscene& cut, float time, Vec3& position, Quat& orientation, float& fov_y)
{
    if (cut.camera.size() < 3)
    {
        return false;
    }
    position.x = sample_channel(cut.camera[0], time);
    position.y = sample_channel(cut.camera[1], time);
    position.z = sample_channel(cut.camera[2], time);
    orientation = cut.camera.size() >= 7 ? sample_orientation(cut, time) : Quat{};
    fov_y = 0.9f;
    if (cut.camera.size() >= 8 && !cut.camera[7].keys.empty())
    {
        const float raw = sample_channel(cut.camera[7], time);
        if (std::isfinite(raw) && raw > 0.f)
        {
            // Attract clips store 35. Values above a full turn are degrees; smaller ones are already radians.
            fov_y = raw > 6.2831853f ? raw * 0.017453292f : raw;
        }
    }
    return true;
}

Cutscene parse_cutscene(std::span<const u8> cpu, std::string_view name)
{
    Cutscene c;
    c.name = std::string(name);
    if (cpu.size() < 8)
    {
        return c;
    }
    c.keyframes = read_u16(cpu, 0);
    c.flag = cpu[2];
    c.tracks = cpu[3];
    c.duration = read_f32(cpu, 4);
    if (!(c.duration > 0.f) || c.duration > 3600.f)
    {
        c.duration = 0.f;
    }
    collect_aids(cpu, c);
    parse_camera_curves(cpu, c);
    parse_actor_tracks(cpu, c);
    return c;
}

} // namespace gbtg
