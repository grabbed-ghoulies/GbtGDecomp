#pragma once

#include "../entities/ghouly_box_decoder.h"
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"

namespace gbtg
{
struct ActorCameraEffect
{
    ActorCameraEffect* previous;
    ActorCameraEffect* next;
    u8 payload[0x7C]; // Xbox +8 through +0x83, unchanged raw layout.
};

struct ActorCameraShake
{
    float axes[2][8];
    const u8* resource;
    Vec3 position;
    float scale;
};

extern ActorCameraEffect g_actor_camera_effects; // DAT_004E1734
extern GhoulySpawnPool* g_actor_camera_shakes; // DAT_00507D80

void release_actor_camera_effects(); // Xbox 0x00048F10
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
}
