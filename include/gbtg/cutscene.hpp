#pragma once

#include "gbtg/types.hpp"

namespace gbtg
{

// One channel. `keys` is float[count]; `duration` is the channel's own seconds (0x000514C0 divides by it).
struct CutsceneCurve
{
    std::vector<float> keys;
    float duration = 0.f;
};

// BNL type 7 (`aid_cutscene_*`).
//   u16 keyframes; u8 flag; u8 tracks; f32 duration_seconds; then reloc tables.
// Camera list entry is { u32 name, u32 channels } (0x0004E0F6, stride 8).
// Each channel is 12 bytes: u16 count, u16 pad, f32 duration, u32 data.
// data is relative to the channel table; load adds the table address (0x0004F700).
// Channels 0..2 are position (0x0004F770). Channels 3..6 are a quaternion (0x0004F8C0).
// Channel 7 is the scalar stored on the camera at +0x24 (35 on the attract clips).
// One actor row in the type-7 track table (stride 0x64, count = header tracks).
// +0x00 model, +0x04 anim, +0x1C six {count, duration, data} curves.
// Curves 0..2 are position. Curves 3..5 are scale (1 on the title books).
struct CutsceneActor
{
    std::string model;
    std::string anim;
    std::vector<CutsceneCurve> curves;
    std::vector<u8> mina; // skeletal block embedded at the track row + 8
};

struct Cutscene
{
    std::string name;
    float duration = 0.f;
    u16 keyframes = 0;
    u8 flag = 0;
    u8 tracks = 0;
    std::string camera_name;
    std::vector<CutsceneCurve> camera; // dense curves only, in file order
    std::vector<CutsceneActor> actors;
    std::vector<std::string> cameras;
    std::vector<std::string> models;
    std::vector<std::string> anims;
    std::vector<std::string> aids;
};

struct CutsceneQueue
{
    struct CameraEvent
    {
        float time = 0.f;
        std::string aid;
    };
    std::vector<Cutscene> items;
    size_t index = 0;
    float time = 0.f;
    bool loop = true;
    bool playing = false;
    std::string camera_event_clip;
    std::vector<CameraEvent> camera_events;
    size_t camera_event_index = 0;
    float camera_event_time = -1.f;
    Cutscene camera_wobble;
    float camera_wobble_time = 0.f;
};

Cutscene parse_cutscene(std::span<const u8> cpu, std::string_view name = {});

float sample_curve(const CutsceneCurve& curve, float time, float duration);
// Position, quaternion, and fov (radians) from the camera channels. False when the clip has none.
bool sample_cutscene_camera(const Cutscene& cut, float time, Vec3& position, Quat& orientation, float& fov_y);

} // namespace gbtg
