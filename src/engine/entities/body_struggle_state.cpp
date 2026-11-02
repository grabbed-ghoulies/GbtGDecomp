#include "body_struggle_state.h"
#include "mind_fields.h"
#include "body_orientation.h"
#include "entity.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include "body_state_fields.h"

#include <bit>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace field = struggle_state_field;
namespace parameter = struggle_state_parameter;
namespace base = body_state_field;

namespace
{
// The animation records each struggle state found; the state's found flags only say whether one was found.
struct StruggleRecords
{
    const AnimationRecord* in_band = nullptr;
    const AnimationRecord* out_of_band = nullptr;
};
std::unordered_map<const u8*, StruggleRecords> struggle_records;

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
float parameter_or(float value, float scale, float fallback)
{
    return value == 0.f ? fallback : value * scale;
}
}

// Xbox 0x000A2B10
void struggle_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x000A2B90
bool initialize_runtime_struggle_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context)
{
    const auto body = context.body;
    set(state, field::input_key, get(parameters, parameter::input_key));
    set(state, field::idle_key, get(parameters, parameter::idle_key));
    set(state, field::moving_key, get(parameters, parameter::moving_key));
    set(state, field::settles, get(parameters, parameter::settles));
    auto& records = struggle_records[state.data()];
    // The inlined 0x0003AAA0 pairs: the attached actor's +0xDB0 variant first, then the plain name.
    const auto name_at = [&context](u32 address) -> const char*
    {
        return address == 0 ? nullptr : reinterpret_cast<const char*>(context.resolve(address, 0).data());
    };
    records.in_band = find_runtime_body_animation(name_at(get(parameters, parameter::in_band)), context.actions);
    set(state, field::in_band_found, records.in_band != nullptr ? 1 : 0);
    records.out_of_band =
        find_runtime_body_animation(name_at(get(parameters, parameter::out_of_band)), context.actions);
    set(state, field::out_of_band_found, records.out_of_band != nullptr ? 1 : 0);
    const float degrees = std::bit_cast<float>(0x3C8EFA35u); // DAT_0032B0FC.
    set(state, field::band_timer, 0.f);
    set(state, field::turn_limit, get(parameters, parameter::turn_limit));
    set(state, field::heading_offset, get(parameters, parameter::heading_offset) * degrees);
    const auto rates = get(parameters, parameter::turn_rates);
    set(state, field::turn_rates,
        {parameter_or(rates[0], degrees, std::bit_cast<float>(0x3F860A92u)), // DAT_0032B484.
            parameter_or(rates[1], degrees, std::bit_cast<float>(0x4011361Eu)), // DAT_0032B63C.
            parameter_or(rates[2], 1.f, 6.f)}); // DAT_0032B218.
    context.bits->bytes[6] |= 1;
    set(body, body_field::turn_rates, get(state, field::turn_rates));
    body[0x38] = 1;
    body[0x39] = 1;
    std::memset(body.data() + 0x160, 0, 2);
    set(state, field::band_mode, 0);
    if (get(state, field::out_of_band_found) != 0 || get(state, field::started) == 0)
    {
        start_record(records.in_band, context.actions);
        set(state, field::started, 1);
    }
    set(state, field::band_mode, 1);
    return true;
}

