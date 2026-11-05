#include "body_channels.h"
#include <cstddef>
#include "body_fields.h"
#include <cstring>

namespace gbtg
{
namespace
{
void store_channel_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

// Xbox 0x00030B40
bool runtime_body_channel_is_bound(const RuntimeBodyChannelBindings& animation, i32 channel)
{
    for (const auto& row : animation.rows)
    {
        if (row.resource != nullptr && row.resource->asset != nullptr
            && row.resource->asset->channels != nullptr
            && row.resource->asset->channels[channel] != 0)
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x0001EA90
void ease_runtime_body_channel_targets(std::span<const u8> body,
    RuntimeBodyChannelContext& context)
{
    const u32 count = body_record(body).hash_count;
    for (u32 i = 0; i < count; ++i)
    {
        store_channel_float(context.rows, size_t(i) * 12 + 8, 0.f);
    }
    const auto bank = context.banks[body_record(body).alert_level];
    for (size_t offset = 0; offset < bank.size(); offset += 8)
    {
        u8* selected = nullptr;
        for (u32 i = 0; i < count; ++i)
        {
            if (read_u32(context.rows, size_t(i) * 12) == read_u32(bank, offset))
            {
                selected = context.rows.data() + size_t(i) * 12;
                break;
            }
        }
        // A bank id without a matching row writes through address zero in retail.
        std::memcpy(selected + 8, bank.data() + offset + 4, 4);
    }
    const float step = (runtime_actor_rate(context.actor) * runtime_frame_step(context.clock)) * 0.75f;
    for (u32 i = 0; i < count; ++i)
    {
        const size_t offset = size_t(i) * 12;
        const float current = read_f32(context.rows, offset + 4);
        const float target = read_f32(context.rows, offset + 8);
        const float delta = target - current;
        float next = current;
        if (step != 0.f)
        {
            if (step > engine_absolute(delta))
            {
                next = target;
            }
            else if (!(0.f > delta))
            {
                next = current + step;
            }
            else
            {
                next = current - step;
            }
        }
        store_channel_float(context.rows, offset + 4, next);
        store_channel_float(context.rows, offset + 8, 0.f);
    }
}

// Xbox 0x0001D850
void tick_runtime_body_channel_keys(std::span<u8> body, RuntimeBodyChannelContext& context)
{
    if (body_record(body).unk_92 == 1)
    {
        return;
    }
    float remaining = runtime_actor_rate(context.actor) * runtime_frame_step(context.clock);
    if (remaining > 0.f)
    {
        do
        {
            const float timer = body_record(body).channel_timer - remaining;
            body_record(body).channel_timer = timer;
            if (timer > 0.f)
            {
                return;
            }
            remaining = engine_absolute(timer);
            const auto keys = body_record(body).channel_keys <= 3 ? context.keys[body_record(body).channel_keys]
                : context.address_zero;
            const size_t offset = size_t(body_record(body).channel_key) * 12;
            const size_t slot = 0xE4 + size_t(read_u32(keys, offset + 8)) * 12;
            body[slot + 8] = 1;
            std::memcpy(body.data() + slot, keys.data() + offset + 4, 4);
            std::memcpy(body.data() + offsetof(BodyRecord, channel_timer), keys.data() + offset + 12, 4);
            if (read_u32(keys, offset + 20) == 2)
            {
                if (body_record(body).channel_keys == 0)
                {
                    body_record(body).unk_92 = 1;
                }
                else
                {
                    body_record(body).channel_key = 0;
                    if (body_record(body).channel_keys == 1)
                    {
                        body_record(body).channel_keys = 2;
                        std::memcpy(body.data() + offsetof(BodyRecord, channel_timer), context.keys[2].data(), 4);
                        body_record(body).unk_92 = 2;
                    }
                }
            }
            else
            {
                ++body_record(body).channel_key;
            }
        }
        while (remaining > 0.f);
    }
}

// Xbox 0x0001EDE0
void tick_runtime_body_channel_blends(std::span<u8> body, RuntimeBodyChannelContext& context)
{
    const float step = runtime_actor_rate(context.actor) * runtime_frame_step(context.clock);
    for (size_t slot = 0xE4; slot != 0xFC; slot += 12)
    {
        float blend = read_f32(body, slot + 4);
        switch (body[slot + 8])
        {
        case 1:
        case 5:
            blend = step / read_f32(body, slot) + blend;
            if (blend > 1.f)
            {
                body[slot + 8] = body[slot + 8] == 1 ? 2 : 0;
                blend = 1.f;
            }
            store_channel_float(body, slot + 4, blend);
            break;
        case 2:
            body[slot + 8] = 3;
            break;
        case 3:
        case 4:
            blend -= step / read_f32(body, slot);
            if (0.f > blend)
            {
                blend = 0.f;
                body[slot + 8] = 0;
            }
            store_channel_float(body, slot + 4, blend);
            break;
        default:
            break;
        }
    }
}

// Xbox 0x0001EC20
void apply_runtime_body_channels(std::span<const u8> body, RuntimeBodyChannelContext& context)
{
    RuntimeModelInstance& model = *context.model;
    bool bound = false;
    const i32 initial_count = i32(body_record(body).hash_count);
    for (i32 i = 0; i < initial_count; ++i)
    {
        if (context.animation != nullptr && runtime_body_channel_is_bound(*context.animation,
            i32(read_u32(context.rows, size_t(i) * 12))))
        {
            bound = true;
            break;
        }
    }
    for (i32 i = 0; i < i32(body_record(body).hash_count); ++i)
    {
        const size_t offset = size_t(i) * 12;
        const u32 channel = read_u32(context.rows, offset);
        float* target = model.auxiliary_targets[channel];
        if (bound)
        {
            store_channel_float(context.rows, offset + 4, target != nullptr ? *target : 0.f);
        }
        else if (channel == 0 && body_record(body).unk_FC != 0
            && (body_record(body).aux_level_0 != 0 || body_record(body).aux_level_1 != 0))
        {
            float value = target != nullptr ? *target : 0.f;
            const float selected = read_f32(context.rows, offset + 4);
            if (selected >= value)
            {
                value = selected;
            }
            if (target != nullptr)
            {
                *target = value;
            }
        }
        else if (target != nullptr)
        {
            std::memcpy(target, context.rows.data() + offset + 4, 4);
        }
    }
}

// Xbox 0x0001EEC0
void apply_runtime_body_default_blend(std::span<const u8> body, RuntimeBodyChannelContext& context)
{
    if (context.animation != nullptr && runtime_body_channel_is_bound(*context.animation, 0))
    {
        return;
    }
    if (context.model->auxiliary_targets[0] != nullptr)
    {
        std::memcpy(context.model->auxiliary_targets[0], body.data() + offsetof(BodyRecord, aux_value_0), 4);
    }
    if (context.model->auxiliary_targets[0] != nullptr)
    {
        std::memcpy(context.model->auxiliary_targets[0], body.data() + offsetof(BodyRecord, aux_value_1), 4);
    }
}

// Xbox 0x00072AC0
void step_runtime_body_channels(std::span<u8> body, RuntimeBodyChannelChild*& attachment,
    RuntimeBodyChannelContext& context)
{
    if (attachment != nullptr)
    {
        attachment->step();
    }
    if (i32(body_record(body).hash_count) > 0)
    {
        ease_runtime_body_channel_targets(body, context);
    }
    if (body_record(body).unk_FC != 0)
    {
        tick_runtime_body_channel_keys(body, context);
        tick_runtime_body_channel_blends(body, context);
    }
}

// Xbox 0x00025190
void update_runtime_body_model_anchors(std::span<u8> body, entity_detail::ListHead& anchors)
{
    if (body_record(body).unk_310 == 0)
    {
        for (auto* anchor = static_cast<ActorModelAnchor*>(anchors.next); anchor != nullptr;
            anchor = static_cast<ActorModelAnchor*>(anchor->owner_link.next))
        {
            update_actor_model_anchor(*anchor);
        }
    }
    body_record(body).look_point_set = 0;
}

// Xbox 0x00025130
void apply_runtime_body_model_state(std::span<const u8> body,
    RuntimeModelInstance*& model, RuntimeModelInstance*& secondary, std::span<u32> shared_states)
{
    if (body_record(body).unk_2D0 != 0)
    {
        const u8 slot = body_record(body).unk_2D1;
        const u8 value = body_record(body).unk_2D2;
        if (model != nullptr)
        {
            set_runtime_model_node_state(*model, slot, value, shared_states);
        }
        if (secondary != nullptr)
        {
            set_runtime_model_node_state(*secondary, slot, value, shared_states);
        }
    }
}
}
