#include "fx_custom_electric.h"
#include "actor_runtime_queries.h"
#include "actor_scare_requests.h"
#include "actor_render_registration.h"
#include "actor_transform.h"
#include "body_death_state.h"
#include "shape_transform.h"
#include "../scene/scene_shadow_casters.h"
#include "../frontend/dialogue_draw_context.h"
#include "gbtg/anim.h"

#include <array>
#include <bit>
#include <xmmintrin.h>
#include <cstring>
#include <memory>
#include <vector>

namespace gbtg
{
namespace base_parameter = fx_custom_parameter;
namespace electric_parameter = fx_electric_parameter;
namespace solid_parameter = fx_solid_fade_in_parameter;
namespace fade_parameter = fx_fade_parameter;
namespace outline_parameter = fx_outline_parameter;

namespace
{
FxCustomBase& base_of(std::span<u8> bytes)
{
    return fx_custom_view<FxCustomBase>(bytes);
}

const FxCustomBase& base_of(std::span<const u8> bytes)
{
    return fx_custom_view<FxCustomBase>(bytes);
}

FxElectricState& electric_of(std::span<u8> bytes)
{
    return fx_custom_view<FxElectricState>(bytes);
}

const FxElectricState& electric_of(std::span<const u8> bytes)
{
    return fx_custom_view<FxElectricState>(bytes);
}

FxSolidFadeInState& solid_of(std::span<u8> bytes)
{
    return fx_custom_view<FxSolidFadeInState>(bytes);
}

const FxSolidFadeInState& solid_of(std::span<const u8> bytes)
{
    return fx_custom_view<FxSolidFadeInState>(bytes);
}

FxFadeState& fade_of(std::span<u8> bytes)
{
    return fx_custom_view<FxFadeState>(bytes);
}

const FxFadeState& fade_of(std::span<const u8> bytes)
{
    return fx_custom_view<FxFadeState>(bytes);
}

FxOutlineState& outline_of(std::span<u8> bytes)
{
    return fx_custom_view<FxOutlineState>(bytes);
}

const FxOutlineState& outline_of(std::span<const u8> bytes)
{
    return fx_custom_view<FxOutlineState>(bytes);
}

// The host halves of an effect: its node on the actor's frame list, and for the electric effect the texture, the
// {shape, frame} rows and the light record (in its light slot).
struct FxCustomInstance
{
    ActorFrameAttachment node{};
    u8* bytes = nullptr; // The object's bytes, which stay put while the scene moves its FactoryObject.
    u16 type = 0;
    Actor* actor = nullptr;
    std::span<const u8> texture{};
    struct Spark
    {
        const u8* shape;
        i32 frame;
        float time = 0.f; // Type 0x31's rows (0xC bytes): +4 the row's own seconds.
    };
    std::vector<Spark> sparks;
    std::unique_ptr<std::array<u8, 0x98>> light;
};
std::unordered_map<const u8*, std::unique_ptr<FxCustomInstance>> fx_instances;
std::unordered_map<const ActorFrameAttachment*, FxCustomInstance*> fx_nodes;
RuntimeFxCustomContext* fx_context = nullptr;

u32& deferred(RuntimeDialogueDrawContext& c, u32 address)
{
    return c.deferred[(address - 0x181968) / 4];
}

std::span<u8> bytes_of(FactoryObject& object)
{
    return {object.bytes.get(), object.size};
}

FxCustomInstance& instance_of(FactoryObject& object)
{
    return *fx_instances.at(object.bytes.get());
}

void put_word(u8* bytes, size_t offset, u32 value)
{
    std::memcpy(bytes + offset, &value, 4);
}

float frame_step(const RuntimeFxCustomContext& context)
{
    if (context.pause > 0.f)
    {
        return 0.f;
    }
    return context.frame_delta * context.time_scale;
}

u32 next_random(RuntimeFxCustomContext& context)
{
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    return context.random;
}

float random_fraction(RuntimeFxCustomContext& context)
{
    return std::bit_cast<float>((next_random(context) & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
}

// The light's position: the actor's world position, half its model height up.
Vec3 light_position(const Actor& actor)
{
    Vec3 position = actor_world_position(actor); // 0x000412A0
    position.y = float(actor_model_height(actor) * 0.5L + position.y); // 0x0003DB90, DAT_0032B0F4
    return position;
}

// Xbox 0x00051BC0, EDX the record
void initialize_runtime_light(u8* record)
{
    std::memset(record, 0, 0x64);
    light_record(record).kind = 0;
    light_record(record).set_flag_bits(0);
    light_record(record).joint = 0xFF;
    light_record(record).inner_range = 0.f;
    light_record(record).range = 0.f;
    light_record(record).unk_74 = 0;
    light_record(record).colour = 0xFFFFFFFFu; // DAT_00314F48.
    light_record(record).unk_7C = 0xFF000000u; // DAT_00314F4C.
    light_record(record).colour_low = 0xFFFFFFFFu;
    light_record(record).colour_high = 0xFFFFFFFFu;
    light_record(record).colour_period = 0.f;
    light_record(record).range_low = 0.f;
    light_record(record).range_high = 0.f;
    light_record(record).range_period = 0.f;
}

// Xbox 0x000EC710: the base update counts the effect's seconds.
void update_fx_custom(FxCustomInstance& instance)
{
    const std::span<u8> effect{instance.bytes, sizeof(FxCustomBase)};
    base_of(effect).elapsed = base_of(effect).elapsed + frame_step(*fx_context);
}

// Xbox 0x00076720: the light follows the actor and each spark may jump to another frame.
void update_fx_custom_electric(FxCustomInstance& instance)
{
    RuntimeFxCustomContext& context = *fx_context;
    const std::span<u8> effect{instance.bytes, sizeof(FxElectricState)};
    base_of(effect).elapsed = base_of(effect).elapsed + frame_step(context);
    const std::int8_t slot = electric_of(effect).light_slot;
    if (slot != -1)
    {
        move_runtime_light(slot, light_position(*instance.actor), context.lights); // 0x000FE0F0
    }
    const u8 frames = instance.texture[0x11];
    for (auto& spark : instance.sparks)
    {
        const float roll = random_fraction(context);
        if (!(0.4f < roll)) // DAT_0032B11C.
        {
            spark.frame = i32(random_fraction(context) * float(frames)); // CVTTSS2SI
        }
    }
}

// Xbox 0x00076850: each spark a billboard on its shape's sphere, twice its radius across, in the opaque pass.
void draw_fx_custom_electric(FxCustomInstance& instance, Actor& actor, i32 kind)
{
    if (kind != 0)
    {
        return;
    }
    if (actor.model == nullptr || instance.sparks.empty()) // Actor +0x204.
    {
        return;
    }
    std::optional<BillboardFlushContext> pass;
    for (const auto& spark : instance.sparks)
    {
        const EntityBonePose* pose = actor.model->alternate_shape_pose; // Model +0x168, else +0x164.
        if (pose == nullptr)
        {
            pose = actor.model->primary_shape_pose;
        }
        Vec3 center;
        const float radius = shape_sphere_metrics(*reinterpret_cast<const EntityShapeNode*>(spark.shape),
            actor.xform, pose, center); // 0x00056330, actor +0x2AC
        if (!pass)
        {
            pass.emplace(fx_context->billboards());
            begin_billboard_draw(*pass); // 0x0005C410
            pass->draw.d3d.simple_state(0x40354, 0x203);
            deferred(pass->draw, 0x181C54) = 0x203;
            pass->draw.d3d.simple_state(0x4035C, 0);
            deferred(pass->draw, 0x181C70) = 0;
            pass->draw.d3d.depth(1);
        }
        const std::array<float, 2> size{radius * 2.f, radius * 2.f}; // DAT_0032B110.
        set_billboard_transform(center, 0.f, *pass); // 0x0005C580
        float width;
        float height;
        bind_billboard_sprite(instance.texture.data(), spark.frame, 0xFFFFFFFFu, width, height,
            *pass); // 0x0005C7F0, DAT_00314F48
        push_billboard_quad(size.data(), width, height, *pass); // 0x0005C8C0
    }
    if (pass && --pass->draw.override_count == 0) // DAT_00509B5C.
    {
        pass->draw.fog_disabled = 0; // DAT_00509B00.
    }
}

// The actor's +0x1D8 frame list walks call the object's vtable +0x10 and +0x14 through the node.
// Xbox 0x00078110, EAX effect: the fade starts again from its first phase with a time.
void restart_fx_solid_fade_in(u8* bytes)
{
    const std::span<u8> effect{bytes, sizeof(FxSolidFadeInState)};
    solid_of(effect).level = 0.f;
    if (0.f < solid_of(effect).fade_in_time)
    {
        solid_of(effect).phase = 0;
        solid_of(effect).time_left = solid_of(effect).fade_in_time;
        return;
    }
    solid_of(effect).level = 1.f; // DAT_0032B0F8.
    if (0.f < solid_of(effect).hold_time)
    {
        solid_of(effect).phase = 1;
        solid_of(effect).time_left = solid_of(effect).hold_time;
        return;
    }
    solid_of(effect).phase = 2;
    solid_of(effect).time_left = solid_of(effect).fade_out_time;
}

// Xbox 0x00077ED0: the phase's time runs down by the actor's step; the level sets the actor's tint.
void update_fx_solid_fade_in(FxCustomInstance& instance, Actor& actor)
{
    RuntimeFxCustomContext& context = *fx_context;
    u8* bytes = instance.bytes;
    const std::span<u8> effect{bytes, sizeof(FxSolidFadeInState)};
    base_of(effect).elapsed = base_of(effect).elapsed + frame_step(context);
    bool ended = false;
    if (0.f < solid_of(effect).time_left)
    {
        const float rate = actor.animation_pause > 0.f ? 0.f : actor.animation_rate; // Actor +0x930, +0x92C.
        if (0.f < solid_of(effect).time_left)
        {
            const float left = solid_of(effect).time_left - rate * frame_step(context);
            solid_of(effect).time_left = left;
            if (!(0.f < left)) // COMISS; JC keeps a positive time.
            {
                solid_of(effect).time_left = 0.f;
                ended = true;
            }
        }
    }
    bool release = false;
    switch (solid_of(effect).phase)
    {
    case 0:
        solid_of(effect).level = 1.f - solid_of(effect).time_left / solid_of(effect).fade_in_time; // DAT_0032B0F8.
        if (ended)
        {
            if (solid_of(effect).hold_time == 0.f)
            {
                solid_of(effect).phase = 2;
                solid_of(effect).time_left = solid_of(effect).fade_out_time;
            }
            else
            {
                solid_of(effect).phase = 1;
                solid_of(effect).time_left = solid_of(effect).hold_time;
            }
        }
        break;
    case 1:
        if (ended || solid_of(effect).hold_ended != 0)
        {
            solid_of(effect).phase = 2;
            solid_of(effect).time_left = solid_of(effect).fade_out_time;
        }
        break;
    case 2:
        if (ended)
        {
            if (solid_of(effect).wait_time <= 0.f)
            {
                release = true;
                solid_of(effect).level = 0.f;
            }
            else
            {
                solid_of(effect).phase = 3;
                solid_of(effect).time_left = solid_of(effect).wait_time;
            }
        }
        else
        {
            solid_of(effect).level = solid_of(effect).time_left / solid_of(effect).fade_out_time;
        }
        break;
    case 3:
        if (ended)
        {
            restart_fx_solid_fade_in(bytes); // 0x00078110
        }
        break;
    default:
        break;
    }
    RuntimeActorStartupState& state = context.startup_of(actor);
    u32 color = 0xFFFFFFFFu; // DAT_00314F48.
    const i32 level =
        _mm_cvttss_si32(_mm_set_ss(solid_of(effect).strength * solid_of(effect).level * 255.f)); // DAT_0032B158.
    if (level == 0)
    {
        state.flags[0] = 0; // Actor +0x19C..+0x19E.
        state.flags[1] = 0;
        state.flags[2] = 0;
    }
    else
    {
        state.flags[0] = u8(level);
        state.flags[1] = u8(level);
        state.flags[2] = u8(level);
        color = solid_of(effect).colour;
    }
    state.color = color; // Actor +0x198.
    if (release)
    {
        context.release_frame_object(actor, context.address_of(bytes)); // 0x0003DE30, ECX actor, ESI effect
    }
}

// The frame of a sprite at `time` seconds: 0 for a still sprite (+0x10 0), else its frames (+0x11) cycled at +0x10.
i32 sprite_frame(std::span<const u8> sprite, long double time)
{
    const float rate = float(sprite[0x10]);
    if (rate == 0.f) // UCOMISS DAT_0032B100.
    {
        return 0;
    }
    return u8(u32(truncate_runtime_float(time * rate)) % sprite[0x11]); // 0x001673D8
}

// The actor's step (+0x930 / +0x92C) times the frame step.
float actor_step(const Actor& actor)
{
    const float rate = actor.animation_pause > 0.f ? 0.f : actor.animation_rate;
    return rate * frame_step(*fx_context);
}

// The outline's level as a phase runs: from 0.01 up to 1 over the fade-in, 1 down to 0.01 over the fade-out.
float outline_level(float time, float length, bool rising)
{
    if (length == 0.f) // UCOMISS DAT_0032B100.
    {
        return rising ? 0.01f : 1.f; // DAT_0032B124, DAT_0032B0F8.
    }
    if (rising)
    {
        const float level = 1.f - time / length * 0.99f; // DAT_0032B228.
        if (0.01f > level)
        {
            return 0.01f;
        }
        return level > 1.f ? 1.f : level;
    }
    const float level = time / length * 0.99f + 0.01f;
    if (level > 1.f)
    {
        return 1.f;
    }
    return 0.01f > level ? 0.01f : level;
}

// Xbox 0x00077520: the light follows the actor at its level, the rows' frames run on, and the phases fade the outline
// in (1), hold it (2, until its time or message 5) and fade it out (3) before the effect releases itself.
void update_fx_custom_outline(FxCustomInstance& instance, Actor& actor)
{
    RuntimeFxCustomContext& context = *fx_context;
    u8* bytes = instance.bytes;
    const std::span<u8> effect{bytes, sizeof(FxOutlineState)};
    base_of(effect).elapsed = base_of(effect).elapsed + frame_step(context);
    const std::int8_t slot = outline_of(effect).light_slot;
    if (slot != -1)
    {
        const float reach = runtime_actor_view_radius(actor, context.actor_bytes(actor)) * outline_of(effect).level
            * 2.f; // 0x0003D860, DAT_0032B110
        move_runtime_light(slot, light_position(actor), context.lights); // 0x000FE0F0
        u8* record = context.lights.records.at(size_t(slot));
        light_record(record).inner_range = reach;
        light_record(record).flags |= 1;
        light_record(record).range = reach * 2.f;
        light_record(record).flags |= 1;
    }
    const float step = actor_step(actor);
    const float time = base_of(effect).elapsed;
    for (auto& row : instance.sparks)
    {
        row.time += step;
        row.frame = sprite_frame(instance.texture, static_cast<long double>(row.time) + time);
    }
    const float left = outline_of(effect).time_left;
    switch (outline_of(effect).phase)
    {
    case 1:
        outline_of(effect).level = outline_level(left, outline_of(effect).fade_in_time, true);
        if (0.f < left)
        {
            const float next = left - actor_step(actor);
            outline_of(effect).time_left = next;
            if (!(0.f < next)) // COMISS; JC keeps a positive time.
            {
                outline_of(effect).time_left = 0.f;
                outline_of(effect).phase = 2;
                outline_of(effect).time_left = outline_of(effect).hold_time;
            }
        }
        return;
    case 2:
    {
        bool ended = false;
        if (0.f < left)
        {
            const float next = left - actor_step(actor);
            outline_of(effect).time_left = next;
            if (!(0.f < next))
            {
                outline_of(effect).time_left = 0.f;
                ended = true;
            }
        }
        if (!ended && outline_of(effect).hold_ended == 0)
        {
            return;
        }
        if (0.f < outline_of(effect).fade_out_time)
        {
            outline_of(effect).phase = 3;
            outline_of(effect).time_left = outline_of(effect).fade_out_time;
            return;
        }
        context.release_frame_object(actor, context.address_of(bytes)); // 0x0003DE30, ESI effect
        return;
    }
    case 3:
        outline_of(effect).level = outline_level(left, outline_of(effect).fade_out_time, false);
        if (0.f < left)
        {
            const float next = left - actor_step(actor);
            outline_of(effect).time_left = next;
            if (!(0.f < next))
            {
                outline_of(effect).time_left = 0.f;
                context.release_frame_object(actor, context.address_of(bytes)); // 0x0003DE30, ESI effect
            }
        }
        return;
    default:
        return;
    }
}

// Xbox 0x00077920: each row a billboard on its shape's sphere, the radius by the level and the scale, in the
// effect's colour, in the opaque pass.
void draw_fx_custom_outline(FxCustomInstance& instance, Actor& actor, i32 kind)
{
    if (kind != 0 || actor.model == nullptr) // Actor +0x204.
    {
        return;
    }
    const std::span<const u8> effect{instance.bytes, sizeof(FxOutlineState)};
    const float factor = outline_of(effect).level * outline_of(effect).scale;
    BillboardFlushContext pass = fx_context->billboards();
    begin_billboard_draw(pass); // 0x0005C410
    pass.draw.d3d.simple_state(0x40354, 0x203);
    deferred(pass.draw, 0x181C54) = 0x203;
    pass.draw.d3d.simple_state(0x4035C, 0);
    deferred(pass.draw, 0x181C70) = 0;
    pass.draw.d3d.depth(1);
    for (const auto& row : instance.sparks)
    {
        const EntityBonePose* pose = actor.model->alternate_shape_pose; // Model +0x168, else +0x164.
        if (pose == nullptr)
        {
            pose = actor.model->primary_shape_pose;
        }
        Vec3 center;
        const float radius = shape_sphere_metrics(*reinterpret_cast<const EntityShapeNode*>(row.shape),
            actor.xform, pose, center); // 0x00056330, actor +0x2AC
        const float across = float(static_cast<long double>(radius) * factor); // FMUL; FST; FSTP
        const std::array<float, 2> size{across, across};
        set_billboard_transform(center, 0.f, pass); // 0x0005C580
        float width;
        float height;
        bind_billboard_sprite(instance.texture.data(), row.frame, outline_of(effect).colour, width, height,
            pass); // 0x0005C7F0
        push_billboard_quad(size.data(), width, height, pass); // 0x0005C8C0
    }
    if (--pass.draw.override_count == 0) // DAT_00509B5C.
    {
        pass.draw.fog_disabled = 0; // DAT_00509B00.
    }
}

// Xbox 0x00076FF0: the fade's level moves toward its target.
void update_fx_fade_to_grey(FxCustomInstance& instance, Actor& actor)
{
    RuntimeFxCustomContext& context = *fx_context;
    const std::span<u8> effect{instance.bytes, sizeof(FxFadeState)};
    const float step = context.pause > 0.f ? 0.f : context.frame_delta * context.time_scale;
    const float rate = actor.animation_pause > 0.f ? 0.f : actor.animation_rate; // +0x930, +0x92C
    const float actor_step = rate * step;
    base_of(effect).elapsed = base_of(effect).elapsed + step;
    const float target = fade_of(effect).target;
    const float difference = target - fade_of(effect).level;
    const float distance = float(engine_absolute(difference)); // 0x00152BB0
    const float speed = fade_of(effect).speed;
    const float delta = speed * actor_step;
    if (speed == 0.f)
    {
        return;
    }
    if (delta > distance)
    {
        fade_of(effect).speed = 0.f;
        fade_of(effect).level = target;
        return;
    }
    const float level = fade_of(effect).level;
    fade_of(effect).level = 0.f > difference ? level - delta : level + delta;
}

// Xbox 0x00076CB0: the level moves toward its target as the fade-to-grey's does, the actor's alpha (+0x19B) is what
// is left of it, and at full level the effect releases itself (0x0003DE30), first activating the actor's bind tree
// (0x00112CC0) when its switch is set.
void update_fx_fade_out(FxCustomInstance& instance, Actor& actor)
{
    RuntimeFxCustomContext& context = *fx_context;
    u8* bytes = instance.bytes;
    const std::span<u8> effect{bytes, sizeof(FxFadeState)};
    const float step = frame_step(context);
    const float rate = actor.animation_pause > 0.f ? 0.f : actor.animation_rate; // +0x930, +0x92C
    const float actor_step = rate * step;
    base_of(effect).elapsed = base_of(effect).elapsed + step;
    const float target = fade_of(effect).target;
    const float difference = target - fade_of(effect).level;
    const float distance = float(engine_absolute(difference)); // 0x00152BB0
    const float speed = fade_of(effect).speed;
    const float delta = speed * actor_step;
    if (speed != 0.f)
    {
        if (delta > distance)
        {
            fade_of(effect).speed = 0.f;
            fade_of(effect).level = target;
        }
        else
        {
            const float level = fade_of(effect).level;
            fade_of(effect).level = 0.f > difference ? level - delta : level + delta;
        }
    }
    const u8 alpha = u8(truncate_runtime_float((1.L - fade_of(effect).level) * 255.f)); // 0x001673D8, DAT_0032B158
    RuntimeActorStartupState& state = context.startup_of(actor);
    state.color = (state.color & 0x00FFFFFFu) | (u32(alpha) << 24); // Actor +0x19B.
    if (fade_of(effect).level != 1.f) // DAT_0032B0F8.
    {
        return;
    }
    if (fade_of(effect).fade_out_activates_binds != 0)
    {
        activate_actor_bind_tree(actor); // 0x00112CC0, ESI actor
    }
    context.release_frame_object(actor, context.address_of(bytes)); // 0x0003DE30, ECX actor, ESI effect
}

void update_fx_node(ActorFrameAttachment& node, Actor& actor)
{
    FxCustomInstance& instance = *fx_nodes.at(&node);
    if (instance.type == 0x30)
    {
        update_fx_fade_to_grey(instance, actor); // 0x00076FF0
        return;
    }
    if (instance.type == 0x33)
    {
        update_fx_fade_out(instance, actor); // 0x00076CB0
        return;
    }
    if (instance.type == 0x31)
    {
        update_fx_custom_outline(instance, actor); // 0x00077520
        return;
    }
    if (instance.type == 0x2E)
    {
        update_fx_custom_electric(instance); // 0x00076720
        return;
    }
    if (instance.type == 0x32)
    {
        update_fx_solid_fade_in(instance, actor); // 0x00077ED0
        return;
    }
    update_fx_custom(instance); // 0x000EC710
}

void draw_fx_node(ActorFrameAttachment& node, Actor& actor, i32 kind)
{
    FxCustomInstance& instance = *fx_nodes.at(&node);
    if (instance.type == 0x2E)
    {
        draw_fx_custom_electric(instance, actor, kind); // 0x00076850
    }
    if (instance.type == 0x31)
    {
        draw_fx_custom_outline(instance, actor, kind); // 0x00077920
    }
    // fxCustom's own +0x14 is the empty 0x001628B0.
}

// Xbox 0x000EC6A0: the effect joins the tail of its actor's frame list.
bool construct_fx_custom_view(FactoryObject& object, std::span<const u8> parameters)
{
    RuntimeFxCustomContext& context = *fx_context;
    auto& slot = fx_instances[object.bytes.get()];
    slot = std::make_unique<FxCustomInstance>();
    FxCustomInstance& instance = *slot;
    instance.bytes = object.bytes.get();
    instance.type = object.type;
    const u32 actor_address = get(parameters, base_parameter::actor);
    base_of(std::span<u8>{object.bytes.get(), sizeof(FxCustomBase)}).actor = actor_address;
    instance.actor = context.actor_at(actor_address);
    if (instance.actor == nullptr)
    {
        throw Error("0x000EC6A0 walks the frame list of an actor with no scene view");
    }
    ActorFrameState& frame = context.frame_of(*instance.actor);
    ActorFrameAttachment** tail = &frame.updates; // Actor +0x1D4's +4.
    while (*tail != nullptr)
    {
        tail = &(*tail)->next;
    }
    instance.node.next = nullptr; // +0xC.
    instance.node.update = update_fx_node;
    *tail = &instance.node;
    fx_nodes[&instance.node] = &instance;
    context.frame_objects[&instance.node] = object.bytes.get();
    context.draws[&instance.node] = draw_fx_node;
    return true;
}

// Xbox 0x000EC6E0: the effect leaves its actor's frame list.
void destroy_fx_custom_view(FactoryObject& object)
{
    RuntimeFxCustomContext& context = *fx_context;
    const auto found = fx_instances.find(object.bytes.get());
    if (found == fx_instances.end())
    {
        return;
    }
    FxCustomInstance& instance = *found->second;
    ActorFrameState& frame = context.frame_of(*instance.actor);
    for (ActorFrameAttachment** link = &frame.updates; *link != nullptr; link = &(*link)->next)
    {
        if (*link == &instance.node)
        {
            *link = instance.node.next; // The previous node's +4 takes this one's next.
            break;
        }
    }
    instance.node.next = nullptr;
    fx_nodes.erase(&instance.node);
    context.frame_objects.erase(&instance.node);
    context.draws.erase(&instance.node);
    fx_instances.erase(found);
}

// Xbox 0x000769D0, EAX actor, ESI effect: a {shape, random frame} row for each contact shape of kind 0.
void collect_fx_electric_sparks(FxCustomInstance& instance, u8* bytes)
{
    const u8* block = runtime_actor_model_block(*instance.actor, 4); // Actor +0x204's kind-4 block.
    if (block == nullptr)
    {
        return;
    }
    const u32 mask = (runtime_shape_list_mask(block) & 0x80) != 0 ? 0x80u : 0x20002u; // 0x00056C80
    u32 count = 0;
    for (const u8* record = block + 8; record != nullptr; record = record + record[1])
    {
        if ((read_u32({record, 8}, 4) & mask) != 0 && record[0] == 0)
        {
            ++count;
        }
        if (record[1] == 0)
        {
            break;
        }
    }
    electric_of(std::span<u8>{bytes, sizeof(FxElectricState)}).spark_count = count;
    if (count == 0)
    {
        return;
    }
    // 0x000F72D0(count * 8): the rows live on the host.
    electric_of(std::span<u8>{bytes, sizeof(FxElectricState)}).sparks = 1;
    const u8 frames = instance.texture[0x11];
    for (const u8* record = block + 8; record != nullptr; record = record + record[1])
    {
        if ((read_u32({record, 8}, 4) & mask) != 0 && record[0] == 0)
        {
            instance.sparks.push_back({record, i32(random_fraction(*fx_context) * float(frames))}); // CVTTSS2SI
        }
        if (record[1] == 0)
        {
            break;
        }
    }
}

// Xbox 0x00076B00, EAX actor: a kind-2 light at the actor, its radius twice and four times the actor's view radius.
void start_fx_electric_light(FxCustomInstance& instance, u8* bytes)
{
    RuntimeFxCustomContext& context = *fx_context;
    Actor& actor = *instance.actor;
    const float reach = runtime_actor_view_radius(actor, context.actor_bytes(actor)); // 0x0003D860
    instance.light = std::make_unique<std::array<u8, 0x98>>(); // 0x000F72D0(0x98)
    u8* record = instance.light->data();
    const std::int8_t slot = allocate_runtime_light(2, light_position(actor), record, context.lights); // 0x000FE080
    light_record(record).flags |= 0x48;
    light_record(record).inner_range = reach * 2.f; // DAT_0032B110.
    light_record(record).flags |= 1;
    light_record(record).range = reach * 4.f; // DAT_0032B1B0.
    light_record(record).colour = 0xFFFFFF00u; // DAT_00314F40.
    light_record(record).colour_low = 0xFFFFFF00u;
    light_record(record).colour_high = 0xFFC0C0C0u; // DAT_00314F94.
    light_record(record).colour_period = 2.f;
    electric_of(std::span<u8>{bytes, sizeof(FxElectricState)}).light_slot = slot;
}

// Xbox 0x000765E0
bool construct_fx_custom_electric_view(FactoryObject& object, std::span<const u8> parameters)
{
    RuntimeFxCustomContext& context = *fx_context;
    FxCustomInstance& instance = instance_of(object);
    instance.type = 0x2E;
    u8* bytes = object.bytes.get();
    const std::span<u8> effect{bytes, sizeof(FxElectricState)};
    const char* texture_name = field_data(parameters, electric_parameter::texture);
    u32 prefix;
    std::memcpy(&prefix, texture_name, 4);
    if (prefix == 0x5F646961) // "aid_".
    {
        instance.texture = acquire_animation_resource(context.registry, texture_name); // 0x00035E90
        if (instance.texture.data() == nullptr)
        {
            resource_error(context.error, 2);
        }
    }
    electric_of(effect).texture_loaded = instance.texture.data() != nullptr ? 1u : 0u;
    electric_of(effect).light_slot = -1;
    if (instance.texture.data() == nullptr)
    {
        return false;
    }
    collect_fx_electric_sparks(instance, bytes); // 0x000769D0
    if (electric_of(effect).spark_count == 0)
    {
        return false;
    }
    start_fx_electric_light(instance, bytes); // 0x00076B00
    const char* sound = field_data(parameters, electric_parameter::sound);
    char* name = electric_of(effect).sound.data();
    size_t index = 0;
    do
    {
        name[index] = sound[index];
    }
    while (sound[index++] != 0);
    if (name[0] != 0)
    {
        request_actor_sound(*instance.actor, 4, name, context.sounds); // 0x00041D00
    }
    return true;
}

// Xbox 0x00076690
void destroy_fx_custom_electric_view(FactoryObject& object)
{
    RuntimeFxCustomContext& context = *fx_context;
    const auto found = fx_instances.find(object.bytes.get());
    if (found == fx_instances.end())
    {
        return;
    }
    FxCustomInstance& instance = *found->second;
    const std::span<u8> effect{object.bytes.get(), sizeof(FxElectricState)};
    if (electric_of(effect).texture_loaded != 0)
    {
        release_animation_resource(context.registry, instance.texture); // 0x00035C00
        instance.texture = {};
        electric_of(effect).texture_loaded = 0;
    }
    if (electric_of(effect).sparks != 0)
    {
        instance.sparks.clear(); // 0x000663E0
        electric_of(effect).sparks = 0;
    }
    const std::int8_t slot = electric_of(effect).light_slot;
    if (slot != -1)
    {
        u8*& record = context.lights.records.at(size_t(slot));
        // The host keeps the record in the instance; the scene's light reset (0x000FDED0) may have emptied the slot.
        if (record != nullptr && (light_record(record).flags & 0x10) == 0)
        {
            instance.light.reset(); // 0x000663E0
        }
        record = nullptr;
        --context.lights.count;
        electric_of(effect).light_slot = -1;
    }
    if (electric_of(effect).sound.data()[0] != 0)
    {
        release_actor_sound(instance.actor->sound_slots[4], context.sounds); // 0x0012A3D0, actor +0x614
    }
}

// Xbox 0x00077AD0, EAX actor, ESI effect: a {shape, random time, frame} row for each shape of the actor's kind-4
// block that carries the first of 0x80, 0x20002 and 1 its list has (0x00077D30), is of kind 0 and passes the
// effect's shape group (-1 for any).
void collect_fx_outline_rows(FxCustomInstance& instance, u8* bytes)
{
    const u8* block = runtime_actor_model_block(*instance.actor, 4); // Actor +0x204's kind-4 block.
    if (block == nullptr)
    {
        return;
    }
    const u32 carried = runtime_shape_list_mask(block); // 0x00077D30's OR, as 0x00056C80 takes it.
    u32 mask = 0;
    for (const u32 candidate : {0x80u, 0x20002u, 1u}) // DAT_002FFEA8.
    {
        if ((candidate & carried) != 0)
        {
            mask = candidate;
            break;
        }
    }
    const std::span<u8> effect{bytes, sizeof(FxOutlineState)};
    const i16 filter = outline_of(effect).shape_group;
    const auto wanted = [mask, filter](const u8* record)
    {
        i16 group;
        std::memcpy(&group, record + 2, 2);
        return (read_u32({record, 8}, 4) & mask) != 0 && record[0] == 0 && (filter == -1 || group == filter);
    };
    u32 count = 0;
    for (const u8* record = block + 8; record != nullptr; record = record + record[1])
    {
        if (wanted(record))
        {
            ++count;
        }
        if (record[1] == 0)
        {
            break;
        }
    }
    outline_of(effect).row_count = count;
    if (count == 0)
    {
        return;
    }
    outline_of(effect).rows = 1; // 0x000F72D0(count * 0xC): the rows live on the host.
    for (const u8* record = block + 8; record != nullptr; record = record + record[1])
    {
        if (wanted(record))
        {
            const float time = random_fraction(*fx_context);
            instance.sparks.push_back({record, sprite_frame(instance.texture, time), time});
        }
        if (record[1] == 0)
        {
            break;
        }
    }
}

// Xbox 0x00077C70, EAX parameters, ECX actor: with the parameters' light switch set, a kind-2 light at the actor
// half its height up, 20 across, its flags and its +0x78 / +0x80 / +0x84 words from the parameters; else no light
// (-1).
std::int8_t start_fx_outline_light(FxCustomInstance& instance, std::span<const u8> parameters)
{
    if (get(parameters, outline_parameter::has_light) == 0)
    {
        return -1;
    }
    RuntimeFxCustomContext& context = *fx_context;
    instance.light = std::make_unique<std::array<u8, 0x98>>(); // 0x000F72D0(0x98) in 0x000FE080.
    u8* record = instance.light->data();
    const std::int8_t slot = allocate_runtime_light(2, light_position(*instance.actor), record,
        context.lights); // 0x000FE080
    u16 flags;
    flags = light_record(record).flag_bits();
    flags |= get(parameters, outline_parameter::light_flags);
    light_record(record).set_flag_bits(flags);
    light_record(record).inner_range = 10.f; // DAT_0032B10C.
    light_record(record).flags |= 1;
    light_record(record).inner_range = 20.f; // DAT_0032B234.
    light_record(record).flags |= 1;
    light_record(record).colour = get(parameters, outline_parameter::light_78);
    light_record(record).flags |= 1;
    light_record(record).colour_low = get(parameters, outline_parameter::light_80);
    light_record(record).colour_high = get(parameters, outline_parameter::light_84);
    light_record(record).colour_period = 2.f; // DAT_0032B110.
    return slot;
}

// Xbox 0x00077350
bool construct_fx_outline_view(FactoryObject& object, std::span<const u8> parameters)
{
    RuntimeFxCustomContext& context = *fx_context;
    FxCustomInstance& instance = instance_of(object);
    instance.type = 0x31;
    u8* bytes = object.bytes.get();
    const std::span<u8> effect{bytes, sizeof(FxOutlineState)};
    outline_of(effect).light_slot = -1;
    instance.texture = {};
    const char* texture_name = field_data(parameters, outline_parameter::texture);
    if (read_u32(parameters, outline_parameter::texture.offset) == 0x5F646961) // "aid_".
    {
        instance.texture = acquire_animation_resource(context.registry, texture_name); // 0x00035E90
        if (instance.texture.data() == nullptr)
        {
            resource_error(context.error, 2); // 0x00032700
        }
    }
    outline_of(effect).texture_loaded = instance.texture.data() != nullptr ? 1u : 0u;
    if (instance.texture.data() == nullptr)
    {
        return false;
    }
    outline_of(effect).shape_group = get(parameters, outline_parameter::shape_group);
    outline_of(effect).scale = get(parameters, outline_parameter::scale);
    outline_of(effect).colour = get(parameters, outline_parameter::colour);
    outline_of(effect).hold_time = get(parameters, outline_parameter::hold_time);
    outline_of(effect).fade_in_time = get(parameters, outline_parameter::fade_in_time);
    outline_of(effect).fade_out_time = get(parameters, outline_parameter::fade_out_time);
    float level = 1.f; // DAT_0032B0F8.
    if (get(parameters, outline_parameter::fade_in_time) > 0.f) // DAT_0032B100.
    {
        outline_of(effect).phase = 1;
        outline_of(effect).time_left = get(parameters, outline_parameter::fade_in_time);
        level = 0.01f; // DAT_0032B124.
    }
    else
    {
        outline_of(effect).phase = 2;
        outline_of(effect).time_left = get(parameters, outline_parameter::hold_time);
    }
    outline_of(effect).level = level;
    collect_fx_outline_rows(instance, bytes); // 0x00077AD0
    if (outline_of(effect).row_count == 0)
    {
        return false;
    }
    outline_of(effect).light_slot = start_fx_outline_light(instance, parameters); // 0x00077C70
    const char* sound = field_data(parameters, outline_parameter::sound);
    char* name = outline_of(effect).sound.data();
    size_t index = 0;
    do
    {
        name[index] = sound[index];
    }
    while (sound[index++] != 0);
    if (name[0] != 0)
    {
        request_actor_sound(*instance.actor, 4, name, context.sounds); // 0x00041D00
    }
    return true;
}

// Xbox 0x00077490
void destroy_fx_outline_view(FactoryObject& object)
{
    RuntimeFxCustomContext& context = *fx_context;
    const auto found = fx_instances.find(object.bytes.get());
    if (found == fx_instances.end())
    {
        return;
    }
    FxCustomInstance& instance = *found->second;
    const std::span<u8> effect{object.bytes.get(), sizeof(FxOutlineState)};
    if (outline_of(effect).texture_loaded != 0)
    {
        release_animation_resource(context.registry, instance.texture); // 0x00035C00
        instance.texture = {};
        outline_of(effect).texture_loaded = 0;
    }
    if (outline_of(effect).rows != 0)
    {
        instance.sparks.clear(); // 0x000663E0
        outline_of(effect).rows = 0;
    }
    const std::int8_t slot = outline_of(effect).light_slot;
    if (slot != -1)
    {
        u8*& record = context.lights.records.at(size_t(slot));
        // The host keeps the record in the instance; the scene's light reset (0x000FDED0) may have emptied the slot.
        if (record != nullptr && (light_record(record).flags & 0x10) == 0)
        {
            instance.light.reset(); // 0x000663E0
        }
        record = nullptr;
        --context.lights.count;
        outline_of(effect).light_slot = -1;
    }
    if (outline_of(effect).sound.data()[0] != 0)
    {
        release_actor_sound(instance.actor->sound_slots[4], context.sounds); // 0x0012A3D0, actor +0x614
    }
}

const ObjectFactoryVtable* fx_outline_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505428, fx_custom_outline_object_sizes, construct_fx_outline_view,
        destroy_fx_outline_view};
    return &table;
}

const ObjectFactoryVtable* fx_custom_lifecycle()
{
    static const ObjectFactoryVtable table{0x005068F4, fx_custom_object_sizes, construct_fx_custom_view,
        destroy_fx_custom_view};
    return &table;
}

const ObjectFactoryVtable* fx_custom_electric_lifecycle()
{
    static const ObjectFactoryVtable table{0x005053E0, fx_custom_electric_object_sizes,
        construct_fx_custom_electric_view, destroy_fx_custom_electric_view};
    return &table;
}

// Xbox 0x00077E00
bool construct_fx_solid_fade_in_view(FactoryObject& object, std::span<const u8> parameters)
{
    FxCustomInstance& instance = instance_of(object);
    instance.type = 0x32;
    u8* bytes = object.bytes.get();
    const std::span<u8> effect{bytes, sizeof(FxSolidFadeInState)};
    solid_of(effect).fade_in_time = get(parameters, solid_parameter::fade_in_time);
    solid_of(effect).hold_time = get(parameters, solid_parameter::hold_time);
    solid_of(effect).fade_out_time = get(parameters, solid_parameter::fade_out_time);
    solid_of(effect).wait_time = get(parameters, solid_parameter::wait_time);
    solid_of(effect).colour = get(parameters, solid_parameter::colour);
    solid_of(effect).strength = get(parameters, solid_parameter::strength);
    restart_fx_solid_fade_in(bytes); // The inlined 0x00078110.
    return true;
}

// Xbox 0x00077E90: the actor's tint and its +0x1A0..+0x1A8 draw callbacks back to none.
void destroy_fx_solid_fade_in_view(FactoryObject& object)
{
    RuntimeFxCustomContext& context = *fx_context;
    const auto found = fx_instances.find(object.bytes.get());
    if (found == fx_instances.end())
    {
        return;
    }
    Actor& actor = *found->second->actor; // Effect +0x14.
    const std::span<u8> bytes = context.object_bytes(actor);
    put_word(bytes.data(), 0x1A0, 0);
    put_word(bytes.data(), 0x1A4, 0);
    put_word(bytes.data(), 0x1A8, 0);
    RuntimeActorStartupState& state = context.startup_of(actor);
    state.color = 0xFFFFFFFFu; // DAT_00314F48.
    state.flags[0] = 0;
    state.flags[1] = 0;
    state.flags[2] = 0;
}

// Xbox 0x00076ED0
bool construct_fx_fade_to_grey_view(FactoryObject& object, std::span<const u8> parameters)
{
    RuntimeFxCustomContext& context = *fx_context;
    FxCustomInstance& instance = instance_of(object);
    instance.type = 0x30;
    u8* bytes = object.bytes.get();
    const std::span<u8> effect{bytes, sizeof(FxFadeState)};
    const float seconds = get(parameters, fade_parameter::seconds);
    if (seconds > 0.f)
    {
        fade_of(effect).target = 1.f; // DAT_0032B0F8.
        fade_of(effect).speed = float(engine_absolute((1.f - fade_of(effect).level) / seconds)); // 0x00152BB0
    }
    else
    {
        fade_of(effect).level = 1.f;
        fade_of(effect).speed = 0.f;
    }
    Actor& actor = *instance.actor; // Effect +0x14.
    const std::span<u8> actor_bytes = context.object_bytes(actor);
    put_word(actor_bytes.data(), 0x1A0, 0x000770F0u);
    put_word(actor_bytes.data(), 0x1A4, 0x000771F0u);
    put_word(actor_bytes.data(), 0x1A8, context.address_of(bytes));
    instance.texture = {};
    if (read_u32(parameters, fade_parameter::fade_to_grey_texture.offset) == 0x5F646961) // "aid_".
    {
        instance.texture = acquire_animation_resource(context.registry,
            field_data(parameters, fade_parameter::fade_to_grey_texture)); // 0x00035E90
        if (instance.texture.data() == nullptr)
        {
            resource_error(context.error, 2); // 0x00032700
        }
    }
    fade_of(effect).fade_to_grey_texture = instance.texture.data() == nullptr ? 0u
        : context.resource_address(instance.texture.data());
    actor_bytes[0x196] = 1;
    return true;
}

// Xbox 0x00076FC0
void destroy_fx_fade_to_grey_view(FactoryObject& object)
{
    RuntimeFxCustomContext& context = *fx_context;
    const auto found = fx_instances.find(object.bytes.get());
    if (found == fx_instances.end())
    {
        return;
    }
    FxCustomInstance& instance = *found->second;
    const std::span<u8> actor_bytes = context.object_bytes(*instance.actor);
    put_word(actor_bytes.data(), 0x1A0, 0);
    put_word(actor_bytes.data(), 0x1A4, 0);
    put_word(actor_bytes.data(), 0x1A8, 0);
    actor_bytes[0x196] = 0;
    if (fade_of(bytes_of(object)).fade_to_grey_texture != 0)
    {
        release_animation_resource(context.registry, instance.texture); // 0x00035C00, EBX texture
        instance.texture = {};
    }
}

// Getter 0x00076E80; +0x14 is the empty 0x001628B0.
const ObjectFactoryVtable* fx_fade_to_grey_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505410, fx_custom_fade_to_grey_object_sizes,
        construct_fx_fade_to_grey_view, destroy_fx_fade_to_grey_view};
    return &table;
}

