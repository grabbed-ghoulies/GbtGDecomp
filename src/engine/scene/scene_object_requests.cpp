#include "scene_object_requests.h"
#include "../entities/actor_trail_requests.h"

#include <cstring>

namespace gbtg
{
SceneObjectRequests g_scene_object_requests;

// Xbox 0x0011C890
u32 request_scene_background(SceneObjectRequests& requests, const char* name)
{
    std::strcpy(requests.background.data(), name);
    return 1;
}

// Xbox 0x0011C950
u32 request_scene_object(SceneObjectRequests& requests, const char* name)
{
    std::strcpy(requests.objects.data() + requests.count * 128, name);
    ++requests.count;
    return 1;
}

// Xbox 0x0011C8B0
FactoryObject create_scene_background(ObjectFactoryRegistry& factory,
    const char* name, const char* default_name)
{
    std::array<u8, 0x334> parameters;
    initialize_actor_effect_parameters(parameters.data(), default_name);
    const u16 type = 4;
    const u16 size = 0x334;
    const u16 kind = 0x46;
    const float scale = 1.f;
    std::memcpy(parameters.data(), &size, sizeof(size));
    std::memcpy(parameters.data() + 2, &kind, sizeof(kind));
    std::memcpy(parameters.data() + 4, &type, sizeof(type));
    std::strcpy(reinterpret_cast<char*>(parameters.data() + 0xA8), default_name);
    std::memcpy(parameters.data() + 0x20, &scale, sizeof(scale));
    std::strcpy(reinterpret_cast<char*>(parameters.data() + 0x28), name);
    std::strcpy(reinterpret_cast<char*>(parameters.data() + 0x1A8),
        "aid_callout_ghoulies_background");
    return create_factory_object(factory, parameters);
}
}
