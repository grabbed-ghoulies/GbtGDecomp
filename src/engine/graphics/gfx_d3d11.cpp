#include "render_profile.h"
#include "xbox_vertex_hlsl.h"
#include "xbox_vertex_program.h"
#include "xg_shader_assembly.h"
#include "vertex_program_constants.h"
#if !defined(GBTG_GFX_D3D11)
#error gfx_d3d11.cpp is the PC DirectX 11 backend
#endif

#include "gfx_backend.h"
#include "win32_window.h"
#include "gfx_present_logic.h"
#include "gfx_shader_sources.h"
#include "shader_cache.h"
#include "gbtg/gfx.h"
#include "gbtg/log.h"
#include "gbtg/movie.h"
#include "gbtg/entity.h"
#include "../gbtg_resource.h"

#if defined(GBTG_INSPECTOR_APP)
#include "../inspector/inspector_graphics.h"
#endif

#if defined(GBTG_INSPECTOR_APP)
#include "gbtg/debug_ui.h"
#elif !defined(GBTG_RETAIL_GAME_ONLY)
#include "../plugins/plugins.h"
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <intrin.h>

#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <dxgi.h>

#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <unordered_map>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace gbtg
{
// The gfx_* definitions below are this backend's; gfx_dispatch.cpp owns the public names.
namespace d3d11
{
void gfx_release_sprite(const RgbaImage& image);

namespace
{
void present_hud_viewport();
void present_camera_viewport();
void present_hud_sprites();

struct Mat4
{
    float m[16]{}; // column-major
};

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

struct GpuCB
{
    float mvp[16]{};
    float albedo_uv = 0.f;
    float light_uv = 1.f;
    float use_light = 0.f;
    float use_cel = 0.f;
    // FUN_0005e380 vertex constant 100. The draw passes a packed colour and the
    // function stores each byte times 1/255 (the float at 0x32b154). White until
    // a material colour is threaded through.
    float light_x = 1.f;
    float light_y = 1.f;
    float light_z = 1.f;
    float skin = 0.f; // light_dir.w: rigid skin palette is active
    // FUN_0005e0c0. x = ALPHATESTENABLE, y = ALPHAREF (0..255), z = ALPHAFUNC.
    float alpha_test = 0.f;
    float alpha_ref = 0.f;
    float alpha_func = 0.f;
    float alpha_pad = 0.f;
    std::array<float, 4> tint{1.f, 1.f, 1.f, 1.f};
    std::array<float, 4> skin_mask{1.f, 1.f, 0.f, 0.f};
    // native_ps without a combiner definition: 1/width and 1/height of a linear texture 0, which the NV2A samples
    // in texels; zero leaves the coordinates normalized.
    std::array<float, 4> native_uv_scale{0.f, 0.f, 0.f, 0.f};
};

static_assert(sizeof(GpuVert) == 96 && offsetof(GpuVert, diffuse) == 64 &&
              offsetof(GpuVert, u3) == 72 && offsetof(GpuVert, bone3) == 80 && offsetof(GpuVert, w2) == 88);
static_assert(sizeof(GpuCB) == 160 && offsetof(GpuCB, skin_mask) == 128 && offsetof(GpuCB, native_uv_scale) == 144);

struct GfxD3D11
{
    GfxConfig cfg;
    bool ok = false;
    HWND hwnd = nullptr;
    bool quit = false;

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swap;
    ComPtr<ID3D11Texture2D> color;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11Texture2D> depth;
    ComPtr<ID3D11DepthStencilView> dsv;
    ComPtr<ID3D11Texture2D> last_frame; // The last swapped game frame, without the debug UI, for gfx_present_again.
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11VertexShader> clear_vs;
    ComPtr<ID3D11Buffer> clear_cb;
    ComPtr<ID3D11DepthStencilState> clear_depth;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11PixelShader> model_ps;
    std::string model_source;
    std::string vertex_source;
    struct ProgramGpu
    {
        ComPtr<ID3D11VertexShader> shader;
        ComPtr<ID3D11InputLayout> layout;
        bool validated = false;
#if GBTG_OPTIMIZE
        // GBTG_OPTIMIZE step 18: the constant registers the program reads (program_constant_range).
        u32 constants_first = 0;
        u32 constants_count = 192;
#endif
    };
    ComPtr<ID3D11GeometryShader> validation_shader;
    struct ProgramKeyLess
    {
        using is_transparent = void;
        bool operator()(std::span<const u32> a, std::span<const u32> b) const
        {
            return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
        }
    };
    std::map<std::vector<u32>, ProgramGpu, ProgramKeyLess> native_programs;
    ComPtr<ID3D11Buffer> native_constants;
    std::map<std::array<u32, 60>, ComPtr<ID3D11PixelShader>> specialized_pixels;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> cb;
    ComPtr<ID3D11Buffer> bone_cb;
    static constexpr int kMaxBones = 128;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11SamplerState> light_sampler;
    ComPtr<ID3D11RasterizerState> rast;
    ComPtr<ID3D11DepthStencilState> depth_state;
    ComPtr<ID3D11DepthStencilState> depth_lequal;
    ComPtr<ID3D11DepthStencilState> depth_lequal_ro;
    ComPtr<ID3D11DepthStencilState> depth_off;
    ComPtr<ID3D11BlendState> blend_opaque;
    ComPtr<ID3D11BlendState> blend_alpha;
    ComPtr<ID3D11Buffer> ui_vb;
    // Native scene draws (gfx_draw_native).
    ComPtr<ID3D11VertexShader> native_vs;
    ComPtr<ID3D11PixelShader> native_ps;
    ComPtr<ID3D11InputLayout> native_layout;
    ComPtr<ID3D11Buffer> native_vb;
    UINT native_vb_size = 0;
    UINT native_vb_cursor = 0;
    bool native_batch = false;
    bool native_pipeline = false;
    ComPtr<ID3D11Buffer> native_ib;
    UINT native_ib_size = 0;
    UINT native_ib_cursor = 0;
    ComPtr<ID3D11Buffer> native_material;
    std::unordered_map<u64, ComPtr<ID3D11BlendState>> native_blends;
    std::map<std::array<u32, 9>, ComPtr<ID3D11DepthStencilState>> native_depths;
    std::unordered_map<u64, ComPtr<ID3D11RasterizerState>> native_rasterizers; // Cull mode, then the z bias.
    std::unordered_map<u32, ComPtr<ID3D11SamplerState>> native_samplers;
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 2: what the previous native draw bound and uploaded. It describes the
    // pipeline only while native_pipeline is set, because nothing else binds between adjacent
    // native draws of one batch (clears, copies and target changes end the batch).
    struct NativeBound
    {
        D3D11_VIEWPORT viewport{};
        D3D11_RECT scissor{};
        std::array<ID3D11ShaderResourceView*, 4> views{};
        std::array<ID3D11SamplerState*, 4> samplers{};
        ID3D11BlendState* blend = nullptr;
        ID3D11DepthStencilState* depth = nullptr;
        UINT stencil_reference = 0;
        ID3D11RasterizerState* rasterizer = nullptr;
        ID3D11InputLayout* layout = nullptr;
        D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
        ID3D11VertexShader* vertex_shader = nullptr;
        ID3D11PixelShader* pixel_shader = nullptr;
        bool material = false; // native_material holds `words` and is bound at PS b2.
        bool constants = false; // native_constants holds `vertex_constants` and is bound at VS b3.
        GpuCB cb{};
        std::array<u32, 80> words{};
        std::array<std::array<float, 4>, 192> vertex_constants{};
        // GBTG_OPTIMIZE step 18: the registers of vertex_constants that native_constants holds (the rest of the
        // buffer is undefined after a partial upload), the specialized shader of words[0..59], and the keys the
        // state objects and views above were found under.
        u32 constants_first = 0;
        u32 constants_count = 0;
        u64 constants_id = 0; // GBTG_OPTIMIZE step 21: GfxNativeDraw::vertex_constants_id of vertex_constants.
        ID3D11PixelShader* definition_shader = nullptr;
        bool states = false; // blend_key .. sampler_keys name blend, depth, rasterizer and samplers.
        u64 blend_key = 0;
        std::array<u32, 9> depth_key{};
        u64 rasterizer_key = 0;
        std::array<u32, 4> sampler_keys{};
        std::array<const RgbaImage*, 4> images{};
        std::array<u32, 4> image_serials{};
    } native_bound;
    // The program gfx_native_vertex_program_supported just found, for the draw that follows it.
    std::span<const u32> supported_words;
    ProgramGpu* supported_program = nullptr;
    // GBTG_OPTIMIZE step 13: native_programs lookups by the address of the program's words, verified against a
    // copy of them, so a reused address with other words looks up again (the Vulkan step 3 memo).
    struct ProgramMemo
    {
        std::vector<u32> words;
        ProgramGpu* program = nullptr;
        u64 batch = 0; // GBTG_OPTIMIZE step 18: native_batch_serial when the words were last compared.
    };
    // Counts gfx_begin_native_batch calls. Within a batch the caller issues draws only, so the words a program
    // span points at were compared against the memo's copy once already.
    u64 native_batch_serial = 0;
    std::unordered_map<const u32*, ProgramMemo> program_memo;
    // GBTG_OPTIMIZE step 7: immutable uploads of the presenter's retained geometry, by
    // GfxNativeDraw::retained_geometry. An id's contents never change, so an upload is made once.
    struct RetainedUpload
    {
        ComPtr<ID3D11Buffer> vertices;
        ComPtr<ID3D11Buffer> indices;
        size_t bytes = 0;
        u64 last_used = 0;
    };
    std::unordered_map<u64, RetainedUpload> retained_uploads;
    size_t retained_bytes = 0;
    u64 frame_number = 0;
    // GBTG_OPTIMIZE step 3: the last specialized pixel shader and its key (owned by specialized_pixels).
    std::array<u32, 60> last_pixel_key{};
    ID3D11PixelShader* last_pixel = nullptr;
    // GBTG_OPTIMIZE step 6: gfx_readback's staging texture.
    ComPtr<ID3D11Texture2D> readback_staging;
#endif
    struct SpriteGpu
    {
        ComPtr<ID3D11ShaderResourceView> srv;
        u32 serial = 0;
    };
    std::unordered_map<const RgbaImage*, SpriteGpu> sprites;
    // Device shadow at 0x00181B70 (index * 4). BSS until a render-state setter writes it.
    u32 rs_dirty = 0;        // DAT_00181968
    int rs_zfunc = 0;        // DAT_00181c54, D3DRS_ZFUNC (index 0x39)
    int rs_alpha_func = 0;  // DAT_00181c58, D3DRS_ALPHAFUNC (index 0x3A)
    int rs_alpha_blend = 0; // DAT_00181c5c, D3DRS_ALPHABLENDENABLE (index 0x3B)
    int rs_alpha_test = 0;  // DAT_00181c60, D3DRS_ALPHATESTENABLE (index 0x3C)
    int rs_alpha_ref = 0;   // DAT_00181c64, D3DRS_ALPHAREF (index 0x3D)
    int rs_src_blend = 0;   // DAT_00181c68, D3DRS_SRCBLEND (index 0x3E)
    int rs_dest_blend = 0;  // DAT_00181c6c, D3DRS_DESTBLEND (index 0x3F)
    int rs_zwrite = 0;      // DAT_00181c70, D3DRS_ZWRITEENABLE (index 0x40)
    int rs_dither = 0;      // DAT_00181c74, D3DRS_DITHERENABLE (index 0x41)
    int rs_color_write = 0; // DAT_00181c7c, D3DRS_COLORWRITEENABLE (index 0x43)
    int rs_blend_op = 0;    // DAT_00181c98, D3DRS_BLENDOP (index 0x4A)
    int rs_cull = 0;        // DAT_00181dbc, CullMode argument
    int rs_zenable = 0;     // DAT_00181dac, ZEnable argument (0 off, 1 Z, 2 W)
    int rs_dxt1_noise = 0;  // DAT_00181dec
    int rs_state_60 = 0;    // DAT_00181cf0, index 0x60, dirty bit 0x2000
    int rs_state_66 = 0;    // DAT_00181d08, index 0x66, dirty bit 0x1200
    int vertex_shader = 0;  // D3DDevice_SetVertexShader argument. 2 = D3DFVF_XYZ.
    // Stage 0 lives at DAT_00181970, stride 0x80. The 0x0005E110 loop anchors on ALPHAOP.
    // Ghidra names the stage base D3D_g_DeferredTextureState; it is ADDRESSU.
    struct StageRs
    {
        int address_u = 0;      // +0x00 D3DTSS_ADDRESSU
        int address_v = 0;      // +0x04 D3DTSS_ADDRESSV
        int address_w = 0;      // +0x08 D3DTSS_ADDRESSW
        int mag_filter = 0;     // +0x0C D3DTSS_MAGFILTER
        int min_filter = 0;     // +0x10 D3DTSS_MINFILTER
        int mip_filter = 0;     // +0x14 D3DTSS_MIPFILTER
        int max_mip_level = 0;  // +0x1C D3DTSS_MAXMIPLEVEL
        u32 color_sign = 0;     // +0x28 D3DTSS_COLORSIGN
        int color_op = 0;       // +0x30 D3DTSS_COLOROP
        int color_arg1 = 0;     // +0x38 D3DTSS_COLORARG1
        int color_arg2 = 0;     // +0x3C D3DTSS_COLORARG2
        int alpha_op = 0;       // +0x40 D3DTSS_ALPHAOP
        int alpha_arg1 = 0;     // +0x48 D3DTSS_ALPHAARG1
        int alpha_arg2 = 0;     // +0x4C D3DTSS_ALPHAARG2
        int tex_transform = 0;  // +0x54 D3DTSS_TEXTURETRANSFORMFLAGS
        int texcoord_index = 0; // +0x70 D3DTSS_TEXCOORDINDEX
        // +0x58. D3DTSS_BUMPENVMAT00, MAT01, MAT10, MAT11, BUMPENVLSCALE, BUMPENVLOFFSET.
        u32 bump[6]{};
    };
    StageRs stages[4]{};
};

GfxD3D11 g;

// The texture surface native draws render into between gfx_begin_native_target and gfx_end_native_target.
struct NativeRenderTarget
{
    int width = 0;
    int height = 0;
    bool rendering = false;
    bool depth_bound = true; // False while a target with no depth surface is open.
    D3D11_VIEWPORT saved_viewport{};
    ComPtr<ID3D11Texture2D> color;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11Texture2D> depth;
    ComPtr<ID3D11DepthStencilView> dsv;
    ComPtr<ID3D11Texture2D> staging;
    ComPtr<ID3D11Texture2D> depth_staging;
    // GBTG_OPTIMIZE step 12: the staging copy gfx_end_native_target_into last started, until it is read.
    ComPtr<ID3D11Texture2D> ended;
};
NativeRenderTarget native_target;

// A texture target that shares the back buffer's depth-stencil (D3DDevice_SetRenderTarget onto a texture with the
// device's own depth surface): native draws go to a colour surface shaped like the back buffer, under the same
// fitted viewport, and ending scales the frame's area down into the texture.
struct NativeSceneSurface
{
    D3D11_TEXTURE2D_DESC desc{}; // The colour surface's, matching the back buffer.
    ComPtr<ID3D11Texture2D> color;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11Texture2D> resolved;
    ComPtr<ID3D11Texture2D> staging;
    // A depth-stencil of the surface's own, made the first time the surface opens with own_depth.
    ComPtr<ID3D11Texture2D> depth;
    ComPtr<ID3D11DepthStencilView> dsv;
    ComPtr<ID3D11Texture2D> depth_staging; // For gfx_read_native_target_depth.
};
struct NativeSceneTarget
{
    int width = 0; // The texture's size.
    int height = 0;
    bool rendering = false;
    bool own_depth = false; // The open surface draws against its own depth-stencil, not the back buffer's.
    D3D11_VIEWPORT saved_viewport{};
    std::unordered_map<u32, NativeSceneSurface> surfaces; // By texture, each keeping its contents between opens.
    NativeSceneSurface* open = nullptr;
};
NativeSceneTarget scene_target;

LRESULT CALLBACK gfx_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
#if defined(GBTG_INSPECTOR_APP)
    if (msg == WM_GETMINMAXINFO)
    {
        RECT minimum{0, 0, 800, 600};
        AdjustWindowRectEx(&minimum, DWORD(GetWindowLongPtrW(hwnd, GWL_STYLE)), FALSE,
            DWORD(GetWindowLongPtrW(hwnd, GWL_EXSTYLE)));
        auto* limits = reinterpret_cast<MINMAXINFO*>(lp);
        limits->ptMinTrackSize = {minimum.right - minimum.left, minimum.bottom - minimum.top};
        return 0;
    }
#endif
#if defined(GBTG_INSPECTOR_APP)
    if (debug_ui_wndproc(hwnd, msg, wp, lp))
    {
        return 0;
    }
    // Escape quits the process. While the menu bar is open it stays with ImGui.
    if (debug_ui_menu_open() && msg == WM_KEYDOWN && wp == VK_ESCAPE)
    {
        return 0;
    }
#elif !defined(GBTG_RETAIL_GAME_ONLY)
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
    if (msg == WM_CLOSE || (msg == WM_KEYDOWN && wp == VK_ESCAPE))
    {
        g.quit = true;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

#if GBTG_OPTIMIZE
// GBTG_OPTIMIZE step 15: compiled bytecode is kept in dxbc/<FNV-1a of the inputs>.dxbc in the shader cache
// (shader_cache.h), beside the Vulkan backend's spirv/, so a shader is compiled by D3DCompile once per machine instead of once per run. Bump the
// version when the D3DCompile flags change: an entry is keyed by its inputs, not by them.
constexpr u64 kDxbcCacheVersion = 1;

std::filesystem::path dxbc_cache_path(const char* src, const char* entry, const char* profile, bool strict)
{
    u64 hash = 0xcbf29ce484222325ull;
    const auto mix = [&](std::string_view bytes)
    {
        for (const char c : bytes)
        {
            hash = (hash ^ u8(c)) * 0x100000001b3ull;
        }
        hash = (hash ^ 0xFF) * 0x100000001b3ull;
    };
    mix(src);
    mix(entry);
    mix(profile);
    mix(strict ? "strict" : "");
    mix(std::to_string(kDxbcCacheVersion));
    char name[32];
    std::snprintf(name, sizeof(name), "%016llx.dxbc", static_cast<unsigned long long>(hash));
    return std::filesystem::path("dxbc") / name;
}

bool read_dxbc(const std::filesystem::path& path, ComPtr<ID3DBlob>& blob)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return false;
    }
    const std::vector<char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    // A container starts with "DXBC" and holds its own size at +24; anything else is a damaged entry.
    u32 size = 0;
    if (bytes.size() < 32 || std::memcmp(bytes.data(), "DXBC", 4) != 0)
    {
        return false;
    }
    std::memcpy(&size, bytes.data() + 24, 4);
    if (size != bytes.size() || FAILED(D3DCreateBlob(bytes.size(), blob.ReleaseAndGetAddressOf())))
    {
        return false;
    }
    std::memcpy(blob->GetBufferPointer(), bytes.data(), bytes.size());
    return true;
}

void write_dxbc(const std::filesystem::path& path, ID3DBlob* blob)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    const std::filesystem::path partial = path.string() + ".tmp";
    {
        std::ofstream output(partial, std::ios::binary | std::ios::trunc);
        output.write(static_cast<const char*>(blob->GetBufferPointer()), std::streamsize(blob->GetBufferSize()));
        if (!output)
        {
            return;
        }
    }
    std::filesystem::rename(partial, path, error);
}
#endif

bool compile_shader(const char* src, const char* entry, const char* profile, ComPtr<ID3DBlob>& blob, bool strict = false)
{
#if GBTG_OPTIMIZE
    const std::filesystem::path cached = dxbc_cache_path(src, entry, profile, strict);
    for (const std::filesystem::path& path : shader_cache_reads(cached))
    {
        if (read_dxbc(path, blob))
        {
            return true;
        }
    }
#endif
    ComPtr<ID3DBlob> err;
    const HRESULT hr = D3DCompile(src, std::strlen(src), "gbtg", nullptr, nullptr, entry, profile, D3DCOMPILE_OPTIMIZATION_LEVEL1 | (strict ? D3DCOMPILE_IEEE_STRICTNESS : 0),
                                  0, blob.GetAddressOf(), err.GetAddressOf());
    if (FAILED(hr))
    {
        if (err)
        {
            log::error("hlsl: %s", static_cast<const char*>(err->GetBufferPointer()));
        }
        return false;
    }
#if GBTG_OPTIMIZE
    write_dxbc(shader_cache_write(cached), blob.Get());
#endif
    return true;
}

ComPtr<ID3D11ShaderResourceView> upload_bgra(const u8* pixels, int width, int height)
{
    ComPtr<ID3D11ShaderResourceView> srv;
    if (!pixels || width <= 0 || height <= 0)
    {
        return srv;
    }
    D3D11_TEXTURE2D_DESC td{};
    td.Width = u32(width);
    td.Height = u32(height);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = pixels;
    data.SysMemPitch = u32(width * 4);
    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(g.device->CreateTexture2D(&td, &data, tex.GetAddressOf())))
    {
        return srv;
    }
    g.device->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf());
    return srv;
}

