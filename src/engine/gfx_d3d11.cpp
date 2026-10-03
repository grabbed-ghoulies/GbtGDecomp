#if !defined(GBTG_GFX_D3D11)
#error gfx_d3d11.cpp is the PC DirectX 11 backend
#endif

#include "gbtg/gfx.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>

#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace gbtg
{
namespace
{

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
    float bone = 0.f;
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
};

struct DrawRange
{
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11ShaderResourceView> light;
    u32 start = 0;
    u32 count = 0;
    float albedo_uv = 0.f;
    float light_uv = 1.f;
    float use_light = 0.f;
    float use_cel = 0.f;
    std::vector<u8> skin;
};

struct GpuMesh
{
    ComPtr<ID3D11Buffer> vb;
    ComPtr<ID3D11Buffer> ib;
    std::vector<DrawRange> ranges;
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
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> cb;
    ComPtr<ID3D11Buffer> bone_cb;
    bool skin = false;
    static constexpr int kMaxBones = 128;
    float bones[kMaxBones * 16]{};
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
    ComPtr<ID3D11ShaderResourceView> white;
    std::unordered_map<const Mesh*, GpuMesh> meshes;
    std::unordered_map<const RgbaImage*, ComPtr<ID3D11ShaderResourceView>> sprites;
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
        int mag_filter = 0;     // +0x0C D3DTSS_MAGFILTER
        int min_filter = 0;     // +0x10 D3DTSS_MINFILTER
        int mip_filter = 0;     // +0x14 D3DTSS_MIPFILTER
        int max_mip_level = 0;  // +0x1C D3DTSS_MAXMIPLEVEL
        int color_op = 0;       // +0x30 D3DTSS_COLOROP
        int color_arg1 = 0;     // +0x38 D3DTSS_COLORARG1
        int color_arg2 = 0;     // +0x3C D3DTSS_COLORARG2
        int alpha_op = 0;       // +0x40 D3DTSS_ALPHAOP
        int alpha_arg1 = 0;     // +0x48 D3DTSS_ALPHAARG1
        int alpha_arg2 = 0;     // +0x4C D3DTSS_ALPHAARG2
        int tex_transform = 0;  // +0x54 D3DTSS_TEXTURETRANSFORMFLAGS
        int texcoord_index = 0; // +0x70 D3DTSS_TEXCOORDINDEX
    };
    StageRs stages[4]{};
};

GfxD3D11 g;

LRESULT CALLBACK gfx_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
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
    const Vec3 x = normalize(cross(up, z));
    const Vec3 y = cross(z, x);
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