// Xbox 0x000A2D60
void destroy_runtime_struggle_state(std::span<u8> state, RuntimeBodyStateContext& context)
{
    const auto body = context.body;
    // DAT_0032B220, DAT_0032B21C, DAT_0032B218.
    set(body, body_field::turn_rates, {std::bit_cast<float>(0x3F0EFA35u), std::bit_cast<float>(0x41490FDBu), 6.f});
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
        const float heading = runtime_body_point_heading(body, context.actor) + get(state, field::heading_offset);
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
        const long double limit = static_cast<long double>(get(state, field::turn_limit))
            * static_cast<long double>(std::bit_cast<float>(0x3C8EFA35u)); // DAT_0032B0FC.
        if (static_cast<long double>(engine_absolute(difference)) > limit)
        {
            set(body, body_field::euler, Vec3{0.f, heading, 0.f});
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
    const float heading = get(context.body, body_field::euler).y;
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(context.actor);
    const float timer = get(state, field::band_timer);
    if (0.f < timer)
    {
        const float left = timer - rate * step;
        set(state, field::band_timer, left);
        if (!(0.f < left))
        {
            set(state, field::band_timer, 0.f);
        }
    }
    const float band = std::bit_cast<float>(0x3DB2B8C2u); // DAT_0032B660, 5 degrees.
    const bool inside = !(facing < heading - band) && !(heading + band < facing);
    const StruggleRecords& records = records_of(state);
    const u32 mode = get(state, field::band_mode);
    if (mode == 1)
    {
        if (!inside)
        {
            set(state, field::band_timer, std::bit_cast<float>(0x3ECCCCCDu)); // DAT_0032B11C, 0.4 s.
            if (get(state, field::out_of_band_found) != 0)
            {
                start_record(records.out_of_band, context.actions);
            }
            set(state, field::band_mode, 2);
        }
        return;
    }
    if (mode == 2 && inside && get(state, field::band_timer) == 0.f)
    {
        if (get(state, field::out_of_band_found) != 0 || get(state, field::started) == 0)
        {
            start_record(records.in_band, context.actions);
            set(state, field::started, 1);
        }
        set(state, field::band_mode, 1);
    }
}

// Xbox 0x000A2EB0
void select_runtime_struggle_key(std::span<u8> state, RuntimeStruggleTick& context)
{
    const auto body = context.body;
    if (get(state, field::moving_key) != 0)
    {
        const float z = read_f32(body, 0x250);
        const float y = read_f32(body, 0x24C);
        const float x = read_f32(body, 0x248);
        u16 key = 0;
        if ((z * z + y * y) + x * x > 0.f && (context.flags.bytes[0] & 2) != 0) // DAT_0032B100.
        {
            key = context.moving_action();
        }
        set(state, base::next_key, key);
    }
    if (context.mind.empty() || get(context.mind, mind_field::target_mode) == 0)
    {
        const u16 idle = get(state, field::idle_key);
        if (idle != 0)
        {
            set(state, base::next_key, idle);
        }
    }
    const u16 input_key = get(state, field::input_key);
    if (input_key != 0)
    {
        std::array<float, 2> axes;
        runtime_input_axes(context.input, axes, context.input_state);
        if (axes[1] * axes[1] + axes[0] * axes[0] > std::bit_cast<float>(0x3C23D70Bu)) // DAT_0032B22C.
        {
            set(state, base::next_key, input_key);
        }
    }
}

// Xbox 0x000A2DB0
void tick_runtime_struggle_state(std::span<u8> state, RuntimeStruggleTick& context)
{
    const auto body = context.body;
    const float elapsed =
        runtime_actor_rate(context.actor) * runtime_frame_step(context.clock) + get(state, base::elapsed);
    set(state, base::elapsed, elapsed);
    bool targeted = false;
    if (!context.mind.empty() && get(context.mind, mind_field::target_mode) != 0
        && get(context.mind, mind_field::target) != 0)
    {
        const Vec3 point = context.target_position(get(context.mind, mind_field::target));
        std::memcpy(body.data() + 0x3C, &point, 12);
        targeted = true;
    }
    if (get(body, body_field::move_holds) == 0 && get(body, body_field::move_hold_time) == 0.f &&
        (context.flags.bytes[0] & 0x40) == 0)
    {
        turn_runtime_struggle_body(targeted, state, context);
    }
    if (get(state, field::settles) != 0)
    {
        settle_runtime_body_turn(body, context.actor);
    }
    update_runtime_struggle_animation(state, context);
    select_runtime_struggle_key(state, context);
}
}
