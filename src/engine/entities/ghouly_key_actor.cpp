#include "ghouly_key_actor.h"
#include "actor_transform.h"
#include "../camera/camera_body_slots.h"
#include "gbtg/entity.h"
#include "gbtg/object_factory.h"
#include <cstring>

namespace gbtg
{
namespace
{
GhoulyKeyState& fields_of(std::span<u8> bytes)
{
    return actor_record_view<GhoulyKeyState>(bytes);
}
} // namespace


// Xbox 0x0006E930
void ghouly_key_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(GhoulyKeyState);
    output.parameters = 0x334;
}

// Xbox 0x0006E990
bool initialize_runtime_ghouly_key(RuntimeGhoulyKey& context)
{
    context.callouts(0x4A); // 0x0003BE20, EAX actor +0x8C
    request_audio_event_voice("XACT_SOUNDBANK_GPICKUPDITTIES_KEYAPPEAR", 1, context.voices); // The inlined 0x000395E0.
    fields_of(context.bytes).ditty_delay = 0.4f; // DAT_0032B11C.
    return true;
}

// Xbox 0x0006EA00 after 0x000420D0
void update_runtime_ghouly_key(RuntimeGhoulyKey& context)
{
    Actor& actor = context.actor;
    Vec3 position = actor_world_position(actor); // 0x000412A0
    position.y = position.y + 0.2f; // DAT_0032B1AC.
    const Vec3 turned{actor.euler.x, actor.euler.y + 0.06f, actor.euler.z}; // DAT_0032B60C.
    actor_set_world_position(actor, position); // 0x00041730, ESI the point
    wrap_runtime_euler(turned, actor.euler); // 0x00105AA0
    const u32 stale = 0;
    std::memcpy(context.bytes.data() + 0x2A8, &stale, 4); // Actor +0x2A8, the transform.
    actor.xform_valid = 0;
    if (!(fields_of(context.bytes).ditty_delay > 0.f))
    {
        return;
    }
    // Inlined 0x00042320.
    fields_of(context.bytes).ditty_delay = fields_of(context.bytes).ditty_delay - runtime_actor_step(actor, context.clock);
    if (!(0.f > fields_of(context.bytes).ditty_delay))
    {
        return;
    }
    request_audio_event_voice("XACT_SOUNDBANK_GSTREAMSDITTIES_FINDKEYEVENT", 1, context.voices); // Inlined 0x000395E0.
}
}
