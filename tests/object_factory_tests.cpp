#include "gbtg/object_factory.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace gbtg;

namespace
{

std::vector<int> calls;
bool fail_child = false;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void sizes(ObjectSizes& output)
{
    output = ObjectSizes{19, 8};
}

bool parent_constructor(FactoryObject& object, std::span<const u8> parameters)
{
    require(object.type == read_u16(parameters, 4) && object.bytes[18] == 0,
            "constructor did not receive derived type or zeroed remainder byte");
    object.bytes[8] = 1;
    calls.push_back(1);
    return true;
}

bool child_constructor(FactoryObject& object, std::span<const u8>)
{
    require(object.bytes[8] == 1, "child constructor ran before its parent");
    object.bytes[9] = 2;
    calls.push_back(2);
    return !fail_child;
}

void parent_destructor(FactoryObject& object)
{
    require(object.bytes[8] == 1, "parent destructor lost parent state");
    calls.push_back(4);
    object.bytes[8] = 0;
}

void child_destructor(FactoryObject& object)
{
    require(object.bytes[9] == 2 && object.bytes[8] == 1,
            "child destructor ran after its parent or before child initialization");
    calls.push_back(3);
    object.bytes[9] = 0;
}

const ObjectFactoryVtable parent_vtable{0x100, sizes, parent_constructor, parent_destructor};
const ObjectFactoryVtable child_vtable{0x200, sizes, child_constructor, child_destructor};
const ObjectFactoryVtable shared_vtable{0x300, sizes, parent_constructor, parent_destructor};
const ObjectFactoryVtable null_vtable{0x400, sizes, nullptr, nullptr};

const ObjectFactoryVtable* parent_factory()
{
    return &parent_vtable;
}

const ObjectFactoryVtable* child_factory()
{
    return &child_vtable;
}

const ObjectFactoryVtable* shared_factory()
{
    return &shared_vtable;
}

const ObjectFactoryVtable* null_factory()
{
    return &null_vtable;
}

} // namespace

int main()
{
    try
    {
        const std::array<ObjectFactoryType, 5> types
        {
            ObjectFactoryType{0, nullptr}, ObjectFactoryType{0, parent_factory},
            ObjectFactoryType{1, child_factory}, ObjectFactoryType{1, shared_factory},
            ObjectFactoryType{0, null_factory}
        };
        ObjectFactoryRegistry registry{types};
        std::array<u8, 8> parameters{};
        u16 type = 2;
        std::memcpy(parameters.data() + 4, &type, 2);
        auto object = create_factory_object(registry, parameters);
        require(object.bytes != nullptr && object.size == 19 && registry.count == 1 &&
                read_u32(std::span<const u8>(object.bytes.get(), object.size), 0) == 0x200 &&
                calls == std::vector<int>{1, 2},
                "factory did not allocate the derived object and construct parents first");
        destroy_factory_object(registry, type, object, nullptr);
        require(calls == std::vector<int>{1, 2, 3, 4}, "destructor order differs");
        calls.clear();
        fail_child = true;
        auto failed = create_factory_object(registry, parameters);
        require(failed.bytes == nullptr && registry.count == 1 &&
                calls == std::vector<int>{1, 2, 3, 4},
                "failed construction did not roll back or incremented live count");
        calls.clear();
        type = 3;
        std::memcpy(parameters.data() + 4, &type, 2);
        auto shared = create_factory_object(registry, parameters);
        require(shared.bytes != nullptr && calls == std::vector<int>{1},
                "shared constructor was called twice");
        destroy_factory_object(registry, type, shared, nullptr);
        require(calls == std::vector<int>{1, 4}, "shared destructor was called twice");
        calls.clear();
        type = 4;
        std::memcpy(parameters.data() + 4, &type, 2);
        auto no_callbacks = create_factory_object(registry, parameters);
        destroy_factory_object(registry, type, no_callbacks, nullptr);
        require(no_callbacks.bytes != nullptr && calls.empty() && registry.count == 3,
                "null constructor/destructor entry changed allocation behavior");
        ObjectSizes retail_sizes{};
        button_bat_object_sizes(retail_sizes);
        require(retail_sizes.instance == 0x958 && retail_sizes.parameters == 0x340,
                "button bat sizes differ from executable");
        actor_object_sizes(retail_sizes);
        require(retail_sizes.instance == 0x934 && retail_sizes.parameters == 0x334,
                "actor sizes differ from executable");
        std::cout << "object factory lifecycle checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
