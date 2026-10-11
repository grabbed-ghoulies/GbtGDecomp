#pragma once
#include "gbtg/object_factory.h"
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"

namespace gbtg
{
struct RuntimeCinemaMindLifecycle
{
    std::span<u8> bytes;
    std::span<const u8> actor; // Mind +8.
    std::span<const u8>& resource; // Mind +0x84, native acquired resource view.
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    u32& random;
    std::function<u32(const void*)> address_of;
};
void runtime_cinema_mind_storage_sizes(ObjectSizes& sizes); // Xbox 0x0007C500
void load_runtime_cinema_mind_resource(RuntimeCinemaMindLifecycle& context,
    std::span<const u8> parameters); // Xbox 0x0007CC00
bool initialize_runtime_cinema_mind(RuntimeCinemaMindLifecycle& context,
    std::span<const u8> parameters); // Xbox 0x0007C7D0
void destroy_runtime_cinema_mind(RuntimeCinemaMindLifecycle& context); // Xbox 0x0007C880
}
