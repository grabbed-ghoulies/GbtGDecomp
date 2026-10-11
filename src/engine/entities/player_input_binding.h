#pragma once
#include "entity_types.h"
#include "../frontend/input_activation.h"

namespace gbtg
{
struct RuntimePlayerInputBinding
{
    std::span<u8> actor;
    u32 actor_address;
    entity_detail::ListHead& link; // Actor +0x93C.
    entity_detail::ListHead& controlled; // DAT_00511B3C.
    std::span<u8>& input; // Actor +0x960.
    const u32& flags; // DAT_0050EA9C.
    std::span<const u32> port_devices; // DAT_0050E0F4's actual pointer rows, +4 + index*4.
    const RuntimeInputReleaseContext& release;
    RuntimeInputActivationContext& activation;
    std::function<u32(std::span<u8>)> register_input; // Host address mapping for allocated device.
    std::function<u32(const void*)> address_of; // Actual intrusive link identities.
};
void bind_runtime_player_input(RuntimePlayerInputBinding& context); // Xbox 0x0002CAB0
void register_runtime_controlled_player(RuntimePlayerInputBinding& context); // Xbox 0x0007F870
}
