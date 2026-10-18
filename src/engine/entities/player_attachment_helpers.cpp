#include "player_attachment_helpers.h"
#include "body_message_helpers.h"
#include "entity_heap.h"
#include "actor_spawn_query.h"
#include "actor_transform.h"
#include "actor_bind_tree.h"
#include "gbtg/model_runtime.h"
#include <cstring>
#include <new>

namespace gbtg
{
// Xbox 0x0001D3D0
float runtime_player_body_attachment_radius(const RuntimePlayerAttachmentEligibility& context)
{
    const float margin = context.body_actor.radius_xz * read_f32(context.body, 0x78);
    const auto* attached = context.attached;
    if (attached != nullptr && attached->bytes[0x93D] == 1
        && attached->actor->model != nullptr && attached->actor->model->cpu.data() != nullptr)
    {
        const float radius = actor_model_radius(*attached->actor);
        return radius + margin;
    }
    return 0.f + margin;
}

// Xbox 0x00027DA0
bool runtime_player_body_allows_attachment(const RuntimePlayerAttachmentEligibility& context,
    const Actor& target)
{
    const auto body = context.body;
    if (body[0x6A] != 0 || read_u16(body, 0x30) != 0 || read_f32(body, 0x34) != 0.f
        || context.attached != nullptr)
    {
        return false;
    }
    constexpr std::array<u32, 3> flags{27, 42, 40}; // DAT_00217F08.
    for (u32 flag : flags)
    {
        if (runtime_body_bit_set(context.flags, flag))
        {
            return false;
        }
    }
    const Vec3 owner = actor_world_position(context.body_actor);
    const Vec3 position = actor_world_position(target);
    return !(owner.y - position.y > 5.f); // DAT_0032B184, JBE accepts unordered.
}

// Xbox 0x00027E50
bool runtime_player_attachment_is_in_range(const RuntimePlayerAttachmentEligibility& context,
    const Actor& target)
{
    const auto body = context.body;
    const float reach = read_f32(body, 0x888) * context.body_actor.radius_xz;
    const float margin = read_f32(body, 0x78) * context.body_actor.radius_xz;
    float model_radius = 0.f;
    const auto* attached = context.attached;
    if (attached != nullptr && attached->bytes[0x93D] == 1
        && attached->actor->model != nullptr && attached->actor->model->cpu.data() != nullptr)
    {
        model_radius = actor_model_radius(*attached->actor);
    }
    const Vec3 owner = actor_world_position(context.body_actor);
    Vec3 point;
    float radius;
    i32 index;
    if (select_actor_spawn_point(target, owner, point, radius, index))
    {
        const float range = (((model_radius + margin) + radius) + reach) + 2.f;
        const float dz = owner.z - point.z;
        const float dx = owner.x - point.x;
        return range * range > dz * dz + dx * dx;
    }
    return false;
}

// Xbox 0x00027D40
bool runtime_player_can_attach(const RuntimePlayerAttachmentEligibility& body,
    const RuntimeBodyAttachedActor& target, std::span<const u8> target_link)
{
    if (runtime_player_body_allows_attachment(body, *target.actor))
    {
        const u32 link = read_u32(target.bytes, 0x918);
        if (link != 0 && link != 0x918 && target_link.empty())
        {
            throw Error("attachment target +0x918 link has no native view");
        }
        if ((link == 0 || link == 0x918 || read_u32(target_link, 0) == 0)
            && target.bytes[0x93C] != 0 && read_u32(target.bytes, 0xAD0) == 0
            && read_u32(target.bytes, 0xAC4) != 0)
        {
            return runtime_player_attachment_is_in_range(body, *target.actor);
        }
    }
    return false;
}

// Xbox 0x00134240
bool runtime_player_body_target_is_near(const RuntimePlayerAttachmentEligibility& context,
    std::span<const u8> target_body, const Actor& target_actor)
{
    const auto body = context.body;
    const float margin = context.body_actor.radius_xz * read_f32(body, 0x78);
    float model_radius = 0.f;
    const auto* attached = context.attached;
    if (attached != nullptr && attached->bytes[0x93D] == 1
        && attached->actor->model != nullptr && attached->actor->model->cpu.data() != nullptr)
    {
        model_radius = actor_model_radius(*attached->actor);
    }
    const float reach = read_f32(body, 0x888) * context.body_actor.radius_xz;
    const Vec3 owner = actor_world_position(context.body_actor);
    const float target_reach = target_actor.radius_xz * read_f32(target_body, 0x888);
    const Vec3 target = actor_world_position(target_actor);
    const float range = ((model_radius + margin) + target_reach) + reach;
    const float dz = owner.z - target.z;
    const float dx = owner.x - target.x;
    const float dy = owner.y - target.y;
    return range * range >= (dz * dz + dx * dx) + dy * dy;
}

// Xbox 0x00134350
bool runtime_player_body_point_is_near(const RuntimePlayerAttachmentEligibility& context,
    std::span<const u8> target)
{
    const auto body = context.body;
    const float margin = context.body_actor.radius_xz * read_f32(body, 0x78);
    float model_radius = 0.f;
    const auto* attached = context.attached;
    if (attached != nullptr && attached->bytes[0x93D] == 1
        && attached->actor->model != nullptr && attached->actor->model->cpu.data() != nullptr)
    {
        model_radius = actor_model_radius(*attached->actor);
    }
    const float reach = read_f32(body, 0x888) * context.body_actor.radius_xz;
    const Vec3 owner = actor_world_position(context.body_actor);
    const float range = ((model_radius + margin) + read_f32(target, 0x38)) + reach;
    const float dz = owner.z - read_f32(target, 0x28);
    const float dx = owner.x - read_f32(target, 0x20);
    return range * range >= dz * dz + dx * dx;
}

// Xbox 0x00146190
i32 runtime_attachment_class_value(std::span<const u8> attached, u16 host_class)
{
    const i32 count = i16(read_u16(attached, 0x944));
    i32 index = 0;
    if (count > 0)
    {
        do
        {
            const size_t offset = 0x948 + size_t(index) * 0x10;
            if (read_u16(attached, offset) == host_class)
            {
                return i16(read_u16(attached, offset + 2));
            }
            ++index;
        }
        while (index < count);
    }
    return -1;
}

// Xbox 0x001461C0
void set_runtime_attachment_host(RuntimeBodyAttachedActor& attached,
    const RuntimeBodyAttachedActor& host)
{
    i32 index = 0;
    const u16 host_class = read_u16(host.bytes, 6);
    if (i16(read_u16(attached.bytes, 0x944)) > 0)
    {
        while (read_u16(attached.bytes, 0x948 + size_t(index) * 0x10) != host_class)
        {
            const i32 count = i16(read_u16(attached.bytes, 0x944));
            ++index;
            if (index >= count)
            {
                index = -1;
                break;
            }
        }
    }
    else
    {
        index = -1;
    }
    const u16 selected = u16(index);
    std::memcpy(attached.bytes.data() + 0x946, &selected, 2);
    std::memcpy(attached.bytes.data() + 0xD8C, &host.address, 4);
    attached.host = const_cast<RuntimeBodyAttachedActor*>(&host);
    if (attached.actor != nullptr)
    {
        attached.actor->spawn_owner = host.actor; // The native view of +0xD8C.
    }
    const u32 message[]{0x91, host.address};
    attached.message(message);
}

// Xbox 0x00146100
bool runtime_attachment_available(std::span<const u8> attached, const RuntimeBodyAttachedActor* host)
{
    if (attached[0x93F] == 1)
    {
        return false;
    }
    if (attached[0x93F] == 2)
    {
        if (host == nullptr)
        {
            return false;
        }
        if (read_u16(host->bytes, 0xCDC) != 2 && read_u32(host->bytes, 0x93C) == 0)
        {
            return false;
        }
    }
    return attached[0x941] == 0;
}

// Xbox 0x0003B9D0
ActorBind* ensure_runtime_actor_bind(Actor& actor)
{
    if (actor.bind == nullptr)
    {
        void* block = engine_heap_allocate(i32(sizeof(ActorBind)));
        actor.bind = static_cast<ActorBind*>(block);
        if (block != nullptr)
        {
            actor.bind = new (block) ActorBind{};
        }
    }
    return actor.bind;
}

// Xbox 0x0003B840
void bind_runtime_actor_joint(Actor& actor, Actor* parent, i32 joint_key, u8 flags)
{
    if (parent != nullptr && parent->model != nullptr && parent->model->cpu.data() != nullptr
        && joint_key != -1 && parent->model != nullptr && parent->model->cpu.data() != nullptr)
    {
        const u32 joint = actor_lookup_joint_index(parent->model->cpu, i16(joint_key)) & 0xFFu;
        if (joint != 0xFFu && joint != 0xFFFFFFFFu && ensure_runtime_actor_bind(actor) != nullptr)
        {
            actor.bind->mode_parent = 2;
            actor.bind->parent = parent;
            actor.bind->flags = flags;
            actor.bind->joint = i32(joint);
        }
    }
}

namespace
{
// The shared tail of 0x0003B460 and 0x0003B4C0.
void apply_runtime_actor_bind(Actor& actor, const Vec3& origin, const Vec3& euler)
{
    ActorBind* bind = actor.bind; // Actor +0x88.
    if (bind != nullptr)
    {
        bind->mode_local = bind->mode_parent;
        if (bind->parent == nullptr)
        {
            throw Error("0x0003BB20 links an actor under a null bind +8 parent");
        }
        bind_actor_under_parent(actor, *bind->parent, origin, euler); // 0x0003BB20
        bind->parent = nullptr;
    }
}
}

// Xbox 0x0003B460
bool bind_runtime_actor_parent_now(Actor& actor, Actor* parent, u8 flags, const Vec3& origin, const Vec3& euler)
{
    if (parent != nullptr && ensure_runtime_actor_bind(actor) != nullptr)
    {
        actor.bind->mode_parent = 1;
        actor.bind->parent = parent;
        actor.bind->flags = flags;
    }
    apply_runtime_actor_bind(actor, origin, euler);
    return true;
}

// Xbox 0x0003B4C0
bool bind_runtime_actor_joint_now(Actor& actor, Actor* parent, i32 joint_key, u8 flags, const Vec3& origin,
    const Vec3& euler)
{
    bind_runtime_actor_joint(actor, parent, joint_key, flags); // 0x0003B840
    apply_runtime_actor_bind(actor, origin, euler);
    return true;
}

// Xbox 0x0001B7C0
void notify_runtime_secondary_attachment_host(RuntimeBodyAttachedActor& host, RuntimeBodyAttachedActor& attached)
{
    const u32 message[]{0x5F, attached.address};
    host.message(message);
    if (read_u16(host.bytes, 0xCDC) != 2 && read_u32(host.bytes, 0x93C) == 0)
    {
        const u32 active = 1;
        std::memcpy(attached.bytes.data() + 0x9A4, &active, 4);
    }
}

// Xbox 0x0001B770
void notify_runtime_attachment_host(RuntimeBodyAttachedActor& host, RuntimeBodyAttachedActor& attached)
{
    const u32 message[]{0x5E, attached.address};
    host.message(message);
    if (read_u16(host.bytes, 0xCDC) != 2 && read_u32(host.bytes, 0x93C) == 0)
    {
        const u32 active = 1;
        std::memcpy(attached.bytes.data() + 0x9A4, &active, 4);
    }
}
}
