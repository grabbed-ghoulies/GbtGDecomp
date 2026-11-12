#pragma once
#include "model_library_hooks.h"
#include "model_matrix_array_draw.h"
#include "gbtg/model_runtime.h"

#include <memory>

namespace gbtg
{
void set_runtime_model_material_hook(std::function<void(u32)> hook,
    RuntimeModelLibraryHooks& hooks); // Xbox 0x0015B2D0
void set_runtime_model_stage_texture_hook(std::function<void(u32, u32)> hook,
    RuntimeModelLibraryHooks& hooks); // Xbox 0x0015B2E0
void set_runtime_model_texture_frames_hook(u32 hook, RuntimeModelLibraryHooks& hooks); // Xbox 0x0015B2F0
void set_runtime_model_texture_lock_hook(u32 hook, RuntimeModelLibraryHooks& hooks); // Xbox 0x001565F0
void reset_runtime_model_library_buffer(RuntimeModelLibraryAllocator& allocator); // Xbox 0x00165AB0
void set_runtime_model_library_allocator(std::function<void*(u32)> allocate, std::function<void(void*)> release,
    RuntimeModelLibraryAllocator& allocator); // Xbox 0x001652E0

// What boot 0x000F8740 sets up: the model library's matrix scratch, the deferred model releases and the hooks.
struct RuntimeModelLibraryBoot
{
    std::unique_ptr<u8, void (*)(u8*)>& matrix_block; // DAT_005079D8
    RuntimeModelMatrixScratch& matrix_scratch; // DAT_00437080..DAT_00437094
    RuntimeModelDeferredPool& deferred; // DAT_005079E0
    RuntimeModelLibraryHooks& hooks;
    RuntimeModelLibraryAllocator& allocator;
    std::function<u8*(i32 size, u32 alignment)> allocate_aligned; // 0x000F7330
    // The game's hooks, each a translated function over the host's draw contexts.
    std::function<void(u32 shader)> post_shader; // 0x000F9B40
    std::function<void(u32 material)> material_colors; // 0x000F9B90
    std::function<void(u32 texture, u32 stage)> bind_texture; // 0x000F9BC0
    std::function<void*(u32 size)> heap_allocate; // 0x000F9C10, a jump to 0x000F72D0
    std::function<void(void* block)> heap_free; // 0x000D8AC0
};
void boot_runtime_model_library(RuntimeModelLibraryBoot& boot); // Xbox 0x000F8740
}
