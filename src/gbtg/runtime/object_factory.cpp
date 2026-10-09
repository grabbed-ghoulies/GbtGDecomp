#include "gbtg/object_factory.h"
#include "object_lifecycle.h"
#include <cstring>
#include <new>

namespace gbtg
{

// Xbox 0x001074D0
void base_object_sizes(ObjectSizes& output)
{
    output.instance = 8;
    output.parameters = 8;
}

// Xbox 0x00107700
bool initialize_base_object(FactoryObject& object, std::span<const u8> parameters)
{
    std::memcpy(object.bytes.get() + 6, parameters.data() + 2, 2);
    return true;
}

// Xbox 0x0006D480.
void button_bat_object_sizes(ObjectSizes& output)
{
    output.instance = 0x958;
    output.parameters = 0x340;
}

// Xbox 0x0006DB30.
void actor_object_sizes(ObjectSizes& output)
{
    output.instance = 0x934;
    output.parameters = 0x334;
}

// Xbox 0x001076B0.
void destroy_factory_object(ObjectFactoryRegistry& registry, u16 type,
    FactoryObject& object, ObjectDestructor previous)
{
    while (true)
    {
        const ObjectFactoryVtable* vtable = registry.types[type].vtable();
        const ObjectDestructor destructor = vtable->destroy;
        if (destructor != nullptr && previous != destructor)
        {
            destructor(object);
        }
        type = registry.types[type].parent;
        if (type == 0)
        {
            break;
        }
        previous = vtable->destroy;
    }
}

// Xbox 0x001075C0
void release_factory_object(ObjectFactoryRegistry& registry, FactoryObject& object)
{
    const u16 type = read_u16(std::span<const u8>(object.bytes.get(), object.size), 4);
    destroy_factory_object(registry, type, object, nullptr);
    object.bytes.reset(); // Host heap and thread heap selection boundary.
    --registry.count;
}

// Xbox 0x00107620.
bool construct_factory_object(ObjectFactoryRegistry& registry, FactoryObject& object,
    u32 type, std::span<const u8> parameters)
{
    const size_t index = type & 0xffff;
    const u16 parent = registry.types[index].parent;
    ObjectConstructor previous;
    if (parent == 0)
    {
        previous = nullptr;
    }
    else
    {
        if (!construct_factory_object(registry, object, parent, parameters))
        {
            return false;
        }
        const ObjectFactoryVtable* parent_vtable = registry.types[parent].vtable();
        previous = parent_vtable->construct;
    }
    const ObjectFactoryVtable* vtable = registry.types[index].vtable();
    const ObjectConstructor constructor = vtable->construct;
    if (constructor != nullptr && previous != constructor)
    {
        if (!constructor(object, parameters))
        {
            destroy_factory_object(registry, u16(type), object, nullptr);
            return false;
        }
    }
    return true;
}

// Xbox 0x00107520. Host heap allocation is the 0x000F72D0 boundary.
FactoryObject create_factory_object(ObjectFactoryRegistry& registry,
    std::span<const u8> parameters)
{
    const u16 type = read_u16(parameters, 4);
    const ObjectFactoryVtable* vtable = registry.types[type].vtable();
    ObjectSizes sizes{0, 0};
    vtable->sizes(sizes);
    FactoryObject object;
    object.bytes.reset(new (std::nothrow) u8[sizes.instance]);
    if (object.bytes != nullptr)
    {
        std::memset(object.bytes.get(), 0, sizes.instance);
        object.size = sizes.instance;
        object.vtable = vtable;
        object.type = type;
        std::memcpy(object.bytes.get(), &vtable->address, 4);
        std::memcpy(object.bytes.get() + 4, &type, 2);
        if (construct_factory_object(registry, object, type, parameters))
        {
            ++registry.count;
            return object;
        }
        object.bytes.reset();
    }
    return {};
}

} // namespace gbtg
