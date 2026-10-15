#pragma once
#include "gbtg/types.h"

#include <array>
#include <memory>
#include <span>

namespace gbtg
{
struct GhoulySpawnPool;
// The boot-owned input stack every scene shares: the entry pool (DAT_0050E0E8, four 0x50-byte entries from
// 0x00033274), the selected entry (DAT_0050E0EC), the players' port masks (DAT_0050E0F4..DAT_0050E104, set at
// 0x00033248) and the "Scene Master Pad" device (DAT_00510E58 +0x230) 0x0011C3F0 pushes at the stack's bottom.
struct NativeInputStack
{
    GhoulySpawnPool* pool = nullptr;
    i32 selected = 0;
    std::array<u32, 5> port_masks{};
    std::span<u8> master_pad;
    u32 master_address = 0; // Its native address in each scene's input map.
};
std::shared_ptr<NativeInputStack> boot_native_input_stack(); // 0x00033248, 0x00033274, 0x0011C3F0's push.
}