const ObjectFactoryVtable* fx_solid_fade_in_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505440, fx_custom_solid_fade_in_object_sizes,
        construct_fx_solid_fade_in_view, destroy_fx_solid_fade_in_view};
    return &table;
}

// Xbox 0x00076C30
bool construct_fx_fade_out_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> effect{object.bytes.get(), sizeof(FxFadeState)};
    const float seconds = get(parameters, fade_parameter::seconds);
    if (0.f < seconds)
    {
        fade_of(effect).target = 1.f; // DAT_0032B0F8.
        fade_of(effect).speed = float(engine_absolute((1.f - fade_of(effect).level) / seconds)); // 0x00152BB0
        fade_of(effect).fade_out_activates_binds = get(parameters, fade_parameter::fade_out_activates_binds);
        return true;
    }
    fade_of(effect).level = 1.f;
    fade_of(effect).speed = 0.f;
    fade_of(effect).fade_out_activates_binds = get(parameters, fade_parameter::fade_out_activates_binds);
    return true;
}

// Xbox 0x001628B0
void destroy_fx_fade_out_view(FactoryObject&)
{
}

// Getter 0x00076BE0: +0 is 0x00076BC0 (the same code as the death state's sizes), +8 and +0x14 the empty 0x001628B0,
// +0xC the fade-to-grey's message 0x00076DF0.
const ObjectFactoryVtable* fx_fade_out_lifecycle()
{
    static const ObjectFactoryVtable table{0x005053F8, death_state_object_sizes, construct_fx_fade_out_view,
        destroy_fx_fade_out_view};
    return &table;
}
}

