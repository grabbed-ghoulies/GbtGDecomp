#pragma once
#include "gbtg/retail_field.h"

#include <cstddef>
#include <span>
#include <type_traits>

namespace gbtg
{
// The fields every body state (the states a body runs, such as the hit, walk-target and throw states) keeps at the
// start of its object, at their retail offsets (see docs/RETAIL_LAYOUTS.md). +8 is the state's body, a native view.
namespace body_state_field
{
// The key the state hands its body next: the states' vtable +0x1C (0x00091F70 among them) returns it, and the
// states store their parameters' keys into it when they end.
constexpr RetailField<u16, 0x0C> next_key{};
// Copied with next_key as one word (the hold state 0x000A3520); nothing reads it alone.
constexpr RetailField<u16, 0x0E> unk_E{};
// Seconds the state has run: each state's tick adds the actor's step (0x000A6520 among them).
constexpr RetailField<float, 0x10> elapsed{};
} // namespace body_state_field

// The same base as a struct, for the states consolidated into structs: type 9's 0x18-byte instance (0x00095530)
// under the factory's header (0x00107520, 0x00107700).
struct BodyStateBase
{
    u32 vtable = 0; // +0, the type's vtable address (0x00107520)
    u16 type = 0; // +4, the factory type
    u16 unk_6 = 0; // +6, the parameters' +2 (0x00107700)
    u32 body = 0; // +8, the state's body, by address: the parameters' +8 (0x0002B600)
    u16 next_key = 0; // +0xC
    u16 unk_E = 0; // +0xE
    float elapsed = 0.f; // +0x10
    u32 unk_14 = 0; // +0x14, never read or written by translated code
};
static_assert(sizeof(BodyStateBase) == 0x18);
static_assert(offsetof(BodyStateBase, next_key) == body_state_field::next_key.offset);
static_assert(offsetof(BodyStateBase, unk_E) == body_state_field::unk_E.offset);
static_assert(offsetof(BodyStateBase, elapsed) == body_state_field::elapsed.offset);

// A factory state's bytes viewed as its struct.
template <typename State>
State& body_state_view(std::span<u8> bytes)
{
    static_assert(std::is_trivially_copyable_v<State>);
    detail::check_retail_range(bytes.size(), 0, sizeof(State));
    return *reinterpret_cast<State*>(bytes.data());
}
} // namespace gbtg
