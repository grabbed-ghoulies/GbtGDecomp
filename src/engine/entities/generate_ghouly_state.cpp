#include "generate_ghouly_state.h"
#include <cstddef>
#include "actor_render_registration.h"
#include "actor_runtime_queries.h"
#include "actor_scare_requests.h"
#include "actor_shape_bounds.h"
#include "actor_shape_push.h"
#include "actor_template_creation.h"
#include "actor_trail_requests.h"
#include "actor_transform.h"
#include "body_damage_motion.h"
#include "body_death_drops.h"
#include "body_orientation.h"
#include "entity_heap.h"
#include "fx_custom_electric.h"
#include "player_mind_requests.h"
#include "shape_transform.h"
#include "../camera/runtime_display_setup.h"
#include "../frontend/dialogue_draw_context.h"
#include "../graphics/world_sprite_passes.h"
#include "../navigation/player_avoidance.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include "player_fields.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <memory>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace parameter = generate_ghouly_parameter;

constexpr float kTurn = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
constexpr std::array<float, 2> kBoltAngles{std::bit_cast<float>(0xBFC90FDBu),
    std::bit_cast<float>(0x3FC90FDBu)}; // UNK_003028C8: a quarter turn either side.
constexpr std::array<float, 3> kBeamFadeTint{1.f, 100.f, 1.f}; // 0x003028F8.

// The host halves of the words the state keeps as pointers: the bolt pools and shape records (+0x32C rows), the
// textures (+0x348 / +0x34C) and the light record (+0x494's slot).
struct GenerateGhoulyInstance
{
    std::array<GhoulySpawnPool*, 2> pools{};
    std::array<const u8*, 2> shapes{};
    std::span<const u8> bolt_texture{};
    std::span<const u8> spark_texture{};
    std::unique_ptr<std::array<u8, 0x98>> light;
    std::array<const AnimationRecord*, 3> animations{}; // +0x20, +0x24, +0x28.
};
std::unordered_map<const u8*, GenerateGhoulyInstance> generate_ghouly_instances;
std::function<RuntimeGenerateGhoulyContext&(u32)> generate_ghouly_context_of;


GenerateGhoulyInstance& instance_of(const GenerateGhoulyState& state)
{
    return generate_ghouly_instances[body_state_key(state)];
}

