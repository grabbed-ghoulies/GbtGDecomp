#include "shader_rewrite_context.h"
#include "shader_rewrite_storage.h"
#include "shader_rewrite_outputs.h"
#include <bit>
#include <cstdlib>

namespace gbtg
{
// Xbox 0x001864C4
u32 remap_runtime_shader_rewrite_mask(u8* output, u8 mask, u32 x, u32 y, u32 z, u32 w)
{
    *output = static_cast<u8>((((mask >> 3) & 1u) << ((3u - x) & 31u))
        | (((mask >> 2) & 1u) << ((3u - y) & 31u))
        | (((mask >> 1) & 1u) << ((3u - z) & 31u))
        | ((mask & 1u) << ((3u - w) & 31u)));
    return 0;
}

// Xbox 0x00186520
u32 decode_runtime_shader_rewrite_source(u8 mask, u8* sources, u8* output,
    const u32* instruction, u8 index, u16 type)
{
    if (mask == 0)
    {
        *output = 0xF1;
    }
    else
    {
        if (type == 1)
        {
            *output = static_cast<u8>(index - 0x20u);
        }
        else
        {
            if (type != 2)
            {
                if (type != 3)
                {
                    return 0;
                }
                *output = static_cast<u8>((instruction[2] >> 17) + 0x20u);
                if ((instruction[0] & 2u) == 0)
                {
                    return 0;
                }
                sources[6] = 0xF0;
                sources[7] = 8;
                return 0;
            }
            *output = static_cast<u8>((instruction[2] >> 13) & 15u);
        }
    }
    return 0;
}

// Xbox 0x0018657E
i32 filter_runtime_shader_rewrite_destination(u8* storage, const u32*, u8* destination)
{
    const u8 index = destination[0];
    u8 mask;
    if (index < 0x20 || index > 0xDF)
    {
        mask = static_cast<u8>(~storage[index + 0x100] & 15u);
    }
    else
    {
        mask = static_cast<u8>((~storage[index + 0x100] & 15u) | storage[0x200]);
    }
    destination[1] &= mask;
    if ((index > 0xDF && index < 0xEC) || index == 0xF0)
    {
        destination[1] = static_cast<u8>(destination[1] & storage[index] & 15u);
    }
    return 0;
}

// Xbox 0x001865DB
i32 retain_runtime_shader_rewrite_destination(u8* storage, const u32*, const u8* destination)
{
    storage[destination[0]] |= destination[1];
    storage[destination[0] + 0x100] |= destination[1];
    if (destination[0] == 0x10)
    {
        storage[0xEC] |= destination[1];
        storage[0x1EC] |= destination[1];
    }
    return 0;
}

// Xbox 0x00186617
i32 retain_runtime_shader_rewrite_source(u8* storage, const u32* instruction,
    const u8* source, const void*)
{
    const u8 index = source[0];
    if (index != 0xF1)
    {
        if ((instruction[0] & 2u) == 0 || index < 0x20 || index > 0xDF)
        {
            storage[index] |= source[1];
            storage[source[0] + 0x100] &= static_cast<u8>(~source[1]);
            if (source[0] == 0xEC)
            {
                storage[0x10] |= source[1];
                storage[0x110] &= static_cast<u8>(~source[1]);
            }
        }
        else
        {
            storage[0x200] |= source[1];
        }
    }
    return 0;
}

// Xbox 0x00189ADC
i32 filter_runtime_shader_rewrite_destinations(u8* storage, const u32* instruction, u8* destinations)
{
    i32 result = 0;
    if ((instruction[2] & 0x1E000000u) != 0)
    {
        result = filter_runtime_shader_rewrite_destination(storage, instruction, destinations + 3);
        if (result < 0)
        {
            return result;
        }
        if (destinations[0] != 0)
        {
            result = filter_runtime_shader_rewrite_destination(storage, instruction, destinations + 1);
        }
        if (result < 0)
        {
            return result;
        }
    }
    if ((instruction[2] & 0xE0000000u) != 0)
    {
        result = filter_runtime_shader_rewrite_destination(storage, instruction, destinations + 5);
    }
    if (result >= 0 && destinations[0] == 0)
    {
        result = filter_runtime_shader_rewrite_destination(storage, instruction, destinations + 1);
    }
    return result;
}

// Xbox 0x00189B3F
i32 retain_runtime_shader_rewrite_destinations(u8* storage, const u32* instruction, const u8* destinations)
{
    i32 result = 0;
    if ((instruction[2] & 0x1E000000u) != 0)
    {
        result = retain_runtime_shader_rewrite_destination(storage, instruction, destinations + 3);
        if (result < 0)
        {
            return result;
        }
        if (destinations[0] != 0)
        {
            result = retain_runtime_shader_rewrite_destination(storage, instruction, destinations + 1);
        }
        if (result < 0)
        {
            return result;
        }
    }
    if ((instruction[2] & 0xE0000000u) != 0)
    {
        result = retain_runtime_shader_rewrite_destination(storage, instruction, destinations + 5);
    }
    if (result >= 0 && destinations[0] == 0)
    {
        result = retain_runtime_shader_rewrite_destination(storage, instruction, destinations + 1);
    }
    return result;
}

// Xbox 0x00189BA2
i32 retain_runtime_shader_rewrite_sources(u8* storage, const u32* instruction, const u8* sources)
{
    // The third arguments are retail label addresses; 0x00186617 does not read them.
    i32 result = retain_runtime_shader_rewrite_source(storage, instruction, sources,
        reinterpret_cast<const void*>(0x001A281C));
    if (result >= 0)
    {
        result = retain_runtime_shader_rewrite_source(storage, instruction, sources + 2,
            reinterpret_cast<const void*>(0x001A2818));
        if (result >= 0)
        {
            result = retain_runtime_shader_rewrite_source(storage, instruction, sources + 4,
                reinterpret_cast<const void*>(0x001A2814));
            if (result >= 0)
            {
                result = retain_runtime_shader_rewrite_source(storage, instruction, sources + 6,
                    reinterpret_cast<const void*>(0x001A2810));
            }
        }
    }
    return result;
}

// Xbox 0x00189858
i32 decode_runtime_shader_rewrite_sources(const u32* instruction, const u8* destinations, u8* sources)
{
    i32 result = 0;
    sources[6] = 0xF1;
    u8 third_mask = 0;
    u8 second_mask = 0;
    u8 first_mask = 0;
    switch ((instruction[2] >> 25) & 15u)
    {
    case 0:
        break;
    case 1:
        if (destinations[0] != 0)
        {
            first_mask = destinations[2];
        }
        first_mask |= destinations[4];
        break;
    case 2:
    case 9:
    case 10:
    case 11:
    case 12:
        if (destinations[0] != 0)
        {
            second_mask = destinations[2];
        }
        second_mask |= destinations[4];
        first_mask = second_mask;
        break;
    case 3:
        if (destinations[0] != 0)
        {
            third_mask = destinations[2];
        }
        third_mask |= destinations[4];
        first_mask = third_mask;
        break;
    case 4:
        if (destinations[0] != 0)
        {
            third_mask = destinations[2];
        }
        third_mask |= destinations[4];
        second_mask = third_mask;
        first_mask = third_mask;
        break;
    case 5:
        second_mask = 14;
        first_mask = 14;
        break;
    case 6:
        first_mask = 14;
        second_mask = 15;
        break;
    case 7:
        first_mask = 15;
        second_mask = 15;
        break;
    case 8:
        second_mask = 5;
        first_mask = 6;
        break;
    case 13:
        first_mask = 8;
        break;
    default:
        result = std::bit_cast<i32>(0x80004005u);
        break;
    }
    const u32 scalar_opcode = instruction[2] >> 29;
    if (scalar_opcode != 0)
    {
        if (scalar_opcode == 1)
        {
            u8 mask = 0;
            if (destinations[0] == 0)
            {
                mask = destinations[2];
            }
            third_mask = static_cast<u8>(third_mask | destinations[6] | mask);
        }
        else
        {
            if (scalar_opcode < 2)
            {
                return std::bit_cast<i32>(0x80004005u);
            }
            if (scalar_opcode < 7)
            {
                third_mask |= 8;
            }
            else
            {
                if (scalar_opcode != 7)
                {
                    return std::bit_cast<i32>(0x80004005u);
                }
                third_mask |= 13;
            }
        }
    }
    if (result >= 0)
    {
        u32 operand = instruction[2];
        result = static_cast<i32>(remap_runtime_shader_rewrite_mask(sources + 1, first_mask,
            (operand >> 10) & 3u, (operand >> 8) & 3u, (operand >> 6) & 3u, (operand >> 4) & 3u));
        if (result >= 0)
        {
            operand = instruction[1];
            result = static_cast<i32>(remap_runtime_shader_rewrite_mask(sources + 3, second_mask,
                (operand >> 25) & 3u, (operand >> 23) & 3u, (operand >> 21) & 3u, (operand >> 19) & 3u));
            if (result >= 0)
            {
                operand = instruction[1];
                result = static_cast<i32>(remap_runtime_shader_rewrite_mask(sources + 5, third_mask,
                    (operand >> 10) & 3u, (operand >> 8) & 3u, (operand >> 6) & 3u, (operand >> 4) & 3u));
                if (result >= 0)
                {
                    result = static_cast<i32>(decode_runtime_shader_rewrite_source(first_mask, sources,
                        sources, instruction, static_cast<u8>(instruction[2] & 15u),
                        static_cast<u16>((instruction[1] >> 28) & 3u)));
                    if (result >= 0)
                    {
                        result = static_cast<i32>(decode_runtime_shader_rewrite_source(second_mask, sources,
                            sources + 2, instruction, static_cast<u8>((instruction[1] >> 15) & 15u),
                            static_cast<u16>((instruction[1] >> 13) & 3u)));
                        if (result >= 0)
                        {
                            result = static_cast<i32>(decode_runtime_shader_rewrite_source(third_mask, sources,
                                sources + 4, instruction, static_cast<u8>(instruction[1] & 15u),
                                static_cast<u16>((instruction[0] >> 28) & 3u)));
                        }
                    }
                }
            }
        }
    }
    return result;
}

// Xbox 0x0018B299
i32 rewrite_runtime_shader_instruction(u8* storage, const u32* instruction, u32*& output)
{
    u32 rewritten[3] = {instruction[0], instruction[1], instruction[2]};
    u8 sources[8] = {};
    u8 destinations[7] = {};
    i32 result = static_cast<i32>(decode_runtime_shader_rewrite_destinations(rewritten, destinations));
    if (result >= 0)
    {
        result = filter_runtime_shader_rewrite_destinations(storage, rewritten, destinations);
        if (result >= 0)
        {
            result = static_cast<i32>(apply_runtime_shader_rewrite_destinations(rewritten, destinations));
            if (result >= 0)
            {
                result = static_cast<i32>(remove_runtime_shader_identity_writes(rewritten));
                if (result >= 0)
                {
                    if (!runtime_shader_instruction_outputs_are_redundant(rewritten)
                        && (destinations[4] != 0 || destinations[2] != 0 || destinations[6] != 0))
                    {
                        result = decode_runtime_shader_rewrite_sources(rewritten, destinations, sources);
                        if (result >= 0)
                        {
                            result = retain_runtime_shader_rewrite_destinations(storage, rewritten, destinations);
                            if (result >= 0)
                            {
                                result = retain_runtime_shader_rewrite_sources(storage, rewritten, sources);
                                if (result >= 0 && (rewritten[2] & 0xFE000000u) != 0)
                                {
                                    output[0] = rewritten[0];
                                    output[1] = rewritten[1];
                                    output[2] = rewritten[2];
                                    output += 3;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return result;
}

// Xbox 0x0018BB40
u32 remove_runtime_shader_dead_writes(u8* storage, u32 options, u32* program)
{
    u32 result = initialize_runtime_shader_rewrite_storage(storage, options);
    auto* allocation = static_cast<u32*>(std::malloc(0x1320));
    if (allocation == nullptr)
    {
        result = 0x8007000Eu;
    }
    else
    {
        const std::uintptr_t sentinel = reinterpret_cast<std::uintptr_t>(program) - 8;
        std::uintptr_t source = reinterpret_cast<std::uintptr_t>(program) + program[0] * 12u - 8;
        u32* output = allocation;
        while (source > sentinel)
        {
            if (std::bit_cast<i32>(result) < 0)
            {
                break;
            }
            result = static_cast<u32>(rewrite_runtime_shader_instruction(storage,
                reinterpret_cast<const u32*>(source), output));
            source -= 12;
        }
        if (std::bit_cast<i32>(result) >= 0)
        {
            const i32 count = static_cast<i32>(output - allocation) / 3;
            if (count != 0)
            {
                u32* destination = program + 1;
                std::uintptr_t input_address = reinterpret_cast<std::uintptr_t>(allocation) + count * 12u - 12;
                i32 remaining = count;
                do
                {
                    const u32* input = reinterpret_cast<const u32*>(input_address);
                    destination[0] = input[0];
                    destination[1] = input[1];
                    destination[2] = input[2];
                    input_address -= 12;
                    destination += 3;
                    --remaining;
                } while (remaining != 0);
            }
            program[0] = static_cast<u32>(count);
        }
        std::free(allocation);
    }
    return result;
}
}
