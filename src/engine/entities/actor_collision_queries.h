#pragma once
#include "collision_shape_dispatch.h"

namespace gbtg
{
struct RuntimeActorCollision
{
    Actor& actor;
    std::span<const u8> bytes;
    RuntimeCollisionShapeDispatch& shapes;
};
u32 query_runtime_actor_physical_segment(RuntimeActorCollision& context); // Xbox 0x0003DF70
u32 query_runtime_actor_physical_sweep(RuntimeActorCollision& context); // Xbox 0x0003E110
u32 collect_runtime_actor_physical_contacts(RuntimeActorCollision& context, u32 filter); // Xbox 0x0003E050
u32 query_runtime_actor_model_sweep(RuntimeActorCollision& context); // Xbox 0x0003E200
u32 collect_runtime_actor_model_contacts(RuntimeActorCollision& context); // Xbox 0x0003E3C0
}
