#include "render_profile.h"
#include "xbox_vertex_hlsl.h"
#include "xbox_vertex_program.h"
#include "xg_shader_assembly.h"
#if !defined(GBTG_GFX_D3D11)
#error gfx_d3d11.cpp is the PC DirectX 11 backend
#endif

#include "gbtg/gfx.h"
#include "gbtg/movie.h"
#include "gbtg/entity.h"
#include "../gbtg_resource.h"

#if defined(GBTG_INSPECTOR_APP)
#include "../inspector/inspector_graphics.h"
#endif

#include "gbtg/debug_ui.h" // Defines GBTG_DEBUG_UI when the debug UI is enabled.

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
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <unordered_map>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace gbtg
{
namespace
{
constexpr u32 kShaderPermutationFileVersion = 1;
constexpr u32 kShaderTranslatorVersion = 1; // Bump when shader generation or permutation normalization changes.

struct ShaderPermutationManifest
{
    std::vector<std::vector<u32>> vertex_programs;
    std::vector<std::array<u32, 60>> pixel_definitions;
};

bool contains_program(const std::vector<std::vector<u32>>& programs, const std::vector<u32>& program)
{
    return std::find(programs.begin(), programs.end(), program) != programs.end();
}

void read_shader_permutation_manifest(const std::filesystem::path& path, ShaderPermutationManifest& manifest)
{
    std::ifstream input(path);
    if (!input)
    {
        throw Error("cannot open shader permutation manifest: " + path.string());
    }
    std::string line;
    std::string magic;
    u32 version = 0;
    if (!std::getline(input, line))
    {
        throw Error("unsupported or invalid shader permutation manifest header: " + path.string());
    }
    std::istringstream header(line);
    std::string extra;
    if (!(header >> magic >> version) || magic != "GBTG_SHADER_PERMUTATIONS"
        || version != kShaderPermutationFileVersion || (header >> extra))
    {
        throw Error("unsupported or invalid shader permutation manifest header: " + path.string());
    }
    if (!std::getline(input, line))
    {
        throw Error("shader permutation manifest is missing its translator version: " + path.string());
    }
    std::istringstream translator(line);
    std::string label;
    if (!(translator >> label >> version) || label != "translator" || version != kShaderTranslatorVersion
        || (translator >> extra))
    {
        throw Error("shader permutation manifest translator version mismatch: " + path.string());
    }
    size_t line_number = 2;
    while (std::getline(input, line))
    {
        ++line_number;
        if (line.empty())
        {
            continue;
        }
        std::istringstream record(line);
        std::string kind;
        size_t count = 0;
        if (!(record >> kind))
        {
            continue;
        }
        if (kind == "pixel")
        {
            std::array<u32, 60> definition{};
            for (u32& word : definition)
            {
                if (!(record >> std::hex >> word))
                {
                    throw Error("invalid pixel permutation at " + path.string() + ":" + std::to_string(line_number));
                }
            }
            if (std::find(manifest.pixel_definitions.begin(), manifest.pixel_definitions.end(), definition)
                == manifest.pixel_definitions.end())
            {
                manifest.pixel_definitions.push_back(definition);
            }
        }
        else if (kind == "vertex")
        {
            if (!(record >> std::dec >> count) || count == 0 || count > 65536)
            {
                throw Error("invalid vertex permutation length at " + path.string() + ":" + std::to_string(line_number));
            }
            std::vector<u32> program(count);
            for (u32& word : program)
            {
                if (!(record >> std::hex >> word))
                {
                    throw Error("invalid vertex permutation at " + path.string() + ":" + std::to_string(line_number));
                }
            }
            if (!contains_program(manifest.vertex_programs, program))
            {
                manifest.vertex_programs.push_back(std::move(program));
            }
        }
        else
        {
            throw Error("unknown shader permutation record at " + path.string() + ":" + std::to_string(line_number));
        }
        std::string extra;
        if (record >> extra)
        {
            throw Error("extra shader permutation data at " + path.string() + ":" + std::to_string(line_number));
        }
    }
    if (!input.eof())
    {
        throw Error("failed reading shader permutation manifest: " + path.string());
    }
}

void write_shader_permutation_manifest(const std::filesystem::path& path, const ShaderPermutationManifest& manifest)
{
    if (!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream output(path, std::ios::trunc);
    if (!output)
    {
        throw Error("cannot write shader permutation manifest: " + path.string());
    }
    output << "GBTG_SHADER_PERMUTATIONS " << kShaderPermutationFileVersion << "\n";
    output << "translator " << kShaderTranslatorVersion << "\n";
    for (const auto& definition : manifest.pixel_definitions)
    {
        output << "pixel";
        for (u32 word : definition)
        {
            output << ' ' << std::hex << word;
        }
        output << std::dec << '\n';
    }
    for (const auto& program : manifest.vertex_programs)
    {
        output << "vertex " << std::dec << program.size();
        for (u32 word : program)
        {
            output << ' ' << std::hex << word;
        }
        output << std::dec << '\n';
    }
    if (!output)
    {
        throw Error("failed writing shader permutation manifest: " + path.string());
    }
}

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

struct DrawRange
{
    u8 cull_disable = 0;
    std::int8_t node_state_slot = -1;
    u8 node_state_value = 0;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11ShaderResourceView> light;
    i32 tex_index = -1;
    i32 light_index = -1;
    u32 start = 0;
    u32 count = 0;
    float albedo_uv = 0.f;
    float light_uv = 1.f;
    float use_light = 0.f;
    float use_cel = 0.f;
    float color_r = 1.f;
    float color_g = 1.f;
    float color_b = 1.f;
    float alpha_ref = 0.f;
    std::vector<u8> skin;
    i32 render_mode = -1;
    u8 skin_weight_mask = 0;
    bool uses_skin_palette = false;
    std::vector<u16> mtx;
    ComPtr<ID3D11Buffer> material_cb;
    std::array<i32, 4> stages{-1, -1, -1, -1};
    std::array<ComPtr<ID3D11SamplerState>, 4> samplers;

};

struct GpuMesh
{
    ComPtr<ID3D11Buffer> vb;
    ComPtr<ID3D11Buffer> ib;
    std::vector<DrawRange> ranges;
    std::vector<ComPtr<ID3D11ShaderResourceView>> tex_srv;
    std::vector<u32> tex_serial;
};

struct GfxD3D11
{
    GfxConfig cfg;
    bool ok = false;
    bool logged_draw = false;
    HWND hwnd = nullptr;
    bool quit = false;
    Mat4 mvp{};

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
    bool skin = false;
    static constexpr int kMaxBones = 128;
    float bones[kMaxBones * 16]{};
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11SamplerState> light_sampler;
    ComPtr<ID3D11RasterizerState> rast;
    ComPtr<ID3D11RasterizerState> rast_model_cull; // DAT_0032E3D0, D3DCULL_CW
    ComPtr<ID3D11DepthStencilState> depth_state;
    ComPtr<ID3D11DepthStencilState> depth_lequal;
    ComPtr<ID3D11DepthStencilState> depth_lequal_ro;
    ComPtr<ID3D11DepthStencilState> depth_off;
    ComPtr<ID3D11BlendState> blend_opaque;
    ComPtr<ID3D11BlendState> blend_alpha;
    ComPtr<ID3D11BlendState> blend_additive;
    ComPtr<ID3D11BlendState> blend_model_alpha;
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
    std::unordered_map<u32, ComPtr<ID3D11RasterizerState>> native_rasterizers;
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
    } native_bound;
    // The program gfx_native_vertex_program_supported just found, for the draw that follows it.
    std::span<const u32> supported_words;
    ProgramGpu* supported_program = nullptr;
    // GBTG_OPTIMIZE step 3: the last specialized pixel shader and its key (owned by specialized_pixels).
    std::array<u32, 60> last_pixel_key{};
    ID3D11PixelShader* last_pixel = nullptr;
    // GBTG_OPTIMIZE step 6: gfx_readback's staging texture.
    ComPtr<ID3D11Texture2D> readback_staging;
#endif
    std::unordered_map<const Mesh*, GpuMesh> meshes;
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
    // D3DDevice_SetTexture writes device+stage*4+0xf78. The handle is the one
    // this bind passed. Nothing here substitutes a different texture.
    u32 stage_texture[4]{};
    // SetPixelShaderConstant shadow at device+0x8e0. Four floats per register.
    float ps_const[256 * 4]{};
    // Vertex-constant file. SetVertexShaderConstant1/4 copy to (register << 4) + 0x00180D50.
    float vs_const[256 * 4]{};
    u8 vs_src_4d9680[0x40]{}; // DAT_004D9680
    u8 vs_src_4d9710[0x40]{}; // DAT_004D9710
    u32 texture_override = 0; // DAT_004D9764
    const int* texture_by_id = nullptr; // DAT_004D976C
    void (*stage_bind)(int handle, u32 stage) = nullptr; // DAT_004D9754
    void (*material_cb)(int* material) = nullptr; // DAT_004D9750
    // DAT_004d9644. 0x00159AC0 reads the link dword at +4. BSS, so it starts empty.
    u8 tex_list_4d9644[8]{};
    // DAT_0054d950. (id, out) when the list walk finds no node.
    int (*tex_lookup_cb)(int id, int* out) = nullptr;
    // DAT_004d9770. The id 0x0015AD20 passes into 0x0015ABB0.
    int tex_lookup_id = 0;
};

GfxD3D11 g;

struct DialogRenderTarget
{
    u8 slot = 0;
    int width = 0;
    int height = 0;
    bool rendering = false;
    Mat4 saved_camera{};
    D3D11_VIEWPORT saved_viewport{};
    ComPtr<ID3D11Texture2D> color;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11Texture2D> depth;
    ComPtr<ID3D11DepthStencilView> dsv;
};
DialogRenderTarget dialog_target;

// The texture surface native draws render into between gfx_begin_native_target and gfx_end_native_target.
struct NativeRenderTarget
{
    int width = 0;
    int height = 0;
    bool rendering = false;
    D3D11_VIEWPORT saved_viewport{};
    ComPtr<ID3D11Texture2D> color;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11Texture2D> depth;
    ComPtr<ID3D11DepthStencilView> dsv;
    ComPtr<ID3D11Texture2D> staging;
    ComPtr<ID3D11Texture2D> depth_staging;
};
NativeRenderTarget native_target;

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
#if defined(GBTG_DEBUG_UI)
    if (debug_ui_wndproc(hwnd, msg, wp, lp))
    {
        return 0;
    }
    // Escape quits the process. While the menu bar is open it stays with ImGui.
    if (debug_ui_menu_open() && msg == WM_KEYDOWN && wp == VK_ESCAPE)
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

Mat4 mul(const Mat4& a, const Mat4& b)
{
    Mat4 o;
    for (int c = 0; c < 4; ++c)
    {
        for (int r = 0; r < 4; ++r)
        {
            float s = 0.f;
            for (int k = 0; k < 4; ++k)
            {
                s += a.m[k * 4 + r] * b.m[c * 4 + k];
            }
            o.m[c * 4 + r] = s;
        }
    }
    return o;
}

float dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(const Vec3& a, const Vec3& b)
{
    return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vec3 normalize(Vec3 v)
{
    const float len = std::sqrt(dot(v, v));
    if (len < 1e-8f)
    {
        return Vec3{0.f, 0.f, 1.f};
    }
    return Vec3{v.x / len, v.y / len, v.z / len};
}

Mat4 look_at(const Vec3& eye, const Vec3& at, const Vec3& up)
{
    const Vec3 z = normalize(Vec3{at.x - eye.x, at.y - eye.y, at.z - eye.z});
    // The cutscene basis is aimed with local -Z, but cross(up, z) paints the
    // attract shot as a horizontal mirror (cover text reads backwards).
    const Vec3 x = normalize(cross(z, up));
    const Vec3 y = cross(x, z);
    Mat4 m{};
    m.m[0] = x.x;
    m.m[1] = y.x;
    m.m[2] = z.x;
    m.m[4] = x.y;
    m.m[5] = y.y;
    m.m[6] = z.y;
    m.m[8] = x.z;
    m.m[9] = y.z;
    m.m[10] = z.z;
    m.m[12] = -dot(x, eye);
    m.m[13] = -dot(y, eye);
    m.m[14] = -dot(z, eye);
    m.m[15] = 1.f;
    return m;
}

Mat4 perspective(float fov_y, float aspect, float zn, float zf)
{
    const float y = 1.f / std::tan(fov_y * 0.5f);
    const float x = y / (aspect > 0.f ? aspect : 1.f);
    Mat4 m{};
    m.m[0] = x;
    m.m[5] = y;
    m.m[10] = zf / (zf - zn);
    m.m[11] = 1.f;
    m.m[14] = -zn * zf / (zf - zn);
    return m;
}

bool compile_shader(const char* src, const char* entry, const char* profile, ComPtr<ID3DBlob>& blob, bool strict = false)
{
    ComPtr<ID3DBlob> err;
    const HRESULT hr = D3DCompile(src, std::strlen(src), "gbtg", nullptr, nullptr, entry, profile, D3DCOMPILE_OPTIMIZATION_LEVEL1 | (strict ? D3DCOMPILE_IEEE_STRICTNESS : 0),
                                  0, blob.GetAddressOf(), err.GetAddressOf());
    if (FAILED(hr))
    {
        if (err)
        {
            std::cerr << "hlsl: " << static_cast<const char*>(err->GetBufferPointer()) << "\n";
        }
        return false;
    }
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

bool upload_mesh(const Mesh& mesh, GpuMesh& gpu)
{
    if (mesh.positions.empty() || mesh.indices.size() < 3)
    {
        return false;
    }
    std::vector<GpuVert> verts(mesh.positions.size());
    for (size_t i = 0; i < mesh.positions.size(); ++i)
    {
        verts[i].x = mesh.positions[i].x;
        verts[i].y = mesh.positions[i].y;
        verts[i].z = mesh.positions[i].z;
        // D3DCOLOR bytes are BGRA; the D3D11 input format reads RGBA bytes.
        auto rgba = [](u32 argb)
        {
            return (argb & 0xff00ff00u) | ((argb >> 16) & 255u) | ((argb & 255u) << 16);
        };
        if (i < mesh.diffuse.size())
        {
            verts[i].diffuse = rgba(mesh.diffuse[i]);
        }
        if (i < mesh.specular.size())
        {
            verts[i].specular = rgba(mesh.specular[i]);
        }
        if (i < mesh.uvs.size())
        {
            verts[i].u0 = mesh.uvs[i].u;
            verts[i].v0 = mesh.uvs[i].v;
        }
        if (i < mesh.uv1.size())
        {
            verts[i].u1 = mesh.uv1[i].u;
            verts[i].v1 = mesh.uv1[i].v;
        }
        if (i < mesh.normals.size())
        {
            verts[i].nx = mesh.normals[i].x;
            verts[i].ny = mesh.normals[i].y;
            verts[i].nz = mesh.normals[i].z;
        }
        if (i < mesh.uv2.size())
        {
            verts[i].u2 = mesh.uv2[i].u;
            verts[i].v2 = mesh.uv2[i].v;
        }
        if (i < mesh.uv3.size())
        {
            verts[i].u3 = mesh.uv3[i].u;
            verts[i].v3 = mesh.uv3[i].v;
        }
        if (i < mesh.bones.size())
        {
            verts[i].bone = float(mesh.bones[i]);
        }
        if (i < mesh.bone2.size())
        {
            verts[i].bone2 = float(mesh.bone2[i]);
        }
        if (i < mesh.weight0.size() && i < mesh.weight1.size())
        {
            verts[i].w0 = mesh.weight0[i];
            verts[i].w1 = mesh.weight1[i];
        }
        if (i < mesh.bone3.size() && i < mesh.bone4.size())
        {
            verts[i].bone3 = float(mesh.bone3[i]);
            verts[i].bone4 = float(mesh.bone4[i]);
        }
        if (i < mesh.weight2.size() && i < mesh.weight3.size())
        {
            verts[i].w2 = mesh.weight2[i];
            verts[i].w3 = mesh.weight3[i];
        }
    }
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = u32(verts.size() * sizeof(GpuVert));
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = verts.data();
    if (FAILED(g.device->CreateBuffer(&bd, &sd, gpu.vb.GetAddressOf())))
    {
        return false;
    }
    bd.ByteWidth = u32(mesh.indices.size() * sizeof(u32));
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    sd.pSysMem = mesh.indices.data();
    if (FAILED(g.device->CreateBuffer(&bd, &sd, gpu.ib.GetAddressOf())))
    {
        return false;
    }

    gpu.tex_srv.clear();
    gpu.tex_serial.clear();
    gpu.tex_srv.reserve(mesh.textures.size());
    gpu.tex_serial.reserve(mesh.textures.size());
    for (const auto& img : mesh.textures)
    {
        gpu.tex_srv.push_back(upload_bgra(img.bgra.data(), img.width, img.height));
        gpu.tex_serial.push_back(img.serial);
    }

    if (mesh.submeshes.empty())
    {
        DrawRange range;
        range.start = 0;
        range.count = u32(mesh.indices.size());
        gpu.ranges.push_back(std::move(range));
        return true;
    }
    for (const auto& sub : mesh.submeshes)
    {
        if (sub.index_count < 3 || sub.texture == -2)
        {
            continue;
        }
        DrawRange range;
        range.start = sub.first_index;
        range.cull_disable = sub.cull_disable;
        range.node_state_slot = sub.node_state_slot;
        range.node_state_value = sub.node_state_value;
        range.count = sub.index_count;
        range.albedo_uv = float(sub.albedo_uv);
        range.light_uv = float(sub.lightmap_uv);
        if (sub.texture >= 0 && size_t(sub.texture) < gpu.tex_srv.size() && gpu.tex_srv[size_t(sub.texture)])
        {
            range.tex_index = sub.texture;
            range.srv = gpu.tex_srv[size_t(sub.texture)];
        }
        if (sub.celmap >= 0 && size_t(sub.celmap) < gpu.tex_srv.size() && gpu.tex_srv[size_t(sub.celmap)])
        {
            range.light_index = sub.celmap;
            range.light = gpu.tex_srv[size_t(sub.celmap)];
            range.use_cel = 1.f;
        }
        else if (sub.lightmap >= 0 && size_t(sub.lightmap) < gpu.tex_srv.size() && gpu.tex_srv[size_t(sub.lightmap)])
        {
            range.light_index = sub.lightmap;
            range.light = gpu.tex_srv[size_t(sub.lightmap)];
            range.use_light = 1.f;
        }
        if (sub.has_pixel_shader && sub.direct_uv)
        {
            struct MaterialConstants
            {
                std::array<u32, 60> words;
                std::array<u32, 4> uv;
                std::array<u32, 4> control;
            };
            MaterialConstants constants{sub.pixel_shader,
                {sub.stage_uv[0], sub.stage_uv[1], sub.stage_uv[2], sub.stage_uv[3]},
                {sub.colour_source[0], sub.colour_source[1], 0, 0}};
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = sizeof(constants);
            desc.Usage = D3D11_USAGE_IMMUTABLE;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            D3D11_SUBRESOURCE_DATA data{};
            data.pSysMem = &constants;
            if (FAILED(g.device->CreateBuffer(&desc, &data, range.material_cb.GetAddressOf())))
            {
                return false;
            }
            range.stages = sub.stages;
            for (size_t stage = 0; stage < 4; ++stage)
            {
                D3D11_SAMPLER_DESC sampler{};
                sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
                sampler.AddressU = D3D11_TEXTURE_ADDRESS_MODE(sub.address_u[stage]);
                sampler.AddressV = D3D11_TEXTURE_ADDRESS_MODE(sub.address_v[stage]);
                sampler.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
                sampler.MaxLOD = D3D11_FLOAT32_MAX;
                sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
                if (FAILED(g.device->CreateSamplerState(&sampler, range.samplers[stage].GetAddressOf())))
                {
                    return false;
                }
            }
        }
        range.color_r = sub.color.x;
        range.color_g = sub.color.y;
        range.color_b = sub.color.z;
        range.alpha_ref = float(sub.alpha_ref);
        range.render_mode = sub.render_mode;
        range.skin_weight_mask = sub.skin_weight_mask;
        range.uses_skin_palette = sub.uses_skin_palette;
        range.skin = sub.skin;
        range.mtx = sub.mtx;
        gpu.ranges.push_back(std::move(range));
    }
    return !gpu.ranges.empty();
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

// Xbox 0x0005E0C0
// Three D3DDevice_SetRenderState_Simple pushes, each followed by the device shadow:
//   0x40300 = 1  D3DRS_ALPHATESTENABLE
//   0x40340 = 5  D3DRS_ALPHAREF
//   0x40304 = 1  D3DRS_ALPHABLENDENABLE
// Alpha blend factors stay at the device default (SRCALPHA / INVSRCALPHA). ALPHAFUNC is not written.
// Xbox 0x0005E0C0
void d3d_enable_alpha_blending()
{
    g.rs_alpha_test = 1;
    g.rs_alpha_ref = 5;
    g.rs_alpha_blend = 1;
    if (!g.ok)
    {
        return;
    }
    g.context->OMSetBlendState(g.blend_alpha.Get(), nullptr, 0xFFFFFFFFu);
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

// Xbox 0x0005E2A0
// Stage 0 combiner becomes diffuse * texture. The game writes the D3D shadow, then
// SetTextureState_TexCoordIndex(0, 0) and SetVertexShader(2) (D3DFVF_XYZ).
//   COLOROP / ALPHAOP = 4           D3DTOP_MODULATE
//   COLORARG2 / ALPHAARG2 = 2       D3DTA_TEXTURE
//   ADDRESSU / ADDRESSV = 3         D3DTADDRESS_CLAMP  (ADDRESSU is D3D_g_DeferredTextureState)
//   MAXMIPLEVEL = 0
//   MINFILTER / MAGFILTER / MIPFILTER = 2   D3DTEXF_LINEAR
//   dirty |= 0xC01                  stage 0 plus bits 0x400 and 0x800
//   TEXTURETRANSFORMFLAGS = 0
//   TEXCOORDINDEX = 0
//   COLORARG1 / ALPHAARG1 = 0       D3DTA_DIFFUSE
//   dirty |= 0x800
// Xbox 0x0005E2A0
void d3d_configure_ui_texture()
{
    GfxD3D11::StageRs& slot = g.stages[0];
    slot.color_op = 4;
    slot.alpha_op = 4;
    slot.color_arg2 = 2;
    slot.alpha_arg2 = 2;
    slot.address_u = 3;
    slot.address_v = 3;
    slot.max_mip_level = 0;
    slot.min_filter = 2;
    slot.mag_filter = 2;
    slot.mip_filter = 2;
    g.rs_dirty |= 0xc01u;
    slot.tex_transform = 0;
    slot.texcoord_index = 0;
    slot.color_arg1 = 0;
    g.rs_dirty |= 0x800u;
    slot.alpha_arg1 = 0;
    g.vertex_shader = 2;
    if (!g.ok)
    {
        return;
    }
    g.context->PSSetSamplers(0, 1, g.light_sampler.GetAddressOf());
}

const u8* as_u8(int bits)
{
    return reinterpret_cast<const u8*>(static_cast<uintptr_t>(static_cast<u32>(bits)));
}

u8* as_u8_mut(int bits)
{
    return reinterpret_cast<u8*>(static_cast<uintptr_t>(static_cast<u32>(bits)));
}

// Xbox 0x00159AC0
// *out = *(node + 4). The list link used by 0x0015ABB0.
// Xbox 0x00159AC0
void d3d_next_texture_record(const u8* node, int* out)
{
    std::memcpy(out, node + 4, 4);
}

// Xbox 0x0015ABB0
// Walks the list headed at DAT_004d9644. A node whose dword at +0xc4 equals
// param_1 stores 1 at +0x68 and writes node+0xc8 through param_2.
//
// DAT_0054d950, when it is not null, is called as (param_1, param_2). The
// decompiler dropped the second operand of the recursive call: add esp, 4
// leaves param_2 under the return value, so the call is
// d3d_resolve_texture_record(result, param_2).
// Xbox 0x0015ABB0
void d3d_resolve_texture_record(int param_1, int* param_2)
{
    int local_4 = 0;
    d3d_next_texture_record(g.tex_list_4d9644, &local_4);
    while (local_4 != 0)
    {
        u8* node = as_u8_mut(local_4);
        int key = 0;
        std::memcpy(&key, node + 0xc4, 4);
        if (key == param_1)
        {
            const int one = 1;
            std::memcpy(node + 0x68, &one, 4);
            *param_2 = local_4 + 0xc8;
            return;
        }
        d3d_next_texture_record(node, &local_4);
    }
    if (g.tex_lookup_cb != nullptr)
    {
        const int found = g.tex_lookup_cb(param_1, param_2);
        d3d_resolve_texture_record(found, param_2);
        return;
    }
    *param_2 = 0;
}

// Xbox 0x0015AD20
// Material bind. param_1 is the shader-param object (dword fields). param_2 is
// the draw table: dword 0 is the 0x44-stride constant rows, dword 4 is the
// per-stage texture handles.
//
// Pixel constants (count at +0x14) are the bytes at param_1[0], register 0
// first, each byte times the float at 0x0032B154 (0x3B808081).
// Stages (count at +0x0c) are 0x1c records at param_1[2]. The shadow is
// DAT_00181970, stride 0x80. Each stage ORs 1<<stage into DAT_00181968.
// Bump-env types 0x16, 0x17, 0x19, 0x18 land at +0x58, +0x5C, +0x64, +0x60.
//
// SetVertexShaderConstant1/4 take the register in ecx and the source in edx.
// The decompiler dropped those operands. Constant1 is *(param_1+0x34)+0x60
// from the four floats at 0x00336120. Constant4's source select is the
// compares at 0x0015AF61, then the register is that dword + 0x60.
//
// Byte +0x18 == 0 clears ALPHATESTENABLE. Any other value sets it, stores
// D3DCMP_GREATER (0x204) as ALPHAFUNC, and stores the byte as ALPHAREF.
// Record byte +7 set calls 0x0015ABB0(DAT_004d9770) and SetTexture binds
// the pointer that call stores.
// Xbox 0x0015AD20
void d3d_apply_material_state(int* param_1, int param_2)
{
    float byte_scale = 0.f;
    const u32 scale_bits = 0x3B808081u;
    std::memcpy(&byte_scale, &scale_bits, sizeof(byte_scale));
    // DAT_00336120. SetVertexShaderConstant1 copies these four floats.
    const float vs_one[4] = {0.5f, 0.5f, 1.f, 1.f};

    u32 index = 0;
    if (param_1[5] != 0)
    {
        const u8* colors = as_u8(param_1[0]);
        do
        {
            const u8* px = colors + index * 4u;
            g.ps_const[index * 4u + 0] = float(px[0]) * byte_scale;
            g.ps_const[index * 4u + 1] = float(px[1]) * byte_scale;
            g.ps_const[index * 4u + 2] = float(px[2]) * byte_scale;
            g.ps_const[index * 4u + 3] = float(px[3]) * byte_scale;
            index = index + 1;
        } while (index < static_cast<u32>(param_1[5]));
    }

    index = 0;
    if (param_1[3] != 0)
    {
        u32 rec_off = 0;
        do
        {
            const u8* rec = as_u8(param_1[2]) + rec_off;
            GfxD3D11::StageRs& slot = g.stages[index];
            const u32 bit = 1u << (index & 0x1fu);
            slot.address_u = rec[4];
            g.rs_dirty |= bit;
            slot.address_v = rec[5];
            g.rs_dirty |= bit;
            slot.address_w = rec[6];
            slot.max_mip_level = 0;
            slot.min_filter = 2;
            slot.mip_filter = 2;
            g.rs_dirty |= bit;
            slot.mag_filter = 2;
            if (*reinterpret_cast<const i16*>(rec + 0x0a) == 0)
            {
                g.rs_dirty |= bit;
                slot.color_sign = 0;
                if (*reinterpret_cast<const i16*>(rec + 0x08) >= 0)
                {
                    u32 bits = 0;
                    std::memcpy(&bits, rec + 0x0c, 4);
                    slot.bump[0] = bits;
                    std::memcpy(&bits, rec + 0x10, 4);
                    slot.bump[1] = bits;
                    std::memcpy(&bits, rec + 0x14, 4);
                    slot.bump[3] = bits;
                    std::memcpy(&bits, rec + 0x18, 4);
                    slot.bump[2] = bits;
                }
            }
            else
            {
                g.rs_dirty |= bit;
                slot.color_sign = 0xf0000000u;
            }
            if (rec[7] == 0)
            {
                const u8* mat = reinterpret_cast<const u8*>(param_1);
                if (static_cast<u32>(param_1[0x10]) == index && mat[0x1b] != 0 && g.texture_override != 0)
                {
                    g.stage_texture[index] = g.texture_override;
                    const u32 vs_reg = static_cast<u32>(param_1[0x0d]) + 0x60u;
                    std::memcpy(g.vs_const + vs_reg * 4u, vs_one, sizeof(vs_one));
                }
                else
                {
                    const i32 id = *reinterpret_cast<const i32*>(rec);
                    if (id != -1)
                    {
                        const u8* draw = as_u8(param_2);
                        int table_bits = 0;
                        std::memcpy(&table_bits, draw + 4, 4);
                        const int* table = reinterpret_cast<const int*>(as_u8(table_bits));
                        int handle = table[index];
                        if (handle == 0)
                        {
                            handle = g.texture_by_id[id];
                        }
                        g.stage_bind(handle, index);
                    }
                }
            }
            else
            {
                int texture = 0;
                d3d_resolve_texture_record(g.tex_lookup_id, &texture);
                g.stage_texture[index] = static_cast<u32>(texture);
            }
            index = index + 1;
            rec_off = rec_off + 0x1c;
        } while (index < static_cast<u32>(param_1[3]));
    }

    index = 0;
    if (param_1[4] != 0)
    {
        u32 row = 0;
        u32 block = 0;
        do
        {
            const void* src;
            u32 vs_reg;
            if (param_2 != 0)
            {
                const u8* draw = as_u8(param_2);
                int rows_bits = 0;
                std::memcpy(&rows_bits, draw, 4);
                const u8* rows = as_u8(rows_bits);
                if (rows[row + 0x40] != 0)
                {
                    const u8* constants = as_u8(param_1[1]);
                    vs_reg = *reinterpret_cast<const u32*>(constants + block + 0x40);
                    src = rows + row;
                }
                else
                {
                    const u8* rec = as_u8(param_1[1]) + block;
                    const u32 kind = *reinterpret_cast<const u32*>(rec + 0x44);
                    vs_reg = *reinterpret_cast<const u32*>(rec + 0x40);
                    if (kind == 1)
                    {
                        src = g.vs_src_4d9710;
                    }
                    else if (kind == 2)
                    {
                        src = g.vs_src_4d9680;
                    }
                    else
                    {
                        src = rec;
                    }
                }
            }
            else
            {
                const u8* rec = as_u8(param_1[1]) + block;
                const u32 kind = *reinterpret_cast<const u32*>(rec + 0x44);
                vs_reg = *reinterpret_cast<const u32*>(rec + 0x40);
                if (kind == 1)
                {
                    src = g.vs_src_4d9710;
                }
                else if (kind == 2)
                {
                    src = g.vs_src_4d9680;
                }
                else
                {
                    src = rec;
                }
            }
            vs_reg = vs_reg + 0x60u;
            std::memcpy(g.vs_const + vs_reg * 4u, src, 0x40);
            index = index + 1;
            row = row + 0x44;
            block = block + 0x48;
        } while (index < static_cast<u32>(param_1[4]));
    }

    const u8* mat = reinterpret_cast<const u8*>(param_1);
    if (mat[0x18] == 0)
    {
        g.rs_alpha_test = 0;
    }
    else
    {
        g.rs_alpha_test = 1;
        g.rs_alpha_func = 0x204;
        g.rs_alpha_ref = mat[0x18];
    }
    if (g.material_cb != nullptr)
    {
        g.material_cb(param_1);
    }
}

void refresh_mesh_textures(const Mesh& mesh, GpuMesh& gpu)
{
    if (gpu.tex_serial.size() != mesh.textures.size() || gpu.tex_srv.size() != mesh.textures.size())
    {
        return;
    }
    bool changed = false;
    for (size_t i = 0; i < mesh.textures.size(); ++i)
    {
        if (gpu.tex_serial[i] == mesh.textures[i].serial)
        {
            continue;
        }
        gpu.tex_srv[i] = upload_bgra(mesh.textures[i].bgra.data(), mesh.textures[i].width, mesh.textures[i].height);
        gpu.tex_serial[i] = mesh.textures[i].serial;
        changed = true;
    }
    if (!changed)
    {
        return;
    }
    for (DrawRange& range : gpu.ranges)
    {
        if (range.tex_index >= 0 && size_t(range.tex_index) < gpu.tex_srv.size())
        {
            range.srv = gpu.tex_srv[size_t(range.tex_index)];
        }
        if (range.light_index >= 0 && size_t(range.light_index) < gpu.tex_srv.size())
        {
            range.light = gpu.tex_srv[size_t(range.light_index)];
        }
    }
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
        RECT rect{0, 0, width, height};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
        g.hwnd = CreateWindowW(wc.lpszClassName, L"Grabbed by the Ghoulies", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, wc.hInstance, nullptr);
        if (!g.hwnd)
        {
            std::cerr << "d3d11: CreateWindow failed\n";
            return false;
        }
    }

    DXGI_SWAP_CHAIN_DESC sc{};
    sc.BufferCount = 1;
    sc.BufferDesc.Width = u32(width);
    sc.BufferDesc.Height = u32(height);
    sc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc.OutputWindow = g.hwnd;
    sc.SampleDesc.Count = 1;
    sc.Windowed = TRUE;
    sc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

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
        std::cerr << "d3d11: device create failed\n";
        gfx_shutdown();
        return false;
    }
    if (render_profile::enabled)
    {
        ComPtr<IDXGIDevice> dxgi;
        ComPtr<IDXGIAdapter> adapter;
        DXGI_ADAPTER_DESC desc{};
        if (SUCCEEDED(g.device.As(&dxgi)) && SUCCEEDED(dxgi->GetAdapter(adapter.GetAddressOf())) && SUCCEEDED(adapter->GetDesc(&desc)))
        {
            std::wcout << L"profile adapter=" << desc.Description << L" vendor=" << desc.VendorId << L" device=" << desc.DeviceId << L"\n";
        }
    }
    if (!create_targets(width, height))
    {
        std::cerr << "d3d11: render target failed\n";
        gfx_shutdown();
        return false;
    }

    const char* hlsl = R"(
cbuffer Camera : register(b0) {
    float4x4 mvp;
    float4 uv_sel;   // x = albedo uv set, y = light uv set, z = lightmap, w = cel ramp
    float4 light_dir; // xyz draw colour (0x5e380), w = rigid skin on
    float4 alpha_rs; // x = ALPHATESTENABLE, y = ALPHAREF, z = ALPHAFUNC (0x0005E0C0)
    float4 tint;
    float4 skin_mask;
    float4 native_uv_scale;
};
cbuffer Bones : register(b1) {
    float4x4 joints[128];
};
Texture2D g_tex : register(t0);
Texture2D g_light : register(t1);
SamplerState g_samp : register(s0);
SamplerState g_light_samp : register(s1);
struct VSIn { float3 pos : POSITION; float2 uv0 : TEXCOORD0; float2 uv1 : TEXCOORD1; float3 nrm : NORMAL; float2 bone : TEXCOORD2; float2 wt : TEXCOORD3; float2 uv2 : TEXCOORD4; float4 diffuse : COLOR0; float4 specular : COLOR1; float2 uv3 : TEXCOORD5; float2 extra_bone : TEXCOORD6; float2 extra_wt : TEXCOORD7; };
struct VSOut { float4 pos : SV_Position; float2 uv0 : TEXCOORD0; float2 uv1 : TEXCOORD1; float3 nrm : TEXCOORD2; float2 uv2 : TEXCOORD3; float4 diffuse : COLOR0; float4 specular : COLOR1; float2 uv3 : TEXCOORD4; float4 tq2 : TEXCOORD5; float4 tq3 : TEXCOORD6; float fog : TEXCOORD7; float4 tq0 : TEXCOORD8; float4 tq1 : TEXCOORD9; };
VSOut vs_main(VSIn i) {
    VSOut o;
    float3 p = i.pos;
    float3 nrm = i.nrm;
    if (light_dir.w > 0.5) {
        // Blended skin shader: R2 = c[v4.x] * v5.x; MAD c[v4.y] * v5.y; DP4.
        // Weight (1, 0) is the rigid path, so one-bone meshes stay on bone.x.
        int b0 = clamp((int)i.bone.x, 0, 127);
        int b1 = clamp((int)i.bone.y, 0, 127);
        int b2 = clamp((int)i.extra_bone.x, 0, 127);
        int b3 = clamp((int)i.extra_bone.y, 0, 127);
        float4 wt = float4(i.wt, i.extra_wt) * skin_mask;
        if (dot(skin_mask, skin_mask) == 0)
        {
            wt = float4(1, 0, 0, 0);
        }
        p = mul(joints[b0], float4(i.pos, 1)).xyz * wt.x + mul(joints[b1], float4(i.pos, 1)).xyz * wt.y
          + mul(joints[b2], float4(i.pos, 1)).xyz * wt.z + mul(joints[b3], float4(i.pos, 1)).xyz * wt.w;
        nrm = mul(joints[b0], float4(i.nrm, 0)).xyz * wt.x + mul(joints[b1], float4(i.nrm, 0)).xyz * wt.y
            + mul(joints[b2], float4(i.nrm, 0)).xyz * wt.z + mul(joints[b3], float4(i.nrm, 0)).xyz * wt.w;
    }
    o.pos = mul(mvp, float4(p, 1));
    o.uv0 = i.uv0;
    o.uv1 = i.uv1;
    o.uv2 = i.uv2;
    o.uv3 = i.uv3;
    o.tq2 = float4(i.uv2, 0, 1);
    o.tq3 = float4(i.uv3, 0, 1);
    o.tq0 = float4(i.uv0, 0, 1);
    o.tq1 = float4(i.uv1, 0, 1);
    o.nrm = nrm;
    o.diffuse = i.diffuse;
    o.specular = i.specular;
    o.fog = 1;
    return o;
}
float4 ps_main(VSOut i) : SV_Target {
    float2 uv_a = uv_sel.x < 0.5 ? i.uv0 : i.uv1;
    float2 uv_l = uv_sel.y < 0.5 ? i.uv0 : i.uv1;
    // alpha_rs.w is 1 only when register 0 has a bound texture. Unbound does not sample.
    float4 albedo = float4(1.0, 1.0, 1.0, 1.0);
    if (alpha_rs.w > 0.5) {
        albedo = g_tex.Sample(g_samp, uv_a);
    }
    // 0x0015AD20 register 0. With no texture, the draw colour is the constant.
    float3 light = light_dir.xyz;
    if (uv_sel.w > 0.5 && dot(i.nrm, i.nrm) > 1e-8) {
        // RARE_celmap0 is a ramp along U. The mesh constant is a colour, not a
        // direction; the ramp coordinate is the world-up component of the normal
        // (the unit +Y FUN_0013e520 stores as the LookAt up).
        float3 n = normalize(i.nrm);
        float u = saturate(0.5 + 0.5 * n.y);
        light = g_light.Sample(g_light_samp, float2(u, 0.25)).rgb * light_dir.xyz;
    } else if (uv_sel.z > 0.5) {
        // FUN_000644f0 stores D3DTOP_MODULATE (4) at the stage colour ops.
        light = g_light.Sample(g_light_samp, uv_l).rgb * light_dir.xyz;
    }
    float4 color = float4(albedo.rgb * light, albedo.a) * tint;
    // D3DRS_ALPHATESTENABLE / ALPHAREF from 0x0005E0C0. ALPHAFUNC stays at the
    // shadow (0 until programmed). 0x200..0x207 are the Xbox D3DCMP values.
    if (alpha_rs.x > 0.5 && alpha_rs.z >= 512.0) {
        float a = round(color.a * 255.0);
        float refv = alpha_rs.y;
        float f = alpha_rs.z;
        bool keep = true;
        if (f < 512.5) keep = false;
        else if (f < 513.5) keep = a < refv;
        else if (f < 514.5) keep = a == refv;
        else if (f < 515.5) keep = a <= refv;
        else if (f < 516.5) keep = a > refv;
        else if (f < 517.5) keep = a != refv;
        else if (f < 518.5) keep = a >= refv;
        if (!keep) discard;
    }
    return color;
}
)";
    std::string model_hlsl = std::string(hlsl) + R"(
// NV2A register-combiner fields in the 240-byte shader definition. This
// interprets the asset's operations; sampler names do not prescribe blending.
cbuffer Material : register(b2)
{
    uint4 ps_words[15];
    uint4 ps_uv;
    uint4 ps_control;
    // Per stage (xy, zw): 1/width and 1/height for a linear texture, which the NV2A samples in texels.
    // Zero (a buffer without these words) means normalized coordinates.
    float4 ps_uv_scale[2];
    // x: D3DRS_FOGENABLE, y: D3DRS_FOGCOLOR.
    uint4 ps_fog;
};
Texture2D stage_tex[4] : register(t0);
SamplerState stage_samp[4] : register(s0);
uint ps_word(uint n)
{
    return ps_words[n / 4][n % 4];
}
float4 argb(uint x)
{
    return float4((x >> 16) & 255, (x >> 8) & 255, x & 255, x >> 24) / 255.0;
}
float4 ps_constant(uint stage, bool second)
{
    uint control = ps_word(53);
    uint unique = second ? 65536 : 4096;
    uint n = (control & unique) != 0 ? stage : 0;
    return argb(ps_word((second ? 18 : 10) + n));
}
float4 comb_input(uint input, float4 regs[16], float4 c0, float4 c1)
{
    uint reg = input & 15;
    float4 value = reg == 1 ? c0 : (reg == 2 ? c1 : regs[reg]);
    uint mapping = input >> 5;
    if (mapping == 0)
    {
        return max(value, 0);
    }
    if (mapping == 1)
    {
        return 1 - max(value, 0);
    }
    if (mapping == 2)
    {
        return 2 * max(value, 0) - 1;
    }
    if (mapping == 3)
    {
        return 1 - 2 * max(value, 0);
    }
    if (mapping == 4)
    {
        return max(value, 0) - 0.5;
    }
    if (mapping == 5)
    {
        return 0.5 - max(value, 0);
    }
    return mapping == 6 ? value : -value;
}
float3 rgb_input(uint input, float4 regs[16], float4 c0, float4 c1)
{
    float4 value = comb_input(input, regs, c0, c1);
    return (input & 16) != 0 ? value.aaa : value.rgb;
}
float alpha_input(uint input, float4 regs[16], float4 c0, float4 c1)
{
    float4 value = comb_input(input, regs, c0, c1);
    return (input & 16) != 0 ? value.a : value.b;
}
float4 output_map(float4 value, uint output)
{
    uint mapping = (output >> 15) & 7;
    if ((mapping & 1) != 0)
    {
        value -= 0.5;
    }
    uint shift = mapping >> 1;
    if (shift == 1)
    {
        value *= 2;
    }
    else if (shift == 2)
    {
        value *= 4;
    }
    else if (shift == 3)
    {
        value *= 0.5;
    }
    return clamp(value, -1, 1);
}
// PROJECT2D samples at (s / q, t / q): q is the coordinate's w, 1 unless a vertex program writes it.
float2 stage_coord(VSOut i, uint stage)
{
    uint uv = ps_uv[stage];
    float4 full = uv == 0 ? i.tq0 : (uv == 1 ? i.tq1 : (uv == 2 ? i.tq2 : i.tq3));
    float2 coord = full.xy / full.w;
    float4 pair = ps_uv_scale[stage / 2];
    float2 scale = (stage & 1) != 0 ? pair.zw : pair.xy;
    return any(scale != 0) ? coord * scale : coord;
}
// PSDotMapping (word 55, stage n at bits (n - 1) * 4): 0 ZERO_TO_ONE, 4 HILO_1 (the 16-bit A:R and G:B halves
// of the texel's 32 bits, and 1).
float3 ps_dotmap(uint stage, float4 c)
{
    uint mapping = (ps_word(55) >> ((stage - 1) * 4)) & 7;
    if (mapping == 4)
    {
        uint4 b = uint4(round(saturate(c) * 255));
        return float3(float((b.a << 8) | b.r) / 65535.0, float((b.g << 8) | b.b) / 65535.0, 1.0);
    }
    return c.rgb;
}
// D3D's spec/fog final combiner, used when a definition has none: FOG.a * R0 + (1 - FOG.a) * FOG.rgb.
float3 native_fog(float3 color, float factor)
{
    return ps_fog.x != 0 ? lerp(argb(ps_fog.y).rgb, color, saturate(factor)) : color;
}
float4 ps_model_core(VSOut i, inout float depth);
float4 ps_model(VSOut i) : SV_Target
{
    float depth = i.pos.z;
    return ps_model_core(i, depth);
}
struct DepthOut { float4 color : SV_Target; float depth : SV_Depth; };
// DOT_ZW on stage 3 replaces the fragment depth: z / w in Xbox Z-buffer units (D24 here, 0xFFFFFF).
DepthOut ps_model_depth(VSOut i)
{
    DepthOut o;
    o.depth = i.pos.z;
    o.color = ps_model_core(i, o.depth);
    return o;
}
float4 ps_model_core(VSOut i, inout float depth)
{
    float4 regs[16];
    [unroll]
    for (uint n = 0; n < 16; ++n)
    {
        regs[n] = 0;
    }
    // The shared host vertex path supplies the current draw colour. Dynamic
    // Xbox lighting splices are tracked as a separate unresolved vertex path.
    regs[4] = float4(light_dir.xyz, 1);
    if (ps_control.x < 2)
    {
        regs[4] = ps_control.x == 0 ? i.diffuse : i.specular;
    }
    if (ps_control.y < 2)
    {
        regs[5] = ps_control.y == 0 ? i.diffuse : i.specular;
    }
    // NONE (0): the stage's texture register is opaque black (the glow depth stamp's alpha comes from t0).
    [unroll]
    for (uint stage_none = 0; stage_none < 4; ++stage_none)
    {
        if (((ps_word(54) >> (stage_none * 5)) & 31) == 0)
        {
            regs[8 + stage_none] = float4(0, 0, 0, 1);
        }
    }
    if ((ps_word(54) & 31) == 1)
    {
        regs[8] = stage_tex[0].Sample(stage_samp[0], stage_coord(i, 0));
    }
    if (((ps_word(54) >> 5) & 31) == 1)
    {
        regs[9] = stage_tex[1].Sample(stage_samp[1], stage_coord(i, 1));
    }
    if (((ps_word(54) >> 10) & 31) == 1)
    {
        regs[10] = stage_tex[2].Sample(stage_samp[2], stage_coord(i, 2));
    }
    if (((ps_word(54) >> 15) & 31) == 1)
    {
        regs[11] = stage_tex[3].Sample(stage_samp[3], stage_coord(i, 3));
    }
    // PASSTHRU (4): the stage's texture register is its texture coordinates, clamped to [0, 1].
    if ((ps_word(54) & 31) == 4)
    {
        regs[8] = saturate(i.tq0);
    }
    if (((ps_word(54) >> 5) & 31) == 4)
    {
        regs[9] = saturate(i.tq1);
    }
    if (((ps_word(54) >> 10) & 31) == 4)
    {
        regs[10] = saturate(i.tq2);
    }
    if (((ps_word(54) >> 15) & 31) == 4)
    {
        regs[11] = saturate(i.tq3);
    }
    // DOTPRODUCT (17) on stage 2 and DOT_ZW (10) on stage 3 take their input texture from PSInputTexture
    // (word 56, stage 2 at bits 16, stage 3 at bits 20) and the full texture coordinates. As texm3x2depth, the
    // first dot product is z and the second w; z / w is in Z-buffer units.
    float dot_z = 0;
    if (((ps_word(54) >> 10) & 31) == 17)
    {
        dot_z = dot(i.tq2.xyz, ps_dotmap(2, regs[8 + ((ps_word(56) >> 16) & 3)]));
        regs[10] = 0;
    }
    if (((ps_word(54) >> 15) & 31) == 10)
    {
        float w = dot(i.tq3.xyz, ps_dotmap(3, regs[8 + ((ps_word(56) >> 20) & 3)]));
        depth = saturate(dot_z / w / 16777215.0);
        regs[11] = 0;
    }
    regs[12].a = regs[8].a;
    uint count = min(ps_word(53) & 255, 8);
    [loop]
    for (uint stage = 0; stage < count; ++stage)
    {
        float4 c0 = ps_constant(stage, false);
        float4 c1 = ps_constant(stage, true);
        uint rgb = ps_word(34 + stage);
        uint alpha = ps_word(stage);
        uint ro = ps_word(45 + stage);
        uint ao = ps_word(26 + stage);
        float3 a = rgb_input(rgb >> 24, regs, c0, c1);
        float3 b = rgb_input((rgb >> 16) & 255, regs, c0, c1);
        float3 c = rgb_input((rgb >> 8) & 255, regs, c0, c1);
        float3 d = rgb_input(rgb & 255, regs, c0, c1);
        float3 ab = (ro & 8192) != 0 ? dot(a, b).xxx : a * b;
        float3 cd = (ro & 4096) != 0 ? dot(c, d).xxx : c * d;
        float aa = alpha_input(alpha >> 24, regs, c0, c1) * alpha_input((alpha >> 16) & 255, regs, c0, c1);
        float ac = alpha_input((alpha >> 8) & 255, regs, c0, c1) * alpha_input(alpha & 255, regs, c0, c1);
        bool select_cd = (ps_word(53) & 256) != 0 ? regs[12].a >= 0.5 : (((uint)round(saturate(regs[12].a) * 255)) & 1) != 0;
        float3 sum = (ro & 16384) != 0 ? (select_cd ? cd : ab) : ab + cd;
        float asum = (ao & 16384) != 0 ? (select_cd ? ac : aa) : aa + ac;
        float4 pab = output_map(float4(ab, aa), ro);
        float4 pcd = output_map(float4(cd, ac), ro);
        float3 psum = output_map(float4(sum, 0), ro).rgb;
        float aab = output_map(float4(aa, aa, aa, aa), ao).a;
        float acd = output_map(float4(ac, ac, ac, ac), ao).a;
        float as = output_map(float4(asum, asum, asum, asum), ao).a;
        if ((ro & 15) != 0)
        {
            regs[ro & 15].rgb = pcd.rgb;
        }
        if (((ro >> 4) & 15) != 0)
        {
            regs[(ro >> 4) & 15].rgb = pab.rgb;
        }
        if (((ro >> 8) & 15) != 0)
        {
            regs[(ro >> 8) & 15].rgb = psum;
        }
        if ((ro & 262144) != 0 && (ro & 15) != 0)
        {
            regs[ro & 15].a = pcd.b;
        }
        if ((ro & 524288) != 0 && ((ro >> 4) & 15) != 0)
        {
            regs[(ro >> 4) & 15].a = pab.b;
        }
        if ((ao & 15) != 0)
        {
            regs[ao & 15].a = acd;
        }
        if (((ao >> 4) & 15) != 0)
        {
            regs[(ao >> 4) & 15].a = aab;
        }
        if (((ao >> 8) & 15) != 0)
        {
            regs[(ao >> 8) & 15].a = as;
        }
    }
    float4 color = regs[12];
    uint final0 = ps_word(8);
    uint final1 = ps_word(9);
    // The FOG register: the fog colour, with the vertex fog factor in alpha.
    regs[3] = float4(argb(ps_fog.y).rgb, saturate(i.fog));
    if (final0 == 0 && final1 == 0)
    {
        color.rgb = native_fog(color.rgb, i.fog);
    }
    if (final0 != 0 || final1 != 0)
    {
        uint flags = final1 & 255;
        float3 v1 = (flags & 64) != 0 ? 1 - regs[5].rgb : regs[5].rgb;
        float3 r0 = (flags & 32) != 0 ? 1 - regs[12].rgb : regs[12].rgb;
        regs[14] = float4((flags & 128) != 0 ? saturate(v1 + r0) : v1 + r0, 0);
        float4 c0 = argb(ps_word(43));
        float4 c1 = argb(ps_word(44));
        regs[15] = float4(rgb_input(final1 >> 24, regs, c0, c1) * rgb_input((final1 >> 16) & 255, regs, c0, c1), 0);
        float3 a = rgb_input(final0 >> 24, regs, c0, c1);
        color.rgb = a * rgb_input((final0 >> 16) & 255, regs, c0, c1) + (1 - a) * rgb_input((final0 >> 8) & 255, regs, c0, c1) + rgb_input(final0 & 255, regs, c0, c1);
        color.a = alpha_input((final1 >> 8) & 255, regs, c0, c1);
    }
    color = saturate(color) * tint;
    if (alpha_rs.x > 0.5 && alpha_rs.z == 516 && round(color.a * 255) <= alpha_rs.y)
    {
        discard;
    }
    if (alpha_rs.x > 0.5 && alpha_rs.z != 516 && !native_alpha_pass(color.a))
    {
        discard;
    }
    return color;
}

)";
    // Native scene draws arrive with their NV2A vertex program already run: clip position, colours and
    // four texture coordinates. Their pixel stage is ps_model (the combiner definition) or, without one,
    // texture 0 modulated by the diffuse colour.
    const char* native_hlsl = R"(
struct NativeIn { float4 pos : POSITION; float4 diffuse : COLOR0; float4 specular : COLOR1; float2 uv0 : TEXCOORD0; float2 uv1 : TEXCOORD1; float2 uv2 : TEXCOORD2; float2 uv3 : TEXCOORD3; float fog : TEXCOORD4; float2 zw0 : TEXCOORD5; float2 zw1 : TEXCOORD6; float2 zw2 : TEXCOORD7; float2 zw3 : TEXCOORD8; };
VSOut native_vs(NativeIn i)
{
    VSOut o;
    o.pos = i.pos;
    o.uv0 = i.uv0;
    o.uv1 = i.uv1;
    o.uv2 = i.uv2;
    o.uv3 = i.uv3;
    o.tq2 = float4(i.uv2, i.zw2);
    o.tq3 = float4(i.uv3, i.zw3);
    o.tq0 = float4(i.uv0, i.zw0);
    o.tq1 = float4(i.uv1, i.zw1);
    o.nrm = float3(0, 0, 0);
    o.diffuse = i.diffuse;
    o.specular = i.specular;
    o.fog = i.fog;
    return o;
}
float4 native_ps(VSOut i) : SV_Target
{
    float4 color = i.diffuse;
    if (alpha_rs.w > 0.5)
    {
        float2 uv = any(native_uv_scale.xy != 0) ? i.uv0 * native_uv_scale.xy : i.uv0;
        color *= g_tex.Sample(g_samp, uv);
    }
    color = saturate(color) * tint;
    if (alpha_rs.x > 0.5 && !native_alpha_pass(color.a))
    {
        discard;
    }
    return color;
}
)";
    const char* alpha_hlsl = R"(
bool native_alpha_pass(float alpha)
{
    float a = round(saturate(alpha) * 255.0);
    float f = alpha_rs.z;
    float r = alpha_rs.y;
    if (f < 512.5) return false;
    if (f < 513.5) return a < r;
    if (f < 514.5) return a == r;
    if (f < 515.5) return a <= r;
    if (f < 516.5) return a > r;
    if (f < 517.5) return a != r;
    if (f < 518.5) return a >= r;
    return true;
}
)";
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
    const char* clear_hlsl = R"(
cbuffer ClearRectangle : register(b0)
{
    float4 rectangle;
};
float4 clear_vertex(uint id : SV_VertexID) : SV_Position
{
    const float2 corner[6] =
    {
        float2(0, 0), float2(1, 0), float2(0, 1),
        float2(0, 1), float2(1, 0), float2(1, 1)
    };
    return float4(lerp(rectangle.xy, rectangle.zw, corner[id]), 1, 1);
}
)";
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
    cbd.ByteWidth = sizeof(g.bones);
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
    // Model trees draw with CullMode(DAT_0032E3D0) = D3DCULL_CW. look_at uses
    // cross(z, up), which mirrors the Xbox basis, so retail CW culling is the
    // D3D11 front face here.
    rast.CullMode = D3D11_CULL_FRONT;
    g.device->CreateRasterizerState(&rast, g.rast_model_cull.GetAddressOf());
    rast.CullMode = D3D11_CULL_NONE;

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
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    g.device->CreateBlendState(&blend, g.blend_model_alpha.GetAddressOf());
    // Xbox 0x0015B580: SRCALPHA + ONE, including the alpha channel.
    blend.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    g.device->CreateBlendState(&blend, g.blend_additive.GetAddressOf());

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
           g.depth_off && g.depth_lequal && g.depth_lequal_ro && g.blend_model_alpha && g.blend_additive &&
           g.clear_vs && g.clear_cb && g.clear_depth;
    if (!g.ok)
    {
        std::cerr << "d3d11: pipeline incomplete\n";
        gfx_shutdown();
        return false;
    }
