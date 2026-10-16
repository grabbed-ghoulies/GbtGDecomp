#include "xg_pixel_shader.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace gbtg
{
namespace
{
std::string compact(std::string text)
{
    text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char ch)
    {
        return std::isspace(ch) != 0;
    }), text.end());
    return text;
}

struct PixelRegister
{
    u32 reg;
    i32 constant = -1;
    u32 mapping = 0xC0;
    std::string channel;
};

PixelRegister pixel_register(std::string text)
{
    text = compact(text);
    PixelRegister result{};
    if (text.starts_with("1-"))
    {
        result.mapping = 0x20;
        text.erase(0, 2);
    }
    else if (text.starts_with('-'))
    {
        result.mapping = 0xE0;
        text.erase(0, 1);
    }
    const auto dot = text.find('.');
    if (dot != std::string::npos)
    {
        result.channel = text.substr(dot + 1);
        text.resize(dot);
    }
    if (text.ends_with("_bx2"))
    {
        text.resize(text.size() - 4);
        if (result.mapping != 0xC0 && result.mapping != 0xE0)
        {
            throw Error("XGAssembleShader: unsupported pixel source modifier combination");
        }
        result.mapping = result.mapping == 0xC0 ? 0x40 : 0x60;
    }
    if (text == "0" || text == "zero" || text == "discard")
    {
        result.reg = 0;
    }
    else if (text == "1")
    {
        result.reg = 0;
        result.mapping = 0x20;
    }
    else if (text.size() == 2 && std::isdigit(static_cast<unsigned char>(text[1])))
    {
        const u32 index = text[1] - '0';
        if (text[0] == 'c' && index < 8)
        {
            result.constant = i32(index);
        }
        else if (text[0] == 'r' && index < 2)
        {
            result.reg = 12 + index;
        }
        else if (text[0] == 'v' && index < 2)
        {
            result.reg = 4 + index;
        }
        else if (text[0] == 't' && index < 4)
        {
            result.reg = 8 + index;
        }
        else
        {
            throw Error("XGAssembleShader: unsupported pixel register " + text);
        }
    }
    else
    {
        throw Error("XGAssembleShader: unsupported pixel register " + text);
    }
    return result;
}

struct PixelOperation
{
    std::string name;
    std::vector<std::string> operands;
    u32 scale = 0;
};

PixelOperation pixel_operation(const std::string& text)
{
    std::istringstream tokens(text);
    PixelOperation operation;
    tokens >> operation.name;
    if (operation.name.ends_with("_x4"))
    {
        operation.scale = 4u << 15;
        operation.name.resize(operation.name.size() - 3);
    }
    std::string operand;
    while (std::getline(tokens, operand, ','))
    {
        operation.operands.push_back(compact(operand));
    }
    return operation;
}

struct PixelAssembler
{
    XboxPixelShaderDefinition words{};
    std::array<u32, 8> defined_colors{};
    std::array<bool, 8> defined{};
    u32 stages = 0;
    std::array<i32, 2> constants{-1, -1};
    bool rgb_written = false;
    bool alpha_written = false;

    u32 input(const std::string& text, bool alpha)
    {
        const PixelRegister value = pixel_register(text);
        u32 reg = value.reg;
        if (value.constant != -1)
        {
            u32 slot = 0;
            while (slot < 2 && constants[slot] != -1 && constants[slot] != value.constant)
            {
                ++slot;
            }
            if (slot == 2)
            {
                throw Error("XGAssembleShader: pixel stage reads more than two constants");
            }
            constants[slot] = value.constant;
            reg = slot + 1;
        }
        u32 channel = 0;
        // NV2A's channel bit selects RGB/blue (0) or alpha (0x10). A .b selector is the blue
        // input on the alpha combiner and the RGB input on the RGB combiner, not a general swizzle.
        if (value.channel == "a" || (alpha && (value.channel.empty() || value.channel == "rgba")))
        {
            channel = 0x10;
        }
        else if (!value.channel.empty() && value.channel != "rgb" && value.channel != "rgba" && value.channel != "b")
        {
            throw Error("XGAssembleShader: unsupported pixel source channel " + value.channel);
        }
        return reg | value.mapping | channel;
    }

