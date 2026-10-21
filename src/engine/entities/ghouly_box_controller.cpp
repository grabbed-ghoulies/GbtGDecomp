#include "ghouly_box_controller.h"
#include "ghouly_spawn_record.h"
#include "ghouly_spawn_trigger.h"
#include "actor_transform.h"

#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x0006BCA0
bool query_ghouly_spawn_pose(GhoulyBoxController& controller, Vec3& source_position,
    Vec3& source_euler, Vec3& position, Vec3& euler)
{
    source_position = actor_world_position(*static_cast<Actor*>(controller.source_93c));
    source_euler = actor_world_euler(*static_cast<Actor*>(controller.source_93c));
    if (controller.anchor_934 == nullptr)
    {
        position = source_position;
        euler = source_euler;
        return true;
    }
    GhoulyPoseMessage message;
    message.position = source_position;
    message.euler = source_euler;
    auto& anchor = *static_cast<GhoulyController*>(controller.anchor_934);
    anchor.message(anchor, &message);
    if (message.result == 0)
    {
        return false;
    }
    position = message.output_position;
    euler = message.output_euler;
    return true;
}

// Xbox 0x0006BB70
bool initialize_ghouly_box_controller(GhoulyBoxController& controller,
    std::span<const u8> parameters, u32& random_state)
{
    const u32 key = read_u32(parameters, 0x334);
    if (key != 0)
    {
        // 0x000662A0 grows the stride-8 registry by its header's grow value.
        // 0x00116E00 sets that value to 10. Host pointers live in the vector.
        if (g_ghouly_controllers.size() == g_ghouly_controllers.capacity())
        {
            g_ghouly_controllers.reserve(g_ghouly_controllers.size() + 10);
        }
        g_ghouly_controllers.push_back({key, &controller});
    }
    controller.anchor_934 = nullptr;
    controller.source_93c = nullptr;
    random_state = random_state * 0x19660Du + 0x3C6EF35Fu;
    const float random = std::bit_cast<float>((random_state & 0x7FFFFFu) | 0x3F800000u) - 1.f;
    if (random <= read_f32(parameters, 0x438) && read_u32(parameters, 0x338) == 0x5F646961u)
    {
        std::strcpy(controller.box_944, reinterpret_cast<const char*>(parameters.data() + 0x338));
        std::strcpy(controller.attributes_9c4, reinterpret_cast<const char*>(parameters.data() + 0x3B8));
        controller.value_a48 = read_u32(parameters, 0x43C);
        controller.value_a4c = read_u32(parameters, 0x440);
    }
    controller.value_a50 = read_u32(parameters, 0x444);
    return true;
}

// Xbox 0x0006BC70
void destroy_ghouly_box_controller(GhoulyBoxController& controller)
{
    remove_ghouly_controller(&controller);
    auto* record = static_cast<GhoulySpawnRecord*>(controller.load_938);
    if (record != nullptr)
    {
        release_ghouly_spawn_record(*record);
        controller.load_938 = nullptr;
    }
}


// Xbox 0x0006C0A0
void tick_ghouly_box_controller(GhoulyBoxController& controller,
    u32& random_state)
{
    const Vec3 position = actor_world_position(controller.actor);
    if (controller.spawned_940 == 0 && check_ghouly_spawn_trigger(position,
        std::bit_cast<float>(controller.value_a48), std::bit_cast<float>(controller.value_a4c),
        random_state) == 1)
    {
        GhoulyRequest message;
        std::strcpy(message.box, controller.box_944);
        std::strcpy(message.attributes, controller.attributes_9c4);
        controller.message(controller, &message);
    }
    auto* record = static_cast<GhoulySpawnRecord*>(controller.load_938);
    if (record != nullptr && record->remaining_c == 0)
    {
        release_ghouly_spawn_record(*record);
        controller.load_938 = nullptr;
    }
}

}
