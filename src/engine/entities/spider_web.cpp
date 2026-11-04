#include "spider_web.h"
#include "actor_render_registration.h"
#include "actor_transform.h"
#include "actor_world_lists.h"
#include "entity.h"
#include "entity_collision.h"
#include "player_mind_requests.h"
#include "../camera/camera_body_slots.h"
#include "../camera/entity_camera.h"
#include "../graphics/vector_normalize.h"
#include "gbtg/anim.h"
#include "gbtg/entity.h"
#include "gbtg/model_runtime.h"
#include <array>
#include <bit>

namespace gbtg
{
namespace
{
SpiderWebState& fields_of(std::span<u8> bytes)
{
    return actor_record_view<SpiderWebState>(bytes);
}

const SpiderWebState& fields_of(std::span<const u8> bytes)
{
    return actor_record_view<SpiderWebState>(bytes);
}


// DAT_00427430: how far a new web sits off its surface; each web placed raises it by 0.0375 up to 0.5, then it
// starts again at 0.2, so overlapping webs do not fight.
float g_spider_web_lift = 0.2f;

// The (x, z) offsets of the four probes around the hit (DAT_002FFDC8).
constexpr std::array<std::array<float, 2>, 4> kProbeOffsets{{{5.f, 5.f}, {-5.f, 5.f}, {5.f, -5.f}, {-5.f, -5.f}}};

// The inlined 0x0003AAA0/0x0003A9E0 pair: the named clip, when the actor has an animation table.
void start_named_animation(RuntimeSpiderWeb& context, const char* name)
{
    if (context.animation.table == nullptr)
    {
        return;
    }
    const AnimationRecord* record = find_actor_animation(context.animation.table, name, nullptr, context.random);
    if (record != nullptr)
    {
        start_actor_animation(context.animation, *record, context.registry, context.error, context.random);
    }
}

// Xbox 0x0003D820, EAX actor: the model's record of kind 5 (its size), or 0 without one.
u32 actor_model_size_record(const Actor& actor)
{
    if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
    {
        return 0;
    }
    const auto cpu = actor.model->cpu;
    const u32 count = read_u32(cpu, 4);
    const u32 table = read_u32(cpu, 0);
    for (u32 index = 0; index < count; ++index)
    {
        if (read_u32(cpu, table + index * 8) == 5)
        {
            return read_u32(cpu, table + index * 8 + 4);
        }
    }
    return 0;
}

Vec3 read_vec3(std::span<const u8> bytes, size_t offset)
{
    return Vec3{read_f32(bytes, offset), read_f32(bytes, offset + 4), read_f32(bytes, offset + 8)};
}
}

// Xbox 0x000704B0
void spider_web_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = sizeof(SpiderWebState);
    sizes.parameters = spider_web_parameter::kSize;
}

// Xbox 0x00070510
bool initialize_runtime_spider_web(RuntimeSpiderWeb& context)
{
    Actor& actor = context.actor;
    const float scale = 3.f; // DAT_0032B260.
    if (actor.scale_200 != scale)
    {
        actor.scale_200 = scale; // +0x200.
        actor.xform_valid = 0; // +0x2A8.
    }
    i32 joint = -1;
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const u32 found = actor_lookup_joint_index(actor.model->cpu, 0) & 0xFF; // 0x00052FC0
        if (found != 0xFF)
        {
            joint = i32(found);
        }
    }
    fields_of(context.bytes).joint = joint;
    fields_of(context.bytes).flat = 1u;
    fields_of(context.bytes).state = 0u;
    start_named_animation(context, "expand"); // 0x0031C3E0
    return true;
}

// Xbox 0x000705B0
void update_runtime_spider_web(RuntimeSpiderWeb& context, RuntimeSpiderWebUpdate& update)
{
    update.update_actor(); // 0x000420D0
    const u32 state = fields_of(context.bytes).state;
    if (state == 0)
    {
        snare_runtime_spider_web_players(context, update.players()); // 0x00070780
        if (runtime_actor_animation_slot_complete(context.animation, 0)) // 0x0003B290, ECX 0
        {
            set_runtime_spider_web_state(context, fields_of(context.bytes).flat == 0 ? 2u : 1u); // 0x00070720
        }
        return;
    }
    if (state == 1)
    {
        snare_runtime_spider_web_players(context, update.players());
        const float step = runtime_frame_step(update.clock);
        const float rate = runtime_actor_rate(context.actor); // +0x930, +0x92C
        bool ended = false;
        const float timer = fields_of(context.bytes).timer;
        if (timer > 0.f)
        {
            const float left = timer - rate * step;
            fields_of(context.bytes).timer = left;
            if (!(0.f < left))
            {
                fields_of(context.bytes).timer = 0.f;
                ended = true;
            }
        }
        if (!ended && runtime_decay_enabled(update.clock)) // DAT_0050E768, DAT_0030F144[DAT_0051248C]
        {
            return;
        }
        fields_of(context.bytes).state = 2u;
        start_named_animation(context, "shrink"); // 0x0031C3D8
        return;
    }
    if (state == 2)
    {
        snare_runtime_spider_web_players(context, update.players());
        if (runtime_actor_animation_slot_complete(context.animation, 0))
        {
            activate_actor_bind_tree(context.actor); // 0x00112CC0
        }
    }
}

