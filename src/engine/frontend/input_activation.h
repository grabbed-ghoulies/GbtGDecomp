#pragma once
#include "dialogue_input.h"
#include "../entities/ghouly_spawn_record.h"
#include <unordered_map>

namespace gbtg
{
struct RuntimeInputActivationContext
{
    GhoulySpawnPool* entries; // DAT_0050E0E8, retail stride 0x50, rebound on growth
    i32& selected; // DAT_0050E0EC, one based
    std::span<const u8> controllers; // DAT_005079F8, stride 0xA8
    std::span<const u8> keyboard; // DAT_00507C98
    const u32* port_masks; // DAT_0050E0F8, also retail index -1
    const std::unordered_map<u32, std::span<u8>>& devices;
    RuntimeInputActivationContext(GhoulySpawnPool& pool, i32& selected_entry,
        std::span<const u8> controller_states, std::span<const u8> keyboard_state,
        const u32* masks, const std::unordered_map<u32, std::span<u8>>& addresses)
        : entries(&pool), selected(selected_entry), controllers(controller_states),
          keyboard(keyboard_state), port_masks(masks), devices(addresses)
    {
    }
};
void snapshot_runtime_input_state(u8* state); // Xbox 0x000FA110
void copy_runtime_input_state(u8* output, const u8* input); // Xbox 0x000FA190
i32 runtime_controller_mask_index(u32 mask); // Xbox 0x000FACD0
i32 runtime_player_mask_index(u32 mask); // Xbox 0x00107FF0
i32 runtime_player_controller_index(u32 mask, const u32* ports); // Xbox 0x00109720
const u8* runtime_input_state_for_mask(u32 mask,
    const RuntimeInputActivationContext& context); // Xbox 0x000FABF0
u32 runtime_input_port_mask(std::span<const u8> device,
    const RuntimeInputActivationContext& context); // Xbox 0x00107C60
void reset_runtime_analog_input(std::span<u8> device); // Xbox 0x001078A0
void reset_runtime_selected_input(std::span<u8> device,
    const RuntimeInputActivationContext& context); // Xbox 0x00107EC0
void reset_runtime_port_input(std::span<u8> device,
    const RuntimeInputActivationContext& context); // Xbox 0x00107C30
u8* find_runtime_input_entry(const char* name,
    RuntimeInputActivationContext& context); // Xbox 0x001096A0
u32 activate_runtime_input_entry(const char* name,
    RuntimeInputActivationContext& context); // Xbox 0x001095B0
void pop_runtime_input_entry(RuntimeInputActivationContext& context); // Xbox 0x001093C0
void push_runtime_input_entry(u32 address, const char* name,
    RuntimeInputActivationContext& context); // Xbox 0x00109340
void clear_runtime_input_states(std::span<u8> device); // Xbox 0x00107E20
void clear_runtime_actor_input_states(std::span<u8> device); // Xbox 0x00107B90
struct RuntimeInputReleaseContext
{
    const std::unordered_map<u32, std::span<u8>>& addresses;
};
void release_runtime_simple_input(std::span<u8> device); // Xbox 0x00107DD0
void release_runtime_input_slot(std::span<u8> pool, i32 slot,
    const RuntimeInputReleaseContext& context); // Xbox 0x00107A40
void release_runtime_actor_input(std::span<u8> device,
    const RuntimeInputReleaseContext& context); // Xbox 0x00107B60
void release_runtime_composite_input(std::span<u8> device,
    const RuntimeInputReleaseContext& context); // Xbox 0x00107960
void release_runtime_input_child(std::span<u8> device,
    const RuntimeInputReleaseContext& context); // Xbox 0x001081E0
void release_runtime_input_device(std::span<u8> device,
    const RuntimeInputReleaseContext& context); // Xbox 0x00108230
void reset_runtime_input_device(std::span<u8> device,
    const RuntimeInputReleaseContext& context); // Xbox 0x00108270
void reset_runtime_composite_input(std::span<u8> device,
    const RuntimeInputReleaseContext& context); // Xbox 0x001079D0
std::span<u8> create_runtime_keyboard_input(const RuntimeInputReleaseContext& context); // Xbox 0x00107EF0
std::span<u8> create_runtime_selected_input(const RuntimeInputReleaseContext& context); // Xbox 0x00107D60
std::span<u8> create_runtime_actor_base_input(const RuntimeInputReleaseContext& context); // Xbox 0x00107830
void refresh_runtime_input_device(u32 address, u32 frame,
    RuntimeInputActivationContext& context, const RuntimeInputReleaseContext& release); // Xbox 0x00109470
}
