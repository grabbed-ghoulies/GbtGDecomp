#include "model_library_boot.h"
#include "model_shader_states.h"

namespace gbtg
{
// Xbox 0x0015B2D0
void set_runtime_model_material_hook(std::function<void(u32)> hook, RuntimeModelLibraryHooks& hooks)
{
    hooks.material = std::move(hook);
}

// Xbox 0x0015B2E0
void set_runtime_model_stage_texture_hook(std::function<void(u32, u32)> hook, RuntimeModelLibraryHooks& hooks)
{
    hooks.stage_texture = std::move(hook);
}

// Xbox 0x0015B2F0
void set_runtime_model_texture_frames_hook(u32 hook, RuntimeModelLibraryHooks& hooks)
{
    hooks.texture_frames = hook;
}

// Xbox 0x001565F0
void set_runtime_model_texture_lock_hook(u32 hook, RuntimeModelLibraryHooks& hooks)
{
    hooks.texture_lock = hook;
}

// Xbox 0x00165AB0
void reset_runtime_model_library_buffer(RuntimeModelLibraryAllocator& allocator)
{
    if (allocator.buffer != nullptr)
    {
        allocator.release(allocator.buffer);
    }
    allocator.buffer = nullptr;
    allocator.unk_bc = 0;
    allocator.unk_c0 = 0;
}

// Xbox 0x001652E0
void set_runtime_model_library_allocator(std::function<void*(u32)> allocate, std::function<void(void*)> release,
    RuntimeModelLibraryAllocator& allocator)
{
    allocator.allocate = std::move(allocate);
    allocator.release = std::move(release);
}

// Xbox 0x000F8740
void boot_runtime_model_library(RuntimeModelLibraryBoot& boot)
{
    const u32 size = runtime_model_matrix_scratch_size(); // 0x00155D40
    boot.matrix_block.reset(boot.allocate_aligned(i32(size), 0x20)); // 0x000F7330
    bind_runtime_model_matrix_scratch(reinterpret_cast<RuntimeShaderMatrix*>(boot.matrix_block.get()), size,
        boot.matrix_scratch); // 0x00155D20
    // 0x000F72D0(0x60): the pool header (record stride 8 at +0, growth 10 at +2), then room for ten 8-byte records
    // from +0x10 to the limit at +0x60, none used.
    boot.deferred.records.clear();
    boot.deferred.records.reserve(10);
    boot.deferred.grow = 10;
    boot.deferred.limit = (0x60 - 0x10) / 8;
    bind_runtime_model_shader_callback(boot.post_shader, boot.hooks); // 0x0015B660(0x000F9B40)
    set_runtime_model_material_hook(boot.material_colors, boot.hooks); // 0x0015B2D0(0x000F9B90)
    set_runtime_model_stage_texture_hook(boot.bind_texture, boot.hooks); // 0x0015B2E0(0x000F9BC0)
    set_runtime_model_texture_frames_hook(0x000F9C00, boot.hooks); // 0x0015B2F0
    set_runtime_model_texture_lock_hook(0x000F9BE0, boot.hooks); // 0x001565F0
    reset_runtime_model_library_buffer(boot.allocator); // 0x00165AB0
    set_runtime_model_library_allocator(boot.heap_allocate, boot.heap_free, boot.allocator); // 0x001652E0
}
}
