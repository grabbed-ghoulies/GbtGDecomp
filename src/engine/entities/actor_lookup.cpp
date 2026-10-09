#include "actor_lookup.h"
#include <cstring>
#include "../camera/camera.h"

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
