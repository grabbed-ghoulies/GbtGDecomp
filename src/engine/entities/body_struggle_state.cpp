#include "body_struggle_state.h"
#include "body_orientation.h"
#include "entity.h"
#include "gbtg/anim.h"

#include <bit>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
// The +0x24 / +0x28 animation records each struggle state found; the state words only say whether one was found.
struct StruggleRecords
{
    const AnimationRecord* in_band = nullptr; // +0x24
    const AnimationRecord* out_of_band = nullptr; // +0x28
};
std::unordered_map<const u8*, StruggleRecords> struggle_records;

u32 word(std::span<const u8> bytes, size_t offset)
{
    return read_u32(bytes, offset);
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

const StruggleRecords& records_of(std::span<const u8> state)
{
    const auto found = struggle_records.find(state.data());
    if (found == struggle_records.end())
    {
        throw Error("struggle state has no animation records");
    }
    return found->second;
}

// 0x0001AFA0 with ECX the record; retail reads the record's fields, so a missing one is not played.
void start_record(const AnimationRecord* record, RuntimeBodyActionSelection& actions)
{
    if (record == nullptr)
    {
        throw Error("struggle animation record is null; 0x0001AFA0 reads it");
    }
    start_runtime_body_animation_record(*record, actions);
}

// A parameter float, or the default when it is zero.
float parameter_or(std::span<const u8> parameters, size_t offset, float scale, float fallback)
{
    const float value = read_f32(parameters, offset);
    return value == 0.f ? fallback : value * scale;
}
}

// Xbox 0x000A2B10
void struggle_state_object_sizes(ObjectSizes& output)
{
    output.instance = 0x4C;
    output.parameters = 0x34;
}

// Xbox 0x000A2B90
bool initialize_runtime_struggle_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context)
{
    const auto body = context.body;
    std::memcpy(state.data() + 0x18, parameters.data() + 0xC, 2);
    std::memcpy(state.data() + 0x1A, parameters.data() + 0xE, 2);
    put_word(state, 0x1C, word(parameters, 0x10));
    put_word(state, 0x20, word(parameters, 0x14));
    auto& records = struggle_records[state.data()];
    // The inlined 0x0003AAA0 pairs: the attached actor's +0xDB0 variant first, then the plain name.
    const auto name_at = [&context](u32 address) -> const char*
    {
        return address == 0 ? nullptr : reinterpret_cast<const char*>(context.resolve(address, 0).data());
    };
    records.in_band = find_runtime_body_animation(name_at(word(parameters, 0x18)), context.actions);
    put_word(state, 0x24, records.in_band != nullptr ? 1 : 0);
    records.out_of_band = find_runtime_body_animation(name_at(word(parameters, 0x1C)), context.actions);
    put_word(state, 0x28, records.out_of_band != nullptr ? 1 : 0);
    const float degrees = std::bit_cast<float>(0x3C8EFA35u); // DAT_0032B0FC.
    put_float(state, 0x2C, 0.f);
    put_word(state, 0x38, word(parameters, 0x20));
    put_float(state, 0x3C, read_f32(parameters, 0x24) * degrees);
    put_float(state, 0x40, parameter_or(parameters, 0x28, degrees, std::bit_cast<float>(0x3F860A92u))); // DAT_0032B484.
    put_float(state, 0x44, parameter_or(parameters, 0x2C, degrees, std::bit_cast<float>(0x4011361Eu))); // DAT_0032B63C.
    put_float(state, 0x48, parameter_or(parameters, 0x30, 1.f, 6.f)); // DAT_0032B218.
    context.bits->bytes[6] |= 1;
    std::memcpy(body.data() + 0x71C, state.data() + 0x40, 4);
    std::memcpy(body.data() + 0x720, state.data() + 0x44, 4);
    std::memcpy(body.data() + 0x724, state.data() + 0x48, 4);
    body[0x38] = 1;
    body[0x39] = 1;
    std::memset(body.data() + 0x160, 0, 2);
    put_word(state, 0x34, 0);
    if (word(state, 0x28) != 0 || word(state, 0x30) == 0)
    {
        start_record(records.in_band, context.actions);
        put_word(state, 0x30, 1);
    }
    put_word(state, 0x34, 1);
    return true;
}

// Xbox 0x000A2D60
void destroy_runtime_struggle_state(std::span<u8> state, RuntimeBodyStateContext& context)
{
    const auto body = context.body;
    put_float(body, 0x71C, std::bit_cast<float>(0x3F0EFA35u)); // DAT_0032B220.
    put_float(body, 0x720, std::bit_cast<float>(0x41490FDBu)); // DAT_0032B21C.
    put_float(body, 0x724, 6.f); // DAT_0032B218.
    context.bits->bytes[6] &= 0xFE;
    struggle_records.erase(state.data());
}

