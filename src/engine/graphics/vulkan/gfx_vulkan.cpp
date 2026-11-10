#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <SDL3/SDL.h>
#endif

#include "vk_context.h"
#include "vk_shader_compiler.h"
#include "vk_swapchain.h"

#include "../gfx_backend.h"
#include "../win32_window.h"
#include "../gfx_present_logic.h"
#include "../gfx_shader_sources.h"
#include "../render_profile.h"
#include "../vertex_program_constants.h"
#include "../xbox_vertex_hlsl.h"
#include "../xbox_vertex_program.h"
#include "../xg_shader_assembly.h"
#include "../../gbtg_resource.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "../../plugins/plugins.h"
#endif

#include "gbtg/gfx.h"
#include "gbtg/log.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#if !defined(GBTG_GFX_VULKAN)
#error gfx_vulkan.cpp is the PC Vulkan backend
#endif

namespace gbtg
{
// The gfx_* definitions below are this backend's; gfx_dispatch.cpp owns the public names.
namespace vulkan
{
void gfx_release_sprite(const RgbaImage& image);
void gfx_draw_sprite(const RgbaImage& image, const GfxRect& rect);
bool gfx_readback(RgbaImage& out);

namespace
{

// The host mesh vertex vs_main reads; sprites are its only Vulkan user. Same layout as the D3D11 backend's GpuVert.
struct GpuVert
{
    float x, y, z;
    float u0, v0;
    float u1, v1;
    float nx, ny, nz;
    float u2 = 0.f, v2 = 0.f;
    float bone = 0.f;
    float bone2 = 0.f;
    float w0 = 1.f;
    float w1 = 0.f;
    u32 diffuse = 0xffffffff;
    u32 specular = 0;
    float u3 = 0.f, v3 = 0.f;
    float bone3 = 0.f, bone4 = 0.f;
    float w2 = 0.f, w3 = 0.f;
};

// cbuffer Camera (b0) in gfx_hlsl_common.
struct GpuCB
{
    float mvp[16]{};
    float albedo_uv = 0.f;
    float light_uv = 1.f;
    float use_light = 0.f;
    float use_cel = 0.f;
    float light_x = 1.f;
    float light_y = 1.f;
    float light_z = 1.f;
    float skin = 0.f;
    float alpha_test = 0.f;
    float alpha_ref = 0.f;
    float alpha_func = 0.f;
    float alpha_pad = 0.f;
    std::array<float, 4> tint{1.f, 1.f, 1.f, 1.f};
    std::array<float, 4> skin_mask{1.f, 1.f, 0.f, 0.f};
    std::array<float, 4> native_uv_scale{0.f, 0.f, 0.f, 0.f};
};

static_assert(sizeof(GpuVert) == 96 && offsetof(GpuVert, diffuse) == 64 && offsetof(GpuVert, u3) == 72);
static_assert(sizeof(GpuCB) == 160);
static_assert(sizeof(GfxNativeVertex) == 116);

constexpr VkDeviceSize kBonesBytes = 128 * 64;      // cbuffer Bones (b1)
constexpr VkDeviceSize kMaterialBytes = 80 * 4;     // cbuffer Material (b2)
constexpr VkDeviceSize kConstantsBytes = 192 * 16;  // cbuffer NativeConstants (b3)
constexpr VkDeviceSize kRingBytes = 32ull << 20;

struct Shader
{
    VkShaderModule module = VK_NULL_HANDLE;
    std::string entry;
};

enum class VertexLayout : u32
{
    Native,  // GfxNativeVertex
    Program, // GfxNativeInputs, run by a translated vertex program
    Sprite,  // GpuVert
};

// Everything a pipeline bakes in. Viewport, scissor and stencil reference are dynamic.
struct PipelineState
{
    const Shader* vertex = nullptr;
    const Shader* pixel = nullptr;
    VertexLayout layout = VertexLayout::Native;
    bool lines = false;
    VkCullModeFlags cull = VK_CULL_MODE_NONE;
    bool blend = false;
    VkBlendFactor source_color = VK_BLEND_FACTOR_ONE;
    VkBlendFactor destination_color = VK_BLEND_FACTOR_ZERO;
    VkBlendFactor source_alpha = VK_BLEND_FACTOR_ONE;
    VkBlendFactor destination_alpha = VK_BLEND_FACTOR_ZERO;
    VkBlendOp equation = VK_BLEND_OP_ADD;
    VkColorComponentFlags write_mask = 0xF;
    bool depth_test = false;
    bool depth_write = false;
    VkCompareOp depth_compare = VK_COMPARE_OP_LESS_OR_EQUAL;
    i32 z_bias = 0; // D3DRS_ZBias.
    bool stencil = false;
    VkCompareOp stencil_compare = VK_COMPARE_OP_ALWAYS;
    u32 stencil_read_mask = 0xFF;
    u32 stencil_write_mask = 0xFF;
    VkStencilOp stencil_fail = VK_STENCIL_OP_KEEP;
    VkStencilOp stencil_depth_fail = VK_STENCIL_OP_KEEP;
    VkStencilOp stencil_pass = VK_STENCIL_OP_KEEP;

    std::array<u64, 10> key() const
    {
        return {u64(reinterpret_cast<uintptr_t>(vertex ? vertex->module : VK_NULL_HANDLE)),
                u64(reinterpret_cast<uintptr_t>(pixel ? pixel->module : VK_NULL_HANDLE)),
                u64(layout) | (u64(lines) << 8) | (u64(cull) << 16) | (u64(blend) << 24) | (u64(write_mask) << 32),
                u64(source_color) | (u64(destination_color) << 16) | (u64(source_alpha) << 32) | (u64(destination_alpha) << 48),
                u64(equation) | (u64(u32(z_bias)) << 32),
                u64(depth_test) | (u64(depth_write) << 1) | (u64(depth_compare) << 8) | (u64(stencil) << 16)
                    | (u64(stencil_compare) << 24),
                u64(stencil_read_mask) | (u64(stencil_write_mask) << 32),
                u64(stencil_fail),
                u64(stencil_depth_fail),
                u64(stencil_pass)};
    }
};

struct Texture
{
    vk::Image image;
    u32 serial = 0;
};

// The D3D viewport (top-left origin, y down). Vulkan gets it flipped so the D3D clip space is kept.
struct Viewport
{
    float x = 0.f;
    float y = 0.f;
    float width = 0.f;
    float height = 0.f;
};

struct ProgramKeyLess
{
    using is_transparent = void;
    bool operator()(std::span<const u32> a, std::span<const u32> b) const
    {
        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
    }
};

struct GfxVulkan
{
    GfxConfig cfg;
    bool ok = false;
#if defined(_WIN32)
    HWND window = nullptr;
#else
    SDL_Window* window = nullptr;
#endif
    bool quit = false;

    vk::Context context;
    vk::Swapchain swapchain;
    VkCommandBuffer command = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    bool recording = false;

    // The back buffer. Windowed runs copy it onto the swapchain at the swap.
    vk::Image frame_color;
    vk::Image frame_depth;
    bool frame_finished = false; // A swapped frame is held for gfx_present_again.

    // The texture surface between gfx_begin_native_target and gfx_end_native_target.
    vk::Image target_color;
    vk::Image target_depth;
    bool target_rendering = false;
    bool target_has_depth = true; // False while a target with no depth surface is open: no depth or stencil test.
    Viewport target_saved_viewport;

    // A texture target that shares the back buffer's depth-stencil, between gfx_begin_native_scene_target and
    // gfx_end_native_scene_target: a colour image shaped like frame_color, drawn under the fitted viewport.
    std::unordered_map<u32, vk::Image> scene_target_colors; // By texture, each keeping its contents between opens.
    vk::Image* scene_target_open = nullptr;
    bool scene_target_rendering = false;
    int scene_target_width = 0; // The texture's size.
    int scene_target_height = 0;
    Viewport scene_target_saved_viewport;

    // Open dynamic rendering, onto target_* while target_rendering, else frame_*.
    bool rendering = false;
    VkPipeline bound_pipeline = VK_NULL_HANDLE;
    Viewport viewport;

    // Per-submission vertex, index, uniform and staging data.
    vk::Buffer ring;
    VkDeviceSize ring_cursor = 0;
    vk::Buffer readback;

    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    vk::Buffer zero_uniform; // Bound for the constant buffers a draw does not use.
    vk::Image zero_texture;  // D3D11 samples zero from an unbound slot.
    VkSampler wrap_sampler = VK_NULL_HANDLE;
    VkSampler clamp_sampler = VK_NULL_HANDLE;

