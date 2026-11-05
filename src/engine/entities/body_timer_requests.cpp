#include "body_timer_requests.h"
#include <cstddef>
#include "body_fields.h"
#include "player_goal_cleanup.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x00027B60
void request_runtime_body_hit_timer(std::span<u8> body, float duration,
    u32 mode, RuntimeBodyTimerCreation& context)
{
    // Body +0x760 decides, as retail reads it; a timer's 0x14 (0x00027B20) may have cleared it after the native view.
    auto* recipient = body_record(body).hit_timer == 0 ? nullptr : context.recipient;
    const u32 actor = read_u32(body, 8);
    if (recipient == nullptr)
    {
        auto clone = clone_executable_resource(context.child.resources,
            body_record(body).hit_timer_name.data());
        u32 address;
        if (clone.bytes == nullptr)
        {
            address = 0;
            context.recipient = nullptr;
        }
        else
        {
            std::memcpy(clone.bytes.get() + 8, &actor, 4);
            auto object = create_factory_object(context.child.factory, {clone.bytes.get(), clone.size});
            if (object.bytes != nullptr)
            {
                auto& native = context.keep(std::move(object));
                context.recipient = &native.recipient;
                address = native.address;
            }
            else
            {
                context.recipient = nullptr;
                address = 0;
            }
            clone.bytes.reset();
        }
        body_record(body).hit_timer = address;
    }
    else
    {
        const std::array<u32, 1> message{3};
        recipient->message(message);
    }
    if (body_record(body).hit_timer_running == 0)
    {
        body_record(body).unk_758 = duration;
        body_record(body).hit_timer_running = mode;
    }
}

// Xbox 0x00027BE0
void hold_runtime_body_latches(std::span<u8> body, RuntimeBodyHoldTimerContext& context)
{
    auto* recipient = body_record(body).second_timer == 0 ? nullptr : context.timer.recipient; // Body +0x7F0 decides.
    const u32 actor = read_u32(body, 8);
    if (recipient == nullptr)
    {
        if (read_u32(body, offsetof(BodyRecord, second_timer_name)) == 0x5F646961) // "aid_"
        {
            auto clone = clone_executable_resource(context.timer.child.resources,
                body_record(body).second_timer_name.data());
            u32 address = 0;
            if (clone.bytes != nullptr)
            {
                std::memcpy(clone.bytes.get() + 8, &actor, 4);
                auto object = create_factory_object(context.timer.child.factory, {clone.bytes.get(), clone.size});
                if (object.bytes != nullptr)
                {
                    auto& native = context.timer.keep(std::move(object));
                    context.timer.recipient = &native.recipient;
                    address = native.address;
                }
                clone.bytes.reset();
            }
            body_record(body).second_timer = address;
        }
    }
    else
    {
        const std::array<u32, 1> message{3};
        recipient->message(message);
    }
    if (body_record(body).unk_7E4 == 0)
    {
        // 0x0001F7A0 before and after raising all six latches; {0x47, actor} goes to the DAT_00511AA4 nodes when the
        // full mask drops, as in 0x0001F3F0.
        increment_runtime_body_latch_mask(body, 0x3F, context.owner, context.first);
        body_record(body).unk_7E4 = 1;
    }
    body_record(body).unk_7E5 = u8(body_record(body).unk_7E5 + 1);
    std::memcpy(body.data() + offsetof(BodyRecord, unk_7E8), body.data() + offsetof(BodyRecord, unk_7EC), 4);
}
}
