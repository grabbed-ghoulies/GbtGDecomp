#include "cinema_mind_control.h"
#include "player_fields.h"
#include "mind_fields.h"
#include "body_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void value(std::span<u8> bytes, size_t offset, float number)
{
    std::memcpy(bytes.data() + offset, &number, 4);
}
}

// Xbox 0x0007C4C0
void refresh_runtime_cinema_mind_action_permission(std::span<u8> mind,
    std::span<const u8> actor, i32 allowed_count, std::span<const u8> allowed_rows)
{
    if (player_record(actor).unk_CA0 == 0)
    {
        i32 index = 0;
        if (allowed_count > 0)
        {
            do
            {
                if (read_u16(actor, 6) == read_u16(allowed_rows, size_t(index) * 4))
                {
                    return;
                }
                ++index;
            }
            while (index < allowed_count);
        }
        mind_record(mind).target_range = 0.f;
    }
}

// Xbox 0x0007CCD0
void update_runtime_cinema_mind_sound(std::span<u8> mind,
    std::span<const u8> body, std::span<const u8> resource, Actor& actor,
    const RuntimeActorClock& clock, u32& random, RuntimeActorSoundQueues& sounds)
{
    const i32 count = i32(cinema_mind_record(mind).sound_count);
    if (count != 0)
    {
        if (body_record(body).move_holds == 0 && body_record(body).move_hold_time == 0.f)
        {
            const float step = runtime_frame_step(clock);
            const float rate = runtime_actor_rate(actor);
            const float timer = cinema_mind_record(mind).sound_timer;
            if (timer > 0.f)
            {
                const float remaining = timer - rate * step;
                cinema_mind_record(mind).sound_timer = remaining;
                // COMISS zero,remaining / JC: unordered also exits.
                if (remaining <= 0.f)
                {
                    cinema_mind_record(mind).sound_timer = 0.f;
                    random = random * 0x19660Du + 0x3C6EF35Fu;
                    const float fraction = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
                    const i32 selected = i32(fraction * float(count));
                    request_actor_sound(actor, 1,
                        reinterpret_cast<const char*>(resource.data() + 0x10 + selected * 0x80), sounds);
                    const float minimum = read_f32(resource, 8);
                    const float maximum = read_f32(resource, 0xC);
                    random = random * 0x19660Du + 0x3C6EF35Fu;
                    const float delay = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
                    cinema_mind_record(mind).sound_timer = (maximum - minimum) * delay + minimum;
                }
            }
        }
    }
}
}
