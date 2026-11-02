#pragma once

#include "gbtg/retail_field.h"
#include "gbtg/types.h"
#include <array>
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

// The actor's parameter block (type 2, 0x334 bytes), which every actor type's parameters extend. 0x0003E940 reads
// it; the strings are AIDs when they start "aid_", and the defaults (0x000411A0) name the default object.
namespace actor_parameter_field
{
constexpr RetailField<Vec3, 0x08> position{};
constexpr RetailField<Vec3, 0x14> euler{};
constexpr RetailField<float, 0x20> scale{};
constexpr RetailField<float, 0x24> model_scale{}; // the actor's +0x200
constexpr RetailField<std::array<char, 0x80>, 0x28> model{};
constexpr RetailField<std::array<char, 0x80>, 0xA8> animation{}; // the first animation
constexpr RetailField<std::array<char, 0x80>, 0x128> animation_table{};
constexpr RetailField<std::array<char, 0x80>, 0x1A8> resource{};
constexpr RetailField<std::array<char, 0x80>, 0x228> attachment{};
constexpr RetailField<u16, 0x2A8> attachment_bone{}; // 0xFFFF for none
constexpr RetailField<u16, 0x2AA> marker_id{}; // the marker that placed it (the actor's +0x1FC)
constexpr RetailField<u16, 0x2AC> next_marker{}; // (the actor's +0x1FE)
constexpr RetailField<u32, 0x2B0> secondary_mode{};
constexpr RetailField<std::array<char, 0x80>, 0x2B4> secondary_model{};
constexpr u32 kSize = 0x334;
static_assert(retail_fits(kSize, secondary_model));
} // namespace actor_parameter_field

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
