#include "gfx_shader_sources.h"

#include <algorithm>
#include <cstring>

namespace gbtg
{

// Shared vertex input/output, the host mesh and sprite shaders (vs_main, ps_main) and their constant buffers.
const char* gfx_hlsl_common()
{
    return R"(
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
}

// NV2A register combiners (ps_model, ps_model_depth). Follows gfx_hlsl_common and gfx_hlsl_alpha_test.
const char* gfx_hlsl_combiners()
{
    return R"(
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
// PSDotMapping (word 55, stage n at bits (n - 1) * 4): 0 ZERO_TO_ONE, 1 MINUS_ONE_TO_ONE_D3D (each byte less 128,
// over 127), 4 HILO_1 (the 16-bit A:R and G:B halves of the texel's 32 bits, and 1).
float3 ps_dotmap(uint stage, float4 c)
{
    uint mapping = (ps_word(55) >> ((stage - 1) * 4)) & 7;
    if (mapping == 1)
    {
        return (round(saturate(c.rgb) * 255) - 128) / 127;
    }
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
    // DOTPRODUCT (17) on stage 1 with DOT_ST (9) on stage 2 (texm3x2pad, texm3x2tex): stage 1 dots its whole
    // coordinates with t0, stage 2 its own with its PSInputTexture (word 56, bits 16), and stage 2's texture is
    // sampled at the two dot products (scaled as stage 2's coordinates are for a linear texture).
    if (((ps_word(54) >> 5) & 31) == 17 && ((ps_word(54) >> 10) & 31) == 9)
    {
        float2 st = float2(dot(i.tq1.xyz, ps_dotmap(1, regs[8])),
            dot(i.tq2.xyz, ps_dotmap(2, regs[8 + ((ps_word(56) >> 16) & 3)])));
        float2 st_scale = ps_uv_scale[1].xy;
        regs[9] = 0;
        regs[10] = stage_tex[2].Sample(stage_samp[2], any(st_scale != 0) ? st * st_scale : st);
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
    // CLIPPLANE (5): the stage's texture register is zero, and the pixel is discarded when one of the stage's texture
    // coordinates fails its PSCompareMode (word 42) bit: set, at or above zero; clear, below zero.
    [unroll]
    for (uint stage_clip = 0; stage_clip < 4; ++stage_clip)
    {
        if (((ps_word(54) >> (stage_clip * 5)) & 31) == 5)
        {
            float4 clip_coord = stage_clip == 0 ? i.tq0 : stage_clip == 1 ? i.tq1 : stage_clip == 2 ? i.tq2 : i.tq3;
            uint compare = ps_word(42) >> (stage_clip * 4);
            [unroll]
            for (uint axis = 0; axis < 4; ++axis)
            {
                if (((compare >> axis) & 1) != 0 ? clip_coord[axis] >= 0 : clip_coord[axis] < 0)
                {
                    discard;
                }
            }
            regs[8 + stage_clip] = 0;
        }
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
}

// Native scene draws: native_vs passes the recorded vertex through, native_ps modulates texture 0 by diffuse.
const char* gfx_hlsl_native()
{
    return R"(
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
}

// native_alpha_pass: the NV2A alpha test with the D3DCMP functions 0x200..0x207.
const char* gfx_hlsl_alpha_test()
{
    return R"(
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
}

// clear_vertex: one rectangle in clip space, for depth/stencil rectangle clears.
const char* gfx_hlsl_clear()
{
    return R"(
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
}

void gfx_check_native_pixel_definition(const u32* definition)
{
    if ((definition[53] & 255) > 8)
    {
        throw Error("native pixel shader exceeds eight combiner stages");
    }
    for (u32 stage = 0; stage < 4; ++stage)
    {
        const u32 mode = (definition[54] >> (stage * 5)) & 31;
        const u32 stage2 = (definition[54] >> 10) & 31;
        const bool dot = (stage == 2 && mode == 17) || (stage == 3 && mode == 10 && stage2 == 17)
            || (stage == 1 && mode == 17 && stage2 == 9) || (stage == 2 && mode == 9
                && ((definition[54] >> 5) & 31) == 17);
        if (mode > 1 && mode != 4 && mode != 5 && !dot)
        {
            throw Error("native pixel shader uses unsupported texture mode " + std::to_string(mode));
        }
        if (dot)
        {
            const u32 mapping = (definition[55] >> ((stage - 1) * 4)) & 7;
            if (mapping != 0 && mapping != 1 && mapping != 4)
            {
                throw Error("native pixel shader uses unsupported dot mapping " + std::to_string(mapping));
            }
        }
    }
}

std::array<u32, 60> gfx_native_pixel_key(const u32* definition)
{
    std::array<u32, 60> key{};
    std::copy_n(definition, key.size(), key.begin());
    std::fill(key.begin() + 10, key.begin() + 26, 0);
    key[43] = key[44] = 0;
    return key;
}

bool gfx_native_pixel_writes_depth(const std::array<u32, 60>& key)
{
    return ((key[54] >> 15) & 31) == 10;
}

std::string gfx_specialize_native_pixel_source(std::string source, const std::array<u32, 60>& key)
{
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
    return source;
}

} // namespace gbtg