    void emit_channel(const PixelOperation& operation, bool alpha)
    {
        const auto& args = operation.operands;
        const u32 stage = stages;
        if ((alpha && alpha_written) || (!alpha && rgb_written))
        {
            throw Error("XGAssembleShader: paired instructions overlap pixel channels");
        }
        if (alpha)
        {
            alpha_written = true;
        }
        else
        {
            rgb_written = true;
        }
        u32 inputs = 0, outputs = operation.scale;
        const auto pack = [this, alpha](const std::string& a, const std::string& b,
            const std::string& c, const std::string& d)
        {
            // input() claims constant slots, so the operands are read in order A, B, C, D.
            const u32 first = input(a, alpha);
            const u32 second = input(b, alpha);
            const u32 third = input(c, alpha);
            const u32 fourth = input(d, alpha);
            return (first << 24) | (second << 16) | (third << 8) | fourth;
        };
        const auto destination = [](const std::string& text)
        {
            const auto value = pixel_register(text);
            if (value.constant != -1 || (value.reg == 0 && !text.starts_with("discard")))
            {
                throw Error("XGAssembleShader: invalid pixel destination");
            }
            return value.reg;
        };
        const auto count = [&args](size_t required)
        {
            if (args.size() != required)
            {
                throw Error("XGAssembleShader: wrong pixel operand count");
            }
        };
        if (operation.name == "xdd" || operation.name == "xmma")
        {
            const bool dot = operation.name == "xdd";
            count(dot ? 6 : 7);
            if (alpha && dot)
            {
                throw Error("XGAssembleShader: alpha dot-product instruction");
            }
            const size_t first = dot ? 2 : 3;
            inputs = pack(args[first], args[first + 1], args[first + 2], args[first + 3]);
            outputs |= destination(args[0]) << 4;
            outputs |= destination(args[1]);
            if (dot)
            {
                outputs |= 0x3000;
            }
            else
            {
                outputs |= destination(args[2]) << 8;
            }
        }
        else
        {
            count(operation.name == "mov" ? 2 : (operation.name == "mad" || operation.name == "lrp" ||
                operation.name == "cnd" ? 4 : 3));
            const u32 dest = destination(args[0]);
            if (operation.name == "mov")
            {
                inputs = pack(args[1], "1", "0", "0");
                outputs |= dest << 4;
            }
            else if (operation.name == "mul" || operation.name == "dp3")
            {
                inputs = pack(args[1], args[2], "0", "0");
                outputs |= dest << 4;
                if (operation.name == "dp3")
                {
                    outputs |= 0x2000;
                }
            }
            else if (operation.name == "add" || operation.name == "sub")
            {
                inputs = pack(args[1], "1", operation.name == "sub" ? "-" + args[2] : args[2], "1");
                outputs |= dest << 8;
            }
            else if (operation.name == "mad")
            {
                inputs = pack(args[1], args[2], args[3], "1");
                outputs |= dest << 8;
            }
            else if (operation.name == "lrp")
            {
                inputs = pack(args[1], args[2], "1-" + args[1], args[3]);
                outputs |= dest << 8;
            }
            else if (operation.name == "cnd")
            {
                if (args[1] != "r0.a")
                {
                    throw Error("XGAssembleShader: cnd must select with r0.a");
                }
                // MUX selects CD for a set MSB, so the true operand goes in CD.
                inputs = pack(args[3], "1", args[2], "1");
                outputs |= (dest << 8) | 0x4000;
                words[53] |= 0x100;
            }
            else
            {
                throw Error("XGAssembleShader: unsupported pixel operation " + operation.name);
            }
        }
        words[(alpha ? 0 : 34) + stage] = inputs;
        words[(alpha ? 26 : 45) + stage] = outputs;
    }

    void emit(const PixelOperation& operation)
    {
        if (operation.operands.empty())
        {
            throw Error("XGAssembleShader: missing pixel operands");
        }
        // Dot-product instructions occupy RGB; alpha may be paired independently.
        if (operation.name == "xdd" || operation.name == "dp3")
        {
            emit_channel(operation, false);
            return;
        }
        const auto dest = pixel_register(operation.operands[0]);
        if (dest.channel.empty() || dest.channel == "rgba" || dest.channel == "rgb")
        {
            emit_channel(operation, false);
        }
        if (dest.channel.empty() || dest.channel == "rgba" || dest.channel == "a")
        {
            emit_channel(operation, true);
        }
        if (!dest.channel.empty() && dest.channel != "rgba" && dest.channel != "rgb" && dest.channel != "a")
        {
            throw Error("XGAssembleShader: unsupported pixel destination channel");
        }
    }

    void finish_stage()
    {
        for (u32 slot = 0; slot < 2; ++slot)
        {
            const i32 index = constants[slot];
            const u32 mapping_word = 57 + slot;
            const u32 shift = stages * 4;
            if (index >= 0)
            {
                if (defined[size_t(index)])
                {
                    words[10 + slot * 8 + stages] = defined_colors[size_t(index)];
                }
                else
                {
                    words[mapping_word] = (words[mapping_word] & ~(15u << shift)) | (u32(index) << shift);
                }
            }
        }
        ++stages;
        constants = {-1, -1};
        rgb_written = false;
        alpha_written = false;
    }