#if defined(GBTG_DEBUG_UI)
    if (g.device && g.context
#if !defined(GBTG_INSPECTOR_APP)
        && g.hwnd && g.swap
#endif
        )
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
    gfx_release_dialog_texture();
#if defined(GBTG_DEBUG_UI)
    debug_ui_shutdown();
#endif
    g.meshes.clear();
    g.model_ps.Reset();
    g.specialized_pixels.clear();
    g.model_source.clear();
    g.vertex_source.clear();
    g.validation_shader.Reset();
    g.native_programs.clear();
#if GBTG_OPTIMIZE
    g.supported_words = {};
    g.supported_program = nullptr;
    g.native_pipeline = false;
    g.last_pixel = nullptr;
    g.readback_staging.Reset();
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
    g.blend_additive.Reset();
    g.blend_model_alpha.Reset();
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

void gfx_begin_frame()
{
    profile_gpu_begin();
    g.native_vb_cursor = 0;
    g.native_ib_cursor = 0;
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
    g.skin = false;
    g.context->VSSetShader(g.vs.Get(), nullptr, 0);
    g.context->PSSetShader(g.ps.Get(), nullptr, 0);
    g.context->PSSetSamplers(0, 1, g.sampler.GetAddressOf());
    g.context->PSSetSamplers(1, 1, g.light_sampler.GetAddressOf());
    ID3D11Buffer* cbs[2] = {g.cb.Get(), g.bone_cb.Get()};
    g.context->VSSetConstantBuffers(0, 2, cbs);
    g.context->PSSetConstantBuffers(0, 1, cbs);
}

