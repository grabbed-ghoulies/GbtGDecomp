// Xbox 0x000357D0.
bool resource_names_match(const char* first, const char* second)
{
    if (std::memcmp(first, "aid_", 4) == 0)
    {
        if (std::memcmp(second, "aid_", 4) == 0)
        {
            const auto* cursor = reinterpret_cast<const u8*>(first + 4);
            const auto* other = reinterpret_cast<const u8*>(second + 4);
            u8 value = *cursor & 0xdf;
            if (value == (*other & 0xdf))
            {
                do
                {
                    ++cursor;
                    ++other;
                    if (value == 0)
                    {
                        return true;
                    }
                    value = *cursor & 0xdf;
                } while (value == (*other & 0xdf));
            }
        }
    }
    else if (std::memcmp(second, "aid_", 4) != 0)
    {
        return true;
    }
    return false;
}

// Xbox 0x00034730. EBX pool. The native records already hold resolved CPU spans, so the dump's
// +0x18 base addition is done by the bundle reader. The type copy into DAT_004DF220 and its hook
// search inline 0x000362B0.
bool relocate_animation_bundle_pool(AnimationBundlePool& pool, AnimationResourceRegistry& registry,
    const u32& loading, const std::function<void()>& pump_audio)
{
    u32 index = 0;
    if (pool.records.size() != 0)
    {
        do
        {
            AnimationBundleRecord& record = pool.records[index];
            record.references = 0;
            std::string type;
            for (size_t cursor = 4; record.name[cursor] != '_'; ++cursor)
            {
                type.push_back(record.name[cursor]);
            }
            AnimationResourceHook* hook = find_animation_resource_hook(registry, type.c_str());
            if (hook != nullptr && hook->relocate)
            {
                AnimationLoadedResource loaded;
                loaded.cpu = record.cpu;
                loaded.gpu = record.gpu;
                loaded.cpu_storage = record.cpu_storage;
                loaded.gpu_storage = record.gpu_storage;
                hook->relocate(loaded);
            }
            if (loading != 0)
            {
                // 0x0015B780 with DAT_005096C0, then 0x0003A040 and 0x000396F0.
                pump_audio();
            }
            ++index;
        } while (index < pool.records.size());
    }
    return true;
}

// Xbox 0x00034A70.
bool find_bundle_resource(std::span<AnimationBundlePool> pools, const char* name,
    AnimationBundlePool*& pool, AnimationBundleRecord*& record)
{
    size_t pool_index = 0;
    do
    {
        if (pools.size() <= pool_index)
        {
            return false;
        }
        AnimationBundlePool& candidate = pools[pool_index];
        if (candidate.active)
        {
            size_t index = 0;
            if (candidate.records.size() != 0)
            {
                do
                {
                    if (resource_names_match(candidate.records[index].name.c_str(), name))
                    {
                        pool = &candidate;
                        record = &candidate.records[index];
                        return true;
                    }
                    ++index;
                } while (index < candidate.records.size());
            }
        }
        ++pool_index;
    } while (true);
}

// Xbox 0x00034A30.
u32 find_bundle_resource_provider(std::span<AnimationBundlePool> pools, const char* name,
    const void*& resolved)
{
    AnimationBundlePool* pool;
    AnimationBundleRecord* record;
    if (find_bundle_resource(pools, name, pool, record))
    {
        resolved = record;
        return 0xffffffffu;
    }
    return 0;
}

// Xbox 0x00034AF0.
bool load_bundle_resource_provider(const void* resolved, AnimationLoadedResource& output)
{
    auto& record = *const_cast<AnimationBundleRecord*>(static_cast<const AnimationBundleRecord*>(resolved));
    output.cpu = record.cpu;
    output.gpu = record.gpu;
    output.cpu_storage = record.cpu_storage;
    output.gpu_storage = record.gpu_storage;
    ++record.references;
    return true;
}

// Xbox 0x00034B30.
void unload_bundle_resource_provider(std::span<AnimationBundlePool> pools, const char* name)
{
    AnimationBundlePool* pool;
    AnimationBundleRecord* record;
    find_bundle_resource(pools, name, pool, record);
    --record->references;
}

// Xbox 0x00034CB0.
ExecutableResourceRecord* find_executable_resource_record(std::span<ExecutableResourceRecord> records,
    const char* name, u32 hash)
{
    int index = 0;
    if (0 < int(records.size()))
    {
        do
        {
            if (hash == records[size_t(index)].hash &&
                resource_names_match(name, records[size_t(index)].name.c_str()))
            {
                return &records[size_t(index)];
            }
            ++index;
        } while (index < int(records.size()));
    }
    return nullptr;
}

// Xbox 0x00034D00.
ExecutableResourceRecord* find_executable_resource(std::span<ExecutableResourcePool> pools,
    const char* name, bool include_inactive)
{
    const u32 hash = animation_resource_hash(name);
    if (pools.size() != 0)
    {
        size_t pool_index = 0;
        do
        {
            auto& pool = pools[pool_index];
            if (include_inactive || pool.active)
            {
                std::string type;
                size_t index = 0;
                char value = name[4];
                while (value != '_')
                {
                    type.push_back(value);
                    ++index;
                    value = name[index + 4];
                }
                if (compare_animation_resource_type(type.c_str(), pool.type.c_str()) == 0)
                {
                    ExecutableResourceRecord* record = find_executable_resource_record(pool.records, name, hash);
                    if (record != nullptr)
                    {
                        return record;
                    }
                }
            }
            ++pool_index;
        } while (pool_index < pools.size());
    }
    return nullptr;
}

// Xbox 0x00034E90.
u32 find_executable_resource_provider(std::span<ExecutableResourcePool> pools, const char* name,
    const void*& resolved)
{
    ExecutableResourceRecord* record = find_executable_resource(pools, name, false);
    if (record != nullptr)
    {
        resolved = record;
        return 1;
    }
    return 0;
}

// Xbox 0x00034EC0.
bool load_executable_resource_provider(const void* resolved, AnimationLoadedResource& output)
{
    const auto& record = *static_cast<const ExecutableResourceRecord*>(resolved);
    output.gpu = record.gpu;
    output.cpu = record.cpu;
    output.cpu_storage = {};
    output.gpu_storage = {};
    return true;
}

// Xbox 0x00034E00.
void register_executable_resource_pool(std::vector<ExecutableResourcePool>& pools,
    std::span<ExecutableResourceRecord> records, const char* type, bool active)
{
    pools.push_back(ExecutableResourcePool{records, type, active});
    if (records.size() != 0)
    {
        size_t index = 0;
        do
        {
            records[index].hash = animation_resource_hash(records[index].name.c_str());
            ++index;
        } while (index < records.size());
    }
}

// Xbox 0x00107720. Host heap allocation is the 0x000F72D0 boundary;
// copying the recorded byte count translates 0x000F71F0.
ClonedExecutableResource clone_executable_resource(std::span<ExecutableResourcePool> pools,
    const char* name)
{
    if (name != nullptr && std::memcmp(name, "aid_", 4) == 0)
    {
        ExecutableResourceRecord* record = find_executable_resource(pools, name, true);
        if (record != nullptr && record->cpu.data() != nullptr)
        {
            const u16 size = read_u16(record->cpu, 0);
            ClonedExecutableResource result;
            result.bytes.reset(new (std::nothrow) u8[size]);
            if (result.bytes != nullptr)
            {
                std::memcpy(result.bytes.get(), record->cpu.data(), size);
                result.size = size;
            }
            return result;
        }
    }
    return {};
}