// Xbox 0x00076E60
void fx_custom_fade_to_grey_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(FxFadeState);
    output.parameters = fade_parameter::kFadeToGreySize;
}

// Xbox 0x00076DF0
void message_fx_custom_fade_to_grey(std::span<u8> effect, std::span<const u32> message)
{
    if (message[0] != 0xAC)
    {
        return;
    }
    const float level = std::bit_cast<float>(message[1]);
    const float seconds = std::bit_cast<float>(message[2]);
    if (seconds > 0.f)
    {
        fade_of(effect).target = level;
        fade_of(effect).speed = float(engine_absolute((level - fade_of(effect).level) / seconds)); // 0x00152BB0
        return;
    }
    fade_of(effect).level = level;
    fade_of(effect).speed = 0.f;
}

// Xbox 0x000772E0
void fx_custom_outline_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(FxOutlineState);
    output.parameters = outline_parameter::kSize;
}

// Xbox 0x00077A80
void message_fx_custom_outline(std::span<u8> effect, std::span<const u32> message)
{
    if (message[0] == 3)
    {
        outline_of(effect).level = 1.f; // DAT_0032B0F8.
        outline_of(effect).phase = 2;
        outline_of(effect).time_left = outline_of(effect).hold_time;
        outline_of(effect).hold_ended = 0;
        return;
    }
    if (message[0] == 5)
    {
        outline_of(effect).hold_ended = 1;
    }
}