bool create_targets(int width, int height)
{
    D3D11_TEXTURE2D_DESC depth{};
    depth.Width = u32(width);
    depth.Height = u32(height);
    depth.MipLevels = 1;
    depth.ArraySize = 1;
    depth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth.SampleDesc.Count = 1;
    depth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(g.device->CreateTexture2D(&depth, nullptr, g.depth.GetAddressOf())))
    {
        return false;
    }
    if (FAILED(g.device->CreateDepthStencilView(g.depth.Get(), nullptr, g.dsv.GetAddressOf())))
    {
        return false;
    }
    if (g.swap)
    {
        ComPtr<ID3D11Texture2D> back;
        if (FAILED(g.swap->GetBuffer(0, IID_PPV_ARGS(back.GetAddressOf()))))
        {
            return false;
        }
        return SUCCEEDED(g.device->CreateRenderTargetView(back.Get(), nullptr, g.rtv.GetAddressOf()));
    }
    D3D11_TEXTURE2D_DESC color{};
    color.Width = u32(width);
    color.Height = u32(height);
    color.MipLevels = 1;
    color.ArraySize = 1;
    color.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    color.SampleDesc.Count = 1;
    color.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (FAILED(g.device->CreateTexture2D(&color, nullptr, g.color.GetAddressOf())))
    {
        return false;
    }
    return SUCCEEDED(g.device->CreateRenderTargetView(g.color.Get(), nullptr, g.rtv.GetAddressOf()));
}

void store_alpha_rs(GpuCB& cb)
{
    cb.alpha_test = g.rs_alpha_test != 0 ? 1.f : 0.f;
    cb.alpha_ref = float(g.rs_alpha_ref);
    cb.alpha_func = float(g.rs_alpha_func);
}

// ZFUNC stays at the LESS default until 0x0005E110 stores D3DCMP_LESSEQUAL (0x203).
void bind_depth_shadow()
{
    if (!g.ok)
    {
        return;
    }
    ID3D11DepthStencilState* state = g.depth_state.Get();
    if (g.rs_zfunc == 0x203)
    {
        if (g.rs_zenable == 0)
        {
            state = g.depth_off.Get();
        }
        else if (g.rs_zwrite != 0)
        {
            state = g.depth_lequal.Get();
        }
        else
        {
            state = g.depth_lequal_ro.Get();
        }
    }
    g.context->OMSetDepthStencilState(state, 0);
}

// Xbox 0x0005E110
// Installs the frame device state. Simple pushes are (method, value) then the shadow dword.
//   Dxt1NoiseEnable(0)                    DAT_00181dec = 0, dirty |= 0x2000
//   index 0x60                            DAT_00181cf0 = 1
//   0x40310 = 0     D3DRS_DITHERENABLE    DAT_00181c74 = 0
//   CullMode(0)                           DAT_00181dbc = 0
//   0x40350 = 0x8006 D3DBLENDOP_ADD       DAT_00181c98
//   0x40344 = 0x302  D3DBLEND_SRCALPHA    DAT_00181c68
//   0x40348 = 0x303  D3DBLEND_INVSRCALPHA DAT_00181c6c
//   0x40304 = 1      ALPHABLENDENABLE     DAT_00181c5c
//   0x4033c = 0x204  D3DCMP_GREATER       DAT_00181c58
//   index 0x66                            DAT_00181d08 = 0, dirty |= 0x1200
//   0x40358 = 0x01010101 COLORWRITEENABLE DAT_00181c7c
//   0x40354 = 0x203  D3DCMP_LESSEQUAL     DAT_00181c54 ZFUNC
//   0x4035c = 1      ZWRITEENABLE         DAT_00181c70
//   ZEnable(1)                            DAT_00181dac = 1
// Four stages at DAT_001819B0 stride 0x80: unbound texture, linear mag/min,
// colour and alpha op D3DTOP_DISABLE, dirty |= (1 << stage) | 0x800.
// Then vertex-shader input, sixteen stream sources, pixel shader, and vertex shader are cleared.
// Xbox 0x0005E110
void d3d_reset_render_state()
{
    g.rs_dxt1_noise = 0;
    g.rs_dirty |= 0x2000u;
    g.rs_state_60 = 1;
    g.rs_dither = 0;
    g.rs_cull = 0;
    g.rs_blend_op = 0x8006;
    g.rs_src_blend = 0x302;
    g.rs_dest_blend = 0x303;
    g.rs_alpha_blend = 1;
    g.rs_alpha_func = 0x204;
    g.rs_dirty |= 0x1200u;
    g.rs_state_66 = 0;
    g.rs_color_write = 0x01010101;
    g.rs_zfunc = 0x203;
    g.rs_zwrite = 1;
    g.rs_zenable = 1;
    for (int stage = 0; stage < 4; ++stage)
    {
        GfxD3D11::StageRs& slot = g.stages[stage];
        slot.mag_filter = 2;
        slot.min_filter = 2;
        slot.color_op = 1;
        slot.alpha_op = 1;
        g.rs_dirty |= (1u << stage) | 0x800u;
    }
    if (!g.ok)
    {
        return;
    }
    g.context->RSSetState(g.rast.Get());
    g.context->OMSetBlendState(g.blend_alpha.Get(), nullptr, 0xFFFFFFFFu);
    bind_depth_shadow();
    ID3D11ShaderResourceView* srvs[2] = {};
    g.context->PSSetShaderResources(0, 2, srvs);
    ID3D11Buffer* streams[16] = {};
    UINT strides[16] = {};
    UINT offsets[16] = {};
    g.context->IASetVertexBuffers(0, 16, streams, strides, offsets);
    g.context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    g.context->IASetInputLayout(nullptr);
    g.vertex_shader = 0;
    g.context->VSSetShader(nullptr, nullptr, 0);
    g.context->PSSetShader(nullptr, nullptr, 0);
}

} // namespace

bool gfx_init(const GfxConfig& cfg)
{
    g = {};
    g.cfg = cfg;
    const int width = cfg.width > 0 ? cfg.width : 960;
    const int height = cfg.height > 0 ? cfg.height : 540;
    g.cfg.width = width;
    g.cfg.height = height;
    g.cfg.vsync = cfg.vsync;

    if (cfg.visible)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = gfx_wndproc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"GbtGDecompGfx";
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(IDI_GBTG_APP));
        wc.hIconSm = wc.hIcon;
        RegisterClassExW(&wc);
        const Win32WindowPlacement placement = win32_window_placement(cfg.fullscreen, width, height);
        g.hwnd = CreateWindowW(wc.lpszClassName, L"Grabbed by the Ghoulies", placement.style, placement.x, placement.y,
                               placement.width, placement.height, nullptr, nullptr, wc.hInstance, nullptr);
        if (!g.hwnd)
        {
            log::error("d3d11: CreateWindow failed");
            return false;
        }
    }

    // The flip model: Present waits for the vertical blank in a window too, where the blit model can tear under
    // composition (variable refresh, monitors of different rates). The back buffer's contents after a Present are
    // undefined; gfx_present_again copies the held frame back before presenting again.
    DXGI_SWAP_CHAIN_DESC sc{};
    sc.BufferCount = 2;
    sc.BufferDesc.Width = u32(width);
    sc.BufferDesc.Height = u32(height);
    sc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc.OutputWindow = g.hwnd;
    sc.SampleDesc.Count = 1;
    sc.Windowed = TRUE;
    sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    const D3D_FEATURE_LEVEL want = D3D_FEATURE_LEVEL_11_0;
    D3D_FEATURE_LEVEL got{};
    HRESULT hr;
    if (cfg.visible)
    {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &want, 1, D3D11_SDK_VERSION, &sc,
                                           g.swap.GetAddressOf(), g.device.GetAddressOf(), &got, g.context.GetAddressOf());
    }
    else
    {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &want, 1, D3D11_SDK_VERSION,
                               g.device.GetAddressOf(), &got, g.context.GetAddressOf());
    }
    if (FAILED(hr))
    {
        log::error("d3d11: device create failed");
        gfx_shutdown();
        return false;
    }
    if (g.swap)
    {
        // One frame in flight: each Present waits for its own vertical blank instead of queueing frames ahead.
        ComPtr<IDXGIDevice1> latency;
        if (SUCCEEDED(g.device.As(&latency)))
        {
            latency->SetMaximumFrameLatency(1);
        }
    }
    if (render_profile::enabled)
    {
        ComPtr<IDXGIDevice> dxgi;
        ComPtr<IDXGIAdapter> adapter;
        DXGI_ADAPTER_DESC desc{};
        if (SUCCEEDED(g.device.As(&dxgi)) && SUCCEEDED(dxgi->GetAdapter(adapter.GetAddressOf())) && SUCCEEDED(adapter->GetDesc(&desc)))
        {
            log::info("profile adapter=%ls vendor=%u device=%u", desc.Description, desc.VendorId, desc.DeviceId);
        }
    }
    if (!create_targets(width, height))
    {
        log::error("d3d11: render target failed");
        gfx_shutdown();
        return false;
    }

    const char* hlsl = gfx_hlsl_common();
    std::string model_hlsl = std::string(hlsl) + gfx_hlsl_combiners();
    // Native scene draws arrive with their NV2A vertex program already run: clip position, colours and
    // four texture coordinates. Their pixel stage is ps_model (the combiner definition) or, without one,
    // texture 0 modulated by the diffuse colour.
    const char* native_hlsl = gfx_hlsl_native();
    const char* alpha_hlsl = gfx_hlsl_alpha_test();
    const std::string shared_hlsl = std::string(hlsl) + alpha_hlsl;
    model_hlsl = shared_hlsl + model_hlsl.substr(std::string(hlsl).size());
    const std::string native_source = model_hlsl + native_hlsl;
    ComPtr<ID3DBlob> native_vs_blob;
    ComPtr<ID3DBlob> native_ps_blob;
    if (!compile_shader(native_source.c_str(), "native_vs", "vs_4_0", native_vs_blob) ||
        !compile_shader(native_source.c_str(), "native_ps", "ps_4_0", native_ps_blob))
    {
        gfx_shutdown();
        return false;
    }
    g.device->CreateVertexShader(native_vs_blob->GetBufferPointer(), native_vs_blob->GetBufferSize(), nullptr,
        g.native_vs.GetAddressOf());
    g.device->CreatePixelShader(native_ps_blob->GetBufferPointer(), native_ps_blob->GetBufferSize(), nullptr,
        g.native_ps.GetAddressOf());
    const D3D11_INPUT_ELEMENT_DESC native_layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 56, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, 64, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT, 0, 72, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 4, DXGI_FORMAT_R32_FLOAT, 0, 80, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 5, DXGI_FORMAT_R32G32_FLOAT, 0, 84, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 6, DXGI_FORMAT_R32G32_FLOAT, 0, 92, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 7, DXGI_FORMAT_R32G32_FLOAT, 0, 100, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 8, DXGI_FORMAT_R32G32_FLOAT, 0, 108, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    static_assert(sizeof(GfxNativeVertex) == 116);
    g.device->CreateInputLayout(native_layout, UINT(std::size(native_layout)), native_vs_blob->GetBufferPointer(),
        native_vs_blob->GetBufferSize(), g.native_layout.GetAddressOf());
    D3D11_BUFFER_DESC native_material{};
    native_material.ByteWidth = 80 * 4;
    native_material.Usage = D3D11_USAGE_DYNAMIC;
    native_material.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    native_material.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    g.device->CreateBuffer(&native_material, nullptr, g.native_material.GetAddressOf());
    g.model_source = model_hlsl;
    g.vertex_source = hlsl;
    D3D11_BUFFER_DESC native_constants_desc{};
    native_constants_desc.ByteWidth = 192 * 16;
    native_constants_desc.Usage = D3D11_USAGE_DYNAMIC;
    native_constants_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    native_constants_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(g.device->CreateBuffer(&native_constants_desc, nullptr, g.native_constants.GetAddressOf())))
    {
        throw Error("native vertex constants creation failed");
    }
    ComPtr<ID3DBlob> model_blob;
    if (!compile_shader(model_hlsl.c_str(), "ps_model", "ps_4_0", model_blob))
    {
        gfx_shutdown();
        return false;
    }
    g.device->CreatePixelShader(model_blob->GetBufferPointer(), model_blob->GetBufferSize(), nullptr, g.model_ps.GetAddressOf());
    ComPtr<ID3DBlob> vs_blob;
    ComPtr<ID3DBlob> ps_blob;
    if (!compile_shader(hlsl, "vs_main", "vs_4_0", vs_blob) || !compile_shader(hlsl, "ps_main", "ps_4_0", ps_blob))
    {
        gfx_shutdown();
        return false;
    }
    g.device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, g.vs.GetAddressOf());
    g.device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, g.ps.GetAddressOf());
    const char* clear_hlsl = gfx_hlsl_clear();
    ComPtr<ID3DBlob> clear_blob;
    if (!compile_shader(clear_hlsl, "clear_vertex", "vs_4_0", clear_blob))
    {
        gfx_shutdown();
        return false;
    }
    g.device->CreateVertexShader(clear_blob->GetBufferPointer(), clear_blob->GetBufferSize(), nullptr,
                                g.clear_vs.GetAddressOf());
    D3D11_BUFFER_DESC clear_buffer{};
    clear_buffer.ByteWidth = 16;
    clear_buffer.Usage = D3D11_USAGE_DYNAMIC;
    clear_buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    clear_buffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    g.device->CreateBuffer(&clear_buffer, nullptr, g.clear_cb.GetAddressOf());
    D3D11_DEPTH_STENCIL_DESC clear_depth{};
    clear_depth.DepthEnable = TRUE;
    clear_depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    clear_depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
    clear_depth.StencilEnable = TRUE;
    clear_depth.StencilReadMask = 0xff;
    clear_depth.StencilWriteMask = 0xff;
    clear_depth.FrontFace.StencilFailOp = D3D11_STENCIL_OP_REPLACE;
    clear_depth.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
    clear_depth.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
    clear_depth.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    clear_depth.BackFace = clear_depth.FrontFace;
    g.device->CreateDepthStencilState(&clear_depth, g.clear_depth.GetAddressOf());
    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT, 0, 56, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 4, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 64, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 68, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 5, DXGI_FORMAT_R32G32_FLOAT, 0, 72, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 6, DXGI_FORMAT_R32G32_FLOAT, 0, 80, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 7, DXGI_FORMAT_R32G32_FLOAT, 0, 88, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    g.device->CreateInputLayout(layout, UINT(std::size(layout)), vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), g.layout.GetAddressOf());

    D3D11_BUFFER_DESC cbd{};
    cbd.ByteWidth = sizeof(GpuCB);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    g.device->CreateBuffer(&cbd, nullptr, g.cb.GetAddressOf());
    cbd.ByteWidth = UINT(GfxD3D11::kMaxBones * 16 * sizeof(float));
    g.device->CreateBuffer(&cbd, nullptr, g.bone_cb.GetAddressOf());

    D3D11_SAMPLER_DESC samp{};
    samp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samp.AddressU = samp.AddressV = samp.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samp.MaxLOD = D3D11_FLOAT32_MAX;
    g.device->CreateSamplerState(&samp, g.sampler.GetAddressOf());
    samp.AddressU = samp.AddressV = samp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    g.device->CreateSamplerState(&samp, g.light_sampler.GetAddressOf());

    D3D11_RASTERIZER_DESC rast{};
    rast.FillMode = D3D11_FILL_SOLID;
    rast.CullMode = D3D11_CULL_NONE;
    rast.DepthClipEnable = TRUE;
    g.device->CreateRasterizerState(&rast, g.rast.GetAddressOf());

    D3D11_DEPTH_STENCIL_DESC ds{};
    ds.DepthEnable = TRUE;
    ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    ds.DepthFunc = D3D11_COMPARISON_LESS;
    g.device->CreateDepthStencilState(&ds, g.depth_state.GetAddressOf());
    ds.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    g.device->CreateDepthStencilState(&ds, g.depth_lequal.GetAddressOf());
    ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    g.device->CreateDepthStencilState(&ds, g.depth_lequal_ro.GetAddressOf());
    ds.DepthEnable = FALSE;
    g.device->CreateDepthStencilState(&ds, g.depth_off.GetAddressOf());

    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    g.device->CreateBlendState(&blend, g.blend_opaque.GetAddressOf());
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    g.device->CreateBlendState(&blend, g.blend_alpha.GetAddressOf());


    D3D11_BUFFER_DESC ui_bd{};
    ui_bd.ByteWidth = u32(6 * sizeof(GpuVert));
    ui_bd.Usage = D3D11_USAGE_DYNAMIC;
    ui_bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    ui_bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    g.device->CreateBuffer(&ui_bd, nullptr, g.ui_vb.GetAddressOf());

    if (g.hwnd)
    {
#if defined(GBTG_INSPECTOR_APP)
        SetWindowTextA(g.hwnd, cfg.title);
#endif
        ShowWindow(g.hwnd, SW_SHOW);
    }
    g.ok = g.vs && g.ps && g.layout && g.cb && g.bone_cb && g.rtv && g.dsv && g.ui_vb && g.blend_alpha &&
           g.depth_off && g.depth_lequal && g.depth_lequal_ro &&
           g.clear_vs && g.clear_cb && g.clear_depth;
    if (!g.ok)
    {
        log::error("d3d11: pipeline incomplete");
        gfx_shutdown();
        return false;
    }