// Xbox 0x000A2F90
void turn_runtime_struggle_body(bool targeted, std::span<u8> state, RuntimeStruggleTick& context)
{
    const auto body = context.body;
    if (targeted)
    {
        const float facing = read_f32(context.actor_bytes, 0x18);
        const float heading = runtime_body_point_heading(body, context.actor) + read_f32(state, 0x3C);
        const float pi = std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
        const float turn = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
        const long double remainder = animation_remainder(heading - facing, turn);
        float difference = float(remainder);
        if (remainder < static_cast<long double>(std::bit_cast<float>(0xC0490FDBu))) // DAT_0032B29C.
        {
            difference = difference + turn;
        }
        if (!(difference < pi))
        {
            difference = difference - turn;
        }
        const long double limit = static_cast<long double>(read_f32(state, 0x38))
            * static_cast<long double>(std::bit_cast<float>(0x3C8EFA35u)); // DAT_0032B0FC.
        if (static_cast<long double>(engine_absolute(difference)) > limit)
        {
            put_float(body, 0x734, 0.f);
            put_float(body, 0x72C, 0.f);
            put_float(body, 0x730, heading);
            return;
        }
    }
    put_float(body, 0x734, 0.f);
    put_float(body, 0x72C, 0.f);
}

// Xbox 0x000A3070
void update_runtime_struggle_animation(std::span<u8> state, RuntimeStruggleTick& context)
{
    const float facing = read_f32(context.actor_bytes, 0x18);
    const float heading = read_f32(context.body, 0x730);
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(context.actor);
    const float timer = read_f32(state, 0x2C);
    if (0.f < timer)
    {
        const float left = timer - rate * step;
        put_float(state, 0x2C, left);
        if (!(0.f < left))
        {
            put_float(state, 0x2C, 0.f);
        }
    }
    const float band = std::bit_cast<float>(0x3DB2B8C2u); // DAT_0032B660, 5 degrees.
    const bool inside = !(facing < heading - band) && !(heading + band < facing);
    const StruggleRecords& records = records_of(state);
    const u32 mode = word(state, 0x34);
    if (mode == 1)
    {
        if (!inside)
        {
            put_float(state, 0x2C, std::bit_cast<float>(0x3ECCCCCDu)); // DAT_0032B11C, 0.4 s.
            if (word(state, 0x28) != 0)
            {
                start_record(records.out_of_band, context.actions);
            }
            put_word(state, 0x34, 2);
        }
        return;
    }
    if (mode == 2 && inside && read_f32(state, 0x2C) == 0.f)
    {
        if (word(state, 0x28) != 0 || word(state, 0x30) == 0)
        {
            start_record(records.in_band, context.actions);
            put_word(state, 0x30, 1);
        }
        put_word(state, 0x34, 1);
    }
}

// Xbox 0x000A2EB0
void select_runtime_struggle_key(std::span<u8> state, RuntimeStruggleTick& context)
{
    const auto body = context.body;
    if (word(state, 0x1C) != 0)
    {
        const float z = read_f32(body, 0x250);
        const float y = read_f32(body, 0x24C);
        const float x = read_f32(body, 0x248);
        u16 key = 0;
        if ((z * z + y * y) + x * x > 0.f && (context.flags.bytes[0] & 2) != 0) // DAT_0032B100.
        {
            key = context.moving_action();
        }
        std::memcpy(state.data() + 0xC, &key, 2);
    }
    if (context.mind.empty() || context.mind[0x20] == 0)
    {
        const u16 idle = read_u16(state, 0x1A);
        if (idle != 0)
        {
            std::memcpy(state.data() + 0xC, &idle, 2);
        }
    }
    const u16 input_key = read_u16(state, 0x18);
    if (input_key != 0)
    {
        std::array<float, 2> axes;
        runtime_input_axes(context.input, axes, context.input_state);
        if (axes[1] * axes[1] + axes[0] * axes[0] > std::bit_cast<float>(0x3C23D70Bu)) // DAT_0032B22C.
        {
            std::memcpy(state.data() + 0xC, &input_key, 2);
        }
    }
}

// Xbox 0x000A2DB0
void tick_runtime_struggle_state(std::span<u8> state, RuntimeStruggleTick& context)
{
    const auto body = context.body;
    const float elapsed = runtime_actor_rate(context.actor) * runtime_frame_step(context.clock) + read_f32(state, 0x10);
    put_float(state, 0x10, elapsed);
    bool targeted = false;
    if (!context.mind.empty() && context.mind[0x20] != 0 && word(context.mind, 0xC) != 0)
    {
        const Vec3 point = context.target_position(word(context.mind, 0xC));
        std::memcpy(body.data() + 0x3C, &point, 12);
        targeted = true;
    }
    if (read_u16(body, 0x30) == 0 && read_f32(body, 0x34) == 0.f && (context.flags.bytes[0] & 0x40) == 0)
    {
        turn_runtime_struggle_body(targeted, state, context);
    }
    if (word(state, 0x20) != 0)
    {
        settle_runtime_body_turn(body, context.actor);
    }
    update_runtime_struggle_animation(state, context);
    select_runtime_struggle_key(state, context);
}
}
