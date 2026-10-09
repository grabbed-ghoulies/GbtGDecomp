#include "event_assets.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x0004F0B0
void load_cutscene_camera_event_asset(const char* name,
    CutsceneCameraEventAsset& state, AnimationResourceRegistry& resources,
    ResourceErrorState& error)
{
    u32 prefix;
    std::memcpy(&prefix, name, sizeof(prefix));
    if (prefix != 0x5F646961u)
    {
        state.resource = {};
        // Retail clears the resource pointer and still reads resource+4.
        // This invalid read is preserved; a non-AID name is not success.
        const auto* invalid = reinterpret_cast<const float*>(std::uintptr_t{4});
        state.duration = *invalid;
        return;
    }
    const auto resource = acquire_animation_resource(resources, name);
    if (resource.data() == nullptr)
    {
        resource_error(error, 2);
    }
    state.resource = resource;
    std::memcpy(&state.duration, resource.data() + 4, sizeof(state.duration));
}

// Xbox 0x0001BB60
i32 select_cutscene_random_index(u32& random, i32 upper, i32 lower)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    const float fraction = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
    const i32 difference = std::bit_cast<i32>(static_cast<u32>(upper) - static_cast<u32>(lower));
    return static_cast<i32>(static_cast<float>(difference) * fraction + static_cast<float>(lower));
}
}