u32& deferred(RuntimeDialogueDrawContext& c, u32 address)
{
    return c.deferred[(address - 0x181968) / 4];
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

float random_fraction(u32 value)
{
    return std::bit_cast<float>((value & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
}

u32 next_random(u32 value)
{
    return value * 0x19660Du + 0x3C6EF35Fu;
}

template <typename From>
void copy_name(char* target, std::span<const u8> parameters, From from)
{
    const char* source = field_data(parameters, from);
    size_t index = 0;
    do
    {
        target[index] = source[index];
    }
    while (source[index++] != 0);
}

bool aid_name(const char* name)
{
    u32 prefix;
    std::memcpy(&prefix, name, 4);
    return prefix == 0x5F646961; // "aid_"
}

// Body +0x30 and the four +0x309..+0x30C counters the constructor raises and the destructor drops.
void adjust_body_counters(std::span<u8> body, i32 step)
{
    body_record(body).move_holds = u16(body_record(body).move_holds + step);
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + step);
    }
}

// Each record on the actor's +0x8F4 list (actor +0x8F8 is the first) gains or drops a +0x15 hold.
void adjust_live_attachment_holds(entity_detail::ListHead& live_attachments, i32 step)
{
    for (auto* live = static_cast<ActorLiveAttachment*>(live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->hold = u8(live->hold + step);
    }
}

// Phase 2's holds, which 0x000941A0 (and the destructor and 0x00093EC0's inline copy) drop when phase 2 ends.
void drop_release_holds(GenerateGhoulyState& state, RuntimeGenerateGhoulyContext& context)
{
    if (state.phase == 2)
    {
        adjust_live_attachment_holds(context.live_attachments, -1);
        body_record(context.body).unk_6D = u8(body_record(context.body).unk_6D - 1);
        body_record(context.body).unk_7E5 = u8(body_record(context.body).unk_7E5 - 1);
    }
}

// 0x0003AAA0 with the attached actor's +0xDB0 variant, inlined as 0x0001AED0 does.
void find_animation(GenerateGhoulyState& state, size_t index, u32& found_word, u32 name,
    RuntimeGenerateGhoulyContext& context)
{
    const AnimationRecord* record = find_runtime_body_animation(context.image_string(name), context.actions);
    instance_of(state).animations[index] = record;
    found_word = record != nullptr ? 1u : 0u;
}

// 0x0001AFA0 (ESI body, ECX the record word).
void start_animation(GenerateGhoulyState& state, size_t index, RuntimeGenerateGhoulyContext& context)
{
    const AnimationRecord* record = instance_of(state).animations[index];
    if (record == nullptr)
    {
        throw Error("0x0001AFA0 reads +0x117 of a generate ghouly animation record that 0x0003AAA0 did not find");
    }
    start_runtime_body_animation_record(*record, context.actions);
}

Actor& actor_at(u32 address, RuntimeGenerateGhoulyContext& context)
{
    Actor* actor = context.actor_at(address);
    if (actor == nullptr)
    {
        throw Error("generate ghouly state actor " + std::to_string(address) + " outside the scene");
    }
    return *actor;
}

void set_scale(Actor& actor, float x, float y, float z)
{
    if (x != actor.scale_x || y != actor.radius_y || z != actor.radius_xz)
    {
        actor.scale_x = x;
        actor.radius_y = y;
        actor.radius_xz = z;
        actor.xform_valid = 0; // Actor +0x2A8.
    }
}

Vec3 scale_of(const Actor& actor)
{
    return Vec3{actor.scale_x, actor.radius_y, actor.radius_xz};
}

// Xbox 0x00092F80, EDI the row, ESI the point: the point and a random spin join the pool, which grows by its
// +2 count when full (0x000662A0). 0x00092840 inlines the same push.
void push_bolt_point(GhoulySpawnPool*& pool, const Vec3& point, u32& random)
{
    if (pool->cursor == pool->limit)
    {
        pool = grow_runtime_shape_pool(pool); // 0x000662A0
    }
    u8* row = pool->cursor;
    pool->cursor += pool->stride;
    std::memcpy(row, &point, 12);
    random = next_random(random);
    const float spin = random_fraction(random) * kTurn;
    std::memcpy(row + 0xC, &spin, 4);
}

// Xbox 0x00092D50: the segment splits at a random point between 40% and 60% along it, pushed off by up to a quarter
// of its length and swung on a small circle, until the pieces are shorter than 5.
void split_bolt(GhoulySpawnPool*& pool, const Vec3& from, const Vec3& to, float heading, u32& random)
{
    const long double dx = static_cast<long double>(from.x) - to.x;
    const long double dy = static_cast<long double>(from.y) - to.y;
    const long double dz = static_cast<long double>(from.z) - to.z;
    const float length = float(std::sqrt(dy * dy + dz * dz + dx * dx));
    if (5.f > length) // DAT_0032B184.
    {
        return;
    }
    const u32 first = next_random(random);
    const u32 second = next_random(first);
    const u32 third = next_random(second);
    random = next_random(third);
    const float along = random_fraction(first) * std::bit_cast<float>(0x3E4CCCCEu)
        + std::bit_cast<float>(0x3ECCCCCDu); // DAT_0032B480, DAT_0032B11C.
    const float quarter = length * 0.25f; // DAT_0032B104.
    const float spread = (0.f - quarter) - quarter;
    Vec3 middle;
    middle.x = random_fraction(second) * spread + ((to.x - from.x) * along + from.x) + quarter;
    middle.y = random_fraction(third) * spread + ((to.y - from.y) * along + from.y) + quarter;
    middle.z = random_fraction(random) * spread + ((to.z - from.z) * along + from.z) + quarter;
    const float angle = quarter * 0.1f; // DAT_0032B254.
    const float radius = quarter * 0.3f; // DAT_0032B268.
    middle.x = float(retail_sin(angle)) * radius + middle.x; // 0x00153320
    middle.z = float(retail_cos(angle)) * radius + middle.z; // 0x00153230
    split_bolt(pool, from, middle, heading, random);
    push_bolt_point(pool, middle, random); // 0x00092F80
    split_bolt(pool, middle, to, heading, random);
}

// Xbox 0x000925C0: the two bolt rows (each the next kind-2 record of the actor's model block 2 and a pool of
// 10 points) and the bolt and spark textures.
void start_bolts(GenerateGhoulyState& state, RuntimeGenerateGhoulyContext& context)
{
    GenerateGhoulyInstance& instance = instance_of(state);
    const u8* previous = nullptr;
    for (size_t row = 0; row < 2; ++row)
    {
        const u8* found = previous;
        const u8* block = runtime_actor_model_block(context.actor, 2); // Actor +0x204's kind-2 block.
        if (block != nullptr)
        {
            const u8* end = block + 4 + read_u32({block, 4}, 0) * 0x34;
            const u8* record = previous == nullptr ? block + 4 : previous + 0x34;
            for (; record < end; record += 0x34)
            {
                if (read_u16({record, 2}, 0) == 2)
                {
                    found = record;
                    break;
                }
            }
        }
        previous = found;
        // 0x0014E872 on the current heap (DAT_00507808) for 0xB4 bytes with its 0x1234 tag: a 0x10 header and 10
        // rows of 0x10.
        auto* pool = static_cast<GhoulySpawnPool*>(engine_heap_allocate(i32(sizeof(GhoulySpawnPool) + 10 * 0x10)));
        if (pool == nullptr)
        {
            throw Error("0x000925C0 writes the header of a bolt pool the heap did not give");
        }
        pool->begin = reinterpret_cast<u8*>(pool + 1);
        pool->cursor = pool->begin;
        pool->limit = pool->begin + 10 * 0x10;
        pool->stride = 0x10;
        pool->grow = 10;
        instance.pools[row] = pool;
        instance.shapes[row] = found;
        state.bolt_rows[row].pool = 1;
        state.bolt_rows[row].shape = found != nullptr ? 1u : 0u;
        state.bolt_rows[row].angle = kBoltAngles[row];
    }
    const char* bolt = state.bolt_texture_name.data();
    instance.bolt_texture = {};
    if (aid_name(bolt))
    {
        instance.bolt_texture = acquire_animation_resource(context.scene.registry, bolt); // 0x00035E90
        if (instance.bolt_texture.data() == nullptr)
        {
            resource_error(context.scene.error, 2); // 0x00032700
        }
    }
    state.bolt_texture = instance.bolt_texture.data() != nullptr ? 1u : 0u;
    const char* spark = state.spark_texture_name.data();
    instance.spark_texture = {};
    if (aid_name(spark))
    {
        instance.spark_texture = acquire_animation_resource(context.scene.registry, spark); // 0x00035E90
        if (instance.spark_texture.data() == nullptr)
        {
            resource_error(context.scene.error, 2); // 0x00032700
        }
    }
    state.spark_texture = instance.spark_texture.data() != nullptr ? 1u : 0u;
}

// Xbox 0x00092790: the pools (0x000663E0 inlined: the block's heap by 0x000F7100, then HeapFree) and the textures.
void release_bolts(GenerateGhoulyState& state, RuntimeGenerateGhoulyContext& context)
{
    GenerateGhoulyInstance& instance = instance_of(state);
    for (size_t row = 0; row < 2; ++row)
    {
        if (state.bolt_rows[row].pool != 0)
        {
            engine_heap_free(instance.pools[row]);
            instance.pools[row] = nullptr;
            state.bolt_rows[row].pool = 0;
        }
    }
    if (state.bolt_texture != 0)
    {
        release_animation_resource(context.scene.registry, instance.bolt_texture); // 0x00035C00
        instance.bolt_texture = {};
        state.bolt_texture = 0u;
    }
    if (state.spark_texture != 0)
    {
        release_animation_resource(context.scene.registry, instance.spark_texture); // 0x00035C00
        instance.spark_texture = {};
        state.spark_texture = 0u;
    }
}

// Xbox 0x00092840: each bolt starts again at its shape record's point (through its bone and the actor's +0x2AC
// matrix); unless `idle`, it then forks down to a point on the beam's rim.
void update_bolts(GenerateGhoulyState& state, u32 idle, RuntimeGenerateGhoulyContext& context)
{
    GenerateGhoulyInstance& instance = instance_of(state);
    state.bolts = 1u;
    const Actor& actor = context.actor;
    const float heading = actor.euler.y; // Actor +0x18.
    for (size_t row = 0; row < 2; ++row)
    {
        const EntityBonePose* pose = nullptr;
        if (actor.model != nullptr) // Actor +0x204: model +0x168, else +0x164.
        {
            pose = actor.model->alternate_shape_pose;
            if (pose == nullptr)
            {
                pose = actor.model->primary_shape_pose;
            }
        }
        const u8* shape = instance.shapes[row];
        if (shape == nullptr)
        {
            throw Error("0x00092840 reads a bolt row without a kind-2 shape record");
        }
        float z = read_f32({shape, 0x34}, 0x30);
        const float y = read_f32({shape, 0x34}, 0x20);
        const float x = read_f32({shape, 0x34}, 0x10);
        float local_x = x;
        float local_y = y;
        const i16 bone = std::bit_cast<i16>(read_u16({shape, 4}, 2));
        if (bone > -1)
        {
            if (pose == nullptr)
            {
                throw Error("0x00092840 reads the bone matrices of an actor without a pose");
            }
            const float* m = shape_bone_matrix(*pose, bone); // The inlined 0x000315F0.
            local_x = m[1] * y + m[2] * z + m[0] * x + m[3];
            local_y = m[4] * x + m[5] * y + m[6] * z + m[7];
            z = m[8] * x + m[9] * y + m[10] * z + m[11];
        }
        const float* w = actor.xform; // Actor +0x2AC.
        const Vec3 origin{w[1] * local_y + w[2] * z + w[0] * local_x + w[3],
            w[5] * local_y + w[6] * z + w[4] * local_x + w[7],
            w[9] * local_y + w[10] * z + w[8] * local_x + w[11]};
        GhoulySpawnPool*& pool = instance.pools[row];
        pool->cursor = pool->begin;
        push_bolt_point(pool, origin, context.random);
        if (idle == 0)
        {
            const float angle = heading + state.bolt_rows[row].angle;
            Vec3 target = state.spawn_point;
            const float radius = state.beam_radius;
            target.x = float(retail_sin(angle)) * radius + target.x; // 0x00153320
            target.z = float(retail_cos(angle)) * radius + target.z; // 0x00153230
            split_bolt(pool, origin, target, actor.euler.y, context.random); // 0x00092D50
            push_bolt_point(pool, target, context.random);
        }
    }
}

// Xbox 0x00092FF0, EAX the pool: the bolt's segments as view-facing quads of the bolt texture, then a spark
// billboard on each point (the first larger and brighter).
void draw_bolt(const GhoulySpawnPool& pool, u8* bolt_texture, const u8* spark_texture,
    RuntimeGenerateGhoulyContext& context)
{
    u32 color = 0xC0FFFFFFu;
    const u8* cursor = pool.cursor;
    const u8* point = pool.begin;
    if (u32((cursor - point) / pool.stride) > 1)
    {
        RuntimeDisplaySetupContext& display = context.scene.display();
        begin_world_line_draw(0, bolt_texture, display); // 0x00065460, EDI texture
        display.draw.d3d.simple_state(0x40354, 0x203);
        deferred(display.draw, 0x181C54) = 0x203;
        display.draw.d3d.simple_state(0x4035C, 0);
        deferred(display.draw, 0x181C70) = 0;
        display.draw.d3d.depth(1);
        const u8* previous = point;
        for (const u8* next = point + 0x10; next < cursor; next += 0x10)
        {
            Vec3 to;
            Vec3 from;
            std::memcpy(&to, next, 12);
            std::memcpy(&from, previous, 12);
            draw_world_line_quad(to, from, 1.5f, std::bit_cast<float>(0x40ACCCCDu), color, display); // 0x000654E0
            previous = next;
        }
    }
    std::array<float, 2> size{4.f, 4.f}; // DAT_0032B1B0.
    BillboardFlushContext pass = context.scene.billboards();
    begin_billboard_draw(pass); // 0x0005C410
    pass.draw.d3d.simple_state(0x40354, 0x203);
    deferred(pass.draw, 0x181C54) = 0x203;
    pass.draw.d3d.simple_state(0x4035C, 0);
    deferred(pass.draw, 0x181C70) = 0;
    pass.draw.d3d.depth(1);
    const auto spark = [&](const u8* row)
    {
        Vec3 position;
        std::memcpy(&position, row, 12);
        set_billboard_transform(position, read_f32({row, 0x10}, 0xC), pass); // 0x0005C580, ESI the point
        float width;
        float height;
        bind_billboard_sprite(spark_texture, 0, color, width, height, pass); // 0x0005C7F0
        push_billboard_quad(size.data(), width, height, pass); // 0x0005C8C0
    };
    spark(point); // 0x0005C9C0
    color = 0x80FFFFFFu;
    size = {2.f, 2.f}; // DAT_0032B110.
    for (point += 0x10; point < cursor; point += 0x10)
    {
        spark(point);
    }
    if (--pass.draw.override_count == 0) // DAT_00509B5C.
    {
        pass.draw.fog_disabled = 0; // DAT_00509B00.
    }
}

// Xbox 0x00093AD0, ESI state: the light follows the spawn point, its radii three and six times the beam's height.
void move_beam_light(GenerateGhoulyState& state, RuntimeGenerateGhoulyContext& context)
{
    const std::int8_t slot = std::bit_cast<std::int8_t>(state.light);
    if (slot == -1)
    {
        return;
    }
    move_runtime_light(slot, state.spawn_point, context.scene.lights); // 0x000FE0F0
    u8* record = context.scene.lights.records.at(size_t(slot));
    const float near = state.beam_height * 3.f; // DAT_0032B260.
    std::memcpy(record + 0x6C, &near, 4);
    record[0x66] |= 1;
    record = context.scene.lights.records.at(size_t(slot));
    const float far = state.beam_height * 6.f; // DAT_0032B218.
    std::memcpy(record + 0x70, &far, 4);
    record[0x66] |= 1;
}

// Xbox 0x00093A60, EDI state: a yellow kind-2 light at the spawn point.
void start_beam_light(GenerateGhoulyState& state, RuntimeGenerateGhoulyContext& context)
{
    GenerateGhoulyInstance& instance = instance_of(state);
    instance.light = std::make_unique<std::array<u8, 0x98>>(); // 0x000FE080's 0x000F72D0(0x98)
    u8* record = instance.light->data();
    const std::int8_t slot = allocate_runtime_light(2, state.spawn_point, record,
        context.scene.lights); // 0x000FE080, ESI state +0x480
    record = context.scene.lights.records.at(size_t(slot));
    record[0x66] |= 0x48;
    put_word({record, 0x98}, 0x78, 0xFFFFFF00u); // DAT_00314F40.
    record[0x66] |= 1;
    put_word({record, 0x98}, 0x80, 0xFFFFFF00u);
    put_word({record, 0x98}, 0x84, 0xFFC0C0C0u); // DAT_00314F94.
    const float falloff = 2.f; // DAT_0032B110.
    std::memcpy(record + 0x88, &falloff, 4);
    move_beam_light(state, context); // 0x00093AD0
    state.light = std::bit_cast<u8>(slot);
}

// The light's release shared by 0x00093780 and 0x000936A0.
void release_beam_light(GenerateGhoulyState& state, RuntimeGenerateGhoulyContext& context)
{
    const std::int8_t slot = std::bit_cast<std::int8_t>(state.light);
    if (slot == -1)
    {
        return;
    }
    u8*& record = context.scene.lights.records.at(size_t(slot));
    // The host keeps the record in the instance; the scene's light reset (0x000FDED0) may have emptied the slot.
    if (record != nullptr && (record[0x66] & 0x10) == 0)
    {
        instance_of(state).light.reset(); // 0x000663E0
    }
    record = nullptr;
    --context.scene.lights.count;
    state.light = u8(0xFF);
}

// Xbox 0x00093780, EBX state: the beam fades out over 0.2 s and goes, with the light.
void fade_beam(GenerateGhoulyState& state, RuntimeGenerateGhoulyContext& context)
{
    const u32 beam = state.beam;
    if (beam != 0)
    {
        context.scene.fade(beam, std::bit_cast<float>(0x3E4CCCCDu), kBeamFadeTint.data()); // 0x0013AF80, EDI 0x003028F8
        context.scene.retire(beam); // 0x00112CC0, ESI beam
        state.beam = 0u;
    }
    state.beam_on = 0u;
    release_beam_light(state, context);
}

// Xbox 0x000936A0, EDI state: the beam and the light go at once.
void release_beam(GenerateGhoulyState& state, RuntimeGenerateGhoulyContext& context)
{
    const u32 beam = state.beam;
    if (beam != 0)
    {
        context.scene.retire(beam); // 0x00112CC0, ESI beam
        state.beam = 0u;
    }
    release_beam_light(state, context);
}

// Xbox 0x00093800, EBX state: the spawn point (first found in front of the actor, 0x0013AA50, else straight ahead),
// then the beam stood there, its scale fitted to the ghouly's cylinder (or 5 wide and tall without one).
void place_beam(GenerateGhoulyState& state, RuntimeGenerateGhoulyContext& context)
{
    Actor& actor = context.actor;
    if (state.spawn_found == 0)
    {
        const Vec3 origin = actor_world_position(actor); // 0x000412A0
        const float reach = actor.radius_xz * body_record(context.body).radius_scale * 3.f; // DAT_0032B260.
        Vec3 point = state.spawn_point;
        RuntimeNavigationHeadingSearch search{origin, i32(get(context.actor_bytes, player_field::floor)), reach,
            reach + 5.f, nullptr, nullptr, &point, 1, 0, 0, context.random, context.occupancy_initial};
        const bool found = search_runtime_navigation_headings(search, actor.euler.y,
            std::bit_cast<float>(0x3F490FDBu), 3); // 0x0013AA50
        state.spawn_found = found ? 1u : 0u;
        if (!found)
        {
            offset_runtime_horizontal_point(point, origin, actor.euler.y, reach); // 0x001060F0, ESI state +0x480
        }
        state.spawn_point = point;
    }
    const u32 beam_address = state.beam;
    if (beam_address == 0)
    {
        return;
    }
    Actor& beam = actor_at(beam_address, context);
    set_scale(beam, 1.f, 1.f, 1.f);
    const float height = float(actor_model_height(beam)); // 0x0003DB90, EDI beam
    const float radius = float(actor_model_bound_radius(beam)); // 0x0003DB30, EDI beam
    float width = 5.f; // DAT_0032B184.
    const u32 ghouly_address = state.ghouly;
    if (ghouly_address != 0)
    {
        Vec3 position;
        float cylinder_height;
        float cylinder_radius;
        runtime_actor_cylinder_metrics(actor_at(ghouly_address, context), position, cylinder_height,
            cylinder_radius); // 0x0003DD50
        width = cylinder_radius * std::bit_cast<float>(0x3F933333u); // DAT_0032B47C.
    }
    float tall = 5.f;
    if (ghouly_address != 0)
    {
        Actor& ghouly = actor_at(ghouly_address, context);
        Vec3 position;
        float cylinder_height;
        float cylinder_radius;
        runtime_actor_cylinder_metrics(ghouly, position, cylinder_height, cylinder_radius); // 0x0003DD50
        tall = cylinder_height * std::bit_cast<float>(0x3F933333u) * ghouly.radius_y;
    }
    const float across = width / radius;
    const float up = tall / (height * 0.5f); // DAT_0032B0F4.
    state.beam_height = up * height;
    state.beam_radius = across * radius;
    set_scale(beam, across * 2.5f, up * 2.5f, across * 2.5f); // DAT_0032B20C.
    actor_set_world_position(beam, state.spawn_point); // 0x00041730, ESI state +0x480
}

// Xbox 0x00093700, EAX state: the beam actor (0x000411A0's parameters with the +0x12C model at the spawn point),
// then its light.
void start_beam(GenerateGhoulyState& state, RuntimeGenerateGhoulyContext& context)
{
    std::array<u8, 0x334> parameters{};
    initialize_actor_effect_parameters(parameters.data(), context.default_name); // 0x000411A0
    const Vec3 point = state.spawn_point;
    std::memcpy(parameters.data() + 8, &point, 12);
    const char* model = state.beam_model.data();
    char* name = reinterpret_cast<char*>(parameters.data() + 0x28);
    size_t index = 0;
    do
    {
        name[index] = model[index];
    }
    while (model[index++] != 0);
    state.beam = context.scene.keep(create_factory_object(context.factory, parameters)); // 0x00107520
    start_beam_light(state, context); // 0x00093A60
}

// Xbox 0x00093190, ESI state: the ghouly the actor's box names (refilled from its spawn record when empty) is
// made at the spawn point, hidden from the live list and kept at its scale.
void create_ghouly(GenerateGhoulyState& state, RuntimeGenerateGhoulyContext& context)
{
    char* name = state.ghouly_name.data();
    char* variant = state.ghouly_variant.data();
    if (!take_runtime_body_drop_names(context.actor_bytes, name, variant)) // 0x000286B0, EDI actor
    {
        GhoulySpawnRecord* record = context.spawn_record; // Actor +0xB98.
        if (record == nullptr)
        {
            return;
        }
        const i32 result = dispatch_next_ghouly_spawn(*record, 1, context.scene.spawns()); // 0x000F40A0
        if (result != -1 && result == 1 && record->remaining_c != -1)
        {
            --record->remaining_c;
        }
        if (!take_runtime_body_drop_names(context.actor_bytes, name, variant))
        {
            return;
        }
    }
    const i32 kind = std::bit_cast<i16>(get(context.actor_bytes, player_field::ghouly_kind));
    state.ghouly_kind = kind;
    if (!aid_name(name))
    {
        return;
    }
    const Vec3 euler{0.f, context.actor.euler.y, 0.f};
    const Vec3 point = state.spawn_point;
    if (kind == 0)
    {
        const char* override = aid_name(variant) ? variant : nullptr;
        u32 ghouly = 0;
        if (aid_name(name))
        {
            auto clone = clone_executable_resource(context.resources, name); // 0x00107720
            if (clone.bytes != nullptr)
            {
                u8* bytes = clone.bytes.get();
                std::memcpy(bytes + 8, &point, 12);
                const float zero = 0.f;
                const float one = 1.f; // DAT_0032B0F8.
                std::memcpy(bytes + 0x14, &zero, 4);
                std::memcpy(bytes + 0x1C, &zero, 4);
                std::memcpy(bytes + 0x18, &euler.y, 4);
                std::memcpy(bytes + 0x20, &one, 4);
                const u16 none = 0;
                std::memcpy(bytes + 0x2AA, &none, 2);
                std::memcpy(bytes + 0x2AC, &none, 2);
                if (override != nullptr)
                {
                    std::strcpy(reinterpret_cast<char*>(bytes + 0x5BA), override);
                }
                ghouly = context.scene.keep(create_factory_object(context.factory,
                    {clone.bytes.get(), clone.size})); // 0x00107520
                clone.bytes.reset(); // 0x000663E0
            }
        }
        state.ghouly = ghouly;
        if (ghouly == 0)
        {
            return;
        }
        Actor& actor = actor_at(ghouly, context);
        unlink_actor_view(context.scene.live_link(ghouly)); // 0x0003EE70, EAX ghouly
        state.ghouly_scale = scale_of(actor);
        const u32 attached = context.scene.attached_of(ghouly); // Ghouly +0x95C -> +0x48.
        state.attached = attached;
        if (attached != 0)
        {
            unlink_actor_view(context.scene.live_link(attached)); // 0x0003EE70
            state.attached_scale = scale_of(actor); // EDI is still the ghouly.
        }
        return;
    }
    if (kind > 0 && kind < 3)
    {
        const u32 ghouly = context.scene.keep(create_runtime_actor_at(name, &point, &euler, 1.f, context.resources,
            context.factory, context.default_position, context.default_euler)); // 0x00112C50
        state.ghouly = ghouly;
        if (ghouly != 0)
        {
            unlink_actor_view(context.scene.live_link(ghouly)); // 0x0003EE70
            state.ghouly_scale = scale_of(actor_at(ghouly, context));
        }
    }
}

// Xbox 0x00093540, ECX state: over phase 2's half second the ghouly (and its attachment) grows from 0.01 tall.
void grow_ghouly(GenerateGhoulyState& state, RuntimeGenerateGhoulyContext& context)
{
    const float flat = std::bit_cast<float>(0x3C23D70Au); // DAT_0032B124.
    const Vec3 scale = state.ghouly_scale;
    const float t = 1.f - state.grow * 2.f; // DAT_0032B0F8, DAT_0032B110.
    Actor& ghouly = actor_at(state.ghouly, context);
    set_scale(ghouly, (scale.x - scale.x) * t + scale.x, (scale.y - flat) * t + flat, (scale.z - scale.z) * t + scale.z);
    const u32 attached_address = state.attached;
    if (attached_address != 0)
    {
        const Vec3 attached_scale = state.attached_scale;
        Actor& attached = actor_at(attached_address, context);
        set_scale(attached, (attached_scale.x - scale.x) * t + scale.x, (attached_scale.y - flat) * t + flat,
            (attached_scale.z - scale.z) * t + scale.z);
    }
}

// Xbox 0x000933C0, EBX state: the hidden ghouly is placed where it stands (message 0x4D for a template clone),
// the actor is told 0x38, the ghouly (and its attachment) rejoins the live list with the release effect.
u32 release_ghouly(GenerateGhoulyState& state, RuntimeGenerateGhoulyContext& context)
{
    if (state.released != 0)
    {
        return 0;
    }
    const u32 ghouly = state.ghouly;
    if (ghouly == 0 || state.spawn_found == 0)
    {
        return 0;
    }
    if (state.ghouly_kind == 0)
    {
        Actor& actor = actor_at(ghouly, context);
        const Vec3 position = actor_world_position(actor); // 0x000412A0
        const Vec3 euler = actor_world_euler(actor); // 0x00041820
        const std::array<u32, 9> message{0x4D, 5, std::bit_cast<u32>(position.x), std::bit_cast<u32>(position.y),
            std::bit_cast<u32>(position.z), std::bit_cast<u32>(euler.x), std::bit_cast<u32>(euler.y),
            std::bit_cast<u32>(euler.z), 0};
        context.message(ghouly, message); // Ghouly vtable +0xC.
    }
    const std::array<u32, 2> notice{0x38, ghouly};
    context.message(read_u32(context.body, 8), notice); // Actor vtable +0xC.
    link_actor_view(context.scene.live_actors, context.scene.live_link(ghouly)); // 0x0003EE30, EDX ghouly
    const u32 attached = state.attached;
    if (attached != 0)
    {
        link_actor_view(context.scene.live_actors, context.scene.live_link(attached)); // 0x0003EE30
        context.scene.create_effect(state.release_effect.data(), attached); // 0x0003DF40
    }
    context.scene.create_effect(state.release_effect.data(), ghouly); // 0x0003DF40
    state.released = 1u;
    grow_ghouly(state, context); // 0x00093540
    return 1;
}

// Xbox 0x00093660, EDI state: once released, a template clone's goals are drained (0x00028210) the first time.
u32 settle_ghouly(GenerateGhoulyState& state, RuntimeGenerateGhoulyContext& context)
{
    const u32 ghouly = state.ghouly;
    if (ghouly == 0 || state.released == 0 || state.goals_drained != 0)
    {
        return 0;
    }
    if (state.ghouly_kind == 0)
    {
        context.scene.drain_goals(ghouly); // 0x00028210, ESI ghouly
    }
    state.goals_drained = 1u;
    return 1;
}

// Xbox 0x000941A0, EAX state, ECX phase.
void change_phase(GenerateGhoulyState& state, u32 phase, RuntimeGenerateGhoulyContext& context)
{
    drop_release_holds(state, context);
    state.phase = phase;
    Actor& actor = context.actor;
    switch (phase)
    {
    case 1:
        start_animation(state, 0, context); // 0x0001AFA0, state +0x20
        if (state.intro_sound.data()[0] != 0)
        {
            queue_actor_sound(actor.sound_slots[4], 0, &actor, 4, nullptr, nullptr, 1.f,
                state.intro_sound.data(), context.sounds); // 0x0012A260, actor +0x614
        }
        return;
    case 2:
        if (state.intro_sound.data()[0] != 0)
        {
            release_actor_sound(actor.sound_slots[4], context.sounds); // 0x0012A3D0, actor +0x614
        }
        if (state.release_sound.data()[0] != 0)
        {
            queue_actor_sound(actor.sound_slots[2], 0, &actor, 2, nullptr, nullptr, 1.f,
                state.release_sound.data(), context.sounds); // 0x0012A260, actor +0x4A4
        }
        adjust_live_attachment_holds(context.live_attachments, 1);
        body_record(context.body).unk_6D = u8(body_record(context.body).unk_6D + 1);
        hold_runtime_body_latches(context.body, context.hold); // 0x00027BE0, ESI body
        state.grow = 0.5f; // DAT_0032B0F4.
        release_ghouly(state, context); // 0x000933C0
        start_animation(state, 1, context); // 0x0001AFA0, state +0x24
        return;
    case 3:
        state.bolts = 0u;
        fade_beam(state, context); // 0x00093780
        settle_ghouly(state, context); // 0x00093660
        start_animation(state, 2, context); // 0x0001AFA0, state +0x28
        return;
    default:
        return;
    }
}

bool construct_generate_ghouly_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    GenerateGhoulyState& state = body_state_view<GenerateGhoulyState>({object.bytes.get(), object.size});
    return initialize_runtime_generate_ghouly_state(state, parameters,
        generate_ghouly_context_of(state.base.body));
}

void destroy_generate_ghouly_state_view(FactoryObject& object)
{
    GenerateGhoulyState& state = body_state_view<GenerateGhoulyState>({object.bytes.get(), object.size});
    destroy_runtime_generate_ghouly_state(state, generate_ghouly_context_of(state.base.body));
}

const ObjectFactoryVtable* generate_ghouly_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x005058C0, generate_ghouly_state_object_sizes,
        construct_generate_ghouly_state_view, destroy_generate_ghouly_state_view};
    return &table;
}
}