// Xbox 0x000706E0
void dispatch_runtime_spider_web_message(RuntimeSpiderWeb& context, std::span<u32> message,
    RuntimeSpiderWebMessage& handlers)
{
    handlers.base_message(message); // 0x0003EF70
    if (message[0] == 0x23)
    {
        unregister_actor_segment_probes(handlers.render); // 0x0003E850
        clear_actor_shape_probe(context.actor, handlers.shape); // 0x0003C700
    }
    else if (message[0] == 0x7A)
    {
        place_runtime_spider_web(context, message, handlers.memory_at); // 0x000709F0
    }
}

// Xbox 0x00070720
void set_runtime_spider_web_state(RuntimeSpiderWeb& context, u32 state)
{
    fields_of(context.bytes).state = state;
    if (state == 0)
    {
        start_named_animation(context, "expand"); // 0x0031C3E0
    }
    else if (state == 1)
    {
        fields_of(context.bytes).timer = 5.f; // DAT_0032B184
    }
    else if (state == 2)
    {
        start_named_animation(context, "shrink"); // 0x0031C3D8
    }
}

// Xbox 0x00070780
void snare_runtime_spider_web_players(RuntimeSpiderWeb& context,
    const std::vector<RuntimeSegmentContactActor*>& players)
{
    const i32 joint = fields_of(context.bytes).joint;
    if (joint == -1 || fields_of(context.bytes).flat == 0)
    {
        return;
    }
    Actor& actor = context.actor;
    // The joint's matrix on the model's selected pose (0x00031530 over +0x168, else +0x164).
    const RuntimeModelInstance* model = actor.model == nullptr ? nullptr : actor.model->runtime;
    const ModelPoseBuffer* pose = model == nullptr ? nullptr
        : model->alternate != nullptr ? model->alternate : model->primary;
    if (pose == nullptr)
    {
        throw Error("0x00031530 reads the spider web's joint without a model pose");
    }
    const auto& matrix = model_pose_matrix(*pose, joint);
    const float squared = matrix[8] * matrix[8] + matrix[4] * matrix[4] + matrix[0] * matrix[0];
    float length = 0.f;
    if (squared != 0.f)
    {
        length = float(retail_square_root(squared)); // 0x00152B30
    }
    const u32 size_record = actor_model_size_record(actor); // 0x0003D820
    if (size_record == 0)
    {
        throw Error("0x0003D820 finds no size record on the spider web's model");
    }
    const float reach = float(engine_absolute(read_f32(actor.model->cpu, size_record)) * length
        * 0.7f); // 0x00152BB0, DAT_0032B264
    Vec3 unused{};
    actor_transform_joint_point(actor, u16(joint), actor_transform_origin, unused); // 0x0003AEB0, a dead result
    const float reach_squared = reach * reach;
    std::array<u32, 0x42> notice{};
    bool built = false;
    const float* inverse = actor.xform_inv; // +0x2DC.
    for (RuntimeSegmentContactActor* player : players)
    {
        const Vec3 p = actor_world_position(player->actor); // 0x000412A0, ESI node - 0x934
        const float x = inverse[0] * p.x + inverse[1] * p.y + inverse[2] * p.z + inverse[3];
        const float y = inverse[4] * p.x + inverse[5] * p.y + inverse[6] * p.z + inverse[7];
        const float z = inverse[9] * p.y + inverse[10] * p.z + p.x * inverse[8] + inverse[11];
        const bool within = reach_squared > z * z + x * x;
        const bool level = 1.f > engine_absolute(y); // DAT_0032B0F8
        if (within && level)
        {
            if (!built)
            {
                notice[0] = 0x4F;
                notice[1] = 0xC;
                built = true;
            }
            player->message(notice.data()); // Vtable +0xC.
        }
    }
}

