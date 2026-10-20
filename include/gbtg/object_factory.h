#pragma once

#include "gbtg/retail_field.h"
#include "gbtg/types.h"
#include <memory>

namespace gbtg
{
// The header every factory parameter block starts with (docs/RETAIL_LAYOUTS.md).
namespace object_parameter_field
{
constexpr RetailField<u16, 0x0> size{}; // 0x000873A0 writes its block's size, 0x1D8
constexpr RetailField<u16, 0x2> unk_2{}; // 0x000873A0 writes 0x6D; nothing here reads it
constexpr RetailField<u16, 0x4> type{}; // the factory type 0x00107520 creates
} // namespace object_parameter_field

struct ObjectSizes
{
    u32 instance;
    u32 parameters;
};
struct FactoryObject;
using ObjectConstructor = bool (*)(FactoryObject&, std::span<const u8>);
using ObjectDestructor = void (*)(FactoryObject&);
struct ObjectFactoryVtable
{
    u32 address; // original vtable address, host pointer stored separately
    void (*sizes)(ObjectSizes&); // +0
    ObjectConstructor construct; // +4
    ObjectDestructor destroy; // +8
};
struct ObjectFactoryType
{
    u16 parent; // DAT_0030D41C, stride 0xC
    const ObjectFactoryVtable* (*vtable)(); // DAT_0030D420
};
struct ObjectFactoryRegistry
{
    std::span<const ObjectFactoryType> types;
    u32 count = 0; // DAT_0050E0D0
};
struct FactoryObject
{
    std::unique_ptr<u8[]> bytes;
    u32 size = 0;
    const ObjectFactoryVtable* vtable = nullptr; // +0, host pointer mapping
    u16 type = 0; // +4
};

void button_bat_object_sizes(ObjectSizes& output); // Xbox 0x0006D480
void base_object_sizes(ObjectSizes& output); // Xbox 0x001074D0
bool initialize_base_object(FactoryObject& object,
    std::span<const u8> parameters); // Xbox 0x00107700
void actor_object_sizes(ObjectSizes& output); // Xbox 0x0006DB30
void destroy_factory_object(ObjectFactoryRegistry& registry, u16 type,
    FactoryObject& object, ObjectDestructor previous); // Xbox 0x001076B0
bool construct_factory_object(ObjectFactoryRegistry& registry, FactoryObject& object,
    u32 type, std::span<const u8> parameters); // Xbox 0x00107620
FactoryObject create_factory_object(ObjectFactoryRegistry& registry,
    std::span<const u8> parameters); // Xbox 0x00107520

} // namespace gbtg