// Xbox 0x00093B40
void generate_ghouly_state_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(GenerateGhoulyState);
    output.parameters = parameter::kSize;
}

// Xbox 0x00093BC0
bool initialize_runtime_generate_ghouly_state(GenerateGhoulyState& state, std::span<const u8> parameters,
    RuntimeGenerateGhoulyContext& context)
{
    generate_ghouly_instances[body_state_key(state)] = GenerateGhoulyInstance{};
    find_animation(state, 0, state.intro_animation, get(parameters, parameter::intro_animation), context);
    find_animation(state, 1, state.release_animation, get(parameters, parameter::release_animation), context);
    find_animation(state, 2, state.end_animation, get(parameters, parameter::end_animation), context);
    copy_name(state.intro_sound.data(), parameters, parameter::intro_sound);
    copy_name(state.release_sound.data(), parameters, parameter::release_sound);
    const ActorSoundSlot& slot = context.actor.sound_slots[4]; // Actor +0x614.
    if (actor_sound_is_active(slot, context.sounds) && slot.previous_bone != 0xFFFF) // 0x0012A970, +0x61C
    {
        state.intro_sound.data()[0] = 0;
    }
    copy_name(state.beam_model.data(), parameters, parameter::beam_model);
    copy_name(state.release_effect.data(), parameters, parameter::release_effect);
    copy_name(state.bolt_texture_name.data(), parameters, parameter::bolt_texture);
    copy_name(state.spark_texture_name.data(), parameters, parameter::spark_texture);
    const auto body = context.body;
    body_record(body).move_holds = u16(body_record(body).move_holds + 1);
    for (size_t offset : {size_t(0x30B), size_t(0x309), size_t(0x30A), size_t(0x30C)})
    {
        body[offset] = u8(body[offset] + 1);
    }
    body_record(body).unk_38 = 1;
    body_record(body).unk_39 = 1;
    set(context.actor_bytes, player_field::ghouly_count, u16(get(context.actor_bytes, player_field::ghouly_count) + 1));
    state.light = u8(0xFF);
    place_beam(state, context); // 0x00093800, EBX state
    start_bolts(state, context); // 0x000925C0
    create_ghouly(state, context); // 0x00093190, ESI state
    change_phase(state, 1, context); // 0x000941A0
    return true;
}

