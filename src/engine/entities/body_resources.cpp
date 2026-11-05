#include "body_resources.h"
#include <cstddef>
#include "body_fields.h"
#include "entity_heap.h"
#include "player_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00035D90
u32 runtime_body_resource_size(const u8* resource,
    std::span<const AnimationResourceRecord> records,
    std::span<const RuntimeBodyLoadedResourceSize> loaded)
{
    for (const auto& record : records)
    {
        if (record.resource_slot != 0 && loaded[record.resource_slot - 1].cpu == resource)
        {
            const auto& entry = loaded[record.resource_slot - 1];
            u32 size = entry.secondary;
            if (entry.storage != nullptr)
            {
                size += read_u32({entry.storage, 4}, 0);
            }
            return size + entry.tail;
        }
    }
    return 0;
}

// Xbox 0x0001E7C0
void acquire_runtime_body_resource_hashes(std::span<const u8> parameters,
    std::span<std::span<const u8>, 10> resources, std::span<u32, 10> counts,
    RuntimeBodyResourceHash*& hashes, u32& count, AnimationResourceRegistry& registry,
    std::span<const RuntimeBodyLoadedResourceSize> loaded, ResourceErrorState& error)
{
    u32 capacity = 0;
    for (size_t i = 0; i < 10; ++i)
    {
        const size_t offset = 0x2C8 + i * 0x80;
        if (read_u32(parameters, offset) == 0x5F646961)
        {
            const auto acquired = acquire_animation_resource(registry,
                reinterpret_cast<const char*>(parameters.data() + offset));
            if (acquired.data() == nullptr)
            {
                resource_error(error, 2);
            }
            resources[i] = acquired;
            if (acquired.data() != nullptr)
            {
                counts[i] = runtime_body_resource_size(acquired.data(), registry.records, loaded) >> 3;
                ++capacity;
            }
        }
    }
    if (capacity != 0)
    {
        for (size_t i = 1; i < 10; ++i)
        {
            if (resources[i].data() != nullptr)
            {
                if (hashes == nullptr)
                {
                    hashes = static_cast<RuntimeBodyResourceHash*>(engine_heap_allocate(4 * sizeof(*hashes)));
                    hashes[count] = {read_u32(resources[i], 0), 0, 0};
                    ++count;
                    capacity = 4;
                }
                else
                {
                    for (i32 index = 0; index < std::bit_cast<i32>(counts[i]); ++index)
                    {
                        const u32 key = read_u32(resources[i], size_t(index) * 8);
                        auto* row = hashes;
                        auto* end = hashes + count;
                        while (row < end && row->key != key)
                        {
                            ++row;
                        }
                        if (row != end)
                        {
                            continue;
                        }
                        if (count == capacity)
                        {
                            hashes = static_cast<RuntimeBodyResourceHash*>(engine_heap_reallocate(hashes,
                                i32((capacity + 4) * sizeof(*hashes))));
                            capacity += 4;
                        }
                        hashes[count] = {key, 0, 0};
                        ++count;
                    }
                }
            }
        }
    }
}

// Xbox 0x0001FA50
void initialize_runtime_body_friction(std::span<u8> body, std::span<const u8> parameters,
    std::span<u8> actor, std::span<const u8> curve, std::span<const u8> tags)
{
    body_record(body).push_mode = 1;
    body_record(body).shape_mode = parameters[0x1AA];
    const u16 tag = get(actor, player_field::damage_class);
    const float maximum = read_f32(curve, 8);
    float value = maximum;
    if (tag != 0 && curve[1] != curve[0])
    {
        const float minimum = read_f32(curve, 4);
        value = ((float(tags[size_t(tag) * 0x87 - 0x37]) - float(curve[0]))
            / (float(curve[1]) - float(curve[0]))) * (maximum - minimum) + minimum;
        if (maximum > minimum)
        {
            if (value > maximum)
            {
                value = maximum;
            }
            else if (minimum > value)
            {
                value = minimum;
            }
        }
        else
        {
            if (maximum > value)
            {
                value = maximum;
            }
            else if (value > minimum)
            {
                value = minimum;
            }
        }
    }
    value *= 0.25f;
    std::memcpy(body.data() + offsetof(BodyRecord, unk_170), &value, 4);
    const u32 damping = 0x3F7D70A4;
    std::memcpy(body.data() + offsetof(BodyRecord, stagger), &damping, 4);
    const u32 mode = parameters[0x1AA];
    std::memcpy(actor.data() + 0x120, &mode, 4);
}

// Xbox 0x000242E0
void acquire_runtime_body_attribute_table(std::span<u8> body,
    std::span<const u8> parameters, std::span<const u8>& resource,
    AnimationResourceRegistry& registry, ResourceErrorState& error)
{
    const u32 one = 1;
    body_record(body).unk_278 = one;
    if (read_u32(parameters, 0x108) == 0x5F646961)
    {
        const auto acquired = acquire_animation_resource(registry,
            reinterpret_cast<const char*>(parameters.data() + 0x108));
        if (acquired.data() == nullptr)
        {
            resource_error(error, 2);
        }
        resource = acquired;
    }
}

