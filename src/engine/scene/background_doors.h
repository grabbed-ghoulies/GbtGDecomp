#pragma once
#include "../cutscenes/door_registration.h"

namespace gbtg
{
void register_background_doors(std::span<const u8> cpu,
    RuntimeDoorContext& context); // Xbox 0x00043B40
void register_selected_background_doors(std::span<const u8> cpu, std::span<const u8> message,
    RuntimeDoorContext& context); // Xbox 0x00044300
}