    std::string vertex_source;
    std::string model_source;
    Shader native_vs;
    Shader native_ps;
    Shader model_ps;
    Shader sprite_vs;
    Shader sprite_ps;
    struct ProgramGpu
    {
        Shader shader;
#if GBTG_OPTIMIZE
        // GBTG_OPTIMIZE step 10 (Vulkan): the constant registers the program can read, [first, first + count).
        u32 constants_first = 0;
        u32 constants_count = 192;
#endif
    };
    std::map<std::vector<u32>, ProgramGpu, ProgramKeyLess> native_programs;
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 3 (Vulkan): native_programs lookups by the address of the program's words, verified
    // against a copy of them, so a reused address with other words looks up again.
    struct ProgramMemo
    {
        std::vector<u32> words;
        ProgramGpu* program = nullptr;
        u64 batch = 0; // GBTG_OPTIMIZE step 23: native_batch_serial when the words were last compared.
    };
    std::unordered_map<const u32*, ProgramMemo> program_memo;
    // GBTG_OPTIMIZE step 23 (the D3D11 step 18 and 21 shortcuts): what the previous native draw of this batch found.
    // A view stands for the image at the serial it was found for; gfx_release_sprite forgets the images. The constant
    // slot stands for the constants id it was copied for while the ring keeps its contents (ring_epoch).
    struct NativeLast
    {
        std::array<const RgbaImage*, 4> images{};
        std::array<u32, 4> image_serials{};
        std::array<VkImageView, 4> views{};
        std::array<u32, 4> sampler_keys{};
        std::array<VkSampler, 4> samplers{};
        bool samplers_valid = false;
        u64 constants_id = 0;
        u64 constants_epoch = 0;
        VkDeviceSize constants_offset = 0;
        u32 constants_first = 0;
        u32 constants_count = 0;
    } native_last;
    u64 native_batch_serial = 0; // gfx_begin_native_batch calls.
    // GBTG_OPTIMIZE step 23: the readback gfx_end_native_target_into last recorded, until gfx_read_ended_native_target.
    vk::Buffer ended_readback;
    bool ended_pending = false;
    int ended_width = 0;
    int ended_height = 0;
    u64 ring_epoch = 1; // Bumped whenever the ring's earlier contents may be overwritten or the ring is replaced.
    // GBTG_OPTIMIZE step 2 (Vulkan): what the current command buffer last set, so a draw sets only what differs.
    // Push descriptors stay in effect until overwritten or until the set is disturbed, and dynamic state until set
    // again; both are undefined when a command buffer begins, so `valid` starts false there and after any pass that
    // binds another pipeline layout (the debug UI).
    struct CommandState
    {
        bool valid = false;
        std::array<VkDescriptorBufferInfo, 4> buffers{};
        std::array<VkImageView, 4> views{};
        std::array<VkSampler, 4> samplers{};
        bool viewport_valid = false;
        VkViewport viewport{};
        VkRect2D scissor{};
        bool stencil_valid = false;
        u32 stencil_reference = 0;
    } command_state;
    // One-entry memos in front of the pipeline and specialized pixel shader maps.
    std::array<u64, 10> last_pipeline_key{};
    VkPipeline last_pipeline = VK_NULL_HANDLE;
    std::array<u32, 60> last_pixel_words{};
    const Shader* last_pixel = nullptr;
#endif
    std::map<std::array<u32, 60>, std::unique_ptr<Shader>> specialized_pixels;
    std::map<std::array<u64, 10>, VkPipeline> pipelines;
    std::unordered_map<u32, VkSampler> samplers;
    std::unordered_map<const RgbaImage*, Texture> textures;
    // Images a recorded command may still read; destroyed once the next submission completes.
    std::vector<vk::Image> retired;
    bool native_batch = false;
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 7: uploads of the presenter's retained geometry, by GfxNativeDraw::retained_geometry:
    // program inputs, then the u32 indices at index_offset. An id's contents never change.
    struct RetainedUpload
    {
        vk::Buffer buffer;
        VkDeviceSize index_offset = 0;
        u64 last_used = 0;
    };
    std::unordered_map<u64, RetainedUpload> retained_uploads;
    VkDeviceSize retained_bytes = 0;
    u64 frame_number = 0;
    // Buffers a recorded command may still read, like `retired`.
    std::vector<vk::Buffer> retired_buffers;
#endif
};

GfxVulkan g;

#if GBTG_OPTIMIZE
// GBTG_OPTIMIZE step 11: window events arrive on the main thread (gfx_pump) while the render thread may be presenting,
// so a resize is handed over here and applied by take_window_events before the swapchain is used. The extent is
// window_extent's width << 32 | height; without a new extent (WM_SIZE) only the stale flag is set.
std::atomic<bool> g_resize_pending{false};
std::atomic<bool> g_resize_has_extent{false};
std::atomic<u64> g_resize_extent{0};

void take_window_events()
{
    if (!g_resize_pending.exchange(false))
    {
        return;
    }
    if (g_resize_has_extent.exchange(false))
    {
        const u64 extent = g_resize_extent.load();
        g.swapchain.window_extent = {u32(extent >> 32), u32(extent)};
    }
    g.swapchain.stale = true;
}
#endif

#if defined(_WIN32)
LRESULT CALLBACK gfx_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
#if !defined(GBTG_RETAIL_GAME_ONLY)
    // Plugin overlays (the debug UI) see each message first. Escape quits the process unless an overlay has the keys.
    const GbtgWin32Message message{hwnd, msg, wp, lp};
    if (plugins_window_event(&message))
    {
        return 0;
    }
    if (plugins_capture_input() && msg == WM_KEYDOWN && wp == VK_ESCAPE)
    {
        return 0;
    }
#endif
    if (msg == WM_SIZE)
    {
#if GBTG_OPTIMIZE
        g_resize_pending.store(true);
#else
        g.swapchain.stale = true;
#endif
    }
    if (msg == WM_CLOSE || (msg == WM_KEYDOWN && wp == VK_ESCAPE))
    {
        g.quit = true;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
#else
// The SDL counterpart of gfx_wndproc.
void handle_window_event(const SDL_Event& event)
{
#if !defined(GBTG_RETAIL_GAME_ONLY)
    // Plugin overlays (the debug UI) see each event first. Escape quits the process unless an overlay has the keys.
    if (plugins_window_event(&event))
    {
        return;
    }
    if (plugins_capture_input() && event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
    {
        return;
    }
#endif
    if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
    {
#if GBTG_OPTIMIZE
        g_resize_extent.store(u64(u32(std::max(event.window.data1, 0))) << 32 | u32(std::max(event.window.data2, 0)));
        g_resize_has_extent.store(true);
        g_resize_pending.store(true);
#else
        g.swapchain.window_extent = {u32(std::max(event.window.data1, 0)), u32(std::max(event.window.data2, 0))};
        g.swapchain.stale = true;
#endif
    }
    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED
        || (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE))
    {
        g.quit = true;
    }
}
#endif

Shader make_shader(std::string_view source, const char* entry, const char* profile, bool strict = false)
{
    Shader shader;
    const std::vector<u32> words = vk::compile_hlsl_spirv(source, entry, profile, strict);
    if (words.empty())
    {
        return shader;
    }
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = words.size() * 4;
    info.pCode = words.data();
    vk::check(vkCreateShaderModule(g.context.device, &info, nullptr, &shader.module), "vkCreateShaderModule");
    shader.entry = entry;
    return shader;
}

void destroy_shader(Shader& shader)
{
    if (shader.module != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(g.context.device, shader.module, nullptr);
    }
    shader = {};
}

// --- Command recording ---------------------------------------------------------------------------------------

void ensure_recording()
{
    if (g.recording)
    {
        return;
    }
    vk::check(vkResetCommandBuffer(g.command, 0), "vkResetCommandBuffer");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk::check(vkBeginCommandBuffer(g.command, &begin), "vkBeginCommandBuffer");
    g.recording = true;
#if GBTG_OPTIMIZE
    g.command_state = {};
#endif
}

void end_rendering()
{
    if (g.rendering)
    {
        vkCmdEndRendering(g.command);
        g.rendering = false;
        vk::memory_barrier(g.command);
    }
}

const vk::Image& render_color()
{
    return g.target_rendering ? g.target_color : g.scene_target_rendering ? *g.scene_target_open : g.frame_color;
}

const vk::Image& render_depth()
{
    return g.target_rendering ? g.target_depth : g.frame_depth;
}

void begin_rendering()
{
    ensure_recording();
    if (g.rendering)
    {
        return;
    }
    const vk::Image& color = render_color();
    const vk::Image& depth = render_depth();
    VkRenderingAttachmentInfo color_attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    color_attachment.imageView = color.view;
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingAttachmentInfo depth_attachment = color_attachment;
    depth_attachment.imageView = depth.view;
    VkRenderingInfo info{VK_STRUCTURE_TYPE_RENDERING_INFO};
    info.renderArea = {{0, 0}, {u32(color.width), u32(color.height)}};
    info.layerCount = 1;
    info.colorAttachmentCount = 1;
    info.pColorAttachments = &color_attachment;
    info.pDepthAttachment = &depth_attachment;
    info.pStencilAttachment = &depth_attachment;
    vkCmdBeginRendering(g.command, &info);
    g.rendering = true;
    g.bound_pipeline = VK_NULL_HANDLE;
}

// Ends the command buffer, submits it and waits. Everything recorded so far has then executed, so the ring and
// the retired images are free again.
void submit(VkSemaphore wait = VK_NULL_HANDLE, VkSemaphore signal = VK_NULL_HANDLE)
{
    if (!g.recording)
    {
        return;
    }
    end_rendering();
    vk::check(vkEndCommandBuffer(g.command), "vkEndCommandBuffer");
    g.recording = false;
    const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    info.commandBufferCount = 1;
    info.pCommandBuffers = &g.command;
    if (wait != VK_NULL_HANDLE)
    {
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &wait;
        info.pWaitDstStageMask = &stage;
    }
    if (signal != VK_NULL_HANDLE)
    {
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &signal;
    }
    vk::check(vkQueueSubmit(g.context.queue, 1, &info, g.fence), "vkQueueSubmit");
    vk::check(vkWaitForFences(g.context.device, 1, &g.fence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
    vk::check(vkResetFences(g.context.device, 1, &g.fence), "vkResetFences");
    g.ring_cursor = 0;
#if GBTG_OPTIMIZE
    ++g.ring_epoch;
#endif
    for (vk::Image& image : g.retired)
    {
        vk::image_destroy(g.context, image);
    }
    g.retired.clear();
#if GBTG_OPTIMIZE
    for (vk::Buffer& buffer : g.retired_buffers)
    {
        vk::buffer_destroy(g.context, buffer);
    }
    g.retired_buffers.clear();
#endif
}

#if GBTG_OPTIMIZE
constexpr VkDeviceSize kRetainedUploadBytes = VkDeviceSize(256) << 20;
constexpr u64 kRetainedUploadIdleFrames = 600;

void retire_retained_upload(GfxVulkan::RetainedUpload& upload)
{
    g.retained_bytes -= upload.buffer.size;
    g.retired_buffers.push_back(upload.buffer);
    upload.buffer = {};
}

// GBTG_OPTIMIZE step 7: the buffer holding a retained geometry id, filled on its first draw.
GfxVulkan::RetainedUpload& retained_upload(const GfxNativeDraw& draw)
{
    auto found = g.retained_uploads.find(draw.retained_geometry);
    if (found != g.retained_uploads.end())
    {
        if (render_profile::enabled)
        {
            // Counted as uploaded at the top of gfx_draw_native; this draw uploads nothing.
            render_profile::frame.uploaded_bytes -= draw.program_inputs.size_bytes() + draw.indices.size_bytes();
        }
        found->second.last_used = g.frame_number;
        return found->second;
    }
    const VkDeviceSize vertex_bytes = draw.program_inputs.size_bytes();
    const VkDeviceSize total = vertex_bytes + draw.indices.size_bytes();
    if (g.retained_bytes + total > kRetainedUploadBytes)
    {
        for (auto& [id, upload] : g.retained_uploads)
        {
            (void)id;
            retire_retained_upload(upload);
        }
        g.retained_uploads.clear();
    }
    GfxVulkan::RetainedUpload upload;
    upload.buffer = vk::buffer_create(g.context, total, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    std::memcpy(upload.buffer.mapped, draw.program_inputs.data(), size_t(vertex_bytes));
    std::memcpy(upload.buffer.mapped + vertex_bytes, draw.indices.data(), draw.indices.size_bytes());
    upload.index_offset = vertex_bytes;
    upload.last_used = g.frame_number;
    g.retained_bytes += upload.buffer.size;
    if (render_profile::enabled)
    {
        ++render_profile::frame.buffer_allocations;
    }
    return g.retained_uploads.emplace(draw.retained_geometry, upload).first->second;
}

void age_retained_uploads()
{
    if (++g.frame_number % 128 != 0)
    {
        return;
    }
    for (auto upload = g.retained_uploads.begin(); upload != g.retained_uploads.end();)
    {
        if (upload->second.last_used + kRetainedUploadIdleFrames < g.frame_number)
        {
            retire_retained_upload(upload->second);
            upload = g.retained_uploads.erase(upload);
        }
        else
        {
            ++upload;
        }
    }
}
#endif

// Makes `bytes` (with alignment slack) available in the ring without another submission, so the offsets a draw
// takes from ring_take stay valid until it is recorded.
void ring_reserve(VkDeviceSize bytes)
{
    const VkDeviceSize needed = bytes + 4 * g.context.properties.limits.minUniformBufferOffsetAlignment + 64;
    if (g.ring_cursor + needed <= g.ring.size)
    {
        return;
    }
    submit();
    if (needed > g.ring.size)
    {
        vk::buffer_destroy(g.context, g.ring);
#if GBTG_OPTIMIZE
        ++g.ring_epoch;
#endif
        g.ring = vk::buffer_create(g.context, std::max(needed, g.ring.size * 2),
                                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                       | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        if (render_profile::enabled)
        {
            ++render_profile::frame.buffer_allocations;
        }
    }
}

VkDeviceSize ring_take(const void* data, VkDeviceSize bytes, VkDeviceSize alignment)
{
    const VkDeviceSize offset = (g.ring_cursor + alignment - 1) / alignment * alignment;
    if (offset + bytes > g.ring.size)
    {
        throw Error("vulkan: upload ring overflow");
    }
    std::memcpy(g.ring.mapped + offset, data, size_t(bytes));
    g.ring_cursor = offset + bytes;
    return offset;
}

#if GBTG_OPTIMIZE
// GBTG_OPTIMIZE step 10 (Vulkan): ring_take of a `bytes` slot, copying only [copy_begin, copy_begin + copy_bytes)
// of `data`. The rest of the slot keeps earlier ring contents, which the shader never reads.
VkDeviceSize ring_take_range(const void* data, VkDeviceSize bytes, VkDeviceSize alignment, VkDeviceSize copy_begin,
                             VkDeviceSize copy_bytes)
{
    const VkDeviceSize offset = (g.ring_cursor + alignment - 1) / alignment * alignment;
    if (offset + bytes > g.ring.size || copy_begin + copy_bytes > bytes)
    {
        throw Error("vulkan: upload ring overflow");
    }
    std::memcpy(g.ring.mapped + offset + copy_begin, static_cast<const u8*>(data) + copy_begin, size_t(copy_bytes));
    g.ring_cursor = offset + bytes;
    return offset;
}
#endif

VkDeviceSize uniform_alignment()
{
    return std::max<VkDeviceSize>(16, g.context.properties.limits.minUniformBufferOffsetAlignment);
}

// Readback copies land here; it grows to the largest one.
vk::Buffer& readback_buffer(VkDeviceSize bytes)
{
    if (g.readback.size < bytes)
    {
        submit();
        vk::buffer_destroy(g.context, g.readback);
        g.readback = vk::buffer_create(g.context, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
    }
    return g.readback;
}

void retire(vk::Image& image)
{
    if (image.image != VK_NULL_HANDLE)
    {
        g.retired.push_back(image);
    }
    image = {};
}

// Colour and depth-stencil surfaces stay in GENERAL; the backend's full barriers order every use.
void create_surface(vk::Image& color, vk::Image& depth, int width, int height)
{
    retire(color);
    retire(depth);
    color = vk::image_create(g.context, width, height, VK_FORMAT_B8G8R8A8_UNORM,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                             VK_IMAGE_ASPECT_COLOR_BIT);
    depth = vk::image_create(g.context, width, height, g.context.depth_format,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                             VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    ensure_recording();
    end_rendering();
    vk::image_barrier(g.command, color.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    vk::image_barrier(g.command, depth.image, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
}

// --- Textures and samplers -----------------------------------------------------------------------------------

// Writes `pixels` (BGRA rows) into a sampled image, which is left in SHADER_READ_ONLY_OPTIMAL.
void upload_texture(vk::Image& image, const u8* pixels, bool fresh)
{
    const VkDeviceSize bytes = VkDeviceSize(image.width) * VkDeviceSize(image.height) * 4;
    ring_reserve(bytes);
    const VkDeviceSize offset = ring_take(pixels, bytes, 16);
    ensure_recording();
    end_rendering();
    vk::image_barrier(g.command, image.image, VK_IMAGE_ASPECT_COLOR_BIT,
                      fresh ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy copy{};
    copy.bufferOffset = offset;
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {u32(image.width), u32(image.height), 1};
    vkCmdCopyBufferToImage(g.command, g.ring.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    vk::image_barrier(g.command, image.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

vk::Image create_texture(int width, int height)
{
    return vk::image_create(g.context, width, height, VK_FORMAT_B8G8R8A8_UNORM,
                            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
}

// The image's texture, uploaded again when its serial changed. Null for an absent or empty image.
VkImageView native_texture(const RgbaImage* image)
{
    render_profile::Scope measurement(render_profile::Texture);
    if (image == nullptr || image->bgra.empty() || image->width <= 0 || image->height <= 0)
    {
        return VK_NULL_HANDLE;
    }
    auto it = g.textures.find(image);
    if (it != g.textures.end() && it->second.serial == image->serial)
    {
        return it->second.image.view;
    }
    if (render_profile::enabled)
    {
        ++render_profile::frame.texture_uploads;
        render_profile::frame.uploaded_bytes += image->bgra.size();
    }
    // A refreshed image of the same size (movie frames, render-target captures) is written in place; the copy is
    // ordered after the draws already recorded against the old contents.
    if (it != g.textures.end() && it->second.image.width == image->width && it->second.image.height == image->height)
    {
        upload_texture(it->second.image, image->bgra.data(), false);
        it->second.serial = image->serial;
        return it->second.image.view;
    }
    if (it != g.textures.end())
    {
        retire(it->second.image);
    }
    Texture texture;
    texture.image = create_texture(image->width, image->height);
    texture.serial = image->serial;
    upload_texture(texture.image, image->bgra.data(), true);
    it = g.textures.insert_or_assign(image, texture).first;
    return it->second.image.view;
}

VkSamplerAddressMode native_address(u32 mode)
{
    switch (mode)
    {
    case 1:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case 2:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case 3:
    case 5:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case 4:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    default:
        throw Error("native draw uses texture address mode " + std::to_string(mode));
    }
}

// D3D11_FILTER_MIN_MAG_MIP_LINEAR with the default transparent-black border.
VkSampler make_sampler(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w)
{
    VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.addressModeU = u;
    info.addressModeV = v;
    info.addressModeW = w;
    info.maxLod = VK_LOD_CLAMP_NONE;
    info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    VkSampler sampler = VK_NULL_HANDLE;
    vk::check(vkCreateSampler(g.context.device, &info, nullptr, &sampler), "vkCreateSampler");
    return sampler;
}

VkSampler native_sampler(u32 address_u, u32 address_v)
{
    const u32 key = address_u | (address_v << 8);
    auto found = g.samplers.find(key);
    if (found == g.samplers.end())
    {
        if (render_profile::enabled)
        {
            ++render_profile::frame.state_misses;
        }
        found = g.samplers.emplace(key, make_sampler(native_address(address_u), native_address(address_v),
                                                     VK_SAMPLER_ADDRESS_MODE_REPEAT)).first;
    }
    return found->second;
}

// --- NV2A state ----------------------------------------------------------------------------------------------

VkBlendFactor native_blend_factor(u32 factor, bool alpha)
{
    switch (factor)
    {
    case 0:
        return VK_BLEND_FACTOR_ZERO;
    case 1:
        return VK_BLEND_FACTOR_ONE;
    case 0x300:
        return alpha ? VK_BLEND_FACTOR_SRC_ALPHA : VK_BLEND_FACTOR_SRC_COLOR;
    case 0x301:
        return alpha ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case 0x302:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case 0x303:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case 0x304:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case 0x305:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case 0x306:
        return alpha ? VK_BLEND_FACTOR_DST_ALPHA : VK_BLEND_FACTOR_DST_COLOR;
    case 0x307:
        return alpha ? VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA : VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case 0x308:
        return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    default:
        throw Error("native draw uses blend factor " + std::to_string(factor));
    }
}

VkBlendOp native_blend_equation(u32 equation)
{
    switch (equation)
    {
    case 0x8006:
        return VK_BLEND_OP_ADD;
    case 0x8007:
        return VK_BLEND_OP_MIN;
    case 0x8008:
        return VK_BLEND_OP_MAX;
    case 0x800A:
        return VK_BLEND_OP_SUBTRACT;
    case 0x800B:
        return VK_BLEND_OP_REVERSE_SUBTRACT;
    default:
        throw Error("native draw uses blend equation " + std::to_string(equation));
    }
}

VkStencilOp native_stencil_op(u32 operation)
{
    switch (operation)
    {
    case 0x1E00:
        return VK_STENCIL_OP_KEEP;
    case 0:
        return VK_STENCIL_OP_ZERO;
    case 0x1E01:
        return VK_STENCIL_OP_REPLACE;
    case 0x1E02:
        return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case 0x1E03:
        return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case 0x150A:
        return VK_STENCIL_OP_INVERT;
    case 0x8507:
        return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case 0x8508:
        return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    default:
        throw Error("native stencil operation " + std::to_string(operation) + " is not an NV097 operation");
    }
}

// NV2A comparisons 0x200 (never) .. 0x207 (always) are VkCompareOp's order.
VkCompareOp native_comparison(u32 function)
{
    if (function < 0x200 || function > 0x207)
    {
        throw Error("native draw uses comparison " + std::to_string(function));
    }
    return VkCompareOp(function - 0x200);
}

// Retail 0x00171420 emits CULL_FACE FRONT when CullMode == FrontFace (0x900). The flipped viewport keeps Xbox
// screen orientation, where front faces are clockwise as in D3D11, so 0x900 culls FRONT and 0x901 BACK.
VkCullModeFlags native_cull(u32 cull)
{
    switch (cull)
    {
    case 0:
        return VK_CULL_MODE_NONE;
    case 0x900:
        return VK_CULL_MODE_FRONT_BIT;
    case 0x901:
        return VK_CULL_MODE_BACK_BIT;
    default:
        throw Error("native draw uses cull mode " + std::to_string(cull));
    }
}

// --- Pipelines -----------------------------------------------------------------------------------------------

void vertex_input(VertexLayout layout, std::vector<VkVertexInputAttributeDescription>& attributes, u32& stride)
{
    const auto add = [&](u32 location, VkFormat format, u32 offset)
    {
        attributes.push_back({location, 0, format, offset});
    };
    switch (layout)
    {
    case VertexLayout::Native:
        // native_vs's NativeIn, in declaration order.
        stride = sizeof(GfxNativeVertex);
        add(0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
        add(1, VK_FORMAT_R32G32B32A32_SFLOAT, 16);
        add(2, VK_FORMAT_R32G32B32A32_SFLOAT, 32);
        add(3, VK_FORMAT_R32G32_SFLOAT, 48);
        add(4, VK_FORMAT_R32G32_SFLOAT, 56);
        add(5, VK_FORMAT_R32G32_SFLOAT, 64);
        add(6, VK_FORMAT_R32G32_SFLOAT, 72);
        add(7, VK_FORMAT_R32_SFLOAT, 80);
        add(8, VK_FORMAT_R32G32_SFLOAT, 84);
        add(9, VK_FORMAT_R32G32_SFLOAT, 92);
        add(10, VK_FORMAT_R32G32_SFLOAT, 100);
        add(11, VK_FORMAT_R32G32_SFLOAT, 108);
        break;
    case VertexLayout::Program:
        // program_vs's ProgramInput v0..v15.
        stride = sizeof(GfxNativeInputs);
        for (u32 n = 0; n < 16; ++n)
        {
            add(n, VK_FORMAT_R32G32B32A32_SFLOAT, n * 16);
        }
        break;
    case VertexLayout::Sprite:
        // vs_main's VSIn, in declaration order.
        stride = sizeof(GpuVert);
        add(0, VK_FORMAT_R32G32B32_SFLOAT, 0);
        add(1, VK_FORMAT_R32G32_SFLOAT, 12);
        add(2, VK_FORMAT_R32G32_SFLOAT, 20);
        add(3, VK_FORMAT_R32G32B32_SFLOAT, 28);
        add(4, VK_FORMAT_R32G32_SFLOAT, 48);
        add(5, VK_FORMAT_R32G32_SFLOAT, 56);
        add(6, VK_FORMAT_R32G32_SFLOAT, 40);
        add(7, VK_FORMAT_R8G8B8A8_UNORM, 64);
        add(8, VK_FORMAT_R8G8B8A8_UNORM, 68);
        add(9, VK_FORMAT_R32G32_SFLOAT, 72);
        add(10, VK_FORMAT_R32G32_SFLOAT, 80);
        add(11, VK_FORMAT_R32G32_SFLOAT, 88);
        break;
    }
}

VkPipeline pipeline(const PipelineState& state)
{
    const auto key = state.key();
#if GBTG_OPTIMIZE
    if (g.last_pipeline != VK_NULL_HANDLE && g.last_pipeline_key == key)
    {
        return g.last_pipeline;
    }
#endif
    auto found = g.pipelines.find(key);
    if (found != g.pipelines.end())
    {
#if GBTG_OPTIMIZE
        g.last_pipeline_key = key;
        g.last_pipeline = found->second;
#endif
        return found->second;
    }
    render_profile::Scope compilation(render_profile::Compilation);
    if (render_profile::enabled)
    {
        ++render_profile::frame.state_misses;
    }
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = state.vertex->module;
    stages[0].pName = state.vertex->entry.c_str();
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = state.pixel->module;
    stages[1].pName = state.pixel->entry.c_str();

    std::vector<VkVertexInputAttributeDescription> attributes;
    u32 stride = 0;
    vertex_input(state.layout, attributes, stride);
    const VkVertexInputBindingDescription binding{0, stride, VK_VERTEX_INPUT_RATE_VERTEX};
    VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    input.vertexBindingDescriptionCount = 1;
    input.pVertexBindingDescriptions = &binding;
    input.vertexAttributeDescriptionCount = u32(attributes.size());
    input.pVertexAttributeDescriptions = attributes.data();
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = state.lines ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = state.cull;
    raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    raster.lineWidth = 1.f;
    // NV2A polygon offset: units -bias (the depth buffer's resolvable step) and factor -0.25 bias.
    raster.depthBiasEnable = state.z_bias != 0 ? VK_TRUE : VK_FALSE;
    raster.depthBiasConstantFactor = float(-state.z_bias);
    raster.depthBiasSlopeFactor = float(state.z_bias) * -0.25f;
    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    // NV2A writes depth only while its test is enabled; Vulkan, like D3D11, does the same.
    depth.depthTestEnable = state.depth_test ? VK_TRUE : VK_FALSE;
    depth.depthWriteEnable = state.depth_write ? VK_TRUE : VK_FALSE;
    depth.depthCompareOp = state.depth_compare;
    depth.stencilTestEnable = state.stencil ? VK_TRUE : VK_FALSE;
    depth.front.failOp = state.stencil_fail;
    depth.front.passOp = state.stencil_pass;
    depth.front.depthFailOp = state.stencil_depth_fail;
    depth.front.compareOp = state.stencil_compare;
    depth.front.compareMask = state.stencil_read_mask;
    depth.front.writeMask = state.stencil_write_mask;
    depth.back = depth.front;
    VkPipelineColorBlendAttachmentState attachment{};
    attachment.blendEnable = state.blend ? VK_TRUE : VK_FALSE;
    attachment.srcColorBlendFactor = state.source_color;
    attachment.dstColorBlendFactor = state.destination_color;
    attachment.colorBlendOp = state.equation;
    attachment.srcAlphaBlendFactor = state.source_alpha;
    attachment.dstAlphaBlendFactor = state.destination_alpha;
    attachment.alphaBlendOp = state.equation;
    attachment.colorWriteMask = state.write_mask;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1;
    blend.pAttachments = &attachment;
    const VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
                                             VK_DYNAMIC_STATE_STENCIL_REFERENCE};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = u32(std::size(dynamic_states));
    dynamic.pDynamicStates = dynamic_states;
    const VkFormat color_format = VK_FORMAT_B8G8R8A8_UNORM;
    VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachmentFormats = &color_format;
    rendering.depthAttachmentFormat = g.context.depth_format;
    rendering.stencilAttachmentFormat = g.context.depth_format;

    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    info.pNext = &rendering;
    info.stageCount = 2;
    info.pStages = stages;
    info.pVertexInputState = &input;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &multisample;
    info.pDepthStencilState = &depth;
    info.pColorBlendState = &blend;
    info.pDynamicState = &dynamic;
    info.layout = g.pipeline_layout;
    VkPipeline created = VK_NULL_HANDLE;
    vk::check(vkCreateGraphicsPipelines(g.context.device, VK_NULL_HANDLE, 1, &info, nullptr, &created),
              "vkCreateGraphicsPipelines");
    g.pipelines.emplace(key, created);
    return created;
}

// --- Viewports -----------------------------------------------------------------------------------------------

// Xbox 0x000F6F70. DAT_005096E8 / DAT_005096EA are the HUD width and height, 640x480; a texture target takes Xbox
// pixels 1:1 and the back buffer the fitted 4:3 area.
Viewport hud_viewport()
{
    Viewport vp;
    if (g.target_rendering)
    {
        vp.width = 640.f;
        vp.height = 480.f;
        return vp;
    }
    // The 640x480 frame fills a 16:9 area in widescreen (the Xbox's anamorphic output), else 4:3.
    float frame = g.cfg.widescreen ? 16.f / 9.f : 640.f / 480.f;
    if (g.cfg.frame_aspect > 0.f)
    {
        frame = g.cfg.frame_aspect; // The wide_aspect plugin's frame.
    }
    const float window = float(g.cfg.width) / float(g.cfg.height);
    if (window > frame)
    {
        vp.height = float(g.cfg.height);
        vp.width = vp.height * frame;
        vp.x = (float(g.cfg.width) - vp.width) * 0.5f;
    }
    else
    {
        vp.width = float(g.cfg.width);
        vp.height = vp.width / frame;
        vp.y = (float(g.cfg.height) - vp.height) * 0.5f;
    }
    return vp;
}

void present_hud_viewport()
{
    if (!g.ok || g.cfg.width <= 0 || g.cfg.height <= 0)
    {
        return;
    }
    g.viewport = hud_viewport();
}

Viewport full_viewport()
{
    return Viewport{0.f, 0.f, float(g.cfg.width), float(g.cfg.height)};
}

VkExtent2D target_extent()
{
    const vk::Image& color = render_color();
    return {u32(color.width), u32(color.height)};
}

// Clamped to the target: Vulkan scissors and clear rectangles may not leave the attachment.
VkRect2D clamp_rect(i32 left, i32 top, i32 right, i32 bottom)
{
    const VkExtent2D extent = target_extent();
    left = std::clamp<i32>(left, 0, i32(extent.width));
    right = std::clamp<i32>(right, 0, i32(extent.width));
    top = std::clamp<i32>(top, 0, i32(extent.height));
    bottom = std::clamp<i32>(bottom, 0, i32(extent.height));
    return {{left, top}, {u32(std::max(0, right - left)), u32(std::max(0, bottom - top))}};
}

// The HUD viewport maps the 640x480 back buffer onto the window; rectangles follow it.
VkRect2D native_rectangle(const std::array<i32, 4>& rectangle)
{
    const Viewport& vp = g.viewport;
    return clamp_rect(i32(std::floor(vp.x + vp.width * float(rectangle[0]) / 640.f)),
                      i32(std::floor(vp.y + vp.height * float(rectangle[1]) / 480.f)),
                      i32(std::ceil(vp.x + vp.width * float(rectangle[2]) / 640.f)),
                      i32(std::ceil(vp.y + vp.height * float(rectangle[3]) / 480.f)));
}

void set_viewport_and_scissor(const VkRect2D& scissor)
{
    const VkViewport viewport{g.viewport.x, g.viewport.y + g.viewport.height, g.viewport.width, -g.viewport.height,
                              0.f, 1.f};
#if GBTG_OPTIMIZE
    auto& state = g.command_state;
    if (state.viewport_valid && std::memcmp(&state.viewport, &viewport, sizeof(viewport)) == 0
        && std::memcmp(&state.scissor, &scissor, sizeof(scissor)) == 0)
    {
        return;
    }
    state.viewport_valid = true;
    state.viewport = viewport;
    state.scissor = scissor;
#endif
    vkCmdSetViewport(g.command, 0, 1, &viewport);
    vkCmdSetScissor(g.command, 0, 1, &scissor);
}

void clear_attachments(const VkRect2D& rectangle, bool color, const std::array<float, 4>& rgba, VkImageAspectFlags depth_aspects,
                       float depth)
{
    if (rectangle.extent.width == 0 || rectangle.extent.height == 0)
    {
        return;
    }
    begin_rendering();
    VkClearAttachment attachments[2]{};
    u32 count = 0;
    if (color)
    {
        attachments[count].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        attachments[count].colorAttachment = 0;
        std::memcpy(attachments[count].clearValue.color.float32, rgba.data(), sizeof(float) * 4);
        ++count;
    }
    if (depth_aspects != 0)
    {
        attachments[count].aspectMask = depth_aspects;
        attachments[count].clearValue.depthStencil = {depth, 0};
        ++count;
    }
    if (count == 0)
    {
        return;
    }
    const VkClearRect rect{rectangle, 0, 1};
    vkCmdClearAttachments(g.command, count, attachments, 1, &rect);
}

VkRect2D whole_target()
{
    return VkRect2D{{0, 0}, target_extent()};
}

std::array<float, 4> argb_color(u32 color)
{
    return {float((color >> 16) & 0xFF) / 255.f, float((color >> 8) & 0xFF) / 255.f, float(color & 0xFF) / 255.f,
            float(color >> 24) / 255.f};
}

// --- Draw submission -----------------------------------------------------------------------------------------

struct DrawBindings
{
    VkDeviceSize camera = 0;
    VkDeviceSize material = VK_WHOLE_SIZE;  // VK_WHOLE_SIZE: the zero buffer
    VkDeviceSize constants = VK_WHOLE_SIZE;
    std::array<VkImageView, 4> views{};
    std::array<VkSampler, 4> samplers{};
};

void push_descriptors(const DrawBindings& bindings)
{
    VkDescriptorBufferInfo buffers[4]{};
    buffers[0] = {g.ring.buffer, bindings.camera, sizeof(GpuCB)};
    buffers[1] = {g.zero_uniform.buffer, 0, kBonesBytes};
    buffers[2] = bindings.material == VK_WHOLE_SIZE ? VkDescriptorBufferInfo{g.zero_uniform.buffer, 0, kMaterialBytes}
                                                    : VkDescriptorBufferInfo{g.ring.buffer, bindings.material, kMaterialBytes};
    buffers[3] = bindings.constants == VK_WHOLE_SIZE ? VkDescriptorBufferInfo{g.zero_uniform.buffer, 0, kConstantsBytes}
                                                     : VkDescriptorBufferInfo{g.ring.buffer, bindings.constants, kConstantsBytes};
    VkDescriptorImageInfo images[4]{};
    VkDescriptorImageInfo samplers[4]{};
    for (size_t stage = 0; stage < 4; ++stage)
    {
        const VkImageView view = bindings.views[stage];
        images[stage].imageView = view != VK_NULL_HANDLE ? view : g.zero_texture.view;
        images[stage].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        samplers[stage].sampler = bindings.samplers[stage] != VK_NULL_HANDLE ? bindings.samplers[stage] : g.wrap_sampler;
    }
#if GBTG_OPTIMIZE
    auto& state = g.command_state;
    VkWriteDescriptorSet changed[12]{};
    u32 count = 0;
    for (u32 n = 0; n < 4; ++n)
    {
        const bool buffer_same = state.valid && state.buffers[n].buffer == buffers[n].buffer
            && state.buffers[n].offset == buffers[n].offset && state.buffers[n].range == buffers[n].range;
        if (!buffer_same)
        {
            changed[count] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            changed[count].dstBinding = vk::kConstantBinding + n;
            changed[count].descriptorCount = 1;
            changed[count].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            changed[count].pBufferInfo = &buffers[n];
            ++count;
            state.buffers[n] = buffers[n];
        }
        if (!state.valid || state.views[n] != images[n].imageView)
        {
            changed[count] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            changed[count].dstBinding = vk::kTextureBinding + n;
            changed[count].descriptorCount = 1;
            changed[count].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            changed[count].pImageInfo = &images[n];
            ++count;
            state.views[n] = images[n].imageView;
        }
        if (!state.valid || state.samplers[n] != samplers[n].sampler)
        {
            changed[count] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            changed[count].dstBinding = vk::kSamplerBinding + n;
            changed[count].descriptorCount = 1;
            changed[count].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            changed[count].pImageInfo = &samplers[n];
            ++count;
            state.samplers[n] = samplers[n].sampler;
        }
    }
    state.valid = true;
    if (count != 0)
    {
        g.context.push_descriptor_set(g.command, VK_PIPELINE_BIND_POINT_GRAPHICS, g.pipeline_layout, 0, count, changed);
    }
#else
    VkWriteDescriptorSet writes[12]{};
    for (u32 n = 0; n < 4; ++n)
    {
        writes[n] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[n].dstBinding = vk::kConstantBinding + n;
        writes[n].descriptorCount = 1;
        writes[n].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[n].pBufferInfo = &buffers[n];
        writes[4 + n] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[4 + n].dstBinding = vk::kTextureBinding + n;
        writes[4 + n].descriptorCount = 1;
        writes[4 + n].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[4 + n].pImageInfo = &images[n];
        writes[8 + n] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[8 + n].dstBinding = vk::kSamplerBinding + n;
        writes[8 + n].descriptorCount = 1;
        writes[8 + n].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        writes[8 + n].pImageInfo = &samplers[n];
    }
    g.context.push_descriptor_set(g.command, VK_PIPELINE_BIND_POINT_GRAPHICS, g.pipeline_layout, 0, 12, writes);
#endif
}

void bind_pipeline(VkPipeline pipeline)
{
    if (g.bound_pipeline != pipeline)
    {
        vkCmdBindPipeline(g.command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        g.bound_pipeline = pipeline;
    }
}

// Specialize structural words only; the material buffer still owns mutable colour constants.
// Same HLSL equations, register dependencies, alpha test and outputs as the generic reference.
const Shader* native_pixel_shader(const u32* definition)
{
#if GBTG_OPTIMIZE
    if (g.last_pixel != nullptr && !render_profile::reference
        && std::memcmp(g.last_pixel_words.data(), definition, sizeof(g.last_pixel_words)) == 0)
    {
        return g.last_pixel;
    }
#endif
    gfx_check_native_pixel_definition(definition);
    if (render_profile::reference)
    {
        return &g.model_ps;
    }
    const std::array<u32, 60> key = gfx_native_pixel_key(definition);
    const bool depth_output = gfx_native_pixel_writes_depth(key);
    auto found = g.specialized_pixels.find(key);
    if (found != g.specialized_pixels.end())
    {
#if GBTG_OPTIMIZE
        std::memcpy(g.last_pixel_words.data(), definition, sizeof(g.last_pixel_words));
        // A null entry (compilation fell back) is returned as stored and is never memoised.
        g.last_pixel = found->second.get();
        return found->second.get();
#else
        return found->second.get();
#endif
    }
    render_profile::Scope compilation(render_profile::Compilation);
    const std::string source = gfx_specialize_native_pixel_source(g.model_source, key);
    auto shader = std::make_unique<Shader>(make_shader(source, depth_output ? "ps_model_depth" : "ps_model", "ps_6_0"));
    const Shader* result = shader.get();
    if (shader->module == VK_NULL_HANDLE)
    {
        if (depth_output)
        {
            throw Error("native depth-replacing pixel shader failed to compile");
        }
        // Explicit fallback to the unchanged generic implementation, including its error checks.
        shader.reset();
        result = &g.model_ps;
    }
    if (render_profile::enabled)
    {
        ++render_profile::frame.pixel_misses;
    }
    g.specialized_pixels.emplace(key, std::move(shader));
    return result;
}

void draw_sprite_quad(VkImageView view, const GfxRect& rect)
{
    const float x0 = rect.x * 2.f - 1.f;
    const float y0 = 1.f - rect.y * 2.f;
    const float x1 = (rect.x + rect.w) * 2.f - 1.f;
    const float y1 = 1.f - (rect.y + rect.h) * 2.f;
    const GpuVert verts[6] = {
        {x0, y0, 0.f, rect.u0, rect.v0, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}, {x1, y0, 0.f, rect.u1, rect.v0, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
        {x0, y1, 0.f, rect.u0, rect.v1, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}, {x1, y0, 0.f, rect.u1, rect.v0, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
        {x1, y1, 0.f, rect.u1, rect.v1, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}, {x0, y1, 0.f, rect.u0, rect.v1, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    };
    GpuCB cb;
    cb.mvp[0] = cb.mvp[5] = cb.mvp[10] = cb.mvp[15] = 1.f;
    // The retail alpha-test shadow (0x0005E0C0 / 0x0015AD20) is only written by the host mesh path, which the
    // Vulkan backend does not have, so it is still clear: no alpha test.
    cb.alpha_pad = 1.f;
    cb.tint = rect.color;

    ring_reserve(sizeof(verts) + sizeof(cb));
    DrawBindings bindings;
    bindings.camera = ring_take(&cb, sizeof(cb), uniform_alignment());
    const VkDeviceSize vertex_offset = ring_take(verts, sizeof(verts), 16);
    bindings.views[0] = view;
    bindings.samplers[0] = g.wrap_sampler;
    bindings.samplers[1] = g.clamp_sampler;

    PipelineState state;
    state.vertex = &g.sprite_vs;
    state.pixel = &g.sprite_ps;
    state.layout = VertexLayout::Sprite;
    state.blend = true;
    state.source_color = VK_BLEND_FACTOR_SRC_ALPHA;
    state.destination_color = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    state.source_alpha = VK_BLEND_FACTOR_ONE;
    state.destination_alpha = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    const VkPipeline sprite_pipeline = pipeline(state);

    begin_rendering();
    bind_pipeline(sprite_pipeline);
    // The shared host rasterizer state has no scissor test.
    set_viewport_and_scissor(whole_target());
    push_descriptors(bindings);
    vkCmdBindVertexBuffers(g.command, 0, 1, &g.ring.buffer, &vertex_offset);
    vkCmdDraw(g.command, 6, 1, 0, 0);
}

// --- Frame ---------------------------------------------------------------------------------------------------

// Copies the back buffer onto the window and presents it. Without a window this only submits.
void present_frame_image()
{
#if GBTG_OPTIMIZE
    take_window_events();
#endif
    if (g.window == nullptr || !vk::swapchain_ready(g.context, g.swapchain))
    {
        submit();
        return;
    }
    ensure_recording();
    end_rendering();
    u32 index = 0;
    const VkResult acquired = vkAcquireNextImageKHR(g.context.device, g.swapchain.swapchain, UINT64_MAX,
                                                    g.swapchain.acquired, VK_NULL_HANDLE, &index);
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR)
    {
        g.swapchain.stale = true;
        submit();
        return;
    }
    if (acquired != VK_SUBOPTIMAL_KHR)
    {
        vk::check(acquired, "vkAcquireNextImageKHR");
    }
    const VkImage image = g.swapchain.images[index];
    vk::image_barrier(g.command, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    // DXGI stretches the fixed-size back buffer over the client area; the blit does the same.
    VkImageBlit blit{};
    blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.srcOffsets[1] = {g.frame_color.width, g.frame_color.height, 1};
    blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.dstOffsets[1] = {i32(g.swapchain.extent.width), i32(g.swapchain.extent.height), 1};
    vkCmdBlitImage(g.command, g.frame_color.image, VK_IMAGE_LAYOUT_GENERAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &blit, VK_FILTER_LINEAR);
    VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    // Plugin overlays (the debug UI) draw over the copy, so the held back buffer stays without them for
    // gfx_present_again.
#if !defined(GBTG_RETAIL_GAME_ONLY)
    if (plugins_overlay_has_draws())
    {
        vk::image_barrier(g.command, image, VK_IMAGE_ASPECT_COLOR_BIT, layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkRenderingAttachmentInfo attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        attachment.imageView = g.swapchain.views[index];
        attachment.imageLayout = layout;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
        rendering.renderArea = {{0, 0}, g.swapchain.extent};
        rendering.layerCount = 1;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachments = &attachment;
        vkCmdBeginRendering(g.command, &rendering);
        plugins_overlay_render(g.command);
#if GBTG_OPTIMIZE
        g.command_state = {};
#endif
        vkCmdEndRendering(g.command);
    }
#endif
    vk::image_barrier(g.command, image, VK_IMAGE_ASPECT_COLOR_BIT, layout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    const VkSemaphore rendered = g.swapchain.rendered[index];
    {
        render_profile::Scope measurement(render_profile::Present);
        submit(g.swapchain.acquired, rendered);
        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &rendered;
        present.swapchainCount = 1;
        present.pSwapchains = &g.swapchain.swapchain;
        present.pImageIndices = &index;
        const VkResult presented = vkQueuePresentKHR(g.context.queue, &present);
        if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR)
        {
            g.swapchain.stale = true;
        }
        else
        {
            vk::check(presented, "vkQueuePresentKHR");
        }
    }
}

// Xbox 0x00033A90
// DAT_0033A5B8 is the full backbuffer: origin 0, retail 640x480, z 0..1.
void present_set_viewport()
{
    if (!g.ok)
    {
        return;
    }
    g.viewport = full_viewport();
}

// Xbox 0x00033A90
// 0x00154A20: D3DDevice_Clear(0, nullptr, flags, DAT_00434264, 1.0f, 0).
// Z (1) and stencil (2) clear here. The target mask 0xF0 stays off this call so the
// scene drawn earlier in the frame is still in the color target for the swap.
void present_clear(u32 flags)
{
    if (!g.ok)
    {
        return;
    }
    VkImageAspectFlags aspects = 0;
    if ((flags & 0x1u) != 0)
    {
        aspects |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if ((flags & 0x2u) != 0)
    {
        aspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    clear_attachments(whole_target(), false, {}, aspects, 1.f);
}

// Xbox 0x00033A90
// 0x00154A40. Notify 0, D3DDevice_Swap(0), notify 1.
void present_swap()
{
    gfx_present_swap_notify(0);
    if (g.ok)
    {
        present_frame_image();
        g.frame_finished = true;
    }
    gfx_present_swap_notify(1);
}

// Quad 0x0005E380 / 0x0005E770 push. x,y is the top-left in the 640x480 HUD.
struct HudQuad
{
    const RgbaImage* image = nullptr;
    float x = 0.f;
    float y = 0.f;
    float w = 0.f;
    float h = 0.f;
    float u0 = 0.f;
    float v0 = 0.f;
    float u1 = 1.f;
    float v1 = 1.f;
    std::array<float, 4> color{1.f, 1.f, 1.f, 1.f};
};

// 0x000662A0 grows by the u16 at pool+2 when the cursor meets the limit.
u16 hud_extra = 16;
std::vector<HudQuad> hud_quads;

// DAT_00504960. 0x00066080 walks begin (+4) up to the cursor (+8), stride 0x18.
// 0x00065F90 writes the flag at +0x14. No submitter fills this list yet.
struct DepthProbe
{
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float depth = 0.f;
    float reach = 0.f;
    float flag = 0.f;
};
static_assert(sizeof(DepthProbe) == 0x18, "DAT_00504960 stride");

std::vector<DepthProbe> depth_probes;

// Quads submitted through 0x0005CA60, drawn in the HUD viewport.
void present_hud_sprites()
{
    present_hud_viewport();
    for (const HudQuad& quad : hud_quads)
    {
        if (quad.image == nullptr || quad.w == 0.f || quad.h == 0.f)
        {
            continue;
        }
        GfxRect rect;
        rect.x = quad.x / 640.f;
        rect.y = quad.y / 480.f;
        rect.w = quad.w / 640.f;
        rect.h = quad.h / 480.f;
        rect.u0 = quad.u0;
        rect.v0 = quad.v0;
        rect.u1 = quad.u1;
        rect.v1 = quad.v1;
        rect.color = quad.color;
        vulkan::gfx_draw_sprite(*quad.image, rect);
    }
}

// Xbox 0x00066080
// An empty cursor matches begin, which is the boot state, so the pass returns.
void present_sprite_span()
{
    if (depth_probes.empty())
    {
        return;
    }
    for (DepthProbe& probe : depth_probes)
    {
        // 0x00065F90 clears the flag, then samples the depth surface when one is locked.
        probe.flag = 0.f;
    }
}

// Xbox 0x00033A90
void present_draw_frame()
{
    const u32 flags = gfx_present_clear_flags();
    present_set_viewport();
    present_clear(flags);
    // 0x0005E110 installs the frame render states. Each Vulkan draw carries its whole state in its pipeline.
    // 0x000F9780 retires the deferred span at DAT_005079E0.
    // 0x00036180 submits the draw span at DAT_004DF240 and resets it.
    gfx_present_overlays();
    gfx_present_cameras();
    // 0x000F6F70 restores the HUD viewport from DAT_005096E8 / DAT_005096EA.
    present_hud_sprites();
    // 0x00066080 walks the 0x18 depth list at DAT_00504960.
    present_sprite_span();
    present_swap();
}

[[noreturn]] void unsupported(const char* what)
{
    throw Error(std::string("vulkan: ") + what + " belongs to the legacy host mesh path, which the Vulkan backend does not implement");
}

void release_all()
{
    if (g.context.device == VK_NULL_HANDLE)
    {
        // context_create may have stopped after the instance or surface.
        vk::context_destroy(g.context);
        return;
    }
    vkDeviceWaitIdle(g.context.device);
    for (vk::Image& image : g.retired)
    {
        vk::image_destroy(g.context, image);
    }
    g.retired.clear();
#if GBTG_OPTIMIZE
    for (auto& [id, upload] : g.retained_uploads)
    {
        (void)id;
        vk::buffer_destroy(g.context, upload.buffer);
    }
    g.retained_uploads.clear();
    g.retained_bytes = 0;
    for (vk::Buffer& buffer : g.retired_buffers)
    {
        vk::buffer_destroy(g.context, buffer);
    }
    g.retired_buffers.clear();
#endif
    for (auto& [image, texture] : g.textures)
    {
        (void)image;
        vk::image_destroy(g.context, texture.image);
    }
    g.textures.clear();
    for (auto& [key, created] : g.pipelines)
    {
        (void)key;
        vkDestroyPipeline(g.context.device, created, nullptr);
    }
    g.pipelines.clear();
#if GBTG_OPTIMIZE
    g.last_pipeline = VK_NULL_HANDLE;
    g.last_pixel = nullptr;
#endif
    for (auto& [key, sampler] : g.samplers)
    {
        (void)key;
        vkDestroySampler(g.context.device, sampler, nullptr);
    }
    g.samplers.clear();
    for (auto& [program, gpu] : g.native_programs)
    {
        (void)program;
        destroy_shader(gpu.shader);
    }
    g.native_programs.clear();
#if GBTG_OPTIMIZE
    g.program_memo.clear();
#endif
    for (auto& [key, shader] : g.specialized_pixels)
    {
        (void)key;
        if (shader)
        {
            destroy_shader(*shader);
        }
    }
    g.specialized_pixels.clear();
    for (Shader* shader : {&g.native_vs, &g.native_ps, &g.model_ps, &g.sprite_vs, &g.sprite_ps})
    {
        destroy_shader(*shader);
    }
    for (VkSampler* sampler : {&g.wrap_sampler, &g.clamp_sampler})
    {
        if (*sampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(g.context.device, *sampler, nullptr);
            *sampler = VK_NULL_HANDLE;
        }
    }
    for (vk::Image* image : {&g.frame_color, &g.frame_depth, &g.target_color, &g.target_depth, &g.zero_texture})
    {
        vk::image_destroy(g.context, *image);
    }
    for (vk::Buffer* buffer : {&g.ring, &g.readback, &g.zero_uniform})
    {
        vk::buffer_destroy(g.context, *buffer);
    }
#if GBTG_OPTIMIZE
    vk::buffer_destroy(g.context, g.ended_readback);
    g.ended_pending = false;
#endif
    if (g.pipeline_layout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(g.context.device, g.pipeline_layout, nullptr);
    }
    if (g.set_layout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(g.context.device, g.set_layout, nullptr);
    }
    if (g.fence != VK_NULL_HANDLE)
    {
        vkDestroyFence(g.context.device, g.fence, nullptr);
    }
    vk::swapchain_destroy(g.context, g.swapchain);
    const u32 errors = vk::context_validation_errors();
    if (g.context.validation)
    {
        log::info("vulkan: %u validation errors", errors);
    }
    vk::context_destroy(g.context);
}

void create_layout()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    const VkShaderStageFlags stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    for (u32 n = 0; n < 4; ++n)
    {
        bindings.push_back({vk::kConstantBinding + n, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, stages, nullptr});
        bindings.push_back({vk::kTextureBinding + n, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, stages, nullptr});
        bindings.push_back({vk::kSamplerBinding + n, VK_DESCRIPTOR_TYPE_SAMPLER, 1, stages, nullptr});
    }
    VkDescriptorSetLayoutCreateInfo set{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    set.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    set.bindingCount = u32(bindings.size());
    set.pBindings = bindings.data();
    vk::check(vkCreateDescriptorSetLayout(g.context.device, &set, nullptr, &g.set_layout), "vkCreateDescriptorSetLayout");
    VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout.setLayoutCount = 1;
    layout.pSetLayouts = &g.set_layout;
    vk::check(vkCreatePipelineLayout(g.context.device, &layout, nullptr, &g.pipeline_layout), "vkCreatePipelineLayout");
}

bool initialize(const GfxConfig& cfg)
{
    const int width = cfg.width > 0 ? cfg.width : 960;
    const int height = cfg.height > 0 ? cfg.height : 540;
    g.cfg.width = width;
    g.cfg.height = height;

    if (cfg.visible)
    {
#if defined(_WIN32)
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = gfx_wndproc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"GbtGDecompVulkan";
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(IDI_GBTG_APP));
        wc.hIconSm = wc.hIcon;
        RegisterClassExW(&wc);
        const Win32WindowPlacement placement = win32_window_placement(cfg.fullscreen, width, height);
        g.window = CreateWindowW(wc.lpszClassName, L"Grabbed by the Ghoulies", placement.style, placement.x, placement.y,
                               placement.width, placement.height, nullptr, nullptr, wc.hInstance, nullptr);
        if (!g.window)
        {
            log::error("vulkan: CreateWindow failed");
            return false;
        }
#else
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        {
            log::error("vulkan: SDL_InitSubSystem: %s", SDL_GetError());
            return false;
        }
        // SDL3 fullscreen is borderless at the desktop mode, which is what Gamescope expects on the Steam Deck.
        g.window = SDL_CreateWindow("Grabbed by the Ghoulies", width, height,
                                    SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN
                                        | (cfg.fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
        if (!g.window)
        {
            log::error("vulkan: SDL_CreateWindow: %s", SDL_GetError());
            return false;
        }
        int pixel_width = width;
        int pixel_height = height;
        SDL_GetWindowSizeInPixels(g.window, &pixel_width, &pixel_height);
        g.swapchain.window_extent = {u32(pixel_width), u32(pixel_height)};
#endif
    }

    vk::context_create(g.context, g.window);
    if (g.window != nullptr)
    {
        vk::swapchain_create(g.context, g.swapchain, cfg.vsync);
    }
    VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate.commandPool = g.context.command_pool;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = 1;
    vk::check(vkAllocateCommandBuffers(g.context.device, &allocate, &g.command), "vkAllocateCommandBuffers");
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vk::check(vkCreateFence(g.context.device, &fence, nullptr, &g.fence), "vkCreateFence");
    g.ring = vk::buffer_create(g.context, kRingBytes,
                               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                   | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    g.zero_uniform = vk::buffer_create(g.context, kBonesBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    std::memset(g.zero_uniform.mapped, 0, size_t(kBonesBytes));
    create_layout();
    g.wrap_sampler = make_sampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    g.clamp_sampler = make_sampler(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                   VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    g.zero_texture = create_texture(1, 1);
    const u8 zero[4]{};
    upload_texture(g.zero_texture, zero, true);
    create_surface(g.frame_color, g.frame_depth, width, height);
    submit();

    const std::string hlsl = vk::vulkan_hlsl(gfx_hlsl_common());
    const std::string shared_hlsl = hlsl + gfx_hlsl_alpha_test();
    g.model_source = vk::vulkan_hlsl(shared_hlsl + gfx_hlsl_combiners());
    g.vertex_source = hlsl;
    const std::string native_source = g.model_source + gfx_hlsl_native();
    g.native_vs = make_shader(native_source, "native_vs", "vs_6_0");
    g.native_ps = make_shader(native_source, "native_ps", "ps_6_0");
    g.model_ps = make_shader(g.model_source, "ps_model", "ps_6_0");
    g.sprite_vs = make_shader(hlsl, "vs_main", "vs_6_0");
    g.sprite_ps = make_shader(hlsl, "ps_main", "ps_6_0");
    if (g.native_vs.module == VK_NULL_HANDLE || g.native_ps.module == VK_NULL_HANDLE || g.model_ps.module == VK_NULL_HANDLE
        || g.sprite_vs.module == VK_NULL_HANDLE || g.sprite_ps.module == VK_NULL_HANDLE)
    {
        log::error("vulkan: pipeline incomplete");
        return false;
    }
    g.viewport = full_viewport();
    if (g.window)
    {
#if defined(_WIN32)
        ShowWindow(g.window, SW_SHOW);
#else
        SDL_ShowWindow(g.window);
#endif
    }
    g.ok = true;
    return true;
}

} // namespace

bool gfx_init(const GfxConfig& cfg)
{
    g.ok = false;
    g.cfg = cfg;
    try
    {
        if (initialize(cfg))
        {
            return true;
        }
    }
    catch (const std::exception& error)
    {
        log::error("%s", error.what());
    }
    gfx_shutdown();
    return false;
}

void gfx_shutdown()
{
    release_all();
    if (g.window)
    {
#if defined(_WIN32)
        DestroyWindow(g.window);
#else
        SDL_DestroyWindow(g.window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
#endif
    }
    g = GfxVulkan{};
}

const char* gfx_backend_name()
{
    return "vulkan";
}

bool gfx_has_window()
{
    return g.window != nullptr;
}

bool gfx_overlay_device(GfxOverlayDevice& device)
{
    if (!g.ok || g.window == nullptr)
    {
        return false;
    }
    device = {};
    device.backend = "vulkan";
    device.window = g.window;
    device.vk_api_version = VK_API_VERSION_1_3;
    device.vk_instance = g.context.instance;
    device.vk_physical_device = g.context.physical;
    device.vk_device = g.context.device;
    device.vk_queue_family = g.context.queue_family;
    device.vk_queue = g.context.queue;
    // The swapchain images the overlay draws onto (see present).
    device.vk_color_format = VK_FORMAT_B8G8R8A8_UNORM;
    device.vk_image_count = u32(std::max<size_t>(2, g.swapchain.images.size()));
    return true;
}

void gfx_pump(bool& quit)
{
    quit = false;
    if (!g.window)
    {
        return;
    }
#if defined(_WIN32)
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
#else
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        handle_window_event(event);
    }
#endif
    quit = g.quit;
}

void gfx_begin_frame()
{
    if (!g.ok)
    {
        return;
    }
    if (render_profile::enabled)
    {
        static bool logged = false;
        if (!logged)
        {
            log::info("profile gpu timings are not recorded by the Vulkan backend");
            logged = true;
        }
    }
    g.frame_finished = false;
#if GBTG_OPTIMIZE
    age_retained_uploads();
#endif
    begin_rendering();
    clear_attachments(whole_target(), true, {0.f, 0.f, 0.f, 1.f}, VK_IMAGE_ASPECT_DEPTH_BIT, 1.f);
    g.viewport = full_viewport();
}

// Host boundary for Xbox D3DDevice_Clear(1, rect, 3, ..., 1.f, 0): depth 1 and stencil 0 over a 640x480 UI
// rectangle in the HUD viewport. The rectangle covers the pixels whose centres D3D11's clear quad covers.
void gfx_clear_ui_depth(int left, int top, int right, int bottom)
{
    if (!g.ok || right <= left || bottom <= top)
    {
        return;
    }
    const Viewport vp = hud_viewport();
    const auto edge = [](float position)
    {
        return i32(std::ceil(position - 0.5f));
    };
    const VkRect2D rect = clamp_rect(edge(vp.x + vp.width * float(left) / 640.f), edge(vp.y + vp.height * float(top) / 480.f),
                                     edge(vp.x + vp.width * float(right) / 640.f),
                                     edge(vp.y + vp.height * float(bottom) / 480.f));
    clear_attachments(rect, false, {}, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 1.f);
}

#if GBTG_OPTIMIZE
// GBTG_OPTIMIZE step 3 (Vulkan): native_programs.find through program_memo.
GfxVulkan::ProgramGpu* find_native_program(std::span<const u32> program)
{
    const auto memo = g.program_memo.find(program.data());
    // Step 23: within a native batch the caller issues draws only, so words compared once in it stand.
    if (memo != g.program_memo.end() && memo->second.words.size() == program.size()
        && ((g.native_batch && memo->second.batch == g.native_batch_serial)
            || std::memcmp(memo->second.words.data(), program.data(), program.size_bytes()) == 0))
    {
        memo->second.batch = g.native_batch_serial;
        return memo->second.program;
    }
    const auto found = g.native_programs.find(program);
    if (found == g.native_programs.end())
    {
        return nullptr;
    }
    if (g.program_memo.size() >= 4096)
    {
        g.program_memo.clear();
    }
    g.program_memo[program.data()] = {std::vector<u32>(program.begin(), program.end()), &found->second,
                                      g.native_batch_serial};
    return &found->second;
}
#endif

bool gfx_native_vertex_program_supported(std::span<const u32> program)
{
    if (!g.ok || render_profile::reference || render_profile::cpu_vertex)
    {
        return false;
    }
#if GBTG_OPTIMIZE
    if (const GfxVulkan::ProgramGpu* known = find_native_program(program))
    {
        return known->shader.module != VK_NULL_HANDLE;
    }
#else
    auto found = g.native_programs.find(program);
    if (found != g.native_programs.end())
    {
        return found->second.shader.module != VK_NULL_HANDLE;
    }
#endif
    render_profile::Scope compilation(render_profile::Compilation);
    GfxVulkan::ProgramGpu gpu;
#if GBTG_OPTIMIZE
    program_constant_range(program, gpu.constants_first, gpu.constants_count);
#endif
    const auto body = xbox_vertex_hlsl(program);
    if (render_profile::enabled && body.empty())
    {
        std::string line = log::format("profile vertex_fallback instructions=%u mac/ilu/relative:",
                                       program.empty() ? 0u : program[0] >> 16);
        for (size_t n = 1; n + 3 < program.size(); n += 4)
        {
            line += log::format(" %u/%u/%u", (program[n + 1] >> 21) & 15, (program[n + 1] >> 25) & 7, (program[n + 3] >> 1) & 1);
        }
        log::info("%s", line.c_str());
    }
    if (!body.empty())
    {
        gpu.shader = make_shader(g.vertex_source + body, "program_vs", "vs_6_0", true);
    }
    const bool supported = gpu.shader.module != VK_NULL_HANDLE;
    g.native_programs.emplace(std::vector<u32>(program.begin(), program.end()), std::move(gpu));
    return supported;
}

bool gfx_prepare_native_pixel_definition(const u32* definition)
{
    if (!g.ok)
    {
        return false;
    }
    return native_pixel_shader(definition) != &g.model_ps;
}

void gfx_compiled_shader_permutations(std::vector<std::vector<u32>>& vertex_programs,
                                      std::vector<std::array<u32, 60>>& pixel_definitions)
{
    if (!g.ok)
    {
        return;
    }
    for (const auto& [program, gpu] : g.native_programs)
    {
        if (gpu.shader.module != VK_NULL_HANDLE)
        {
            vertex_programs.push_back(program);
        }
    }
    for (const auto& [definition, shader] : g.specialized_pixels)
    {
        (void)shader;
        pixel_definitions.push_back(definition);
    }
}

void gfx_begin_native_batch()
{
    g.native_batch = !render_profile::reference;
#if GBTG_OPTIMIZE
    ++g.native_batch_serial;
    g.native_last.images = {};
    g.native_last.samplers_valid = false;
#endif
}

void gfx_end_native_batch()
{
    g.native_batch = false;
}

void gfx_draw_native(const GfxNativeDraw& draw)
{
    render_profile::Scope measurement(render_profile::Submission);
    if (render_profile::enabled)
    {
        ++render_profile::frame.draws;
        ++render_profile::frame.all_draws;
        render_profile::frame.output_vertices += draw.indices.empty() ? draw.vertices.size() : draw.indices.size();
        render_profile::frame.uploaded_bytes += draw.vertices.size_bytes() + draw.program_inputs.size_bytes() + draw.indices.size_bytes();
    }
    if (!g.ok || (draw.vertices.empty() && draw.program_inputs.empty()))
    {
        return;
    }
    const bool program_draw = !draw.program_inputs.empty();
    if (program_draw && render_profile::validate_vertex)
    {
        throw Error("vulkan: --validate-gpu-vertices needs stream output, which the Vulkan backend does not implement");
    }
    present_hud_viewport();

    // Texture uploads record copies outside the draw's rendering, so they come first.
    DrawBindings bindings;
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 23: the previous draw's view of the same image at the same serial, and its sampler for the
    // same address modes, are the ones native_texture and native_sampler would find.
    auto& last = g.native_last;
    for (size_t stage = 0; stage < 4; ++stage)
    {
        const RgbaImage* image = draw.textures[stage];
        if (g.native_batch && image != nullptr && image == last.images[stage] && image->serial == last.image_serials[stage]
            && !image->bgra.empty() && image->width > 0 && image->height > 0)
        {
            bindings.views[stage] = last.views[stage];
        }
        else
        {
            bindings.views[stage] = native_texture(image);
            last.images[stage] = bindings.views[stage] != VK_NULL_HANDLE ? image : nullptr;
            last.image_serials[stage] = image != nullptr ? image->serial : 0;
            last.views[stage] = bindings.views[stage];
        }
        const u32 sampler_key = draw.address_u[stage] | (draw.address_v[stage] << 8);
        if (!g.native_batch || !last.samplers_valid || last.sampler_keys[stage] != sampler_key)
        {
            last.samplers[stage] = native_sampler(draw.address_u[stage], draw.address_v[stage]);
            last.sampler_keys[stage] = sampler_key;
        }
        bindings.samplers[stage] = last.samplers[stage];
    }
    last.samplers_valid = g.native_batch;
#else
    for (size_t stage = 0; stage < 4; ++stage)
    {
        bindings.views[stage] = native_texture(draw.textures[stage]);
        bindings.samplers[stage] = native_sampler(draw.address_u[stage], draw.address_v[stage]);
    }
#endif

    GpuCB cb;
    cb.mvp[0] = cb.mvp[5] = cb.mvp[10] = cb.mvp[15] = 1.f;
    cb.alpha_test = draw.alpha_test ? 1.f : 0.f;
    cb.alpha_ref = float(draw.alpha_reference & 0xFF);
    cb.alpha_func = float(draw.alpha_function);
    cb.alpha_pad = draw.textures[0] != nullptr ? 1.f : 0.f;
    if (draw.pixel_shader == nullptr && draw.textures[0] != nullptr && draw.textures[0]->linear
        && draw.textures[0]->width > 0 && draw.textures[0]->height > 0)
    {
        cb.native_uv_scale = {1.f / float(draw.textures[0]->linear_width()), 1.f / float(draw.textures[0]->linear_height()), 0.f, 0.f};
    }
    std::array<u32, 80> words{};
    const Shader* pixel = &g.native_ps;
    if (draw.pixel_shader != nullptr)
    {
        // Combiner words, then stage n samples texture coordinate n, and v0/v1 are the vertex colours.
        std::memcpy(words.data(), draw.pixel_shader, 60 * 4);
        words[76] = draw.fog ? 1u : 0u; // ps_fog
        words[77] = draw.fog_color;
        for (size_t stage = 0; stage < 4; ++stage)
        {
            const RgbaImage* texture = draw.textures[stage];
            if (texture != nullptr && texture->linear && texture->width > 0 && texture->height > 0)
            {
                const float scale[2] = {1.f / float(texture->linear_width()), 1.f / float(texture->linear_height())};
                std::memcpy(&words[68 + stage * 2], scale, sizeof(scale));
            }
        }
        words[60] = 0;
        words[61] = 1;
        words[62] = 2;
        words[63] = 3;
        words[64] = 0;
        words[65] = 1;
        pixel = native_pixel_shader(draw.pixel_shader);
    }
    const Shader* vertex = &g.native_vs;
#if GBTG_OPTIMIZE
    const GfxVulkan::ProgramGpu* program_gpu = nullptr;
#endif
    if (program_draw)
    {
        if (draw.vertex_constants == nullptr)
        {
            throw Error("native vertex constants map failed");
        }
#if GBTG_OPTIMIZE
        program_gpu = find_native_program(draw.vertex_program);
        vertex = &program_gpu->shader;
#else
        vertex = &g.native_programs.find(draw.vertex_program)->second.shader;
#endif
    }

    const VkDeviceSize vertex_bytes = program_draw ? draw.program_inputs.size_bytes() : draw.vertices.size_bytes();
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 7: retained geometry draws from its own buffer; the ring keeps the rest.
    GfxVulkan::RetainedUpload* retained = program_draw && draw.retained_geometry != 0 && !draw.indices.empty()
        ? &retained_upload(draw) : nullptr;
    ring_reserve((retained != nullptr ? 0 : vertex_bytes + draw.indices.size_bytes()) + sizeof(cb) + sizeof(words)
                 + kConstantsBytes);
#else
    ring_reserve(vertex_bytes + draw.indices.size_bytes() + sizeof(cb) + sizeof(words) + kConstantsBytes);
#endif
    const VkDeviceSize alignment = uniform_alignment();
    bindings.camera = ring_take(&cb, sizeof(cb), alignment);
    if (draw.pixel_shader != nullptr)
    {
        bindings.material = ring_take(words.data(), sizeof(words), alignment);
    }
    if (program_draw)
    {
#if GBTG_OPTIMIZE
        // Step 23: the slot copied for the same constants id, while the ring still holds it, has this draw's values
        // for every register the slot was copied for.
        const u32 first = program_gpu->constants_first;
        const u32 count = program_gpu->constants_count;
        auto& last_slot = g.native_last;
        if (draw.vertex_constants_id != 0 && draw.vertex_constants_id == last_slot.constants_id
            && last_slot.constants_epoch == g.ring_epoch && first >= last_slot.constants_first
            && first + count <= last_slot.constants_first + last_slot.constants_count)
        {
            bindings.constants = last_slot.constants_offset;
        }
        else
        {
            bindings.constants = ring_take_range(draw.vertex_constants->data(), kConstantsBytes, alignment,
                                                 VkDeviceSize(first) * 16, VkDeviceSize(count) * 16);
            last_slot.constants_id = draw.vertex_constants_id;
            last_slot.constants_epoch = g.ring_epoch;
            last_slot.constants_offset = bindings.constants;
            last_slot.constants_first = first;
            last_slot.constants_count = count;
        }
#else
        bindings.constants = ring_take(draw.vertex_constants->data(), kConstantsBytes, alignment);
#endif
    }
#if GBTG_OPTIMIZE
    VkBuffer vertex_buffer = g.ring.buffer;
    VkDeviceSize vertex_offset = 0;
    VkDeviceSize index_offset = 0;
    if (retained != nullptr)
    {
        vertex_buffer = retained->buffer.buffer;
        index_offset = retained->index_offset;
    }
    else
    {
        vertex_offset = ring_take(program_draw ? static_cast<const void*>(draw.program_inputs.data())
                                               : static_cast<const void*>(draw.vertices.data()),
                                  vertex_bytes, 16);
        index_offset = draw.indices.empty() ? 0 : ring_take(draw.indices.data(), draw.indices.size_bytes(), 4);
    }
#else
    const VkDeviceSize vertex_offset = ring_take(program_draw ? static_cast<const void*>(draw.program_inputs.data())
                                                              : static_cast<const void*>(draw.vertices.data()),
                                                 vertex_bytes, 16);
    const VkDeviceSize index_offset = draw.indices.empty() ? 0 : ring_take(draw.indices.data(), draw.indices.size_bytes(), 4);
#endif

    PipelineState state;
    state.vertex = vertex;
    state.pixel = pixel;
    state.layout = program_draw ? VertexLayout::Program : VertexLayout::Native;
    state.lines = draw.lines;
    state.cull = native_cull(draw.cull);
    state.blend = draw.blend;
    state.source_color = native_blend_factor(draw.source_blend, false);
    state.destination_color = native_blend_factor(draw.destination_blend, false);
    state.source_alpha = native_blend_factor(draw.source_blend, true);
    state.destination_alpha = native_blend_factor(draw.destination_blend, true);
    state.equation = native_blend_equation(draw.blend_equation);
    state.write_mask = ((draw.color_mask >> 16) & 1 ? VK_COLOR_COMPONENT_R_BIT : 0)
        | ((draw.color_mask >> 8) & 1 ? VK_COLOR_COMPONENT_G_BIT : 0) | (draw.color_mask & 1 ? VK_COLOR_COMPONENT_B_BIT : 0)
        | ((draw.color_mask >> 24) & 1 ? VK_COLOR_COMPONENT_A_BIT : 0);
    // A target with no depth surface neither tests nor writes depth and stencil.
    const bool depth_surface = !g.target_rendering || g.target_has_depth;
    state.depth_test = draw.depth_test && depth_surface;
    state.depth_write = draw.depth_write && depth_surface;
    state.depth_compare = native_comparison(draw.depth_function);
    state.z_bias = depth_surface ? draw.z_bias : 0;
    if (draw.stencil_test && depth_surface)
    {
        state.stencil = true;
        state.stencil_compare = native_comparison(draw.stencil_function);
        state.stencil_read_mask = draw.stencil_read_mask & 0xFF;
        state.stencil_write_mask = draw.stencil_write_mask & 0xFF;
        state.stencil_fail = native_stencil_op(draw.stencil_fail);
        state.stencil_depth_fail = native_stencil_op(draw.stencil_depth_fail);
        state.stencil_pass = native_stencil_op(draw.stencil_pass);
    }
    const VkPipeline draw_pipeline = pipeline(state);

    begin_rendering();
    bind_pipeline(draw_pipeline);
    set_viewport_and_scissor(native_rectangle(draw.scissor));
#if GBTG_OPTIMIZE
    if (!g.command_state.stencil_valid || g.command_state.stencil_reference != (draw.stencil_reference & 0xFF))
    {
        vkCmdSetStencilReference(g.command, VK_STENCIL_FACE_FRONT_AND_BACK, draw.stencil_reference & 0xFF);
        g.command_state.stencil_valid = true;
        g.command_state.stencil_reference = draw.stencil_reference & 0xFF;
    }
#else
    vkCmdSetStencilReference(g.command, VK_STENCIL_FACE_FRONT_AND_BACK, draw.stencil_reference & 0xFF);
#endif
    push_descriptors(bindings);
#if GBTG_OPTIMIZE
    vkCmdBindVertexBuffers(g.command, 0, 1, &vertex_buffer, &vertex_offset);
#else
    vkCmdBindVertexBuffers(g.command, 0, 1, &g.ring.buffer, &vertex_offset);
#endif
    if (draw.indices.empty())
    {
        vkCmdDraw(g.command, u32(draw.vertices.size()), 1, 0, 0);
    }
    else
    {
#if GBTG_OPTIMIZE
        vkCmdBindIndexBuffer(g.command, vertex_buffer, index_offset, VK_INDEX_TYPE_UINT32);
#else
        vkCmdBindIndexBuffer(g.command, g.ring.buffer, index_offset, VK_INDEX_TYPE_UINT32);
#endif
        vkCmdDrawIndexed(g.command, u32(draw.indices.size()), 1, 0, 0, 0);
    }
}

void gfx_begin_native_target(int width, int height, bool depth)
{
    if (!g.ok)
    {
        return;
    }
    if (g.target_rendering)
    {
        throw Error("native render target is already bound");
    }
    end_rendering();
    if (g.target_color.width != width || g.target_color.height != height)
    {
        create_surface(g.target_color, g.target_depth, width, height);
    }
    g.target_saved_viewport = g.viewport;
    g.target_rendering = true;
    g.target_has_depth = depth;
    // Xbox pixel coordinates map 1:1 onto the surface; the 640x480 space extends past its edges.
    g.viewport = Viewport{0.f, 0.f, 640.f, 480.f};
}

// The open texture target's depth-stencil as the Xbox D24S8 words its memory holds (depth << 8 | stencil).
void gfx_read_native_target_depth(std::vector<u32>& output)
{
    output.clear();
    if (!g.ok)
    {
        return;
    }
    if (!g.target_rendering)
    {
        throw Error("native render target is not bound");
    }
    const size_t count = size_t(g.target_depth.width) * size_t(g.target_depth.height);
    const VkDeviceSize stencil_offset = (VkDeviceSize(count) * 4 + 15) / 16 * 16;
    vk::Buffer& buffer = readback_buffer(stencil_offset + count);
    ensure_recording();
    end_rendering();
    VkBufferImageCopy copies[2]{};
    copies[0].imageSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
    copies[0].imageExtent = {u32(g.target_depth.width), u32(g.target_depth.height), 1};
    copies[1] = copies[0];
    copies[1].bufferOffset = stencil_offset;
    copies[1].imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    vkCmdCopyImageToBuffer(g.command, g.target_depth.image, VK_IMAGE_LAYOUT_GENERAL, buffer.buffer, 2, copies);
    vk::memory_barrier(g.command);
    submit();
    output.resize(count);
    const bool d24 = g.context.depth_format == VK_FORMAT_D24_UNORM_S8_UINT;
    for (size_t n = 0; n < count; ++n)
    {
        u32 value;
        std::memcpy(&value, buffer.mapped + n * 4, 4);
        u32 depth = value & 0xFFFFFFu;
        if (!d24)
        {
            float f;
            std::memcpy(&f, &value, 4);
            depth = u32(std::lround(double(std::clamp(f, 0.f, 1.f)) * 16777215.0));
        }
        output[n] = (depth << 8) | buffer.mapped[stencil_offset + n];
    }
}

void gfx_end_native_target(RgbaImage& output)
{
    if (!g.ok)
    {
        return;
    }
    if (!g.target_rendering)
    {
        throw Error("native render target is not bound");
    }
    const VkDeviceSize bytes = VkDeviceSize(g.target_color.width) * VkDeviceSize(g.target_color.height) * 4;
    vk::Buffer& buffer = readback_buffer(bytes);
    ensure_recording();
    end_rendering();
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {u32(g.target_color.width), u32(g.target_color.height), 1};
    vkCmdCopyImageToBuffer(g.command, g.target_color.image, VK_IMAGE_LAYOUT_GENERAL, buffer.buffer, 1, &copy);
    vk::memory_barrier(g.command);
    submit();
    output.width = g.target_color.width;
    output.height = g.target_color.height;
    output.bgra.assign(buffer.mapped, buffer.mapped + bytes);
    g.target_rendering = false;
    g.viewport = g.target_saved_viewport;
}

#if GBTG_OPTIMIZE
// GBTG_OPTIMIZE steps 23 and 25: a width x height region of `source` (in VK_IMAGE_LAYOUT_GENERAL) at (left, top)
// recorded as a copy into the texture native draws bind for `image`, which then stands at image.serial. The texture is
// kept when it already has the image's size. Recording must be open, outside a render pass.
void copy_into_image_texture(const RgbaImage& image, VkImage source, int left, int top, int width, int height)
{
    auto it = g.textures.find(&image);
    bool fresh = false;
    if (it == g.textures.end() || it->second.image.width != image.width || it->second.image.height != image.height)
    {
        if (it != g.textures.end())
        {
            retire(it->second.image);
        }
        Texture texture;
        texture.image = create_texture(image.width, image.height);
        it = g.textures.insert_or_assign(&image, texture).first;
        fresh = true;
    }
    vk::Image& texture = it->second.image;
    vk::image_barrier(g.command, texture.image, VK_IMAGE_ASPECT_COLOR_BIT,
                      fresh ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkImageCopy region{};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.srcOffset = {left, top, 0};
    region.dstSubresource = region.srcSubresource;
    region.extent = {u32(width), u32(height), 1};
    vkCmdCopyImage(g.command, source, VK_IMAGE_LAYOUT_GENERAL, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                   &region);
    vk::image_barrier(g.command, texture.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    it->second.serial = image.serial;
    g.native_last.images = {}; // The image's texture may be new.
}

// GBTG_OPTIMIZE step 23: step 12 on Vulkan. The target is copied on the GPU into the texture native draws bind for
// `image`, which then stands at image.serial, and into a readback buffer, with no submission; the copy is ordered
// after the target's draws and before the texture's next use by the backend's full barriers.
bool gfx_end_native_target_into(const RgbaImage& image)
{
    if (!g.ok)
    {
        return false;
    }
    if (!g.target_rendering)
    {
        throw Error("native render target is not bound");
    }
    if (image.width != g.target_color.width || image.height != g.target_color.height)
    {
        return false;
    }
    const VkDeviceSize bytes = VkDeviceSize(image.width) * VkDeviceSize(image.height) * 4;
    if (g.ended_readback.size < bytes)
    {
        // A recorded copy may still name the old buffer.
        submit();
        vk::buffer_destroy(g.context, g.ended_readback);
        g.ended_readback = vk::buffer_create(g.context, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
    }
    ensure_recording();
    end_rendering();
    copy_into_image_texture(image, g.target_color.image, 0, 0, image.width, image.height);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {u32(image.width), u32(image.height), 1};
    vkCmdCopyImageToBuffer(g.command, g.target_color.image, VK_IMAGE_LAYOUT_GENERAL, g.ended_readback.buffer, 1, &copy);
    vk::memory_barrier(g.command);
    g.ended_pending = true;
    g.ended_width = image.width;
    g.ended_height = image.height;
    g.target_rendering = false;
    g.viewport = g.target_saved_viewport;
    return true;
}

// The pixels of the target gfx_end_native_target_into last ended: the recorded work is submitted and waited for.
void gfx_read_ended_native_target(RgbaImage& output)
{
    if (!g.ended_pending)
    {
        throw Error("no native target ended on the GPU");
    }
    submit();
    const VkDeviceSize bytes = VkDeviceSize(g.ended_width) * VkDeviceSize(g.ended_height) * 4;
    output.width = g.ended_width;
    output.height = g.ended_height;
    output.bgra.assign(g.ended_readback.mapped, g.ended_readback.mapped + bytes);
    g.ended_pending = false;
}
#else
// GBTG_OPTIMIZE step 12 is D3D11 only: targets end through gfx_end_native_target here.
bool gfx_end_native_target_into(const RgbaImage&)
{
    return false;
}

void gfx_read_ended_native_target(RgbaImage&)
{
    throw Error("the Vulkan backend ends no native target on the GPU");
}
#endif

void gfx_begin_native_scene_target(u32 key, int width, int height)
{
    if (!g.ok)
    {
        return;
    }
    if (g.target_rendering || g.scene_target_rendering)
    {
        throw Error("native render target is already bound");
    }
    end_rendering();
    vk::Image& surface = g.scene_target_colors[key];
    if (surface.width != g.frame_color.width || surface.height != g.frame_color.height)
    {
        retire(surface);
        surface = vk::image_create(g.context, g.frame_color.width, g.frame_color.height,
            VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT);
        ensure_recording();
        vk::image_barrier(g.command, surface.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL);
    }
    g.scene_target_open = &surface;
    g.scene_target_width = width;
    g.scene_target_height = height;
    g.scene_target_saved_viewport = g.viewport;
    g.scene_target_rendering = true;
    present_hud_viewport();
}

void gfx_end_native_scene_target(RgbaImage& output)
{
    if (!g.ok)
    {
        return;
    }
    if (!g.scene_target_rendering)
    {
        throw Error("native scene target is not bound");
    }
    const vk::Image& color = *g.scene_target_open;
    const VkDeviceSize bytes = VkDeviceSize(color.width) * VkDeviceSize(color.height) * 4;
    vk::Buffer& buffer = readback_buffer(bytes);
    ensure_recording();
    end_rendering();
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {u32(color.width), u32(color.height), 1};
    vkCmdCopyImageToBuffer(g.command, color.image, VK_IMAGE_LAYOUT_GENERAL, buffer.buffer, 1, &copy);
    vk::memory_barrier(g.command);
    submit();
    // The fitted 640x480 area the frame is drawn into, at the window's resolution.
    const Viewport area = hud_viewport();
    const int left = std::clamp(int(area.x + 0.5f), 0, color.width - 1);
    const int top = std::clamp(int(area.y + 0.5f), 0, color.height - 1);
    output.width = std::clamp(int(area.width + 0.5f), 1, color.width - left);
    output.height = std::clamp(int(area.height + 0.5f), 1, color.height - top);
    output.bgra.resize(size_t(output.width) * size_t(output.height) * 4);
    for (int y = 0; y < output.height; ++y)
    {
        std::memcpy(output.bgra.data() + size_t(y) * size_t(output.width) * 4,
            buffer.mapped + (size_t(top + y) * size_t(color.width) + size_t(left)) * 4, size_t(output.width) * 4);
    }
    g.scene_target_rendering = false;
    g.scene_target_open = nullptr;
    g.viewport = g.scene_target_saved_viewport;
}

// D3DDevice_Clear over the 640x480 back buffer: flags 1 depth, 2 stencil, 0xF0 colour.
void gfx_clear_native(std::span<const std::array<i32, 4>> rectangles, u32 flags, u32 color, float depth)
{
    if (!g.ok)
    {
        return;
    }
    present_hud_viewport();
    const bool depth_surface = !g.target_rendering || g.target_has_depth; // No depth surface, nothing to clear.
    const VkImageAspectFlags aspects = !depth_surface ? 0
        : ((flags & 1) != 0 ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) | ((flags & 2) != 0 ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
    // A single rectangle over the whole 640x480 back buffer (the full view's fitted rectangle) clears as a whole.
    const bool whole = rectangles.size() == 1 && rectangles[0][0] == 0 && rectangles[0][1] == 0
        && rectangles[0][2] == 640 && rectangles[0][3] == 480;
    if (rectangles.empty() || whole)
    {
        clear_attachments(whole_target(), (flags & 0xF0) != 0, argb_color(color), aspects, depth);
        return;
    }
    if ((flags & 0xF0) != 0)
    {
        // A texture target is in Xbox pixels (see present_hud_viewport); on the back buffer each rectangle maps
        // through the HUD viewport onto the window (a widescreen view's fitted 4:3 area, for one).
        if ((flags & 0xF0) != 0xF0)
        {
            throw Error("native clear of a colour rectangle supports a whole colour only");
        }
        for (const auto& rectangle : rectangles)
        {
            const VkRect2D area = g.target_rendering ? clamp_rect(rectangle[0], rectangle[1], rectangle[2], rectangle[3])
                                                     : native_rectangle(rectangle);
            clear_attachments(area, true, argb_color(color), 0, depth);
        }
    }
    if ((flags & 3) == 0)
    {
        return;
    }
    if ((flags & 3) != 3 || depth != 1.f)
    {
        throw Error("native clear of a rectangle supports depth and stencil to 1 only");
    }
    for (const auto& rectangle : rectangles)
    {
        gfx_clear_ui_depth(rectangle[0], rectangle[1], rectangle[2], rectangle[3]);
    }
}

void gfx_size(int& width, int& height)
{
    width = g.cfg.width;
    height = g.cfg.height;
}

void gfx_release_sprite(const RgbaImage& image)
{
#if GBTG_OPTIMIZE
    g.native_last.images = {}; // Step 23: never answer for an image released and allocated again.
#endif
    auto found = g.textures.find(&image);
    if (found != g.textures.end())
    {
        retire(found->second.image);
        g.textures.erase(found);
    }
}

void gfx_draw_sprite(const RgbaImage& image, const GfxRect& rect)
{
    render_profile::Scope measurement(render_profile::HostRendering);
    if (!g.ok || image.bgra.empty() || image.width <= 0 || image.height <= 0 || rect.w == 0.f || rect.h == 0.f)
    {
        return;
    }
    const VkImageView view = native_texture(&image);
    if (view == VK_NULL_HANDLE)
    {
        return;
    }
    draw_sprite_quad(view, rect);
}

void gfx_show_startup_loading_screen(const RgbaImage& text, float progress)
{
    if (!g.ok || g.window == nullptr || g.cfg.width <= 0 || g.cfg.height <= 0 || text.width <= 0 || text.height <= 0)
    {
        return;
    }
    begin_rendering();
    clear_attachments(whole_target(), true, {0.f, 0.f, 0.f, 1.f}, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 1.f);
    g.viewport = full_viewport();

    const float text_height_pixels = 42.f * float(g.cfg.height) / 540.f;
    const float text_width_pixels = text_height_pixels * float(text.width) / float(text.height);
    GfxRect rectangle;
    rectangle.x = (float(g.cfg.width) - text_width_pixels) / (2.f * float(g.cfg.width));
    rectangle.y = (float(g.cfg.height) - text_height_pixels) / (2.f * float(g.cfg.height));
    rectangle.w = text_width_pixels / float(g.cfg.width);
    rectangle.h = text_height_pixels / float(g.cfg.height);
    vulkan::gfx_draw_sprite(text, rectangle);
    vulkan::gfx_release_sprite(text);

    constexpr float bar_width_pixels = 280.f;
    constexpr float bar_height_pixels = 8.f;
    constexpr float bar_gap_pixels = 18.f;
    const float bar_width = bar_width_pixels * float(g.cfg.height) / 540.f;
    const float bar_height = bar_height_pixels * float(g.cfg.height) / 540.f;
    const float bar_y = rectangle.y + rectangle.h + bar_gap_pixels / 540.f;
    RgbaImage pixel;
    pixel.width = 1;
    pixel.height = 1;
    pixel.bgra = {255, 255, 255, 255};
    std::array<u64, 3> color_total{};
    u64 color_samples = 0;
    for (size_t offset = 0; offset + 3 < text.bgra.size(); offset += 4)
    {
        if (text.bgra[offset + 3] == 0)
        {
            continue;
        }
        color_total[0] += text.bgra[offset + 2];
        color_total[1] += text.bgra[offset + 1];
        color_total[2] += text.bgra[offset + 0];
        ++color_samples;
    }
    const std::array<float, 4> font_color = color_samples == 0
        ? std::array<float, 4>{0.f, 1.f, 0.f, 1.f}
        : std::array<float, 4>{float(color_total[0]) / float(color_samples) / 255.f,
              float(color_total[1]) / float(color_samples) / 255.f,
              float(color_total[2]) / float(color_samples) / 255.f, 1.f};
    GfxRect bar;
    bar.x = (float(g.cfg.width) - bar_width) / (2.f * float(g.cfg.width));
    bar.y = bar_y;
    bar.w = bar_width / float(g.cfg.width);
    bar.h = bar_height / float(g.cfg.height);
    bar.color = {0.22f, 0.22f, 0.22f, 1.f};
    vulkan::gfx_draw_sprite(pixel, bar);
    bar.x += 1.f / float(g.cfg.width);
    bar.y += 1.f / float(g.cfg.height);
    bar.w = std::max(0.f, bar_width - 2.f) / float(g.cfg.width);
    bar.h = std::max(0.f, bar_height - 2.f) / float(g.cfg.height);
    bar.w *= std::clamp(progress, 0.f, 1.f);
    bar.color = font_color;
    vulkan::gfx_draw_sprite(pixel, bar);
    vulkan::gfx_release_sprite(pixel);
    present_swap();
}

void gfx_sprite_clear()
{
    hud_quads.clear();
}

void gfx_sprite_submit(const RgbaImage& image, float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                       std::array<float, 4> color)
{
    if (image.bgra.empty() || image.width <= 0 || image.height <= 0 || w == 0.f || h == 0.f)
    {
        return;
    }
    // 0x000662A0. The old pool pointer is the argument, so the live quads stay.
    if (hud_quads.size() == hud_quads.capacity())
    {
        hud_quads.reserve(hud_quads.size() + hud_extra);
    }
    HudQuad quad;
    quad.image = &image;
    quad.x = x;
    quad.y = y;
    quad.w = w;
    quad.h = h;
    quad.u0 = u0;
    quad.v0 = v0;
    quad.u1 = u1;
    quad.v1 = v1;
    quad.color = color;
    hud_quads.push_back(quad);
}

void gfx_end_frame()
{
    // Present 0x00033480, then 0x00033A90 (clear and swap). frame_tick runs 0x00032AB0, the app error screen.
    gfx_present_tick_insert();
    present_draw_frame();
}

void gfx_present_again()
{
    if (!g.ok || g.window == nullptr || !g.frame_finished)
    {
        return;
    }
    present_frame_image();
}

bool gfx_readback(RgbaImage& out)
{
    if (!g.ok)
    {
        return false;
    }
    const VkDeviceSize bytes = VkDeviceSize(g.frame_color.width) * VkDeviceSize(g.frame_color.height) * 4;
    vk::Buffer& buffer = readback_buffer(bytes);
    ensure_recording();
    end_rendering();
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {u32(g.frame_color.width), u32(g.frame_color.height), 1};
    vkCmdCopyImageToBuffer(g.command, g.frame_color.image, VK_IMAGE_LAYOUT_GENERAL, buffer.buffer, 1, &copy);
    vk::memory_barrier(g.command);
    submit();
    out.width = g.frame_color.width;
    out.height = g.frame_color.height;
    out.bgra.assign(buffer.mapped, buffer.mapped + bytes);
    return true;
}

bool gfx_readback_ui(RgbaImage& out)
{
    present_hud_sprites();
    gfx_sprite_clear();
    RgbaImage frame;
    if (!vulkan::gfx_readback(frame))
    {
        return false;
    }
    // The presented frame's area (gfx_ui_frame).
    const GfxUiFrame area = gfx_ui_frame(frame.width, frame.height, g.cfg.widescreen, g.cfg.frame_aspect);
    out.width = area.width;
    out.height = area.height;
    out.bgra.resize(size_t(area.width) * area.height * 4);
    for (int row = 0; row < area.height; ++row)
    {
        std::memcpy(out.bgra.data() + size_t(row) * area.width * 4,
            frame.bgra.data() + (size_t(row + area.top) * frame.width + area.left) * 4,
            size_t(area.width) * 4);
    }
    return true;
}

void gfx_ui_frame_size(int& width, int& height)
{
    width = 0;
    height = 0;
    if (!g.ok)
    {
        return;
    }
    const GfxUiFrame area = gfx_ui_frame(g.cfg.width, g.cfg.height, g.cfg.widescreen, g.cfg.frame_aspect);
    width = area.width;
    height = area.height;
}

// GBTG_OPTIMIZE step 25: gfx_readback_ui's picture copied on the GPU into the texture native draws bind for `image`,
// with no submission or readback; the backend's full barriers order it after the frame's draws so far.
bool gfx_copy_ui_into(const RgbaImage& image)
{
#if GBTG_OPTIMIZE
    if (!g.ok || image.width <= 0 || image.height <= 0)
    {
        return false;
    }
    present_hud_sprites();
    gfx_sprite_clear();
    const GfxUiFrame area = gfx_ui_frame(g.frame_color.width, g.frame_color.height, g.cfg.widescreen,
        g.cfg.frame_aspect);
    ensure_recording();
    end_rendering();
    // The image was sized by gfx_ui_frame_size; a resize since then copies the part both hold.
    copy_into_image_texture(image, g.frame_color.image, area.left, area.top, std::min(area.width, image.width),
        std::min(area.height, image.height));
    return true;
#else
    (void)image;
    return false;
#endif
}

// The D3D11 backend's boundary check on this backend: vertex/index mutation, texture serial and release,
// vertex constants, and a device unload/reload. GPU-versus-CPU vertex validation needs stream output and is left out.
void gfx_validate_native_boundary()
{
    const GfxConfig config{64, 64, 0, false};
    const auto initialize_device = [&]()
    {
        if (!vulkan::gfx_init(config))
        {
            throw Error("boundary validation device creation failed");
        }
        gfx_begin_frame();
    };
    initialize_device();
    std::array<GfxNativeVertex, 6> vertices{};
    for (size_t n = 0; n < 6; ++n)
    {
        vertices[n].position = n % 3 == 0 ? std::array<float, 4>{-1.f, -1.f, .5f, 1.f} :
            n % 3 == 1 ? std::array<float, 4>{0.f, 1.f, .5f, 1.f} : std::array<float, 4>{1.f, -1.f, .5f, 1.f};
        vertices[n].diffuse = n < 3 ? std::array<float, 4>{1.f, 0.f, 0.f, 1.f} : std::array<float, 4>{0.f, 1.f, 0.f, 1.f};
    }
    std::array<u32, 3> indices{0, 1, 2};
    GfxNativeDraw draw;
    draw.vertices = vertices;
    draw.indices = indices;
    const auto expect = [&](u8 red, u8 green, u8 blue)
    {
        vulkan::gfx_draw_native(draw);
        RgbaImage image;
        if (!vulkan::gfx_readback(image))
        {
            throw Error("boundary validation readback failed");
        }
        const size_t offset = (size_t(image.height / 2) * image.width + image.width / 2) * 4;
        if (image.bgra[offset] != blue || image.bgra[offset + 1] != green || image.bgra[offset + 2] != red)
        {
            throw Error("native mutation/lifetime validation colour mismatch");
        }
    };
    expect(255, 0, 0);
    indices = {3, 4, 5};
    expect(0, 255, 0);
    for (auto& vertex : vertices)
    {
        vertex.diffuse = {1.f, 1.f, 1.f, 1.f};
    }
    RgbaImage texture;
    texture.width = texture.height = 1;
    texture.bgra = {255, 0, 0, 255};
    draw.textures[0] = &texture;
    expect(0, 0, 255);
    texture.bgra = {0, 255, 0, 255};
    ++texture.serial;
    expect(0, 255, 0);
    vulkan::gfx_release_sprite(texture);
    texture.bgra = {0, 0, 255, 255};
    texture.serial = 0;
    expect(255, 0, 0);
    draw.textures = {};
    const auto program = assemble_xg_vertex_shader("xvs.1.1\nm4x4 oPos, v0, c0\nmov oD0, c4\n");
    if (!gfx_native_vertex_program_supported(program))
    {
        throw Error("GPU mutation fixture unexpectedly fell back");
    }
    std::array<GfxNativeInputs, 6> inputs{};
    for (size_t n = 0; n < inputs.size(); ++n)
    {
        inputs[n][0] = vertices[n].position;
    }
    std::array<XboxVector, 192> constants{};
    for (size_t n = 0; n < 4; ++n)
    {
        constants[96 + n][n] = 1.f;
    }
    constants[58] = {320.f, -240.f, 16777215.f, 0.f};
    constants[59] = {320.f, 240.f, 0.f, 0.f};
    constants[100] = {1.f, 0.f, 0.f, 1.f};
    draw.vertices = {};
    draw.program_inputs = inputs;
    draw.vertex_program = program;
    draw.vertex_constants = &constants;
    expect(255, 0, 0);
    constants[100] = {0.f, 0.f, 1.f, 1.f};
    expect(0, 0, 255);
    vulkan::gfx_release_sprite(texture);
    gfx_shutdown();
    initialize_device();
    draw = {};
    draw.vertices = vertices;
    draw.indices = indices;
    expect(255, 255, 255);
    gfx_shutdown();
    log::info("Native boundary PASS (vulkan): vertex/index mutation, texture serial/release/address reuse, constants, "
              "device unload/reload; GPU vertex reference validation skipped");
}

const GfxBackend& backend_table()
{
    static const GfxBackend table = GBTG_GFX_BACKEND_TABLE("vulkan");
    return table;
}

} // namespace vulkan

const GfxBackend& gfx_vulkan_backend()
{
    return vulkan::backend_table();
}

} // namespace gbtg