#if defined(GBTG_INSPECTOR_APP)
    if (g.device && g.context)
    {
        debug_ui_init(g.hwnd, g.device.Get(), g.context.Get());
    }
#endif
    return true;
}

namespace
{
void reset_profile_queries();
}

void gfx_shutdown()
{
    reset_profile_queries();
#if defined(GBTG_INSPECTOR_APP)
    debug_ui_shutdown();
#endif
    g.model_ps.Reset();
    g.specialized_pixels.clear();
    g.model_source.clear();
    g.vertex_source.clear();
    g.validation_shader.Reset();
    g.native_programs.clear();
#if GBTG_OPTIMIZE
    g.supported_words = {};
    g.supported_program = nullptr;
    g.program_memo.clear();
    g.native_pipeline = false;
    g.last_pixel = nullptr;
    g.readback_staging.Reset();
    g.retained_uploads.clear();
    g.retained_bytes = 0;
#endif
    g.native_constants.Reset();
    g.native_vs.Reset();
    g.native_ps.Reset();
    g.native_layout.Reset();
    g.native_vb.Reset();
    g.native_ib.Reset();
    g.native_ib_size = 0;
    g.native_vb_size = 0;
    g.native_material.Reset();
    g.native_blends.clear();
    g.native_depths.clear();
    g.native_rasterizers.clear();
    g.native_samplers.clear();
    g.sprites.clear();
    g.ui_vb.Reset();
    g.blend_alpha.Reset();
    g.blend_opaque.Reset();
    g.depth_off.Reset();
    g.depth_lequal_ro.Reset();
    g.depth_lequal.Reset();
    g.depth_state.Reset();
    g.rast.Reset();
    g.sampler.Reset();
    g.light_sampler.Reset();
    g.cb.Reset();
    g.layout.Reset();
    g.ps.Reset();
    g.vs.Reset();
    g.clear_vs.Reset();
    g.clear_cb.Reset();
    g.clear_depth.Reset();
    g.dsv.Reset();
    g.depth.Reset();
    g.rtv.Reset();
    g.color.Reset();
    g.swap.Reset();
    g.context.Reset();
    g.device.Reset();
    if (g.hwnd)
    {
        DestroyWindow(g.hwnd);
        g.hwnd = nullptr;
    }
    g.ok = false;
}

const char* gfx_backend_name()
{
    return "d3d11";
}

bool gfx_has_window()
{
    return g.hwnd != nullptr;
}

bool gfx_overlay_device(GfxOverlayDevice& device)
{
    if (!g.ok || g.hwnd == nullptr || !g.swap)
    {
        return false;
    }
    device = {};
    device.backend = "d3d11";
    device.window = g.hwnd;
    device.d3d11_device = g.device.Get();
    device.d3d11_context = g.context.Get();
    return true;
}

void gfx_pump(bool& quit)
{
    quit = false;
    if (!g.hwnd)
    {
        return;
    }
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    quit = g.quit;
#if defined(GBTG_INSPECTOR_APP)
    // Rebuild once after the message batch, never inside WM_SIZE while a draw may hold a target.
    if (!quit && g.ok && !inspector_window_minimized())
    {
        RECT client{};
        if (GetClientRect(g.hwnd, &client))
        {
            inspector_resize_surface(client.right - client.left, client.bottom - client.top);
        }
    }
#endif
}

