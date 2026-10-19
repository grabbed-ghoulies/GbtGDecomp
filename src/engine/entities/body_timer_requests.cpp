#include "body_timer_requests.h"
#include "player_goal_cleanup.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x00027B60
void request_runtime_body_hit_timer(std::span<u8> body, float duration,
    u32 mode, RuntimeBodyTimerCreation& context)
{
    auto* recipient = context.recipient;
    const u32 actor = read_u32(body, 8);
    if (recipient == nullptr)
    {
        auto clone = clone_executable_resource(context.child.resources,
            reinterpret_cast<const char*>(body.data() + 0x764));
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
                auto& native = *context.created.at(object.bytes.get());
                context.recipient = &native.recipient;
                address = native.address;
            }
            else
            {
                context.recipient = nullptr;
                address = 0;
            }
            context.object = std::move(object);
            clone.bytes.reset();
        }
        std::memcpy(body.data() + 0x760, &address, 4);
    }
    else
    {
        const std::array<u32, 1> message{3};
        recipient->message(message);
    }
    if (read_u32(body, 0x754) == 0)
    {
        std::memcpy(body.data() + 0x758, &duration, 4);
        std::memcpy(body.data() + 0x754, &mode, 4);
    }
}

// Xbox 0x00027BE0
void hold_runtime_body_latches(std::span<u8> body, RuntimeBodyHoldTimerContext& context)
{
    auto* recipient = context.timer.recipient;
    const u32 actor = read_u32(body, 8);
    if (recipient == nullptr)
    {
        if (read_u32(body, 0x7F4) == 0x5F646961) // "aid_"
        {
            auto clone = clone_executable_resource(context.timer.child.resources,
                reinterpret_cast<const char*>(body.data() + 0x7F4));
            u32 address = 0;
            if (clone.bytes != nullptr)
            {
                std::memcpy(clone.bytes.get() + 8, &actor, 4);
                auto object = create_factory_object(context.timer.child.factory, {clone.bytes.get(), clone.size});
                if (object.bytes != nullptr)
                {
                    auto& native = *context.timer.created.at(object.bytes.get());
                    context.timer.recipient = &native.recipient;
                    address = native.address;
                }
                context.timer.object = std::move(object);
                clone.bytes.reset();
            }
            std::memcpy(body.data() + 0x7F0, &address, 4);
        }
    }
    else
    {
        const std::array<u32, 1> message{3};
        recipient->message(message);
    }
    if (body[0x7E4] == 0)
    {
        // 0x0001F7A0 before and after raising all six latches; {0x47, actor} goes to the DAT_00511AA4 nodes when the
        // full mask drops, as in 0x0001F3F0.
        increment_runtime_body_latch_mask(body, 0x3F, context.owner, context.first);
        body[0x7E4] = 1;
    }
    body[0x7E5] = u8(body[0x7E5] + 1);
    std::memcpy(body.data() + 0x7E8, body.data() + 0x7EC, 4);
}
}