    void texture(const PixelOperation& operation)
    {
        const auto& args = operation.operands;
        if (args.empty())
        {
            throw Error("XGAssembleShader: texture instruction has no destination");
        }
        const auto dest = pixel_register(args[0]);
        if (dest.reg < 8 || dest.reg > 11 || dest.constant != -1)
        {
            throw Error("XGAssembleShader: invalid texture destination");
        }
        const u32 stage = dest.reg - 8;
        u32 mode;
        if (operation.name == "tex")
        {
            mode = 1;
        }
        else if (operation.name == "texcoord")
        {
            mode = 4;
        }
        else if (operation.name == "texkill")
        {
            mode = 5;
        }
        else if (operation.name == "texm3x2pad" || operation.name == "texm3x2tex")
        {
            if (args.size() != 2 || stage == 0)
            {
                throw Error("XGAssembleShader: invalid texture dot-product operands");
            }
            const auto src = pixel_register(args[1]);
            if (src.reg < 8 || src.reg >= dest.reg || src.constant != -1 || src.mapping != 0x40)
            {
                throw Error("XGAssembleShader: unsupported texture dot-product source");
            }
            mode = operation.name == "texm3x2pad" ? 0x11 : 9;
            words[55] |= 1u << ((stage - 1) * 4); // D3D signed dot mapping.
            if (stage >= 2)
            {
                words[56] |= (src.reg - 8) << (stage == 2 ? 16 : 20);
            }
        }
        else
        {
            throw Error("XGAssembleShader: unsupported texture operation " + operation.name);
        }
        if (args.size() != (operation.name.starts_with("texm") ? 2 : 1))
        {
            throw Error("XGAssembleShader: wrong texture operand count");
        }
        const u32 shift = stage * 5;
        words[54] = (words[54] & ~(31u << shift)) | (mode << shift);
    }
};
}

XboxPixelShaderDefinition assemble_xg_pixel_shader(std::string_view source)
{
    PixelAssembler assembler;
    assembler.words[57] = 0xFFFFFFFF;
    assembler.words[58] = 0xFFFFFFFF;
    assembler.words[59] = 0xFF;
    // Final combiner passes the arithmetic program's r0 RGB and alpha.
    assembler.words[8] = 0xC;
    assembler.words[9] = 0x1C00;
    std::string text(source);
    std::replace(text.begin(), text.end(), '\0', ' ');
    std::istringstream lines(text);
    std::string line;
    bool version = false;
    while (std::getline(lines, line))
    {
        const auto comment = line.find(';');
        if (comment != std::string::npos)
        {
            line.resize(comment);
        }
        if (compact(line).empty())
        {
            continue;
        }
        if (compact(line) == "xps.1.1")
        {
            version = true;
            continue;
        }
        if (!version)
        {
            throw Error("XGAssembleShader: missing xps.1.1 version");
        }
        const auto first = pixel_operation(line);
        if (first.name == "def")
        {
            if (first.operands.size() != 5)
            {
                throw Error("XGAssembleShader: wrong def operand count");
            }
            const auto dest = pixel_register(first.operands[0]);
            if (dest.constant < 0)
            {
                throw Error("XGAssembleShader: def requires a constant register");
            }
            std::array<u32, 4> color;
            for (u32 index = 0; index < 4; ++index)
            {
                color[index] = u32(std::clamp(std::stof(first.operands[index + 1]), 0.f, 1.f) * 255.f + .5f);
            }
            assembler.defined[size_t(dest.constant)] = true;
            assembler.defined_colors[size_t(dest.constant)] = (color[3] << 24) | (color[0] << 16) | (color[1] << 8) | color[2];
            continue;
        }
        if (first.name.starts_with("tex"))
        {
            assembler.texture(first);
            continue;
        }
        if (assembler.stages >= 8)
        {
            throw Error("XGAssembleShader: pixel shader exceeds eight arithmetic stages");
        }
        const auto paired = line.find('+');
        assembler.emit(pixel_operation(line.substr(0, paired)));
        if (paired != std::string::npos)
        {
            assembler.emit(pixel_operation(line.substr(paired + 1)));
        }
        assembler.finish_stage();
    }
    if (!version)
    {
        throw Error("XGAssembleShader: missing xps.1.1 version");
    }
    assembler.words[53] |= assembler.stages | 0x11000;
    return assembler.words;
}

// Xbox 0x00063F40
u32 create_runtime_boot_texture_pixel_shader(RuntimeBootPixelShaders& context)
{
    XboxPixelShaderDefinition definition{};
    definition[54] = 0x54421;
    definition[56] = 0x110000;
    definition[55] = 0x440;
    definition[53] = 1;
    definition[8] = 0x20080000;
    definition[9] = 0x1800;
    u32 handle;
    context.create(definition, handle);
    return handle;
}

// Xbox 0x00063FB0
void create_runtime_boot_pixel_shaders(RuntimeBootPixelShaders& context)
{
    const auto& image = context.image;
    for (u32 row = 0x00425DE8; row < 0x00425F28; row += 0x10)
    {
        const u32 factory = image.word(row);
        u32 handle;
        if (factory == 0)
        {
            const std::string_view source(reinterpret_cast<const char*>(image.pointer(image.word(row - 8))),
                image.word(row - 4));
            XboxPixelShaderDefinition definition;
            if (context.assemble(source, definition) == 0)
            {
                context.create(definition, handle);
            }
            else
            {
                handle = 0;
            }
        }
        else
        {
            handle = context.factory(factory);
        }
        image.set_word(row, 4, handle);
    }
}
}
