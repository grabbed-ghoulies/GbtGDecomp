#include "actor_template_creation.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x00112C50
FactoryObject create_runtime_actor_at(const char* name, const Vec3* position,
    const Vec3* euler, float scale, std::span<ExecutableResourcePool> resources,
    ObjectFactoryRegistry& factory, const Vec3& default_position,
    const Vec3& default_euler)
{
    auto clone = clone_executable_resource(resources, name);
    if (clone.bytes == nullptr)
    {
        return {};
    }
    if (position == nullptr)
    {
        position = &default_position;
    }
    std::memcpy(clone.bytes.get() + 8, position, 12);
    if (euler == nullptr)
    {
        euler = &default_euler;
    }
    std::memcpy(clone.bytes.get() + 0x14, euler, 12);
    std::memcpy(clone.bytes.get() + 0x20, &scale, 4);
    auto object = create_factory_object(factory, {clone.bytes.get(), clone.size});
    clone.bytes.reset();
    return object;
}
}
