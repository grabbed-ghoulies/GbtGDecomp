#include "actor_effect_nodes.h"

#include <cstring>

namespace gbtg
{
namespace
{
template<class T>
void write(ActorEffectNode& effect, size_t offset, const T& value)
{
    std::memcpy(effect.bytes.data() + offset, &value, sizeof(T));
}
}

// Xbox 0x000ED8C0
void set_actor_effect_model(ActorEffectNode& effect, const char* name,
    ActorEffectModelContext& context)
{
    if (effect.texture.data() != nullptr)
    {
        release_animation_resource(context.registry, effect.texture);
        effect.texture = {};
    }
    if (effect.model != nullptr)
    {
        const RuntimeModelAsset* old = defer_runtime_model_release(*effect.model,
            context.deferred, context.fence, context.registry, context.shaders, context.heap_free);
        release_animation_resource(context.registry, old->cpu);
        context.deferred.records.back().owner = std::move(effect.model);
    }
    recycle_actor_effect_particles(effect);
    if (std::memcmp(name, "aid_", 4) == 0)
    {
        const auto resource = acquire_animation_resource(context.registry, name);
        if (resource.data() != nullptr)
        {
            effect.model = make_runtime_model_instance(*context.views.assets.at(resource.data()),
                1, 1, context.types, context.shaders, context.scratch);
            write(effect, 0x13C, u32{4});
            return;
        }
        resource_error(context.error, 2);
    }
    effect.model = nullptr;
    write(effect, 0x13C, u32{4});
}

// Xbox 0x000ED730
u32 set_actor_effect_texture(ActorEffectNode& effect, const char* name, float rate,
    float spread, u32 setting, i32 mode, i32 enabled, i32 blend,
    ActorEffectModelContext& context)
{
    std::span<const u8> resource;
    bool changed = false;
    if (std::memcmp(name, "aid_", 4) == 0)
    {
        resource = acquire_animation_resource(context.registry, name);
        if (resource.data() == nullptr)
        {
            resource_error(context.error, 2);
        }
    }
    else
    {
        resource = {};
    }
    if (resource.data() != effect.texture.data())
    {
        if (effect.texture.data() != nullptr)
        {
            release_animation_resource(context.registry, effect.texture);
            effect.texture = {};
        }
        if (effect.model != nullptr)
        {
            const RuntimeModelAsset* old = defer_runtime_model_release(*effect.model,
                context.deferred, context.fence, context.registry, context.shaders, context.heap_free);
            release_animation_resource(context.registry, old->cpu);
            context.deferred.records.back().owner = std::move(effect.model);
        }
        recycle_actor_effect_particles(effect);
        changed = true;
    }
    release_animation_resource(context.registry, resource);
    if (changed)
    {
        if (std::memcmp(name, "aid_", 4) == 0)
        {
            resource = acquire_animation_resource(context.registry, name);
            if (resource.data() == nullptr)
            {
                resource_error(context.error, 2);
            }
        }
        else
        {
            resource = {};
        }
        effect.texture = resource;
    }
    const u8 frames = effect.texture.data()[0x11];
    u32 last;
    if (frames == 0)
    {
        last = 0;
    }
    else
    {
        last = frames - 1;
    }
    write(effect, 0x148, last);
    write(effect, 0x14C, (1.f - spread) * rate);
    write(effect, 0x150, (spread + 1.f) * rate);
    effect.bytes[0x154] = 0;
    write(effect, 0x13C, static_cast<u32>((blend != 0) - 1) & 2);
    write(effect, 0x140, setting);
    if (mode == 1)
    {
        effect.bytes[0x154] = 4;
    }
    else if (mode == 2)
    {
        effect.bytes[0x154] = 8;
    }
    else if (mode == 3)
    {
        effect.bytes[0x154] = 0x10;
    }
    if (enabled != 0)
    {
        effect.bytes[0x154] |= 1;
    }
    return last;
}
}
