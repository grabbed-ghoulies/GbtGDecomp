#include "xbox_vertex_program.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace gbtg
{
namespace
{
u32 field(const u32* words, u32 word, u32 position, u32 size)
{
    return (words[word] >> position) & ((1u << size) - 1u);
}

enum : u32
{
    kMacNop = 0,
    kMacMov,
    kMacMul,
    kMacAdd,
    kMacMad,
    kMacDp3,
    kMacDph,
    kMacDp4,
    kMacDst,
    kMacMin,
    kMacMax,
    kMacSlt,
    kMacSge,
    kMacArl,
};

enum : u32
{
    kIluNop = 0,
    kIluMov,
    kIluRcp,
    kIluRcc,
    kIluRsq,
    kIluExp,
    kIluLog,
    kIluLit,
};

struct Operand
{
    u32 negate;
    std::array<u32, 4> swizzle;
    u32 temporary;
    u32 source; // 1 temporary, 2 input, 3 constant.
};

Operand operand(const u32* words, u32 which)
{
    Operand result{};
    switch (which)
    {
    case 0:
        result.negate = field(words, 1, 8, 1);
        result.swizzle = {field(words, 1, 6, 2), field(words, 1, 4, 2), field(words, 1, 2, 2), field(words, 1, 0, 2)};
        result.temporary = field(words, 2, 28, 4);
        result.source = field(words, 2, 26, 2);
        break;
    case 1:
        result.negate = field(words, 2, 25, 1);
        result.swizzle = {field(words, 2, 23, 2), field(words, 2, 21, 2), field(words, 2, 19, 2), field(words, 2, 17, 2)};
        result.temporary = field(words, 2, 13, 4);
        result.source = field(words, 2, 11, 2);
        break;
    default:
        result.negate = field(words, 2, 10, 1);
        result.swizzle = {field(words, 2, 8, 2), field(words, 2, 6, 2), field(words, 2, 4, 2), field(words, 2, 2, 2)};
        result.temporary = (field(words, 2, 0, 2) << 2) | field(words, 3, 30, 2);
        result.source = field(words, 3, 28, 2);
        break;
    }
    return result;
}

float dot3(const XboxVector& a, const XboxVector& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

XboxVector splat(float value)
{
    return {value, value, value, value};
}

void write_masked(XboxVector& output, const XboxVector& value, u32 mask)
{
    for (u32 component = 0; component < 4; ++component)
    {
        if ((mask & (8u >> component)) != 0)
        {
            output[component] = value[component];
        }
    }
}
}

void run_xbox_vertex_program(std::span<const u32> program, XboxVertexProgramState& state)
{
    if (program.empty() || (program[0] & 0xFFFF) != 0x2078)
    {
        throw Error("vertex program does not carry the NV2A program header");
    }
    const u32 count = program[0] >> 16;
    if (program.size() < 1 + size_t(count) * 4)
    {
        throw Error("vertex program is shorter than its instruction count");
    }
    std::array<XboxVector, 12> temporaries{};
    i32 address = 0;
    // R12 reads the position output.
    const auto temporary = [&](u32 index) -> XboxVector&
    {
        if (index == 12)
        {
            return state.outputs[0];
        }
        if (index >= temporaries.size())
        {
            throw Error("vertex program reads temporary r" + std::to_string(index));
        }
        return temporaries[index];
    };
    for (u32 index = 0; index < count; ++index)
    {
        const u32* words = program.data() + 1 + index * 4;
        const u32 mac = field(words, 1, 21, 4);
        const u32 ilu = field(words, 1, 25, 3);
        const u32 input = field(words, 1, 9, 4);
        const u32 constant = field(words, 1, 13, 8);
        const u32 relative = field(words, 3, 1, 1);
        const auto read = [&](u32 which)
        {
            const Operand source = operand(words, which);
            XboxVector value;
            switch (source.source)
            {
            case 1:
                value = temporary(source.temporary);
                break;
            case 2:
                value = state.inputs[input];
                break;
            case 3:
            {
                const i32 slot = i32(constant) + (relative != 0 ? address : 0);
                if (slot < 0 || slot >= i32(state.constants.size()))
                {
                    throw Error("vertex program reads a constant outside the register file");
                }
                value = state.constants[size_t(slot)];
                break;
            }
            default:
                throw Error("vertex program operand has no source");
            }
            XboxVector result;
            for (u32 component = 0; component < 4; ++component)
            {
                result[component] = value[source.swizzle[component]];
                if (source.negate != 0)
                {
                    result[component] = -result[component];
                }
            }
            return result;
        };
        XboxVector mac_result{};
        XboxVector ilu_result{};
        bool new_address = false;
        i32 next_address = 0;
        if (mac != kMacNop)
        {
            const XboxVector a = read(0);
            XboxVector b{};
            XboxVector c{};
            if (mac == kMacMul || mac == kMacMad || (mac >= kMacDp3 && mac <= kMacSge))
            {
                b = read(1);
            }
            if (mac == kMacAdd || mac == kMacMad)
            {
                c = read(2);
            }
            switch (mac)
            {
            case kMacMov:
                mac_result = a;
                break;
            case kMacMul:
                for (u32 n = 0; n < 4; ++n)
                {
                    mac_result[n] = a[n] * b[n];
                }
                break;
            case kMacAdd:
                for (u32 n = 0; n < 4; ++n)
                {
                    mac_result[n] = a[n] + c[n];
                }
                break;
            case kMacMad:
                for (u32 n = 0; n < 4; ++n)
                {
                    mac_result[n] = a[n] * b[n] + c[n];
                }
                break;
            case kMacDp3:
                mac_result = splat(dot3(a, b));
                break;
            case kMacDph:
                mac_result = splat(dot3(a, b) + b[3]);
                break;
            case kMacDp4:
                mac_result = splat(dot3(a, b) + a[3] * b[3]);
                break;
            case kMacDst:
                mac_result = {1.f, a[1] * b[1], a[2], b[3]};
                break;
            case kMacMin:
                for (u32 n = 0; n < 4; ++n)
                {
                    mac_result[n] = std::min(a[n], b[n]);
                }
                break;
            case kMacMax:
                for (u32 n = 0; n < 4; ++n)
                {
                    mac_result[n] = std::max(a[n], b[n]);
                }
                break;
            case kMacSlt:
                for (u32 n = 0; n < 4; ++n)
                {
                    mac_result[n] = a[n] < b[n] ? 1.f : 0.f;
                }
                break;
            case kMacSge:
                for (u32 n = 0; n < 4; ++n)
                {
                    mac_result[n] = a[n] >= b[n] ? 1.f : 0.f;
                }
                break;
            case kMacArl:
                new_address = true;
                next_address = i32(std::floor(a[0]));
                break;
            default:
                throw Error("vertex program uses MAC operation " + std::to_string(mac));
            }
        }
        if (ilu != kIluNop)
        {
            const float x = read(2)[0];
            switch (ilu)
            {
            case kIluMov:
                ilu_result = read(2);
                break;
            case kIluRcp:
                ilu_result = splat(1.f / x);
                break;
            case kIluRcc:
            {
                float value = 1.f / x;
                const float magnitude = std::clamp(std::fabs(value), 5.42101e-20f, 1.884467e+19f);
                ilu_result = splat(std::copysign(magnitude, value));
                break;
            }
            case kIluRsq:
                ilu_result = splat(1.f / std::sqrt(std::fabs(x)));
                break;
            case kIluExp:
            {
                const float whole = std::floor(x);
                ilu_result = {std::exp2(whole), x - whole, std::exp2(x), 1.f};
                break;
            }
            case kIluLog:
            {
                const float magnitude = std::fabs(x);
                if (magnitude == 0.f)
                {
                    const float infinity = -std::numeric_limits<float>::infinity();
                    ilu_result = {infinity, 1.f, infinity, 1.f};
                }
                else
                {
                    const float exponent = std::floor(std::log2(magnitude));
                    ilu_result = {exponent, magnitude / std::exp2(exponent), std::log2(magnitude), 1.f};
                }
                break;
            }
            case kIluLit:
            {
                const XboxVector source = read(2);
                const float power = std::clamp(source[3], -128.f, 128.f);
                const float diffuse = std::max(source[0], 0.f);
                const float specular = source[0] > 0.f ? std::pow(std::max(source[1], 0.f), power) : 0.f;
                ilu_result = {1.f, diffuse, specular, 1.f};
                break;
            }
            default:
                throw Error("vertex program uses ILU operation " + std::to_string(ilu));
            }
        }
        const u32 mac_mask = field(words, 3, 24, 4);
        const u32 target = field(words, 3, 20, 4);
        const u32 ilu_mask = field(words, 3, 16, 4);
        const u32 output_mask = field(words, 3, 12, 4);
        const u32 output_register = field(words, 3, 11, 1);
        const u32 output_address = field(words, 3, 3, 8);
        const u32 output_ilu = field(words, 3, 2, 1);
        if (mac != kMacNop && mac != kMacArl && mac_mask != 0)
        {
            write_masked(temporary(target), mac_result, mac_mask);
        }
        if (ilu != kIluNop && ilu_mask != 0)
        {
            // A paired ILU operation writes R1.
            write_masked(temporary(mac != kMacNop ? 1 : target), ilu_result, ilu_mask);
        }
        if (output_mask != 0 && (output_ilu != 0 ? ilu != kIluNop : (mac != kMacNop && mac != kMacArl)))
        {
            const XboxVector& value = output_ilu != 0 ? ilu_result : mac_result;
            if (output_register != 0)
            {
                if (output_address >= state.outputs.size())
                {
                    throw Error("vertex program writes output o" + std::to_string(output_address));
                }
                write_masked(state.outputs[output_address], value, output_mask);
            }
            else
            {
                if (output_address >= state.constants.size())
                {
                    throw Error("vertex program writes a constant outside the register file");
                }
                write_masked(state.constants[output_address], value, output_mask);
            }
        }
        if (new_address)
        {
            address = next_address;
        }
        if (field(words, 3, 0, 1) != 0)
        {
            return;
        }
    }
}
}
