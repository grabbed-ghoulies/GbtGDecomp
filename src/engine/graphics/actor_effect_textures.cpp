#include "actor_effect_nodes.h"

#include <cstring>

namespace gbtg
{
// Xbox 0x000ED6E0
void reset_actor_effect_to_debris(ActorEffectNode& effect, ActorEffectModelContext& context)
{
    if (effect.texture.data() != nullptr)
    {
        release_animation_resource(context.registry, effect.texture);
        effect.texture = {};
        store_effect_field(effect.bytes, actor_effect_field::texture, u32{0});
    }
    if (effect.model != nullptr)
    {
        const RuntimeModelAsset* old = defer_runtime_model_release(*effect.model,
            context.deferred, context.fence, context.registry, context.shaders, context.heap_free);
        release_animation_resource(context.registry, old->cpu);
        context.deferred.records.back().owner = std::move(effect.model);
        store_effect_field(effect.bytes, actor_effect_field::model, u32{0});
    }
    store_effect_field(effect.bytes, actor_effect_field::draw_mode, u32{1});
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
            store_effect_field(effect.bytes, actor_effect_field::draw_mode, u32{4});
            return;
        }
        resource_error(context.error, 2);
    }
    effect.model = nullptr;
    store_effect_field(effect.bytes, actor_effect_field::draw_mode, u32{4});
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
    store_effect_field(effect.bytes, actor_effect_field::frame_count, last);
    store_effect_field(effect.bytes, actor_effect_field::frame_rate_low, (1.f - spread) * rate);
    store_effect_field(effect.bytes, actor_effect_field::frame_rate_high, (spread + 1.f) * rate);
    effect.bytes[0x154] = 0;
    store_effect_field(effect.bytes, actor_effect_field::draw_mode, static_cast<u32>((blend != 0) - 1) & 2);
    store_effect_field(effect.bytes, actor_effect_field::depth_mode, setting);
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
