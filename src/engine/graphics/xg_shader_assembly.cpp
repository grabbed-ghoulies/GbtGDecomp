#include "xg_shader_assembly.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <string>

namespace gbtg
{
namespace
{
struct Register
{
    char kind;
    i32 index;
    bool negative = false;
    std::string components;
};

Register parse_register(std::string text)
{
    text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char ch)
    {
        return std::isspace(ch) != 0;
    }), text.end());
    Register result{};
    if (!text.empty() && text[0] == '-')
    {
        result.negative = true;
        text.erase(0, 1);
    }
    const auto dot = text.find('.');
    if (dot != std::string::npos)
    {
        result.components = text.substr(dot + 1);
        text.resize(dot);
    }
    if (text.empty())
    {
        throw Error("XGAssembleShader: missing register");
    }
    result.kind = text[0];
    if (result.kind == 'o')
    {
        if (text == "oPos")
        {
            result.index = 0;
        }
        else if (text == "oD0" || text == "oD1")
        {
            result.index = 3 + (text.back() - '0');
        }
        else if (text == "oB0" || text == "oB1")
        {
            result.index = 7 + (text.back() - '0');
        }
        else if (text.size() == 3 && text.substr(0, 2) == "oT" && text[2] >= '0' && text[2] <= '3')
        {
            result.index = 9 + (text[2] - '0');
        }
        else
        {
            throw Error("XGAssembleShader: unsupported output " + text);
        }
    }
    else if (result.kind == 'r' || result.kind == 'v' || result.kind == 'c')
    {
        std::string number = text.substr(1);
        if (number.size() > 2 && number.front() == '[' && number.back() == ']')
        {
            number = number.substr(1, number.size() - 2);
        }
        size_t used = 0;
        result.index = std::stoi(number, &used);
        if (used != number.size() || (result.kind == 'r' && (result.index < 0 || result.index > 12)) ||
            (result.kind == 'v' && (result.index < 0 || result.index > 15)) ||
            (result.kind == 'c' && (result.index < -96 || result.index > 95)))
        {
            throw Error("XGAssembleShader: unsupported register " + text);
        }
    }
    else
    {
        throw Error("XGAssembleShader: unsupported register " + text);
    }
    return result;
}

u32 component(char ch)
{
    const auto index = std::string_view("xyzw").find(ch);
    if (index == std::string_view::npos)
    {
        throw Error("XGAssembleShader: invalid component");
    }
    return u32(index);
}

void encode_source(std::array<u32, 4>& words, const Register& reg, u32 slot, i32& input, i32& constant)
{
    u32 source;
    u32 temporary = 0;
    if (reg.kind == 'r')
    {
        source = 1;
        temporary = u32(reg.index);
    }
    else if (reg.kind == 'v')
    {
        source = 2;
        if (input != -1 && input != reg.index)
        {
            throw Error("XGAssembleShader: instruction reads two input registers");
        }
        input = reg.index;
        words[1] |= u32(input) << 9;
    }
    else if (reg.kind == 'c')
    {
        source = 3;
        if (constant != -1 && constant != reg.index + 96)
        {
            throw Error("XGAssembleShader: instruction reads two constant registers");
        }
        constant = reg.index + 96;
        words[1] |= u32(constant) << 13;
    }
    else
    {
        throw Error("XGAssembleShader: unsupported source register");
    }
    std::array<u32, 4> swizzle{0, 1, 2, 3};
    if (!reg.components.empty())
    {
        if (reg.components.size() > 4)
        {
            throw Error("XGAssembleShader: invalid swizzle");
        }
        for (u32 index = 0; index < 4; ++index)
        {
            swizzle[index] = component(reg.components[std::min(size_t(index), reg.components.size() - 1)]);
        }
    }
    const u32 packed = (swizzle[0] << 6) | (swizzle[1] << 4) | (swizzle[2] << 2) | swizzle[3];
    if (slot == 0)
    {
        words[1] |= (u32(reg.negative) << 8) | packed;
        words[2] |= (temporary << 28) | (source << 26);
    }
    else if (slot == 1)
    {
        words[2] |= (u32(reg.negative) << 25) | (packed << 17) | (temporary << 13) | (source << 11);
    }
    else
    {
        words[2] |= (u32(reg.negative) << 10) | (packed << 2) | (temporary >> 2);
        words[3] |= ((temporary & 3) << 30) | (source << 28);
    }
}

std::array<u32, 4> encode_instruction(const std::string& operation, const Register& destination,
    std::vector<Register> sources)
{
    std::array<u32, 4> words{};
    u32 mac = 0, ilu = 0;
    if (operation == "mov")
    {
        mac = 1;
    }
    else if (operation == "mul")
    {
        mac = 2;
    }
    else if (operation == "add" || operation == "sub")
    {
        mac = 3;
    }
    else if (operation == "mad")
    {
        mac = 4;
    }
    else if (operation == "dp3")
    {
        mac = 5;
    }
    else if (operation == "dp4")
    {
        mac = 7;
    }
    else if (operation == "min")
    {
        mac = 9;
    }
    else if (operation == "max")
    {
        mac = 10;
    }
    else if (operation == "sge")
    {
        mac = 12;
    }
    else if (operation == "rcc")
    {
        ilu = 3;
    }
    else if (operation == "rsq")
    {
        ilu = 4;
    }
    else
    {
        throw Error("XGAssembleShader: unsupported vertex operation " + operation);
    }
    const size_t count = ilu != 0 || mac == 1 ? 1 : (mac == 4 ? 3 : 2);
    if (sources.size() != count)
    {
        throw Error("XGAssembleShader: wrong vertex operand count");
    }
    if (operation == "sub")
    {
        sources[1].negative = !sources[1].negative;
    }
    words[1] = (mac << 21) | (ilu << 25);
    i32 input = -1, constant = -1;
    for (u32 index = 0; index < sources.size(); ++index)
    {
        const u32 slot = ilu != 0 ? 2 : (mac == 3 && index == 1 ? 2 : index);
        encode_source(words, sources[index], slot, input, constant);
    }
    u32 mask = destination.components.empty() ? 15 : 0;
    for (char ch : destination.components)
    {
        mask |= 8u >> component(ch);
    }
    if (destination.kind == 'r')
    {
        words[3] |= (u32(destination.index) << 20) | (mask << (ilu != 0 ? 16 : 24));
    }
    else if (destination.kind == 'o')
    {
        words[3] |= (mask << 12) | (1 << 11) | (u32(destination.index) << 3) | (u32(ilu != 0) << 2);
    }
    else
    {
        throw Error("XGAssembleShader: unsupported vertex destination");
    }
    return words;
}
}

