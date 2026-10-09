// Host pool storage; capacities and grow words come from Xbox 0x00033030 /
// 0x00035A90. This does not install the missing loading-provider initializers.
AnimationResourceRegistry::AnimationResourceRegistry()
{
    records.reserve(32);
    loaded.reserve(32);
    deferred[0].reserve(2);
    deferred[1].reserve(2);
}

// Xbox 0x000358B0. The exported assembly also XORs the high nibble itself;
// Ghidra's C omits that term. The caller has already checked the AID prefix.
u32 animation_resource_hash(const char* name)
{
    const auto* cursor = reinterpret_cast<const u8*>(name + 4);
    u32 hash = 0;
    u8 value = *cursor;
    while (value != 0)
    {
        hash = (value & 0xdfu) + hash * 0x10u;
        ++cursor;
        const u32 high = hash & 0xf0000000u;
        if (high != 0)
        {
            hash ^= high | (high >> 24);
        }
        value = *cursor;
    }
    return hash;
}

// Xbox 0x00036030.
AnimationResourceRecord* find_animation_resource(std::vector<AnimationResourceRecord>& records, const char* name)
{
    size_t cursor = 0;
    while (true)
    {
        if (records.size() <= cursor)
        {
            return nullptr;
        }
        const u32 hash = animation_resource_hash(name);
        if (records[cursor].hash == hash)
        {
            break;
        }
        ++cursor;
    }
    return &records[cursor];
}

// Xbox 0x00035540.
void register_animation_resource_provider(AnimationResourceRegistry& registry,
    std::function<u32(u32, const AnimationResourceRecord&, const void*&)> find,
    std::function<bool(u32, const void*, AnimationLoadedResource&)> load,
    std::function<void(u32, const char*, const AnimationLoadedResource&)> unload,
    int category, const void* context)
{
    AnimationResourceProvider* selected = nullptr;
    size_t index = 0;
    do
    {
        if (registry.providers[index].handle == 0)
        {
            selected = &registry.providers[index];
            break;
        }
        ++index;
    } while (index < 16);
    ++registry.provider_serial;
    u32 handle = registry.provider_serial;
    if (registry.provider_serial == 0)
    {
        handle = 1;
    }
    selected->find = std::move(find);
    selected->load = std::move(load);
    selected->unload = std::move(unload);
    selected->category = category;
    selected->handle = handle;
    selected->context = context;
}

namespace
{

// Xbox 0x00035E10.
AnimationResourceRecord* allocate_animation_resource_name(AnimationResourceRegistry& registry, const char* name)
{
    size_t cursor = 0;
    AnimationResourceRecord* selected = nullptr;
    for (; cursor < registry.records.size(); ++cursor)
    {
        if (registry.records[cursor].resource_slot == 0)
        {
            selected = &registry.records[cursor];
            break;
        }
    }
    if (selected == nullptr)
    {
        if (registry.records.size() == registry.record_limit)
        {
            registry.record_limit += 32;
            registry.records.reserve(registry.record_limit);
        }
        registry.records.emplace_back();
        selected = &registry.records[cursor];
    }
    selected->name = name;
    selected->resource_slot = 0;
    selected->references = 0;
    selected->hash = animation_resource_hash(name);
    return selected;
}

// Xbox 0x00035710.
size_t allocate_animation_loaded_resource(AnimationResourceRegistry& registry)
{
    size_t cursor = 0;
    bool found = false;
    for (; cursor < registry.loaded.size(); ++cursor)
    {
        if (registry.loaded[cursor].provider == 0)
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        if (registry.loaded.size() == registry.loaded_limit)
        {
            registry.loaded_limit += 32;
            registry.loaded.reserve(registry.loaded_limit);
        }
        registry.loaded.emplace_back();
    }
    registry.loaded[cursor] = AnimationLoadedResource{};
    return cursor;
}

// Xbox 0x000355A0. Provider callbacks are the original indirect-call boundaries;
// installing a provider requires translating that provider's routines.
bool load_animation_registered_resource(AnimationResourceRegistry& registry, AnimationResourceRecord& record)
{
    if (record.resource_slot != 0 && registry.loaded.data() + (record.resource_slot - 1) != nullptr)
    {
        return false;
    }
    const void* resolved = &record.resource_slot;
    size_t provider_index = 0;
    do
    {
        AnimationResourceProvider& provider = registry.providers[provider_index];
        if (provider.handle != 0)
        {
            const u32 found = provider.find(provider.handle, record, resolved);
            if (found != 0)
            {
                const size_t loaded_index = allocate_animation_loaded_resource(registry);
                record.resource_slot = u32(loaded_index + 1);
                AnimationLoadedResource& loaded = registry.loaded[loaded_index];
                loaded.provider = provider.handle;
                loaded.provider_slot = found;
                return provider.load(provider.handle, resolved, loaded);
            }
        }
        ++provider_index;
    } while (provider_index < 16);
    return false;
}

} // namespace