// Xbox 0x00077D90
void fx_custom_solid_fade_in_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(FxSolidFadeInState);
    output.parameters = solid_parameter::kSize;
}

// Xbox 0x000780C0
void message_fx_custom_solid_fade_in(std::span<u8> effect, std::span<const u32> message)
{
    if (message[0] == 3)
    {
        const i32 phase = i32(solid_of(effect).phase);
        if (0 < phase && phase < 3)
        {
            solid_of(effect).phase = 1;
            solid_of(effect).time_left = solid_of(effect).hold_time;
            solid_of(effect).level = 1.f; // DAT_0032B0F8.
            solid_of(effect).hold_ended = 0;
        }
    }
    else if (message[0] == 5)
    {
        solid_of(effect).hold_ended = 1;
    }
}

// Xbox 0x00095530
void fx_custom_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(FxCustomBase);
    output.parameters = base_parameter::kSize;
}

// Xbox 0x00076570
void fx_custom_electric_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(FxElectricState);
    output.parameters = electric_parameter::kSize;
}

// Xbox 0x000FE080, ESI position
std::int8_t allocate_runtime_light(u8 kind, const Vec3& position, u8* record, RuntimeShaderRegistrationState& lights)
{
    std::int8_t slot = -1;
    for (std::int8_t index = 0; index < 0x28; ++index)
    {
        if (lights.records[size_t(index)] == nullptr)
        {
            slot = index;
            break;
        }
    }
    if (slot == -1)
    {
        throw Error("0x000FE080 stores a light record through DAT_00509B7C[-1]: all 40 slots are taken");
    }
    initialize_runtime_light(record); // 0x00051BC0
    light_record(record).kind = kind;
    if (kind == 2 || kind == 3)
    {
        light_record(record).flags |= 0xA;
    }
    light_record(record).position = position;
    lights.records[size_t(slot)] = record;
    ++lights.count;
    return slot;
}

