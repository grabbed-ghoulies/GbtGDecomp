#pragma once
#include "gbtg/retail_field.h"

#include <cstddef>
#include <span>
#include <type_traits>

namespace gbtg
{
// The start of every body state (the states a body runs, such as the hit, walk-target and throw states): type 9's
// 0x18-byte instance (0x00095530) under the factory's header (0x00107520, 0x00107700). See docs/RETAIL_LAYOUTS.md.
struct BodyStateBase
{
    u32 vtable = 0; // +0, the type's vtable address (0x00107520)
    u16 type = 0; // +4, the factory type
    u16 unk_6 = 0; // +6, the parameters' +2 (0x00107700)
    u32 body = 0; // +8, the state's body, by address: the parameters' +8 (0x0002B600)
    // +0xC: the key the state hands its body next. The states' vtable +0x1C (0x00091F70 among them) returns it, and the
    // states store their parameters' keys into it when they end.
    u16 next_key = 0;
    u16 unk_E = 0; // +0xE, copied with next_key as one word (the hold state 0x000A3520); nothing reads it alone
    float elapsed = 0.f; // +0x10, seconds the state has run: each tick adds the actor's step (0x000A6520 among them)
    u32 unk_14 = 0; // +0x14, never read or written by translated code
};
static_assert(sizeof(BodyStateBase) == 0x18);

// A factory state's bytes viewed as its struct.
template <typename State>
State& body_state_view(std::span<u8> bytes)
{
    static_assert(std::is_trivially_copyable_v<State>);
    detail::check_retail_range(bytes.size(), 0, sizeof(State));
    return *reinterpret_cast<State*>(bytes.data());
}

// A state's address, the key of the host's side tables (the records and overrides retail keeps as pointers).
template <typename State>
const u8* body_state_key(const State& state)
{
    return reinterpret_cast<const u8*>(&state);
}
} // namespace gbtg