void gfx_set_bones(const float* column_major, int count)
{
    if (!g.ok)
    {
        return;
    }
    g.skin = column_major && count > 0;
    for (int i = 0; i < GfxD3D11::kMaxBones; ++i)
    {
        float* m = g.bones + i * 16;
        std::memset(m, 0, sizeof(float) * 16);
        m[0] = m[5] = m[10] = m[15] = 1.f;
    }
    const int n = std::min(count, GfxD3D11::kMaxBones);
    if (g.skin)
    {
        std::memcpy(g.bones, column_major, sizeof(float) * 16 * size_t(n));
    }
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(g.context->Map(g.bone_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        std::memcpy(mapped.pData, g.bones, sizeof(g.bones));
        g.context->Unmap(g.bone_cb.Get(), 0);
    }
}

Vec3 quat_rotate(Quat q, Vec3 v)
{
    const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len > 1e-8f)
    {
        q.x /= len;
        q.y /= len;
        q.z /= len;
        q.w /= len;
    }
    const float tx = 2.f * (q.y * v.z - q.z * v.y);
    const float ty = 2.f * (q.z * v.x - q.x * v.z);
    const float tz = 2.f * (q.x * v.y - q.y * v.x);
    return Vec3{v.x + q.w * tx + (q.y * tz - q.z * ty), v.y + q.w * ty + (q.z * tx - q.x * tz),
                v.z + q.w * tz + (q.x * ty - q.y * tx)};
}

