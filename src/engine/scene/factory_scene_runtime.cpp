#include "factory_scene_runtime.h"
#include "../entities/entity_heap.h"
#include "../entities/actor_factory_owner.h"
#include "../../gbtg/runtime/object_lifecycle.h"
#include <limits>
#include <utility>

namespace gbtg
{
namespace
{
template<size_t Type>
const ObjectFactoryVtable* missing_class()
{
    throw Error("scene factory type " + std::to_string(Type) + " is not translated/installed");
}

template<size_t... Types>
constexpr auto missing_getters(std::index_sequence<Types...>)
{
    return std::array<const ObjectFactoryVtable* (*)(), sizeof...(Types)>{missing_class<Types>...};
}
}

SceneFactoryRuntime::SceneFactoryRuntime(AidStore& aids, std::shared_ptr<SceneExecutableResources> image)
    : executable(std::move(image)), resources(aids)
{
    if (executable == nullptr)
    {
        throw Error("scene factory requires its executable resource owner");
    }
    const auto table = executable->memory(0x0030D41C, type_count * 12);
    constexpr auto getters = missing_getters(std::make_index_sequence<type_count>{});
    for (size_t type = 0; type < type_count; ++type)
    {
        const u32 parent = read_u32(table, type * 12);
        if (parent >= type_count)
        {
            throw Error("scene factory parent is outside the retail type table");
        }
        types[type] = {u16(parent), getters[type]};
        retail_getters[type] = read_u32(table, type * 12 + 4);
        retail_names[type] = read_u32(table, type * 12 + 8);
    }
    install_runtime_base_object_class(types);
}

SceneFactoryRuntime::~SceneFactoryRuntime()
{
    release_objects();
}

FactoryObject* SceneFactoryRuntime::keep_object(FactoryObject object)
{
    if (object.bytes == nullptr)
    {
        return nullptr;
    }
    bind_address({object.bytes.get(), object.size});
    objects.push_back(std::move(object));
    return &objects.back();
}

void SceneFactoryRuntime::release_objects()
{
    for (auto object = objects.rbegin(); object != objects.rend(); ++object)
    {
        if (object->bytes != nullptr)
        {
            const void* pointer = object->bytes.get();
            release_factory_object(factory, *object);
            unbind_address(pointer);
        }
    }
    objects.clear();
}

u32 SceneFactoryRuntime::bind_address(std::span<u8> bytes)
{
    if (bytes.empty() || bytes.data() == nullptr)
    {
        throw Error("cannot register an empty native instance");
    }
    const auto begin = reinterpret_cast<std::uintptr_t>(bytes.data());
    for (const auto& [address, existing] : addresses_)
    {
        const auto other = reinterpret_cast<std::uintptr_t>(existing.data());
        if (begin == other && bytes.size() == existing.size())
        {
            return address;
        }
        if (begin < other + existing.size() && other < begin + bytes.size())
        {
            throw Error("native address registration overlaps an existing instance");
        }
    }
    const u64 step = (u64(bytes.size()) + 15) & ~u64(15);
    if (step > std::numeric_limits<u32>::max() - u64(next_address_))
    {
        throw Error("native scene address token arena is exhausted");
    }
    const u32 address = next_address_;
    addresses_.emplace(address, bytes);
    next_address_ += u32(step);
    return address;
}

void SceneFactoryRuntime::unbind_address(const void* pointer)
{
    for (auto row = addresses_.begin(); row != addresses_.end(); ++row)
    {
        if (row->second.data() == pointer)
        {
            addresses_.erase(row);
            return;
        }
    }
    throw Error("native scene address release has no registered instance");
}

u32 SceneFactoryRuntime::address_of(const void* pointer) const
{
    if (pointer == nullptr)
    {
        return 0;
    }
    const auto value = reinterpret_cast<std::uintptr_t>(pointer);
    for (const auto& [address, bytes] : addresses_)
    {
        const auto begin = reinterpret_cast<std::uintptr_t>(bytes.data());
        if (value >= begin && value - begin < bytes.size())
        {
            return address + u32(value - begin);
        }
    }
    throw Error("native pointer has no scene address mapping");
}

std::span<u8> SceneFactoryRuntime::resolve_address(u32 address, size_t size) const
{
    auto row = addresses_.upper_bound(address);
    if (row == addresses_.begin())
    {
        throw Error("scene address is not registered");
    }
    --row;
    const size_t offset = address - row->first;
    if (offset > row->second.size() || size > row->second.size() - offset)
    {
        throw Error("scene address range exceeds its native instance");
    }
    return row->second.subspan(offset, size);
}

size_t SceneFactoryRuntime::address_count() const
{
    return addresses_.size();
}

void SceneFactoryRuntime::MarkerDeleter::operator()(u8* pointer) const
{
    engine_heap_free(pointer);
}

void SceneFactoryRuntime::load_markers(const char* scene_record)
{
    markers_.reset(load_runtime_scene_markers(scene_record, resources.registry, resources.error));
}

std::span<const u8> SceneFactoryRuntime::markers() const
{
    if (markers_ == nullptr)
    {
        return {};
    }
    return {markers_.get(), read_u32({markers_.get(), 8}, 4)};
}
}
