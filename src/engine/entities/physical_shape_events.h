#pragma once

#include "gbtg/types.h"
#include <functional>
#include <unordered_map>

namespace gbtg
{
struct PhysicalShapeEventList
{
    u32 count;
    const u8* records; // +4, native view of relocated address
};
struct PhysicalShapeEventContext
{
    u32 mask = 0;
    u8 category = 0;
    u8 subtype = 0;
    std::function<u32(const u8*, void*)> callback;
    void* user = nullptr;
    const std::unordered_map<const u8*, PhysicalShapeEventList>& lists;
};

u32 visit_physical_shape_event_list(const PhysicalShapeEventList& list,
    PhysicalShapeEventContext& context); // Xbox 0x000555C0
u32 visit_compound_shape_events(std::span<const u8> shape,
    PhysicalShapeEventContext& context); // Xbox 0x00054110
u32 visit_single_shape_events(std::span<const u8> shape,
    PhysicalShapeEventContext& context); // Xbox 0x00054DB0
void visit_model_physical_shape_events(std::span<const u8> shapes,
    PhysicalShapeEventContext& context); // Xbox 0x00055A30
void visit_scene_script_shape_events(u32 first, u8 category,
    const std::function<std::span<const u8>(u32)>& resolve,
    PhysicalShapeEventContext& context); // Xbox 0x0012C8A0
u32 set_physical_shape_event_bits(std::span<u8> event, u32 mask); // Xbox 0x0012C930
u32 clear_physical_shape_event_bits(std::span<u8> event, u32 mask); // Xbox 0x0012C950
}