void gfx_set_camera(const Vec3& position, const Quat& orientation, float fov_y)
{
    if (!g.ok)
    {
        return;
    }
    // Local camera forward is -Z. The cutscene quaternion (channels 3..6) rotates that basis.
    const Vec3 forward = quat_rotate(orientation, Vec3{0.f, 0.f, -1.f});
    const Vec3 up = quat_rotate(orientation, Vec3{0.f, 1.f, 0.f});
    const Vec3 at{position.x + forward.x, position.y + forward.y, position.z + forward.z};
    // Xbox 0x000F5FD0: mode-0 projection uses the retail aspect table
    // and its 1.1 pixel-aspect multiplier. 0x000F6170 submits the fitted
    // camera viewport, not the full HUD viewport.
    present_camera_viewport();
    const float aspect = (640.f / 480.f) * 1.1f;
    g.mvp = mul(perspective(fov_y, aspect, 0.1f, 2000.f), look_at(position, at, up));
}

void gfx_release_mesh(const Mesh& mesh)
{
    g.meshes.erase(&mesh);
}

void gfx_set_dialog_camera(const Vec3& position, const Quat& orientation, float fov_y)
{
    present_camera_viewport();
    const Vec3 forward = quat_rotate(orientation, Vec3{0.f, 0.f, -1.f});
    const Vec3 up = quat_rotate(orientation, Vec3{0.f, 1.f, 0.f});
    const Vec3 at{position.x + forward.x, position.y + forward.y, position.z + forward.z};
    // Xbox 0x000F5FD0 multiplies mode-0 aspect by DAT_0032B1A0 (1.1).
    g.mvp = mul(perspective(fov_y, (640.f / 480.f) * 1.1f, 0.1f, 2000.f), look_at(position, at, up));
}

// Xbox 0x00058150 calls D3DDevice_Clear(0xF3) for a background dialogue.
void gfx_clear_dialog_background()
{
    const float black[4] = {0.f, 0.f, 0.f, 1.f};
    g.context->ClearRenderTargetView(dialog_target.rendering ? dialog_target.rtv.Get() : g.rtv.Get(), black);
    g.context->ClearDepthStencilView(dialog_target.rendering ? dialog_target.dsv.Get() : g.dsv.Get(),
                                   D3D11_CLEAR_DEPTH, 1.f, 0);
}

void gfx_begin_dialog_texture(u8 slot, int width, int height)
{
    if (!g.ok)
    {
        return;
    }
    if (dialog_target.slot != slot || dialog_target.width != width || dialog_target.height != height)
    {
        dialog_target = DialogRenderTarget{};
        dialog_target.slot = slot;
        dialog_target.width = width;
        dialog_target.height = height;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = UINT(width);
        desc.Height = UINT(height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(g.device->CreateTexture2D(&desc, nullptr, dialog_target.color.GetAddressOf())) ||
            FAILED(g.device->CreateRenderTargetView(dialog_target.color.Get(), nullptr, dialog_target.rtv.GetAddressOf())) ||
            FAILED(g.device->CreateShaderResourceView(dialog_target.color.Get(), nullptr, dialog_target.srv.GetAddressOf())))
        {
            throw Error("dialogue render target creation failed");
        }
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        if (FAILED(g.device->CreateTexture2D(&desc, nullptr, dialog_target.depth.GetAddressOf())) ||
            FAILED(g.device->CreateDepthStencilView(dialog_target.depth.Get(), nullptr, dialog_target.dsv.GetAddressOf())))
        {
            throw Error("dialogue depth target creation failed");
        }
    }
    dialog_target.saved_camera = g.mvp;
    gfx_sprite_clear();
    UINT viewports = 1;
    g.context->RSGetViewports(&viewports, &dialog_target.saved_viewport);
    ID3D11ShaderResourceView* unbound[2]{};
    g.context->PSSetShaderResources(0, 2, unbound);
    g.context->OMSetRenderTargets(1, dialog_target.rtv.GetAddressOf(), dialog_target.dsv.Get());
    dialog_target.rendering = true;
}

