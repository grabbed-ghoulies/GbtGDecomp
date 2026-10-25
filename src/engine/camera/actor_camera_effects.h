#pragma once

#include "../entities/ghouly_box_decoder.h"
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include "gbtg/clock.h"

#include <array>
#include <span>

namespace gbtg
{
struct ActorCameraEffect
{
    ActorCameraEffect* previous;
    ActorCameraEffect* next;
    u8 payload[0x7C]; // Xbox +8 through +0x83, unchanged raw layout.
};

// One of a camera shake's two axes (the two pad motors), moving through the resource's phases: up to a first level,
// on to a second, held, then down to a last, while its wave widens (+0x10) and its angle turns (+0x14).
struct ActorCameraShakeAxis
{
    u32 phase; // +0x00, 5 once the axis is done
    float value; // +0x04
    float target; // +0x08
    float rate; // +0x0C, the hold time left in phase 3
    float reach; // +0x10, the wave front's distance
    float angle; // +0x14
    float amplitude; // +0x18
    float middle; // +0x1C
};

// DAT_00507D80's rows (Xbox stride 0x54).
struct ActorCameraShake
{
    ActorCameraShakeAxis axes[2]; // +0x00
    const u8* resource; // +0x40, two 0x3C-byte axis records
    Vec3 position; // +0x44
    float scale; // +0x50
};

// DAT_00507D84, stride 0x1C, one per port: the view the port's shakes are measured from, its enables and its two
// motors' strongest shake this frame.
struct PadShakeRecord
{
    Vec3 position; // +0x00
    u32 unk_0c; // +0x0C
    u32 enabled; // +0x10
    float motors[2]; // +0x14, +0x18
};

extern ActorCameraEffect g_actor_camera_effects; // DAT_004E1734
extern GhoulySpawnPool* g_actor_camera_shakes; // DAT_00507D80
extern std::array<PadShakeRecord, 4> g_pad_shakes; // DAT_00507D84
extern u32 g_pad_shakes_held; // DAT_00507DF4

void release_actor_camera_effects(); // Xbox 0x00048F10
// The frame's camera effects: each starts (state 1), swings and fades (state 2, gone below 0.001 unless its decay
// is 1) or follows its keyed curve over its length (state 3, gone at the end).
void tick_actor_camera_effects(float step); // Xbox 0x00048F90
// The scene teardown's: each camera shake's resource (+0x40) lets go, the DAT_00507D80 pool is freed and the four pads'
// rumble stops (0x00155930).
void release_actor_camera_shakes(AnimationResourceRegistry& registry); // Xbox 0x000FAE40
ActorCameraEffect* allocate_actor_camera_effect(u8 type); // Xbox 0x000494B0
ActorCameraEffect* create_positioned_camera_effect(u8 type, float size,
    float value, const Vec3& position); // Xbox 0x00049520
ActorCameraEffect* load_actor_camera_effect(const char* name, const Vec3* position,
    AnimationResourceRegistry& registry, ResourceErrorState& error); // Xbox 0x000493D0
ActorCameraShake* allocate_actor_camera_shake(); // Xbox 0x000FB7C0
void load_actor_camera_shake(const char* name, const Vec3& position, float scale,
    AnimationResourceRegistry& registry, ResourceErrorState& error); // Xbox 0x000FB120
// Boot's: the shake pool, and every port's shakes and rumble switched on.
void boot_actor_camera_shakes(std::span<RuntimePadRumbleCommand, 4> commands); // Xbox 0x000FADE0
// The scene load's: a fresh shake pool, and the ports' shakes on unless the unlock flags' bit 4 turns them off.
void reset_actor_camera_shakes(u8 unlock_flags, std::span<RuntimePadRumbleCommand, 4> commands); // Xbox 0x000FAEE0
// The frame's shakes: each port measures from the view, each shake steps (a finished one lets its resource go) and
// shakes the selected port's motors by its distance, and the strongest become the ports' rumble commands.
void tick_actor_camera_shakes(float step, const float* view, u32 port_mask,
    std::span<RuntimePadRumbleCommand, 4> commands, AnimationResourceRegistry& registry); // Xbox 0x000FAF70
}