// Xbox 0x0001ED70
void initialize_runtime_body_model_channels(std::span<u8> body, const u8* model)
{
    u32 count = 0;
    for (size_t i = 0; i < 2; ++i)
    {
        const float one = 1.f;
        const u32 zero = 0;
        std::memcpy(body.data() + offsetof(BodyRecord, aux_time_0) + i * 12, &one, 4);
        body[offsetof(BodyRecord, aux_level_0) + i * 12] = 0;
        std::memcpy(body.data() + offsetof(BodyRecord, aux_value_0) + i * 12, &zero, 4);
        // Both indices in DAT_00217E44 are zero, including the second probe.
        if (model != nullptr && read_u32({model, 0x1D0}, 0x1CC) != 0)
        {
            ++count;
        }
    }
    const u32 enabled = count != 0 && model != nullptr ? 1 : 0;
    body_record(body).unk_FC = enabled;
}

// Xbox 0x0001E9F0
void release_runtime_body_resource_hashes(std::span<std::span<const u8>, 10> resources,
    RuntimeBodyResourceHash*& hashes, u32& count, AnimationResourceRegistry& registry)
{
    for (auto& resource : resources)
    {
        if (resource.data() != nullptr)
        {
            release_animation_resource(registry, resource);
            resource = {};
        }
    }
    if (hashes != nullptr)
    {
        engine_heap_free(hashes);
        hashes = nullptr;
        count = 0;
        return;
    }
    count = 0;
}

// Xbox 0x00027740
void reload_runtime_body_actions(std::span<u8> body, const char* name,
    FactoryObject& first, FactoryObject& second, std::span<const u8>& resource,
    ObjectFactoryRegistry& factory, AnimationResourceRegistry& registry,
    ResourceErrorState& error)
{
    release_runtime_body_actions(body, first, second, resource, factory, registry);
    u32 prefix;
    std::memcpy(&prefix, name, 4);
    if (prefix != 0x5F646961)
    {
        resource = {};
        return;
    }
    const auto acquired = acquire_animation_resource(registry, name);
    if (acquired.data() == nullptr)
    {
        resource_error(error, 2);
    }
    resource = acquired;
}

// Xbox 0x0001B920
bool release_runtime_body_cached_resource(void* resource,
    std::span<RuntimeBodyResourceCacheEntry> entries, i32& count)
{
    for (i32 i = 0; i < count; ++i)
    {
        if (entries[i].resource == resource)
        {
            auto* entry = &entries[i];
            if (entry == nullptr)
            {
                return false;
            }
            --entry->references;
            if (entry->references != 0)
            {
                return false;
            }
            --count;
            if (i != count)
            {
                *entry = entries[count];
            }
            return true;
        }
    }
    return false;
}

// Xbox 0x0013F150
RuntimeBodyCollisionState* create_runtime_body_collision_state(const RuntimeBodyModelNodes* model)
{
    if (model != nullptr)
    {
        for (const auto& node : model->nodes)
        {
            if (node.kind == 4)
            {
                if (node.data == nullptr)
                {
                    return nullptr;
                }
                const u8* first = node.data + 8;
                u32 count = 0;
                const u8* shape = first;
                do
                {
                    if (shape[0] == 0 && (shape[4] & 1) != 0)
                    {
                        ++count;
                    }
                    if (shape[1] == 0)
                    {
                        break;
                    }
                    shape += shape[1];
                } while (shape != nullptr);
                if (count == 0)
                {
                    return nullptr;
                }
                const size_t size = sizeof(RuntimeBodyCollisionState)
                    + count * sizeof(RuntimeBodyCollisionRecord);
                auto* state = static_cast<RuntimeBodyCollisionState*>(engine_heap_allocate(i32(size)));
                if (state == nullptr)
                {
                    return nullptr;
                }
                std::memset(state, 0, size);
                state->count = count;
                state->records = reinterpret_cast<RuntimeBodyCollisionRecord*>(state + 1);
                u8 sequence = 0;
                size_t index = 0;
                shape = first;
                do
                {
                    if (shape[0] == 0 && (shape[4] & 1) != 0)
                    {
                        auto& record = state->records[index++];
                        record.bytes[0x16] = sequence++;
                        record.shape = shape;
                        record.bytes[0x17] = 1;
                        const float one = 1.f;
                        std::memcpy(record.bytes.data() + 0x18, &one, 4);
                    }
                    if (shape[1] == 0)
                    {
                        break;
                    }
                    shape += shape[1];
                } while (shape != nullptr);
                return state;
            }
        }
    }
    return nullptr;
}

// Xbox 0x0013EE80
void restore_runtime_body_collision_overrides(GhoulySpawnPool& pool)
{
    auto* row = reinterpret_cast<RuntimeBodyCollisionOverride*>(pool.begin);
    auto* end = reinterpret_cast<RuntimeBodyCollisionOverride*>(pool.cursor);
    for (; row < end; ++row)
    {
        if (row->active != 0)
        {
            row->actor->model_flags = row->saved;
            row->active = 0;
        }
    }
}

// Xbox 0x0013F240
void destroy_runtime_body_collision_state(RuntimeBodyCollisionState* state)
{
    state->records = nullptr;
    state->count = 0;
    if (state->overrides != nullptr)
    {
        restore_runtime_body_collision_overrides(*state->overrides);
        engine_heap_free(state->overrides);
        state->overrides = nullptr;
    }
    if (state->removed != nullptr)
    {
        engine_heap_free(state->removed);
        state->removed = nullptr;
    }
    engine_heap_free(state);
}
}
