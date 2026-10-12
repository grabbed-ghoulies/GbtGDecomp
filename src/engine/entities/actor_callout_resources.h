#pragma once
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include <functional>
#include <unordered_map>

namespace gbtg
{
// The "callout" resource hook: each callout resource ends in a terminator record whose +0x48 may name a
// further callout resource; post-load acquires it into +0xC8, which callout dispatch follows.
struct RuntimeCalloutResources
{
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    // Native view of each terminator's +0xC8 link (terminator record -> linked resource), read by the callout walk.
    std::unordered_map<const u8*, const u8*>& linked_records;
    std::function<u32(const void*)> address_of; // Retail address stored at +0xC8.
};
void post_load_runtime_callout_resource(const AnimationLoadedResource& loaded,
    RuntimeCalloutResources& context); // Xbox 0x0004F650
void unload_runtime_callout_resource(const AnimationLoadedResource& loaded,
    RuntimeCalloutResources& context); // Xbox 0x0004F6C0
void register_runtime_callout_resource_hook(AnimationResourceRegistry& registry,
    RuntimeCalloutResources& context); // Xbox 0x0004F610
}
