#pragma once

#include "../scene/scene_types.h"

namespace gbtg
{
namespace scene_detail
{
extern CameraWash g_camera_wash;

// Xbox 0x0001DD00
void camera_tick_wash(CameraWash& c, float dt); // Xbox 0x0001DD00

extern CameraPairs g_camera_pairs;

extern u32 g_pair_cache[256];

// DAT_005078D8

// Xbox 0x00025130
void camera_apply_pairs(CameraPairs& cam); // Xbox 0x00025130

extern CameraChannels g_camera_channels;

// Xbox 0x0001EC20
void camera_apply_channels(CameraChannels& cam); // Xbox 0x0001EC20

// Xbox 0x0001EEC0
void camera_apply_default_blend(CameraChannels& cam); // Xbox 0x0001EEC0

// camera_apply (0x00025AE0) calls 0x0001FB40 with ESI = camera, after the
// channel pass and before the wash. DAT_004312dc > 0 freezes the step
// (BanjoCam). Otherwise the step is DAT_005096C0 * DAT_004312C4 (dt * scale).
// Host is camera+8. Its +0x930 > 0 forces the decay rate to 0; otherwise the
// rate is host+0x92c. Those host fields stay 0 until that object is filled in.
extern float g_sim_freeze;

// DAT_004312dc
extern float g_sim_scale;

// DAT_004312c4
extern float g_sim_dt;

extern CameraPulse g_camera_pulse;

extern PulseNode* g_pulse_nodes;

// Xbox 0x0001FB40
void camera_tick_pulse(CameraPulse& cam, float dt); // Xbox 0x0001FB40

extern CameraTimers g_camera_timers;

// Xbox 0x00027960
void camera_tick_timers(CameraTimers& cam, CameraPulse& pulse, float dt); // Xbox 0x00027960

extern CamFocus g_cam_focus;

// Xbox 0x0011BE40
void camera_update_anchor(CamAnchor& node); // Xbox 0x0011BE40

// DAT_0050E764 and DAT_0050E768, the two counts 0x00117400 keeps.
extern std::array<u32, 2> g_pause_references;

// DAT_0050E768. Nonzero skips the +0x930 decay inside the base update.
extern i32& g_block_decay;

// DAT_0050E764. 0x00117400 counts the other mask bit beside DAT_0050E768.
extern i32& g_count_e764;

// Byte at [DAT_0051248C]+0x30F144. Zero skips that same decay.
extern u8 g_decay_arm;

extern ActorMark* g_actor_mark;

extern RangeLink* g_range_links;

extern BodyLink* g_body_links;

extern AltLink* g_alt_links;

u8 index_byte(float scaled);

void release_host(CamFocus& host);

void focus_bounds_tick();

} // namespace scene_detail
float scene_sim_step(float dt);

} // namespace gbtg
