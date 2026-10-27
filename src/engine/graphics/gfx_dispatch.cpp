#include "gfx_backend.h"

#include "gbtg/log.h"

#include <array>
#include <cstring>

namespace gbtg
{
namespace
{

// Preference order when GfxConfig::backend is empty.
const GfxBackend* compiled_backend(size_t index)
{
    const std::array<const GfxBackend*, 3> backends{
#if defined(GBTG_GFX_D3D11)
        &gfx_d3d11_backend(),
#endif
#if defined(GBTG_GFX_VULKAN)
        &gfx_vulkan_backend(),
#endif
        &gfx_null_backend(),
    };
    return index < backends.size() ? backends[index] : nullptr;
}

const GfxBackend* active = nullptr;

const GfxBackend& backend()
{
    if (active == nullptr)
    {
        active = compiled_backend(0);
    }
    return *active;
}

} // namespace

bool gfx_select_backend(const char* name)
{
    active = compiled_backend(0);
    if (name == nullptr || name[0] == '\0')
    {
        return true;
    }
    for (size_t i = 0; const GfxBackend* candidate = compiled_backend(i); ++i)
    {
        if (std::strcmp(candidate->name, name) == 0)
        {
            active = candidate;
            return true;
        }
    }
    log::warn("gfx: backend '%s' is not compiled in; using %s", name, active->name);
    return false;
}

bool gfx_init(const GfxConfig& cfg)
{
    return backend().gfx_init(cfg);
}

void gfx_shutdown()
{
    backend().gfx_shutdown();
}

const char* gfx_backend_name()
{
    return backend().gfx_backend_name();
}

bool gfx_has_window()
{
    return backend().gfx_has_window();
}

void gfx_show_startup_loading_screen(const RgbaImage& text, float progress)
{
    backend().gfx_show_startup_loading_screen(text, progress);
}

void gfx_warmup_shader_permutations(const std::filesystem::path& path, const std::function<void(float)>& progress)
{
    backend().gfx_warmup_shader_permutations(path, progress);
}

void gfx_capture_shader_permutations(const std::filesystem::path& path)
{
    backend().gfx_capture_shader_permutations(path);
}

void gfx_pump(bool& quit)
{
    backend().gfx_pump(quit);
}

void gfx_begin_frame()
{
    backend().gfx_begin_frame();
}

void gfx_set_camera(const Vec3& position, const Quat& orientation, float fov_y)
{
    backend().gfx_set_camera(position, orientation, fov_y);
}

void gfx_set_dialog_camera(const Vec3& position, const Quat& orientation, float fov_y)
{
    backend().gfx_set_dialog_camera(position, orientation, fov_y);
}

void gfx_clear_dialog_background()
{
    backend().gfx_clear_dialog_background();
}

void gfx_begin_dialog_texture(u8 slot, int width, int height)
{
    backend().gfx_begin_dialog_texture(slot, width, height);
}

void gfx_end_dialog_texture()
{
    backend().gfx_end_dialog_texture();
}

void gfx_release_dialog_texture()
{
    backend().gfx_release_dialog_texture();
}

void gfx_draw_screen_mesh(const Mesh& mesh, float x, float y, float scale, bool clear_depth)
{
    backend().gfx_draw_screen_mesh(mesh, x, y, scale, clear_depth);
}

void gfx_clear_ui_depth(int left, int top, int right, int bottom)
{
    backend().gfx_clear_ui_depth(left, top, right, bottom);
}

void gfx_set_bones(const float* column_major, int count)
{
    backend().gfx_set_bones(column_major, count);
}

void gfx_draw_mesh(const Mesh& mesh, Vec3 origin, Vec3 scale)
{
    backend().gfx_draw_mesh(mesh, origin, scale);
}

void gfx_release_mesh(const Mesh& mesh)
{
    backend().gfx_release_mesh(mesh);
}

void gfx_draw_sprite(const RgbaImage& image, const GfxRect& rect)
{
    backend().gfx_draw_sprite(image, rect);
}

void gfx_release_sprite(const RgbaImage& image)
{
    backend().gfx_release_sprite(image);
}

void gfx_sprite_clear()
{
    backend().gfx_sprite_clear();
}

void gfx_sprite_submit(const RgbaImage& image, float x, float y, float w, float h, float u0, float v0, float u1,
                       float v1, std::array<float, 4> color)
{
    backend().gfx_sprite_submit(image, x, y, w, h, u0, v0, u1, v1, color);
}

bool gfx_native_vertex_program_supported(std::span<const u32> program)
{
    return backend().gfx_native_vertex_program_supported(program);
}

void gfx_validate_native_boundary()
{
    backend().gfx_validate_native_boundary();
}

void gfx_draw_native(const GfxNativeDraw& draw)
{
    backend().gfx_draw_native(draw);
}

void gfx_begin_native_batch()
{
    backend().gfx_begin_native_batch();
}

void gfx_end_native_batch()
{
    backend().gfx_end_native_batch();
}

void gfx_clear_native(std::span<const std::array<i32, 4>> rectangles, u32 flags, u32 color, float depth)
{
    backend().gfx_clear_native(rectangles, flags, color, depth);
}

void gfx_begin_native_target(int width, int height)
{
    backend().gfx_begin_native_target(width, height);
}

void gfx_end_native_target(RgbaImage& output)
{
    backend().gfx_end_native_target(output);
}

void gfx_read_native_target_depth(std::vector<u32>& output)
{
    backend().gfx_read_native_target_depth(output);
}

void gfx_size(int& width, int& height)
{
    backend().gfx_size(width, height);
}

void gfx_end_frame()
{
    backend().gfx_end_frame();
}

void gfx_present_again()
{
    backend().gfx_present_again();
}

bool gfx_readback(RgbaImage& out)
{
    return backend().gfx_readback(out);
}

bool gfx_readback_ui(RgbaImage& out)
{
    return backend().gfx_readback_ui(out);
}

} // namespace gbtg
