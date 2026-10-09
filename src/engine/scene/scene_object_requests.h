#pragma once

#include "gbtg/object_factory.h"
#include <array>

namespace gbtg
{
struct SceneObjectRequests
{
    std::array<char, 128> background{}; // DAT_00510660
    std::array<char, 1024> objects{}; // DAT_005106E0
    i32 count = 0; // DAT_00510AE0
};

extern SceneObjectRequests g_scene_object_requests;

u32 request_scene_background(SceneObjectRequests& requests, const char* name); // Xbox 0x0011C890
u32 request_scene_object(SceneObjectRequests& requests, const char* name); // Xbox 0x0011C950
FactoryObject create_scene_background(ObjectFactoryRegistry& factory,
    const char* name, const char* default_name); // Xbox 0x0011C8B0
}
