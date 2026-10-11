#pragma once
#include "body_collision.h"
#include "body_shape_push.h"
#include "body_message_helpers.h"

namespace gbtg
{
struct RuntimePlayerBodyCollision
{
    std::span<u8>& body;
    Actor& actor;
    EntityBitBlock*& flags;
    const EntityMaskHost& mask;
    const i32& floor;
    RuntimeBodyShapeStorage& shapes;
    RuntimeBodyShapePush& push;
    RuntimePlayerCollisionAction*& action; // Body +0x744 virtual message recipient.
    std::span<const u8> scene_rows; // DAT_004EE2B8, rows of 0x154 bytes.
    std::span<entity_detail::SlideRecord, 5> contacts; // DAT_00505040.
    const Vec3& stationary; // DAT_0050DD80.
    const RuntimeActorClock& clock;
};
bool update_runtime_player_body_collision(RuntimePlayerBodyCollision& context); // Xbox 0x00074670
}
