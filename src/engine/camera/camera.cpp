#include "gbtg/anim.h"
#include "gbtg/engine_internal.h"
#include "gbtg/entity.h"
#include "gbtg/gfx.h"
#include "gbtg/model.h"
#include "gbtg/movie.h"
#include "gbtg/resource.h"
#include "../transitions/transition_data.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cerrno>
#include <charconv>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>

#if !defined(_WIN32)
#include <cfenv>
#endif

#if defined(_WIN32)
#include <excpt.h>
#include <float.h>
#include <malloc.h>
extern "C"
{
    __declspec(dllimport) void* __stdcall GetModuleHandleA(const char* name);
    __declspec(dllimport) void* __stdcall GetProcAddress(void* module, const char* name);
    __declspec(dllimport) unsigned long __stdcall GetCurrentThreadId();
    __declspec(dllimport) void __stdcall GetSystemTimeAsFileTime(void* time);
    __declspec(dllimport) unsigned long __stdcall GetTickCount();
    __declspec(dllimport) int __stdcall QueryPerformanceCounter(void* counter);
    __declspec(dllimport) void __stdcall RaiseException(unsigned long code, unsigned long flags, unsigned long count,
                                                        const std::uintptr_t* arguments);
    __declspec(dllimport) int __stdcall CloseHandle(void* handle);
    __declspec(dllimport) int __stdcall FlushFileBuffers(void* handle);
    __declspec(dllimport) unsigned long __stdcall GetLastError();
    __declspec(dllimport) int __stdcall ReadFile(void* handle, void* buffer, unsigned long bytes, unsigned long* read,
                                                 void* overlapped);
    __declspec(dllimport) int __stdcall WriteFile(void* handle, const void* buffer, unsigned long bytes,
                                                  unsigned long* written, void* overlapped);
    __declspec(dllimport) unsigned long __stdcall SetFilePointer(void* handle, long distance, long* high,
                                                                 unsigned long method);
    __declspec(dllimport) int __stdcall IsBadReadPtr(const void* lp, std::uintptr_t ucb);
    __declspec(dllimport) int __stdcall IsBadWritePtr(void* lp, std::uintptr_t ucb);
}
#endif


#include "../scene/scene_types.h"
#include "camera.h"

