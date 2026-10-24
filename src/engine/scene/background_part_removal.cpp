#include "background_part_removal.h"
#include "../navigation/grid_regions.h"

#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00044EF0
void remove_background_part_volume(Actor& actor, i32 row, u32 value,
    Vec3& position, BackgroundPartRemoval& context)
{
    auto& volume = context.volumes[row];
    const u32 zero = 0;
    if (volume.attachment != nullptr)
    {
        const auto& shape = volume.attachment->shape;
        position.x = read_f32(shape, 4);
        position.y = read_f32(shape, 8);
        position.z = read_f32(shape, 0xC);
        position.y = read_f32(shape, 0x14) * .5f + position.y;
        release_actor_live_attachment(*volume.attachment);
        volume.attachment = nullptr;
        std::memcpy(volume.fields.data() + 0x14, &zero, 4);
    }
    else
    {
        std::array<u8, 0x20> shape;
        u16 tag;
        if (build_background_animation_volume(actor, row, context.header, shape, tag))
        {
            position.x = read_f32(shape, 4);
            position.z = read_f32(shape, 0xC);
            position.y = read_f32(shape, 0x14) * .5f + read_f32(shape, 8);
        }
    }
    if (volume.anchor != nullptr)
    {
        release_actor_model_anchor(*volume.anchor);
        volume.anchor = nullptr;
        std::memcpy(volume.fields.data() + 0x18, &zero, 4);
    }
    const u16 tag = read_u16(volume.fields, 0x10);
    if (tag != 0)
    {
        entity_detail::set_all_navigation_regions_blocked(tag, 0);
    }
    const std::array<i32, 4> message{0xAB, std::bit_cast<i32>(context.attachments.actor_address),
        row, std::bit_cast<i32>(value)};
    broadcast_actor_pulse_message(message.data());
    PhysicalShapeEventContext events{0, 0, 0,
        [](const u8* event, void* user)
        {
            return clear_physical_shape_event_bits({const_cast<u8*>(event), 0x14}, u32(reinterpret_cast<uintptr_t>(user)));
        }, reinterpret_cast<void*>(uintptr_t(12)), context.shape_lists};
    visit_scene_script_shape_events(context.first_shape_object, u8(u32(row) + 1u), context.actor_of, events);
    auto& options = g_actor_creation_options;
    options.flags = 0;
    options.position = position;
    options.flags = 1;
    if (context.attachments.actor_address + 0x8Cu != 0)
    {
        dispatch_actor_attachment_records(context.attachment_records, 0xD, nullptr, &options, context.attachments, 0);
    }
}
}