void gfx_end_dialog_texture()
{
    if (!g.ok)
    {
        return;
    }
    present_hud_sprites();
    gfx_sprite_clear();
    g.context->OMSetRenderTargets(1, g.rtv.GetAddressOf(), g.dsv.Get());
    g.context->RSSetViewports(1, &dialog_target.saved_viewport);
    g.mvp = dialog_target.saved_camera;
    dialog_target.rendering = false;
}

void gfx_release_dialog_texture()
{
    dialog_target = DialogRenderTarget{};
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

// Xbox 0x000F9250. The D3D boundary consumes the UI projection from 0x000F6260.
void gfx_draw_screen_mesh(const Mesh& mesh, float x, float y, float scale, bool clear_depth)
{
    if (!clear_depth)
    {
        present_hud_sprites();
        gfx_sprite_clear();
    }
    present_hud_viewport();
    const Mat4 saved = g.mvp;
    Mat4 ortho{};
    ortho.m[0] = 2.f / 640.f;
    ortho.m[5] = -2.f / 480.f;
    ortho.m[10] = -1.f / 100.f;
    ortho.m[12] = -1.f;
    ortho.m[13] = 1.f;
    ortho.m[15] = 1.f;
    g.mvp = ortho;
    if (clear_depth)
    {
        g.context->ClearDepthStencilView(g.dsv.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
    }
    gfx_draw_mesh(mesh, Vec3{x, y, -50.f}, Vec3{scale, -scale, scale});
    g.mvp = saved;
}

void gfx_draw_mesh(const Mesh& mesh, Vec3 origin, Vec3 scale)
{
    render_profile::Scope measurement(render_profile::HostRendering);
    if (!g.ok)
    {
        return;
    }
    auto it = g.meshes.find(&mesh);
    if (it == g.meshes.end())
    {
        GpuMesh gpu;
        if (!upload_mesh(mesh, gpu))
        {
            g.meshes.emplace(&mesh, GpuMesh{});
            return;
        }
        it = g.meshes.emplace(&mesh, std::move(gpu)).first;
    }
    GpuMesh& gpu = it->second;
    refresh_mesh_textures(mesh, gpu);
    if (!gpu.vb)
    {
        return;
    }
    // 0x0005E0C0 leaves ALPHABLENDENABLE set. begin_frame installs opaque; a later
    // call of that setter has to win for the draws that follow it.
    if (g.rs_alpha_blend != 0)
    {
        g.context->OMSetBlendState(g.blend_alpha.Get(), nullptr, 0xFFFFFFFFu);
    }
    D3D11_MAPPED_SUBRESOURCE mapped{};
    const UINT stride = sizeof(GpuVert);
    const UINT offset = 0;
    g.context->IASetVertexBuffers(0, 1, gpu.vb.GetAddressOf(), &stride, &offset);
    g.context->IASetIndexBuffer(gpu.ib.Get(), DXGI_FORMAT_R32_UINT, 0);
    const bool placed = origin.x != 0.f || origin.y != 0.f || origin.z != 0.f || scale.x != 1.f ||
                        scale.y != 1.f || scale.z != 1.f;
    Mat4 draw_mvp = g.mvp;
    if (placed)
    {
        Mat4 world{};
        world.m[0] = scale.x;
        world.m[5] = scale.y;
        world.m[10] = scale.z;
        world.m[15] = 1.f;
        world.m[12] = origin.x;
        world.m[13] = origin.y;
        world.m[14] = origin.z;
        draw_mvp = mul(g.mvp, world);
    }
    u32 tris = 0;
    u32 lit = 0;
    // A move still is coplanar with the movie and later in the mesh, so it hides the clip.
    // The playing movie is drawn in the second pass and wins the depth test.
    for (int pass = 0; pass < 2; ++pass)
    {
    for (const auto& range : gpu.ranges)
    {
        // Xbox 0x001582A0 calls 0x00158240 before submitting any sections.
        if (!model_submesh_visible(mesh, range.node_state_slot, range.node_state_value))
        {
            continue;
        }
        const bool front = range.tex_index >= 0 && size_t(range.tex_index) < mesh.textures.size() &&
                           mesh.textures[size_t(range.tex_index)].movie_front;
        if (front != (pass == 1))
        {
            continue;
        }
        if (SUCCEEDED(g.context->Map(g.cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            GpuCB cb;
            std::memcpy(cb.mvp, draw_mvp.m, sizeof(cb.mvp));
            cb.albedo_uv = range.albedo_uv;
            cb.light_uv = range.light_uv;
            cb.use_light = range.light ? range.use_light : 0.f;
            cb.use_cel = range.light ? range.use_cel : 0.f;
            cb.skin = g.skin && range.uses_skin_palette ? 1.f : 0.f;
            for (u32 influence = 0; influence < 4; ++influence)
            {
                cb.skin_mask[influence] = (range.skin_weight_mask & (1 << influence)) ? 1.f : 0.f;
            }
            cb.light_x = range.color_r;
            cb.light_y = range.color_g;
            cb.light_z = range.color_b;
            store_alpha_rs(cb);
            cb.alpha_pad = range.srv ? 1.f : 0.f;
            // 0x0015AD20 writes the alpha shadow for this material. Byte +24 == 0
            // clears ALPHATESTENABLE. Any other byte is D3DCMP_GREATER (0x204).
            if (range.alpha_ref > 0.f)
            {
                cb.alpha_test = 1.f;
                cb.alpha_ref = range.alpha_ref;
                cb.alpha_func = 516.f;
            }
            else
            {
                cb.alpha_test = 0.f;
                cb.alpha_func = 0.f;
            }
            std::memcpy(mapped.pData, &cb, sizeof(cb));
            g.context->Unmap(g.cb.Get(), 0);
        }
        if (g.skin && g.bone_cb)
        {
            float palette[GfxD3D11::kMaxBones * 16];
            for (int i = 0; i < GfxD3D11::kMaxBones; ++i)
            {
                float* m = palette + i * 16;
                std::memset(m, 0, sizeof(float) * 16);
                m[0] = m[5] = m[10] = m[15] = 1.f;
            }
            if (!range.skin.empty())
            {
                const int slots = std::min(int(range.skin.size()), GfxD3D11::kMaxBones);
                for (int i = 0; i < slots; ++i)
                {
                    int joint = int(range.skin[size_t(i)]);
                    // 0x157B90 indexes the pool 0x155E60 just published. The byte is
                    // that submesh's ancestor ndMtxArray record, which names the joint.
                    const std::vector<u16>& mtx = !range.mtx.empty() ? range.mtx : mesh.mtx_joints;
                    if (!mtx.empty())
                    {
                        if (joint < 0 || size_t(joint) >= mtx.size())
                        {
                            continue;
                        }
                        joint = int(mtx[size_t(joint)]);
                    }
                    if (joint >= 0 && joint < GfxD3D11::kMaxBones)
                    {
                        std::memcpy(palette + i * 16, g.bones + joint * 16, sizeof(float) * 16);
                    }
                }
            }
            else
            {
                std::memcpy(palette, g.bones, sizeof(palette));
            }
            if (SUCCEEDED(g.context->Map(g.bone_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            {
                std::memcpy(mapped.pData, palette, sizeof(palette));
                g.context->Unmap(g.bone_cb.Get(), 0);
            }
        }
        ID3D11ShaderResourceView* srvs[2] = {range.srv.Get(), range.light.Get()};
        if (!dialog_target.rendering && dialog_target.srv && range.tex_index >= 0 &&
            size_t(range.tex_index) < mesh.textures.size() &&
            mesh.textures[size_t(range.tex_index)].movie_slot == dialog_target.slot)
        {
            srvs[0] = dialog_target.srv.Get();
        }
        if (range.material_cb)
        {
            ID3D11ShaderResourceView* stages[4]{};
            ID3D11SamplerState* samplers[4]{};
            for (size_t stage = 0; stage < 4; ++stage)
            {
                const i32 texture = range.stages[stage];
                if (texture >= 0 && size_t(texture) < gpu.tex_srv.size())
                {
                    stages[stage] = gpu.tex_srv[size_t(texture)].Get();
                }
                samplers[stage] = range.samplers[stage].Get();
            }
            if (dialog_target.srv && range.stages[0] == range.tex_index && srvs[0] != range.srv.Get())
            {
                stages[0] = srvs[0];
            }
            ID3D11Buffer* material = range.material_cb.Get();
            g.context->PSSetConstantBuffers(2, 1, &material);
            g.context->PSSetShader(g.model_ps.Get(), nullptr, 0);
            g.context->PSSetShaderResources(0, 4, stages);
            g.context->PSSetSamplers(0, 4, samplers);
        }
        else
        {
            g.context->PSSetShader(g.ps.Get(), nullptr, 0);
            g.context->PSSetShaderResources(0, 2, srvs);
            ID3D11SamplerState* samplers[2]{g.sampler.Get(), g.light_sampler.Get()};
            g.context->PSSetSamplers(0, 2, samplers);
        }
        // Xbox 0x0015B5E0 dispatches 0/1/2/3 to 0x0015B380/4B0/3F0/580.
        if (range.render_mode >= 0 && range.render_mode <= 3)
        {
            ID3D11BlendState* blend = range.render_mode == 0 ? g.blend_opaque.Get() :
                (range.render_mode == 3 ? g.blend_additive.Get() : g.blend_model_alpha.Get());
            g.context->OMSetBlendState(blend, nullptr, 0xffffffffu);
            if (pass == 0)
            {
                const bool depth_write = range.render_mode == 0 || range.render_mode == 2;
                g.context->OMSetDepthStencilState(depth_write ? g.depth_lequal.Get() : g.depth_lequal_ro.Get(), 0);
            }
        }
        // Xbox 0x00158A80 sets CullMode(DAT_0032E3D0) for the model tree;
        // 0x001582A0 draws a push buffer with byte +0x3C set using CullMode(0).
        g.context->RSSetState(range.cull_disable != 0 ? g.rast.Get() : g.rast_model_cull.Get());
        profile_gpu_draw_begin();
        g.context->DrawIndexed(range.count, range.start, 0);
        profile_gpu_draw_end();
        g.context->RSSetState(g.rast.Get());

        tris += range.count / 3;
        if (range.use_light > 0.f || range.use_cel > 0.f)
        {
            lit += range.count / 3;
        }
    }
    }
    if (!g.logged_draw)
    {
        std::cout << "d3d11 draw " << mesh.name << " tris " << tris << " lit " << lit << "\n";
        g.logged_draw = true;
    }
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
    if ((definition[53] & 255) > 8)
    {
        throw Error("native pixel shader exceeds eight combiner stages");
    }
    for (u32 stage = 0; stage < 4; ++stage)
    {
        const u32 mode = (definition[54] >> (stage * 5)) & 31;
        const bool dot = (stage == 2 && mode == 17) || (stage == 3 && mode == 10 && ((definition[54] >> 10) & 31) == 17);
        if (mode > 1 && mode != 4 && !dot)
        {
            throw Error("native pixel shader uses unsupported texture mode " + std::to_string(mode));
        }
        if (dot)
        {
            const u32 mapping = (definition[55] >> ((stage - 1) * 4)) & 7;
            if (mapping != 0 && mapping != 4)
            {
                throw Error("native pixel shader uses unsupported dot mapping " + std::to_string(mapping));
            }
        }
    }
    const bool depth_output = ((definition[54] >> 15) & 31) == 10;
    if (render_profile::reference)
    {
        return g.model_ps.Get();
    }
    std::array<u32, 60> key{};
    std::copy_n(definition, key.size(), key.begin());
    std::fill(key.begin() + 10, key.begin() + 26, 0);
    key[43] = key[44] = 0;
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
    std::string source = g.model_source;
    const std::string original = "return ps_words[n / 4][n % 4];";
    std::string replacement = "switch (n) {";
    for (u32 n = 0; n < 60; ++n)
    {
        if ((n >= 10 && n < 26) || n == 43 || n == 44)
        {
            continue;
        }
        replacement += "case " + std::to_string(n) + ": return " + std::to_string(key[n]) + "u;";
    }
    replacement += "default: return ps_words[n / 4][n % 4]; }";
    source.replace(source.find(original), original.size(), replacement);
    const std::string loop = "[loop]\n    for (uint stage = 0; stage < count; ++stage)";
    source.replace(source.find(loop), loop.size(), "[unroll]\n    for (uint stage = 0; stage < count; ++stage)");
    // This entry is exclusively the native path, where the material uses identity UVs and v0/v1.
    const std::string uv = "uint uv = ps_uv[stage];";
    source.replace(source.find(uv), uv.size(), "uint uv = stage;");
    const std::string first = "regs[4] = ps_control.x == 0 ? i.diffuse : i.specular;";
    const std::string second = "regs[5] = ps_control.y == 0 ? i.diffuse : i.specular;";
    source.replace(source.find(first), first.size(), "regs[4] = i.diffuse;");
    source.replace(source.find(second), second.size(), "regs[5] = i.specular;");
    // Constant native controls also remove the conditional enclosing each assignment.
    size_t pos = source.find("if (ps_control.x < 2)");
    source.replace(pos, std::strlen("if (ps_control.x < 2)"), "if (true)");
    pos = source.find("if (ps_control.y < 2)");
    source.replace(pos, std::strlen("if (ps_control.y < 2)"), "if (true)");
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
    auto found = g.native_programs.find(program);
    if (found != g.native_programs.end())
    {
#if GBTG_OPTIMIZE
        // GBTG_OPTIMIZE step 2: the draw that follows reuses this lookup.
        g.supported_words = program;
        g.supported_program = &found->second;
#endif
        return found->second.shader != nullptr;
    }
    render_profile::Scope compilation(render_profile::Compilation);
    GfxD3D11::ProgramGpu gpu;
    const auto body = xbox_vertex_hlsl(program);
    if (render_profile::enabled && body.empty())
    {
        std::cout << "profile vertex_fallback instructions=" << (program.empty() ? 0 : program[0] >> 16) << " mac/ilu/relative:";
        for (size_t n = 1; n + 3 < program.size(); n += 4)
        {
            std::cout << ' ' << ((program[n + 1] >> 21) & 15) << '/' << ((program[n + 1] >> 25) & 7) << '/' << ((program[n + 3] >> 1) & 1);
        }
        std::cout << "\n";
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
    g.native_programs.emplace(std::vector<u32>(program.begin(), program.end()), std::move(gpu));
    return supported;
}

void gfx_warmup_shader_permutations(const std::filesystem::path& path, const std::function<void(float)>& progress)
{
    if (!g.ok)
    {
        throw Error("shader permutation warm-up requires an initialized D3D11 device");
    }
    if (render_profile::reference)
    {
        throw Error("shader permutation warm-up is unavailable with --render-reference");
    }
    ShaderPermutationManifest manifest;
    read_shader_permutation_manifest(path, manifest);
    size_t vertex_count = 0;
    size_t pixel_fallbacks = 0;
    const size_t total = manifest.vertex_programs.size() + manifest.pixel_definitions.size();
    size_t completed = 0;
    if (progress)
    {
        progress(total == 0 ? 1.f : 0.f);
    }
    const auto start = std::chrono::steady_clock::now();
    for (const auto& program : manifest.vertex_programs)
    {
        if (!gfx_native_vertex_program_supported(program))
        {
            throw Error("shader warm-up encountered an unsupported vertex program");
        }
        ++vertex_count;
        ++completed;
        if (progress)
        {
            progress(float(completed) / float(total));
        }
    }
    for (const auto& definition : manifest.pixel_definitions)
    {
        if (native_pixel_shader(definition.data()) == g.model_ps.Get())
        {
            ++pixel_fallbacks;
        }
        ++completed;
        if (progress)
        {
            progress(float(completed) / float(total));
        }
    }
    const double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::cout << "shader warm-up: " << vertex_count << " vertex, " << manifest.pixel_definitions.size()
              << " pixel permutations in " << elapsed << " ms";
    if (pixel_fallbacks != 0)
    {
        std::cout << " (" << pixel_fallbacks << " generic pixel fallbacks)";
    }
    std::cout << "\n";
}

void gfx_capture_shader_permutations(const std::filesystem::path& path)
{
    if (!g.ok)
    {
        return;
    }
    ShaderPermutationManifest manifest;
    if (std::filesystem::exists(path))
    {
        read_shader_permutation_manifest(path, manifest);
    }
    for (const auto& [program, gpu] : g.native_programs)
    {
        if (gpu.shader && !contains_program(manifest.vertex_programs, program))
        {
            manifest.vertex_programs.push_back(program);
        }
    }
    for (const auto& [definition, shader] : g.specialized_pixels)
    {
        (void)shader;
        if (std::find(manifest.pixel_definitions.begin(), manifest.pixel_definitions.end(), definition)
            == manifest.pixel_definitions.end())
        {
            manifest.pixel_definitions.push_back(definition);
        }
    }
    write_shader_permutation_manifest(path, manifest);
    std::cout << "shader capture: " << manifest.vertex_programs.size() << " vertex, "
              << manifest.pixel_definitions.size() << " pixel permutations -> " << path.string() << "\n";
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
        std::array<float, 21> a{}, b{};
        std::memcpy(a.data(), &expected, sizeof(expected));
        std::memcpy(b.data(), &actual[n], sizeof(actual[n]));
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
    std::cout << "GPU vertex reference PASS vertices=" << count << " instructions=" << (draw.vertex_program[0] >> 16) << " max_abs_error=" << maximum << " tolerance=5e-5+5e-5*abs(reference)\n";
}
}

void gfx_begin_native_batch()
{
    g.native_batch = !render_profile::reference;
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
        if (!gfx_init(config))
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
    gfx_release_sprite(texture);
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
    gfx_release_sprite(texture);
    gfx_shutdown();
    initialize();
    draw = {};
    draw.vertices = vertices;
    draw.indices = indices;
    expect(255, 255, 255);
    gfx_shutdown();
    std::cout << "Native boundary PASS: vertex/index mutation, texture serial/release/address reuse, constants, device unload/reload\n";
}

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
        cb.native_uv_scale = {1.f / float(draw.textures[0]->width), 1.f / float(draw.textures[0]->height), 0.f, 0.f};
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
        cb.native_uv_scale = {1.f / float(draw.textures[0]->width), 1.f / float(draw.textures[0]->height), 0.f, 0.f};
    }
    std::memcpy(mapped.pData, &cb, sizeof(cb));
    g.context->Unmap(g.cb.Get(), 0);
#endif

    const u32 mask = ((draw.color_mask >> 24) & 1) | ((draw.color_mask >> 15) & 2) | ((draw.color_mask >> 6) & 4) |
        ((draw.color_mask << 3) & 8);
    const u64 blend_key = u64(draw.blend) | (u64(draw.source_blend & 0xFFFF) << 1) |
        (u64(draw.destination_blend & 0xFFFF) << 17) | (u64(draw.blend_equation & 0xFFFF) << 33) | (u64(mask) << 49);
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
    const std::array<u32, 9> depth_key{(draw.depth_test ? 1u : 0u) | (draw.depth_write ? 2u : 0u)
        | (draw.depth_function << 2), draw.stencil_test ? 1u : 0u, draw.stencil_function, draw.stencil_read_mask,
        draw.stencil_write_mask, draw.stencil_fail, draw.stencil_depth_fail, draw.stencil_pass, 0};
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
    auto rasterizer = g.native_rasterizers.find(draw.cull);
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
        ComPtr<ID3D11RasterizerState> state;
        if (FAILED(g.device->CreateRasterizerState(&desc, state.GetAddressOf())))
        {
            throw Error("D3D11 native rasterizer state creation failed");
        }
        rasterizer = g.native_rasterizers.emplace(draw.cull, state).first;
    }
    ID3D11ShaderResourceView* views[4]{};
    ID3D11SamplerState* samplers[4]{};
    for (size_t stage = 0; stage < 4; ++stage)
    {
        views[stage] = native_texture(draw.textures[stage]);
        const u32 sampler_key = draw.address_u[stage] | (draw.address_v[stage] << 8);
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
                const float scale[2] = {1.f / float(texture->width), 1.f / float(texture->height)};
                std::memcpy(&words[68 + stage * 2], scale, sizeof(scale));
            }
        }
        words[60] = 0;
        words[61] = 1;
        words[62] = 2;
        words[63] = 3;
        words[64] = 0;
        words[65] = 1;
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
        pixel_shader = native_pixel_shader(draw.pixel_shader);
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
    if (bound.blend != blend->second.Get())
    {
        g.context->OMSetBlendState(blend->second.Get(), nullptr, 0xFFFFFFFFu);
        bound.blend = blend->second.Get();
    }
    if (bound.depth != depth->second.Get() || bound.stencil_reference != (draw.stencil_reference & 0xFF))
    {
        g.context->OMSetDepthStencilState(depth->second.Get(), draw.stencil_reference & 0xFF);
        bound.depth = depth->second.Get();
        bound.stencil_reference = draw.stencil_reference & 0xFF;
    }
    if (bound.rasterizer != rasterizer->second.Get())
    {
        g.context->RSSetState(rasterizer->second.Get());
        bound.rasterizer = rasterizer->second.Get();
    }
    const UINT stride = program_draw ? sizeof(GfxNativeInputs) : sizeof(GfxNativeVertex);
    const UINT offset = vertex_offset;
    const D3D11_PRIMITIVE_TOPOLOGY topology = draw.lines ? D3D11_PRIMITIVE_TOPOLOGY_LINELIST
        : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    if (bound.topology != topology)
    {
        g.context->IASetPrimitiveTopology(topology);
        bound.topology = topology;
    }
    g.context->IASetVertexBuffers(0, 1, g.native_vb.GetAddressOf(), &stride, &offset);
    ID3D11InputLayout* layout = g.native_layout.Get();
    ID3D11VertexShader* vertex_shader = g.native_vs.Get();
    GfxD3D11::ProgramGpu* found = nullptr;
    if (program_draw)
    {
        found = g.supported_program;
        if (found == nullptr || g.supported_words.data() != draw.vertex_program.data()
            || g.supported_words.size() != draw.vertex_program.size())
        {
            found = &g.native_programs.find(draw.vertex_program)->second;
        }
        g.supported_program = nullptr;
        auto& program = *found;
        layout = program.layout.Get();
        vertex_shader = program.shader.Get();
        if (draw.vertex_constants == nullptr)
        {
            throw Error("native vertex constants map failed");
        }
        // Bitwise, so a signed zero or NaN change still uploads.
        if (!bound.constants || std::memcmp(bound.vertex_constants.data(), draw.vertex_constants->data(),
            sizeof(bound.vertex_constants)) != 0)
        {
            if (FAILED(g.context->Map(g.native_constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            {
                throw Error("native vertex constants map failed");
            }
            std::memcpy(mapped.pData, draw.vertex_constants->data(), sizeof(*draw.vertex_constants));
            g.context->Unmap(g.native_constants.Get(), 0);
            if (!bound.constants)
            {
                ID3D11Buffer* constants = g.native_constants.Get();
                g.context->VSSetConstantBuffers(3, 1, &constants);
            }
            bound.constants = true;
            bound.vertex_constants = *draw.vertex_constants;
        }
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
                const float scale[2] = {1.f / float(texture->width), 1.f / float(texture->height)};
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

void gfx_begin_native_target(int width, int height)
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
    g.context->OMSetRenderTargets(1, native_target.rtv.GetAddressOf(), native_target.dsv.Get());
    // Xbox pixel coordinates map 1:1 onto the surface; the 640x480 space extends past its edges.
    D3D11_VIEWPORT vp{0.f, 0.f, 640.f, 480.f, 0.f, 1.f};
    g.context->RSSetViewports(1, &vp);
    native_target.rendering = true;
}

void gfx_read_native_target_depth(std::vector<u32>& output)
{
    output.clear();
    if (!g.ok)
    {
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

void gfx_clear_native(std::span<const std::array<i32, 4>> rectangles, u32 flags, u32 color, float depth)
{
    if (!g.ok)
    {
        return;
    }
    present_hud_viewport();
    const UINT depth_flags = ((flags & 1) != 0 ? D3D11_CLEAR_DEPTH : 0) | ((flags & 2) != 0 ? D3D11_CLEAR_STENCIL : 0);
    ID3D11RenderTargetView* target = native_target.rendering ? native_target.rtv.Get()
        : (dialog_target.rendering ? dialog_target.rtv.Get() : g.rtv.Get());
    ID3D11DepthStencilView* depth_view = native_target.rendering ? native_target.dsv.Get()
        : (dialog_target.rendering ? dialog_target.dsv.Get() : g.dsv.Get());
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
        if (depth_flags != 0)
        {
            g.context->ClearDepthStencilView(depth_view, depth_flags, depth, 0);
        }
        return;
    }
    if ((flags & 0xF0) != 0)
    {
        // A texture target is in Xbox pixels (see present_hud_viewport); the back buffer's fitted area is not.
        ComPtr<ID3D11DeviceContext1> context1;
        if ((flags & 0xF0) != 0xF0 || !native_target.rendering || FAILED(g.context.As(&context1)))
        {
            throw Error("native clear of a colour rectangle supports a whole colour on a texture target only");
        }
        const float rgba[4]{float((color >> 16) & 0xFF) / 255.f, float((color >> 8) & 0xFF) / 255.f,
            float(color & 0xFF) / 255.f, float(color >> 24) / 255.f};
        std::vector<D3D11_RECT> rects;
        for (const auto& rectangle : rectangles)
        {
            rects.push_back(D3D11_RECT{rectangle[0], rectangle[1], rectangle[2], rectangle[3]});
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

// DAT_004DE9AC. The present step advances this job by DAT_005096C0.
struct PresentFade
{
    float elapsed = 0.f;  // +0x2C
    float previous = 0.f; // +0x30
    float duration = 0.f; // +0x34
    float step = 0.f;     // +0x38
    int paused = 0;       // +0x158
};

// DAT_004DE9A0. +0x94C is the gate the phase machine tests.
struct PresentScene
{
    int marker = 0;
};

// *DAT_004DEA10. Cursor at +4, limit at +8.
struct PresentLoad
{
    u32 done = 0;
    u32 total = 0;
    u32 streaming = 0; // an entry whose stream byte at +0x11 is still above 1
};

// BSS counterparts of the load/fade globals. fs_init stores phase 0; the pointers stay clear
// until a load is installed, which is the retail boot state.
struct PresentGate
{
    PresentFade* fade = nullptr;   // DAT_004DE9AC
    PresentScene* scene = nullptr; // DAT_004DE9A0
    void* widget = nullptr;        // DAT_004DE99C
    PresentLoad* load = nullptr;   // *DAT_004DEA10
    u32 id_a = 0;                  // DAT_004DEA14
    u32 id_b = 0;                  // DAT_004DEA18
    int phase = 0;                 // DAT_004DD900
    float dt = 1.f / 60.f;         // DAT_005096C0
};

PresentGate present_gate;

// Xbox 0x00032AB0
// 0x0005FFA0: 1 when `id` is still streaming inside the load span.
bool present_load_id_busy(const PresentLoad* load, u32 id)
{
    (void)id;
    if (load == nullptr || load->done >= load->total)
    {
        return false;
    }
    return load->streaming != 0;
}

// Xbox 0x00032AB0
// Xbox 0x00032AB0
void present_tick_fade()
{
    PresentFade* fade = present_gate.fade;
    if (fade != nullptr)
    {
        if (fade->duration <= fade->elapsed)
        {
            // 0x0004E4A0(0, 0, 0). Elapsed becomes duration * 0 and the step clears.
            const float old = fade->elapsed;
            fade->elapsed = 0.f;
            fade->previous = old;
            fade->step = 0.f;
        }
        // 0x0004DA50(dt). ESI is the fade system at 0x004DE9B0.
        if (fade->paused == 0)
        {
            const float dt = present_gate.dt;
            fade->previous = fade->elapsed;
            fade->elapsed += dt;
            fade->step = dt;
            if (fade->elapsed > fade->duration)
            {
                fade->elapsed = fade->duration;
            }
            if (fade->elapsed < 0.f)
            {
                fade->elapsed = 0.f;
            }
        }
    }

    PresentScene* scene = present_gate.scene;
    if (scene != nullptr && present_gate.phase > 0 && present_gate.phase < 4 && present_gate.widget != nullptr)
    {
        // 0x00142460 ticks the load widget. EDI is the scene.
        (void)scene;
    }
    scene = present_gate.scene;

    if (present_gate.phase == 1)
    {
        if (scene != nullptr)
        {
            if (!present_load_id_busy(present_gate.load, present_gate.id_a) &&
                !present_load_id_busy(present_gate.load, present_gate.id_b) && scene->marker != 0)
            {
                present_gate.phase = 2;
            }
        }
    }
    else if (present_gate.phase == 3 && present_gate.load != nullptr &&
             present_gate.load->total <= present_gate.load->done && scene != nullptr && scene->marker == 0)
    {
        // Tail call 0x00032BA0 with phase 4. Leaving phase 3 drops the load set.
        present_gate.load = nullptr;
        present_gate.scene = nullptr;
        present_gate.fade = nullptr;
        present_gate.widget = nullptr;
        present_gate.phase = 4;
    }
}

// Object at the front of a DAT_0050E0E8 record. That span's stride is 0x50.
// DAT_0050E0EC is the record count; phase 1 reads record count-1.
struct PresentSlot
{
    int kind = 0;               // +0
    void* child = nullptr;      // +4
    int state = 0;              // +8, read by 0x00107CE0
    PresentSlot* ports[4] = {}; // +8 when kind is 4 (0x00107AA0)
    u16 show = 0;               // flag word 0x001083F0 masks with 0x10
    u16 hide = 0;
};

// DAT_004DEA34 cluster. fs_init (0x00033980) stores phase 4 and clears the pointers.
struct PresentInsert
{
    int phase = 4;                 // DAT_004DEA34
    int force = 0;                 // DAT_004DEA38
    int hold = 0;                  // DAT_004DEA24, nonzero skips the span poll
    u32 pad_mask = 0;              // DAT_0032E3B4, returned by 0x001554C0(0)
    u32 pad_filter = 0;            // DAT_0050E0F8
    u16 block_show = 0;            // words at 0x00507C98, kind 3 of 0x001083F0
    u16 block_hide = 0;
    PresentLoad* cursor = nullptr; // *DAT_004DEA2C, +4 / +8
    PresentSlot* actor = nullptr;  // DAT_004DEA28
    PresentSlot** records = nullptr;
    int record_count = 0; // (end - begin) / stride
    int record_index = 0; // DAT_0050E0EC
};

PresentInsert present_insert;

// Xbox 0x00033480
// 0x00107CE0. Idle when the pad word's 0x10<<state bit is clear.
bool present_port_idle(const PresentSlot& slot)
{
    if (slot.state < 0 || slot.state > 3)
    {
        return true;
    }
    const u32 bit = 0x10u << slot.state;
    return (present_insert.pad_mask & bit) == 0;
}

// Xbox 0x00033480
// 0x001082B0. Zero lets phase 1 tail-call 0x000337B0 with phase 2.
bool present_slot_busy(const PresentSlot& slot)
{
    if (slot.kind == 2)
    {
        return (present_insert.pad_mask & present_insert.pad_filter) != 0;
    }
    if (slot.kind == 3)
    {
        return present_insert.pad_mask != 0;
    }
    if (slot.kind == 4)
    {
        for (const PresentSlot* port : slot.ports)
        {
            if (port != nullptr && port->child != nullptr && present_port_idle(*port))
            {
                return false;
            }
        }
        return true;
    }
    if (slot.kind == 5)
    {
        return !present_port_idle(slot);
    }
    return false;
}

// Xbox 0x00033480
// 0x001083F0(actor, 0x10). Show bit set and hide bit clear.
bool present_flags_ready(u16 show, u16 hide)
{
    return (show & 0x10) != 0 && (hide & 0x10) == 0;
}

// Xbox 0x00033480
bool present_actor_ready(const PresentSlot* actor)
{
    if (actor == nullptr || actor->kind < 1 || actor->kind > 5)
    {
        return false;
    }
    if (actor->kind == 3)
    {
        return present_flags_ready(present_insert.block_show, present_insert.block_hide);
    }
    if (actor->kind == 4)
    {
        for (const PresentSlot* port : actor->ports)
        {
            if (port != nullptr && port->state != -1 && present_flags_ready(port->show, port->hide))
            {
                return true;
            }
        }
        return false;
    }
    return present_flags_ready(actor->show, actor->hide);
}

// Xbox 0x00033480
// 0x000337B0. EAX is the phase being entered. Leaving phase 3 drops the cursor.
void present_insert_set_phase(int next)
{
    if (next == present_insert.phase)
    {
        return;
    }
    if (present_insert.phase == 3)
    {
        present_insert.cursor = nullptr;
        if (present_insert.actor != nullptr && present_insert.actor->child != nullptr)
        {
            present_insert.actor = nullptr;
        }
    }
    present_insert.phase = next;
}

// Xbox 0x00033480
// Xbox 0x00033480
void present_tick_insert()
{
    switch (present_insert.phase)
    {
    case 1:
        if (present_insert.force != 0)
        {
            present_insert.force = 0;
            present_insert_set_phase(2);
            return;
        }
        if (present_insert.hold == 0 && present_insert.record_count != 0 && present_insert.records != nullptr &&
            present_insert.record_index > 0)
        {
            PresentSlot* slot = present_insert.records[present_insert.record_index - 1];
            if (slot != nullptr && slot->child != nullptr && !present_slot_busy(*slot))
            {
                present_insert_set_phase(2);
                return;
            }
        }
        break;
    case 2:
        // 0x00109470(DAT_004DEA28) refreshes the span record, then 0x001083F0(actor, 0x10).
        if (present_actor_ready(present_insert.actor))
        {
            present_insert_set_phase(3);
            return;
        }
        break;
    case 3:
        if (present_insert.cursor != nullptr && present_insert.cursor->total <= present_insert.cursor->done)
        {
            present_insert_set_phase(1);
            return;
        }
        break;
    case 4:
        if (present_insert.pad_mask != 0)
        {
            present_insert.phase = 1;
        }
        break;
    default:
        break;
    }
}

// Movie node on the DAT_0050E0B8 list. 0x00107420 reads it from ESI.
// +4 is the next link, +8 the clip, +0x28 the open-failed latch, +0x34 the enable.
struct PresentMovie
{
    PresentMovie* next = nullptr;
    void* clip = nullptr;
    int failed = 0;
    int enabled = 0;
};

// One of the four camera slots at DAT_004E16A0. 0x000489E0 returns 0 when the slot is null.
struct PresentCamera
{
    int marker = 0;
};

// Overlay record. 0x00106CD0 walks 0x0050DD90 through 0x0050DFB0, stride 0x88 (four records).
struct PresentOverlay
{
    void* object = nullptr; // +0
    u8 kind = 0;             // +4, indexes the callback table at 0x00545DC0
};

// Clear and movie gates. BSS, so a fresh boot leaves every pointer clear.
struct PresentFrame
{
    int post_pass = 0;    // DAT_00512AF0, first dword of the 0x2C post table
    int cover = 0;        // DAT_004DEA50, mirrors scene+0x93C
    int clear_mode = 0;   // DAT_0054632C, nonzero forces the target clear
    int movie_lock_a = 0; // DAT_004E5CD8
    int movie_lock_b = 0; // DAT_004E5CDC
    PresentMovie* movies = nullptr;
    PresentCamera* cameras[4] = {};
    PresentOverlay overlays[4] = {};
    void (*swap_notify)(int phase) = nullptr; // DAT_0043427C
};

PresentFrame present_frame;

// Xbox 0x00033A90
// Xbox clear masks: ZBUFFER is 1, STENCIL is 2, TARGET is 0xF0.
// Boot (both retain gates clear) uses 0xF3. A post pass or cover keeps the color target
// unless clear_mode is set.
u32 present_clear_flags()
{
    u32 flags = 3;
    if ((present_frame.post_pass == 0 && present_frame.cover == 0) || present_frame.clear_mode != 0)
    {
        flags = 0xf3;
    }
    return flags;
}

// Xbox 0x00033A90
// Xbox 0x00033A90
// 0x00106CD0. A null object skips the record.
void present_overlays()
{
    for (const PresentOverlay& overlay : present_frame.overlays)
    {
        if (overlay.object == nullptr)
        {
            continue;
        }
    }
}

// Xbox 0x00033A90
// 0x00033A20. Each non-null slot is bound by 0x000489E0 and drawn by 0x00011C240.
void present_cameras()
{
    for (PresentCamera* camera : present_frame.cameras)
    {
        if (camera == nullptr)
        {
            continue;
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

void present_swap()
{
    if (present_frame.swap_notify != nullptr)
    {
        present_frame.swap_notify(0);
    }
    if (g.ok && g.swap)
    {
        retain_last_frame();
#if defined(GBTG_DEBUG_UI)
        g.context->OMSetRenderTargets(1, g.rtv.GetAddressOf(), nullptr);
        debug_ui_draw();
#endif
        render_profile::Scope measurement(render_profile::Present);
        g.swap->Present(g.cfg.vsync, 0);
    }
    if (present_frame.swap_notify != nullptr)
    {
        present_frame.swap_notify(1);
    }
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
    const float frame = 640.f / 480.f;
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
    if (dialog_target.rendering)
    {
        vp.Width = float(dialog_target.width);
        vp.Height = float(dialog_target.height);
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
        gfx_draw_sprite(*quad.image, rect);
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
    const u32 flags = present_clear_flags();
    present_set_viewport();
    present_clear(flags);
    d3d_reset_render_state();
    // 0x000F9780 retires the deferred span at DAT_005079E0.
    // 0x00036180 submits the draw span at DAT_004DF240 and resets it.
    present_overlays();
    present_cameras();
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
    gfx_draw_sprite(text, rectangle);
    gfx_release_sprite(text);

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
    gfx_draw_sprite(pixel, bar);
    bar.x += 1.f / float(g.cfg.width);
    bar.y += 1.f / float(g.cfg.height);
    bar.w = std::max(0.f, bar_width - 2.f) / float(g.cfg.width);
    bar.h = std::max(0.f, bar_height - 2.f) / float(g.cfg.height);
    bar.w *= std::clamp(progress, 0.f, 1.f);
    bar.color = font_color;
    gfx_draw_sprite(pixel, bar);
    gfx_release_sprite(pixel);
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
    // Present 0x00033480, then 0x00032AB0, then 0x00033A90 (clear and swap).
    present_tick_insert();
    present_tick_fade();
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
#if defined(GBTG_DEBUG_UI)
    g.context->OMSetRenderTargets(1, g.rtv.GetAddressOf(), nullptr);
    debug_ui_draw();
#endif
    g.swap->Present(g.cfg.vsync, 0);
}

#if defined(GBTG_INSPECTOR_APP)
namespace
{
ComPtr<ID3D11Texture2D> inspection_color;
ComPtr<ID3D11ShaderResourceView> inspection_srv;
ComPtr<ID3D11Texture2D> inspection_depth_staging;
std::vector<u32> inspection_depth;
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
    inspection_srv.Reset();
    inspection_color.Reset();
    inspection_depth_staging.Reset();
    inspection_depth.clear();
}

void* inspector_texture_preview(const RgbaImage& image)
{
    return native_texture(&image);
}

void* inspector_capture_viewport(bool read_depth)
{
    if (!g.ok)
    {
        return nullptr;
    }
    ComPtr<ID3D11Texture2D> source = g.color;
    if (g.swap && FAILED(g.swap->GetBuffer(0, IID_PPV_ARGS(source.GetAddressOf()))))
    {
        throw Error("inspector cannot access the viewport target");
    }
    if (!inspection_color)
    {
        D3D11_TEXTURE2D_DESC desc{};
        source->GetDesc(&desc);
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(g.device->CreateTexture2D(&desc, nullptr, inspection_color.GetAddressOf()))
            || FAILED(g.device->CreateShaderResourceView(inspection_color.Get(), nullptr, inspection_srv.GetAddressOf())))
        {
            throw Error("inspector viewport texture creation failed");
        }
    }
    g.context->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11ShaderResourceView* empty[4]{};
    g.context->PSSetShaderResources(0, 4, empty);
    g.context->CopyResource(inspection_color.Get(), source.Get());
    inspection_depth.clear();
    if (read_depth)
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
    return inspection_srv.Get();
}

bool inspector_depth_visible(float retail_x, float retail_y, float depth)
{
    if (inspection_depth.empty())
    {
        return true;
    }
    const float scale = std::min(float(g.cfg.width) / 640.f, float(g.cfg.height) / 480.f);
    const int x = int((g.cfg.width - 640.f * scale) * .5f + retail_x * scale);
    const int y = int((g.cfg.height - 480.f * scale) * .5f + retail_y * scale);
    if (x < 0 || y < 0 || x >= g.cfg.width || y >= g.cfg.height)
    {
        return false;
    }
    const float sampled = float(inspection_depth[size_t(y) * g.cfg.width + x] & 0xFFFFFFu) / 16777215.f;
    return depth <= sampled + 0.00005f;
}

bool inspector_viewport_readback(RgbaImage& image)
{
    if (!inspection_color)
    {
        return false;
    }
    D3D11_TEXTURE2D_DESC desc{};
    inspection_color->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(g.device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf())))
    {
        return false;
    }
    g.context->CopyResource(staging.Get(), inspection_color.Get());
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
    if (!gfx_readback(frame))
    {
        return false;
    }
    const int width = std::min(frame.width, frame.height * 4 / 3);
    const int height = std::min(frame.height, frame.width * 3 / 4);
    const int left = (frame.width - width) / 2;
    const int top = (frame.height - height) / 2;
    out.width = width;
    out.height = height;
    out.bgra.resize(size_t(width) * height * 4);
    for (int row = 0; row < height; ++row)
    {
        std::memcpy(out.bgra.data() + size_t(row) * width * 4,
            frame.bgra.data() + (size_t(row + top) * frame.width + left) * 4,
            size_t(width) * 4);
    }
    return true;
}

// Xbox 0x00151E64
float* d3d_multiply_matrix(float* out, const float* left, const float* right)
{
    // Xbox 0x00151E64
    // 4x4 product, out = left * right, row-major. Output row r is
    // left[r][0] * right.row0 + left[r][1] * right.row1
    // + left[r][2] * right.row2 + left[r][3] * right.row3.
    // Every input element is loaded before the first store, so out may
    // alias left or right. Returns out.
    float a[16];
    float b[16];
    std::memcpy(a, left, sizeof(a));
    std::memcpy(b, right, sizeof(b));
    for (int row = 0; row < 4; ++row)
    {
        const float x = a[row * 4 + 0];
        const float y = a[row * 4 + 1];
        const float z = a[row * 4 + 2];
        const float w = a[row * 4 + 3];
        out[row * 4 + 0] = x * b[0] + y * b[4] + z * b[8] + w * b[12];
        out[row * 4 + 1] = x * b[1] + y * b[5] + z * b[9] + w * b[13];
        out[row * 4 + 2] = x * b[2] + y * b[6] + z * b[10] + w * b[14];
        out[row * 4 + 3] = x * b[3] + y * b[7] + z * b[11] + w * b[15];
    }
    return out;
}

// Xbox 0x00151F62
float* d3d_transpose_matrix(float* out, const float* m)
{
    // Xbox 0x00151F62
    // 4x4 transpose, row-major. out[col][row] = m[row][col].
    // Every input element is loaded before the first store, so out may
    // alias m. Returns out.
    float a[16];
    std::memcpy(a, m, sizeof(a));
    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            out[col * 4 + row] = a[row * 4 + col];
        }
    }
    return out;
}

// Xbox 0x00171BB0
u32* d3d_emit_texture_packets(u32 device_780, u32* packet)
{
    // Xbox 0x00171BB0
    // Fastcall. ecx is the D3D device, edx the push cursor. The dword at
    // device+0x780 is the texture installed by 0x00175370 (zero when unbound).
    // eax comes back as the cursor after the writes. A null cursor would
    // fault on Xbox; the port leaves it alone.
    //
    // Three packets, seven dwords each. The method word starts at 0x00181B68
    // and steps by 0x40 while it is <= 0x00181BE8, so the headers are
    // 0x00181B68, 0x00181BA8, 0x00181BE8. Each is count 6 at the NV097 bump-env
    // block (SET_TEXTURE_SET_BUMP_ENV_MAT, SCALE, OFFSET) for hardware stages
    // 1, 2, and 3.
    //
    // The six data dwords are the deferred stage shadow at DAT_001819C8
    // (DAT_00181970 + 0x58), stride 0x80. device+0x780 == 0 reads stages 0..2.
    // Any other value reads stages 1..3. The method headers do not move with
    // that select.
    if (packet == nullptr)
    {
        return packet;
    }

    const int stage = device_780 != 0u ? 1 : 0;
    u32 method = 0x00181B68u;
    for (int i = 0; i < 3; ++i)
    {
        const u32* bump = g.stages[stage + i].bump;
        packet[0] = method;
        for (int word = 0; word < 6; ++word)
        {
            packet[1 + word] = bump[word];
        }
        packet += 7;
        method += 0x40u;
    }
    return packet;
}

// DAT_001845a4. BSS list of side records. D3D_DestroyResource (0x00175890)
// passes the resource pointer; a match is the dword at record+4.
struct D3dResourceRecord
{
    D3dResourceRecord* next = nullptr; // Xbox +0
    int key = 0;                       // Xbox +4
    u8 flags = 0;                      // Xbox +0x1C
    void* object = nullptr;            // Xbox +0x20
};

static D3dResourceRecord* resource_records_1845a4 = nullptr;

// Xbox 0x00175840
void d3d_release_resource_side_records(int param_1)
{
    // Xbox 0x00175840
    // stdcall, ret 4. D3D_DestroyResource calls this with the resource in
    // EDI before it frees that resource. The previous-link starts at the
    // head. A record whose key equals param_1 is unlinked by storing its
    // next into that link. Bit 0 of the flag byte at +0x1C calls vtable+8
    // on the object at +0x20 (one argument, the object; the callee pops
    // it). XMemFree then takes the record with tag 0x24800000. The walk
    // continues at the pointer just written, so consecutive matches are
    // removed. A miss makes the record itself the previous link, which is
    // its next dword, and steps to that next. A null cursor ends the walk.
    D3dResourceRecord** link = &resource_records_1845a4;
    D3dResourceRecord* node = resource_records_1845a4;
    while (node != nullptr)
    {
        if (node->key == param_1)
        {
            *link = node->next;
            if ((node->flags & 1) != 0)
            {
                void* object = node->object;
                void** vtable = *reinterpret_cast<void***>(object);
                using ReleaseFn = void (__stdcall*)(void*);
                ReleaseFn release = reinterpret_cast<ReleaseFn>(vtable[2]);
                release(object);
            }
            D3dResourceRecord* dead = node;
            node = *link;
            delete dead;
        }
        else
        {
            link = &node->next;
            node = node->next;
        }
    }
}

// D3D resource Common word. A surface also stores its parent at Xbox +0x14.
// That parent is a resource, so its first word is Common as well.
// 0x00175930 reads those two words and nothing else.
struct D3dResourceRef
{
    u32 common = 0;                    // Xbox +0
    D3dResourceRef* parent = nullptr;  // Xbox +0x14
};

// Xbox 0x00175930
u32 d3d_resource_has_internal_references(const D3dResourceRef* resource)
{
    // Xbox 0x00175930
    // stdcall, ret 4. 0x00177780 calls this while D3D_DestroyResource
    // (0x00175890) retires a resource. eax is 1 when an internal
    // reference is still outstanding; the caller then blocks on the fence
    // at device+0x2c through 0x00177450. eax is 0 when none is outstanding.
    // A null resource would fault on Xbox.
    //
    // resource+0 is the D3D Common word. Bits 0x780000 are the internal
    // refcount. Non-zero returns 1 and does not read the parent.
    //
    // Bits 0x70000 are the type. A surface is 0x50000. Only a surface
    // continues, and only when the parent at +0x14 is non-null and that
    // parent's Common word has a non-zero internal refcount. Any other
    // type, a null parent, or a parent whose internal refcount is zero
    // returns 0.
    const u32 common = resource->common;
    if ((common & 0x780000u) != 0u)
    {
        return 1u;
    }
    if ((common & 0x70000u) != 0x50000u)
    {
        return 0u;
    }
    const D3dResourceRef* parent = resource->parent;
    if (parent == nullptr)
    {
        return 0u;
    }
    if ((parent->common & 0x780000u) == 0u)
    {
        return 0u;
    }
    return 1u;
}

// Xbox 0x00176EE0
u32 d3d_push_buffer_distance(const u32* device, const u32* record, u32 gpu_position)
{
    // Xbox 0x00176EE0
    // Fastcall, ret 4. D3D_BlockOnTime (0x00177450) is the caller. ecx is
    // the device at DAT_00181E08, edx is an 8-byte time slot from the
    // table at device+0x60 (the pointer 0x001770A0 returns), and the
    // stack argument is the GPU position from 0x001771A0. eax is the
    // byte distance. A null device or record would fault on Xbox; the
    // port returns 0.
    //
    // device+0 is the push-buffer put pointer. device+0x40 is the wrap
    // bias 0x001775C0 stores as put-base when the ring wraps. A position
    // at or below put belongs to the generation after that wrap, so the
    // bias is added before the subtract. The slot's recorded put lives
    // at slot+4 (written by 0x001773A0). The result is how far that
    // address sits ahead of the GPU. BlockOnTime compares it with
    // 0x2000. A negative difference means the GPU has already passed
    // the address, and the function returns 0.
    if (device == nullptr || record == nullptr)
    {
        return 0;
    }

    const u32 put = device[0];
    const u32 wrap = device[0x10];
    u32 recorded = record[1];
    if (gpu_position <= put)
    {
        gpu_position += wrap;
    }
    if (recorded <= put)
    {
        recorded += wrap;
    }
    const u32 distance = recorded - gpu_position;
    if (static_cast<i32>(distance) < 0)
    {
        return 0;
    }
    return distance;
}

// Fields this routine reads and writes on the device at DAT_00181E08.
// reg_file and notifier are host pointers to the objects stored at
// device+0x930 and device+0x1c10.
struct D3dPushGetCache
{
    u32 push_start = 0;          // device+0x24
    u32 push_end = 0;            // device+0x28
    u32 cached_get = 0;          // device+0x58
    u32 cached_ref = 0;          // device+0x5c
    const u32* reg_file = nullptr;  // device+0x930
    const u32* notifier = nullptr;  // device+0x1c10
};

// Xbox 0x00176F80
void d3d_update_push_buffer_get(D3dPushGetCache* device)
{
    // Xbox 0x00176F80
    // Fastcall. ecx is the D3D device. 0x001775C0 calls it when flag bit
    // 0x4 at device+8 is clear, before deciding whether the push buffer
    // has to wrap. A null device, register file, or notifier would fault
    // on Xbox; the port returns.
    //
    // notifier+0x44 is the hardware GET. ORing 0x80000000 is the same
    // push-buffer address 0x001771A0 returns while it sits in
    // [device+0x24, device+0x28). reg_file+0x400b10 is the reference
    // 0x001773A0 encodes as (put << 5) | (serial & 0x1f) and the GPU
    // writes back. 0x001771A0 spins until its low five bits match the
    // counter at device+0x30.
    //
    // Inside the ring, +0x58 caches that GET and +0x5c caches the
    // reference. Outside it, an unchanged reference leaves both words
    // alone. The retail path still stores them back: it reloads +0x58
    // and writes that value, then writes the reference over +0x5c.
    // A reference that did change is turned back into an address with
    // (reference >> 5) | 0x80000000, restoring the GPU bit the encode
    // shifted out.
    if (device == nullptr || device->reg_file == nullptr || device->notifier == nullptr)
    {
        return;
    }

    const u32 ref = device->reg_file[0x400b10 / 4];
    u32 get = device->notifier[0x44 / 4] | 0x80000000u;
    if (get < device->push_start || device->push_end <= get)
    {
        if (ref == device->cached_ref)
        {
            const u32 keep = device->cached_get;
            device->cached_get = keep;
            device->cached_ref = ref;
            return;
        }
        get = (ref >> 5) | 0x80000000u;
    }
    device->cached_get = get;
    device->cached_ref = ref;
}

struct D3dTimeSlot
{
    u32 serial = 0;
    u32 put = 0;
};

// Fields read by 0x001771A0 on the device at DAT_00181E08. slots is the
// inline table at +0x60: 64 records of {serial, put}, indexed by
// (serial >> 1) & 0x3f. history is the older ring 0x001770A0 walks
// once a serial has fallen off that table.
struct D3dPushGetState
{
    u32 put = 0;                          // +0x00
    u32 push_start = 0;                   // +0x24
    u32 push_end = 0;                     // +0x28
    u32 fence = 0;                        // +0x2c
    const u32* counter = nullptr;         // +0x30
    u32 hist_index = 0;                   // +0x34
    u32 hist_mask = 0;                    // +0x38
    u32 wrap = 0;                         // +0x40
    const D3dTimeSlot* history = nullptr; // +0x44
    D3dTimeSlot slots[64]{};              // +0x60
    const u32* reg_file = nullptr;        // +0x930
    const u32* notifier = nullptr;        // +0x1c10
};

namespace
{

u32 wrapped_put(u32 recorded, u32 put, u32 wrap)
{
    if (recorded <= put)
    {
        recorded += wrap;
    }
    return recorded;
}

// True when every slot after index, up to but not including end, still
// sits at or ahead of index's put. This is the test 0x001770A0 runs
// before it returns a slot. mask is 0x3f for the inline table.
bool later_puts_hold(
    const D3dTimeSlot* table, u32 index, u32 end, u32 mask, u32 put, u32 wrap)
{
    const u32 baseline = wrapped_put(table[index].put, put, wrap);
    u32 next = (index + 1u) & mask;
    while (next != end)
    {
        if (wrapped_put(table[next].put, put, wrap) < baseline)
        {
            return false;
        }
        next = (next + 1u) & mask;
    }
    return true;
}

const D3dTimeSlot* inline_time_slot(const D3dPushGetState* device, u32 index)
{
    const u32 newest = (device->fence >> 1) & 0x3fu;
    if (!later_puts_hold(device->slots, index, newest, 0x3fu, device->put, device->wrap))
    {
        return nullptr;
    }
    return &device->slots[index];
}

} // namespace

// Xbox 0x001770A0
const D3dTimeSlot* d3d_find_fence_time_slot(const D3dPushGetState* device, u32 query)
{
    // Xbox 0x001770A0
    // eax is the serial 0x001771A0 passes (counter + 2). Retail loads the
    // device from DAT_00181E08; the port uses the caller's device. A null
    // device would fault on Xbox; the port returns no slot.
    //
    // A gap of 0x80 is one lap of the 64-slot table (serials step by 2).
    // Inside that window the inline slot is (serial >> 1) & 0x3f. Beyond
    // it, the history ring is walked from hist_index + 1 until a record
    // is at least as new as the query. The head record being older than
    // the query drops back to the inline slot for the fence itself. The
    // compare against -1 in the retail index is unreachable after the
    // 0x3f mask. A null history cannot be walked; the port returns no slot.
    if (device == nullptr)
    {
        return nullptr;
    }

    const u32 fence = device->fence;
    const u32 gap = fence - query;
    if (gap > 0x80u)
    {
        if (device->history == nullptr)
        {
            return nullptr;
        }
        const u32 head = device->hist_index;
        if ((fence - device->history[head].serial) > gap)
        {
            return inline_time_slot(device, (fence >> 1) & 0x3fu);
        }
        const u32 mask = device->hist_mask;
        u32 index = head;
        do
        {
            index = (index + 1u) & mask;
        }
        while ((fence - device->history[index].serial) > gap);

        const u32 stop = (head + 1u) & mask;
        if (!later_puts_hold(device->history, index, stop, mask, device->put, device->wrap))
        {
            return nullptr;
        }
        return &device->history[index];
    }
    return inline_time_slot(device, (query >> 1) & 0x3fu);
}

// Xbox 0x001771A0
u32 d3d_gpu_push_buffer_position(const D3dPushGetState* device, int wait)
{
    // Xbox 0x001771A0
    // stdcall, ret 8. D3D_BlockOnTime (0x00177450) is the caller. The
    // first argument is the device at DAT_00181E08. The second is
    // nonzero only on the call after the fence kick: spin until the low
    // five bits of the GPU reference at reg_file+0x400b10 match the
    // counter pointed to by device+0x30. 0x001773A0 encodes that
    // reference as (put << 5) | (serial & 0x1f).
    //
    // notifier+0x44 is the hardware GET. ORing 0x80000000 is the
    // push-buffer address. While it sits in [device+0x24, device+0x28)
    // that address is the result.
    //
    // Outside the ring, *counter + 2 == the fence at device+0x2c means
    // the GPU has retired the last kick 0x001773A0 recorded (the fence
    // is already two ahead of that serial). The result is the put
    // pointer at device+0. Otherwise 0x001770A0 looks up the slot for
    // *counter + 2 and the result is the put stored at slot+4. A miss
    // returns put + 4. A null device, counter, or notifier would fault
    // on Xbox; the port returns 0. So does a wait with no register file.
    if (device == nullptr || device->counter == nullptr || device->notifier == nullptr)
    {
        return 0;
    }

    const volatile u32* counter = device->counter;
    if (wait != 0)
    {
        if (device->reg_file == nullptr)
        {
            return 0;
        }
        const volatile u32* reference = device->reg_file + (0x400b10u / 4u);
        do
        {
        }
        while (((*counter ^ *reference) & 0x1fu) != 0u);
    }

    const u32 get = device->notifier[0x44 / 4] | 0x80000000u;
    if (get < device->push_start || device->push_end <= get)
    {
        const u32 seen = *counter;
        if (seen + 2u == device->fence)
        {
            return device->put;
        }
        const D3dTimeSlot* slot = d3d_find_fence_time_slot(device, seen + 2u);
        if (slot != nullptr)
        {
            return slot->put;
        }
        return device->put + 4u;
    }
    return get;
}

// Xbox 0x00177220
void d3d_flush_write_combining(volatile u32* mmio)
{
    // Xbox 0x00177220
    // No arguments. Retail loads the device from DAT_00181E08 and the
    // NV2A register base at device+0x1c18. That pointer is RegisterBase
    // for AvSendTVEncoderOption (kernel ordinal 2) and AvSetDisplayMode
    // (ordinal 3). The port takes the base. A null base would fault on
    // Xbox; the port returns.
    //
    // sfence drains the CPU write-combining buffer. The dword at
    // base+0x100410 is NV_PFB_WBC: PFB is the block at 0x100000 and the
    // register itself is 0x410. ORing bit 16 requests NV_PFB_WBC_FLUSH.
    // The memory controller clears that bit once the framebuffer write
    // buffer has drained, and the function spins until the read says so.
    //
    // 0x00177450 calls this after writing NV097_WAIT_FOR_IDLE (method
    // word 0x40110) and NV097_NO_OPERATION (0x40100) into the push
    // buffer. 0x00177280 and 0x00177330 run the same poke before
    // publishing PUT at notifier+0x40. 0x0017C158 and 0x0017C176 call
    // it after the copy at 0x00176D60.
    if (mmio == nullptr)
    {
        return;
    }

    _mm_sfence();
    volatile u32* wbc = mmio + (0x100410u / 4u);
    const u32 latched = *wbc | 0x10000u;
    *wbc = latched;
    while ((*wbc & 0x10000u) != 0u)
    {
    }
}

// Xbox 0x001782C0
int d3d_truncate_sample_count(float value)
{
    // Xbox 0x001782C0
    // stdcall, ret 4. Nine bytes: cvttss2si eax, dword ptr [esp+4].
    // D3D_CommonSetMultiSampleModeAndScale (0x00171F40) calls it four
    // times. Each site adds the 0.5 at DAT_0032B0F4 and stores the x87
    // result before the call, so truncation rounds a positive value
    // half away from zero. The first two results are sample counts
    // clamped to the surface size. The third indexes the dword table
    // at 0x0017F99C (stored at device+0x964). The fourth is clamped
    // to 0x1FF by the caller and written as the data dword of method
    // 0x40380.
    //
    // Values outside the signed 32-bit range, and NaNs, come back as
    // the indefinite integer 0x80000000. In-range values truncate
    // toward zero.
    return _mm_cvttss_si32(_mm_set_ss(value));
}

// Xbox 0x001799A0
// Deferred commit used by CDevice_SetStateVB (0x00179A20) and D3DDevice_Begin
// (0x00172D10). The device argument is D3D_g_pDevice (DAT_00181E08); the
// shadow it reads is g. DAT_00181968 is g.rs_dirty.
//
// Bits are tested in this order. Only 0x0017DBD0 replaces the flag word the
// later tests see:
//   0x100     D3D_LazySetPointParams (0x001783E0), point scale DAT_00181D40
//   0x800     0x0017DBD0(device, flags)
//   0x4000    0x00178550(device)
//   0xF       0x001788D0(device, flags), one bit per texture stage
//   0x2000    0x00178AC0(device), including rs_state_60 at DAT_00181CF0
//   0x400     0x00178CA0(device)
//   0xFF1000  0x00179450(device, flags), reads rs_state_66
//   0x200     0x001797A0(device, flags), reads rs_state_66
//
// 0x0017DBD0 returns the flags unchanged when device+0x780 is set. Otherwise
// a combiner stage whose colour op is 25 or 26 (D3DTOP_MULTIPLYADD /
// D3DTOP_LERP) sets device+8 bit 0x10000, and the return ORs 0x400F. Colour
// op 1 (D3DTOP_DISABLE) ends the walk before the following stages. 0x400F is
// outside the mask stored below, so it only pulls the 0x4000 and stage
// commits into this same pass. The port has no pixel-shader handle at
// device+0x780, so the walk runs.
//
// Bits 0x10, 0x20, 0x40, 0x40000000 and 0x80000000 stay dirty.
// Xbox 0x001799A0
void d3d_commit_dirty_state()
{
    u32 flags = g.rs_dirty;
    if ((flags & 0x100u) != 0u)
    {
        // 0x001783E0 writes the point-sprite constants. No draw reads them yet.
    }
    if ((flags & 0x800u) != 0u)
    {
        for (int stage = 0; stage < 4; ++stage)
        {
            const int op = g.stages[stage].color_op;
            if (stage > 0 && op == 1)
            {
                break;
            }
            if (op == 25 || op == 26)
            {
                flags |= 0x400fu;
                break;
            }
        }
    }
    if ((flags & 0x4000u) != 0u)
    {
        // 0x00178550. Skipped on Xbox when device+0x780 is set.
    }
    if ((flags & 0xfu) != 0u)
    {
        // 0x001788D0 packs stages at DAT_00181970, stride 0x80.
    }
    if ((flags & 0x2000u) != 0u)
    {
        // 0x00178AC0.
    }
    if ((flags & 0x400u) != 0u)
    {
        // 0x00178CA0.
    }
    if ((flags & 0xff1000u) != 0u)
    {
        // 0x00179450.
    }
    if ((flags & 0x200u) != 0u)
    {
        // 0x001797A0.
    }
    g.rs_dirty = flags & 0xc0000070u;
}

} // namespace gbtg