// Xbox 0x00093DF0
void destroy_runtime_generate_ghouly_state(GenerateGhoulyState& state, RuntimeGenerateGhoulyContext& context)
{
    drop_release_holds(state, context);
    state.phase = 0u;
    release_bolts(state, context); // 0x00092790
    release_beam(state, context); // 0x000936A0, EDI state
    const u32 ghouly = state.ghouly;
    if (ghouly != 0 && state.released == 0)
    {
        context.scene.release(ghouly); // 0x001075C0, EAX ghouly
        const u32 attached = state.attached;
        if (attached != 0)
        {
            context.scene.release(attached); // 0x001075C0
        }
    }
    state.ghouly = 0u;
    state.attached = 0u;
    adjust_body_counters(context.body, -1);
    set(context.actor_bytes, player_field::ghouly_count, u16(get(context.actor_bytes, player_field::ghouly_count) - 1));
    generate_ghouly_instances.erase(body_state_key(state));
}

// Xbox 0x00093EC0
void tick_runtime_generate_ghouly_state(GenerateGhoulyState& state, RuntimeGenerateGhoulyContext& context)
{
    Actor& actor = context.actor;
    const auto body = context.body;
    state.base.elapsed = runtime_actor_rate(actor) * runtime_frame_step(context.clock) + state.base.elapsed;
    const float zero = 0.f;
    body_record(body).euler.z = zero;
    body_record(body).euler.x = zero;
    switch (state.phase)
    {
    case 1:
        state.beam_on = 0u;
        place_beam(state, context); // 0x00093800
        move_beam_light(state, context); // 0x00093AD0
        update_bolts(state, 1, context); // 0x00092840
        face_runtime_body_point(body, actor, state.spawn_point); // 0x00134510
        if (runtime_actor_animation_slot_complete(context.animation, 0)) // 0x0003B290, EAX actor, ECX 0
        {
            start_beam(state, context); // 0x00093700
            change_phase(state, 2, context); // 0x000941A0
        }
        return;
    case 2:
    {
        const float step = runtime_actor_rate(actor) * runtime_frame_step(context.clock);
        if (0.f < state.grow)
        {
            const float left = state.grow - step;
            state.grow = left;
            if (!(0.f < left)) // COMISS; JC keeps a positive time.
            {
                state.grow = 0.f;
            }
        }
        state.beam_on = 1u;
        place_beam(state, context); // 0x00093800
        move_beam_light(state, context); // 0x00093AD0
        update_bolts(state, 0, context); // 0x00092840
        if (state.released != 0 && state.ghouly != 0)
        {
            grow_ghouly(state, context); // 0x00093540
        }
        face_runtime_body_point(body, actor, state.spawn_point); // 0x00134510
        if (state.grow == 0.f) // DAT_0032B100.
        {
            // 0x000941A0's phase 3, inlined.
            drop_release_holds(state, context);
            state.phase = 3u;
            state.bolts = 0u;
            fade_beam(state, context); // 0x00093780
            settle_ghouly(state, context); // 0x00093660
            start_animation(state, 2, context); // 0x0001AFA0, state +0x28
        }
        return;
    }
    case 3:
        if (runtime_actor_animation_slot_complete(context.animation, 0)) // 0x0003B290
        {
            state.base.next_key = context.body_slot_key(0x20); // Body vtable +0x20.
        }
        return;
    default:
        return;
    }
}

