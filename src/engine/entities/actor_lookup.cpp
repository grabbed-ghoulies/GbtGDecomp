#include "actor_lookup.h"
#include "actor_fields.h"
#include <cstring>
#include "entity.h"

namespace gbtg
{
// Xbox 0x00129ED0, DX key
u32 find_runtime_actor_by_id(u16 key, const std::function<u32(const entity_detail::LatchNode&)>& owner,
    const std::function<std::span<u8>(u32)>& bytes_at)
{
    if (key == 0)
    {
        return 0;
    }
    for (const auto* node = entity_detail::g_world.latch; node != nullptr; node = node->next) // DAT_00511AA4.
    {
        const u32 address = owner(*node); // The node sits at the object's +0x900.
        if (actor_marker_id(bytes_at(address)) == key)
        {
            return address;
        }
    }
    return 0;
}

// Xbox 0x00129EA0, DX key
u32 find_runtime_actor_by_class(u16 key, const std::function<u32(const entity_detail::LatchNode&)>& owner,
    const std::function<std::span<u8>(u32)>& bytes_at)
{
    for (const auto* node = entity_detail::g_world.latch; node != nullptr; node = node->next) // DAT_00511AA4.
    {
        const u32 address = owner(*node); // The node sits at the object's +0x900.
        if (read_u16(bytes_at(address), 6) == key)
        {
            return address;
        }
    }
    return 0;
}

// Xbox 0x0013E860
bool spawn_resource_under_class_limit(std::span<ExecutableResourcePool> resources,
    const u32* class_limits, const char* name)
{
    if (name == nullptr || std::memcmp(name, "aid_", 4) != 0)
    {
        return false;
    }
    auto* resource = find_executable_resource(resources, name, true);
    if (resource == nullptr || resource->cpu.data() == nullptr)
    {
        return false;
    }
    const u16 key = read_u16(resource->cpu, 2);
    if (class_limits[key] != 0)
    {
        u32 count = 0;
        for (const auto* node = entity_detail::g_world.latch; node != nullptr; node = node->next) // DAT_00511AA4.
        {
            if (node->tag == key) // The owner's +6 (node -0x8FA).
            {
                ++count;
            }
        }
        if (class_limits[key] <= count)
        {
            return false;
        }
    }
    return true;
}

}
