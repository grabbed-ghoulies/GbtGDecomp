#pragma once

#include "gbtg/entity.h"
#include "gbtg/object_factory.h"

namespace gbtg
{
struct ActorTrailContext
{
    ObjectFactoryRegistry& factory;
    std::vector<FactoryObject>& objects;
    u32 actor_address;
    float& pause;
    float& frame_delta;
    float& time_scale;
    i32& pal;
    i32& pal_override;
    const char* default_name; // DAT_00546208
};
void initialize_actor_effect_parameters(u8* parameters, const char* default_name); // Xbox 0x000411A0
bool request_actor_trail(Actor& actor, float scale, float& timer, i32 frequency,
    ActorTrailContext& context); // Xbox 0x0013B030
void request_actor_video_rate_trail(Actor& actor, float scale, float& timer,
    ActorTrailContext& context); // Xbox 0x0013B1B0
}