// Xbox 0x00094150
void message_runtime_generate_ghouly_state(GenerateGhoulyState& state, std::span<const u32> message)
{
    if (message[0] != 0x7C)
    {
        return;
    }
    const u32 actor = message[1];
    if (state.ghouly == actor)
    {
        state.ghouly = 0u;
        state.released = 0u;
        state.goals_drained = 0u;
    }
    if (state.attached == actor)
    {
        state.attached = 0u;
    }
    if (state.beam == actor)
    {
        state.beam = 0u;
    }
}

// Xbox 0x00094130
void draw_runtime_generate_ghouly_state(GenerateGhoulyState& state, i32 kind, RuntimeGenerateGhoulyContext& context)
{
    if (kind != 2 || state.bolts == 0)
    {
        return;
    }
    // 0x00092D10, EDI state: both bolts.
    GenerateGhoulyInstance& instance = instance_of(state);
    for (size_t row = 0; row < 2; ++row)
    {
        draw_bolt(*instance.pools[row], const_cast<u8*>(instance.bolt_texture.data()),
            instance.spark_texture.data(), context); // 0x00092FF0
    }
}

void install_runtime_generate_ghouly_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeGenerateGhoulyContext&(u32 body_address)> context_of)
{
    generate_ghouly_context_of = std::move(context_of);
    types[0x6F] = ObjectFactoryType{9, generate_ghouly_state_lifecycle}; // Getter 0x00093B60.
}
}
