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
    // World-space key light. The vertex-shader constant that supplies this has not
    // been lifted yet; +Y is the stand-in.
    float light_x = 0.f;
    float light_y = 1.f;
    float light_z = 0.f;
    float skin = 0.f; // light_dir.w: rigid skin palette is active
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
    ComPtr<ID3D11DepthStencilState> depth_off;
    ComPtr<ID3D11BlendState> blend_opaque;
    ComPtr<ID3D11BlendState> blend_alpha;
    ComPtr<ID3D11Buffer> ui_vb;
    ComPtr<ID3D11ShaderResourceView> white;
    std::unordered_map<const Mesh*, GpuMesh> meshes;
    std::unordered_map<const RgbaImage*, ComPtr<ID3D11ShaderResourceView>> sprites;
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
    float4 light_dir; // xyz world key light, w = rigid skin on
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
        // Book RARE_celmap0 (256x2) runs dark to bright along U. Lit faces take the bright end.
        float3 n = normalize(i.nrm);
        float3 l = normalize(light_dir.xyz);
        float u = saturate(0.5 + 0.5 * dot(n, l));
        light = g_light.Sample(g_light_samp, float2(u, 0.25)).rgb;
    } else if (uv_sel.z > 0.5) {
        // Xbox D3D8 lightmaps are commonly MODULATE2X: 0.5 in the texture is neutral.
        light = g_light.Sample(g_light_samp, uv_l).rgb * 2.0;
    }
    return float4(albedo.rgb * light, albedo.a);
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
    ds.DepthEnable = FALSE;
    ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
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
           g.depth_off;
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
    g.context->OMSetDepthStencilState(g.depth_state.Get(), 0);
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

void gfx_end_frame()
{
    if (!g.ok)
    {
        return;
    }
    if (g.swap)
    {
        g.swap->Present(1, 0);
    }
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

} // namespace gbtg
