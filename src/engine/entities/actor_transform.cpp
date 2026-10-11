#include "gbtg/entity.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/gfx.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <malloc.h>
#include <span>
#include <vector>


#include "entity_types.h"
#include "actor_transform.h"
#include "actor_bind_tree.h"
#include "entity_heap.h"

namespace gbtg
{
using namespace entity_detail;
 // namespace

Vec3 actor_world_position(const Actor& actor)
{
    // Xbox 0x000412A0
    if (actor.bind != nullptr)
    {
        if (actor.bind->parent != nullptr || (actor.link != nullptr && actor.link->head != nullptr))
        {
            const Vec3& origin = actor_transform_origin;
            return Vec3{actor.xform[1] * origin.y + actor.xform[2] * origin.z +
                actor.xform[0] * origin.x + actor.xform[3],
                actor.xform[5] * origin.y + actor.xform[6] * origin.z +
                actor.xform[4] * origin.x + actor.xform[7],
                actor.xform[9] * origin.y + actor.xform[10] * origin.z +
                actor.xform[8] * origin.x + actor.xform[11]};
        }
        engine_heap_free(actor.bind);
        const_cast<Actor&>(actor).bind = nullptr;
    }
    return actor.position;
}


Vec3 actor_transform_origin{};
 // DAT_0050DD80

Vec3 actor_world_euler(const Actor& actor)
{
    // Xbox 0x00041820
    if (actor.bind != nullptr)
    {
        if (actor.bind->parent != nullptr || (actor.link != nullptr && actor.link->head != nullptr))
        {
            const Vec3 euler = actor_matrix_euler(actor.bind->basis);
            return Vec3{actor.euler.x + euler.x, actor.euler.y + euler.y, actor.euler.z + euler.z};
        }
        engine_heap_free(actor.bind);
        const_cast<Actor&>(actor).bind = nullptr;
    }
    return actor.euler;
}


// Xbox 0x00104040
Vec3 actor_matrix_euler(const float m[12])
{
    // Xbox 0x00104040
    // Copies the 12 floats, then 0x00103AF0 (EAX = copy, arg = 1) normalizes
    // each column of the 3x3. A zero-length column is left as it is. Pitch is
    // asin(-row1.z), saturated to +/- pi/2 (0x0032B140 / 0x0032B13C). Yaw is
    // atan2(row0.z, row2.z), or atan2(row0.y, row2.y) when those two are 0.
    // Roll is atan2(row1.x, row1.y), or 0 when both are 0. 0x00152E90 is asin
    // and 0x001530E0 is atan2. The three angles are written through EBX.
    float a[12];
    std::memcpy(a, m, sizeof(a));
    normalize_model_matrix(a, 1.f);

    constexpr float kHalfPi = std::bit_cast<float>(0x3FC90FDBu);
    constexpr float kNegHalfPi = std::bit_cast<float>(0xBFC90FDBu);
    const float pitch_arg = 0.f - a[6];
    float pitch = kHalfPi;
    if (pitch_arg < 1.f)
    {
        if (-1.f < pitch_arg)
        {
            pitch = float(retail_asin(pitch_arg));
        }
        else
        {
            pitch = kNegHalfPi;
        }
    }

    float yaw = 0.f;
    if (a[2] == 0.f && a[10] == 0.f)
    {
        yaw = float(retail_atan2(a[1], a[9]));
    }
    else
    {
        yaw = float(retail_atan2(a[2], a[10]));
    }

    float roll = 0.f;
    if (a[4] != 0.f || a[5] != 0.f)
    {
        roll = float(retail_atan2(a[4], a[5]));
    }
    return Vec3{pitch, yaw, roll};
}


// Xbox 0x00041730
void actor_set_world_position(Actor& actor, const Vec3& point)
{
    // Xbox 0x00041730
    // EDI is the actor and ESI is the source point. A bind at +0x88 whose
    // parent (+8) is set, or whose actor+0x918 link has a non-zero head,
    // multiplies the point by the 3x4 at bind+0x3c and writes actor+0x08.
    // A dead bind is released through 0x000663E0 and the point is copied.
    // Both paths clear the matrix stamp at actor+0x2a8.
    if (actor.bind != nullptr)
    {
        const bool parent_live = actor.bind->parent != nullptr;
        const bool link_live = actor.link != nullptr && actor.link->head != nullptr;
        if (parent_live || link_live)
        {
            const float* m = actor.bind->m;
            actor.position.x = m[0] * point.x + m[1] * point.y + m[2] * point.z + m[3];
            actor.position.y = m[4] * point.x + m[5] * point.y + m[6] * point.z + m[7];
            actor.position.z = m[8] * point.x + m[9] * point.y + m[10] * point.z + m[11];
            actor.xform_valid = 0;
            return;
        }
        engine_heap_free(actor.bind);
        actor.bind = nullptr;
    }
    actor.position = point;
    actor.xform_valid = 0;
}


// Xbox 0x00103CB0. Rotation is Ry(euler.y) * Rx(euler.x) * Rz(euler.z).
// A scale component other than 1 multiplies that column. Translation is the position.
static void pose_matrix(float m[12], const Vec3& pos, const Vec3& euler, const Vec3& scale,
    float* inverse = nullptr)
{
    const float sx = float(retail_sin(euler.x));
    const float cx = float(retail_cos(euler.x));
    const float sy = float(retail_sin(euler.y));
    const float cy = float(retail_cos(euler.y));
    const float sz = float(retail_sin(euler.z));
    const float cz = float(retail_cos(euler.z));

    m[0] = sy * sx * sz + cz * cy;
    m[1] = sy * sx * cz - sz * cy;
    m[2] = sy * cx;
    m[3] = pos.x;
    m[4] = sz * cx;
    m[5] = cz * cx;
    m[6] = -sx;
    m[7] = pos.y;
    m[8] = sz * cy * sx - cz * sy;
    m[9] = sz * sy + cz * cy * sx;
    m[10] = cy * cx;
    m[11] = pos.z;

    if (scale.x != 1.f)
    {
        m[0] *= scale.x;
        m[4] *= scale.x;
        m[8] *= scale.x;
    }
    if (scale.y != 1.f)
    {
        m[1] *= scale.y;
        m[5] *= scale.y;
        m[9] *= scale.y;
    }
    if (scale.z != 1.f)
    {
        m[2] *= scale.z;
        m[6] *= scale.z;
        m[10] *= scale.z;
    }
    if (inverse != nullptr)
    {
        if (scale.x == 1.f && scale.y == 1.f && scale.z == 1.f)
        {
            inverse[0] = m[0];
            inverse[1] = m[4];
            inverse[2] = m[8];
            inverse[4] = m[1];
            inverse[5] = m[5];
            inverse[6] = m[9];
            inverse[8] = m[2];
            inverse[9] = m[6];
            inverse[10] = m[10];
        }
        else
        {
            float factor = 1.f / (scale.x * scale.x);
            inverse[0] = m[0] * factor;
            inverse[1] = m[4] * factor;
            inverse[2] = factor * m[8];
            factor = 1.f / (scale.y * scale.y);
            inverse[4] = factor * m[1];
            inverse[5] = m[5] * factor;
            inverse[6] = factor * m[9];
            factor = 1.f / (scale.z * scale.z);
            inverse[8] = m[2] * factor;
            inverse[9] = m[6] * factor;
            inverse[10] = m[10] * factor;
        }
        const float x = 0.f - pos.x;
        const float y = 0.f - pos.y;
        const float z = 0.f - pos.z;
        inverse[3] = inverse[1] * y + inverse[0] * x + inverse[2] * z;
        inverse[7] = inverse[6] * z + inverse[4] * x + inverse[5] * y;
        inverse[11] = inverse[9] * y + inverse[10] * z + inverse[8] * x;
    }
}


static void actor_local_pose(float out[12], const Actor& actor, const Vec3* position)
{
    const Vec3 pos = position != nullptr ? *position : actor.position;
    const float s = actor.scale_200;
    pose_matrix(out, pos, actor.euler, Vec3{s * actor.scale_x, s * actor.radius_y, s * actor.radius_xz});
}


// 0x001041B0. out = A * B. A matching destination is written through a temporary.
// Xbox 0x001041B0
static void mul_affine(float out[12], const float a[12], const float b[12])
{
    float tmp[12];
    float* dest = out;
    if (out == a || out == b)
    {
        dest = tmp;
    }
    dest[0] = (a[0] * b[0] + a[2] * b[8]) + b[4] * a[1];
    dest[1] = (a[0] * b[1] + a[2] * b[9]) + a[1] * b[5];
    dest[2] = (a[0] * b[2] + a[2] * b[10]) + a[1] * b[6];
    dest[3] = ((a[0] * b[3] + a[2] * b[11]) + a[1] * b[7]) + a[3];
    dest[4] = (a[4] * b[0] + b[8] * a[6]) + b[4] * a[5];
    dest[5] = (a[4] * b[1] + a[6] * b[9]) + a[5] * b[5];
    dest[6] = (a[4] * b[2] + a[6] * b[10]) + a[5] * b[6];
    dest[7] = ((a[4] * b[3] + b[11] * a[6]) + a[5] * b[7]) + a[7];
    dest[8] = (b[8] * a[10] + b[4] * a[9]) + a[8] * b[0];
    dest[9] = (a[10] * b[9] + a[9] * b[5]) + a[8] * b[1];
    dest[10] = (a[10] * b[10] + a[9] * b[6]) + a[8] * b[2];
    dest[11] = ((a[10] * b[11] + a[9] * b[7]) + a[8] * b[3]) + a[11];
    if (dest != out)
    {
        std::memcpy(out, tmp, sizeof(tmp));
    }
}


// 0x00103AF0. EAX is the 3x4, the stack float is the column length.
// A zero-length column is left alone.
static void normalize_columns(float m[12], float target)
{
    normalize_model_matrix(m, target);
}


// 0x00104400. ECX is the source, the stack pointer is the destination.
// A zero determinant leaves the destination unchanged.
static void invert_affine(float dest[12], const float src[12])
{
    invert_model_matrix(dest, src);
}


// Xbox 0x0003D1B0. EAX retains the allocated node at return.
ActorAttachmentNode* actor_create_attachment(ActorAttachmentList& list, const char* name, i32 mode, i32 kind)
{
    auto* node = static_cast<ActorAttachmentNode*>(engine_heap_allocate(i32(sizeof(ActorAttachmentNode))));
    size_t index = 0;
    char value;
    do
    {
        value = name[index];
        node->bytes[0x24 + index] = u8(value);
        ++index;
    }
    while (value != '\0');
    auto word = [node](size_t offset, u32 data)
    {
        std::memcpy(node->bytes.data() + offset, &data, 4);
    };
    auto half = [node](size_t offset, u16 data)
    {
        std::memcpy(node->bytes.data() + offset, &data, 2);
    };
    word(0x20, u32(kind));
    half(0xC, 0xFFFF);
    word(8, 0xFFFFFFFF);
    word(0x10, 0);
    node->binding = nullptr;
    word(0x14, 0);
    word(0x1C, u32(mode));
    node->previous = nullptr;
    node->next = nullptr;
    ActorAttachmentNode* tail = &list.sentinel;
    ActorAttachmentNode* next = list.sentinel.next;
    while (next != nullptr)
    {
        tail = tail->next;
        next = tail->next;
    }
    tail->next = node;
    node->next = nullptr;
    node->previous = tail;
    ++list.count;
    half(0x18, list.count);
    node->bytes[0xA4] = 0;
    node->bytes[0xA5] = 0;
    word(0xC0, 0x3F800000);
    return node;
}


// Xbox 0x0003BAB0
static void bind_joint_matrix(Actor& actor, i32 index, float output[12])
{
    Actor& host = *actor_bound_parent(actor);
    RuntimeModelInstance& model = *host.model->runtime;
    const ModelPoseBuffer& pose = *(model.alternate != nullptr ? model.alternate : model.primary);
    const Joint* joint = nullptr;
    if (model.skeleton_node != nullptr && model.skeleton_node->joints.data() != nullptr)
    {
        joint = model.skeleton_node->joints.data() + index;
    }
    AnimationMatrix world;
    std::memcpy(world.data(), host.xform, sizeof(host.xform));
    const auto& result = model_joint_world_matrix(pose, index, *joint, world);
    std::memcpy(output, result.data(), sizeof(world));
}


// Xbox 0x0003B770
static void refresh_bind(Actor& actor)
{
    ActorBind& bind = *actor.bind;
    const u8 mode = bind.parent != nullptr ? bind.mode_parent : bind.mode_local;
    if (mode == 1)
    {
        Actor& host = *actor_bound_parent(actor);
        std::memcpy(bind.basis, host.xform, sizeof(bind.basis));
    }
    else if (mode == 2)
    {
        bind_joint_matrix(actor, bind.joint, bind.basis);
    }
    else if (mode == 3)
    {
        Actor& host = *actor_bound_parent(actor);
        RuntimeModelInstance& model = *host.model->runtime;
        const ModelPoseBuffer* pose = model.alternate != nullptr ? model.alternate : model.primary;
        AnimationMatrix world;
        AnimationMatrix output;
        std::memcpy(world.data(), host.xform, sizeof(host.xform));
        model_bone_world_matrix(*static_cast<const ModelBoneBinding*>(bind.bone), pose, world, output);
        std::memcpy(bind.basis, output.data(), sizeof(bind.basis));
    }
    if ((bind.flags & 1u) == 0u)
    {
        normalize_columns(bind.basis, 1.f);
    }
    invert_affine(bind.m, bind.basis);
}


// Xbox 0x00041B90
void actor_set_uniform_scale(Actor& actor, float scale)
{
    if (actor.scale_x != scale || actor.radius_y != scale || actor.radius_xz != scale)
    {
        actor.scale_x = scale;
        actor.radius_y = scale;
        actor.radius_xz = scale;
        actor.xform_valid = 0;
    }
}

// Xbox 0x00040DE0
void actor_refresh_transform(Actor& actor)
{
    if (actor.bind != nullptr)
    {
        if (actor.bind->parent != nullptr || (actor.link != nullptr && actor.link->head != nullptr))
        {
            float local[12];
            float inverse[12];
            const float scale = actor.scale_200;
            pose_matrix(local, actor.position, actor.euler,
                Vec3{actor.scale_x * scale, actor.radius_y * scale, actor.radius_xz * scale}, inverse);
            refresh_bind(actor);
            mul_affine(actor.xform, actor.bind->basis, local);
            invert_affine(actor.xform_inv, actor.xform);
            return;
        }
        engine_heap_free(actor.bind);
        actor.bind = nullptr;
    }
    if (actor.xform_valid == 0)
    {
        const float scale = actor.scale_200;
        pose_matrix(actor.xform, actor.position, actor.euler,
            Vec3{actor.scale_x * scale, actor.radius_y * scale, actor.radius_xz * scale}, actor.xform_inv);
        actor.xform_valid = 1;
    }
}


// Xbox 0x00040FB0
void actor_build_world_matrix(Actor& actor, float out[12], const Vec3* position)
{
    // Xbox 0x00040FB0
    // EAX is the optional position. A live bind refreshes bind+0xc and stores
    // basis * local pose. A dead bind is released and the local pose is stored
    // on its own (0x000410E0). The pose is actor euler, position, and the
    // scales at +0x200 * +0x30 / +0x34 / +0x38.
    if (actor.bind != nullptr)
    {
        const bool parent_live = actor.bind->parent != nullptr;
        const bool link_live = actor.link != nullptr && actor.link->head != nullptr;
        if (parent_live || link_live)
        {
            float local[12];
            actor_local_pose(local, actor, position);
            refresh_bind(actor);
            mul_affine(out, actor.bind->basis, local);
            return;
        }
        engine_heap_free(actor.bind);
        actor.bind = nullptr;
    }
    actor_local_pose(out, actor, position);
}

} // namespace gbtg
