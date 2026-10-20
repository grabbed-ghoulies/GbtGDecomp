#include "script_event_tallies.h"

namespace gbtg
{
RuntimeScriptTallies g_script_tallies;
std::function<RuntimeScriptKillCounters()> g_script_kill_counters;
std::function<std::span<const u8>(u32 address)> g_script_actor_bytes;
std::function<const u8*(u16 door, std::span<u32, 0x42> pose)> g_script_door_pose;

namespace
{
u32& tally_word(std::span<u32> table, size_t index)
{
    if (index >= table.size())
    {
        throw Error("script tally index lies outside the 7-kind table");
    }
    return table[index];
}

// The actor's +6 tag, or 1 for a null actor (the filter's EBX).
u16 actor_tag_or_one(u32 actor)
{
    if (actor == 0)
    {
        return 1;
    }
    if (!g_script_actor_bytes)
    {
        throw Error("script tally has no scene actor view");
    }
    return read_u16(g_script_actor_bytes(actor), 6);
}
}

// Xbox 0x0012AF80
void tally_runtime_script_pair(u32 kind, std::span<u32> table, u16 second, u16 first)
{
    const size_t row = size_t(kind) * 0x3D + second;
    ++tally_word(table, row * 0x3D);
    ++tally_word(table, size_t(kind) * 0xE89);
    if (first != 0)
    {
        ++tally_word(table, row * 0x3D + first);
        ++tally_word(table, size_t(kind) * 0xE89 + first);
    }
}

// Xbox 0x0012ADC0, ECX kind, DX second
void tally_runtime_script_kind(u32 kind, u16 second)
{
    const size_t row = (size_t(kind) * 0x3D + second) * 0x3D;
    ++tally_word(g_script_tallies.first, row);
    ++tally_word(g_script_tallies.first, size_t(kind) * 0xE89);
    ++tally_word(g_script_tallies.second, row);
    ++tally_word(g_script_tallies.second, size_t(kind) * 0xE89);
}

// Xbox 0x0012ACF0
void tally_runtime_script_event(u16 first, u16 second, u32 kind)
{
    tally_runtime_script_pair(kind, g_script_tallies.first, second, first);
    tally_runtime_script_pair(kind, g_script_tallies.second, second, first);
    if (kind != 0)
    {
        return;
    }
    if (!g_script_kill_counters)
    {
        throw Error("script kill counters are not bound");
    }
    const RuntimeScriptKillCounters counters = g_script_kill_counters();
    switch (first)
    {
    case 4:
    case 9:
    case 0xE:
        ++*counters.first_tag_kills;
        ++*counters.first_tag_total;
        return;
    default:
        break;
    }
    switch (second)
    {
    case 1:
    case 4:
    case 9:
    case 0xE:
    case 0x20:
        ++*counters.second_tag_kills;
        return;
    default:
        return;
    }
}

// Xbox 0x00116C10
void tally_runtime_script_hit(u32 source, u32 target, u32 unblocked, u32 third)
{
    const u16 second = actor_tag_or_one(target);
    const u16 first = read_u16(g_script_actor_bytes(source), 6);
    tally_runtime_script_event(first, second, unblocked == 0 ? 2 : 1);
    tally_runtime_script_event(first, second, third != 0 ? 3 : 4);
}
}