bool compile_shader(const char* src, const char* entry, const char* profile, ComPtr<ID3DBlob>& blob)
{
    ComPtr<ID3DBlob> err;
    const HRESULT hr = D3DCompile(src, std::strlen(src), "gbtg", nullptr, nullptr, entry, profile, D3DCOMPILE_OPTIMIZATION_LEVEL1,
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
        if (i < mesh.bones.size())
        {
            verts[i].bone = float(mesh.bones[i]);
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
    bd.ByteWidth = u32(mesh.indices.size() * sizeof(u16));
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    sd.pSysMem = mesh.indices.data();
    if (FAILED(g.device->CreateBuffer(&bd, &sd, gpu.ib.GetAddressOf())))
    {
        return false;
    }

    std::vector<ComPtr<ID3D11ShaderResourceView>> textures;
    textures.reserve(mesh.textures.size());
    for (const auto& img : mesh.textures)
    {
        textures.push_back(upload_bgra(img.bgra.data(), img.width, img.height));
    }

    if (mesh.submeshes.empty())
    {
        DrawRange range;
        range.srv = g.white;
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
        range.count = sub.index_count;
        range.albedo_uv = float(sub.albedo_uv);
        range.light_uv = float(sub.lightmap_uv);
        if (sub.texture >= 0 && size_t(sub.texture) < textures.size() && textures[size_t(sub.texture)])
        {
            range.srv = textures[size_t(sub.texture)];
        }
        else
        {
            range.srv = g.white;
        }
        if (sub.celmap >= 0 && size_t(sub.celmap) < textures.size() && textures[size_t(sub.celmap)])
        {
            range.light = textures[size_t(sub.celmap)];
            range.use_cel = 1.f;
        }
        else if (sub.lightmap >= 0 && size_t(sub.lightmap) < textures.size() && textures[size_t(sub.lightmap)])
        {
            range.light = textures[size_t(sub.lightmap)];
            range.use_light = 1.f;
        }
        range.skin = sub.skin;
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
void fn_5e0c0()
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
void fn_5e110()
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
void fn_5e2a0()
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

} // namespace

bool gfx_init(const GfxConfig& cfg)
{
    g = {};
    g.cfg = cfg;
    const int width = cfg.width > 0 ? cfg.width : 960;
    const int height = cfg.height > 0 ? cfg.height : 540;
    g.cfg.width = width;
    g.cfg.height = height;

    if (cfg.visible)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = gfx_wndproc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"GbtGDecompGfx";
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        RegisterClassW(&wc);
        RECT rect{0, 0, width, height};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
        g.hwnd = CreateWindowW(wc.lpszClassName, L"GbtGDecomp", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
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
};
cbuffer Bones : register(b1) {
    float4x4 joints[128];
};
Texture2D g_tex : register(t0);
Texture2D g_light : register(t1);
SamplerState g_samp : register(s0);
SamplerState g_light_samp : register(s1);
struct VSIn { float3 pos : POSITION; float2 uv0 : TEXCOORD0; float2 uv1 : TEXCOORD1; float3 nrm : NORMAL; float bone : TEXCOORD2; };
struct VSOut { float4 pos : SV_Position; float2 uv0 : TEXCOORD0; float2 uv1 : TEXCOORD1; float3 nrm : TEXCOORD2; };
VSOut vs_main(VSIn i) {
    VSOut o;
    float3 p = i.pos;
    float3 nrm = i.nrm;
    if (light_dir.w > 0.5) {
        int bi = clamp((int)i.bone, 0, 127);
        p = mul(joints[bi], float4(i.pos, 1)).xyz;
        nrm = mul(joints[bi], float4(i.nrm, 0)).xyz;
    }
    o.pos = mul(mvp, float4(p, 1));
    o.uv0 = i.uv0;
    o.uv1 = i.uv1;
    o.nrm = nrm;
    return o;
}
float4 ps_main(VSOut i) : SV_Target {
    float2 uv_a = uv_sel.x < 0.5 ? i.uv0 : i.uv1;
    float2 uv_l = uv_sel.y < 0.5 ? i.uv0 : i.uv1;
    float4 albedo = g_tex.Sample(g_samp, uv_a);
    float3 light = 1.0;
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
    float4 color = float4(albedo.rgb * light, albedo.a);
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
    ComPtr<ID3DBlob> vs_blob;
    ComPtr<ID3DBlob> ps_blob;
    if (!compile_shader(hlsl, "vs_main", "vs_4_0", vs_blob) || !compile_shader(hlsl, "ps_main", "ps_4_0", ps_blob))
    {
        gfx_shutdown();
        return false;
    }
    g.device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, g.vs.GetAddressOf());
    g.device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, g.ps.GetAddressOf());
    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 2, DXGI_FORMAT_R32_FLOAT, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    g.device->CreateInputLayout(layout, 5, vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), g.layout.GetAddressOf());

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

    const u8 white_px[4] = {255, 255, 255, 255};
    g.white = upload_bgra(white_px, 1, 1);

    if (g.hwnd)
    {
        ShowWindow(g.hwnd, SW_SHOW);
    }
    g.ok = g.vs && g.ps && g.layout && g.cb && g.bone_cb && g.white && g.rtv && g.dsv && g.ui_vb && g.blend_alpha &&
           g.depth_off && g.depth_lequal && g.depth_lequal_ro;
    if (!g.ok)
    {
        std::cerr << "d3d11: pipeline incomplete\n";
        gfx_shutdown();
        return false;
    }
    return true;
}

void gfx_shutdown()
{
    g.meshes.clear();
    g.sprites.clear();
    g.white.Reset();
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
}

void gfx_begin_frame()
{
    if (!g.ok)
    {
        return;
    }
    const float clear[4] = {0.05f, 0.06f, 0.09f, 1.f};
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
    const float aspect = float(g.cfg.width) / float(g.cfg.height > 0 ? g.cfg.height : 1);
    g.mvp = mul(perspective(fov_y, aspect, 0.1f, 2000.f), look_at(position, at, up));
}

void gfx_draw_mesh(const Mesh& mesh, Vec3 origin, Vec3 scale)
{
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
    g.context->IASetIndexBuffer(gpu.ib.Get(), DXGI_FORMAT_R16_UINT, 0);
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
    for (const auto& range : gpu.ranges)
    {
        if (SUCCEEDED(g.context->Map(g.cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            GpuCB cb;
            std::memcpy(cb.mvp, draw_mvp.m, sizeof(cb.mvp));
            cb.albedo_uv = range.albedo_uv;
            cb.light_uv = range.light_uv;
            cb.use_light = range.use_light;
            cb.use_cel = range.use_cel;
            cb.skin = g.skin ? 1.f : 0.f;
            store_alpha_rs(cb);
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
                    const int joint = int(range.skin[size_t(i)]);
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
        ID3D11ShaderResourceView* srvs[2] = {range.srv ? range.srv.Get() : g.white.Get(),
                                             range.light ? range.light.Get() : g.white.Get()};
        g.context->PSSetShaderResources(0, 2, srvs);
        g.context->DrawIndexed(range.count, range.start, 0);
        tris += range.count / 3;
        if (range.use_light > 0.f || range.use_cel > 0.f)
        {
            lit += range.count / 3;
        }
    }
    if (!g.logged_draw)
    {
        std::cout << "d3d11 draw " << mesh.name << " tris " << tris << " lit " << lit << "\n";
        g.logged_draw = true;
    }
}

void gfx_size(int& width, int& height)
{
    width = g.cfg.width;
    height = g.cfg.height;
}

void gfx_draw_sprite(const RgbaImage& image, const GfxRect& rect)
{
    if (!g.ok || image.bgra.empty() || image.width <= 0 || image.height <= 0 || rect.w == 0.f || rect.h == 0.f)
    {
        return;
    }
    auto it = g.sprites.find(&image);
    if (it == g.sprites.end())
    {
        auto srv = upload_bgra(image.bgra.data(), image.width, image.height);
        it = g.sprites.emplace(&image, std::move(srv)).first;
    }
    if (!it->second)
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
        std::memcpy(mapped.pData, &cb, sizeof(cb));
        g.context->Unmap(g.cb.Get(), 0);
    }

    const UINT stride = sizeof(GpuVert);
    const UINT offset = 0;
    g.context->IASetVertexBuffers(0, 1, g.ui_vb.GetAddressOf(), &stride, &offset);
    g.context->OMSetDepthStencilState(g.depth_off.Get(), 0);
    g.context->OMSetBlendState(g.blend_alpha.Get(), nullptr, 0xFFFFFFFFu);
    ID3D11ShaderResourceView* srvs[2] = {it->second.Get(), g.white.Get()};
    g.context->PSSetShaderResources(0, 2, srvs);
    g.context->Draw(6, 0);
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
void present_32ab0()
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
void present_33480()
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
// 0x00107420. Opening the clip is Bink (0x001A6010) and stays in that library.
void present_movie_tick(PresentMovie* node)
{
    if (node->enabled == 0 || node->failed != 0)
    {
        return;
    }
    if (node->clip == nullptr)
    {
        return;
    }
}

// Xbox 0x00033A90
void present_movies()
{
    if (present_frame.movie_lock_a != 0 || present_frame.movie_lock_b != 0)
    {
        return;
    }
    for (PresentMovie* node = present_frame.movies; node != nullptr; node = node->next)
    {
        present_movie_tick(node);
    }
}

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
void present_swap()
{
    if (present_frame.swap_notify != nullptr)
    {
        present_frame.swap_notify(0);
    }
    if (g.ok && g.swap)
    {
        g.swap->Present(1, 0);
    }
    if (present_frame.swap_notify != nullptr)
    {
        present_frame.swap_notify(1);
    }
}

// Xbox 0x00033A90
void present_33a90()
{
    const u32 flags = present_clear_flags();
    present_set_viewport();
    present_clear(flags);
    fn_5e110();
    // 0x000F9780 retires the deferred span at DAT_005079E0.
    // 0x00036180 submits the draw span at DAT_004DF240 and resets it.
    present_movies();
    present_overlays();
    present_cameras();
    // 0x000F6F70 restores the HUD viewport from DAT_005096E8 / DAT_005096EA.
    // 0x00066080 draws the sprite span at DAT_00504960.
    present_swap();
    present_movies();
}

} // namespace

void gfx_end_frame()
{
    // Present 0x00033480, then 0x00032AB0, then 0x00033A90 (clear and swap).
    present_33480();
    present_32ab0();
    present_33a90();
}

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
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(g.device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf())))
    {
        return false;
    }
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

float* fn_151e64(float* out, const float* left, const float* right)
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

} // namespace gbtg