// Xbox 0x000FE0F0, AL slot, ECX position
void move_runtime_light(std::int8_t slot, const Vec3& position, RuntimeShaderRegistrationState& lights)
{
    u8* record = lights.records.at(size_t(slot));
    if (record == nullptr)
    {
        throw Error("0x000FE0F0 moves an empty light slot");
    }
    light_record(record).position = position;
    if (light_record(record).kind != 2)
    {
        return;
    }
    const Vec3 toward{0.f - position.x, 0.f - position.y, 0.f - position.z};
    Vec3 direction;
    runtime_direction_or_axis(toward, direction, 0x79); // 0x00106B90, EDX the record's direction
    light_record(record).direction = direction;
}

// Xbox 0x00056C80, EDX the list
u32 runtime_shape_list_mask(const u8* list)
{
    i32 count;
    std::memcpy(&count, list, 4);
    u32 mask = 0;
    const u8* record = list + 8;
    for (i32 index = 0; index < count; ++index)
    {
        if (record == nullptr)
        {
            throw Error("0x00056C80 reads past a shape list's last record");
        }
        mask |= read_u32({record, 8}, 4);
        record = record[1] != 0 ? record + record[1] : nullptr;
    }
    return mask;
}

void install_runtime_fx_custom_classes(std::span<ObjectFactoryType> types, RuntimeFxCustomContext& context)
{
    fx_context = &context;
    types[0xC] = ObjectFactoryType{1, fx_custom_lifecycle}; // Getter 0x000EC650.
    types[0x2E] = ObjectFactoryType{0xC, fx_custom_electric_lifecycle}; // Getter 0x00076590.
    types[0x30] = ObjectFactoryType{0xC, fx_fade_to_grey_lifecycle}; // Getter 0x00076E80.
    types[0x31] = ObjectFactoryType{0xC, fx_outline_lifecycle}; // Getter 0x00077300.
    types[0x32] = ObjectFactoryType{0xC, fx_solid_fade_in_lifecycle}; // Getter 0x00077DB0.
    types[0x33] = ObjectFactoryType{0xC, fx_fade_out_lifecycle}; // Getter 0x00076BE0.
}
}
