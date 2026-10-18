#include "actor_lookup.h"
#include <cstring>
#include "../camera/camera.h"
#include "entity.h"

namespace gbtg
{
// Xbox 0x00129ED0
scene_detail::PulseNode* find_actor_by_id(u16 key)
{
    if (key == 0)
    {
        return nullptr;
    }
    for (auto* node = scene_detail::g_pulse_nodes; node != nullptr; node = node->next)
    {
        if (node->word_1fc == key)
        {
            return node;
        }
    }
    return nullptr;
}

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
        if (read_u16(bytes_at(address), 0x1FC) == key)
        {
            return address;
        }
    }
    return 0;
}

// Xbox 0x00129F00
scene_detail::PulseNode* find_actor_by_group(u16 caller_key)
{
    scene_detail::PulseNode* node = scene_detail::g_pulse_nodes;
    while (true)
    {
        if (node == nullptr)
        {
            return nullptr;
        }
        if (node->word_1fe == caller_key)
        {
            break;
        }
        node = node->next;
    }
    return node;
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
        for (auto* node = scene_detail::g_pulse_nodes; node != nullptr; node = node->next)
        {
            if (node->tag_6 == key)
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
