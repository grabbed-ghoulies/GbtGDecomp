#pragma once
#include "player_attributes.h"
#include "player_children.h"

namespace gbtg
{
struct RuntimeBodyHealth
{
    std::span<const u8> actor;
    std::span<const u8> curve; // DAT_00433CA8, endpoint bytes +0/+1, floats +4/+8.
    std::span<const u8> tags; // DAT_00545AC4, stride 0x87, value at tag*stride-0x47.
    std::span<std::span<const u8>, 4> textures; // Body +0x98/+0x9C/+0xA0/+0xD8.
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
};
void initialize_runtime_body_health(std::span<u8> body, std::span<const u8> parameters,
    RuntimeBodyHealth& context); // Xbox 0x0001DB10
void release_runtime_body_health(RuntimeBodyHealth& context); // Xbox 0x0001DCA0
void release_runtime_body_actions(std::span<u8> body, FactoryObject& first,
    FactoryObject& second, std::span<const u8>& resource, ObjectFactoryRegistry& factory,
    AnimationResourceRegistry& registry); // Xbox 0x00027580
}