std::vector<u32> assemble_xg_vertex_shader(std::string_view source)
{
    std::vector<std::array<u32, 4>> instructions;
    bool screenspace = false;
    std::string text(source);
    std::replace(text.begin(), text.end(), '\0', ' ');
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line))
    {
        line.resize(line.find(';') == std::string::npos ? line.size() : line.find(';'));
        std::istringstream tokens(line);
        std::string operation;
        bool paired = false;
        if (!(tokens >> operation))
        {
            continue;
        }
        if (operation == "vs.1.0" || operation == "xvs.1.1")
        {
            continue;
        }
        if (operation == "#pragma")
        {
            std::string directive;
            tokens >> directive;
            if (directive != "screenspace")
            {
                throw Error("XGAssembleShader: unsupported pragma");
            }
            screenspace = true;
            continue;
        }
        if (operation == "+")
        {
            // The boot table pairs MUL oPos.xyz with RCC r1.x reading oPos.w. Neither changes the
            // other's inputs, so separate instructions preserve the pair's observable register writes.
            tokens >> operation;
            if (operation != "rcc" || instructions.empty() ||
                ((instructions.back()[1] >> 21) & 15) != 2 ||
                ((instructions.back()[3] >> 3) & 255) != 0 ||
                ((instructions.back()[3] >> 12) & 15) != 14)
            {
                throw Error("XGAssembleShader: unsupported paired instruction");
            }
            paired = true;
        }
        std::string operand;
        std::vector<Register> registers;
        while (std::getline(tokens, operand, ','))
        {
            registers.push_back(parse_register(operand));
        }
        if (registers.empty())
        {
            throw Error("XGAssembleShader: instruction has no destination");
        }
        Register destination = registers.front();
        registers.erase(registers.begin());
        if (paired && (destination.kind != 'r' || destination.index != 1 || destination.components != "x" ||
            registers.size() != 1 || registers[0].kind != 'r' || registers[0].index != 12 ||
            registers[0].components != "w" || registers[0].negative))
        {
            throw Error("XGAssembleShader: paired instructions have unsupported register dependencies");
        }
        if (operation == "m4x4")
        {
            if (registers.size() != 2 || registers[1].kind != 'c' || !destination.components.empty())
            {
                throw Error("XGAssembleShader: unsupported matrix macro");
            }
            for (u32 index = 0; index < 4; ++index)
            {
                destination.components = std::string(1, "xyzw"[index]);
                instructions.push_back(encode_instruction("dp4", destination, registers));
                ++registers[1].index;
            }
        }
        else
        {
            instructions.push_back(encode_instruction(operation, destination, registers));
        }
    }
    if (!screenspace)
    {
        instructions.push_back(encode_instruction("mul", parse_register("oPos.xyz"),
            {parse_register("r12"), parse_register("c[-38]")}));
        instructions.push_back(encode_instruction("rcc", parse_register("r11.x"), {parse_register("r12.w")}));
        instructions.push_back(encode_instruction("mad", parse_register("oPos.xyz"),
            {parse_register("r12"), parse_register("r11.x"), parse_register("c[-37]")}));
    }
    if (instructions.empty() || instructions.size() > 136)
    {
        throw Error("XGAssembleShader: invalid vertex program length");
    }
    instructions.back()[3] |= 1;
    std::vector<u32> program{(u32(instructions.size()) << 16) | 0x2078};
    for (const auto& instruction : instructions)
    {
        program.insert(program.end(), instruction.begin(), instruction.end());
    }
    return program;
}

// Xbox 0x00065EE0
void create_runtime_boot_vertex_shaders(RuntimeBootVertexShaders& context)
{
    for (u32 row = 0x004271E4; row < 0x00427424; row += 0x18)
    {
        const auto& image = context.image;
        const u32 declaration = image.word(row, 0xC);
        const std::string_view source(reinterpret_cast<const char*>(image.pointer(image.word(row - 4))), image.word(row));
        std::vector<u32> program;
        u32 handle;
        if (context.assemble(source, program) == 0)
        {
            if (context.create(reinterpret_cast<const i32*>(image.pointer(declaration)), program.data(), handle, 0) != 0)
            {
                handle = 0;
            }
        }
        else
        {
            handle = 0;
        }
        image.set_word(row, 0x10, handle);
    }
}

// Xbox 0x00065F60
void bind_runtime_boot_vertex_shader(u32 index, RuntimeBootVertexShaders& context)
{
    const u32 offset = index * 0x18;
    context.bind(context.image.word(0x004271F4 + offset));
    const u32 constants = context.image.word(0x004271E8 + offset);
    if (constants != 0)
    {
        context.constants(0x60, reinterpret_cast<const float*>(context.image.pointer(constants)), 0xC);
    }
}
}
