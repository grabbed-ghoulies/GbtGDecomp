#include "input_stack.h"
#include "input_activation.h"
#include "../entities/actor_shape_bounds.h"
#include "gbtg/clock.h"

#include <unordered_map>

namespace gbtg
{
std::shared_ptr<NativeInputStack> boot_native_input_stack()
{
    auto stack = std::make_shared<NativeInputStack>();
    // 0x00033248: DAT_0050E0F4 0, then DAT_0050E0F8..DAT_0050E104 1, 2, 4, 8.
    stack->port_masks = {0, 1, 2, 4, 8};
    // 0x00033274, the inline pool allocation: four 0x50-byte input entries.
    stack->pool = allocate_runtime_shape_pool(0x50, 4);
    stack->selected = 0;
    // 0x0011C3F0: the scene record's +0x230 device (0x00107D60) is pushed as "Scene Master Pad" (0x0032A420).
    std::unordered_map<u32, std::span<u8>> devices;
    const RuntimeInputReleaseContext release{devices};
    stack->master_pad = create_runtime_selected_input(release); // 0x00107D60, ESI +0x230
    stack->master_address = static_cast<u32>(reinterpret_cast<std::uintptr_t>(stack->master_pad.data()));
    devices[stack->master_address] = stack->master_pad;
    const auto pads = runtime_pad_state_views();
    RuntimeInputActivationContext activation{*stack->pool, stack->selected, pads.controllers, pads.merged,
        stack->port_masks.data() + 1, devices};
    push_runtime_input_entry(stack->master_address, "Scene Master Pad", activation); // 0x00109340
    stack->pool = activation.entries;
    return stack;
}
}
