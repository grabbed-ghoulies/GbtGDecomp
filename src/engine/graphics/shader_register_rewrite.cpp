#include "shader_register_rewrite.h"
#include "shader_swizzle.h"
#include "shader_rewrite_program.h"
#include <bit>
#include <cstdlib>
#include <cstring>

namespace gbtg
{
namespace
{
u16 read_register_word(const u8* address)
{
    u16 value;
    std::memcpy(&value, address, 2);
    return value;
}
void write_register_word(u8* address, u16 value)
{
    std::memcpy(address, &value, 2);
}
}

// Xbox 0x00186D09
void mark_runtime_shader_register_lifetime(ShaderRegisterRewriteState& state, u16 lifetime,
    u16 instruction, u32, u8 vector)
{
    const u32* code = state.program + 1 + static_cast<u32>(instruction) * 3;
    u8* entry = state.lifetimes + static_cast<u32>(lifetime) * 0x12;
    const u16 scalar = static_cast<u16>(code[2] >> 29);
    if (vector == 0)
    {
        if (scalar != 0 && ((code[0] >> 16) & 15u) != 0)
        {
            entry[8] = 1;
            if ((code[2] & 0x1E000000u) != 0)
            {
                entry[9] = 1;
            }
        }
    }
    else if (scalar != 0 && ((code[0] >> 24) & 15u) != 0)
    {
        entry[10] = 1;
    }
    entry[11] = 1;
}

// Xbox 0x00186D67
bool runtime_shader_register_has_space(const ShaderRegisterRewriteState& state,
    const u8* occupied, u8 reg, u16 lifetime)
{
    const u8* channels = occupied + static_cast<u32>(reg) * 8;
    const u8* entry = state.lifetimes + static_cast<u32>(lifetime) * 0x12;
    const u32 mask = entry[6];
    if (entry[11] == 0)
    {
        u8 empty = 0;
        u8 required = 0;
        i32 channel = 0;
        do
        {
            if ((mask & (1u << (static_cast<u8>(channel) & 31u))) != 0)
            {
                ++required;
            }
            if (read_register_word(channels + channel * 2) == 0)
            {
                ++empty;
            }
            ++channel;
        } while (channel < 4);
        return empty >= required;
    }
    i32 channel = 0;
    do
    {
        if ((mask & (1u << (static_cast<u8>(channel) & 31u))) != 0
            && read_register_word(channels + channel * 2) != 0)
        {
            return false;
        }
        ++channel;
    } while (channel < 4);
    return true;
}

// Xbox 0x00186F20
u32 release_runtime_shader_register_channels(const ShaderRegisterRewriteState& state,
    const u8* references, u8* occupied, u16 instruction)
{
    u16 lifetime = 0;
    i32 channel = 0;
    do
    {
        const u16 value = read_register_word(references + channel * 2);
        if (value != 0)
        {
            lifetime = read_register_word(state.values + static_cast<u32>(value) * 10 + 6);
        }
        ++channel;
    } while (channel < 4);
    if (lifetime != 0)
    {
        const u8* entry = state.lifetimes + static_cast<u32>(lifetime) * 0x12;
        if (read_register_word(entry + 2) == instruction)
        {
            const u8 reg = entry[12];
            const u8 mask = entry[6];
            channel = 0;
            do
            {
                if ((static_cast<u32>(mask) & (1u << (static_cast<u8>(channel) & 31u))) != 0)
                {
                    const u32 mapped = 3u - entry[channel + 13];
                    write_register_word(occupied + static_cast<u32>(reg) * 8 + mapped * 2, 0);
                }
                ++channel;
            } while (channel < 4);
        }
    }
    return 0;
}

// Xbox 0x00186F9D
u8 remap_runtime_shader_register_mask(u8 mask, const u8* lifetime)
{
    u8 output = 0;
    i32 channel = 0;
    do
    {
        if ((static_cast<u32>(mask) & (1u << (static_cast<u8>(channel) & 31u))) != 0)
        {
            output = static_cast<u8>(output | (1u << ((3u - lifetime[channel + 13]) & 31u)));
        }
        ++channel;
    } while (channel < 4);
    return output;
}

// Xbox 0x00186A7B
void collect_runtime_shader_register_sources(ShaderRegisterRewriteState& state, u8 mask, u8 type,
    u8 reg, u8 x, u8 y, u8 z, u8 w, u8* references, u8* current, u16 instruction)
{
    if (mask == 0 || type != 1)
    {
        return;
    }
    i32 channel = 0;
    do
    {
        if ((static_cast<u32>(mask) & (1u << (static_cast<u8>(channel) & 31u))) == 0)
        {
            write_register_word(references + channel * 2, 0);
        }
        else
        {
            u32 component = mask;
            if (channel == 0)
            {
                component = w;
            }
            else if (channel == 1)
            {
                component = z;
            }
            else if (channel == 2)
            {
                component = y;
            }
            else if (channel == 3)
            {
                component = x;
            }
            const u16 value = read_register_word(current + static_cast<u32>(reg) * 8 + (3u - component) * 2);
            u8* entry = state.values + static_cast<u32>(value) * 10;
            write_register_word(entry + 2, instruction);
            const u16 lifetime = read_register_word(entry + 6);
            write_register_word(state.lifetimes + static_cast<u32>(lifetime) * 0x12 + 2, instruction);
            write_register_word(references + channel * 2, value);
        }
        ++channel;
    } while (channel < 4);
    u16 target_lifetime = 0;
    channel = 0;
    do
    {
        const u16 value = read_register_word(references + channel * 2);
        if (value != 0)
        {
            const u16 lifetime = read_register_word(state.values + static_cast<u32>(value) * 10 + 6);
            if (target_lifetime == 0 || lifetime < target_lifetime)
            {
                target_lifetime = lifetime;
            }
        }
        ++channel;
    } while (channel < 4);
    channel = 0;
    do
    {
        const u16 value = read_register_word(references + channel * 2);
        if (value != 0)
        {
            const u16 lifetime = read_register_word(state.values + static_cast<u32>(value) * 10 + 6);
            if (target_lifetime != lifetime)
            {
                u8* source = state.lifetimes + static_cast<u32>(lifetime) * 0x12;
                u8* target = state.lifetimes + static_cast<u32>(target_lifetime) * 0x12;
                if (read_register_word(source) != 0)
                {
                    u16 target_value = read_register_word(target + 4);
                    u16 source_value = read_register_word(source + 4);
                    bool first = true;
                    u16 previous = 0;
                    while (true)
                    {
                        if (target_value == 0)
                        {
                            if (source_value == 0)
                            {
                                break;
                            }
                        }
                        if (target_value == 0 || (source_value != 0 && source_value < target_value))
                        {
                            if (first)
                            {
                                write_register_word(target + 4, source_value);
                            }
                            if (previous != 0)
                            {
                                write_register_word(state.values + static_cast<u32>(previous) * 10 + 8, source_value);
                            }
                            write_register_word(state.values + static_cast<u32>(source_value) * 10 + 6, target_lifetime);
                            previous = source_value;
                            source_value = read_register_word(state.values + static_cast<u32>(source_value) * 10 + 8);
                        }
                        else
                        {
                            if (previous != 0)
                            {
                                write_register_word(state.values + static_cast<u32>(previous) * 10 + 8, target_value);
                            }
                            previous = target_value;
                            target_value = read_register_word(state.values + static_cast<u32>(target_value) * 10 + 8);
                        }
                        first = false;
                    }
                    if (previous != 0)
                    {
                        write_register_word(state.values + static_cast<u32>(previous) * 10 + 8, 0);
                    }
                    u16 start = read_register_word(target);
                    if (read_register_word(source) <= start)
                    {
                        start = read_register_word(source);
                    }
                    write_register_word(target, start);
                    u16 end = read_register_word(target + 2);
                    if (end <= read_register_word(source + 2))
                    {
                        end = read_register_word(source + 2);
                    }
                    write_register_word(target + 2, end);
                    target[6] |= source[6];
                    target[8] = static_cast<u8>(target[8] != 0 || source[8] != 0);
                    target[9] = static_cast<u8>(target[9] != 0 || source[9] != 0);
                    target[10] = static_cast<u8>(target[10] != 0 || source[10] != 0);
                    target[11] = static_cast<u8>(target[11] != 0 || source[11] != 0);
                    const u32 zero = 0;
                    std::memcpy(source, &zero, 4);
                    std::memcpy(source + 4, &zero, 4);
                    std::memcpy(source + 8, &zero, 4);
                    std::memcpy(source + 12, &zero, 4);
                    write_register_word(source + 16, 0);
                }
            }
        }
        ++channel;
    } while (channel < 4);
}

// Xbox 0x0018B6CC
u32 collect_runtime_shader_program_values(ShaderRegisterRewriteState& state)
{
    u8 current[128] = {};
    state.value_count = 1;
    state.lifetime_count = 1;
    const u16 count = static_cast<u16>(state.program[0]);
    u32 instruction = 0;
    if (count != 0)
    {
        u32 reference_offset = 0;
        u32 code_offset = 0;
        do
        {
            const u32* code = state.program + 1 + code_offset;
            u8 masks[3];
            decode_runtime_shader_operand_masks(code, masks);
            collect_runtime_shader_register_sources(state, masks[0], static_cast<u8>((code[1] >> 28) & 3u),
                static_cast<u8>(code[2] & 15u), static_cast<u8>((code[2] >> 10) & 3u),
                static_cast<u8>((code[2] >> 8) & 3u), static_cast<u8>((code[2] >> 6) & 3u),
                static_cast<u8>((code[2] >> 4) & 3u), state.references + reference_offset, current,
                static_cast<u16>(instruction));
            collect_runtime_shader_register_sources(state, masks[1], static_cast<u8>((code[1] >> 13) & 3u),
                static_cast<u8>((code[1] >> 15) & 15u), static_cast<u8>((code[1] >> 25) & 3u),
                static_cast<u8>((code[1] >> 23) & 3u), static_cast<u8>((code[1] >> 21) & 3u),
                static_cast<u8>((code[1] >> 19) & 3u), state.references + reference_offset + 8, current,
                static_cast<u16>(instruction));
            collect_runtime_shader_register_sources(state, masks[2], static_cast<u8>((code[0] >> 28) & 3u),
                static_cast<u8>(code[1] & 15u), static_cast<u8>((code[1] >> 10) & 3u),
                static_cast<u8>((code[1] >> 8) & 3u), static_cast<u8>((code[1] >> 6) & 3u),
                static_cast<u8>((code[1] >> 4) & 3u), state.references + reference_offset + 16, current,
                static_cast<u16>(instruction));
            u8 mask = static_cast<u8>((code[0] >> 24) & 15u);
            if (mask != 0)
            {
                create_runtime_shader_register_value(state, mask, static_cast<u8>((code[0] >> 20) & 15u),
                    state.references + reference_offset + 24, current, instruction, 1);
            }
            mask = static_cast<u8>((code[0] >> 16) & 15u);
            if (mask != 0)
            {
                u8 reg;
                if ((code[2] & 0x1E000000u) == 0)
                {
                    reg = static_cast<u8>((code[0] >> 20) & 15u);
                }
                else
                {
                    reg = 1;
                }
                create_runtime_shader_register_value(state, mask, reg, state.references + reference_offset + 26,
                    current, instruction, 0);
            }
            ++instruction;
            code_offset += 3;
            reference_offset += 28;
        } while (static_cast<u16>(instruction) < count);
    }
    return 1;
}

// Xbox 0x00186DE0
u32 assign_runtime_shader_register_channels(ShaderRegisterRewriteState& state, u8* occupied, u8 reg, u16 lifetime)
{
    u8* channels = occupied + static_cast<u32>(reg) * 8;
    u8* entry = state.lifetimes + static_cast<u32>(lifetime) * 0x12;
    const u8 mask = entry[6];
    i32 channel = 0;
    do
    {
        if ((static_cast<u32>(mask) & (1u << (static_cast<u8>(channel) & 31u))) != 0)
        {
            if (read_register_word(channels + channel * 2) != 0)
            {
                if (entry[11] != 0)
                {
                    return 0x80004005u;
                }
                u8 free_channel = 0;
                channel = 0;
                do
                {
                    if ((static_cast<u32>(mask) & (1u << (static_cast<u8>(channel) & 31u))) != 0)
                    {
                        while (read_register_word(channels + static_cast<u32>(free_channel) * 2) != 0)
                        {
                            if (free_channel > 3)
                            {
                                return 0x80004005u;
                            }
                            ++free_channel;
                        }
                        if (free_channel > 3)
                        {
                            return 0x80004005u;
                        }
                        write_register_word(channels + static_cast<u32>(free_channel) * 2, lifetime);
                        entry[12] = reg;
                        entry[channel + 13] = static_cast<u8>(3u - free_channel);
                    }
                    ++channel;
                    if (channel > 3)
                    {
                        return 0;
                    }
                } while (true);
            }
            write_register_word(channels + channel * 2, lifetime);
            entry[12] = reg;
            entry[channel + 13] = static_cast<u8>(3u - static_cast<u8>(channel));
        }
        ++channel;
        if (channel > 3)
        {
            return 0;
        }
    } while (true);
}

// Xbox 0x00186E96
u32 allocate_runtime_shader_register(ShaderRegisterRewriteState& state, u8* occupied, u16 lifetime, u8 fixed)
{
    constexpr u8 candidates[11] = {0,2,3,4,5,6,7,8,9,10,11}; // DAT_001A2410.
    ++state.cursor;
    if (state.cursor > 10)
    {
        state.cursor = static_cast<u8>(state.cursor - 11);
    }
    const u8 start = state.cursor;
    u32 attempt = 0;
    do
    {
        u32 index = start + attempt;
        if (index > 10)
        {
            index = static_cast<u32>(start) - 11u + attempt;
        }
        const u8 reg = candidates[index];
        if (runtime_shader_register_has_space(state, occupied, reg, lifetime))
        {
            return assign_runtime_shader_register_channels(state, occupied, reg, lifetime);
        }
        ++attempt;
    } while (attempt < 11);
    if (fixed == 0 && runtime_shader_register_has_space(state, occupied, 1, lifetime))
    {
        return assign_runtime_shader_register_channels(state, occupied, 1, lifetime);
    }
    return 0x80004005u;
}

// Xbox 0x0018A24E
void create_runtime_shader_register_value(ShaderRegisterRewriteState& state, u8 mask, u8 reg,
    u8* reference, u8* current, u32 instruction, u32 vector)
{
    const u16 value = state.value_count;
    state.value_count = static_cast<u16>(value + 1);
    i32 channel = 0;
    do
    {
        if ((static_cast<u32>(mask) & (1u << (static_cast<u8>(channel) & 31u))) != 0)
        {
            write_register_word(current + static_cast<u32>(reg) * 8 + channel * 2, value);
        }
        ++channel;
    } while (channel < 4);
    const u16 lifetime = state.lifetime_count;
    state.lifetime_count = static_cast<u16>(lifetime + 1);
    write_register_word(reference, value);
    u8* value_entry = state.values + static_cast<u32>(value) * 10;
    value_entry[4] = reg;
    value_entry[5] = mask;
    write_register_word(value_entry + 6, lifetime);
    write_register_word(value_entry, static_cast<u16>(instruction));
    write_register_word(value_entry + 2, static_cast<u16>(instruction));
    u8* lifetime_entry = state.lifetimes + static_cast<u32>(lifetime) * 0x12;
    lifetime_entry[6] = mask;
    write_register_word(lifetime_entry, static_cast<u16>(instruction));
    write_register_word(lifetime_entry + 2, static_cast<u16>(instruction));
    write_register_word(lifetime_entry + 4, value);
    lifetime_entry[7] = reg;
    mark_runtime_shader_register_lifetime(state, lifetime, static_cast<u16>(instruction), mask, static_cast<u8>(vector));
}

// Xbox 0x0018A302
u32 assign_runtime_shader_register_value(ShaderRegisterRewriteState& state, u16 value, u8* occupied, u16 instruction)
{
    if (value != 0)
    {
        const u16 lifetime = read_register_word(state.values + static_cast<u32>(value) * 10 + 6);
        const u8* entry = state.lifetimes + static_cast<u32>(lifetime) * 0x12;
        if (read_register_word(entry) == instruction)
        {
            u8 fixed;
            if (entry[8] == 0 || entry[10] != 0)
            {
                fixed = entry[10];
            }
            else
            {
                if (runtime_shader_register_has_space(state, occupied, 1, lifetime))
                {
                    return assign_runtime_shader_register_channels(state, occupied, 1, lifetime);
                }
                if (entry[9] != 0)
                {
                    return 0x80004005u;
                }
                fixed = 0;
            }
            return allocate_runtime_shader_register(state, occupied, lifetime, fixed);
        }
    }
    return 0;
}

// Xbox 0x0018B8D0
u32 allocate_runtime_shader_program_registers(ShaderRegisterRewriteState& state)
{
    u8 occupied[128] = {};
    u32 result = 0;
    u32 instruction = 0;
    if (std::bit_cast<i32>(state.program[0]) > 0)
    {
        do
        {
            if (std::bit_cast<i32>(result) < 0)
            {
                return result;
            }
            u8* references = state.references + static_cast<u32>(static_cast<u16>(instruction)) * 0x1C;
            result = assign_runtime_shader_register_value(state, read_register_word(references + 0x18),
                occupied, static_cast<u16>(instruction));
            if (std::bit_cast<i32>(result) >= 0)
            {
                result = assign_runtime_shader_register_value(state, read_register_word(references + 0x1A),
                    occupied, static_cast<u16>(instruction));
                if (std::bit_cast<i32>(result) >= 0)
                {
                    i32 operand = 0;
                    do
                    {
                        if (std::bit_cast<i32>(result) < 0)
                        {
                            break;
                        }
                        result = release_runtime_shader_register_channels(state, references, occupied,
                            static_cast<u16>(instruction));
                        ++operand;
                        references += 8;
                    } while (operand < 3);
                }
            }
            ++instruction;
        } while (static_cast<i32>(static_cast<u16>(instruction)) < std::bit_cast<i32>(state.program[0]));
    }
    return result;
}

// Xbox 0x0018BD12
u32 rewrite_runtime_shader_program_registers(ShaderRegisterRewriteState& state, u32* program)
{
    state.references = nullptr;
    state.values = nullptr;
    state.value_count = 0;
    state.lifetimes = nullptr;
    state.lifetime_count = 0;
    state.cursor = 0;
    state.program = program;
    // XMemAlloc(size, 0x24870000), with actual host allocation success/failure.
    state.references = static_cast<u8*>(std::malloc(program[0] * 28u));
    u32 result = 0;
    if (state.references != nullptr)
    {
        i32 count = std::bit_cast<i32>(program[0]);
        i32 index = 0;
        if (count > 0)
        {
            u32 offset = 0;
            do
            {
                ++index;
                std::memset(state.references + offset, 0, 28);
                offset += 28;
            } while (index < std::bit_cast<i32>(program[0]));
        }
        count = std::bit_cast<i32>(program[0] * 2u + 1u);
        state.values = static_cast<u8*>(std::malloc(std::bit_cast<u32>(count) * 10u));
        if (state.values != nullptr)
        {
            if (count > 0)
            {
                u32 offset = 0;
                do
                {
                    std::memset(state.values + offset, 0, 10);
                    offset += 10;
                    --count;
                } while (count != 0);
            }
            count = std::bit_cast<i32>(program[0] * 2u + 1u);
            state.lifetimes = static_cast<u8*>(std::malloc(std::bit_cast<u32>(count) * 18u));
            if (state.lifetimes != nullptr)
            {
                if (count > 0)
                {
                    u32 offset = 0;
                    do
                    {
                        std::memset(state.lifetimes + offset, 0, 18);
                        offset += 18;
                        --count;
                    } while (count != 0);
                }
                result = collect_runtime_shader_program_values(state);
                if (std::bit_cast<i32>(result) >= 0)
                {
                    const u32 assigned = allocate_runtime_shader_program_registers(state);
                    if (std::bit_cast<i32>(assigned) >= 0)
                    {
                        result = apply_runtime_shader_register_mapping(state);
                    }
                    // Native EBX retains collection's result when allocation fails.
                }
            }
            else
            {
                result = 0x80004005u;
            }
        }
        else
        {
            result = 0x80004005u;
        }
    }
    else
    {
        result = 0x80004005u;
    }
    if (state.references != nullptr)
    {
        std::free(state.references); // XMemFree(pointer, 0x24870000).
    }
    state.references = nullptr;
    if (state.values != nullptr)
    {
        std::free(state.values);
    }
    state.values = nullptr;
    if (state.lifetimes != nullptr)
    {
        std::free(state.lifetimes);
    }
    state.lifetimes = nullptr;
    return result;
}

// Xbox 0x0018BF69
u32 rewrite_runtime_shader_register_one(ShaderRegisterRewriteState& state, u32, u32* program)
{
    u8 required;
    u32 result = runtime_shader_needs_register_one_rewrite(program, &required);
    if (std::bit_cast<i32>(result) >= 0 && required != 0)
    {
        result = rewrite_runtime_shader_program_registers(state, program);
    }
    return result;
}

// Xbox 0x0018A38E
u32 apply_runtime_shader_register_mapping(ShaderRegisterRewriteState& state)
{
    i32 index = 0;
    if (std::bit_cast<i32>(state.program[0]) > 0)
    {
        u32 reference_offset = 0;
        u32 code_offset = 0;
        do
        {
            const u8* references = state.references + reference_offset;
            u32* code = state.program + 1 + code_offset;
            i32 operand = 0;
            do
            {
                i32 channel = 0;
                const u8* value_reference = references + operand * 8;
                do
                {
                    const u16 value = read_register_word(value_reference);
                    if (value != 0)
                    {
                        const u16 lifetime = read_register_word(state.values + static_cast<u32>(value) * 10 + 6);
                        const u8* entry = state.lifetimes + static_cast<u32>(lifetime) * 0x12;
                        if (operand == 0)
                        {
                            code[2] ^= (static_cast<u32>(entry[12]) ^ code[2]) & 15u;
                            break;
                        }
                        if (operand == 1)
                        {
                            code[1] ^= ((static_cast<u32>(entry[12]) << 15) ^ code[1]) & 0x78000u;
                        }
                        else
                        {
                            if (operand != 2)
                            {
                                break;
                            }
                            code[1] ^= (static_cast<u32>(entry[12]) ^ code[1]) & 15u;
                        }
                        break;
                    }
                    ++channel;
                    value_reference += 2;
                } while (channel < 4);
                ++operand;
            } while (operand < 3);
            u16 value = read_register_word(references + 0x18);
            if (value != 0)
            {
                const u16 lifetime = read_register_word(state.values + static_cast<u32>(value) * 10 + 6);
                const u8* entry = state.lifetimes + static_cast<u32>(lifetime) * 0x12;
                code[0] ^= ((static_cast<u32>(entry[12]) << 20) ^ code[0]) & 0xF00000u;
                const u8 mask = remap_runtime_shader_register_mask(static_cast<u8>((code[0] >> 24) & 15u), entry);
                code[0] ^= ((static_cast<u32>(mask) << 24) ^ code[0]) & 0xF000000u;
            }
            value = read_register_word(references + 0x1A);
            if (value != 0)
            {
                const u16 lifetime = read_register_word(state.values + static_cast<u32>(value) * 10 + 6);
                const u8* entry = state.lifetimes + static_cast<u32>(lifetime) * 0x12;
                if ((code[2] & 0x1E000000u) == 0)
                {
                    code[0] ^= ((static_cast<u32>(entry[12]) << 20) ^ code[0]) & 0xF00000u;
                }
                const u32 output = code[0];
                const u8 mask = remap_runtime_shader_register_mask(static_cast<u8>((output >> 16) & 15u), entry);
                code[0] = ((static_cast<u32>(mask) << 16) ^ output) & 0xF0000u ^ output;
            }
            ++index;
            code_offset += 3;
            reference_offset += 0x1C;
        } while (index < std::bit_cast<i32>(state.program[0]));
    }
    return 0;
}
}
