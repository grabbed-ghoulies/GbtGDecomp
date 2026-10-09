#include "background_volumes.h"
#include "../entities/shape_transform.h"
#include "gbtg/anim.h"

#include <cstring>
#include <bit>

namespace gbtg
{
// Xbox 0x00045D70
void initialize_background_animation_volume(Actor& actor, i32 index,
    std::span<const u8> animation_header, std::span<BackgroundAnimationVolume> records,
    entity_detail::ListHead& attachment_owner, entity_detail::ListHead& attachment_global,
    entity_detail::ListHead& anchor_owner, const Vec3& origin)
{
    std::array<u8, 0x20> shape;
    u16 tag;
    if (build_background_animation_volume(actor, index, animation_header, shape, tag))
    {
        auto& record = records[index];
        record.attachment = allocate_actor_live_attachment(actor, attachment_owner,
            attachment_global, 1, 0, 0, shape.data(), origin);
        const float height = read_f32(shape, 0x14) * std::bit_cast<float>(0x3F2A7EFAu);
        auto* anchor = allocate_actor_model_anchor(actor, anchor_owner, 2);
        anchor->local.x = read_f32(shape, 4);
        anchor->local.y = height + read_f32(shape, 8);
        anchor->local.z = read_f32(shape, 0xC);
        anchor->joint = 0xFFFF;
        update_actor_model_anchor(*anchor);
        record.anchor = anchor;
        std::memcpy(record.fields.data() + 0x10, &tag, sizeof(tag));
        if (record.anchor != nullptr)
        {
            record.anchor->radius = read_f32(shape, 0x1C);
        }
    }
}

// Xbox 0x00045F70
bool build_background_animation_volume(const Actor& actor, i32 index,
    std::span<const u8> animation_header, std::array<u8, 0x20>& output, u16& tag)
{
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        u32 current = 0;
        const u32 count = read_u32(cpu, 4);
        if (count != 0)
        {
            const u32 table = read_u32(cpu, 0);
            while (read_u32(cpu, table + current * 8) != 4)
            {
                ++current;
                if (count <= current)
                {
                    return false;
                }
            }
            const u32 block = read_u32(cpu, table + current * 8 + 4);
            if (block != 0)
            {
                const EntityBonePose* pose = actor.model->alternate_shape_pose;
                if (pose == nullptr)
                {
                    pose = actor.model->primary_shape_pose;
                }
                auto shape = cpu.subspan(block + 8);
                bool found = false;
                Vec3 low;
                Vec3 high;
                do
                {
                    if ((shape[4] & 0x40) != 0 && shape[0] == 3 && i32(read_u32(shape, 0x20)) == index)
                    {
                        EntityShapeNode node;
                        std::memcpy(&node, shape.data(), 0x20);
                        Vec3 center;
                        const float radius = shape_sphere_metrics(node, actor.xform, pose, center);
                        if (found)
                        {
                            // COMISS/JA updates the low endpoint for unordered comparisons too.
                            if (!(center.x - radius > low.x))
                            {
                                low.x = center.x - radius;
                            }
                            if (!(center.y - radius > low.y))
                            {
                                low.y = center.y - radius;
                            }
                            if (!(center.z - radius > low.z))
                            {
                                low.z = center.z - radius;
                            }
                            if (!(high.x >= center.x + radius))
                            {
                                high.x = center.x + radius;
                            }
                            if (!(high.y >= center.y + radius))
                            {
                                high.y = center.y + radius;
                            }
                            if (!(high.z >= center.z + radius))
                            {
                                high.z = center.z + radius;
                            }
                        }
                        else
                        {
                            low.y = center.y - radius;
                            low.z = center.z - radius;
                            high.x = center.x + radius;
                            high.y = center.y + radius;
                            low.x = center.x - radius;
                            found = true;
                            high.z = center.z + radius;
                        }
                    }
                    if (shape[1] == 0)
                    {
                        break;
                    }
                    shape = shape.subspan(shape[1]);
                }
                while (shape.data() != nullptr);
                if (found)
                {
                    tag = read_u16(animation_header, 0x1C + index * 0xC);
                    if (tag != 0 && (tag < 3000 || 0xC37 < tag))
                    {
                        tag = 0;
                    }
                    const float x = (high.x - low.x) * 0.5f + low.x;
                    const float z = (high.z - low.z) * 0.5f + low.z;
                    float radius = 0.f;
                    float absolute = animation_absolute(x - low.x);
                    if (0.f < absolute)
                    {
                        radius = absolute;
                    }
                    absolute = animation_absolute(z - low.z);
                    if (radius < absolute)
                    {
                        radius = absolute;
                    }
                    absolute = animation_absolute(x - high.x);
                    if (radius < absolute)
                    {
                        radius = absolute;
                    }
                    absolute = animation_absolute(z - high.z);
                    if (absolute <= radius)
                    {
                        absolute = radius;
                    }
                    auto store = [&](size_t offset, float value)
                    {
                        std::memcpy(output.data() + offset, &value, sizeof(value));
                    };
                    output[0] = 4;
                    output[1] = 0x20;
                    output[2] = 1;
                    store(4, x);
                    store(0xC, z);
                    store(8, low.y);
                    store(0x10, 0.f);
                    store(0x14, high.y - low.y);
                    store(0x18, 0.f);
                    store(0x1C, absolute);
                    return true;
                }
                return false;
            }
        }
    }
    return false;
}
}
