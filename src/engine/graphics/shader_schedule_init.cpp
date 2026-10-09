#include "shader_schedule_init.h"
#include <cstring>

namespace gbtg
{
namespace
{
void store_schedule_word(u8* address, u32 value)
{
    std::memcpy(address, &value, sizeof(value));
}
}

// Xbox 0x00185807
void initialize_runtime_shader_register_schedule(u8* registers)
{
    i32 remaining_registers = 13;
    u8* entry = registers + 0xD0;
    i32 remaining_channels = 4;
    do
    {
        do
        {
            store_schedule_word(entry - 0xD0, 0xFFFFFFFAu);
            store_schedule_word(entry, 4);
            entry += 4;
            --remaining_channels;
        } while (remaining_channels != 0);
        --remaining_registers;
        remaining_channels = 4;
    } while (remaining_registers != 0);
}

// Xbox 0x0018586A
void initialize_runtime_shader_issue_schedule(u8* issue, u32 option)
{
    // Retail floats DAT_0032B20C = 2.5 and DAT_0032B210 = 1.5.
    constexpr u32 initial_vector = 0x40200000u;
    constexpr u32 initial_scalar = 0x3FC00000u;
    i32 remaining_registers = 13;
    u8* entry = issue + 0xE0;
    do
    {
        i32 remaining_channels = 4;
        do
        {
            store_schedule_word(entry + 0x2A0, 0x41);
            store_schedule_word(entry - 0xE0, initial_vector);
            store_schedule_word(entry + 0x380, 0x41);
            store_schedule_word(entry + 0x460, 0x41);
            store_schedule_word(entry, initial_vector);
            store_schedule_word(entry + 0x540, 0x41);
            entry += 4;
            --remaining_channels;
            store_schedule_word(entry + 0xDC, initial_scalar);
            store_schedule_word(entry + 0x1BC, initial_scalar);
        } while (remaining_channels != 0);
        --remaining_registers;
    } while (remaining_registers != 0);
    store_schedule_word(issue + 0x450, 0x20);
    store_schedule_word(issue + 0xD0, 0);
    store_schedule_word(issue + 0x700, option);
}

// Xbox 0x00189462
u32 initialize_runtime_shader_schedule(u8* state, u32 option)
{
    store_schedule_word(state + 0x18, 0xFFFFFFFAu);
    store_schedule_word(state + 0x1C, 0xFFFFFFFAu);
    store_schedule_word(state, 0);
    store_schedule_word(state + 4, 0);
    store_schedule_word(state + 8, 0);
    store_schedule_word(state + 0xC, 0);
    initialize_runtime_shader_register_schedule(state + 0x20);
    initialize_runtime_shader_issue_schedule(state + 0x1C0, option);
    store_schedule_word(state + 0x14, 0);
    return 0;
}

// Xbox 0x0018B1D3
u32 configure_runtime_shader_schedule(u8* state, u8 mode, u32 option)
{
    state[0x10] = static_cast<u8>(mode == 1);
    return initialize_runtime_shader_schedule(state, option);
}
}
