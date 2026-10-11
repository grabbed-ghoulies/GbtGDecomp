#include "body_timer_requests.h"
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
}