// Xbox 0x000FBF50.
int compare_animation_resource_type(const char* first, const char* second)
{
    while (true)
    {
        u8 a = u8(*second);
        if (0x20 < a)
        {
            a &= 0xdf;
        }
        u8 b = u8(*first);
        if (0x20 < b)
        {
            b &= 0xdf;
        }
        if (a != b)
        {
            return int(a) - int(b);
        }
        ++second;
        ++first;
        if (a == 0)
        {
            return 0;
        }
    }
}

namespace
{

// Xbox 0x000362B0.
AnimationResourceHook* find_animation_resource_hook(AnimationResourceRegistry& registry, const char* type)
{
    size_t index = 0;
    do
    {
        AnimationResourceHook& hook = registry.hooks[index];
        if (hook.type != nullptr)
        {
            if (compare_animation_resource_type(type, hook.type) == 0)
            {
                return &hook;
            }
        }
        ++index;
    } while (index < 16);
    return nullptr;
}

// Xbox 0x0013A930.
void post_load_animation_resource(AnimationResourceRegistry& registry, const AnimationResourceRecord& record,
                                   const AnimationLoadedResource& loaded)
{
    std::string type;
    size_t index = 0;
    char value = record.name[4];
    while (value != '_')
    {
        type.push_back(value);
        ++index;
        value = record.name[4 + index];
    }
    AnimationResourceHook* hook = find_animation_resource_hook(registry, type.c_str());
    if (hook != nullptr && hook->post_load)
    {
        hook->post_load(loaded);
    }
}

} // namespace

// Xbox 0x00035E90.
std::span<const u8> acquire_animation_resource(AnimationResourceRegistry& registry, const char* name)
{
    bool newly_loaded = false;
    AnimationResourceRecord* record = find_animation_resource(registry.records, name);
    if (record == nullptr)
    {
        record = allocate_animation_resource_name(registry, name);
        AnimationResourceRecord* first = find_animation_resource(registry.deferred[0], name);
        AnimationResourceRecord* second = find_animation_resource(registry.deferred[1], name);
        if (first != nullptr)
        {
            *record = *first;
            auto& pool = registry.deferred[0];
            if (first < &pool.back())
            {
                *first = pool.back();
            }
            pool.pop_back();
        }
        if (second != nullptr)
        {
            *record = *second;
            auto& pool = registry.deferred[1];
            if (second < &pool.back())
            {
                *second = pool.back();
            }
            pool.pop_back();
        }
        if (record == nullptr)
        {
            record = allocate_animation_resource_name(registry, name);
        }
    }
    if (record->references == 0)
    {
        newly_loaded = load_animation_registered_resource(registry, *record);
    }
    if (record->resource_slot == 0)
    {
        return {};
    }
    const AnimationLoadedResource& loaded = registry.loaded[record->resource_slot - 1];
    const auto result = loaded.cpu;
    ++record->references;
    if (newly_loaded)
    {
        post_load_animation_resource(registry, *record, loaded);
    }
    return result;
}

// Xbox 0x00035B60.
std::span<const u8> acquire_named_animation_resource(AnimationResourceRegistry& registry,
    const char* name, ResourceErrorState& error)
{
    if (std::memcmp(name, "aid_", 4) != 0)
    {
        return {};
    }
    const auto resource = acquire_animation_resource(registry, name);
    if (resource.data() == nullptr)
    {
        resource_error(error, 2);
    }
    return resource;
}
