#pragma once
#include "gbtg/anim.h"
#include "gbtg/object_factory.h"

namespace gbtg
{
FactoryObject create_runtime_actor_pose(const char* name, const Vec3& position,
    const Vec3& euler, std::span<ExecutableResourcePool> resources,
    ObjectFactoryRegistry& factory); // Xbox 0x00141840
FactoryObject create_runtime_actor_at(const char* name, const Vec3* position,
    const Vec3* euler, float scale, std::span<ExecutableResourcePool> resources,
    ObjectFactoryRegistry& factory, const Vec3& default_position,
    const Vec3& default_euler); // Xbox 0x00112C50
}