namespace
{
struct ProfileQueries
{
    ComPtr<ID3D11Query> disjoint, begin, end;
    struct DrawQuery
    {
        ComPtr<ID3D11Query> begin, end;
    };
    std::vector<DrawQuery> draws;
    size_t used = 0;
    bool pending = false;
    unsigned long long frame = 0;
};
std::array<ProfileQueries, 16> profile_queries;
ProfileQueries* active_profile_query = nullptr;
void reset_profile_queries()
{
    active_profile_query = nullptr;
    for (auto& item : profile_queries)
    {
        item = {};
    }
}
void profile_gpu_begin()
{
    if (!render_profile::enabled || !g.ok)
    {
        return;
    }
    for (auto& item : profile_queries)
    {
        if (!item.pending)
        {
            continue;
        }
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
        UINT64 begin = 0, end = 0;
        if (g.context->GetData(item.disjoint.Get(), &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK &&
            g.context->GetData(item.begin.Get(), &begin, sizeof(begin), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK &&
            g.context->GetData(item.end.Get(), &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK)
        {
            if (!disjoint.Disjoint && disjoint.Frequency != 0)
            {
                render_profile::gpu_output << item.frame << ',' << double(end - begin) * 1000.0 / double(disjoint.Frequency) << ',';
                if (item.used != 0)
                {
                    UINT64 ticks = 0;
                    for (size_t n = 0; n < item.used; ++n)
                    {
                        UINT64 first = 0, last = 0;
                        if (g.context->GetData(item.draws[n].begin.Get(), &first, sizeof(first), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
                            g.context->GetData(item.draws[n].end.Get(), &last, sizeof(last), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
                        {
                            throw Error("completed GPU frame has incomplete draw timestamps");
                        }
                        ticks += last - first;
                    }
                    render_profile::gpu_output << double(ticks) * 1000.0 / double(disjoint.Frequency);
                }
                render_profile::gpu_output << '\n';
            }
            item.pending = false;
        }
    }
    auto& item = profile_queries[render_profile::number % profile_queries.size()];
    if (item.pending)
    {
        return; // Bounded ring: never stall for a GPU sample.
    }
    if (!item.disjoint)
    {
        D3D11_QUERY_DESC desc{D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
        if (FAILED(g.device->CreateQuery(&desc, item.disjoint.GetAddressOf())))
        {
            throw Error("GPU disjoint query creation failed");
        }
        desc.Query = D3D11_QUERY_TIMESTAMP;
        if (FAILED(g.device->CreateQuery(&desc, item.begin.GetAddressOf())) ||
            FAILED(g.device->CreateQuery(&desc, item.end.GetAddressOf())))
        {
            throw Error("GPU timestamp query creation failed");
        }
    }
    item.frame = render_profile::number;
    item.used = 0;
    active_profile_query = &item;
    g.context->Begin(item.disjoint.Get());
    g.context->End(item.begin.Get());
}
void profile_gpu_draw_begin()
{
    if (render_profile::enabled)
    {
        ++render_profile::frame.all_draws;
    }
    if (active_profile_query == nullptr || active_profile_query->frame % 32 != 0 || active_profile_query->used >= 4096)
    {
        return;
    }
    auto& item = *active_profile_query;
    if (item.used == item.draws.size())
    {
        ProfileQueries::DrawQuery query;
        D3D11_QUERY_DESC desc{D3D11_QUERY_TIMESTAMP, 0};
        if (FAILED(g.device->CreateQuery(&desc, query.begin.GetAddressOf())) ||
            FAILED(g.device->CreateQuery(&desc, query.end.GetAddressOf())))
        {
            throw Error("GPU draw timestamp query creation failed");
        }
        item.draws.push_back(std::move(query));
    }
    g.context->End(item.draws[item.used].begin.Get());
}
void profile_gpu_draw_end()
{
    if (active_profile_query != nullptr && active_profile_query->frame % 32 == 0 && active_profile_query->used < 4096)
    {
        auto& item = *active_profile_query;
        g.context->End(item.draws[item.used++].end.Get());
    }
}
void profile_gpu_end()
{
    if (active_profile_query != nullptr)
    {
        g.context->End(active_profile_query->end.Get());
        g.context->End(active_profile_query->disjoint.Get());
        active_profile_query->pending = true;
        active_profile_query = nullptr;
        if (!g.swap)
        {
            g.context->Flush(); // Offscreen runs otherwise need not submit each frame.
        }
    }
}
}

#if GBTG_OPTIMIZE
void age_retained_uploads();
#endif

void gfx_begin_frame()
{
    profile_gpu_begin();
    g.native_vb_cursor = 0;
    g.native_ib_cursor = 0;
#if GBTG_OPTIMIZE
    age_retained_uploads();
#endif
    if (!g.ok)
    {
        return;
    }
    const float clear[4] = {0.f, 0.f, 0.f, 1.f};
    g.context->OMSetRenderTargets(1, g.rtv.GetAddressOf(), g.dsv.Get());
    g.context->ClearRenderTargetView(g.rtv.Get(), clear);
    g.context->ClearDepthStencilView(g.dsv.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
    D3D11_VIEWPORT vp{};
    vp.Width = float(g.cfg.width);
    vp.Height = float(g.cfg.height);
    vp.MaxDepth = 1.f;
    g.context->RSSetViewports(1, &vp);
    g.context->RSSetState(g.rast.Get());
    bind_depth_shadow();
    g.context->OMSetBlendState(g.blend_opaque.Get(), nullptr, 0xFFFFFFFFu);
    g.context->IASetInputLayout(g.layout.Get());
    g.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g.context->VSSetShader(g.vs.Get(), nullptr, 0);
    g.context->PSSetShader(g.ps.Get(), nullptr, 0);
    g.context->PSSetSamplers(0, 1, g.sampler.GetAddressOf());
    g.context->PSSetSamplers(1, 1, g.light_sampler.GetAddressOf());
    ID3D11Buffer* cbs[2] = {g.cb.Get(), g.bone_cb.Get()};
    g.context->VSSetConstantBuffers(0, 2, cbs);
    g.context->PSSetConstantBuffers(0, 1, cbs);
}

// Host boundary for Xbox D3DDevice_Clear(1, rect, 3, ..., 1.f, 0).
// D24/S8 needs a raster clear: ClearView only accepts depth-only DSVs.
void gfx_clear_ui_depth(int left, int top, int right, int bottom)
{
    if (!g.ok || right <= left || bottom <= top)
    {
        return;
    }
    const float rectangle[4]
    {
        2.f * float(left) / 640.f - 1.f, 1.f - 2.f * float(top) / 480.f,
        2.f * float(right) / 640.f - 1.f, 1.f - 2.f * float(bottom) / 480.f
    };
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(g.context->Map(g.clear_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        throw Error("D3D11 UI depth-clear buffer map failed");
    }
    std::memcpy(mapped.pData, rectangle, sizeof(rectangle));
    g.context->Unmap(g.clear_cb.Get(), 0);

    ComPtr<ID3D11VertexShader> saved_vs;
    ComPtr<ID3D11PixelShader> saved_ps;
    ComPtr<ID3D11InputLayout> saved_layout;
    ComPtr<ID3D11Buffer> saved_cb;
    ComPtr<ID3D11RasterizerState> saved_rasterizer;
    ComPtr<ID3D11DepthStencilState> saved_depth;
    ComPtr<ID3D11RenderTargetView> saved_target;
    ComPtr<ID3D11DepthStencilView> saved_depth_view;
    D3D11_PRIMITIVE_TOPOLOGY saved_topology;
    UINT saved_stencil;
    D3D11_VIEWPORT saved_viewport{};
    UINT viewport_count = 1;
    g.context->VSGetShader(saved_vs.GetAddressOf(), nullptr, nullptr);
    g.context->PSGetShader(saved_ps.GetAddressOf(), nullptr, nullptr);
    g.context->IAGetInputLayout(saved_layout.GetAddressOf());
    g.context->IAGetPrimitiveTopology(&saved_topology);
    g.context->VSGetConstantBuffers(0, 1, saved_cb.GetAddressOf());
    g.context->RSGetState(saved_rasterizer.GetAddressOf());
    g.context->OMGetDepthStencilState(saved_depth.GetAddressOf(), &saved_stencil);
    g.context->OMGetRenderTargets(1, saved_target.GetAddressOf(), saved_depth_view.GetAddressOf());
    g.context->RSGetViewports(&viewport_count, &saved_viewport);

    present_hud_viewport();
    g.context->OMSetRenderTargets(0, nullptr, saved_depth_view.Get());
    g.context->OMSetDepthStencilState(g.clear_depth.Get(), 0);
    g.context->RSSetState(g.rast.Get());
    g.context->IASetInputLayout(nullptr);
    g.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g.context->VSSetShader(g.clear_vs.Get(), nullptr, 0);
    g.context->PSSetShader(nullptr, nullptr, 0);
    ID3D11Buffer* clear_buffer = g.clear_cb.Get();
    g.context->VSSetConstantBuffers(0, 1, &clear_buffer);
    profile_gpu_draw_begin();
    g.context->Draw(6, 0);
    profile_gpu_draw_end();

    ID3D11RenderTargetView* target = saved_target.Get();
    g.context->OMSetRenderTargets(1, &target, saved_depth_view.Get());
    g.context->OMSetDepthStencilState(saved_depth.Get(), saved_stencil);
    g.context->RSSetState(saved_rasterizer.Get());
    g.context->RSSetViewports(viewport_count, &saved_viewport);
    g.context->IASetInputLayout(saved_layout.Get());
    g.context->IASetPrimitiveTopology(saved_topology);
    g.context->VSSetShader(saved_vs.Get(), nullptr, 0);
    g.context->PSSetShader(saved_ps.Get(), nullptr, 0);
    ID3D11Buffer* constant_buffer = saved_cb.Get();
    g.context->VSSetConstantBuffers(0, 1, &constant_buffer);
}


namespace
{
D3D11_BLEND native_blend_factor(u32 factor, bool alpha)
{
    switch (factor)
    {
    case 0:
        return D3D11_BLEND_ZERO;
    case 1:
        return D3D11_BLEND_ONE;
    case 0x300:
        return alpha ? D3D11_BLEND_SRC_ALPHA : D3D11_BLEND_SRC_COLOR;
    case 0x301:
        return alpha ? D3D11_BLEND_INV_SRC_ALPHA : D3D11_BLEND_INV_SRC_COLOR;
    case 0x302:
        return D3D11_BLEND_SRC_ALPHA;
    case 0x303:
        return D3D11_BLEND_INV_SRC_ALPHA;
    case 0x304:
        return D3D11_BLEND_DEST_ALPHA;
    case 0x305:
        return D3D11_BLEND_INV_DEST_ALPHA;
    case 0x306:
        return alpha ? D3D11_BLEND_DEST_ALPHA : D3D11_BLEND_DEST_COLOR;
    case 0x307:
        return alpha ? D3D11_BLEND_INV_DEST_ALPHA : D3D11_BLEND_INV_DEST_COLOR;
    case 0x308:
        return D3D11_BLEND_SRC_ALPHA_SAT;
    default:
        throw Error("native draw uses blend factor " + std::to_string(factor));
    }
}

D3D11_BLEND_OP native_blend_equation(u32 equation)
{
    switch (equation)
    {
    case 0x8006:
        return D3D11_BLEND_OP_ADD;
    case 0x8007:
        return D3D11_BLEND_OP_MIN;
    case 0x8008:
        return D3D11_BLEND_OP_MAX;
    case 0x800A:
        return D3D11_BLEND_OP_SUBTRACT;
    case 0x800B:
        return D3D11_BLEND_OP_REV_SUBTRACT;
    default:
        throw Error("native draw uses blend equation " + std::to_string(equation));
    }
}

D3D11_STENCIL_OP native_stencil_op(u32 operation)
{
    switch (operation)
    {
    case 0x1E00:
        return D3D11_STENCIL_OP_KEEP;
    case 0:
        return D3D11_STENCIL_OP_ZERO;
    case 0x1E01:
        return D3D11_STENCIL_OP_REPLACE;
    case 0x1E02:
        return D3D11_STENCIL_OP_INCR_SAT;
    case 0x1E03:
        return D3D11_STENCIL_OP_DECR_SAT;
    case 0x150A:
        return D3D11_STENCIL_OP_INVERT;
    case 0x8507:
        return D3D11_STENCIL_OP_INCR;
    case 0x8508:
        return D3D11_STENCIL_OP_DECR;
    default:
        throw Error("native stencil operation " + std::to_string(operation) + " is not an NV097 operation");
    }
}

D3D11_COMPARISON_FUNC native_comparison(u32 function)
{
    if (function < 0x200 || function > 0x207)
    {
        throw Error("native draw uses comparison " + std::to_string(function));
    }
    return D3D11_COMPARISON_FUNC(function - 0x200 + 1);
}

D3D11_TEXTURE_ADDRESS_MODE native_address(u32 mode)
{
    switch (mode)
    {
    case 1:
        return D3D11_TEXTURE_ADDRESS_WRAP;
    case 2:
        return D3D11_TEXTURE_ADDRESS_MIRROR;
    case 3:
    case 5:
        return D3D11_TEXTURE_ADDRESS_CLAMP;
    case 4:
        return D3D11_TEXTURE_ADDRESS_BORDER;
    default:
        throw Error("native draw uses texture address mode " + std::to_string(mode));
    }
}

ID3D11ShaderResourceView* native_texture(const RgbaImage* image)
{
    render_profile::Scope measurement(render_profile::Texture);
    if (image == nullptr || image->bgra.empty() || image->width <= 0 || image->height <= 0)
    {
        return nullptr;
    }
    auto it = g.sprites.find(image);
    if (it == g.sprites.end() || it->second.serial != image->serial)
    {
        if (render_profile::enabled)
        {
            ++render_profile::frame.texture_uploads;
            render_profile::frame.uploaded_bytes += image->bgra.size();
        }
#if GBTG_OPTIMIZE
        // GBTG_OPTIMIZE step 5: a refreshed image of the same size (movie frames, render-target
        // captures) is written into its existing texture instead of a new one.
        if (it != g.sprites.end() && it->second.srv)
        {
            ComPtr<ID3D11Resource> resource;
            it->second.srv->GetResource(resource.GetAddressOf());
            ComPtr<ID3D11Texture2D> texture;
            D3D11_TEXTURE2D_DESC desc{};
            if (SUCCEEDED(resource.As(&texture)))
            {
                texture->GetDesc(&desc);
            }
            if (texture && desc.Width == u32(image->width) && desc.Height == u32(image->height)
                && desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM && desc.MipLevels == 1 && desc.Usage == D3D11_USAGE_DEFAULT)
            {
                g.context->UpdateSubresource(texture.Get(), 0, nullptr, image->bgra.data(), u32(image->width * 4), 0);
                it->second.serial = image->serial;
                return it->second.srv.Get();
            }
        }
#endif
        GfxD3D11::SpriteGpu gpu;
        gpu.srv = upload_bgra(image->bgra.data(), image->width, image->height);
        gpu.serial = image->serial;
        it = g.sprites.insert_or_assign(image, std::move(gpu)).first;
    }
    return it->second.srv.Get();
}

// Specialize structural words only; the material buffer still owns mutable colour constants.
// Same HLSL equations, register dependencies, alpha test and outputs as the generic reference.
ID3D11PixelShader* native_pixel_shader(const u32* definition)
{
    gfx_check_native_pixel_definition(definition);
    if (render_profile::reference)
    {
        return g.model_ps.Get();
    }
    const std::array<u32, 60> key = gfx_native_pixel_key(definition);
    const bool depth_output = gfx_native_pixel_writes_depth(key);
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 3: consecutive draws usually share the specialized shader.
    if (g.last_pixel != nullptr && g.last_pixel_key == key)
    {
        return g.last_pixel;
    }
    if (auto found = g.specialized_pixels.find(key); found != g.specialized_pixels.end())
    {
        g.last_pixel_key = key;
        g.last_pixel = found->second.Get();
        return g.last_pixel;
    }
#endif
    auto found = g.specialized_pixels.find(key);
    if (found != g.specialized_pixels.end())
    {
        return found->second.Get();
    }
    render_profile::Scope compilation(render_profile::Compilation);
    const std::string source = gfx_specialize_native_pixel_source(g.model_source, key);
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3D11PixelShader> shader;
    if (!compile_shader(source.c_str(), depth_output ? "ps_model_depth" : "ps_model", "ps_4_0", blob) ||
        FAILED(g.device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, shader.GetAddressOf())))
    {
        if (depth_output)
        {
            throw Error("native depth-replacing pixel shader failed to compile");
        }
        // Explicit fallback to the unchanged generic implementation, including its error checks.
        shader = g.model_ps;
    }
    if (render_profile::enabled)
    {
        ++render_profile::frame.pixel_misses;
    }
    return g.specialized_pixels.emplace(key, shader).first->second.Get();
}

// The HUD viewport maps the 640x480 back buffer onto the window; rectangles follow it.
D3D11_RECT native_rectangle(const std::array<i32, 4>& rectangle)
{
    D3D11_VIEWPORT vp{};
    UINT count = 1;
    g.context->RSGetViewports(&count, &vp);
    D3D11_RECT output;
    output.left = LONG(std::floor(vp.TopLeftX + vp.Width * float(rectangle[0]) / 640.f));
    output.top = LONG(std::floor(vp.TopLeftY + vp.Height * float(rectangle[1]) / 480.f));
    output.right = LONG(std::ceil(vp.TopLeftX + vp.Width * float(rectangle[2]) / 640.f));
    output.bottom = LONG(std::ceil(vp.TopLeftY + vp.Height * float(rectangle[3]) / 480.f));
    return output;
}
}

bool gfx_native_vertex_program_supported(std::span<const u32> program)
{
    if (!g.ok || render_profile::reference || render_profile::cpu_vertex)
    {
        return false;
    }
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 13: the memo answers before the map's whole-program comparisons.
    if (const auto memo = g.program_memo.find(program.data()); memo != g.program_memo.end()
        && memo->second.words.size() == program.size()
        && ((g.native_batch && memo->second.batch == g.native_batch_serial)
            || std::memcmp(memo->second.words.data(), program.data(), program.size_bytes()) == 0))
    {
        memo->second.batch = g.native_batch_serial;
        g.supported_words = program;
        g.supported_program = memo->second.program;
        return memo->second.program->shader != nullptr;
    }
#endif
    auto found = g.native_programs.find(program);
    if (found != g.native_programs.end())
    {
#if GBTG_OPTIMIZE
        // GBTG_OPTIMIZE step 2: the draw that follows reuses this lookup.
        g.supported_words = program;
        g.supported_program = &found->second;
        if (g.program_memo.size() >= 4096)
        {
            g.program_memo.clear();
        }
        g.program_memo[program.data()] = {std::vector<u32>(program.begin(), program.end()), &found->second,
            g.native_batch_serial};
#endif
        return found->second.shader != nullptr;
    }
    render_profile::Scope compilation(render_profile::Compilation);
    GfxD3D11::ProgramGpu gpu;
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
        const auto source = g.vertex_source + body;
        ComPtr<ID3DBlob> blob;
        if (compile_shader(source.c_str(), "program_vs", "vs_4_0", blob, true))
        {
            std::array<D3D11_INPUT_ELEMENT_DESC, 16> elements{};
            for (UINT n = 0; n < elements.size(); ++n)
            {
                elements[n] = {"TEXCOORD", n, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, n * 16, D3D11_INPUT_PER_VERTEX_DATA, 0};
            }
            if (FAILED(g.device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, gpu.shader.GetAddressOf())) ||
                FAILED(g.device->CreateInputLayout(elements.data(), UINT(elements.size()), blob->GetBufferPointer(), blob->GetBufferSize(), gpu.layout.GetAddressOf())))
            {
                gpu = {};
            }
        }
    }
    const bool supported = gpu.shader != nullptr;
#if GBTG_OPTIMIZE
    program_constant_range(program, gpu.constants_first, gpu.constants_count);
#endif
    g.native_programs.emplace(std::vector<u32>(program.begin(), program.end()), std::move(gpu));
    return supported;
}

bool gfx_prepare_native_pixel_definition(const u32* definition)
{
    if (!g.ok)
    {
        return false;
    }
    return native_pixel_shader(definition) != g.model_ps.Get();
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
        if (gpu.shader)
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

namespace
{
void validate_native_gpu_vertices(const GfxNativeDraw& draw, GfxD3D11::ProgramGpu& program)
{
    if (!render_profile::validate_vertex || program.validated)
    {
        return;
    }
    if (!g.validation_shader)
    {
        const std::string source = g.vertex_source + R"(
[maxvertexcount(1)]
void validate_gs(point VSOut input[1], inout PointStream<VSOut> stream)
{
    stream.Append(input[0]);
}
)";
        ComPtr<ID3DBlob> blob;
        if (!compile_shader(source.c_str(), "validate_gs", "gs_4_0", blob))
        {
            throw Error("GPU validation stream shader compilation failed");
        }
        const D3D11_SO_DECLARATION_ENTRY entries[] =
        {
            {0, "SV_Position", 0, 0, 4, 0},
            {0, "COLOR", 0, 0, 4, 0}, {0, "COLOR", 1, 0, 4, 0},
            {0, "TEXCOORD", 0, 0, 2, 0}, {0, "TEXCOORD", 1, 0, 2, 0},
            {0, "TEXCOORD", 3, 0, 2, 0}, {0, "TEXCOORD", 4, 0, 2, 0}, {0, "TEXCOORD", 7, 0, 1, 0}
        };
        const UINT stride = sizeof(GfxNativeVertex);
        if (FAILED(g.device->CreateGeometryShaderWithStreamOutput(blob->GetBufferPointer(), blob->GetBufferSize(),
            entries, UINT(std::size(entries)), &stride, 1, D3D11_SO_NO_RASTERIZED_STREAM, nullptr, g.validation_shader.GetAddressOf())))
        {
            throw Error("GPU validation stream shader creation failed");
        }
    }
    const UINT count = UINT(std::min<size_t>(draw.program_inputs.size(), 16));
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = count * sizeof(GfxNativeVertex);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_STREAM_OUTPUT;
    ComPtr<ID3D11Buffer> stream, staging;
    if (FAILED(g.device->CreateBuffer(&desc, nullptr, stream.GetAddressOf())))
    {
        throw Error("GPU validation stream buffer creation failed");
    }
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(g.device->CreateBuffer(&desc, nullptr, staging.GetAddressOf())))
    {
        throw Error("GPU validation staging buffer creation failed");
    }
    UINT offset = 0;
    g.context->GSSetShader(g.validation_shader.Get(), nullptr, 0);
    g.context->SOSetTargets(1, stream.GetAddressOf(), &offset);
    g.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    g.context->Draw(count, 0);
    ID3D11Buffer* empty = nullptr;
    g.context->SOSetTargets(1, &empty, &offset);
    g.context->GSSetShader(nullptr, nullptr, 0);
    g.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g.context->CopyResource(staging.Get(), stream.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(g.context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
    {
        throw Error("GPU validation readback failed");
    }
    std::vector<GfxNativeVertex> actual(count);
    std::memcpy(actual.data(), mapped.pData, desc.ByteWidth);
    g.context->Unmap(staging.Get(), 0);
    float maximum = 0;
    for (UINT n = 0; n < count; ++n)
    {
        XboxVertexProgramState reference{};
        reference.constants = *draw.vertex_constants;
        reference.inputs = draw.program_inputs[n];
        reference.outputs.fill({0.f, 0.f, 0.f, 1.f});
        reference.outputs[3] = {1.f, 1.f, 1.f, 1.f};
        reference.outputs[4] = {0.f, 0.f, 0.f, 0.f};
        run_xbox_vertex_program(draw.vertex_program, reference);
        GfxNativeVertex expected{};
        const auto& p = reference.outputs[0];
        expected.position = {(p[0] / 320.f - 1.f) * p[3], (1.f - p[1] / 240.f) * p[3], p[2] / 16777215.f * p[3], p[3]};
        for (size_t c = 0; c < 4; ++c)
        {
            expected.diffuse[c] = std::clamp(reference.outputs[3][c], 0.f, 1.f);
            expected.specular[c] = std::clamp(reference.outputs[4][c], 0.f, 1.f);
            expected.uv[c] = {reference.outputs[9 + c][0], reference.outputs[9 + c][1]};
        }
        expected.fog = std::clamp(reference.outputs[5][0], 0.f, 1.f);
        // The stream-out writes position, both colours, the four UVs and fog: the vertex up to uv_zw.
        std::array<float, 21> a{}, b{};
        static_assert(offsetof(GfxNativeVertex, uv_zw) == sizeof(a));
        std::memcpy(a.data(), &expected, sizeof(a));
        std::memcpy(b.data(), &actual[n], sizeof(b));
        for (size_t c = 0; c < a.size(); ++c)
        {
            if (a[c] == b[c] || (std::isnan(a[c]) && std::isnan(b[c])))
            {
                continue;
            }
            const float difference = std::fabs(a[c] - b[c]);
            if (!std::isfinite(difference) || difference > 0.00005f + 0.00005f * std::fabs(a[c]))
            {
                throw Error("GPU vertex reference mismatch: component " + std::to_string(c) + " expected " + std::to_string(a[c]) + " actual " + std::to_string(b[c]));
            }
            maximum = std::max(maximum, difference);
        }
    }
    program.validated = true;
    log::info("GPU vertex reference PASS vertices=%u instructions=%u max_abs_error=%g tolerance=5e-5+5e-5*abs(reference)", count, draw.vertex_program[0] >> 16, maximum);
}
}

void gfx_begin_native_batch()
{
    g.native_batch = !render_profile::reference;
#if GBTG_OPTIMIZE
    ++g.native_batch_serial;
#endif
}
void gfx_end_native_batch()
{
    g.native_batch = false;
    if (!g.native_pipeline)
    {
        return;
    }
    // Put the shared host pipeline back for the draws that follow.
    g.context->RSSetState(g.rast.Get());
    g.context->IASetInputLayout(g.layout.Get());
    g.context->VSSetShader(g.vs.Get(), nullptr, 0);
    g.context->PSSetShader(g.ps.Get(), nullptr, 0);
    ID3D11SamplerState* shared_samplers[2]{g.sampler.Get(), g.light_sampler.Get()};
    g.context->PSSetSamplers(0, 2, shared_samplers);
    bind_depth_shadow();
    g.native_pipeline = false;
}

void gfx_validate_native_boundary()
{
    const GfxConfig config{64, 64, 0, false};
    const auto initialize = [&]()
    {
        if (!d3d11::gfx_init(config))
        {
            throw Error("boundary validation device creation failed");
        }
        gfx_begin_frame();
    };
    initialize();
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
        gfx_draw_native(draw);
        RgbaImage image;
        if (!gfx_readback(image))
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
    d3d11::gfx_release_sprite(texture);
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
    // Arithmetic edge cases are reference-read back only in this validation mode.
    render_profile::validate_vertex = true;
    std::array<GfxNativeInputs, 6> edge_inputs{};
    for (size_t n = 0; n < edge_inputs.size(); ++n)
    {
        edge_inputs[n][0] = {320.f, 240.f, 0.f, 1.f};
        edge_inputs[n][1][0] = n % 3 == 0 ? 0.f : n % 3 == 1 ? -0.f : 1e-30f;
    }
    constants[96] = {5e-20f, 5e-20f, 5e-20f, 5e-20f};
    for (const char* operation : {"rcc", "rsq"})
    {
        const std::string source = std::string("vs.1.0\n#pragma screenspace\n") + operation + " r0, v1.x\nmul oD0, r0, c0\nmov oPos, v0\n";
        const auto edge_program = assemble_xg_vertex_shader(source);
        if (!gfx_native_vertex_program_supported(edge_program))
        {
            throw Error("GPU arithmetic fixture unexpectedly fell back");
        }
        draw.vertex_program = edge_program;
        draw.program_inputs = edge_inputs;
        gfx_draw_native(draw);
    }
    d3d11::gfx_release_sprite(texture);
    gfx_shutdown();
    initialize();
    draw = {};
    draw.vertices = vertices;
    draw.indices = indices;
    expect(255, 255, 255);
    gfx_shutdown();
    log::info("Native boundary PASS: vertex/index mutation, texture serial/release/address reuse, constants, device unload/reload");
}

#if GBTG_OPTIMIZE
constexpr size_t kRetainedUploadBytes = size_t(256) << 20;
constexpr u64 kRetainedUploadIdleFrames = 600;

// GBTG_OPTIMIZE step 7: the immutable buffers for a retained geometry id, created on its first draw.
GfxD3D11::RetainedUpload& retained_upload(const GfxNativeDraw& draw)
{
    auto& uploads = g.retained_uploads;
    auto found = uploads.find(draw.retained_geometry);
    if (found != uploads.end())
    {
        if (render_profile::enabled)
        {
            // Counted as uploaded at the top of gfx_draw_native; this draw uploads nothing.
            render_profile::frame.uploaded_bytes -= draw.program_inputs.size_bytes() + draw.indices.size_bytes();
        }
        found->second.last_used = g.frame_number;
        return found->second;
    }
    GfxD3D11::RetainedUpload upload;
    const auto create = [](const void* data, size_t bytes, UINT bind, ComPtr<ID3D11Buffer>& buffer)
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = UINT(bytes);
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = bind;
        D3D11_SUBRESOURCE_DATA initial{};
        initial.pSysMem = data;
        if (FAILED(g.device->CreateBuffer(&desc, &initial, buffer.GetAddressOf())))
        {
            throw Error("D3D11 retained native geometry buffer creation failed");
        }
        if (render_profile::enabled)
        {
            ++render_profile::frame.buffer_allocations;
        }
    };
    create(draw.program_inputs.data(), draw.program_inputs.size_bytes(), D3D11_BIND_VERTEX_BUFFER, upload.vertices);
    create(draw.indices.data(), draw.indices.size_bytes(), D3D11_BIND_INDEX_BUFFER, upload.indices);
    upload.bytes = draw.program_inputs.size_bytes() + draw.indices.size_bytes();
    upload.last_used = g.frame_number;
    if (g.retained_bytes + upload.bytes > kRetainedUploadBytes)
    {
        uploads.clear();
        g.retained_bytes = 0;
    }
    g.retained_bytes += upload.bytes;
    return uploads.emplace(draw.retained_geometry, std::move(upload)).first->second;
}

// GBTG_OPTIMIZE step 20: the program of a draw that gfx_native_vertex_program_supported did not just answer for.
// On the render thread every draw is such a draw: the wrapper asks once per program and draws carry its interned
// copy of the words, whose address is stable. The step 13 memo by address answers before the whole-program map.
GfxD3D11::ProgramGpu* draw_program(std::span<const u32> program)
{
    const auto memo = g.program_memo.find(program.data());
    if (memo != g.program_memo.end() && memo->second.words.size() == program.size()
        && ((g.native_batch && memo->second.batch == g.native_batch_serial)
            || std::memcmp(memo->second.words.data(), program.data(), program.size_bytes()) == 0))
    {
        memo->second.batch = g.native_batch_serial;
        return memo->second.program;
    }
    GfxD3D11::ProgramGpu* found = &g.native_programs.find(program)->second;
    if (g.program_memo.size() >= 4096)
    {
        g.program_memo.clear();
    }
    g.program_memo[program.data()] = {std::vector<u32>(program.begin(), program.end()), found,
        g.native_batch_serial};
    return found;
}

// gfx_draw_native's dynamic vertex upload (the #else branch there): returns the offset in native_vb.
UINT upload_native_vertices(const void* data, UINT bytes)
{
    if (!g.native_vb || g.native_vb_size < bytes)
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = std::max<UINT>(bytes, 0x400000);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        g.native_vb.Reset();
        if (FAILED(g.device->CreateBuffer(&desc, nullptr, g.native_vb.GetAddressOf())))
        {
            throw Error("D3D11 native vertex buffer creation failed");
        }
        g.native_vb_size = desc.ByteWidth;
        g.native_vb_cursor = 0;
        if (render_profile::enabled)
        {
            ++render_profile::frame.buffer_allocations;
        }
    }
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (render_profile::reference || bytes > g.native_vb_size - g.native_vb_cursor)
    {
        g.native_vb_cursor = 0;
    }
    const UINT vertex_offset = g.native_vb_cursor;
    if (FAILED(g.context->Map(g.native_vb.Get(), 0,
        vertex_offset == 0 ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped)))
    {
        throw Error("D3D11 native vertex buffer map failed");
    }
    std::memcpy(static_cast<u8*>(mapped.pData) + vertex_offset, data, bytes);
    g.native_vb_cursor += bytes;
    g.context->Unmap(g.native_vb.Get(), 0);
    return vertex_offset;
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
            g.retained_bytes -= upload->second.bytes;
            upload = g.retained_uploads.erase(upload);
        }
        else
        {
            ++upload;
        }
    }
}
#endif

void gfx_draw_native(const GfxNativeDraw& draw)
{
    render_profile::Scope measurement(render_profile::Submission);
    if (render_profile::enabled)
    {
        ++render_profile::frame.draws;
        render_profile::frame.output_vertices += draw.indices.empty() ? draw.vertices.size() : draw.indices.size();
        render_profile::frame.uploaded_bytes += draw.vertices.size_bytes() + draw.program_inputs.size_bytes() + draw.indices.size_bytes();
    }
    if (!g.ok || (draw.vertices.empty() && draw.program_inputs.empty()))
    {
        return;
    }
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 2: after another native draw of this batch the pipeline is as `bound` says.
    auto& bound = g.native_bound;
    const bool reuse = g.native_pipeline && !render_profile::validate_vertex;
    if (!reuse)
    {
        bound = {};
        present_hud_viewport();
        UINT viewports = 1;
        g.context->RSGetViewports(&viewports, &bound.viewport);
    }
    bool cb_current = reuse;
#else
    present_hud_viewport();
#endif
    const bool program_draw = !draw.program_inputs.empty();
    const UINT bytes = UINT(program_draw ? draw.program_inputs.size_bytes() : draw.vertices.size_bytes());
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 7: retained geometry draws from its own buffers; the ring keeps the rest.
    GfxD3D11::RetainedUpload* retained = program_draw && draw.retained_geometry != 0 && !draw.indices.empty()
        ? &retained_upload(draw) : nullptr;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    UINT vertex_offset = 0;
    if (retained == nullptr)
    {
        vertex_offset = upload_native_vertices(program_draw ? static_cast<const void*>(draw.program_inputs.data())
                                                            : static_cast<const void*>(draw.vertices.data()), bytes);
    }
#else
    if (!g.native_vb || g.native_vb_size < bytes)
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = std::max<UINT>(bytes, 0x400000);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        g.native_vb.Reset();
        if (FAILED(g.device->CreateBuffer(&desc, nullptr, g.native_vb.GetAddressOf())))
        {
            throw Error("D3D11 native vertex buffer creation failed");
        }
        g.native_vb_size = desc.ByteWidth;
        g.native_vb_cursor = 0;
        if (render_profile::enabled)
        {
            ++render_profile::frame.buffer_allocations;
        }
    }
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (render_profile::reference || bytes > g.native_vb_size - g.native_vb_cursor)
    {
        g.native_vb_cursor = 0;
    }
    const UINT vertex_offset = g.native_vb_cursor;
    if (FAILED(g.context->Map(g.native_vb.Get(), 0,
        vertex_offset == 0 ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped)))
    {
        throw Error("D3D11 native vertex buffer map failed");
    }
    std::memcpy(static_cast<u8*>(mapped.pData) + vertex_offset, program_draw ? static_cast<const void*>(draw.program_inputs.data()) : static_cast<const void*>(draw.vertices.data()), bytes);
    g.native_vb_cursor += bytes;
    g.context->Unmap(g.native_vb.Get(), 0);
#endif
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 2: the shared constant buffer is rewritten only when it changes.
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
    if (!cb_current || std::memcmp(&bound.cb, &cb, sizeof(cb)) != 0)
    {
        if (FAILED(g.context->Map(g.cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            throw Error("D3D11 native constant buffer map failed");
        }
        std::memcpy(mapped.pData, &cb, sizeof(cb));
        g.context->Unmap(g.cb.Get(), 0);
        bound.cb = cb;
    }
#else
    if (FAILED(g.context->Map(g.cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        throw Error("D3D11 native constant buffer map failed");
    }
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
    std::memcpy(mapped.pData, &cb, sizeof(cb));
    g.context->Unmap(g.cb.Get(), 0);
#endif

    const u32 mask = ((draw.color_mask >> 24) & 1) | ((draw.color_mask >> 15) & 2) | ((draw.color_mask >> 6) & 4) |
        ((draw.color_mask << 3) & 8);
    const u64 blend_key = u64(draw.blend) | (u64(draw.source_blend & 0xFFFF) << 1) |
        (u64(draw.destination_blend & 0xFFFF) << 17) | (u64(draw.blend_equation & 0xFFFF) << 33) | (u64(mask) << 49);
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 18: a state object found for the previous draw of the batch under the same key is that
    // draw's; the maps are searched only when a key changes.
    const bool states_known = reuse && bound.states;
    ID3D11BlendState* blend_state = states_known && bound.blend_key == blend_key ? bound.blend : nullptr;
    if (blend_state == nullptr)
    {
#endif
    auto blend = g.native_blends.find(blend_key);
    if (blend == g.native_blends.end())
    {
        D3D11_BLEND_DESC desc{};
        if (render_profile::enabled)
        {
            ++render_profile::frame.state_misses;
        }
        auto& target = desc.RenderTarget[0];
        target.BlendEnable = draw.blend ? TRUE : FALSE;
        target.SrcBlend = native_blend_factor(draw.source_blend, false);
        target.DestBlend = native_blend_factor(draw.destination_blend, false);
        target.BlendOp = native_blend_equation(draw.blend_equation);
        target.SrcBlendAlpha = native_blend_factor(draw.source_blend, true);
        target.DestBlendAlpha = native_blend_factor(draw.destination_blend, true);
        target.BlendOpAlpha = target.BlendOp;
        target.RenderTargetWriteMask = u8(((draw.color_mask >> 16) & 1 ? D3D11_COLOR_WRITE_ENABLE_RED : 0) |
            ((draw.color_mask >> 8) & 1 ? D3D11_COLOR_WRITE_ENABLE_GREEN : 0) |
            (draw.color_mask & 1 ? D3D11_COLOR_WRITE_ENABLE_BLUE : 0) |
            ((draw.color_mask >> 24) & 1 ? D3D11_COLOR_WRITE_ENABLE_ALPHA : 0));
        ComPtr<ID3D11BlendState> state;
        if (FAILED(g.device->CreateBlendState(&desc, state.GetAddressOf())))
        {
            throw Error("D3D11 native blend state creation failed");
        }
        blend = g.native_blends.emplace(blend_key, state).first;
    }
#if GBTG_OPTIMIZE
    blend_state = blend->second.Get();
    }
#endif
    const std::array<u32, 9> depth_key{(draw.depth_test ? 1u : 0u) | (draw.depth_write ? 2u : 0u)
        | (draw.depth_function << 2), draw.stencil_test ? 1u : 0u, draw.stencil_function, draw.stencil_read_mask,
        draw.stencil_write_mask, draw.stencil_fail, draw.stencil_depth_fail, draw.stencil_pass, 0};
#if GBTG_OPTIMIZE
    ID3D11DepthStencilState* depth_state = states_known && bound.depth_key == depth_key ? bound.depth : nullptr;
    if (depth_state == nullptr)
    {
#endif
    auto depth = g.native_depths.find(depth_key);
    if (depth == g.native_depths.end())
    {
        D3D11_DEPTH_STENCIL_DESC desc{};
        if (render_profile::enabled)
        {
            ++render_profile::frame.state_misses;
        }
        // NV2A writes depth only while its test is enabled.
        desc.DepthEnable = draw.depth_test ? TRUE : FALSE;
        desc.DepthWriteMask = draw.depth_write ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = native_comparison(draw.depth_function);
        if (draw.stencil_test)
        {
            desc.StencilEnable = TRUE;
            desc.StencilReadMask = u8(draw.stencil_read_mask);
            desc.StencilWriteMask = u8(draw.stencil_write_mask);
            desc.FrontFace.StencilFunc = native_comparison(draw.stencil_function);
            desc.FrontFace.StencilFailOp = native_stencil_op(draw.stencil_fail);
            desc.FrontFace.StencilDepthFailOp = native_stencil_op(draw.stencil_depth_fail);
            desc.FrontFace.StencilPassOp = native_stencil_op(draw.stencil_pass);
            desc.BackFace = desc.FrontFace;
        }
        ComPtr<ID3D11DepthStencilState> state;
        if (FAILED(g.device->CreateDepthStencilState(&desc, state.GetAddressOf())))
        {
            throw Error("D3D11 native depth state creation failed");
        }
        depth = g.native_depths.emplace(depth_key, state).first;
    }
#if GBTG_OPTIMIZE
    depth_state = depth->second.Get();
    }
#endif
    const u64 rasterizer_key = u64(draw.cull) | (u64(u32(draw.z_bias)) << 32);
#if GBTG_OPTIMIZE
    ID3D11RasterizerState* rasterizer_state = states_known && bound.rasterizer_key == rasterizer_key
        ? bound.rasterizer : nullptr;
    if (rasterizer_state == nullptr)
    {
#endif
    auto rasterizer = g.native_rasterizers.find(rasterizer_key);
    if (rasterizer == g.native_rasterizers.end())
    {
        D3D11_RASTERIZER_DESC desc{};
        if (render_profile::enabled)
        {
            ++render_profile::frame.state_misses;
        }
        desc.FillMode = D3D11_FILL_SOLID;
        // Retail 0x00171420 emits CULL_FACE FRONT when CullMode == FrontFace (0x900).
        // xemu's SETUPRASTER front-face bit is clear for this game; its GL backend reverses
        // winding for inverted clip Y. Keeping Xbox screen orientation makes D3D11's
        // default clockwise front face equivalent, so 0x900 culls FRONT and 0x901 BACK.
        switch (draw.cull)
        {
        case 0:
            desc.CullMode = D3D11_CULL_NONE;
            break;
        case 0x900:
            desc.CullMode = D3D11_CULL_FRONT;
            break;
        case 0x901:
            desc.CullMode = D3D11_CULL_BACK;
            break;
        default:
            throw Error("native draw uses cull mode " + std::to_string(draw.cull));
        }
        desc.DepthClipEnable = TRUE;
        desc.ScissorEnable = TRUE;
        // NV2A polygon offset: units -bias (the depth buffer's resolvable step) and factor -0.25 bias.
        desc.DepthBias = -draw.z_bias;
        desc.SlopeScaledDepthBias = float(draw.z_bias) * -0.25f;
        ComPtr<ID3D11RasterizerState> state;
        if (FAILED(g.device->CreateRasterizerState(&desc, state.GetAddressOf())))
        {
            throw Error("D3D11 native rasterizer state creation failed");
        }
        rasterizer = g.native_rasterizers.emplace(rasterizer_key, state).first;
    }
#if GBTG_OPTIMIZE
    rasterizer_state = rasterizer->second.Get();
    }
#endif
    ID3D11ShaderResourceView* views[4]{};
    ID3D11SamplerState* samplers[4]{};
    for (size_t stage = 0; stage < 4; ++stage)
    {
#if GBTG_OPTIMIZE
        // GBTG_OPTIMIZE step 18: the view of an image the previous draw bound at this stage, at the same serial,
        // is the view native_texture would find. gfx_release_sprite forgets the images.
        const RgbaImage* image = draw.textures[stage];
        if (reuse && image != nullptr && image == bound.images[stage] && image->serial == bound.image_serials[stage]
            && !image->bgra.empty() && image->width > 0 && image->height > 0)
        {
            views[stage] = bound.views[stage];
        }
        else
        {
            views[stage] = native_texture(image);
            bound.images[stage] = views[stage] != nullptr ? image : nullptr;
            bound.image_serials[stage] = image != nullptr ? image->serial : 0;
        }
        const u32 sampler_key = draw.address_u[stage] | (draw.address_v[stage] << 8);
        if (states_known && bound.sampler_keys[stage] == sampler_key)
        {
            samplers[stage] = bound.samplers[stage];
            continue;
        }
        bound.sampler_keys[stage] = sampler_key;
#else
        views[stage] = native_texture(draw.textures[stage]);
        const u32 sampler_key = draw.address_u[stage] | (draw.address_v[stage] << 8);
#endif
        auto sampler = g.native_samplers.find(sampler_key);
        if (sampler == g.native_samplers.end())
        {
            D3D11_SAMPLER_DESC desc{};
            if (render_profile::enabled)
            {
                ++render_profile::frame.state_misses;
            }
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            desc.AddressU = native_address(draw.address_u[stage]);
            desc.AddressV = native_address(draw.address_v[stage]);
            desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.MaxLOD = D3D11_FLOAT32_MAX;
            ComPtr<ID3D11SamplerState> state;
            if (FAILED(g.device->CreateSamplerState(&desc, state.GetAddressOf())))
            {
                throw Error("D3D11 native sampler creation failed");
            }
            sampler = g.native_samplers.emplace(sampler_key, state).first;
        }
        samplers[stage] = sampler->second.Get();
    }
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 2: the same pipeline as the #else path, binding and uploading only what
    // differs from the previous native draw of this batch.
    ID3D11PixelShader* pixel_shader = g.native_ps.Get();
    if (draw.pixel_shader != nullptr)
    {
        // Combiner words, then stage n samples texture coordinate n, and v0/v1 are the vertex colours.
        std::array<u32, 80> words{};
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
        // GBTG_OPTIMIZE step 18: the same 60 definition words select the same specialized shader, and already
        // passed gfx_check_native_pixel_definition. The generic fallback is not remembered.
        const bool same_definition = bound.material && bound.definition_shader != nullptr
            && std::memcmp(bound.words.data(), words.data(), 60 * sizeof(u32)) == 0;
        if (!bound.material || bound.words != words)
        {
            if (FAILED(g.context->Map(g.native_material.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            {
                throw Error("D3D11 native material map failed");
            }
            std::memcpy(mapped.pData, words.data(), sizeof(words));
            g.context->Unmap(g.native_material.Get(), 0);
            if (!bound.material)
            {
                ID3D11Buffer* material = g.native_material.Get();
                g.context->PSSetConstantBuffers(2, 1, &material);
            }
            bound.material = true;
            bound.words = words;
        }
        if (same_definition)
        {
            pixel_shader = bound.definition_shader;
        }
        else
        {
            pixel_shader = native_pixel_shader(draw.pixel_shader);
            bound.definition_shader = pixel_shader != g.model_ps.Get() ? pixel_shader : nullptr;
        }
    }
    if (bound.pixel_shader != pixel_shader)
    {
        g.context->PSSetShader(pixel_shader, nullptr, 0);
        bound.pixel_shader = pixel_shader;
    }
    // native_rectangle, over the viewport read when the batch's pipeline was first bound.
    const D3D11_VIEWPORT& vp = bound.viewport;
    D3D11_RECT scissor;
    scissor.left = LONG(std::floor(vp.TopLeftX + vp.Width * float(draw.scissor[0]) / 640.f));
    scissor.top = LONG(std::floor(vp.TopLeftY + vp.Height * float(draw.scissor[1]) / 480.f));
    scissor.right = LONG(std::ceil(vp.TopLeftX + vp.Width * float(draw.scissor[2]) / 640.f));
    scissor.bottom = LONG(std::ceil(vp.TopLeftY + vp.Height * float(draw.scissor[3]) / 480.f));
    if (!reuse || std::memcmp(&bound.scissor, &scissor, sizeof(scissor)) != 0)
    {
        g.context->RSSetScissorRects(1, &scissor);
        bound.scissor = scissor;
    }
    if (!reuse || std::memcmp(bound.views.data(), views, sizeof(views)) != 0)
    {
        g.context->PSSetShaderResources(0, 4, views);
        std::memcpy(bound.views.data(), views, sizeof(views));
    }
    if (!reuse || std::memcmp(bound.samplers.data(), samplers, sizeof(samplers)) != 0)
    {
        g.context->PSSetSamplers(0, 4, samplers);
        std::memcpy(bound.samplers.data(), samplers, sizeof(samplers));
    }
    if (bound.blend != blend_state)
    {
        g.context->OMSetBlendState(blend_state, nullptr, 0xFFFFFFFFu);
        bound.blend = blend_state;
    }
    if (bound.depth != depth_state || bound.stencil_reference != (draw.stencil_reference & 0xFF))
    {
        g.context->OMSetDepthStencilState(depth_state, draw.stencil_reference & 0xFF);
        bound.depth = depth_state;
        bound.stencil_reference = draw.stencil_reference & 0xFF;
    }
    if (bound.rasterizer != rasterizer_state)
    {
        g.context->RSSetState(rasterizer_state);
        bound.rasterizer = rasterizer_state;
    }
    bound.blend_key = blend_key;
    bound.depth_key = depth_key;
    bound.rasterizer_key = rasterizer_key;
    bound.states = true;
    const UINT stride = program_draw ? sizeof(GfxNativeInputs) : sizeof(GfxNativeVertex);
    const UINT offset = vertex_offset;
    const D3D11_PRIMITIVE_TOPOLOGY topology = draw.lines ? D3D11_PRIMITIVE_TOPOLOGY_LINELIST
        : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    if (bound.topology != topology)
    {
        g.context->IASetPrimitiveTopology(topology);
        bound.topology = topology;
    }
    ID3D11Buffer* vertex_buffer = retained != nullptr ? retained->vertices.Get() : g.native_vb.Get();
    g.context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
    ID3D11InputLayout* layout = g.native_layout.Get();
    ID3D11VertexShader* vertex_shader = g.native_vs.Get();
    GfxD3D11::ProgramGpu* found = nullptr;
    if (program_draw)
    {
        found = g.supported_program;
        if (found == nullptr || g.supported_words.data() != draw.vertex_program.data()
            || g.supported_words.size() != draw.vertex_program.size())
        {
            found = draw_program(draw.vertex_program);
        }
        g.supported_program = nullptr;
        auto& program = *found;
        layout = program.layout.Get();
        vertex_shader = program.shader.Get();
        if (draw.vertex_constants == nullptr)
        {
            throw Error("native vertex constants map failed");
        }
        // GBTG_OPTIMIZE step 18: only the registers the program reads are compared and uploaded (the Vulkan
        // step 10 range). The buffer is skipped when it already holds this draw's values for all of them;
        // WRITE_DISCARD leaves the other registers undefined, and no instruction of the program reads them.
        // Bitwise, so a signed zero or NaN change still uploads.
        const u32 first = program.constants_first;
        const u32 count = program.constants_count;
        const size_t register_bytes = sizeof((*draw.vertex_constants)[0]);
        const bool held = bound.constants && first >= bound.constants_first
            && first + count <= bound.constants_first + bound.constants_count;
        // Step 21: the same id is the same constants, so the comparison is skipped.
        const bool same_id = draw.vertex_constants_id != 0 && draw.vertex_constants_id == bound.constants_id;
        if (!held || (!same_id && std::memcmp(bound.vertex_constants.data() + first,
            draw.vertex_constants->data() + first, count * register_bytes) != 0))
        {
            if (FAILED(g.context->Map(g.native_constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            {
                throw Error("native vertex constants map failed");
            }
            std::memcpy(static_cast<u8*>(mapped.pData) + first * register_bytes,
                draw.vertex_constants->data() + first, count * register_bytes);
            g.context->Unmap(g.native_constants.Get(), 0);
            if (!bound.constants)
            {
                ID3D11Buffer* constants = g.native_constants.Get();
                g.context->VSSetConstantBuffers(3, 1, &constants);
            }
            bound.constants = true;
            bound.constants_first = first;
            bound.constants_count = count;
            std::memcpy(bound.vertex_constants.data() + first, draw.vertex_constants->data() + first,
                count * register_bytes);
        }
        // The held registers are this draw's either way; another id with the same values names them too.
        bound.constants_id = draw.vertex_constants_id;
    }
    if (bound.layout != layout)
    {
        g.context->IASetInputLayout(layout);
        bound.layout = layout;
    }
    if (bound.vertex_shader != vertex_shader)
    {
        g.context->VSSetShader(vertex_shader, nullptr, 0);
        bound.vertex_shader = vertex_shader;
    }
    if (found != nullptr)
    {
        validate_native_gpu_vertices(draw, *found);
    }
#else
    if (draw.pixel_shader != nullptr)
    {
        if (FAILED(g.context->Map(g.native_material.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            throw Error("D3D11 native material map failed");
        }
        // Combiner words, then stage n samples texture coordinate n, and v0/v1 are the vertex colours.
        std::array<u32, 80> words{};
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
        std::memcpy(mapped.pData, words.data(), sizeof(words));
        g.context->Unmap(g.native_material.Get(), 0);
        ID3D11Buffer* material = g.native_material.Get();
        g.context->PSSetConstantBuffers(2, 1, &material);
        g.context->PSSetShader(native_pixel_shader(draw.pixel_shader), nullptr, 0);
    }
    else
    {
        // A linear texture 0 takes its texel coordinates through native_uv_scale.
        g.context->PSSetShader(g.native_ps.Get(), nullptr, 0);
    }
    const D3D11_RECT scissor = native_rectangle(draw.scissor);
    g.context->RSSetScissorRects(1, &scissor);
    g.context->PSSetShaderResources(0, 4, views);
    g.context->PSSetSamplers(0, 4, samplers);
    g.context->OMSetBlendState(blend->second.Get(), nullptr, 0xFFFFFFFFu);
    g.context->OMSetDepthStencilState(depth->second.Get(), draw.stencil_reference & 0xFF);
    g.context->RSSetState(rasterizer->second.Get());
    const UINT stride = program_draw ? sizeof(GfxNativeInputs) : sizeof(GfxNativeVertex);
    const UINT offset = vertex_offset;
    g.context->IASetInputLayout(g.native_layout.Get());
    g.context->IASetPrimitiveTopology(draw.lines ? D3D11_PRIMITIVE_TOPOLOGY_LINELIST
        : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g.context->IASetVertexBuffers(0, 1, g.native_vb.GetAddressOf(), &stride, &offset);
    if (program_draw)
    {
        auto& program = g.native_programs.find(draw.vertex_program)->second;
        g.context->IASetInputLayout(program.layout.Get());
        g.context->VSSetShader(program.shader.Get(), nullptr, 0);
        if (draw.vertex_constants == nullptr || FAILED(g.context->Map(g.native_constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            throw Error("native vertex constants map failed");
        }
        std::memcpy(mapped.pData, draw.vertex_constants->data(), sizeof(*draw.vertex_constants));
        g.context->Unmap(g.native_constants.Get(), 0);
        ID3D11Buffer* constants = g.native_constants.Get();
        g.context->VSSetConstantBuffers(3, 1, &constants);
        validate_native_gpu_vertices(draw, program);
    }
    else
    {
        g.context->VSSetShader(g.native_vs.Get(), nullptr, 0);
    }
#endif
    if (draw.indices.empty())
    {
        profile_gpu_draw_begin();
        g.context->Draw(UINT(draw.vertices.size()), 0);
        profile_gpu_draw_end();
    }
#if GBTG_OPTIMIZE
    else if (retained != nullptr)
    {
        g.context->IASetIndexBuffer(retained->indices.Get(), DXGI_FORMAT_R32_UINT, 0);
        profile_gpu_draw_begin();
        g.context->DrawIndexed(UINT(draw.indices.size()), 0, 0);
        profile_gpu_draw_end();
    }
#endif
    else
    {
        const UINT index_bytes = UINT(draw.indices.size_bytes());
        if (!g.native_ib || g.native_ib_size < index_bytes)
        {
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = std::max<UINT>(index_bytes, 0x100000);
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            g.native_ib.Reset();
            if (FAILED(g.device->CreateBuffer(&desc, nullptr, g.native_ib.GetAddressOf())))
            {
                throw Error("D3D11 native index buffer creation failed");
            }
            g.native_ib_size = desc.ByteWidth;
            g.native_ib_cursor = 0;
            if (render_profile::enabled)
            {
                ++render_profile::frame.buffer_allocations;
            }
        }
        if (index_bytes > g.native_ib_size - g.native_ib_cursor)
        {
            g.native_ib_cursor = 0;
        }
        const UINT index_offset = g.native_ib_cursor;
        if (FAILED(g.context->Map(g.native_ib.Get(), 0,
            index_offset == 0 ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped)))
        {
            throw Error("D3D11 native index buffer map failed");
        }
        std::memcpy(static_cast<u8*>(mapped.pData) + index_offset, draw.indices.data(), index_bytes);
        g.context->Unmap(g.native_ib.Get(), 0);
        g.native_ib_cursor += index_bytes;
        g.context->IASetIndexBuffer(g.native_ib.Get(), DXGI_FORMAT_R32_UINT, index_offset);
        profile_gpu_draw_begin();
        g.context->DrawIndexed(UINT(draw.indices.size()), 0, 0);
        profile_gpu_draw_end();
    }
    g.native_pipeline = true;
    if (!g.native_batch)
    {
        gfx_end_native_batch();
    }
}

void gfx_begin_native_target(int width, int height, bool depth)
{
    if (!g.ok)
    {
        return;
    }
    if (native_target.rendering)
    {
        throw Error("native render target is already bound");
    }
    if (native_target.width != width || native_target.height != height)
    {
        native_target = NativeRenderTarget{};
        native_target.width = width;
        native_target.height = height;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = UINT(width);
        desc.Height = UINT(height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        if (FAILED(g.device->CreateTexture2D(&desc, nullptr, native_target.color.GetAddressOf())) ||
            FAILED(g.device->CreateRenderTargetView(native_target.color.Get(), nullptr,
                native_target.rtv.GetAddressOf())))
        {
            throw Error("native render target creation failed");
        }
        desc.BindFlags = 0;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(g.device->CreateTexture2D(&desc, nullptr, native_target.staging.GetAddressOf())))
        {
            throw Error("native render target staging creation failed");
        }
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.CPUAccessFlags = 0;
        desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        if (FAILED(g.device->CreateTexture2D(&desc, nullptr, native_target.depth.GetAddressOf())) ||
            FAILED(g.device->CreateDepthStencilView(native_target.depth.Get(), nullptr,
                native_target.dsv.GetAddressOf())))
        {
            throw Error("native render target depth creation failed");
        }
    }
    UINT viewports = 1;
    g.context->RSGetViewports(&viewports, &native_target.saved_viewport);
    ID3D11ShaderResourceView* unbound[4]{};
    g.context->PSSetShaderResources(0, 4, unbound);
    native_target.depth_bound = depth;
    g.context->OMSetRenderTargets(1, native_target.rtv.GetAddressOf(), depth ? native_target.dsv.Get() : nullptr);
    // Xbox pixel coordinates map 1:1 onto the surface; the 640x480 space extends past its edges.
    D3D11_VIEWPORT vp{0.f, 0.f, 640.f, 480.f, 0.f, 1.f};
    g.context->RSSetViewports(1, &vp);
    native_target.rendering = true;
}

void gfx_read_native_target_depth(std::vector<u32>& output, int& width, int& height)
{
    output.clear();
    width = 0;
    height = 0;
    if (!g.ok)
    {
        return;
    }
    if (scene_target.rendering && scene_target.own_depth)
    {
        // The scene target's own depth-stencil, over the fitted 640x480 area as gfx_end_native_scene_target reads
        // the colour.
        NativeSceneSurface& surface = *scene_target.open;
        if (!surface.depth_staging)
        {
            D3D11_TEXTURE2D_DESC desc{};
            surface.depth->GetDesc(&desc);
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            if (FAILED(g.device->CreateTexture2D(&desc, nullptr, surface.depth_staging.GetAddressOf())))
            {
                throw Error("native scene target depth staging creation failed");
            }
        }
        g.context->CopyResource(surface.depth_staging.Get(), surface.depth.Get());
        D3D11_VIEWPORT area{};
        UINT viewports = 1;
        present_hud_viewport();
        g.context->RSGetViewports(&viewports, &area);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(g.context->Map(surface.depth_staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
        {
            throw Error("native scene target depth readback failed");
        }
        const int left = std::clamp(int(area.TopLeftX + 0.5f), 0, int(surface.desc.Width) - 1);
        const int top = std::clamp(int(area.TopLeftY + 0.5f), 0, int(surface.desc.Height) - 1);
        width = std::clamp(int(area.Width + 0.5f), 1, int(surface.desc.Width) - left);
        height = std::clamp(int(area.Height + 0.5f), 1, int(surface.desc.Height) - top);
        output.resize(size_t(width) * size_t(height));
        for (int y = 0; y < height; ++y)
        {
            const u8* row = static_cast<const u8*>(mapped.pData) + size_t(top + y) * mapped.RowPitch;
            for (int x = 0; x < width; ++x)
            {
                u32 value;
                std::memcpy(&value, row + size_t(left + x) * 4, 4);
                output[size_t(y) * size_t(width) + size_t(x)] = ((value & 0xFFFFFFu) << 8) | (value >> 24);
            }
        }
        g.context->Unmap(surface.depth_staging.Get(), 0);
        return;
    }
    if (!native_target.rendering)
    {
        throw Error("native render target is not bound");
    }
    if (!native_target.depth_staging)
    {
        D3D11_TEXTURE2D_DESC desc{};
        native_target.depth->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(g.device->CreateTexture2D(&desc, nullptr, native_target.depth_staging.GetAddressOf())))
        {
            throw Error("native render target depth staging creation failed");
        }
    }
    g.context->CopyResource(native_target.depth_staging.Get(), native_target.depth.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(g.context->Map(native_target.depth_staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
    {
        throw Error("native render target depth readback failed");
    }
    width = native_target.width;
    height = native_target.height;
    output.resize(size_t(native_target.width) * size_t(native_target.height));
    for (int y = 0; y < native_target.height; ++y)
    {
        const u8* row = static_cast<const u8*>(mapped.pData) + size_t(y) * mapped.RowPitch;
        for (int x = 0; x < native_target.width; ++x)
        {
            u32 value;
            std::memcpy(&value, row + size_t(x) * 4, 4);
            // D24_UNORM_S8_UINT holds depth in bits 0..23 and stencil above; the NV2A's D24S8 is the reverse.
            output[size_t(y) * size_t(native_target.width) + size_t(x)] = ((value & 0xFFFFFFu) << 8) | (value >> 24);
        }
    }
    g.context->Unmap(native_target.depth_staging.Get(), 0);
}

void gfx_end_native_target(RgbaImage& output)
{
    if (!g.ok)
    {
        return;
    }
    if (!native_target.rendering)
    {
        throw Error("native render target is not bound");
    }
    g.context->CopyResource(native_target.staging.Get(), native_target.color.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(g.context->Map(native_target.staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
    {
        throw Error("native render target readback failed");
    }
    output.width = native_target.width;
    output.height = native_target.height;
    output.bgra.resize(size_t(output.width) * size_t(output.height) * 4);
    for (int y = 0; y < output.height; ++y)
    {
        std::memcpy(output.bgra.data() + size_t(y) * size_t(output.width) * 4,
            static_cast<const u8*>(mapped.pData) + size_t(y) * mapped.RowPitch, size_t(output.width) * 4);
    }
    g.context->Unmap(native_target.staging.Get(), 0);
    g.context->OMSetRenderTargets(1, g.rtv.GetAddressOf(), g.dsv.Get());
    g.context->RSSetViewports(1, &native_target.saved_viewport);
    native_target.rendering = false;
}

#if GBTG_OPTIMIZE
// GBTG_OPTIMIZE steps 12 and 25: the texture native_texture binds for `image`, made on the GPU by a copy; kept when
// it already has the image's shape.
ComPtr<ID3D11Texture2D> native_image_texture(const RgbaImage& image)
{
    GfxD3D11::SpriteGpu& sprite = g.sprites[&image];
    ComPtr<ID3D11Texture2D> texture;
    if (sprite.srv)
    {
        ComPtr<ID3D11Resource> resource;
        sprite.srv->GetResource(resource.GetAddressOf());
        D3D11_TEXTURE2D_DESC desc{};
        if (SUCCEEDED(resource.As(&texture)))
        {
            texture->GetDesc(&desc);
            if (desc.Width != u32(image.width) || desc.Height != u32(image.height) || desc.MipLevels != 1
                || desc.ArraySize != 1 || desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM || desc.SampleDesc.Count != 1
                || desc.Usage != D3D11_USAGE_DEFAULT)
            {
                texture.Reset();
            }
        }
    }
    if (!texture)
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = u32(image.width);
        desc.Height = u32(image.height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        sprite.srv.Reset();
        if (FAILED(g.device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf()))
            || FAILED(g.device->CreateShaderResourceView(texture.Get(), nullptr, sprite.srv.GetAddressOf())))
        {
            throw Error("native render target texture creation failed");
        }
    }
    return texture;
}
#endif

bool gfx_end_native_target_into(const RgbaImage& image)
{
#if GBTG_OPTIMIZE
    if (!g.ok)
    {
        return false;
    }
    if (!native_target.rendering)
    {
        throw Error("native render target is not bound");
    }
    if (image.width != native_target.width || image.height != native_target.height)
    {
        return false;
    }
    const ComPtr<ID3D11Texture2D> texture = native_image_texture(image);
    g.context->CopyResource(texture.Get(), native_target.color.Get());
    g.context->CopyResource(native_target.staging.Get(), native_target.color.Get());
    g.sprites[&image].serial = image.serial;
    native_target.ended = native_target.staging;
    // Submitted now, so the GPU has drawn and copied the target by the time gfx_read_ended_native_target maps it;
    // otherwise the driver holds the frame's commands until that Map and it waits for all of them.
    g.context->Flush();
    g.context->OMSetRenderTargets(1, g.rtv.GetAddressOf(), g.dsv.Get());
    g.context->RSSetViewports(1, &native_target.saved_viewport);
    native_target.rendering = false;
    return true;
#else
    (void)image;
    return false;
#endif
}

void gfx_read_ended_native_target(RgbaImage& output)
{
    if (!native_target.ended)
    {
        throw Error("no native target ended on the GPU");
    }
    D3D11_TEXTURE2D_DESC desc{};
    native_target.ended->GetDesc(&desc);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(g.context->Map(native_target.ended.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
    {
        throw Error("native render target readback failed");
    }
    output.width = int(desc.Width);
    output.height = int(desc.Height);
    output.bgra.resize(size_t(output.width) * size_t(output.height) * 4);
    for (int y = 0; y < output.height; ++y)
    {
        std::memcpy(output.bgra.data() + size_t(y) * size_t(output.width) * 4,
            static_cast<const u8*>(mapped.pData) + size_t(y) * mapped.RowPitch, size_t(output.width) * 4);
    }
    g.context->Unmap(native_target.ended.Get(), 0);
    native_target.ended.Reset();
}

void gfx_begin_native_scene_target(u32 key, int width, int height, bool own_depth)
{
    if (!g.ok)
    {
        return;
    }
    if (native_target.rendering || scene_target.rendering)
    {
        throw Error("native render target is already bound");
    }
    ComPtr<ID3D11Resource> back;
    g.rtv->GetResource(back.GetAddressOf());
    ComPtr<ID3D11Texture2D> back_texture;
    if (FAILED(back.As(&back_texture)))
    {
        throw Error("native scene target: the back buffer is not a 2D texture");
    }
    D3D11_TEXTURE2D_DESC desc{};
    back_texture->GetDesc(&desc);
    NativeSceneSurface& surface = scene_target.surfaces[key];
    if (!surface.color || surface.desc.Width != desc.Width || surface.desc.Height != desc.Height
        || surface.desc.Format != desc.Format || surface.desc.SampleDesc.Count != desc.SampleDesc.Count)
    {
        surface = NativeSceneSurface{};
        D3D11_TEXTURE2D_DESC color = desc;
        color.MipLevels = 1;
        color.ArraySize = 1;
        color.BindFlags = D3D11_BIND_RENDER_TARGET;
        color.Usage = D3D11_USAGE_DEFAULT;
        color.CPUAccessFlags = 0;
        color.MiscFlags = 0;
        if (FAILED(g.device->CreateTexture2D(&color, nullptr, surface.color.GetAddressOf())) ||
            FAILED(g.device->CreateRenderTargetView(surface.color.Get(), nullptr, surface.rtv.GetAddressOf())))
        {
            throw Error("native scene target creation failed");
        }
        D3D11_TEXTURE2D_DESC single = color;
        single.SampleDesc.Count = 1;
        single.SampleDesc.Quality = 0;
        single.BindFlags = 0;
        if (color.SampleDesc.Count > 1 &&
            FAILED(g.device->CreateTexture2D(&single, nullptr, surface.resolved.GetAddressOf())))
        {
            throw Error("native scene target resolve creation failed");
        }
        single.Usage = D3D11_USAGE_STAGING;
        single.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(g.device->CreateTexture2D(&single, nullptr, surface.staging.GetAddressOf())))
        {
            throw Error("native scene target staging creation failed");
        }
        surface.desc = desc;
    }
    if (own_depth && !surface.dsv)
    {
        ComPtr<ID3D11Resource> back_depth;
        g.dsv->GetResource(back_depth.GetAddressOf());
        ComPtr<ID3D11Texture2D> back_depth_texture;
        if (FAILED(back_depth.As(&back_depth_texture)))
        {
            throw Error("native scene target: the back buffer's depth-stencil is not a 2D texture");
        }
        D3D11_TEXTURE2D_DESC depth_desc{};
        back_depth_texture->GetDesc(&depth_desc);
        D3D11_DEPTH_STENCIL_VIEW_DESC view{};
        g.dsv->GetDesc(&view);
        if (FAILED(g.device->CreateTexture2D(&depth_desc, nullptr, surface.depth.GetAddressOf())) ||
            FAILED(g.device->CreateDepthStencilView(surface.depth.Get(), &view, surface.dsv.GetAddressOf())))
        {
            throw Error("native scene target depth creation failed");
        }
    }
    scene_target.open = &surface;
    scene_target.width = width;
    scene_target.height = height;
    scene_target.own_depth = own_depth;
    UINT viewports = 1;
    g.context->RSGetViewports(&viewports, &scene_target.saved_viewport);
    g.context->OMSetRenderTargets(1, surface.rtv.GetAddressOf(), own_depth ? surface.dsv.Get() : g.dsv.Get());
    scene_target.rendering = true;
    present_hud_viewport();
}

void gfx_end_native_scene_target(RgbaImage& output)
{
    if (!g.ok)
    {
        return;
    }
    if (!scene_target.rendering || scene_target.open == nullptr)
    {
        throw Error("native scene target is not bound");
    }
    NativeSceneSurface& surface = *scene_target.open;
    if (surface.resolved)
    {
        g.context->ResolveSubresource(surface.resolved.Get(), 0, surface.color.Get(), 0, surface.desc.Format);
        g.context->CopyResource(surface.staging.Get(), surface.resolved.Get());
    }
    else
    {
        g.context->CopyResource(surface.staging.Get(), surface.color.Get());
    }
    // The fitted 640x480 area present_hud_viewport draws the frame into, at the window's resolution.
    D3D11_VIEWPORT area{};
    UINT viewports = 1;
    present_hud_viewport();
    g.context->RSGetViewports(&viewports, &area);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(g.context->Map(surface.staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
    {
        throw Error("native scene target readback failed");
    }
    const int left = std::clamp(int(area.TopLeftX + 0.5f), 0, int(surface.desc.Width) - 1);
    const int top = std::clamp(int(area.TopLeftY + 0.5f), 0, int(surface.desc.Height) - 1);
    output.width = std::clamp(int(area.Width + 0.5f), 1, int(surface.desc.Width) - left);
    output.height = std::clamp(int(area.Height + 0.5f), 1, int(surface.desc.Height) - top);
    output.bgra.resize(size_t(output.width) * size_t(output.height) * 4);
    for (int y = 0; y < output.height; ++y)
    {
        const u8* row = static_cast<const u8*>(mapped.pData) + size_t(top + y) * mapped.RowPitch + size_t(left) * 4;
        std::memcpy(output.bgra.data() + size_t(y) * size_t(output.width) * 4, row, size_t(output.width) * 4);
    }
    g.context->Unmap(surface.staging.Get(), 0);
    if (surface.desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || surface.desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
    {
        for (size_t index = 0; index < output.bgra.size(); index += 4)
        {
            std::swap(output.bgra[index], output.bgra[index + 2]);
        }
    }
    g.context->OMSetRenderTargets(1, g.rtv.GetAddressOf(), g.dsv.Get());
    g.context->RSSetViewports(1, &scene_target.saved_viewport);
    scene_target.rendering = false;
    scene_target.open = nullptr;
}

void gfx_scene_target_size(int& width, int& height)
{
    width = 0;
    height = 0;
    if (!g.ok || g.cfg.width <= 0 || g.cfg.height <= 0)
    {
        return;
    }
    const GfxUiFrame area = gfx_scene_target_area(g.cfg.width, g.cfg.height, g.cfg.width, g.cfg.height,
        g.cfg.widescreen, g.cfg.frame_aspect);
    width = area.width;
    height = area.height;
}

// GBTG_OPTIMIZE step 27: the scene target's picture copied on the GPU into the texture native draws bind for `image`,
// with no readback, and the back buffer bound again as gfx_end_native_scene_target leaves it.
bool gfx_end_native_scene_target_into(const RgbaImage& image)
{
#if GBTG_OPTIMIZE
    if (!g.ok || image.width <= 0 || image.height <= 0)
    {
        return false;
    }
    if (!scene_target.rendering || scene_target.open == nullptr)
    {
        throw Error("native scene target is not bound");
    }
    NativeSceneSurface& surface = *scene_target.open;
    if (surface.desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM)
    {
        return false; // The readback reorders other formats' channels.
    }
    ID3D11Texture2D* source = surface.color.Get();
    if (surface.resolved)
    {
        g.context->ResolveSubresource(surface.resolved.Get(), 0, surface.color.Get(), 0, surface.desc.Format);
        source = surface.resolved.Get();
    }
    const GfxUiFrame area = gfx_scene_target_area(g.cfg.width, g.cfg.height, int(surface.desc.Width),
        int(surface.desc.Height), g.cfg.widescreen, g.cfg.frame_aspect);
    const ComPtr<ID3D11Texture2D> texture = native_image_texture(image);
    // The image was sized by gfx_scene_target_size; a resize since then copies the part both hold.
    const D3D11_BOX box{u32(area.left), u32(area.top), 0, u32(area.left + std::min(area.width, image.width)),
        u32(area.top + std::min(area.height, image.height)), 1};
    g.context->CopySubresourceRegion(texture.Get(), 0, 0, 0, 0, source, 0, &box);
    g.sprites[&image].serial = image.serial;
    g.context->OMSetRenderTargets(1, g.rtv.GetAddressOf(), g.dsv.Get());
    g.context->RSSetViewports(1, &scene_target.saved_viewport);
    scene_target.rendering = false;
    scene_target.open = nullptr;
    return true;
#else
    (void)image;
    return false;
#endif
}

void gfx_clear_native(std::span<const std::array<i32, 4>> rectangles, u32 flags, u32 color, float depth)
{
    if (!g.ok)
    {
        return;
    }
    present_hud_viewport();
    const UINT depth_flags = ((flags & 1) != 0 ? D3D11_CLEAR_DEPTH : 0) | ((flags & 2) != 0 ? D3D11_CLEAR_STENCIL : 0);
    ID3D11RenderTargetView* target = native_target.rendering ? native_target.rtv.Get()
        : scene_target.rendering ? scene_target.open->rtv.Get() : g.rtv.Get();
    ID3D11DepthStencilView* depth_view = native_target.rendering
        ? (native_target.depth_bound ? native_target.dsv.Get() : nullptr)
        : scene_target.rendering && scene_target.own_depth ? scene_target.open->dsv.Get() : g.dsv.Get();
    // A single rectangle over the whole 640x480 back buffer (the full view's fitted rectangle) clears as a whole.
    const bool whole = rectangles.size() == 1 && rectangles[0][0] == 0 && rectangles[0][1] == 0
        && rectangles[0][2] == 640 && rectangles[0][3] == 480;
    if (rectangles.empty() || whole)
    {
        if ((flags & 0xF0) != 0)
        {
            const float rgba[4]{float((color >> 16) & 0xFF) / 255.f, float((color >> 8) & 0xFF) / 255.f,
                float(color & 0xFF) / 255.f, float(color >> 24) / 255.f};
            g.context->ClearRenderTargetView(target, rgba);
        }
        if (depth_flags != 0 && depth_view != nullptr) // No depth surface, nothing to clear.
        {
            g.context->ClearDepthStencilView(depth_view, depth_flags, depth, 0);
        }
        return;
    }
    if ((flags & 0xF0) != 0)
    {
        // A texture target is in Xbox pixels (see present_hud_viewport); on the back buffer each rectangle maps
        // through the HUD viewport onto the window (a widescreen view's fitted 4:3 area, for one).
        ComPtr<ID3D11DeviceContext1> context1;
        if ((flags & 0xF0) != 0xF0 || FAILED(g.context.As(&context1)))
        {
            throw Error("native clear of a colour rectangle supports a whole colour on the back buffer or a texture "
                        "target only");
        }
        const float rgba[4]{float((color >> 16) & 0xFF) / 255.f, float((color >> 8) & 0xFF) / 255.f,
            float(color & 0xFF) / 255.f, float(color >> 24) / 255.f};
        std::vector<D3D11_RECT> rects;
        for (const auto& rectangle : rectangles)
        {
            rects.push_back(native_target.rendering ? D3D11_RECT{rectangle[0], rectangle[1], rectangle[2], rectangle[3]}
                                                    : native_rectangle(rectangle));
        }
        context1->ClearView(target, rgba, rects.data(), UINT(rects.size()));
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
    g.sprites.erase(&image);
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 18: the draw memo must not answer for an image released and allocated again.
    g.native_bound.images = {};
#endif
}

void gfx_draw_sprite(const RgbaImage& image, const GfxRect& rect)
{
    render_profile::Scope measurement(render_profile::HostRendering);
    if (!g.ok || image.bgra.empty() || image.width <= 0 || image.height <= 0 || rect.w == 0.f || rect.h == 0.f)
    {
        return;
    }
    auto it = g.sprites.find(&image);
    if (it == g.sprites.end() || it->second.serial != image.serial)
    {
        GfxD3D11::SpriteGpu gpu;
        gpu.srv = upload_bgra(image.bgra.data(), image.width, image.height);
        gpu.serial = image.serial;
        it = g.sprites.insert_or_assign(&image, std::move(gpu)).first;
    }
    if (!it->second.srv)
    {
        return;
    }

    const float x0 = rect.x * 2.f - 1.f;
    const float y0 = 1.f - rect.y * 2.f;
    const float x1 = (rect.x + rect.w) * 2.f - 1.f;
    const float y1 = 1.f - (rect.y + rect.h) * 2.f;
    const GpuVert verts[6] = {
        {x0, y0, 0.f, rect.u0, rect.v0, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}, {x1, y0, 0.f, rect.u1, rect.v0, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
        {x0, y1, 0.f, rect.u0, rect.v1, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}, {x1, y0, 0.f, rect.u1, rect.v0, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
        {x1, y1, 0.f, rect.u1, rect.v1, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}, {x0, y1, 0.f, rect.u0, rect.v1, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    };
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(g.context->Map(g.ui_vb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }
    std::memcpy(mapped.pData, verts, sizeof(verts));
    g.context->Unmap(g.ui_vb.Get(), 0);

    Mat4 identity{};
    identity.m[0] = identity.m[5] = identity.m[10] = identity.m[15] = 1.f;
    if (SUCCEEDED(g.context->Map(g.cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        GpuCB cb;
        std::memcpy(cb.mvp, identity.m, sizeof(cb.mvp));
        store_alpha_rs(cb);
        cb.alpha_pad = 1.f;
        cb.tint = rect.color;
        std::memcpy(mapped.pData, &cb, sizeof(cb));
        g.context->Unmap(g.cb.Get(), 0);
    }

    const UINT stride = sizeof(GpuVert);
    const UINT offset = 0;
    // 0x0005E110 clears the shaders before this draw. Put the UI pipeline back.
    g.context->IASetInputLayout(g.layout.Get());
    g.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g.context->VSSetShader(g.vs.Get(), nullptr, 0);
    g.context->PSSetShader(g.ps.Get(), nullptr, 0);
    g.context->RSSetState(g.rast.Get());
    g.context->IASetVertexBuffers(0, 1, g.ui_vb.GetAddressOf(), &stride, &offset);
    ID3D11Buffer* constant_buffers[2] = {g.cb.Get(), g.bone_cb.Get()};
    g.context->VSSetConstantBuffers(0, 2, constant_buffers);
    g.context->PSSetConstantBuffers(0, 1, constant_buffers);
    g.context->OMSetDepthStencilState(g.depth_off.Get(), 0);
    g.context->OMSetBlendState(g.blend_alpha.Get(), nullptr, 0xFFFFFFFFu);
    g.context->PSSetSamplers(0, 1, g.sampler.GetAddressOf());
    ID3D11ShaderResourceView* unbound = nullptr;
    ID3D11ShaderResourceView* srvs[2] = {it->second.srv.Get(), unbound};
    g.context->PSSetShaderResources(0, 2, srvs);
    profile_gpu_draw_begin();
    g.context->Draw(6, 0);
    profile_gpu_draw_end();
}

namespace
{

// Xbox 0x00033A90
// DAT_0033A5B8 is the full backbuffer: origin 0, retail 640x480, z 0..1.
void present_set_viewport()
{
    if (!g.ok)
    {
        return;
    }
    D3D11_VIEWPORT vp{};
    vp.Width = float(g.cfg.width);
    vp.Height = float(g.cfg.height);
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    g.context->RSSetViewports(1, &vp);
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
    UINT bits = 0;
    if ((flags & 0x1u) != 0)
    {
        bits |= D3D11_CLEAR_DEPTH;
    }
    if ((flags & 0x2u) != 0)
    {
        bits |= D3D11_CLEAR_STENCIL;
    }
    if (bits != 0)
    {
        g.context->ClearDepthStencilView(g.dsv.Get(), bits, 1.f, 0);
    }
}

// Xbox 0x00033A90
// 0x00154A40. Notify 0, D3DDevice_Swap(0), notify 1.
// Copies the finished back buffer aside so gfx_present_again can show it again until the next logic tick.
void retain_last_frame()
{
    ComPtr<ID3D11Texture2D> back;
    if (FAILED(g.swap->GetBuffer(0, IID_PPV_ARGS(back.GetAddressOf()))))
    {
        g.last_frame.Reset();
        return;
    }
    D3D11_TEXTURE2D_DESC desc{};
    back->GetDesc(&desc);
    if (g.last_frame)
    {
        D3D11_TEXTURE2D_DESC held{};
        g.last_frame->GetDesc(&held);
        if (held.Width != desc.Width || held.Height != desc.Height || held.Format != desc.Format)
        {
            g.last_frame.Reset();
        }
    }
    if (!g.last_frame)
    {
        desc.BindFlags = 0;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        if (FAILED(g.device->CreateTexture2D(&desc, nullptr, g.last_frame.GetAddressOf())))
        {
            return;
        }
    }
    g.context->CopyResource(g.last_frame.Get(), back.Get());
}

// Host presentation over the frame about to be presented: the inspector's UI, or plugin overlays (the debug UI).
void draw_overlays()
{
#if defined(GBTG_INSPECTOR_APP)
    g.context->OMSetRenderTargets(1, g.rtv.GetAddressOf(), nullptr);
    debug_ui_draw();
#elif !defined(GBTG_RETAIL_GAME_ONLY)
    if (plugins_overlay_has_draws())
    {
        g.context->OMSetRenderTargets(1, g.rtv.GetAddressOf(), nullptr);
        plugins_overlay_render(nullptr);
    }
#endif
}

void present_swap()
{
    gfx_present_swap_notify(0);
    if (g.ok && g.swap)
    {
        retain_last_frame();
        draw_overlays();
        render_profile::Scope measurement(render_profile::Present);
        g.swap->Present(g.cfg.vsync, 0);
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

// Xbox 0x000F6F70
// DAT_005096E8 / DAT_005096EA are the HUD width and height, 640x480.
void present_hud_viewport()
{
    if (!g.ok || g.cfg.width <= 0 || g.cfg.height <= 0)
    {
        return;
    }
    // The 640x480 frame fills a 16:9 area in widescreen (the Xbox's anamorphic output), else 4:3.
    float frame = g.cfg.widescreen ? 16.f / 9.f : 640.f / 480.f;
    if (g.cfg.frame_aspect > 0.f)
    {
        frame = g.cfg.frame_aspect; // The wide_aspect plugin's frame.
    }
    const float window = float(g.cfg.width) / float(g.cfg.height);
    D3D11_VIEWPORT vp{};
    vp.MaxDepth = 1.f;
    if (native_target.rendering)
    {
        // Xbox pixel coordinates map 1:1 onto the texture surface.
        vp.Width = 640.f;
        vp.Height = 480.f;
        g.context->RSSetViewports(1, &vp);
        return;
    }
    if (window > frame)
    {
        vp.Height = float(g.cfg.height);
        vp.Width = vp.Height * frame;
        vp.TopLeftX = (float(g.cfg.width) - vp.Width) * 0.5f;
    }
    else
    {
        vp.Width = float(g.cfg.width);
        vp.Height = vp.Width / frame;
        vp.TopLeftY = (float(g.cfg.height) - vp.Height) * 0.5f;
    }
    g.context->RSSetViewports(1, &vp);
}

// Xbox 0x000F6170. Translate the logical retail viewport into the fitted
// host game area after 0x000F5760 / 0x000F56C0's integer truncation.
void present_camera_viewport()
{
    present_hud_viewport();
    i32 width, x, height, y;
    fit_display_viewport(640, 480, 0, width, x, height, y);
    D3D11_VIEWPORT vp{};
    UINT count = 1;
    g.context->RSGetViewports(&count, &vp);
    vp.TopLeftX += vp.Width * float(x) / 640.f;
    vp.TopLeftY += vp.Height * float(y) / 480.f;
    vp.Width *= float(width) / 640.f;
    vp.Height *= float(height) / 480.f;
    g.context->RSSetViewports(1, &vp);
}

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
        d3d11::gfx_draw_sprite(*quad.image, rect);
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
// Xbox 0x00033A90
void present_draw_frame()
{
    const u32 flags = gfx_present_clear_flags();
    present_set_viewport();
    present_clear(flags);
    d3d_reset_render_state();
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

} // namespace

void gfx_show_startup_loading_screen(const RgbaImage& text, float progress)
{
    if (!g.ok || !g.swap || g.cfg.width <= 0 || g.cfg.height <= 0 || text.width <= 0 || text.height <= 0)
    {
        return;
    }

    const float black[4] = {0.f, 0.f, 0.f, 1.f};
    g.context->OMSetRenderTargets(1, g.rtv.GetAddressOf(), g.dsv.Get());
    g.context->ClearRenderTargetView(g.rtv.Get(), black);
    if (g.dsv)
    {
        g.context->ClearDepthStencilView(g.dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);
    }
    D3D11_VIEWPORT viewport{};
    viewport.Width = float(g.cfg.width);
    viewport.Height = float(g.cfg.height);
    viewport.MaxDepth = 1.f;
    g.context->RSSetViewports(1, &viewport);

    const float text_height_pixels = 42.f * float(g.cfg.height) / 540.f;
    const float text_width_pixels = text_height_pixels * float(text.width) / float(text.height);
    GfxRect rectangle;
    rectangle.x = (float(g.cfg.width) - text_width_pixels) / (2.f * float(g.cfg.width));
    rectangle.y = (float(g.cfg.height) - text_height_pixels) / (2.f * float(g.cfg.height));
    rectangle.w = text_width_pixels / float(g.cfg.width);
    rectangle.h = text_height_pixels / float(g.cfg.height);
    d3d11::gfx_draw_sprite(text, rectangle);
    d3d11::gfx_release_sprite(text);

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
    d3d11::gfx_draw_sprite(pixel, bar);
    bar.x += 1.f / float(g.cfg.width);
    bar.y += 1.f / float(g.cfg.height);
    bar.w = std::max(0.f, bar_width - 2.f) / float(g.cfg.width);
    bar.h = std::max(0.f, bar_height - 2.f) / float(g.cfg.height);
    bar.w *= std::clamp(progress, 0.f, 1.f);
    bar.color = font_color;
    d3d11::gfx_draw_sprite(pixel, bar);
    d3d11::gfx_release_sprite(pixel);
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
    // Present 0x00033A90 (clear and swap). frame_tick runs 0x00033480 (the controller prompt) and 0x00032AB0.
    present_draw_frame();
    profile_gpu_end();
}

void gfx_present_again()
{
    if (!g.ok || !g.swap || !g.last_frame)
    {
        return;
    }
    ComPtr<ID3D11Texture2D> back;
    if (FAILED(g.swap->GetBuffer(0, IID_PPV_ARGS(back.GetAddressOf()))))
    {
        return;
    }
    g.context->CopyResource(back.Get(), g.last_frame.Get());
    draw_overlays();
    g.swap->Present(g.cfg.vsync, 0);
}

} // namespace d3d11

#if defined(GBTG_INSPECTOR_APP)
// inspector_graphics.h is a gbtg-level interface over this backend's state. The inspector is D3D11 only.
using namespace d3d11;
namespace
{
// Slot 0 is the scene view, slot 1 the asset viewer's preview.
std::array<ComPtr<ID3D11Texture2D>, 2> inspection_color;
std::array<ComPtr<ID3D11ShaderResourceView>, 2> inspection_srv;
ComPtr<ID3D11Texture2D> inspection_depth_staging;
std::vector<u32> inspection_depth;

int inspection_pixel(float retail, int size, float scale, float extent)
{
    return int((size - extent * scale) * .5f + retail * scale);
}
}

void* inspector_window()
{
    return g.hwnd;
}

bool inspector_window_minimized()
{
    return g.hwnd != nullptr && IsIconic(g.hwnd) != FALSE;
}

void inspector_resize_surface(int width, int height)
{
    if (!g.ok || width <= 0 || height <= 0 || (width == g.cfg.width && height == g.cfg.height))
    {
        return;
    }
    g.context->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11ShaderResourceView* empty[4]{};
    g.context->PSSetShaderResources(0, 4, empty);
    inspector_release_graphics();
    g.rtv.Reset();
    g.dsv.Reset();
    g.depth.Reset();
    g.color.Reset();
    g.last_frame.Reset();
    g.context->Flush();
    if (g.swap && FAILED(g.swap->ResizeBuffers(0, UINT(width), UINT(height), DXGI_FORMAT_UNKNOWN, 0)))
    {
        throw Error("inspector swap chain resize failed");
    }
    if (!create_targets(width, height))
    {
        throw Error("inspector render target resize failed");
    }
    g.cfg.width = width;
    g.cfg.height = height;
}

void inspector_release_graphics()
{
    for (size_t slot = 0; slot < inspection_color.size(); ++slot)
    {
        inspection_srv[slot].Reset();
        inspection_color[slot].Reset();
    }
    inspection_depth_staging.Reset();
    inspection_depth.clear();
}

void* inspector_texture_preview(const RgbaImage& image)
{
    return native_texture(&image);
}

void* inspector_capture_viewport(bool read_depth, int slot)
{
    if (slot < 0 || size_t(slot) >= inspection_color.size())
    {
        throw Error("inspector viewport slot is out of range");
    }
    if (!g.ok)
    {
        return nullptr;
    }
    ComPtr<ID3D11Texture2D> source = g.color;
    if (g.swap && FAILED(g.swap->GetBuffer(0, IID_PPV_ARGS(source.GetAddressOf()))))
    {
        throw Error("inspector cannot access the viewport target");
    }
    auto& color = inspection_color[size_t(slot)];
    auto& view = inspection_srv[size_t(slot)];
    if (!color)
    {
        D3D11_TEXTURE2D_DESC desc{};
        source->GetDesc(&desc);
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(g.device->CreateTexture2D(&desc, nullptr, color.GetAddressOf()))
            || FAILED(g.device->CreateShaderResourceView(color.Get(), nullptr, view.GetAddressOf())))
        {
            throw Error("inspector viewport texture creation failed");
        }
    }
    g.context->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11ShaderResourceView* empty[4]{};
    g.context->PSSetShaderResources(0, 4, empty);
    g.context->CopyResource(color.Get(), source.Get());
    if (slot == 0)
    {
        inspection_depth.clear();
    }
    if (read_depth && slot == 0)
    {
        if (!inspection_depth_staging)
        {
            D3D11_TEXTURE2D_DESC desc{};
            g.depth->GetDesc(&desc);
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            if (FAILED(g.device->CreateTexture2D(&desc, nullptr, inspection_depth_staging.GetAddressOf())))
            {
                throw Error("inspector depth readback creation failed");
            }
        }
        g.context->CopyResource(inspection_depth_staging.Get(), g.depth.Get());
        D3D11_MAPPED_SUBRESOURCE map{};
        if (FAILED(g.context->Map(inspection_depth_staging.Get(), 0, D3D11_MAP_READ, 0, &map)))
        {
            throw Error("inspector depth readback failed");
        }
        inspection_depth.resize(size_t(g.cfg.width) * g.cfg.height);
        for (int row = 0; row < g.cfg.height; ++row)
        {
            std::memcpy(inspection_depth.data() + size_t(row) * g.cfg.width,
                static_cast<const u8*>(map.pData) + size_t(row) * map.RowPitch, size_t(g.cfg.width) * 4);
        }
        g.context->Unmap(inspection_depth_staging.Get(), 0);
    }
    g.context->OMSetRenderTargets(1, g.rtv.GetAddressOf(), g.dsv.Get());
    return view.Get();
}

bool inspector_depth_visible(float retail_x, float retail_y, float depth)
{
    if (inspection_depth.empty())
    {
        return true;
    }
    float sampled;
    if (!inspector_depth_at(retail_x, retail_y, sampled))
    {
        return false;
    }
    return depth <= sampled + 0.00005f;
}

bool inspector_depth_at(float retail_x, float retail_y, float& depth)
{
    if (inspection_depth.empty())
    {
        return false;
    }
    const float scale = std::min(float(g.cfg.width) / 640.f, float(g.cfg.height) / 480.f);
    const int x = inspection_pixel(retail_x, g.cfg.width, scale, 640.f);
    const int y = inspection_pixel(retail_y, g.cfg.height, scale, 480.f);
    if (x < 0 || y < 0 || x >= g.cfg.width || y >= g.cfg.height)
    {
        return false;
    }
    depth = float(inspection_depth[size_t(y) * g.cfg.width + x] & 0xFFFFFFu) / 16777215.f;
    return true;
}

void inspector_begin_preview(const float color[4])
{
    if (!g.ok)
    {
        return;
    }
    g.context->OMSetRenderTargets(1, g.rtv.GetAddressOf(), g.dsv.Get());
    g.context->ClearRenderTargetView(g.rtv.Get(), color);
    g.context->ClearDepthStencilView(g.dsv.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
    D3D11_VIEWPORT vp{};
    vp.Width = float(g.cfg.width);
    vp.Height = float(g.cfg.height);
    vp.MaxDepth = 1.f;
    g.context->RSSetViewports(1, &vp);
}

bool inspector_viewport_readback(RgbaImage& image, int slot)
{
    if (slot < 0 || size_t(slot) >= inspection_color.size() || !inspection_color[size_t(slot)])
    {
        return false;
    }
    const auto& color = inspection_color[size_t(slot)];
    D3D11_TEXTURE2D_DESC desc{};
    color->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(g.device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf())))
    {
        return false;
    }
    g.context->CopyResource(staging.Get(), color.Get());
    D3D11_MAPPED_SUBRESOURCE map{};
    if (FAILED(g.context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &map)))
    {
        return false;
    }
    image.width = desc.Width;
    image.height = desc.Height;
    image.bgra.resize(size_t(desc.Width) * desc.Height * 4);
    for (u32 row = 0; row < desc.Height; ++row)
    {
        std::memcpy(image.bgra.data() + size_t(row) * desc.Width * 4,
            static_cast<const u8*>(map.pData) + size_t(row) * map.RowPitch, size_t(desc.Width) * 4);
    }
    g.context->Unmap(staging.Get(), 0);
    return true;
}

void inspector_finish_frame()
{
    if (!g.ok)
    {
        return;
    }
    const float background[4]{0.055f, 0.065f, 0.08f, 1.f};
    g.context->OMSetRenderTargets(1, g.rtv.GetAddressOf(), nullptr);
    g.context->ClearRenderTargetView(g.rtv.Get(), background);
    debug_ui_draw();
    if (g.swap)
    {
        g.swap->Present(g.cfg.vsync, 0);
    }
    profile_gpu_end();
}
#endif

namespace d3d11
{

bool gfx_readback(RgbaImage& out)
{
    if (!g.ok)
    {
        return false;
    }
    ComPtr<ID3D11Texture2D> src;
    if (g.swap)
    {
        if (FAILED(g.swap->GetBuffer(0, IID_PPV_ARGS(src.GetAddressOf()))))
        {
            return false;
        }
    }
    else
    {
        src = g.color;
    }
    if (!src)
    {
        return false;
    }
    D3D11_TEXTURE2D_DESC desc{};
    src->GetDesc(&desc);
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 6: one staging texture per back-buffer size and format, kept between calls.
    if (g.readback_staging)
    {
        D3D11_TEXTURE2D_DESC held{};
        g.readback_staging->GetDesc(&held);
        if (held.Width != desc.Width || held.Height != desc.Height || held.Format != desc.Format
            || held.SampleDesc.Count != desc.SampleDesc.Count)
        {
            g.readback_staging.Reset();
        }
    }
    if (!g.readback_staging && FAILED(g.device->CreateTexture2D(&desc, nullptr, g.readback_staging.GetAddressOf())))
    {
        return false;
    }
    ComPtr<ID3D11Texture2D> staging = g.readback_staging;
#else
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(g.device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf())))
    {
        return false;
    }
#endif
    g.context->CopyResource(staging.Get(), src.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(g.context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
    {
        return false;
    }
    out.width = int(desc.Width);
    out.height = int(desc.Height);
    out.bgra.resize(size_t(out.width) * size_t(out.height) * 4);
    for (int y = 0; y < out.height; ++y)
    {
        const u8* row = static_cast<const u8*>(mapped.pData) + size_t(y) * mapped.RowPitch;
        std::memcpy(out.bgra.data() + size_t(y) * out.width * 4, row, size_t(out.width) * 4);
    }
    g.context->Unmap(staging.Get(), 0);
    return true;
}

bool gfx_readback_ui(RgbaImage& out)
{
    present_hud_sprites();
    gfx_sprite_clear();
    RgbaImage frame;
    if (!d3d11::gfx_readback(frame))
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
// with no readback. The copy is ordered after the frame's draws so far and before the texture's next use.
bool gfx_copy_ui_into(const RgbaImage& image)
{
#if GBTG_OPTIMIZE
    if (!g.ok || image.width <= 0 || image.height <= 0)
    {
        return false;
    }
    ComPtr<ID3D11Texture2D> source;
    if (g.swap)
    {
        if (FAILED(g.swap->GetBuffer(0, IID_PPV_ARGS(source.GetAddressOf()))))
        {
            return false;
        }
    }
    else
    {
        source = g.color;
    }
    if (!source)
    {
        return false;
    }
    present_hud_sprites();
    gfx_sprite_clear();
    D3D11_TEXTURE2D_DESC desc{};
    source->GetDesc(&desc);
    const GfxUiFrame area = gfx_ui_frame(int(desc.Width), int(desc.Height), g.cfg.widescreen, g.cfg.frame_aspect);
    const ComPtr<ID3D11Texture2D> texture = native_image_texture(image);
    // The image was sized by gfx_ui_frame_size; a resize since then copies the part both hold.
    const D3D11_BOX box{u32(area.left), u32(area.top), 0, u32(area.left + std::min(area.width, image.width)),
        u32(area.top + std::min(area.height, image.height)), 1};
    g.context->CopySubresourceRegion(texture.Get(), 0, 0, 0, 0, source.Get(), 0, &box);
    g.sprites[&image].serial = image.serial;
    return true;
#else
    (void)image;
    return false;
#endif
}

const GfxBackend& backend_table()
{
    static const GfxBackend table = GBTG_GFX_BACKEND_TABLE("d3d11");
    return table;
}

} // namespace d3d11

const GfxBackend& gfx_d3d11_backend()
{
    return d3d11::backend_table();
}

} // namespace gbtg
