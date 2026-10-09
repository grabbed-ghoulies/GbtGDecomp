#include "shader_schedule_update.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void store_issue_word(u8* table, u32 offset, u32 value)
{
    std::memcpy(table + offset, &value, sizeof(value));
}

u32 load_issue_word(const u8* table, u32 offset)
{
    u32 value;
    std::memcpy(&value, table + offset, sizeof(value));
    return value;
}

i32 issue_option(const u8* table)
{
    return std::bit_cast<i32>(load_issue_word(table, 0x700));
}

void clear_issue_readiness(u8* table, u32 entry)
{
    store_issue_word(table, entry + 0x1C0, 0);
    store_issue_word(table, entry + 0x540, 0x20);
    store_issue_word(table, entry + 0x2A0, 0);
    store_issue_word(table, entry + 0x620, 0x20);
}
}

// Xbox 0x001856D5
bool runtime_shader_issue_value_near(float value, float reference, float margin)
{
    const long double difference = static_cast<long double>(value) - static_cast<long double>(reference);
    if (difference < static_cast<long double>(margin))
    {
        if (difference > -static_cast<long double>(margin))
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x00185961
void update_runtime_shader_issue_schedule(u8* table, u32 latency, u8 reg, u8 mask,
    float issued, i32 reason)
{
    constexpr float margin = std::bit_cast<float>(0x3E99999Au);
    if (reg == 13)
    {
        u32 duration = 0x3E2E147Bu;
        if ((reason == 0x43 || reason == 0x45 || reason == 0x47)
            && issue_option(table) > 10 && 2.0f < issued)
        {
            duration = 0x3EA8F5C3u;
        }
        store_issue_word(table, 0xD0, duration);
        store_issue_word(table, 0x1B0, duration);
        store_issue_word(table, 0x290, 0);
        store_issue_word(table, 0x450, 0x48);
        store_issue_word(table, 0x370, 0);
        store_issue_word(table, 0x530, 0x48);
        store_issue_word(table, 0x610, 0x20);
        store_issue_word(table, 0x6F0, 0x20);
    }
    else
    {
        i32 channel = 0;
        do
        {
            if ((static_cast<u32>(mask) & (1u << (static_cast<u8>(channel) & 31u))) != 0)
            {
                const u32 entry = (static_cast<u32>(channel) + static_cast<u32>(reg) * 4u) * 4u;
                switch (reason)
                {
                case 0x20:
                case 0x44:
                case 0x48:
                case 0x49:
                    clear_issue_readiness(table, entry);
                    if (latency == 1)
                    {
                        if (load_issue_word(table, entry + 0x380) != 0x41 && reason != 0x41)
                        {
                            store_issue_word(table, entry, 0);
                            store_issue_word(table, entry + 0xE0, 0);
                            store_issue_word(table, entry + 0x380, 0x42);
                            store_issue_word(table, entry + 0x460, 0x42);
                        }
                    }
                    else if (load_issue_word(table, entry + 0x380) != 0x41 && reason != 0x41)
                    {
                        store_issue_word(table, entry, 0x3F000000u);
                        store_issue_word(table, entry + 0x380, 0x43);
                        store_issue_word(table, entry + 0xE0, 0);
                        store_issue_word(table, entry + 0x460, 0x42);
                    }
                    break;
                case 0x41:
                    clear_issue_readiness(table, entry);
                    store_issue_word(table, entry, 0);
                    store_issue_word(table, entry + 0x380, 0x42);
                    store_issue_word(table, entry + 0xE0, 0);
                    store_issue_word(table, entry + 0x460, 0x42);
                    break;
                case 0x42:
                case 0x46:
                    clear_issue_readiness(table, entry);
                    if (latency == 1)
                    {
                        if (load_issue_word(table, entry + 0x380) != 0x41)
                        {
                            store_issue_word(table, entry, 0);
                            store_issue_word(table, entry + 0x380, 0x42);
                            store_issue_word(table, entry + 0xE0, 0);
                            store_issue_word(table, entry + 0x460, 0x42);
                        }
                    }
                    else if (load_issue_word(table, entry + 0x380) != 0x41)
                    {
                        u32 duration;
                        if (runtime_shader_issue_value_near(issued, 1.0f, margin))
                        {
                            duration = 0x3F19999Au;
                        }
                        else if (runtime_shader_issue_value_near(issued, 3.0f, margin)
                            || runtime_shader_issue_value_near(issued, 3.5f, margin))
                        {
                            duration = 0x3F3851ECu;
                        }
                        else
                        {
                            duration = 0x3F547AE1u;
                        }
                        store_issue_word(table, entry, duration);
                        store_issue_word(table, entry + 0x380, 0x47);
                        store_issue_word(table, entry + 0xE0, 0);
                        store_issue_word(table, entry + 0x460, 0x42);
                    }
                    break;
                case 0x43:
                case 0x45:
                case 0x47:
                {
                    u32 readiness = 0x3ECCCCCDu;
                    if (issue_option(table) < 10 || issued < 4.0f)
                    {
                        readiness = 0x3E4CCCCDu;
                    }
                    store_issue_word(table, entry + 0x1C0, readiness);
                    store_issue_word(table, entry + 0x540, 0x44);
                    store_issue_word(table, entry + 0x2A0, readiness);
                    store_issue_word(table, entry + 0x620, 0x44);
                    if (latency == 1)
                    {
                        if (load_issue_word(table, entry + 0x380) != 0x41)
                        {
                            u32 duration = 0x3E4CCCCDu;
                            if (!runtime_shader_issue_value_near(issued, 2.0f, margin)
                                && !runtime_shader_issue_value_near(issued, 3.0f, margin)
                                && !runtime_shader_issue_value_near(issued, 4.0f, margin)
                                && (issue_option(table) < 5 || issue_option(table) > 9))
                            {
                                duration = 0x3F000000u;
                            }
                            store_issue_word(table, entry, duration);
                            store_issue_word(table, entry + 0x380, 0x46);
                            store_issue_word(table, entry + 0xE0, duration);
                            store_issue_word(table, entry + 0x460, 0x46);
                        }
                    }
                    else if (load_issue_word(table, entry + 0x380) != 0x41)
                    {
                        const bool near_two = runtime_shader_issue_value_near(issued, 2.0f, margin);
                        u32 duration = 0x3E4CCCCDu;
                        if (!near_two && !runtime_shader_issue_value_near(issued, 3.0f, margin)
                            && !runtime_shader_issue_value_near(issued, 4.0f, margin)
                            && (issue_option(table) < 5 || issue_option(table) > 9))
                        {
                            duration = 0x3F000000u;
                        }
                        store_issue_word(table, entry + 0xE0, duration);
                        store_issue_word(table, entry + 0x460, 0x46);
                        if (issue_option(table) < 10)
                        {
                            store_issue_word(table, entry, 0x3F000000u);
                            store_issue_word(table, entry + 0x380, 0x43);
                        }
                        else
                        {
                            duration = 0x3FA66666u;
                            if (!near_two && !runtime_shader_issue_value_near(issued, 3.0f, margin))
                            {
                                duration = 0x3F866666u;
                            }
                            store_issue_word(table, entry, duration);
                            store_issue_word(table, entry + 0x380, 0x45);
                        }
                    }
                    break;
                }
                }
            }
            ++channel;
        } while (channel < 4);
    }
}
}
