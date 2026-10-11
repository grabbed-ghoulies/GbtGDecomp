#pragma once

#include "gbtg/types.h"
#include <array>
#include <unordered_map>

namespace gbtg
{
struct GhoulySpawnPool;
struct RuntimeDialogueInputDevice
{
    std::span<const u8> record; // retail device, kind +0, active +4, port +8
    std::array<const RuntimeDialogueInputDevice*, 4> children; // native pointers for +8..+0x14
};
struct RuntimeDialogueInputState
{
    std::span<const u8> keyboard; // DAT_00507C98
    std::span<const u8> device_masks; // DAT_0032E3B4, stride 8
    u32 selected_mask; // DAT_0050E0F8
    i32 activation_mode; // DAT_004DEA34
    i32 activated; // DAT_004DEA38
    const GhoulySpawnPool* devices; // DAT_0050E0E8, native pointer entries
    i32 selected; // DAT_0050E0EC, one based
    const std::unordered_map<u32, RuntimeDialogueInputDevice>* device_views = nullptr; // Native u32 address mapping.
};
u32 runtime_input_device_mask(const RuntimeDialogueInputState& state, i32 kind); // Xbox 0x001554C0
u32 runtime_input_port_presence(const RuntimeDialogueInputDevice& device,
    const RuntimeDialogueInputState& state); // Xbox 0x00107CE0
u32 runtime_input_device_presence(const RuntimeDialogueInputDevice& device,
    const RuntimeDialogueInputState& state); // Xbox 0x001082B0
u32 runtime_input_button_held(const RuntimeDialogueInputDevice& device, u16 mask,
    const RuntimeDialogueInputState& state); // Xbox 0x00108300
u32 runtime_input_button_edge(const RuntimeDialogueInputDevice& device, u16 mask,
    const RuntimeDialogueInputState& state); // Xbox 0x001083F0
void runtime_input_axes(const RuntimeDialogueInputDevice& device,
    std::array<float, 2>& output, const RuntimeDialogueInputState& state); // Xbox 0x001085F0
void runtime_input_secondary_axes(const RuntimeDialogueInputDevice& device,
    std::array<float, 2>& output, const RuntimeDialogueInputState& state); // Xbox 0x001088D0
struct RuntimeInputAxisWriteDevice
{
    std::span<u8> record;
    std::array<RuntimeInputAxisWriteDevice*, 4> children;
};
void set_runtime_input_axes(RuntimeInputAxisWriteDevice& device,
    const std::array<float, 2>& axes, std::span<u8> keyboard); // Xbox 0x00108FC0
void set_runtime_input_secondary_axes(RuntimeInputAxisWriteDevice& device,
    const std::array<float, 2>& axes, std::span<u8> keyboard); // Xbox 0x001090A0
void set_runtime_input_trigger_axes(RuntimeInputAxisWriteDevice& device,
    const std::array<float, 2>& axes, std::span<u8> keyboard); // Xbox 0x00109180
void dialogue_input_direction(const RuntimeDialogueInputDevice& device,
    std::array<float, 2>& output, const RuntimeDialogueInputState& state); // Xbox 0x001429E0
u32 activate_dialogue_input(RuntimeDialogueInputState& state); // Xbox 0x000335A0
}
