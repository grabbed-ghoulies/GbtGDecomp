#include "xbox_vertex_hlsl.h"
#include <algorithm>
#include <cmath>

namespace gbtg
{

std::vector<XboxVertexAddressValidation> xbox_vertex_address_validation(std::span<const u32> program)
{
    std::vector<XboxVertexAddressValidation> result;
    result.push_back({});
    for (size_t n = 1; n + 3 < program.size(); n += 4)
    {
        const u32* w = program.data() + n;
        const u32 mac = (w[1] >> 21) & 15;
        if (mac == 13)
        {
            XboxVertexAddressValidation next;
            next.source = (w[2] >> 26) & 3;
            next.reg = next.source == 2 ? (w[1] >> 9) & 15 : (w[1] >> 13) & 255;
            next.component = (w[1] >> 6) & 3;
            next.negate = ((w[1] >> 8) & 1) != 0;
            result.push_back(next);
        }
        if ((w[3] & 2) != 0)
        {
            const u32 ilu = (w[1] >> 25) & 7;
            const bool a = mac != 0;
            const bool b = mac == 2 || mac == 4 || (mac >= 5 && mac <= 12);
            const bool c = mac == 3 || mac == 4 || ilu != 0;
            if ((a && ((w[2] >> 26) & 3) == 3) || (b && ((w[2] >> 11) & 3) == 3) || (c && ((w[3] >> 28) & 3) == 3))
            {
                auto& validation = result.back();
                const i32 reg = i32((w[1] >> 13) & 255);
                validation.minimum = std::min(validation.minimum, reg);
                validation.maximum = std::max(validation.maximum, reg);
                validation.relative = true;
            }
        }
        if ((w[3] & 1) != 0)
        {
            break;
        }
    }
    return result;
}
void validate_xbox_vertex_address(std::span<const XboxVertexAddressValidation> validations,
    const std::array<std::array<float, 4>, 16>& inputs,
    const std::array<std::array<float, 4>, 192>& constants)
{
    for (const auto& validation : validations)
    {
        if (!validation.relative)
        {
            continue;
        }
        float value = validation.source == 2 ? inputs[validation.reg][validation.component] :
            validation.source == 3 ? constants.at(validation.reg)[validation.component] : 0.f;
        if (validation.negate)
        {
            value = -value;
        }
        const float address = std::floor(value);
        if (!std::isfinite(address) || address + float(validation.minimum) < 0 || address + float(validation.maximum) >= 192)
        {
            throw Error("vertex program reads a constant outside the register file");
        }
    }
}

// Host NV2A presentation compiler, version 1. Programs outside this deliberately
// narrow subset retain the CPU interpreter, which remains responsible for raising.
std::string xbox_vertex_hlsl(std::span<const u32> program)
{
    if (program.empty() || (program[0] & 0xFFFF) != 0x2078 || program.size() < 1 + size_t(program[0] >> 16) * 4)
    {
        return {};
    }
    std::string source = "cbuffer NativeConstants : register(b3) { float4 vc[192]; };\nstruct ProgramInput {";
    for (u32 n = 0; n < 16; ++n)
    {
        source += "float4 v" + std::to_string(n) + " : TEXCOORD" + std::to_string(n) + ";";
    }
    source += "};\nVSOut program_vs(ProgramInput input) { precise float4 t[12]; precise float4 o[13];";
    source += "[unroll] for(uint n=0;n<12;++n) t[n]=0; [unroll] for(uint n=0;n<13;++n) o[n]=float4(0,0,0,1); o[3]=1; o[4]=0; int address=0;\n";
    const auto temp = [](u32 index)
    {
        return index == 12 ? std::string("o[0]") : "t[" + std::to_string(index) + "]";
    };
    const auto write = [&](const std::string& target, const std::string& value, u32 mask)
    {
        std::string result;
        for (u32 n = 0; n < 4; ++n)
        {
            if ((mask & (8u >> n)) != 0)
            {
                result += target + "." + "xyzw"[n] + "=" + value + "." + "xyzw"[n] + ";";
            }
        }
        return result;
    };
    for (u32 index = 0; index < program[0] >> 16; ++index)
    {
        const u32* w = program.data() + 1 + index * 4;
        const auto f = [w](u32 word, u32 position, u32 size)
        {
            return (w[word] >> position) & ((1u << size) - 1);
        };
        const u32 mac = f(1,21,4), ilu = f(1,25,3), target = f(3,20,4);
        // Address-register operations and constant writes need register-file validation.
        // Transcendentals remain reference-only until their edge cases are validated.
        if (mac > 13 || ilu > 4 || target > 12 ||
            (mac == 13 && (f(2,26,2) == 1 || f(3,1,1) != 0)) ||
            (f(3,12,4) != 0 && (f(3,11,1) == 0 || f(3,3,8) >= 13)))
        {
            return {};
        }
        bool valid = true;
        const auto read = [&](u32 which)
        {
            u32 negate, reg, kind;
            std::array<u32,4> swizzle;
            if (which == 0)
            {
                negate=f(1,8,1); reg=f(2,28,4); kind=f(2,26,2);
                swizzle={f(1,6,2),f(1,4,2),f(1,2,2),f(1,0,2)};
            }
            else if (which == 1)
            {
                negate=f(2,25,1); reg=f(2,13,4); kind=f(2,11,2);
                swizzle={f(2,23,2),f(2,21,2),f(2,19,2),f(2,17,2)};
            }
            else
            {
                negate=f(2,10,1); reg=(f(2,0,2)<<2)|f(3,30,2); kind=f(3,28,2);
                swizzle={f(2,8,2),f(2,6,2),f(2,4,2),f(2,2,2)};
            }
            std::string value;
            if (kind == 1 && reg <= 12)
            {
                value=temp(reg);
            }
            else if (kind == 2)
            {
                value="input.v"+std::to_string(f(1,9,4));
            }
            else if (kind == 3 && f(1,13,8) < 192)
            {
                value="vc["+std::to_string(f(1,13,8))+(f(3,1,1) != 0 ? "+address" : "")+"]";
            }
            else
            {
                valid=false;
                return std::string("float4(0,0,0,0)");
            }
            value += ".";
            for (u32 component : swizzle)
            {
                value += "xyzw"[component];
            }
            return negate != 0 ? "(-"+value+")" : value;
        };
        source += "{ precise float4 a=0,b=0,c=0,m=0,l=0;";
        if (mac != 0)
        {
            source += "a="+read(0)+";";
            if (mac == 2 || mac == 4 || (mac >= 5 && mac <= 12))
            {
                source += "b="+read(1)+";";
            }
            if (mac == 3 || mac == 4)
            {
                source += "c="+read(2)+";";
            }
        }
        // Snapshot ILU input before either MAC or ILU writes, as the interpreter does.
        if (ilu != 0)
        {
            source += "precise float4 iv="+read(2)+";";
        }
        switch (mac)
        {
        case 0: break;
        case 1: source += "m=a;"; break;
        case 2: source += "m=a*b;"; break;
        case 3: source += "m=a+c;"; break;
        case 4: source += "m=a*b+c;"; break;
        case 5: source += "m=(a.x*b.x+a.y*b.y+a.z*b.z).xxxx;"; break;
        case 6: source += "m=(a.x*b.x+a.y*b.y+a.z*b.z+b.w).xxxx;"; break;
        case 7: source += "m=(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w).xxxx;"; break;
        case 8: source += "m=float4(1,a.y*b.y,a.z,b.w);"; break;
        case 9:
        case 10:
            for (u32 n=0;n<4;++n)
            {
                const std::string component(1,"xyzw"[n]);
                source += "m."+component+"="+(mac == 9 ? "b."+component+"<a."+component+"?b."+component+":a."+component : "a."+component+"<b."+component+"?b."+component+":a."+component)+";";
            }
            break;
        case 11: source += "m=float4(a.x<b.x,a.y<b.y,a.z<b.z,a.w<b.w);"; break;
        case 12: source += "m=float4(a.x>=b.x,a.y>=b.y,a.z>=b.z,a.w>=b.w);"; break;
        case 13: source += "address=(int)floor(a.x);"; break;
        }
        switch (ilu)
        {
        case 0: break;
        case 1: source += "l=iv;"; break;
        case 2: source += "l=(1.0/iv.x).xxxx;"; break;
        case 3: source += "precise float r=1.0/iv.x; l=((asuint(r)&0x80000000u)!=0?-clamp(abs(r),5.42101e-20,1.884467e19):clamp(abs(r),5.42101e-20,1.884467e19)).xxxx;"; break;
        case 4: source += "l=(1.0/sqrt(abs(iv.x))).xxxx;"; break;
        }
        if (!valid)
        {
            return {};
        }
        if (mac != 0 && mac != 13)
        {
            source += write(temp(target), "m", f(3,24,4));
        }
        if (ilu != 0)
        {
            source += write(temp(mac != 0 ? 1 : target), "l", f(3,16,4));
        }
        if (f(3,12,4) != 0 && (f(3,2,1) != 0 ? ilu != 0 : (mac != 0 && mac != 13)))
        {
            source += write("o["+std::to_string(f(3,3,8))+"]", f(3,2,1) != 0 ? "l" : "m", f(3,12,4));
        }
        source += "}\n";
        if (f(3,0,1) != 0)
        {
            break;
        }
    }
    source += "VSOut result=(VSOut)0; precise float w=o[0].w; result.pos=float4((o[0].x/320-1)*w,(1-o[0].y/240)*w,o[0].z/16777215.0*w,w); result.diffuse=clamp(o[3],0,1); result.specular=clamp(o[4],0,1); result.uv0=o[9].xy;result.uv1=o[10].xy;result.uv2=o[11].xy;result.uv3=o[12].xy;result.tq0=o[9];result.tq1=o[10];result.tq2=o[11];result.tq3=o[12];result.fog=saturate(o[5].x);return result;}";
    return source;
}
}