// Xbox 0x000709F0
void place_runtime_spider_web(RuntimeSpiderWeb& context, std::span<const u32> message,
    const std::function<std::span<const u8>(u32)>& memory_at)
{
    Vec3 direction = read_vec3(memory_at(message[2]), 0); // Message +8, the spit's direction.
    const float squared = direction.z * direction.z + direction.y * direction.y + direction.x * direction.x;
    if (squared != 0.f) // DAT_0032B100
    {
        const float scale = engine_reciprocal_sqrt(squared) * 10.f; // 0x001534C0, DAT_0032B10C
        direction = Vec3{direction.x * scale, direction.y * scale, direction.z * scale};
    }
    Actor& actor = context.actor;
    const Vec3 position = actor_world_position(actor); // 0x000412A0, ESI web
    const float back = -1.f; // DAT_0032B120.
    Vec3 hit{position.x + direction.x, position.y + direction.y, position.z + direction.z};
    Vec3 from{position.x + direction.x * back, position.y + direction.y * back, position.z + direction.z * back};
    if (entity_record_sweep_probe(hit, from, 0xC, 0) == 0) // 0x00122480, EBX hit
    {
        // Nothing along the spit: straight down from 2.5 above to 5 below.
        hit = Vec3{position.x, position.y - 5.f, position.z}; // DAT_0032B184
        from = Vec3{position.x, position.y + 2.5f, position.z}; // DAT_0032B20C
        if (entity_record_sweep_probe(hit, from, 0xC, 0) == 0)
        {
            return;
        }
    }
    const u8* record = entity_detail::g_segment_slots[0].pool.begin; // DAT_00511A20 + 4.
    Vec3 normal = read_vec3({record, 0x54}, 0xC);
    const float upright = 0.9f; // DAT_0032B174.
    fields_of(context.bytes).flat = upright < normal.y ? 1u : 0u;
    float top = hit.y;
    if (fields_of(context.bytes).flat != 0 || (record[0x20] & 8) != 0)
    {
        // Four probes straight down around the hit, 5 above to 5 below; the surface's normal is the average of
        // the normals found, only the upright ones once two are, and without the first hit's once three are.
        Vec3 sum = normal;
        i32 count = 1;
        i32 upward = 0;
        i32 hits = 0;
        std::array<Vec3, 4> samples{};
        std::array<Vec3, 4> normals{};
        std::array<u32, 4> up{};
        for (const auto& offset : kProbeOffsets)
        {
            Vec3& sample = samples[size_t(hits)];
            sample = Vec3{hit.x + offset[0], hit.y, hit.z + offset[1]};
            Vec3 above{sample.x, hit.y + 5.f, sample.z};
            sample.y = sample.y - 5.f;
            if (entity_record_sweep_probe(sample, above, 8, 0) == 1)
            {
                const u8* found = entity_detail::g_segment_slots[0].pool.begin;
                const Vec3 n = read_vec3({found, 0x54}, 0xC);
                normals[size_t(hits)] = n;
                sum = Vec3{n.x + sum.x, n.y + sum.y, sum.z + n.z};
                up[size_t(hits)] = upright < n.y ? 1u : 0u;
                if (up[size_t(hits)] != 0)
                {
                    ++upward;
                }
                ++hits;
                ++count;
            }
        }
        if (hits >= 2)
        {
            if (upward >= 2)
            {
                sum = Vec3{};
                for (i32 index = 0; index < hits; ++index)
                {
                    if (up[size_t(index)] != 0)
                    {
                        sum = Vec3{normals[size_t(index)].x + sum.x, normals[size_t(index)].y + sum.y,
                            normals[size_t(index)].z + sum.z};
                    }
                }
                count = upward;
            }
            else if (hits > 2)
            {
                sum = Vec3{sum.x - normal.x, sum.y - normal.y, sum.z - normal.z};
                --count;
            }
        }
        const float share = 1.f / float(count); // DAT_0032B0F8
        const Vec3 average{share * sum.x, sum.y * share, sum.z * share};
        normalize_runtime_graphics_vector(normal, average); // D3DXVec3Normalize, 0x00184F99
        for (i32 index = 0; index < hits; ++index)
        {
            if (samples[size_t(index)].y > top)
            {
                top = samples[size_t(index)].y;
            }
        }
        fields_of(context.bytes).flat = upright < normal.y ? 1u : 0u;
    }
    const float lift = g_spider_web_lift;
    const Vec3 placed{normal.x * lift + hit.x, lift * normal.y + top, normal.z * lift + hit.z};
    float yaw = 0.f;
    float pitch = 0.f;
    direction_to_yaw_pitch(normal.x, normal.y, normal.z, yaw, pitch); // 0x00105E20, ECX yaw, stack pitch
    const Vec3 angles{pitch + std::bit_cast<float>(0x3FC90FDBu), yaw, 0.f}; // DAT_0032B140, pi / 2.
    actor_set_world_position(actor, placed); // The inlined 0x00041730.
    actor.xform_valid = 0; // +0x2A8.
    wrap_runtime_euler(angles, actor.euler); // 0x00105AA0, ESI angles, EDI actor +0x14
    actor.xform_valid = 0;
    g_spider_web_lift = lift + 0.0375f; // DAT_0032B498
    if (g_spider_web_lift >= 0.5f) // DAT_0032B0F4
    {
        g_spider_web_lift = 0.2f; // DAT_0032B1AC
    }
}
}
