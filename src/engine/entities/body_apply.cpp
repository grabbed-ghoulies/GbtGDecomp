#include "body_apply.h"
#include "body_fields.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "../runtime/rare_replay.h"
#endif
#include <cstring>

namespace gbtg
{
namespace
{
void apply_store_float(std::span<u8> body, size_t offset, float value)
{
    std::memcpy(body.data() + offset, &value, 4);
}
}

// Xbox 0x00025AE0
void apply_runtime_body(std::span<u8> body, RuntimeBodyApplyContext& context)
{
    auto& channels = context.channels;
    if (read_u32(body, 0xFC) != 0)
    {
        apply_runtime_body_default_blend(body, channels);
    }
    if (i32(read_u32(body, 0x154)) > 0)
    {
        apply_runtime_body_channels(body, channels);
    }
    update_runtime_body_model_anchors(body, context.anchors);
    apply_runtime_body_actor_bounds(body, channels.actor, context.actor_bytes,
        context.flags, context.bound, context.shapes);
    u16 frames = get(body, body_field::surface_override_frames);
#if !defined(GBTG_RETAIL_GAME_ONLY)
    // Rare Replay: +0x880 counts retail ticks.
    if (frames != 0 && rare_replay_retail_tick())
#else
    if (frames != 0)
#endif
    {
        --frames;
        std::memcpy(body.data() + 0x880, &frames, 2);
        if (frames == 0)
        {
            body[0x87E] = 0;
        }
    }
    tick_runtime_body_pulse(body, channels.actor, context.first, channels.clock);
    if (runtime_decay_enabled(channels.clock))
    {
        const float step = runtime_frame_step(channels.clock);
        const float rate = runtime_actor_rate(channels.actor);
        if (get(body, body_field::unk_70) > 0.f)
        {
            const float timer = get(body, body_field::unk_70) - rate * step;
            apply_store_float(body, 0x70, timer);
            if (timer <= 0.f)
            {
                apply_store_float(body, 0x70, 0.f);
            }
        }
    }
    tick_runtime_body_timers(body, channels.actor, context.first_recipient,
        context.second_recipient, context.first, channels.clock);
    const float step = runtime_frame_step(channels.clock);
    const float rate = runtime_actor_rate(channels.actor);
    if (get(body, body_field::move_hold_time) > 0.f)
    {
        const float timer = get(body, body_field::move_hold_time) - rate * step;
        apply_store_float(body, 0x34, timer);
        if (timer <= 0.f)
        {
            apply_store_float(body, 0x34, 0.f);
        }
    }
    tick_runtime_body_wash(body, context.actor_bytes, context.actor_bound, channels.clock);
    apply_runtime_body_model_state(body, channels.model, context.secondary_model,
        context.shared_model_states);
    if (context.action != nullptr)
    {
        context.action->step();
    }
}

// Xbox 0x00074490
void apply_runtime_player_body(std::span<u8> body, RuntimeBodyApplyContext& context)
{
    apply_runtime_body(body, context);
}
}
