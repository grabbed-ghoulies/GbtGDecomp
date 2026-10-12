#include "actor_callout_resources.h"
#include <cstring>

namespace gbtg
{
namespace
{
u8* callout_terminator(const AnimationLoadedResource& loaded)
{
    // The resource is the provider's own writable copy; retail writes +0xC8 in place.
    auto* record = const_cast<u8*>(loaded.cpu.data());
    while (read_u16({record, 6}, 4) != 0)
    {
        record += read_u16({record, 2}, 0);
    }
    return record;
}
}

// Xbox 0x0004F650
void post_load_runtime_callout_resource(const AnimationLoadedResource& loaded, RuntimeCalloutResources& context)
{
    u8* record = callout_terminator(loaded);
    const auto* name = reinterpret_cast<const char*>(record + 0x48);
    if (read_u32({record + 0x48, 4}, 0) == 0x5F646961 && read_u32({record + 0x48, 4}, 0) == 0x5F646961)
    {
        const auto linked = acquire_animation_resource(context.registry, name);
        if (linked.data() == nullptr)
        {
            resource_error(context.error, 2);
        }
        const u32 address = context.address_of(linked.data());
        std::memcpy(record + 0xC8, &address, 4);
        context.linked_records[record] = linked.data();
        return;
    }
    const u32 zero = 0;
    std::memcpy(record + 0xC8, &zero, 4);
    context.linked_records[record] = nullptr;
}

// Xbox 0x0004F6C0
void unload_runtime_callout_resource(const AnimationLoadedResource& loaded, RuntimeCalloutResources& context)
{
    u8* record = callout_terminator(loaded);
    if (read_u32({record + 0xC8, 4}, 0) != 0)
    {
        release_animation_resource(context.registry, {context.linked_records.at(record), 0});
        const u32 zero = 0;
        std::memcpy(record + 0xC8, &zero, 4);
    }
    context.linked_records.erase(record);
}

// Xbox 0x0004F610
void register_runtime_callout_resource_hook(AnimationResourceRegistry& registry, RuntimeCalloutResources& context)
{
    u32 index = 0;
    AnimationResourceHook* hook = nullptr;
    do
    {
        if (registry.hooks[index].type == nullptr)
        {
            hook = &registry.hooks[index];
            break;
        }
        ++index;
    } while (index < 16);
    hook->type = "callout"; // String 0x00322DF8.
    hook->relocate = nullptr;
    hook->post_load = [&context](const AnimationLoadedResource& loaded)
    {
        post_load_runtime_callout_resource(loaded, context);
    };
    hook->unload = [&context](const AnimationLoadedResource& loaded)
    {
        unload_runtime_callout_resource(loaded, context);
    };
}
}