namespace gbtg
{
using namespace scene_detail;

namespace scene_detail
{


CameraWash g_camera_wash;


constexpr float kWashExitRatio = std::bit_cast<float>(u32{0x3eaa7efau});
 // 0.333
constexpr float kWashPi = std::bit_cast<float>(u32{0x40490fdbu});


float wash_ratio(const CameraWash& c)
{
    if (c.sample_max == 0)
    {
        return 1.f;
    }
    return float(c.sample) / float(c.sample_max);
}


void wash_set_rate(CameraWash& c)
{
    // Rate helper: sample / sample_max picks 4, 3, 2, or 1.
    if (c.sample_max == 0)
    {
        c.wave_rate = 1.f;
        return;
    }
    const float ratio = float(c.sample) / float(c.sample_max);
    if (ratio < 0.25f)
    {
        c.wave_rate = 4.f;
    }
    else if (ratio < 0.5f)
    {
        c.wave_rate = 3.f;
    }
    else if (ratio < 0.75f)
    {
        c.wave_rate = 2.f;
    }
    else
    {
        c.wave_rate = 1.f;
    }
}


bool wash_host_active(const CameraWash& c)
{
    // Host check. The caller's other flag is independent of this result.
    if (c.host_block != 0)
    {
        return false;
    }
    if (!(c.host_push > 0.f || c.host_flag != 0))
    {
        return false;
    }
    if (!c.host_link || !c.host_link_live)
    {
        return true;
    }
    return false;
}


bool wash_countdown(float& t, float dt)
{
    // Step a timer down. True only on the frame it reaches 0.
    if (t <= 0.f)
    {
        return false;
    }
    t -= dt;
    if (t > 0.f)
    {
        return false;
    }
    t = 0.f;
    return true;
}


u8 wash_level_byte(float v)
{
    // Clamp to [0, 128], then truncate toward zero into a byte.
    if (v < 0.f)
    {
        v = 0.f;
    }
    else if (v > 128.f)
    {
        v = 128.f;
    }
    return static_cast<u8>(static_cast<int>(v));
}


void wash_blinks(CameraWash& c, float dt)
{
    if (c.sample == 0)
    {
        c.blink_time -= dt;
        if (c.blink_time <= 0.f)
        {
            c.blink_time += 0.1f;
            c.blink ^= 1;
        }
    }
    if (c.blink2_on != 0)
    {
        c.blink2_time -= dt;
        if (c.blink2_time <= 0.f)
        {
            c.blink2_time += 0.75f;
            c.blink2 ^= 1;
        }
    }
    else
    {
        c.blink2 = 0;
    }
}


void wash_wave(CameraWash& c, float dt)
{
    if (c.sample == 0)
    {
        c.wave_base = 2.625f;
        c.wave_mag = 0.f;
    }
    float timer = c.wave_timer - dt;
    c.wave_timer = timer;
    if (timer <= 0.f)
    {
        c.wave_timer = timer + (1.f / c.wave_rate);
        c.wave_phase = c.wave_span;
    }
    if (c.wave_phase > 0.f)
    {
        const float angle = (c.wave_phase / c.wave_span) * kWashPi;
        const float s = std::sin(angle);
        c.wave = c.wave_base + (c.wave_base * c.wave_mag * s);
    }
    else
    {
        c.wave = c.wave_base;
    }
    c.wave_phase -= dt;
    if (c.wave_phase < 0.f)
    {
        c.wave_phase = 0.f;
    }
}


// Xbox 0x0001DD00
void camera_tick_wash(CameraWash& c, float dt)
{
    // Xbox 0x0001DD00
    if (c.enabled == 0)
    {
        return;
    }
    switch (c.state)
    {
    case 0:
    {
        bool start = false;
        if (c.latch == 0)
        {
            if (c.hold > 0.f || wash_ratio(c) < 0.25f)
            {
                start = true;
            }
        }
        if (wash_host_active(c) || start)
        {
            c.wave = 1.75f;
            c.wave_base = 1.75f;
            c.wave_mag = 0.5f;
            c.wave_span = 0.2f;
            c.wave_phase = 0.f;
            c.wave_timer = 0.f;
            c.state = 1;
            c.fade = 0.4f;
            c.level = 0;
            c.blink = 1;
            wash_set_rate(c);
        }
        break;
    }
    case 1:
    {
        wash_wave(c, dt);
        wash_blinks(c, dt);
        wash_set_rate(c);
        float scaled = c.fade * 2.5f;
        scaled *= 128.f;
        c.level = wash_level_byte(128.f - scaled);
        if (wash_countdown(c.fade, dt))
        {
            c.state = 2;
        }
        break;
    }
    case 2:
    {
        wash_wave(c, dt);
        const float ratio = wash_ratio(c);
        wash_countdown(c.hold, dt);
        wash_blinks(c, dt);
        wash_set_rate(c);
        c.level = 0x80;
        bool leave = c.latch != 0;
        if (c.hold == 0.f && ratio > kWashExitRatio)
        {
            leave = true;
        }
        if (!wash_host_active(c) && leave)
        {
            c.state = 3;
            c.fade = 0.5f;
        }
        break;
    }
    case 3:
    {
        wash_wave(c, dt);
        wash_set_rate(c);
        float scaled = c.fade * 2.f;
        scaled *= 128.f;
        c.level = wash_level_byte(scaled);
        if (wash_countdown(c.fade, dt))
        {
            c.state = 0;
            c.level = 0;
        }
        break;
    }
    default:
        break;
    }
}


CameraPairs g_camera_pairs;

u32 g_pair_cache[256] = {};
 // DAT_005078D8

// Xbox 0x00025130
void camera_apply_pairs(CameraPairs& cam)
{
    // Xbox 0x00025130
    // 0x000F96F0 packs AL into the pair selected by the stack byte.
    if (cam.enabled == 0)
    {
        return;
    }
    CamPairObject* slots[2] = {cam.at_204, cam.at_310};
    for (CamPairObject* slot : slots)
    {
        if (slot == nullptr)
        {
            continue;
        }
        const int pair = int(cam.index) * 2;
        const int byte_index = pair >> 3;
        const int shift = pair & 7;
        if (g_pair_cache[cam.index] != cam.value)
        {
            g_pair_cache[cam.index] = cam.value;
        }
        const u8 mask = static_cast<u8>(3u << shift);
        const u8 packed = static_cast<u8>(cam.value << shift);
        u8 cur = slot->pairs[byte_index];
        cur = static_cast<u8>(cur & static_cast<u8>(~mask));
        cur = static_cast<u8>(cur | packed);
        slot->pairs[byte_index] = cur;
    }
}


CameraChannels g_camera_channels;


bool channel_bound(const CameraChannels& cam, i32 id)
{
    // 0x00030B40. EAX is host+0x50. A row of stride 0x28 is bound when the
    // pointer chain reaches +0x20 and the dword at id*4 is nonzero.
    if (id < 0)
    {
        return false;
    }
    const size_t index = static_cast<size_t>(id);
    for (const CamBindRow& row : cam.binds)
    {
        if (!row.live)
        {
            continue;
        }
        if (index < row.words.size() && row.words[index] != 0)
        {
            return true;
        }
    }
    return false;
}


float* channel_slot(CameraChannels& cam, i32 id)
{
    if (!cam.slots_live || id < 0)
    {
        return nullptr;
    }
    const size_t index = static_cast<size_t>(id);
    if (index >= cam.slots.size())
    {
        return nullptr;
    }
    return cam.slots[index];
}


// Xbox 0x0001EC20
void camera_apply_channels(CameraChannels& cam)
{
    // Xbox 0x0001EC20
    bool pull = false;
    if (!cam.items.empty() && cam.binds_live)
    {
        for (const CamChannel& ch : cam.items)
        {
            if (channel_bound(cam, ch.id))
            {
                pull = true;
                break;
            }
        }
    }
    for (CamChannel& ch : cam.items)
    {
        float* slot = channel_slot(cam, ch.id);
        if (pull)
        {
            ch.value = slot ? *slot : 0.f;
            continue;
        }
        if (ch.id == 0 && cam.gate != 0)
        {
            bool marked = false;
            for (u8 flag : cam.mark)
            {
                if (flag != 0)
                {
                    marked = true;
                    break;
                }
            }
            if (marked)
            {
                float cur = slot ? *slot : 0.f;
                if (cur <= ch.value)
                {
                    cur = ch.value;
                }
                if (slot)
                {
                    *slot = cur;
                }
                continue;
            }
        }
        if (slot)
        {
            *slot = ch.value;
        }
    }
}


// Xbox 0x0001EEC0
void camera_apply_default_blend(CameraChannels& cam)
{
    // Xbox 0x0001EEC0
    // EDI is the camera. Skip when the bind list is live and channel 0 is bound.
    // Otherwise write blend[0] (+0xe8) then blend[1] (+0xf4) through the channel-0
    // slot (slot table +0x1cc). Both stores use that same pointer, so blend[1] remains.
    if (cam.binds_live && channel_bound(cam, 0))
    {
        return;
    }
    if (float* slot = channel_slot(cam, 0))
    {
        *slot = cam.blend[0];
    }
    if (float* slot = channel_slot(cam, 0))
    {
        *slot = cam.blend[1];
    }
}


// camera_apply (0x00025AE0) calls 0x0001FB40 with ESI = camera, after the
// channel pass and before the wash. DAT_004312dc > 0 freezes the step
// (BanjoCam). Otherwise the step is DAT_005096C0 * DAT_004312C4 (dt * scale).
// Host is camera+8. Its +0x930 > 0 forces the decay rate to 0; otherwise the
// rate is host+0x92c. Those host fields stay 0 until that object is filled in.
float g_sim_freeze = 0.f;
 // DAT_004312dc
float g_sim_scale = 1.f;
  // DAT_004312c4
float g_sim_dt = 0.f;
      // DAT_005096c0

constexpr float kPulseBlend = std::bit_cast<float>(u32{0x3f7d70a4u});
 // 0.99
constexpr i32 kPulseNoteId = 0x47;


CameraPulse g_camera_pulse;

PulseNode* g_pulse_nodes = nullptr;


float sim_step(float dt)
{
    if (g_sim_freeze > 0.f)
    {
        return 0.f;
    }
    return dt * g_sim_scale;
}


float pulse_rate(const CameraPulse& cam)
{
    if (cam.host_gate > 0.f)
    {
        return 0.f;
    }
    return cam.host_rate;
}


int pulse_mask(const u8 counts[6])
{
    // 0x0001F7A0. Bit i is set when byte +0x260+i is nonzero.
    int mask = 0;
    if (counts[0] != 0)
    {
        mask += 1;
    }
    if (counts[1] != 0)
    {
        mask += 2;
    }
    if (counts[2] != 0)
    {
        mask += 4;
    }
    if (counts[3] != 0)
    {
        mask += 8;
    }
    if (counts[4] != 0)
    {
        mask += 0x10;
    }
    if (counts[5] != 0)
    {
        mask += 0x20;
    }
    return mask;
}


void pulse_notify(CameraPulse& cam)
{
    PulseNode* node = g_pulse_nodes;
    while (node != nullptr)
    {
        PulseNode* next = node->next;
        if (node->handle != nullptr)
        {
            node->handle(node, kPulseNoteId, cam.host);
        }
        node = next;
    }
}


// Xbox 0x0001F600
void camera_tick_pulse_latch(CameraPulse& cam, float dt)
{
    // Xbox 0x0001F600
    // Decay +0x25c. On the frame it hits 0, decrement the six latches.
    // If they were all live and one or more lands on 0, broadcast note 0x47.
    if (!(cam.latch > 0.f))
    {
        return;
    }
    cam.latch -= pulse_rate(cam) * sim_step(dt);
    if (cam.latch > 0.f)
    {
        return;
    }
    cam.latch = 0.f;
    const int before = pulse_mask(cam.counts);
    for (u8& count : cam.counts)
    {
        --count;
    }
    const int after = pulse_mask(cam.counts);
    if (before == 0x3f && after != before)
    {
        pulse_notify(cam);
    }
}


// Xbox 0x0001FB40
void camera_tick_pulse(CameraPulse& cam, float dt)
{
    // Xbox 0x0001FB40
    camera_tick_pulse_latch(cam, dt);
    if (cam.primary > 0.f)
    {
        cam.primary -= pulse_rate(cam) * sim_step(dt);
        if (cam.primary <= 0.f)
        {
            cam.primary = 0.f;
            cam.marker = 0;
            cam.blend = kPulseBlend;
        }
    }
    if (cam.secondary > 0.f)
    {
        cam.secondary -= pulse_rate(cam) * sim_step(dt);
        if (cam.secondary <= 0.f)
        {
            cam.secondary = 0.f;
        }
    }
}


CameraTimers g_camera_timers;


void note_five(TimerNote* obj)
{
    if (obj == nullptr || obj->on_note == nullptr)
    {
        return;
    }
    obj->on_note(obj, 5, 0);
}


// Xbox 0x00027960
void camera_tick_timers(CameraTimers& cam, CameraPulse& pulse, float dt)
{
    // Xbox 0x00027960
    const float step = pulse_rate(pulse) * sim_step(dt);
    if (cam.at_75c > 0.f)
    {
        cam.at_75c -= step;
        if (cam.at_75c <= 0.f)
        {
            cam.at_75c = 0.f;
        }
    }
    if (cam.at_760 != nullptr && cam.at_758 > 0.f)
    {
        cam.at_758 -= step;
        if (cam.at_758 <= 0.f)
        {
            cam.at_758 = 0.f;
            note_five(cam.at_760);
        }
    }
    if (cam.at_7e4 != 0 && cam.at_7e5 == 0 && cam.at_7e8 > 0.f)
    {
        cam.at_7e8 -= step;
        if (cam.at_7e8 <= 0.f)
        {
            cam.at_7e8 = 0.f;
            note_five(cam.at_7f0);
            const int before = pulse_mask(pulse.counts);
            for (u8& count : pulse.counts)
            {
                --count;
            }
            const int after = pulse_mask(pulse.counts);
            if (before == 0x3f && after != before)
            {
                pulse_notify(pulse);
            }
            cam.at_7e4 = 0;
        }
    }
}


struct CamAnchor;


CamFocus g_cam_focus;

Vec3 g_focus_probe{};
 // DAT_0050DD80, DAT_0050DD84, DAT_0050DD88

Vec3 mul_mat(const Mat34& a, Vec3 p)
{
    return Vec3{p.x * a.m[0] + p.y * a.m[1] + p.z * a.m[2] + a.m[3],
                p.x * a.m[4] + p.y * a.m[5] + p.z * a.m[6] + a.m[7],
                p.x * a.m[8] + p.y * a.m[9] + p.z * a.m[10] + a.m[11]};
}


// Xbox 0x00152B30
float engine_square_root(float value)
{
    // Xbox 0x00152B30
    // Square root. 0x00153450 seeds a reciprocal square root from
    // ((0x7F000000 - bits) >> 1) + 0x1FC00000, multiplies by 1/sqrt(2) when
    // bit 0x08000000 is clear, and takes two Newton steps. Two further steps
    // and a multiply by the input turn that into the root. A non-positive
    // input is returned unchanged.
    if (!(value > 0.f))
    {
        return value;
    }
    const u32 bits = std::bit_cast<u32>(value);
    float y = std::bit_cast<float>(((0x7F000000u - bits) >> 1) + 0x1FC00000u);
    if ((bits & 0x08000000u) == 0u)
    {
        y *= std::bit_cast<float>(u32{0x3F3504F3u});
    }
    const float half = value * 0.5f;
    y = (1.5f - half * y * y) * y;
    y = (1.5f - half * y * y) * y;
    y = (1.5f - half * y * y) * y;
    y = (1.5f - half * y * y) * y;
    return y * value;
}


float length3(Vec3 d)
{
    return engine_square_root(d.x * d.x + d.y * d.y + d.z * d.z);
}


float dist_xz(float x, float z, float cx, float cz)
{
    const float dx = x - cx;
    const float dz = z - cz;
    return engine_square_root(dx * dx + dz * dz);
}


Vec3 focus_world_pos(CamFocus& host)
{
    // 0x000412A0. Attached and live: matrix times the probe. A dead attach is
    // released (0x000663E0) and the unattached path copies host+0x08.
    if (host.attach != nullptr)
    {
        const bool child = host.attach->child_live;
        const bool linked = host.link != nullptr && host.link->live;
        if (child || linked)
        {
            return mul_mat(host.world, g_focus_probe);
        }
        host.attach = nullptr;
    }
    return host.position;
}


const CamShapeNode* find_typed(const CamFocus& host, i32 type, const CamSlotEntry** entry_out)
{
    if (entry_out != nullptr)
    {
        *entry_out = nullptr;
    }
    if (host.slots == nullptr || host.slots->header == nullptr)
    {
        return nullptr;
    }
    for (const CamSlotEntry& entry : host.slots->header->entries)
    {
        if (entry.type != type)
        {
            continue;
        }
        if (entry_out != nullptr)
        {
            *entry_out = &entry;
        }
        return entry.shape;
    }
    return nullptr;
}


float default_extent(const CamFocus& host, bool height)
{
    // 0x0003DB30 reads type-5 +0x24. 0x0003DB90 reads type-5 +0x20.
    // Both scale by host+0x200 and host+0x38, then take the absolute value.
    const CamSlotEntry* entry = nullptr;
    find_typed(host, 5, &entry);
    if (entry == nullptr || !entry->scale_live)
    {
        return 0.f;
    }
    const float scale = height ? entry->height_scale : entry->radius_scale;
    return std::fabs(host.scale_200 * scale * host.scale_38);
}


const CamBonePose* focus_pose(const CamFocus& host)
{
    if (host.slots == nullptr)
    {
        return nullptr;
    }
    if (host.slots->at_168 != nullptr)
    {
        return host.slots->at_168;
    }
    return host.slots->at_164;
}


Vec3 xform_shape(u16 bone, const CamBonePose* pose, const Mat34& world, Vec3 local)
{
    // 0x00056790. Bone 0xFFFF copies the local point. Otherwise 0x000315F0
    // indexes the pose table (stride 0x30). The actor matrix always follows.
    Vec3 p = local;
    if (bone != 0xFFFF && pose != nullptr && pose->table_live)
    {
        const i32 index = static_cast<i16>(bone);
        if (index >= 0 && static_cast<size_t>(index) < pose->bones.size())
        {
            p = mul_mat(pose->bones[static_cast<size_t>(index)], p);
        }
    }
    return mul_mat(world, p);
}


void push_sphere(CamFocus& host, const CamShapeNode& node, const CamBonePose* pose)
{
    // 0x00056330. World position of node+0x14, radius is the world length of
    // the pose's +0x10 offset along local x. The radius point's bone id sits
    // on the pose; the lookup object is the position just written, so the
    // actor matrix is the transform that applies.
    const Vec3 center = xform_shape(node.bone, pose, host.world, node.point);
    Vec3 arm = node.point;
    arm.x += pose != nullptr ? pose->along_x : 0.f;
    const u16 arm_bone = pose != nullptr ? pose->bone : static_cast<u16>(0xFFFF);
    const Vec3 arm_w = xform_shape(arm_bone, nullptr, host.world, arm);
    CamBoundRec rec;
    rec.kind = 1;
    rec.x = center.x;
    rec.y = center.y;
    rec.z = center.z;
    rec.radius = length3(Vec3{center.x - arm_w.x, center.y - arm_w.y, center.z - arm_w.z});
    host.recs.push_back(rec);
}


void push_cylinder(CamFocus& host, const CamShapeNode& node, const CamBonePose* pose)
{
    // 0x00055AA0 / 0x00055B30. Point at node+0x18, radius from the local-x
    // offset at node+0x10, half-extent from the local-y offset at node+0x14.
    // The record stores the bottom and the full height.
    const Vec3 base = xform_shape(node.bone, pose, host.world, node.point);
    Vec3 along_x = node.point;
    along_x.x += node.radius_x;
    Vec3 along_y = node.point;
    along_y.y += node.half_y;
    const Vec3 wx = xform_shape(node.bone, pose, host.world, along_x);
    const Vec3 wy = xform_shape(node.bone, pose, host.world, along_y);
    const float reach = length3(Vec3{base.x - wx.x, base.y - wx.y, base.z - wx.z});
    const float hy = length3(Vec3{base.x - wy.x, base.y - wy.y, base.z - wy.z});
    CamBoundRec rec;
    rec.kind = 4;
    rec.x = base.x;
    rec.y = base.y - hy;
    rec.z = base.z;
    rec.height = hy - (-hy);
    rec.reach = reach;
    host.recs.push_back(rec);
}


void write_bound(CamBound& out, float x, float y, float z, float height, float radius)
{
    out.kind = 4;
    out.size = 0x20;
    out.flag = 1;
    out.x = x;
    out.y = y;
    out.z = z;
    out.pad0 = 0.f;
    out.height = height;
    out.pad1 = 0.f;
    out.radius = radius;
}


// Xbox 0x000426F0
int camera_build_focus_bound(CamFocus& host, CamBound& out, u32 mask, i32 average)
{
    // Xbox 0x000426F0
    const Vec3 probed = focus_world_pos(host);
    const CamSlotEntry* shape_entry = nullptr;
    find_typed(host, 4, &shape_entry);
    if (shape_entry == nullptr || shape_entry->shape == nullptr)
    {
        write_bound(out, probed.x, probed.y, probed.z, default_extent(host, true), default_extent(host, false));
        return 0;
    }

    const CamBonePose* pose = focus_pose(host);
    host.recs.clear();
    for (const CamShapeNode* node = shape_entry->shape; node != nullptr; node = node->next)
    {
        if ((node->mask & mask) == 0)
        {
            continue;
        }
        if (node->kind == 0)
        {
            push_sphere(host, *node, pose);
        }
        else if (node->kind == 1)
        {
            push_cylinder(host, *node, pose);
        }
    }
    if (host.recs.empty())
    {
        return 0;
    }

    Vec3 center = probed;
    if (average != 0)
    {
        float sx = 0.f;
        float sy = 0.f;
        float sz = 0.f;
        for (const CamBoundRec& rec : host.recs)
        {
            sx += rec.x;
            sz += rec.z;
            if (rec.kind == 4)
            {
                sy += rec.y + rec.height;
            }
            else
            {
                sy += rec.y;
            }
        }
        const float inv = 1.f / float(host.recs.size());
        center = Vec3{sx * inv, sy * inv, sz * inv};
    }
    else
    {
        center = focus_world_pos(host);
    }

    float min_y = center.y;
    float max_y = center.y;
    float max_r = 0.f;
    for (const CamBoundRec& rec : host.recs)
    {
        if (rec.kind == 1)
        {
            const float horiz = dist_xz(rec.x, rec.z, center.x, center.z) + rec.radius;
            if (horiz > max_r)
            {
                max_r = horiz;
            }
            const float bottom = rec.y - rec.radius;
            if (bottom <= min_y)
            {
                min_y = bottom;
            }
            const float top = rec.radius + rec.y;
            if (max_y < top)
            {
                max_y = top;
            }
        }
        else if (rec.kind == 4)
        {
            const float horiz = dist_xz(rec.x, rec.z, center.x, center.z) + rec.reach;
            if (horiz > max_r)
            {
                max_r = horiz;
            }
            if (rec.y <= min_y)
            {
                min_y = rec.y;
            }
            const float top = rec.height + rec.y;
            if (max_y < top)
            {
                max_y = top;
            }
        }
    }
    write_bound(out, center.x, min_y, center.z, max_y - min_y, max_r);
    return int(host.recs.size());
}


Mat34 mul_affine(const Mat34& a, const Mat34& b)
{
    // 0x001041B0. 3x4 affine product, out = A * B.
    Mat34 o;
    for (int row = 0; row < 3; ++row)
    {
        const float ax = a.m[row * 4 + 0];
        const float ay = a.m[row * 4 + 1];
        const float az = a.m[row * 4 + 2];
        const float aw = a.m[row * 4 + 3];
        o.m[row * 4 + 0] = ax * b.m[0] + ay * b.m[4] + az * b.m[8];
        o.m[row * 4 + 1] = ax * b.m[1] + ay * b.m[5] + az * b.m[9];
        o.m[row * 4 + 2] = ax * b.m[2] + ay * b.m[6] + az * b.m[10];
        o.m[row * 4 + 3] = ax * b.m[3] + ay * b.m[7] + az * b.m[11] + aw;
    }
    return o;
}


const Mat34* pose_bone(const CamBonePose* pose, i32 index)
{
    // 0x00031530. +4 set: packed matrices at +0x10, stride 0x30.
    // +4 clear packs a column-major 4x4 (stride 0x40) into that same shape.
    // The port keeps the packed table, so a clear flag has no bone.
    if (pose == nullptr || !pose->table_live || index < 0)
    {
        return nullptr;
    }
    const size_t slot = static_cast<size_t>(index);
    if (slot >= pose->bones.size())
    {
        return nullptr;
    }
    return &pose->bones[slot];
}


bool anchor_bone_point(const CamFocus& host, u16 index, Vec3 probe, Vec3& out)
{
    // 0x0003AEB0. Marker translation (stride 0x20 at [slot+0x1bc]+0x24)
    // becomes T. Bone matrix is 0x00031530 on the same index. Result is
    // (world * bone * T) * probe. Probe is DAT_0050DD80. Writes when the
    // marker record is present.
    if (host.slots == nullptr || host.slots->markers == nullptr)
    {
        return false;
    }
    const std::vector<CamMarker>& items = host.slots->markers->items;
    if (static_cast<size_t>(index) >= items.size())
    {
        return false;
    }
    Mat34 bone{};
    if (const Mat34* found = pose_bone(focus_pose(host), static_cast<i32>(index)))
    {
        bone = *found;
    }
    Mat34 trans{};
    trans.m[3] = items[index].point.x;
    trans.m[7] = items[index].point.y;
    trans.m[11] = items[index].point.z;
    const Mat34 placed = mul_affine(mul_affine(host.world, bone), trans);
    out = mul_mat(placed, probe);
    return true;
}


// Xbox 0x0011BE40
void camera_update_anchor(CamAnchor& node)
{
    // Xbox 0x0011BE40
    // EDX is the anchor. Mode +0x14:
    //   0: locator position, optional bone, then the actor matrix (0x00051CE0)
    //   1: bone/marker probe into +0x28 (0x0003AEB0, EBX = DAT_0050DD80)
    //   2: index 0xFFFF maps +0x1c by the actor matrix (0x000415F0);
    //      any other index takes the mode-1 probe path
    if (node.host == nullptr)
    {
        return;
    }
    CamFocus& host = *node.host;
    if (node.mode == 0)
    {
        if (node.locator == nullptr)
        {
            return;
        }
        Vec3 p{node.locator->x, node.locator->y, node.locator->z};
        if (node.locator->bone >= 0)
        {
            if (const Mat34* bone = pose_bone(focus_pose(host), node.locator->bone))
            {
                p = mul_mat(*bone, p);
            }
        }
        node.out = mul_mat(host.world, p);
        return;
    }
    if (node.mode == 1)
    {
        anchor_bone_point(host, node.index, g_focus_probe, node.out);
        return;
    }
    if (node.mode == 2)
    {
        if (node.index == 0xFFFF)
        {
            node.out = mul_mat(host.world, node.local);
            return;
        }
        anchor_bone_point(host, node.index, g_focus_probe, node.out);
    }
}


// DAT_00509B74. Nonzero eases +0x318 toward +0x324; zero snaps.
i32 g_ease_live = 0;

// DAT_0050E768. Nonzero skips the +0x930 decay inside the base update.
i32 g_block_decay = 0;

// DAT_0050E764. 0x00117400 counts the other mask bit beside DAT_0050E768.
i32 g_count_e764 = 0;

// Byte at [DAT_0051248C]+0x30F144. Zero skips that same decay.
u8 g_decay_arm = 0;

ActorMark* g_actor_mark = nullptr;

// DAT_005077A8. XYZ at [0], [1], [2]. A null pointer faults on Xbox.
const float* g_listener_pos = nullptr;

// DAT_0042742C, copied into the tint block while the host is inside its range.
u32 g_tint_src = 0x800000ffu;

// DAT_00504450 / 54 / 58, then DAT_0050446C / 70 / 74.
struct RangeTint
{
    u32 c0 = 0;
    u32 c1 = 0;
    u32 c2 = 0;
    float n0 = 0.f;
    float n1 = 0.f;
    i32 flag = 0;
};

RangeTint g_range_tint;

// DAT_004E1C5C. 0x000703A0 writes the flag then the world position.
struct AidHit
{
    u32 flags = 0;
    Vec3 pos{};
};

AidHit g_aid_hit;

RangeLink* g_range_links = nullptr;

BodyLink* g_body_links = nullptr;

AltLink* g_alt_links = nullptr;

// Head standing in for DAT_00511B54. 0x0003ED70 appends host+0x908 here.
RetireNode g_retire_head;


constexpr float kSixPi = std::bit_cast<float>(0x4196cbe4u);

constexpr float kSpan = std::bit_cast<float>(0x3f733333u);
 // 0.95
constexpr float kThird = std::bit_cast<float>(0x3eaaaaabu);

constexpr float kEaseStep = std::bit_cast<float>(0x3dcccccdu);
 // 0.1
constexpr float kKick = std::bit_cast<float>(0x3e4ccccdu);
 // 0.2

CamBlend* host_blend(const CamFocus& host)
{
    if (host.bind == nullptr || host.bind->at_58 == nullptr)
    {
        return nullptr;
    }
    return host.bind->at_58->blend;
}


int slot_bone(const CamSlotObject* slots)
{
    // 0x00052FC0 with a zero word. First type-9 entry, then the byte at its blob.
    // 0xFF and a missing entry become -1.
    if (slots == nullptr || slots->header == nullptr)
    {
        return -1;
    }
    for (const CamSlotEntry& entry : slots->header->entries)
    {
        if (entry.type != 9)
        {
            continue;
        }
        if (entry.bone_byte == nullptr)
        {
            return -1;
        }
        if (entry.bone_byte[0] == 0xFF)
        {
            return -1;
        }
        return int(entry.bone_byte[0]);
    }
    return -1;
}


float host_range(const CamFocus& host)
{
    // 0x00070410. Trace of the bone matrix times the mean of +0x30/+0x34/+0x38,
    // each scaled by +0x200 and by 1/3. A missing pose faults on Xbox.
    const CamBonePose* pose = nullptr;
    if (host.slots != nullptr)
    {
        pose = host.slots->at_168 != nullptr ? host.slots->at_168 : host.slots->at_164;
    }
    const Mat34* mat = pose_bone(pose, slot_bone(host.slots));
    if (mat == nullptr)
    {
        return 0.f;
    }
    const float s30 = host.extent_30 * host.scale_200;
    const float s34 = host.at_34 * host.scale_200;
    const float s38 = host.scale_38 * host.scale_200;
    const float trace = mat->m[10] + mat->m[5] + mat->m[0];
    return (trace * kThird) * ((s38 + s34 + s30) * kThird);
}


void host_base(CamFocus& host)
{
    // 0x000420D0. This override calls the base slot first.
    // 0x00030BA0 and 0x00030C70 run only when +0x50 is live, and 0x0003CD00
    // walks +0x1B8. Both stay idle while those pointers are null. The list at
    // +0x1D8 is the same. 0x00165A00 runs when slot+0x29C is live.
    for (CamAnchor* node = host.anchors; node != nullptr; node = node->next)
    {
        camera_update_anchor(*node);
    }
    if (g_ease_live != 0)
    {
        host.ease.x = host.ease.x + (host.ease_to.x - host.ease.x) * kEaseStep;
        host.ease.y = host.ease.y + (host.ease_to.y - host.ease.y) * kEaseStep;
        host.ease.z = host.ease.z + (host.ease_to.z - host.ease.z) * kEaseStep;
    }
    else
    {
        host.ease = host.ease_to;
    }
    const float step = sim_step(g_sim_dt);
    if (g_block_decay == 0 && g_decay_arm != 0 && host.host_gate > 0.f)
    {
        host.host_gate -= step;
        if (host.host_gate <= 0.f)
        {
            host.host_gate = 0.f;
        }
    }
    float rate = 0.f;
    if (!(host.host_gate > 0.f))
    {
        rate = host.host_rate;
    }
    host.accum += rate * step;
    host.accum_n += 1;
}


u8 index_byte(float scaled)
{
    // 0x001673D8 (__ftol). The caller keeps the low 8 bits.
    if (!std::isfinite(scaled))
    {
        return 0;
    }
    const float chopped = std::trunc(scaled);
    if (chopped >= 2147483647.f || chopped <= -2147483648.f)
    {
        return 0;
    }
    return static_cast<u8>(static_cast<i32>(chopped));
}


void pick_index(CamFocus& host, float sample)
{
    if (host.picker == nullptr)
    {
        return;
    }
    const float wave = std::fabs(std::sin(sample * kSixPi));
    const float scaled = wave * (float(host.picker->count) - 1.f);
    const u8 idx = index_byte(scaled);
    if (idx < host.picker->count)
    {
        host.picker->selected = idx;
    }
}


bool span_crossed(float hi, float lo)
{
    if (hi <= lo)
    {
        return (lo <= kSpan) || (hi > kSpan);
    }
    return (lo <= kSpan) && (hi > kSpan);
}


bool at_travel_end(const CamBlend& blend)
{
    if (blend.dir < 0.f)
    {
        return blend.value <= 0.f;
    }
    return blend.value >= 1.f;
}


void retire_host(CamFocus& host)
{
    // 0x0003ED70 links +0x908 onto DAT_00511B58 when that dword is still null,
    // then 0x0003CBB0 / 0x0003E850 / 0x0003C700. The link is what blocks a repeat.
    if (host.retire.prev != nullptr)
    {
        return;
    }
    RetireNode* tail = &g_retire_head;
    while (tail->next != nullptr)
    {
        tail = tail->next;
    }
    tail->next = &host.retire;
    host.retire.prev = tail;
    host.retire.next = nullptr;
}


void release_tree(CamFocus* node)
{
    // 0x00112D10. Child at +8, next at +4, object is the link minus 0x918.
    while (node != nullptr)
    {
        if (node->release_child != nullptr)
        {
            release_tree(node->release_child);
        }
        CamFocus* next = node->release_next;
        if (node->retire.prev == nullptr)
        {
            if (node->on_note != nullptr)
            {
                node->on_note(node, 0x80);
            }
            if (node->retire.prev == nullptr)
            {
                retire_host(*node);
            }
        }
        node = next;
    }
}


void release_host(CamFocus& host)
{
    // 0x00112CC0.
    if (host.release_child != nullptr)
    {
        release_tree(host.release_child);
    }
    if (host.retire.prev != nullptr)
    {
        return;
    }
    if (host.on_note != nullptr)
    {
        host.on_note(&host, 0x80);
    }
    if (host.retire.prev != nullptr)
    {
        return;
    }
    retire_host(host);
}


void post_cross(CamFocus& host)
{
    // 0x000703A0. World position lands in DAT_004E1C60 and the flag becomes 1.
    // 0x00053170 continues only when +0x954 starts with "aid_".
    g_aid_hit.flags = 0;
    g_aid_hit.pos = focus_world_pos(host);
    g_aid_hit.flags = 1;
}


void notify_range(const Vec3& pos, float radius)
{
    // 0x00122D80. Squared test is strict. Note id is 0x5B through vtable+0xC.
    const float limit = radius * radius;
    for (RangeLink* link = g_range_links; link != nullptr; link = link->next)
    {
        if (link->host == nullptr)
        {
            continue;
        }
        const Vec3 at = focus_world_pos(*link->host);
        const float dx = at.x - pos.x;
        const float dy = at.y - pos.y;
        const float dz = at.z - pos.z;
        if (limit > dx * dx + dy * dy + dz * dz && link->on_note != nullptr)
        {
            link->on_note(link->host, 0x5b);
        }
    }
}


// Xbox 0x00070006
void camera_tick_focus_host(CamFocus& host)
{
    // Xbox 0x00070006
    // Retail entry is 0x0006FF90 (vtable slot +0x10, over 0x000420D0). Ghidra
    // split the body at the fsub that scales the picker index. ESI is the host
    // passed on the stack. 0x000412A0 writes the world position used for the
    // range test against DAT_005077A8.
    host_base(host);
    // A live bind with a null +0x58 faults on Xbox before the picker.
    if (host.bind != nullptr && host.bind->at_58 == nullptr)
    {
        return;
    }

    float sample = 0.f;
    if (host.bind != nullptr && host.bind->at_58->blend != nullptr)
    {
        sample = host.bind->at_58->blend->value;
    }
    pick_index(host, sample);

    if (host.pulse_on != 0)
    {
        float rate = 0.f;
        if (!(host.host_gate > 0.f))
        {
            rate = host.host_rate;
        }
        host.pulse -= rate * sim_step(g_sim_dt);
        if (host.pulse <= 0.f)
        {
            host.pulse += 0.5f;
            host.pulse_left -= 1;
            if (host.pulse_left == 0)
            {
                host.pulse_on = 0;
            }
        }
    }

    const u32 previous = host.marker;
    u32 bit = 0;
    if (g_actor_mark != nullptr)
    {
        bit = (u32(g_actor_mark->byte) >> 4) & 1u;
    }
    host.marker = bit;
    if (bit != 0 && previous == 0)
    {
        host.marker_kick = kKick;
    }

    host.range = host_range(host);
    const Vec3 world = focus_world_pos(host);
    u32 inside = 0;
    if (g_listener_pos != nullptr)
    {
        const Vec3 d{world.x - g_listener_pos[0], world.y - g_listener_pos[1], world.z - g_listener_pos[2]};
        inside = length3(d) <= host.range ? 1u : 0u;
    }
    host.in_range = inside;
    if (inside != 0)
    {
        g_range_tint.c0 = g_tint_src;
        g_range_tint.c1 = g_tint_src;
        g_range_tint.c2 = g_tint_src;
        g_range_tint.n0 = -1.f;
        g_range_tint.n1 = -1.f;
        g_range_tint.flag = 1;
    }
    notify_range(world, host.range);

    CamBlend* blend = host_blend(host);
    if (host.bind != nullptr && host.bind->at_58 != nullptr && blend != nullptr && blend->value != blend->previous)
    {
        float hi = blend->value;
        float lo = blend->previous;
        if (blend->dir < 0.f)
        {
            hi = blend->previous;
            lo = blend->value;
        }
        if (span_crossed(hi, lo))
        {
            post_cross(host);
        }
    }

    if (host.bind == nullptr)
    {
        release_host(host);
        return;
    }
    if (host.bind->at_58 == nullptr)
    {
        return;
    }
    if (blend == nullptr || (blend->mark == 1 && at_travel_end(*blend)))
    {
        release_host(host);
    }
}


// Xbox 0x0012B160
void camera_store_follow_bounds(CamFollow& self, const CamBound& primary, const CamBound* secondary, const Vec3& point, u8 mark)
{
    // Xbox 0x0012B160
    // ESI is host+0x8f8. 0x0010AA60 copies 0x20 bytes to +0x1c from the first
    // stack argument, then to +0x3c from EBX. A null EBX repeats the first
    // source. EDI's three floats land at +0x5c. The stack byte lands at +0x19.
    self.at_1c = primary;
    if (secondary != nullptr)
    {
        self.at_3c = *secondary;
    }
    else
    {
        self.at_3c = primary;
    }
    self.at_5c = point;
    self.mark = mark;
}


bool follow_shift_set(const CamFocus& host)
{
    // camera+0x15c -> +8 -> byte +7. Bit 0 keeps the stored follow cylinder.
    if (host.gate_15c == nullptr || host.gate_15c->at_8 == nullptr)
    {
        return false;
    }
    return (host.gate_15c->at_8->bytes[7] & 1u) != 0;
}


void focus_bounds_tick()
{
    // 0x00025C50 calls camera_build_focus_bound with camera+0x88c and flag 0 whenever the
    // slot header is present. Flag 0 keeps the actor position as the center.
    // When host+0x8f8 is set, the follow look-at packets go out through
    // camera_store_follow_bounds first. Bit 0 of the gate shifts the stored cylinder; otherwise
    // the primary cylinder is camera_build_focus_bound with camera+0x890 and the average flag.
    if (g_cam_focus.slots == nullptr || g_cam_focus.slots->header == nullptr)
    {
        return;
    }
    if (g_cam_focus.follow != nullptr)
    {
        CamFollow& follow = *g_cam_focus.follow;
        const Vec3 world = focus_world_pos(g_cam_focus);
        CamBound actor{};
        write_bound(actor, world.x, world.y, world.z, g_cam_focus.at_34 * g_cam_focus.mul_884,
                    g_cam_focus.scale_38 * g_cam_focus.mul_888);
        CamBound primary{};
        if (follow_shift_set(g_cam_focus))
        {
            write_bound(primary, follow.at_1c.x + g_cam_focus.nudge_704.x, follow.at_1c.y + g_cam_focus.nudge_704.y,
                        follow.at_1c.z + g_cam_focus.nudge_704.z, follow.at_1c.height, follow.at_1c.radius);
        }
        else
        {
            camera_build_focus_bound(g_cam_focus, primary, g_cam_focus.mask_890, 1);
        }
        camera_store_follow_bounds(follow, primary, &actor, g_cam_focus.at_3c, static_cast<u8>(g_cam_focus.at_cec));
    }
    camera_build_focus_bound(g_cam_focus, g_cam_focus.bound, g_cam_focus.shape_mask, 0);
}

} // namespace scene_detail


float scene_sim_step(float dt)
{
    return sim_step(dt);
}

} // namespace gbtg
