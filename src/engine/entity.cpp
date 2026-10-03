#include "gbtg/entity.hpp"

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

namespace gbtg
{
namespace
{

std::vector<EntityType> g_types;

u8 case_state(const Entity& body)
{
    if (body.case_link == nullptr)
    {
        return 0;
    }
    return body.case_link->state;
}

// State 1 is classified by 0x00145FF0. No class word is stored here, so that
// case misses the same way state > 4 does.
i32 id_by_state(const Entity& body, i32 state0, i32 state2, i32 state3, i32 state4)
{
    switch (case_state(body))
    {
    case 0:
        return state0;
    case 2:
        return state2;
    case 3:
        return state3;
    case 4:
        return state4;
    default:
        return 0;
    }
}

void body_query_size(u32 out[2])
{
    // 0x0001DA30. Default slot 0. Camera ctors replace this with 0x00073170.
    out[0] = 0x894;
    out[1] = 0x7C8;
}

void body_tick(Entity&)
{
    // 0x00024C50. Default tick. Publishes the actor at +0x1b0 and samples pose.
}

void body_shutdown(Entity&)
{
    // 0x00024F70. Releases the record at +0x64 through 0x000663E0, then the
    // object at +0x5c. This body does not keep those records.
}

bool body_message(Entity& body, const Message& msg);

void body_relay(Entity&, const Message&)
{
    // 0x00025EC0. Forwards to the same two children through vtable +0x10.
}

void body_step_channels(Entity&)
{
    // 0x00072AC0. Child at +0x314 is called through vtable +0x14. Count at
    // +0x154 selects 0x0001EA90. A set +0xfc runs 0x0001D850 and tails to
    // 0x0001EDE0. The channel block is not on this body.
}

void body_apply(Entity&)
{
    // 0x00025AE0 camera_apply. +0xfc selects 0x0001EEC0, +0x154 selects
    // 0x0001EC20, then 0x00025190 and 0x00025C50. Scene tick owns those calls.
}

i32 body_gate()
{
    // 0x000CFFD0. Default slot 7. Returns 1. Camera ctors replace it.
    return 1;
}

i32 body_pick_actor(const Entity&, i32 fallback)
{
    // 0x00026950. Closes to the caller word unless actor+0x958, its byte +0x20,
    // body+0x73c, and 0x00027510 all accept.
    return fallback;
}

i32 body_pick_gated(const Entity&)
{
    // 0x000269C0. State ids 0xB6, 0xBC, 0xBD, 0xBB, 0xB8, 0xBA, 0xB9. Body+0x73c
    // and 0x00027510 must accept. No record returns 0.
    return 0;
}

i32 body_link_idle(const Entity& body)
{
    // 0x00026A70. Null link or state 0 returns 0x112. Any other state returns 0.
    if (body.case_link == nullptr || body.case_link->state == 0)
    {
        return 0x112;
    }
    return 0;
}

i32 body_pick_actor_b(const Entity&, i32 fallback)
{
    // 0x00026A90. Same gate as 0x00026950, after 0x00027140 instead of 0x00027060.
    return fallback;
}

i32 body_pick_direct(const Entity& body)
{
    // 0x00026B00. State 0/2/3/4 are 0x118/0x119/0x11B/0x11A.
    return id_by_state(body, 0x118, 0x119, 0x11B, 0x11A);
}

i32 body_pick_direct_b(const Entity& body)
{
    // 0x00026BB0. State 0/2/3/4 are 0x33/0x34/0x36/0x35.
    return id_by_state(body, 0x33, 0x34, 0x36, 0x35);
}

i32 body_pick_gated_b(const Entity&)
{
    // 0x00026DE0. State ids 0x1C, 0x22, 0x23, 0x21, 0x1D, 0x1F, 0x1E, then the
    // +0x73c gate. No record returns 0.
    return 0;
}

i32 body_pick_gated_c(const Entity&)
{
    // 0x00026EC0. State ids 0x2D, 0x31, 0x32, 0x30, 0x2E, 0x2F, then the gate.
    return 0;
}

i32 body_pick_gated_d(const Entity&)
{
    // 0x00026F90. State ids 0xE4, 0xE9, 0xEA, 0xE8, 0xE5, 0xE7, 0xE6, then the gate.
    return 0;
}

i32 body_pick_direct_c(const Entity& body)
{
    // 0x00027220. State 0/2/3/4 are 0x70/0x71/0x73/0x72.
    return id_by_state(body, 0x70, 0x71, 0x73, 0x72);
}

i32 body_pick_range(const Entity& body)
{
    // 0x000272D0. State 4 returns 0xD7. State 1 asks 0x001460C0 and adds 4 when
    // that call is non-zero (0xDA); a zero result stays 0xD6. Every other state
    // returns 0xD6.
    if (case_state(body) == 4)
    {
        return 0xD7;
    }
    return 0xD6;
}

i32 body_fixed_id()
{
    // 0x00027500. Always 0xA9 in AX.
    return 0xA9;
}

i32 body_pick_direct_d(const Entity& body)
{
    // 0x00027320. State 0/2/3/4 are 0x56/0x57/0x59/0x58.
    return id_by_state(body, 0x56, 0x57, 0x59, 0x58);
}

i32 body_pick_direct_e(const Entity& body)
{
    // 0x000273D0. State 0/2/3/4 are 0x41/0x42/0x44/0x43.
    return id_by_state(body, 0x41, 0x42, 0x44, 0x43);
}

i32 body_pick_direct_f(const Entity& body)
{
    // 0x00027480. States 0, 2, and 4 return 0x15. State 3 returns 0x16.
    // State 1 classifies: class 1 is 0x15, class 2 is 0x18, class 3 is 0x17.
    return id_by_state(body, 0x15, 0x15, 0x16, 0x15);
}

i32 body_pick_near(const Entity& body)
{
    // 0x00026C60. Reads the case byte on its object. State 3 returns 0xDC.
    // States 1 and 2 compare world positions (0x000412A0) and can return
    // 0xC3..0xC9. Any other state returns 0.
    if (case_state(body) == 3)
    {
        return 0xDC;
    }
    return 0;
}

void add_named(const char* name, EntityKind kind)
{
    EntityType t;
    t.name = name;
    t.kind = kind;
    fn_1da60(t.methods);
    t.tick = t.methods.tick;
    t.shutdown = t.methods.shutdown;
    t.handle_message = t.methods.handle_message;
    u32 sizes[2] = {};
    t.methods.query_size(sizes);
    t.xbox_size = sizes[0];
    t.xbox_extra = sizes[1];
    g_types.push_back(t);
}

constexpr i32 kHeapSlotCount = 4;

struct HeapSlot
{
    u32 id = 0;             // record + 0x20
    void* handle = nullptr; // record + 0x24 (DAT_00507834 for slot 0)
};

// DAT_00507810 stride 0x30. g_heap_current is DAT_00507808.
HeapSlot g_heap_slots[kHeapSlotCount];
i32 g_heap_current = 0;

i32 heap_index_for(void* block)
{
    // FUN_000f7100. EDI is the block. Four handles at DAT_00507834, stride 0x30.
    // msize (0x0014DED0) failure forces the footer byte to 0xFF, which cannot
    // match a heap id. The allocator at 0x000F72D0 writes that id at msize-2.
    // No match returns 3.
    for (i32 i = 0; i < kHeapSlotCount; ++i)
    {
        if (g_heap_slots[i].handle == nullptr || block == nullptr)
        {
            continue;
        }
        const u32 tag = 0xFFu;
        if (tag == g_heap_slots[i].id)
        {
            return i;
        }
    }
    return 3;
}

} // namespace

Vec3 actor_world_position(const Actor& actor)
{
    // Xbox 0x000412A0: if attach+8 is live, multiply local by parent matrix.
    // Unattached path copies actor+0x08.
    if (actor.attach == nullptr)
    {
        return actor.position;
    }
    const Vec3 p = actor.attach->position;
    return Vec3{actor.position.x + p.x, actor.position.y + p.y, actor.position.z + p.z};
}

Vec3 actor_world_euler(const Actor& actor)
{
    // Xbox 0x00041820: unattached copies actor+0x14; attached adds parent euler.
    if (actor.attach == nullptr)
    {
        return actor.euler;
    }
    const Vec3 e = actor.attach->euler;
    return Vec3{actor.euler.x + e.x, actor.euler.y + e.y, actor.euler.z + e.z};
}

Vec3 fn_104040(const float m[12])
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
    for (int col = 0; col < 3; ++col)
    {
        const float x = a[col];
        const float y = a[col + 4];
        const float z = a[col + 8];
        const float len2 = x * x + y * y + z * z;
        if (len2 != 0.f)
        {
            const float inv = 1.f / std::sqrt(len2);
            a[col] = x * inv;
            a[col + 4] = y * inv;
            a[col + 8] = z * inv;
        }
    }

    constexpr float kHalfPi = std::bit_cast<float>(0x3FC90FDBu);
    constexpr float kNegHalfPi = std::bit_cast<float>(0xBFC90FDBu);
    const float pitch_arg = 0.f - a[6];
    float pitch = kHalfPi;
    if (pitch_arg < 1.f)
    {
        if (-1.f < pitch_arg)
        {
            pitch = std::asin(pitch_arg);
        }
        else
        {
            pitch = kNegHalfPi;
        }
    }

    float yaw = 0.f;
    if (a[2] == 0.f && a[10] == 0.f)
    {
        yaw = std::atan2(a[1], a[9]);
    }
    else
    {
        yaw = std::atan2(a[2], a[10]);
    }

    float roll = 0.f;
    if (a[4] != 0.f || a[5] != 0.f)
    {
        roll = std::atan2(a[4], a[5]);
    }
    return Vec3{pitch, yaw, roll};
}

void fn_41730(Actor& actor, const Vec3& point)
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
        fn_663e0(actor.bind);
        actor.bind = nullptr;
    }
    actor.position = point;
    actor.xform_valid = 0;
}

// 0x00103CB0 with EDI clear. Rotation is Ry(euler.y) * Rx(euler.x) * Rz(euler.z).
// A scale component other than 1 multiplies that column. Translation is the position.
static void pose_matrix(float m[12], const Vec3& pos, const Vec3& euler, const Vec3& scale)
{
    const float sx = std::sin(euler.x);
    const float cx = std::cos(euler.x);
    const float sy = std::sin(euler.y);
    const float cy = std::cos(euler.y);
    const float sz = std::sin(euler.z);
    const float cz = std::cos(euler.z);

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
}

static void actor_local_pose(float out[12], const Actor& actor, const Vec3* position)
{
    const Vec3 pos = position != nullptr ? *position : actor.position;
    const float s = actor.scale_200;
    pose_matrix(out, pos, actor.euler, Vec3{s * actor.scale_x, s * actor.radius_y, s * actor.radius_xz});
}

// 0x001041B0. out = A * B. A matching destination is written through a temporary.
static void mul_affine(float out[12], const float a[12], const float b[12])
{
    float tmp[12];
    float* dest = out;
    if (out == a || out == b)
    {
        dest = tmp;
    }
    for (int row = 0; row < 3; ++row)
    {
        const float ax = a[row * 4 + 0];
        const float ay = a[row * 4 + 1];
        const float az = a[row * 4 + 2];
        const float aw = a[row * 4 + 3];
        dest[row * 4 + 0] = ax * b[0] + ay * b[4] + az * b[8];
        dest[row * 4 + 1] = ax * b[1] + ay * b[5] + az * b[9];
        dest[row * 4 + 2] = ax * b[2] + ay * b[6] + az * b[10];
        dest[row * 4 + 3] = ax * b[3] + ay * b[7] + az * b[11] + aw;
    }
    if (dest != out)
    {
        std::memcpy(out, tmp, sizeof(tmp));
    }
}

// 0x00103AF0. EAX is the 3x4, the stack float is the column length.
// A zero-length column is left alone.
static void normalize_columns(float m[12], float target)
{
    for (int col = 0; col < 3; ++col)
    {
        const float x = m[col];
        const float y = m[col + 4];
        const float z = m[col + 8];
        const float len2 = x * x + y * y + z * z;
        float factor = 1.f;
        if (len2 != 0.f)
        {
            const float inv = 1.f / std::sqrt(len2);
            if (inv != 0.f)
            {
                factor = target * inv;
            }
        }
        m[col] *= factor;
        m[col + 4] *= factor;
        m[col + 8] *= factor;
    }
}

// 0x00104400. ECX is the source, the stack pointer is the destination.
// A zero determinant leaves the destination unchanged.
static void invert_affine(float dest[12], const float src[12])
{
    const float m00 = src[0];
    const float m01 = src[1];
    const float m02 = src[2];
    const float tx = src[3];
    const float m10 = src[4];
    const float m11 = src[5];
    const float m12 = src[6];
    const float ty = src[7];
    const float m20 = src[8];
    const float m21 = src[9];
    const float m22 = src[10];
    const float tz = src[11];
    const float det = m11 * m00 * m22 + m01 * m12 * m20 + m10 * m21 * m02 - m11 * m20 * m02 - m01 * m10 * m22 -
                      m21 * m12 * m00;
    if (det == 0.f)
    {
        return;
    }
    const float inv = 1.f / det;
    float out[12];
    out[0] = (m11 * m22 - m21 * m12) * inv;
    out[1] = 0.f - (m01 * m22 - m21 * m02) * inv;
    out[2] = (m01 * m12 - m11 * m02) * inv;
    out[4] = 0.f - (m10 * m22 - m12 * m20) * inv;
    out[5] = (m00 * m22 - m20 * m02) * inv;
    out[6] = 0.f - (m12 * m00 - m10 * m02) * inv;
    out[8] = (m10 * m21 - m11 * m20) * inv;
    out[9] = 0.f - (m21 * m00 - m01 * m20) * inv;
    out[10] = (m11 * m00 - m01 * m10) * inv;
    out[3] = 0.f - (out[0] * tx + out[1] * ty + out[2] * tz);
    out[7] = 0.f - (out[4] * tx + out[5] * ty + out[6] * tz);
    out[11] = 0.f - (out[8] * tx + out[9] * ty + out[10] * tz);
    std::memcpy(dest, out, sizeof(out));
}

// 0x0003B770. Parent set reads the mode byte at +1; otherwise the byte at +0.
// Mode 1 copies the host world matrix at +0x2ac (0x0003B8B0). Modes 2 and 3
// sample a bone (0x0003BAB0 / 0x00051E40); this actor has no skeleton, so
// those modes keep the basis already stored. Bit 0 of +2 skips the normalize.
// The inverse of the basis is written at +0x3c.
static void refresh_bind(Actor& actor)
{
    ActorBind& bind = *actor.bind;
    const u8 mode = bind.parent != nullptr ? bind.mode_parent : bind.mode_local;
    Actor* host = bind.parent;
    if (host == nullptr && actor.link != nullptr && actor.link->head != nullptr)
    {
        host = actor.link->owner;
    }
    if (mode == 1 && host != nullptr)
    {
        std::memcpy(bind.basis, host->xform, sizeof(bind.basis));
        std::memcpy(bind.m, host->xform_inv, sizeof(bind.m));
    }
    if ((bind.flags & 1u) == 0u)
    {
        normalize_columns(bind.basis, 1.f);
    }
    invert_affine(bind.m, bind.basis);
}

void fn_40fb0(Actor& actor, float out[12], const Vec3* position)
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
        fn_663e0(actor.bind);
        actor.bind = nullptr;
    }
    actor_local_pose(out, actor, position);
}

void* fn_f72d0(i32 size)
{
    // Xbox 0x000F72D0
    // The size is the stack argument. DAT_00507808 selects the 0x30-byte record
    // at DAT_00507810. A zero handle at +0x24 nulls that record and the load of
    // +0x24 faults; 0x000663E0 releases that case on the process heap, so this
    // allocates there too. 0x0014E872 takes (handle, 0, size+4). On a non-null
    // block, 0x0014DED0's msize stores u16 0x1234 at msize-4 and the byte at
    // record+0x20 at msize-2.
    const i32 index = g_heap_current;
    void* block = std::malloc(static_cast<size_t>(static_cast<u32>(size) + 4u));
    if (block == nullptr)
    {
        return nullptr;
    }
    const size_t span = _msize(block);
    u8* tag = static_cast<u8*>(block) + span - 4;
    tag[0] = 0x34;
    tag[1] = 0x12;
    tag[2] = static_cast<u8>(g_heap_slots[index].id);
    return block;
}

void fn_663e0(void* block)
{
    // Xbox 0x000663E0
    // EAX is the block. The owning slot is selected, DAT_00507808 is switched
    // to that index for HeapFree(handle, 0, block), then the previous index
    // is restored. A null slot faults on Xbox; the process heap releases it.
    const i32 index = heap_index_for(block);
    const i32 saved = g_heap_current;
    const void* handle = g_heap_slots[index].handle;
    g_heap_current = index;
    if (handle != nullptr || block != nullptr)
    {
        std::free(block);
    }
    g_heap_current = saved;
}

void fn_73760(Entity& body)
{
    // Xbox 0x00073760
    // EAX is the body. 0x000734B0 tails here after it has released the pointers
    // at +0x89c and +0x8a0. The six slots are cleared from +0x910 down to +0x8ac.
    // 0x00073A90 hands back successive 0x30-byte slices of one allocation, so the
    // lowest live pointer is the base. That base goes through 0x000663E0.
    void* base = nullptr;
    for (i32 i = 5; i >= 0; --i)
    {
        if (body.block_8ac[i] != nullptr)
        {
            base = body.block_8ac[i];
            body.block_8ac[i] = nullptr;
        }
    }
    if (base != nullptr)
    {
        fn_663e0(base);
    }
}

void fn_fcd70(void* dest, const void* src)
{
    // Xbox 0x000FCD70
    // EAX is the destination and ECX is the source. Copies 0x54 bytes forward.
    // Equal pointers return without a write. 0x000FCCC0 appends records of this
    // size, and other callers plant the block at object +0x1cc and +0xc48.
    if (dest == src)
    {
        return;
    }
    std::memcpy(dest, src, 0x54);
}

static_assert(sizeof(EntityMatchBlock) == 0x28);
static_assert(offsetof(EntityMatchBlock, value) == 4);
static_assert(std::bit_cast<u32>(0.01f) == 0x3C23D70Au);  // 0x0032B124
static_assert(std::bit_cast<u32>(-0.01f) == 0xBC23D70Au); // 0x0032B270

i32 fn_10ae90(const EntityMatchBlock& left, const EntityMatchBlock& right)
{
    // Xbox 0x0010AE90
    // ECX is left and EAX is right. cam_72cb0 calls this with the slot index
    // at 2, passing slot 0 + 0x28 and slot 2 + 0x28 of the 0x54-byte records
    // copied by 0x000FCD70. Both bytes at +0 must be 0. Each of the nine floats
    // at +4 must satisfy -0.01 <= left - right <= 0.01 (0x0032B270 / 0x0032B124).
    // The bounds are inclusive. Any other case returns 0.
    if (left.mark != 0 || right.mark != 0)
    {
        return 0;
    }
    for (int i = 0; i < 9; ++i)
    {
        const float diff = left.value[i] - right.value[i];
        if (diff < -0.01f || diff > 0.01f)
        {
            return 0;
        }
    }
    return 1;
}

void entity_body_camera_sizeof(u32 out_sizes[2])
{
    // Xbox 0x00073170
    out_sizes[0] = 0x914;
    out_sizes[1] = 0x7C8;
}

void entity_body_camera_tick(Entity& body)
{
    // Xbox 0x000731F0: clear a flag, insert into the camera list, copy actor pose.
    if (body.actor == nullptr)
    {
        return;
    }
    body.pose_position = actor_world_position(*body.actor);
    body.pose_euler = actor_world_euler(*body.actor);
    body.pose_quat = body.actor->quat;
    body.pose_fov = body.actor->fov;
    body.listed = true;
}

void entity_body_camera_shutdown(Entity& body)
{
    // Xbox 0x00073300: unlink the record at body+0x894.
    fn_48c70(&body.cam);
    body.listed = false;
}

bool entity_body_camera_message(Entity& body, const Message& msg)
{
    // Xbox 0x00073320: id 99 translates body+0x18; other ids fall through.
    if (msg.id != kMsgCameraTranslate)
    {
        return false;
    }
    body.position.x += msg.vec.x;
    body.position.y += msg.vec.y;
    body.position.z += msg.vec.z;
    return true;
}

namespace
{

// DAT_004e1680, stride 8. Slot 0 is the sentinel. The list head read by
// 0x00048CF0 is anchor.next, at DAT_004e1684 + slot * 8.
EntityCamNode g_cam_anchor[4]{};

// DAT_004e16a0. The record currently bound in that slot.
EntityCamNode* g_cam_active[4]{};

// DAT_004e16b0, stride 0x21. Name copied from the bound record.
char g_cam_saved_name[4][0x21]{};

unsigned char cam_fold(unsigned char ch)
{
    if (ch > 0x20)
    {
        return static_cast<unsigned char>(ch & 0xDFu);
    }
    return ch;
}

// Case-fold used by 0x00048CF0: bytes above 0x20 lose bit 0x20.
bool cam_name_match(const char* node_name, const char* want)
{
    for (;;)
    {
        const unsigned char folded_node = cam_fold(static_cast<unsigned char>(*node_name));
        const unsigned char folded_want = cam_fold(static_cast<unsigned char>(*want));
        if (folded_node != folded_want)
        {
            return false;
        }
        if (folded_node == 0)
        {
            return true;
        }
        ++node_name;
        ++want;
    }
}

void copy_cam_name(char* dest, const char* src)
{
    int i = 0;
    while (i < 0x20 && src[i] != '\0')
    {
        dest[i] = src[i];
        ++i;
    }
    dest[i] = '\0';
}

} // namespace

i32 cam_func_48cf0(i32 slot, const char* name)
{
    // Xbox 0x00048CF0
    // ESI walks the slot list from DAT_004e1684 through +4. EBX is the name.
    // A match stores that record in DAT_004e16a0 and returns 1. A miss clears
    // the slot and returns 0. The flag, pose, and fn_f57f0 writes that follow
    // a hit are the rest of this address.
    EntityCamNode* found = g_cam_anchor[slot].next;
    while (found != nullptr && !cam_name_match(found->name, name))
    {
        found = found->next;
    }
    if (found == nullptr)
    {
        g_cam_active[slot] = nullptr;
        return 0;
    }
    if (g_cam_active[slot] != nullptr)
    {
        copy_cam_name(g_cam_saved_name[slot], g_cam_active[slot]->name);
    }
    g_cam_active[slot] = found;
    return 1;
}

void fn_48c70(EntityCamNode* node)
{
    // Xbox 0x00048C70
    // EAX is the camera record at body+0x894. Unlink it from the view list.
    // If it is the bound record in one of the four DAT_004e16a0 slots, try
    // cam_func_48cf0 with the saved name at DAT_004e16b0. When that misses and
    // the slot list at DAT_004e1684 is still live, bind the first record by
    // its own name. An empty list clears the slot.
    if (node->prev != nullptr)
    {
        node->prev->next = node->next;
    }
    if (node->next != nullptr)
    {
        node->next->prev = node->prev;
    }
    node->prev = nullptr;
    node->next = nullptr;

    for (i32 slot = 0; slot < 4; ++slot)
    {
        if (g_cam_active[slot] != node)
        {
            continue;
        }
        if (cam_func_48cf0(slot, g_cam_saved_name[slot]) == 0)
        {
            if (g_cam_anchor[slot].next != nullptr)
            {
                cam_func_48cf0(slot, g_cam_anchor[slot].next->name);
                return;
            }
            g_cam_active[slot] = nullptr;
        }
        return;
    }
}

namespace
{

// DAT_00545AEC. Profile 4 leaves the aim height alone; every other value adds 6.
i32 g_look_profile = 0;

constexpr float kLookEps = std::bit_cast<float>(0x3456BF95u);  // 0x0032B27C, 2e-7
constexpr float kDeg2Rad = std::bit_cast<float>(0x3C8EFA35u);  // 0x0032B0FC
constexpr float kPitchBiasDeg = std::bit_cast<float>(0xC0000000u); // DAT_0043401C, -2
constexpr float kAimLift = std::bit_cast<float>(0x40C00000u);  // DAT_00434020, 6
constexpr float kPi = std::bit_cast<float>(0x40490FDBu);       // 0x0032B148
constexpr float kTwoPi = std::bit_cast<float>(0x40C90FDBu);    // 0x0032B298

// 0x00105AA0. fmod into one turn, then fold into [-pi, pi).
float wrap_half_turn(float angle)
{
    float wrapped = std::fmod(angle, kTwoPi);
    if (wrapped < -kPi)
    {
        wrapped += kTwoPi;
    }
    if (wrapped >= kPi)
    {
        wrapped -= kTwoPi;
    }
    return wrapped;
}

// DAT_0050dd80. Three BSS floats. Message 9 copies them onto the actor and the body.
Vec3 g_anchor{};

// DAT_00511188 indexes DAT_005111AC. Message 99 writes the contact and does not bump the index.
// 0x00120480 stores the hitting object at DAT_0051118C[index], then bumps the index.
// Eight pointers fit before DAT_005111AC; further stores spill into that array on Xbox.
i32 g_contact_slot = 0;
Vec3 g_contact_points[64]{};

// Object walked from DAT_00511B04. Retail stores the link at object+0x110 (prev)
// and +0x114 (next); the head value is the first link, and the walk follows +4.
// The push is vtable +0x34. The port keeps the object and its next pointer together.
struct ShapeProbe
{
    i32 (*push)(ShapeProbe& self, const Actor* actor, const Vec3* query, EntityCylinderBound& merged,
                EntityCylinderBound* shapes, i32 count, Vec3& out) = nullptr;
    ShapeProbe* next = nullptr;
};

ShapeProbe* g_contact_owners[64]{};

// DAT_00511AA4. Nodes are walked through vtable +0xC when a latch mask leaves 0x3F.
struct LatchNode
{
    void (*handle_message)(LatchNode& self, const Message& msg) = nullptr;
    LatchNode* next = nullptr;
};

struct SegmentProbe;

// Intrusive head. Retail prev is +0 and the first node is +4. Walks follow +4.
struct ListHead
{
    void* prev = nullptr;
    void* next = nullptr;
};

// DAT_00511AA0 through DAT_00511B58. cam_11c480's first call (0x001233C0) zeros
// this whole block. The eight bins at DAT_00511AA8 take the 0xC-byte records
// that start at object+0x124. Segment nodes sit at object+0x1e0, shape probes
// at object+0x110, and the later heads hang off +0x900, +0x918, +0x934, +0x93c,
// +0x944, +0x94c, and +0x908. DAT_00511B18 is the first node 0x00123020 walks.
struct WorldLists
{
    LatchNode* latch_prev = nullptr; // DAT_00511AA0
    LatchNode* latch = nullptr;      // DAT_00511AA4
    ListHead bins[8]{};              // DAT_00511AA8
    ListHead anchor{};               // DAT_00511AE8. Next is DAT_00511AEC.
    SegmentProbe* segment_prev = nullptr; // DAT_00511AF0
    SegmentProbe* segment = nullptr;      // DAT_00511AF4
    ListHead wide{};                      // DAT_00511AF8. Node is object+0x1e8.
    ShapeProbe* shape_prev = nullptr;     // DAT_00511B00
    ShapeProbe* shape = nullptr;          // DAT_00511B04
    ListHead view{};                      // DAT_00511B08. Node is object+0x900.
    ListHead actor_link{};                // DAT_00511B10. Node is object+0x918.
    void* release = nullptr;              // DAT_00511B18
    ListHead body_a{};                    // DAT_00511B1C. Node is object+0x934.
    ListHead body_b{};                    // DAT_00511B24. Node is object+0x934.
    ListHead body_c{};                    // DAT_00511B2C. Node is object+0x934.
    ListHead mark{};                      // DAT_00511B34
    ListHead case_head{};                 // DAT_00511B3C. Node is object+0x93c.
    ListHead alt{};                       // DAT_00511B44. Node is object+0x944.
    ListHead slot{};                      // DAT_00511B4C. Node is object+0x94c.
    ListHead child{};                     // DAT_00511B54. Node is object+0x908.
};

WorldLists g_world{};

constexpr float kHitEps = std::bit_cast<float>(0x3727C5ACu); // 0x0032B230, 1e-5
constexpr float kHitPad = std::bit_cast<float>(0x3C23D70Au); // 0x0032B124, 0.01

// Xbox 0x000253B0
void forward_child(EntityChild* child, const Message& msg)
{
    if (child == nullptr || child->handle_message == nullptr)
    {
        return;
    }
    child->handle_message(*child, msg);
}

// Xbox 0x000253B0
u32 latch_mask(const Entity& body)
{
    u32 mask = 0;
    for (int i = 0; i < 6; ++i)
    {
        if (body.latch[i] != 0)
        {
            mask |= 1u << i;
        }
    }
    return mask;
}

// Xbox 0x000253B0
void broadcast_latch(Actor* actor)
{
    Message note;
    note.id = 0x47;
    note.arg = actor;
    for (LatchNode* node = g_world.latch; node != nullptr; node = node->next)
    {
        if (node->handle_message != nullptr)
        {
            node->handle_message(*node, note);
        }
    }
}

// Xbox 0x000253B0
// 0x0001FC40 increments the six counts at +0x260. 0x0001FD20 decrements them.
// Both read the mask first (0x0001F7A0). Leaving 0x3F broadcasts note 0x47
// with the actor as the payload, through the list at DAT_00511AA4.
void adjust_latches(Entity& body, int step)
{
    const u32 before = latch_mask(body);
    for (u8& count : body.latch)
    {
        if (step > 0)
        {
            ++count;
        }
        else
        {
            --count;
        }
    }
    const u32 after = latch_mask(body);
    if (before == 0x3Fu && after != before)
    {
        broadcast_latch(body.actor);
    }
}

// Xbox 0x000253B0
// 0x000275E0. Frees the children at +0x314 and +0x744 (0x001075C0), then
// clears +0x740 and the word at +0x748.
void release_children(Entity& body)
{
    body.child_314 = nullptr;
    body.child_744 = nullptr;
    body.slot_740 = 0;
    body.mark_748 = 0;
}

// Xbox 0x000253B0
void clear_bit(Entity& body, int bit)
{
    if (body.bits == nullptr || body.bits->bytes == nullptr)
    {
        return;
    }
    const int index = bit >> 3;
    const u8 mask = static_cast<u8>(1u << (bit & 7));
    body.bits->bytes[index] = static_cast<u8>(body.bits->bytes[index] & static_cast<u8>(~mask));
}

// Xbox 0x000253B0
// 0x0013F720. A missing live pointer returns. A missing gate calls 0x0013EDD0
// and returns. Otherwise the dword is inserted once into the list at +0x18.
void watch_insert(EntityWatch& watch, void* token)
{
    if (watch.live == nullptr)
    {
        return;
    }
    if (watch.gate == nullptr)
    {
        return;
    }
    for (void* item : watch.tokens)
    {
        if (item == token)
        {
            return;
        }
    }
    watch.tokens.push_back(token);
}

// Xbox 0x000253B0
// 0x000277E0. Vtable +0x20 must return a non-zero word, and +0x73c must be set,
// before 0x00027510 and 0x000276A0. The default pick returns 0.
void release_parked(Entity& body)
{
    if (body.type == nullptr || body.type->methods.pick_actor == nullptr)
    {
        return;
    }
    if (body.type->methods.pick_actor(body, 0) == 0 || body.record_73c == nullptr)
    {
        return;
    }
}

// Xbox 0x000253B0
// Message 99. The input delta is zero. 0x00120760 keeps the body when its Y
// sits in the slab and its XZ is inside the combined radius, then 0x00120250
// scales the horizontal separation out to that radius. A hit adds the
// correction to body+0x18 and stores the midpoint at DAT_005111AC.
bool push_out(Entity& body, const Message& msg)
{
    if (body.actor == nullptr)
    {
        return false;
    }
    const float px = body.position.x;
    const float py = body.position.y;
    const float pz = body.position.z;
    const float height = body.actor->radius_y * body.scale_884;
    const float reach = body.actor->radius_xz * body.scale_888;
    const float slab_top = msg.hit_center.y + msg.hit_extent_y;
    if (py > slab_top || msg.hit_center.y > height + py)
    {
        return false;
    }
    const float combined = msg.hit_radius + reach;
    float dx = px - msg.hit_center.x;
    float dz = pz - msg.hit_center.z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist < kHitEps)
    {
        dx = 1.f;
        dz = 0.f;
        dist = combined - kHitPad;
    }
    if (combined <= dist)
    {
        return false;
    }
    const float len2 = dx * dx + dz * dz;
    if (len2 != 0.f)
    {
        const float scale = combined / std::sqrt(len2);
        dx *= scale;
        dz *= scale;
    }
    body.position.x = msg.hit_center.x + dx;
    body.position.z = msg.hit_center.z + dz;

    const float mid_y = msg.hit_extent_y * 0.5f + msg.hit_center.y;
    const float sample_y = py + height * 0.5f;
    const i32 slot = g_contact_slot & 63;
    g_contact_points[slot].x = (px - msg.hit_center.x) * 0.5f + msg.hit_center.x;
    g_contact_points[slot].y = (sample_y - mid_y) * 0.5f + mid_y;
    g_contact_points[slot].z = (pz - msg.hit_center.z) * 0.5f + msg.hit_center.z;
    return true;
}

// Xbox 0x000253B0
// 0x0001B4C0 / 0x0001B570 when message 0x7C names the actor. Clears +0x10,
// bit 2 of bit-byte 4, and +0x288 while the case link is set, and clears
// +0x14 while +0x4c is set. The link objects themselves stay.
void detach_actor(Entity& body)
{
    if (body.case_link != nullptr)
    {
        body.link_10 = nullptr;
        if (body.bits != nullptr && body.bits->bytes != nullptr)
        {
            body.bits->bytes[4] = static_cast<u8>(body.bits->bytes[4] & 0xFBu);
        }
        body.mark_288 = 0;
    }
    if (body.link_4c != nullptr)
    {
        body.link_14 = nullptr;
    }
}

// Xbox 0x000253B0
// Default body message. Children at +0x744 and +0x314 are forwarded through
// vtable +0xC first. Ids from 6 through 0x7C are in the jump table; every
// other id, and the table slots with no body, return. Message 7 enters
// 0x0001A8F0 (a second switch on the word at [payload+0xC] - 2). Message 6
// and 0x7B notify through 0x0001F890 / 0x0001F850 when +0x2c8 has a link.
// Message 0x23 samples the actor into +0x18 and +0x72c (0x000257C0).
bool body_message(Entity& body, const Message& msg)
{
    forward_child(body.child_744, msg);
    forward_child(body.child_314, msg);
    Actor* actor = body.actor;
    void* token = msg.arg;

    switch (msg.id)
    {
    case 6:
        // 0x0001F890. +0x2c8 and its link at +0x10 must both be set before
        // 0x0013EBE0 appends the payload. That list is not installed.
        return true;
    case 7:
        // 0x0001A8F0 switches on the word at [message+0xC]+0xC, minus 2.
        return true;
    case 9:
        release_children(body);
        if (actor == nullptr)
        {
            return true;
        }
        actor->anchor = g_anchor;
        body.anchor = g_anchor;
        adjust_latches(body, 1);
        return true;
    case 10:
        if (actor == nullptr)
        {
            return true;
        }
        if (actor->parked_gate <= 0)
        {
            release_parked(body);
        }
        adjust_latches(body, -1);
        return true;
    case 0x14:
        if (body.note_760 == token)
        {
            body.note_760 = nullptr;
            body.timer_758 = 0.f;
        }
        if (body.note_7f0 == token)
        {
            body.note_7f0 = nullptr;
            body.timer_7e8 = 0.f;
        }
        return true;
    case 0x23:
        if (actor == nullptr)
        {
            return true;
        }
        body.position = actor_world_position(*actor);
        body.world_euler = actor_world_euler(*actor);
        if ((actor->intro_flags & 2u) != 0)
        {
            // 0x0001D780. Bit 2 of 0x00042500 arms the world query. No query object.
        }
        return true;
    case 0x47:
        if (body.watch != nullptr)
        {
            watch_insert(*body.watch, token);
        }
        return true;
    case 0x4b:
        if (actor == nullptr)
        {
            return true;
        }
        fn_41730(*actor, actor->parked_pos);
        if (actor->euler_out != nullptr && actor->euler_out->actor != nullptr)
        {
            actor->euler_out->position = actor_world_position(*actor->euler_out->actor);
        }
        actor->euler.x = wrap_half_turn(actor->parked_euler.x);
        actor->euler.y = wrap_half_turn(actor->parked_euler.y);
        actor->euler.z = wrap_half_turn(actor->parked_euler.z);
        actor->xform_valid = 0;
        if (actor->euler_out != nullptr && actor->euler_out->actor != nullptr)
        {
            actor->euler_out->euler = actor_world_euler(*actor->euler_out->actor);
        }
        body.motion_6b = 0;
        body.motion_6a = 0;
        body.anim_94 = body.anim_96;
        clear_bit(body, 0x1b);
        if (body.mark_74a != 0 && body.record_73c != nullptr)
        {
            // 0x00027650. 0x00027510 must accept +0x73c, then 0x000276A0.
        }
        return true;
    case 0x4e:
        body.slot_70 = msg.arg_i;
        return true;
    case 0x4f:
        body.mark_87e = msg.arg_byte;
        body.mark_880 = 2;
        return true;
    case 99:
        push_out(body, msg);
        return true;
    case 0x7b:
        // 0x0001F850. Same link as message 6, and 0x00042500 of the payload
        // must include 0x142, before 0x0013EBE0.
        return true;
    case 0x7c:
        if (body.link_10 == token)
        {
            body.link_10 = nullptr;
        }
        if (body.link_14 == token)
        {
            body.link_14 = nullptr;
        }
        if (token == static_cast<void*>(actor))
        {
            detach_actor(body);
        }
        else
        {
            if (static_cast<void*>(body.case_link) == token)
            {
                body.case_link = nullptr;
            }
            if (body.link_4c == token)
            {
                body.link_4c = nullptr;
            }
        }
        if (body.link_50 == token)
        {
            body.link_50 = nullptr;
            body.link_54 = nullptr;
        }
        if (body.watch != nullptr)
        {
            watch_insert(*body.watch, token);
        }
        if (body.link_274 == token)
        {
            body.link_274 = nullptr;
        }
        if (body.link_270 == token)
        {
            body.link_270 = nullptr;
        }
        if (body.link_718 == token)
        {
            body.link_718 = nullptr;
        }
        if (token == static_cast<void*>(actor))
        {
            release_children(body);
        }
        return true;
    default:
        return false;
    }
}

} // namespace

float fn_152bb0(float value)
{
    // Xbox 0x00152BB0
    // Absolute value. Non-negative inputs, including zero, are returned as
    // stored. A negative input is returned as 0 - value.
    if (0.f <= value)
    {
        return value;
    }
    return 0.f - value;
}

float fn_1534c0(float value)
{
    // Xbox 0x001534C0
    // Reciprocal square root. 0x00153450 seeds from
    // ((0x7F000000 - bits) >> 1) + 0x1FC00000, multiplies by 1/sqrt(2) when
    // bit 0x08000000 is clear, and takes two Newton steps. Two further steps
    // refine that estimate. Every input, including zero and negatives, runs
    // the same steps.
    const u32 bits = std::bit_cast<u32>(value);
    float y = std::bit_cast<float>(((0x7F000000u - bits) >> 1) + 0x1FC00000u);
    if ((bits & 0x08000000u) == 0u)
    {
        y *= std::bit_cast<float>(u32{0x3F3504F3u});
    }
    const float half = value * 0.5f;
    y = (1.5f - half * y * y) * y;
    y = (1.5f - half * y * y) * y;
    y = (1.5f - half * y * y) * y;
    y = (1.5f - half * y * y) * y;
    return y;
}

namespace
{

constexpr int kPadBitPorts = 4;
constexpr int kPadBitCount = 32;

// DAT_00434288. 0x00155470 copies DAT_0032B0F8 (1) before the first poll.
float g_pad_hold_delay = 1.f;

// DAT_00434290. 0x00155470 stores 0. The repeat timer pulses at this level.
float g_pad_repeat_gate = 0.f;

// One port of the 0x13c packet. hold[32] is +0x14. repeat[32] is +0x94.
struct PadBitPort
{
    u32 down = 0;      // +0x00 DAT_00434298
    u32 down_prev = 0; // +0x04 DAT_0043429C
    u32 pulse = 0;     // +0x10 DAT_004342A8
    float hold[kPadBitCount]{};
    float repeat[kPadBitCount]{};
};

PadBitPort g_pad_bits[kPadBitPorts];

} // namespace

void fn_154ba0(float dt)
{
    // Xbox 0x00154BA0
    // 0x001555B0 calls this after it has written each port's button word.
    // A clear bit stores 0 in that hold timer and clears the pulse bit. A set
    // bit adds dt to the hold timer. While the hold is still under
    // DAT_00434288 the pulse bit stays as it was. The frame the hold reaches
    // that delay sets the pulse bit and stores 0 in the repeat timer. After
    // that, each call adds dt to the repeat timer. Under DAT_00434290 the
    // pulse bit clears. At that gate the pulse bit sets and the repeat timer
    // subtracts DAT_00434288. The button word is then copied to +0x04. A clear
    // bit leaves its repeat timer unchanged.
    const float delay = g_pad_hold_delay;
    const float gate = g_pad_repeat_gate;
    for (PadBitPort& port : g_pad_bits)
    {
        for (int bit_index = 0; bit_index < kPadBitCount; ++bit_index)
        {
            const u32 bit = 1u << bit_index;
            if ((port.down & bit) == 0)
            {
                port.hold[bit_index] = 0.f;
                port.pulse &= ~bit;
                continue;
            }
            const float previous = port.hold[bit_index];
            const float held = previous + dt;
            port.hold[bit_index] = held;
            if (held < delay)
            {
                continue;
            }
            if (previous < delay)
            {
                port.pulse |= bit;
                port.repeat[bit_index] = 0.f;
                continue;
            }
            const float next = port.repeat[bit_index] + dt;
            port.repeat[bit_index] = next;
            if (next < gate)
            {
                port.pulse &= ~bit;
                continue;
            }
            port.pulse |= bit;
            port.repeat[bit_index] = next - delay;
        }
        port.down_prev = port.down;
    }
}

i32 fn_105e20(float x, float y, float z, float& yaw, float& pitch)
{
    // Xbox 0x00105E20
    // EAX is the direction. The first out is yaw = atan2(x, z). The second is
    // pitch = atan2(-y, horizontal length), where that length is sqrt(x^2+z^2)
    // and stays 0 when both axes are quiet. 0x00152BB0 is the absolute value
    // and 0x001530E0 is atan2. A component no larger than 2e-7 (0x0032B27C)
    // counts as zero. A dead XZ forces yaw to 0 and the result to 0. A dead Y
    // together with that zero length forces pitch to 0 and returns 0. A live
    // XZ returns 1 after both angles are written.
    const bool xz = fn_152bb0(x) > kLookEps || fn_152bb0(z) > kLookEps;
    float horiz = 0.f;
    i32 live = 1;
    if (xz)
    {
        horiz = std::sqrt(x * x + z * z);
        yaw = std::atan2(x, z);
    }
    else
    {
        yaw = 0.f;
        live = 0;
    }
    if (fn_152bb0(y) <= kLookEps && fn_152bb0(horiz) <= kLookEps)
    {
        pitch = 0.f;
        return 0;
    }
    pitch = std::atan2(-y, horiz);
    return live;
}

void fn_72ba0(EntityLookAt& cam)
{
    // Xbox 0x00072BA0
    // ESI is the actor at camera+8. 0x000412A0 always writes its world position.
    // A zero byte at +0x308 returns after that. Otherwise the aim point at +0x2fc
    // is subtracted from that position. DAT_00545AEC == 4 adds nothing to aim Y;
    // any other profile adds 6 (DAT_00434020). 0x00105E20 turns the delta into
    // yaw and pitch. Actor euler at +0x14 becomes
    // (-(pitch - 2 degrees), yaw + pi, camera+0x8f4), each wrapped by 0x00105AA0.
    // Actor+0x2a8 is cleared. World euler of the actor at [actor+0x95c]+8 is
    // stored at that object +0x72c. A null actor or a null +0x95c faults on Xbox.
    if (cam.actor == nullptr)
    {
        return;
    }
    const Vec3 world = actor_world_position(*cam.actor);
    if (cam.enabled == 0)
    {
        return;
    }
    float lift = kAimLift;
    if (g_look_profile == 4)
    {
        lift = 0.f;
    }
    float yaw = 0.f;
    float pitch = 0.f;
    fn_105e20(cam.aim.x - world.x, cam.aim.y + lift - world.y, cam.aim.z - world.z, yaw, pitch);
    cam.actor->euler.x = wrap_half_turn(-(pitch + kPitchBiasDeg * kDeg2Rad));
    cam.actor->euler.y = wrap_half_turn(yaw + kPi);
    cam.actor->euler.z = wrap_half_turn(cam.roll);
    cam.actor->xform_valid = 0;
    if (cam.actor->euler_out != nullptr && cam.actor->euler_out->actor != nullptr)
    {
        cam.actor->euler_out->euler = actor_world_euler(*cam.actor->euler_out->actor);
    }
}

namespace
{

// DAT_004312dc / DAT_004312c4. Freeze above 0 zeros the step (BanjoCam).
float g_sim_freeze = 0.f;
float g_sim_scale = 1.f;

struct ChannelKey
{
    float duration;
    float value;
    u32 channel; // 2 marks the row past the last live key
};

// Rows at 0x00338F64, 0x00338F88, 0x00338EE0, 0x00338EA4. Stride is three dwords.
constexpr ChannelKey kKeys0[] = {
    {0.f, 0.1f, 0},
    {0.034f, 0.1f, 1},
    {0.f, 0.f, 2},
};
constexpr ChannelKey kKeys1[] = {
    {0.f, 0.1f, 0},
    {0.034f, 0.1f, 1},
    {2.f, 0.f, 2},
};
constexpr ChannelKey kKeys2[] = {
    {2.f, 0.1f, 0},  {0.034f, 0.1f, 1}, {5.f, 0.06f, 0}, {0.034f, 0.06f, 1},
    {0.2f, 0.09f, 0}, {0.034f, 0.09f, 1}, {1.f, 0.1f, 0}, {0.034f, 0.1f, 1},
    {5.f, 0.1f, 0},  {0.034f, 0.1f, 1}, {2.f, 0.f, 2},
};
constexpr ChannelKey kKeys3[] = {
    {0.3f, 0.1f, 0},
    {0.034f, 0.1f, 1},
    {0.3f, 0.1f, 0},
    {0.034f, 0.1f, 1},
    {2.f, 0.f, 2},
};

static_assert(std::bit_cast<u32>(0.034f) == 0x3D0B4396u);
static_assert(std::bit_cast<u32>(0.06f) == 0x3D75C28Fu);
static_assert(std::bit_cast<u32>(0.09f) == 0x3DB851ECu);
static_assert(std::bit_cast<u32>(0.1f) == 0x3DCCCCCDu);
static_assert(std::bit_cast<u32>(0.2f) == 0x3E4CCCCDu);
static_assert(std::bit_cast<u32>(0.3f) == 0x3E99999Au);

std::span<const ChannelKey> keys_for(u8 mode)
{
    switch (mode)
    {
    case 0:
        return kKeys0;
    case 1:
        return kKeys1;
    case 2:
        return kKeys2;
    case 3:
        return kKeys3;
    default:
        return {};
    }
}

} // namespace

void fn_1d850(EntityChannelAnim& anim, float dt)
{
    // Xbox 0x0001D850
    // ESI is the camera. Called from the type slot at 0x00072AC0 when +0xfc is set.
    // Phase 1 returns. Otherwise the step is host+0x92c (0 when host+0x930 > 0)
    // times DAT_005096C0 * DAT_004312C4, or 0 when DAT_004312DC > 0.
    // Each expiry writes the live key's value into +0xe4 + channel*0xc, sets the
    // mark byte at +8, and loads the following key's duration into +0x8c.
    // Channel 2 on that following key ends the row: mode 0 latches phase 1,
    // mode 1 restarts on the mode-2 row, and any other mode restarts at index 0.
    if (anim.phase == 1)
    {
        return;
    }
    float span = 0.f;
    if (!(g_sim_freeze > 0.f))
    {
        span = dt * g_sim_scale;
    }
    float rate = 0.f;
    if (!(anim.host_gate > 0.f))
    {
        rate = anim.host_rate;
    }
    float remain = rate * span;
    while (remain > 0.f)
    {
        anim.timer -= remain;
        if (anim.timer > 0.f)
        {
            return;
        }
        remain = std::fabs(anim.timer);
        const std::span<const ChannelKey> keys = keys_for(anim.mode);
        if (keys.empty() || static_cast<size_t>(anim.index) + 1u >= keys.size())
        {
            return;
        }
        const ChannelKey& cur = keys[anim.index];
        const ChannelKey& next = keys[anim.index + 1u];
        if (cur.channel < 2)
        {
            anim.keys[cur.channel].mark = 1;
            anim.keys[cur.channel].value = cur.value;
        }
        anim.timer = next.duration;
        if (next.channel == 2)
        {
            if (anim.mode == 0)
            {
                anim.phase = 1;
                // The mode-0 terminal duration is 0. A positive overshoot would
                // subtract from that 0 and select this same key again.
                if (!(anim.timer > 0.f))
                {
                    return;
                }
            }
            else
            {
                anim.index = 0;
                if (anim.mode == 1)
                {
                    anim.mode = 2;
                    anim.timer = kKeys2[0].duration;
                    anim.phase = 2;
                }
            }
        }
        else
        {
            anim.index = static_cast<u8>(anim.index + 1u);
        }
    }
}

void fn_1ede0(EntityChannelAnim& anim, float dt)
{
    // Xbox 0x0001EDE0
    // EAX is the camera. 0x00072AC0 jumps here after 0x0001D850 when +0xfc is set.
    // The step is host+0x92c (0 when host+0x930 > 0) times DAT_005096C0 * DAT_004312C4,
    // or 0 when DAT_004312DC > 0. Each slot at +0xe4 (stride 0x0c) reads the mark at +8
    // and moves the blend at +4 by step/value:
    //   1 rises to 1, then the mark becomes 2
    //   2 becomes 3 and holds the blend for that frame
    //   3 and 4 fall to 0, then the mark becomes 0
    //   5 rises to 1, then the mark becomes 0
    // Any other mark leaves the blend stored as it was.
    float scale = 0.f;
    if (!(g_sim_freeze > 0.f))
    {
        scale = dt * g_sim_scale;
    }
    float rate = 0.f;
    if (!(anim.host_gate > 0.f))
    {
        rate = anim.host_rate;
    }
    const float step = rate * scale;
    for (EntityChannelKey& key : anim.keys)
    {
        float blend = key.blend;
        switch (key.mark)
        {
        case 1:
            blend = blend + step / key.value;
            if (blend > 1.f)
            {
                key.mark = 2;
                blend = 1.f;
            }
            key.blend = blend;
            break;
        case 2:
            key.mark = 3;
            break;
        case 3:
        case 4:
            blend = blend - step / key.value;
            if (blend < 0.f)
            {
                blend = 0.f;
                key.mark = 0;
            }
            key.blend = blend;
            break;
        case 5:
            blend = blend + step / key.value;
            if (blend > 1.f)
            {
                key.mark = 0;
                blend = 1.f;
            }
            key.blend = blend;
            break;
        default:
            break;
        }
    }
}

void fn_1ea90(EntityChannelEase& cam, float dt)
{
    // Xbox 0x0001EA90
    // EDX is the camera. 0x00072AC0 calls this when +0x154 > 0, then 0x0001D850
    // when +0xfc is set. Each row's target is cleared, then the bank at
    // +0x100 + index*8 (index from +0x158) copies its value onto the first row
    // with the same id. The value at +4 then moves toward that target by at
    // most host_rate * DAT_005096C0 * DAT_004312C4 * 0.75 (0x0032B2AC).
    // DAT_004312DC > 0 zeros the scale; host+0x930 > 0 zeros the rate. A zero
    // step leaves the value in place. A step past the gap snaps to the target.
    // The target is cleared again. A bank id with no row faults on Xbox.
    for (EntityChannelRow& row : cam.rows)
    {
        row.target = 0.f;
    }
    if (cam.bank >= 0 && cam.bank < 10)
    {
        for (const EntityChannelPair& pair : cam.banks[cam.bank])
        {
            for (EntityChannelRow& row : cam.rows)
            {
                if (row.id == pair.id)
                {
                    row.target = pair.value;
                    break;
                }
            }
        }
    }
    float scale = 0.f;
    if (!(g_sim_freeze > 0.f))
    {
        scale = dt * g_sim_scale;
    }
    float rate = 0.f;
    if (!(cam.host_gate > 0.f))
    {
        rate = cam.host_rate;
    }
    float step = rate * scale;
    if (!cam.rows.empty())
    {
        step *= 0.75f;
    }
    for (EntityChannelRow& row : cam.rows)
    {
        const float delta = row.target - row.value;
        float next = row.value;
        if (step != 0.f)
        {
            if (step > std::fabs(delta))
            {
                next = row.target;
            }
            else if (delta >= 0.f)
            {
                next = row.value + step;
            }
            else
            {
                next = row.value - step;
            }
        }
        row.value = next;
        row.target = 0.f;
    }
}

u32 fn_1fe00(const EntityMaskHost& host)
{
    // Xbox 0x0001FE00
    // EAX is the camera. cam_72cb0 calls this and pushes the mask into 0x001226C0.
    // The object at camera+8 holds the span at +0x980, the flag at +0x93c, and the
    // mode at +0xcdc. A null span reads as code 0. Otherwise the u16 at
    // data + index * stride is the code. When the span's +6 equals +2, or the
    // element base is null, retail loads that u16 from address 0 and faults.
    // Code 0x15 keeps 0xC4000. Mode 2, or a nonzero +0x93c, clears bit 0x4000.
    // Modes 1, 4, and 5 clear bit 0x40000. Mode 3 clears bit 0x80000.
    // Any other mode keeps 0xC4000. The jump table's mode-2 slot is that full
    // mask, and mode 2 has already returned above.
    constexpr u32 kMask = 0xC4000u;
    u16 code = 0;
    if (host.span != nullptr && host.span->end != host.span->mark && host.span->data != nullptr)
    {
        const size_t offset = size_t(host.span->index) * size_t(host.span->stride);
        std::memcpy(&code, host.span->data + offset, sizeof(code));
    }
    if (code != 0x15)
    {
        if (host.mode == 2 || host.flag != 0)
        {
            return kMask & ~0x4000u;
        }
        switch (host.mode)
        {
        case 1:
        case 4:
        case 5:
            return kMask & ~0x40000u;
        case 3:
            return kMask & ~0x80000u;
        default:
            break;
        }
    }
    return kMask;
}

namespace
{

// DAT_005077F8. Row of the triple table at 0x0042A460. 0 is 4:3, 1 is 16:9, 2 is 2.35.
i32 g_aspect_preset = 0;
// DAT_00509704. Zero scales the chosen aspect by 1.1 (0x0032B1A0). Any other byte leaves it.
u8 g_aspect_exact = 0;

// DAT_005077A8. 0x000F5900 points this at the display record and copies the viewport
// (DAT_005096E8 / DAT_005096EA) into the size fields. A null pointer faults on Xbox.
struct DisplayView
{
    i32 aspect_mode = 0; // +0x3c. 3 selects g_aspect_preset.
    i32 origin_x = 0;    // +0x48
    i32 origin_y = 0;    // +0x4c
    i32 width = 0;       // +0x50
    i32 height = 0;      // +0x54
};

DisplayView g_display{};
DisplayView* g_display_view = &g_display;

// 0x0042A460 stride 0x0c: aspect, sensor height, default focal. Index 3 is the sentinel row.
struct AspectTriple
{
    float aspect;
    float sensor;
    float focal;
};

constexpr AspectTriple kAspectTable[] = {
    {std::bit_cast<float>(0x3FAAAAABu), std::bit_cast<float>(0x4178B780u), std::bit_cast<float>(0x416EE3E8u)},
    {std::bit_cast<float>(0x3FE38E39u), std::bit_cast<float>(0x41C00625u), std::bit_cast<float>(0x41B86FF5u)},
    {std::bit_cast<float>(0x40166666u), std::bit_cast<float>(0x41C00625u), std::bit_cast<float>(0x41B86FF5u)},
    {0.f, 0.f, 0.f},
};

constexpr float kAspectPad = std::bit_cast<float>(0x3F8CCCCDu); // 0x0032B1A0, 1.1
constexpr float kFovScale = std::bit_cast<float>(0x42E52EE0u);  // 0x0032B360, 360/pi
constexpr float kHalfRad = std::bit_cast<float>(0x3C0EFA35u);   // 0x0032B358, pi/360
constexpr float kNegOne = std::bit_cast<float>(0xBF800000u);    // 0x0032B120

const AspectTriple& aspect_triple(i32 index)
{
    if (index < 0 || index >= 4)
    {
        return kAspectTable[3];
    }
    return kAspectTable[index];
}

float chosen_aspect(i32 slot)
{
    i32 index = slot;
    if (slot == 3)
    {
        index = g_aspect_preset;
    }
    float aspect = aspect_triple(index).aspect;
    if (g_aspect_exact == 0)
    {
        aspect *= kAspectPad;
    }
    return aspect;
}

// cvttss2si. Out of range and NaN become the indefinite integer.
i32 trunc_i32(float v)
{
    if (!(v >= -2147483648.f && v < 2147483648.f))
    {
        return static_cast<i32>(0x80000000u);
    }
    return static_cast<i32>(v);
}

// 0x000F5760. ECX is the aspect slot, ESI and the stack pointer receive the span and the origin.
void fit_vertical(i32 slot, i32& span, i32& pos)
{
    const float ref = aspect_triple(g_aspect_preset).aspect;
    const float selected = chosen_aspect(slot);
    const i32 limit = g_display_view->height;
    span = trunc_i32(static_cast<float>(limit) * (ref / selected));
    if (span > limit)
    {
        span = limit;
    }
    const i32 remain = static_cast<i32>(static_cast<u32>(limit) - static_cast<u32>(span));
    pos = trunc_i32(static_cast<float>(remain) * 0.5f + static_cast<float>(g_display_view->origin_y));
}

// 0x000F56C0. Same slot. The span is the wide axis and the second out is its centered origin.
void fit_horizontal(i32 slot, i32& span, i32& pos)
{
    const float ref = aspect_triple(g_aspect_preset).aspect;
    const float selected = chosen_aspect(slot);
    const i32 limit = g_display_view->width;
    span = trunc_i32(static_cast<float>(limit) * (selected / ref));
    if (span > limit)
    {
        span = limit;
    }
    const i32 remain = static_cast<i32>(static_cast<u32>(limit) - static_cast<u32>(span));
    pos = trunc_i32(static_cast<float>(remain) * 0.5f + static_cast<float>(g_display_view->origin_x));
}

} // namespace

void fn_f57f0(float focal, i32 aspect_slot, i32& fit_w, i32& fit_x, i32& fit_h, i32& fit_y, float& fov_deg,
              float& pixel_aspect, float& focal_px)
{
    // Xbox 0x000F57F0
    // EDI is the aspect slot. ECX receives the fitted height and EAX its centered y.
    // The stack receives the fitted width and its x, the vertical FOV in degrees,
    // the pixel aspect, and the focal length in pixels. 0x000F5760 runs first, then
    // 0x000F56C0. The sensor row is display +0x3c (3 means DAT_005077F8). A focal of
    // -1 uses the table focal for EDI the same way. FOV is
    // 2 * atan(sensor / (2 * focal)) degrees. The pixel focal is (height / 2) *
    // cot(half of that angle): sin is 0x00153320, cos is 0x00153230, atan is
    // 0x00152780. Pixel aspect divides width/height by the preset row's aspect.
    fit_vertical(aspect_slot, fit_h, fit_y);
    fit_horizontal(aspect_slot, fit_w, fit_x);

    i32 sensor_slot = g_display_view->aspect_mode;
    if (sensor_slot == 3)
    {
        sensor_slot = g_aspect_preset;
    }
    float used = focal;
    if (used == kNegOne)
    {
        i32 focal_slot = aspect_slot;
        if (focal_slot == 3)
        {
            focal_slot = g_aspect_preset;
        }
        used = aspect_triple(focal_slot).focal;
    }
    fov_deg = std::atan(aspect_triple(sensor_slot).sensor / (used * 2.f)) * kFovScale;
    const float half = fov_deg * kHalfRad;
    const float height = static_cast<float>(g_display_view->height);
    const float width = static_cast<float>(g_display_view->width);
    focal_px = (height * std::cos(half) / std::sin(half)) * 0.5f;
    pixel_aspect = (width / height) / aspect_triple(g_aspect_preset).aspect;
}

void fn_1da60(EntityMethods& methods)
{
    // Xbox 0x0001DA60
    // EAX is the type descriptor. Writes the 24 default Body methods. A static
    // ctor then overwrites the slots it owns. Slot 0 reports size 0x894 and
    // extra 0x7C8. Slot 6 is camera apply 0x00025AE0. Slot 7 returns 1.
    methods.query_size = body_query_size;
    methods.tick = body_tick;
    methods.shutdown = body_shutdown;
    methods.handle_message = body_message;
    methods.relay_message = body_relay;
    methods.step_channels = body_step_channels;
    methods.apply = body_apply;
    methods.gate = body_gate;
    methods.pick_actor = body_pick_actor;
    methods.pick_gated = body_pick_gated;
    methods.link_idle = body_link_idle;
    methods.pick_actor_b = body_pick_actor_b;
    methods.pick_direct = body_pick_direct;
    methods.pick_direct_b = body_pick_direct_b;
    methods.pick_gated_b = body_pick_gated_b;
    methods.pick_gated_c = body_pick_gated_c;
    methods.pick_gated_d = body_pick_gated_d;
    methods.pick_direct_c = body_pick_direct_c;
    methods.pick_range = body_pick_range;
    methods.fixed_id = body_fixed_id;
    methods.pick_direct_d = body_pick_direct_d;
    methods.pick_direct_e = body_pick_direct_e;
    methods.pick_direct_f = body_pick_direct_f;
    methods.pick_near = body_pick_near;
}

void register_entity_types()
{
    if (!g_types.empty())
    {
        return;
    }
    g_types.reserve(64);

    // Xbox 0x00073190 entityBodyCamera_ctor: fill defaults, then overwrite.
    // +0x00..+0x0C become the camera size, tick, shutdown, and message.
    // +0x10 and +0x14 stay 0x00025EC0 and 0x00072AC0.
    // Retail then stores 0x00072B10 at +0x18 and 0x00072CB0 at +0x1C. Those
    // two replacements are not ported, so the default apply and gate remain.
    EntityType camera;
    camera.name = "entityBodyCamera";
    camera.kind = EntityKind::Body;
    camera.xbox_desc_va = 0x00504F80;
    fn_1da60(camera.methods);
    camera.methods.query_size = entity_body_camera_sizeof;
    camera.methods.tick = entity_body_camera_tick;
    camera.methods.shutdown = entity_body_camera_shutdown;
    camera.methods.handle_message = entity_body_camera_message;
    camera.tick = camera.methods.tick;
    camera.shutdown = camera.methods.shutdown;
    camera.handle_message = camera.methods.handle_message;
    u32 sizes[2] = {};
    camera.methods.query_size(sizes);
    camera.xbox_size = sizes[0];
    camera.xbox_extra = sizes[1];
    g_types.push_back(camera);

    const char* bodies[] = {"entityBodyPlayer", "entityBodyHuman", "entityBodyZombie", "entityBodyVampire",
                            "entityBodyWorm", "entityBodyJessieClyde"};
    for (const char* n : bodies)
    {
        add_named(n, EntityKind::Body);
    }
    const char* minds[] = {"entityMindPlayer", "entityMindHuman", "entityMindCinema"};
    for (const char* n : minds)
    {
        add_named(n, EntityKind::Mind);
    }
    const char* strategies[] = {
        "entityStrategyPlayer",        "entityStrategyDirector",     "entityStrategyGeneric",
        "entityStrategyFNPC",          "entityStrategyBaron",        "entityStrategyBaronPlane",
        "entityStrategyChicken",       "entityStrategyGhoulieGirl",  "entityStrategyGiantHand",
        "entityStrategyGrimReaper",    "entityStrategyHauntedChair", "entityStrategyHauntedCoat",
        "entityStrategyHauntedDoor",   "entityStrategyHauntedPicture","entityStrategyHauntedTV",
        "entityStrategyHunchback",     "entityStrategyImp",          "entityStrategyImpFire",
        "entityStrategyImpFlying",     "entityStrategyImpNinja",     "entityStrategyJessieClyde",
        "entityStrategyMedusa",        "entityStrategyMummy",        "entityStrategyMummyCursed",
        "entityStrategyPirate",        "entityStrategyRibs",         "entityStrategySkeleton",
        "entityStrategySpider",        "entityStrategyUglyGirl",     "entityStrategyVampire",
        "entityStrategyVampireChicken","entityStrategyWarlock",      "entityStrategyWorm",
        "entityStrategyZombie"};
    for (const char* n : strategies)
    {
        add_named(n, EntityKind::Strategy);
    }
}

const EntityType* find_entity_type(std::string_view name)
{
    for (const auto& t : g_types)
    {
        if (t.name == name)
        {
            return &t;
        }
    }
    return nullptr;
}

std::vector<const EntityType*> all_entity_types()
{
    std::vector<const EntityType*> out;
    out.reserve(g_types.size());
    for (const auto& t : g_types)
    {
        out.push_back(&t);
    }
    return out;
}

static_assert(offsetof(EntityShapeNode, bone) == 8);
static_assert(offsetof(EntityShapeNode, along_x) == 0x10);
static_assert(offsetof(EntityShapeNode, along_y) == 0x14);
static_assert(offsetof(EntityShapeNode, point) == 0x18);
static_assert(offsetof(EntityCylinderBound, x) == 4);
static_assert(offsetof(EntityCylinderBound, height) == 0x14);
static_assert(offsetof(EntityCylinderBound, radius) == 0x1C);
static_assert(sizeof(EntityCylinderBound) == 0x20);

namespace
{

Vec3 mul_row(const float m[12], const Vec3& p)
{
    return Vec3{
        m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3],
        m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7],
        m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11],
    };
}

float length3(const Vec3& d)
{
    return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

// 0x000315F0. A packed table returns the 3x4 at +0x10 + index * 0x30.
// A clear +4 transposes the 4x4 at +0x10 + index * 0x40 into that layout.
bool bone_matrix(const EntityBonePose& pose, i16 bone, float out[12])
{
    const i32 index = bone;
    if (index < 0)
    {
        return false;
    }
    if (pose.packed != 0)
    {
        const size_t base = static_cast<size_t>(index) * 12u;
        if (base + 12u > pose.mats.size())
        {
            return false;
        }
        std::memcpy(out, pose.mats.data() + base, sizeof(float) * 12u);
        return true;
    }
    const size_t base = static_cast<size_t>(index) * 16u;
    if (base + 16u > pose.mats.size())
    {
        return false;
    }
    const float* s = pose.mats.data() + base;
    out[0] = s[0];
    out[1] = s[4];
    out[2] = s[8];
    out[3] = s[12];
    out[4] = s[1];
    out[5] = s[5];
    out[6] = s[9];
    out[7] = s[13];
    out[8] = s[2];
    out[9] = s[6];
    out[10] = s[10];
    out[11] = s[14];
    return true;
}

Vec3 xform_point(const EntityShapeNode& node, const float* world, const EntityBonePose* pose, Vec3 local)
{
    // 0x00056790. Bone 0xFFFF copies the local point. Otherwise the pose
    // matrix is applied first. A null world matrix skips the actor multiply.
    Vec3 p = local;
    if (node.bone != 0xFFFF && pose != nullptr)
    {
        float bone_m[12];
        if (bone_matrix(*pose, static_cast<i16>(node.bone), bone_m))
        {
            p = mul_row(bone_m, p);
        }
    }
    if (world != nullptr)
    {
        p = mul_row(world, p);
    }
    return p;
}

// 0x00056330. World position of the three floats at node+0x14. The radius is
// the world length of that point after the float at +0x10 is added on local x.
// Both samples use the node's bone, the same pose, and the same actor matrix.
float sphere_metrics(const EntityShapeNode& node, const float* world, const EntityBonePose* pose,
                    Vec3& center)
{
    Vec3 local;
    std::memcpy(&local, reinterpret_cast<const char*>(&node) + 0x14, sizeof(local));
    center = xform_point(node, world, pose, local);
    local.x += node.along_x;
    const Vec3 arm = xform_point(node, world, pose, local);
    return length3(Vec3{center.x - arm.x, center.y - arm.y, center.z - arm.z});
}

} // namespace

void fn_55aa0(const EntityShapeNode& node, const float* world, const EntityBonePose* pose,
              EntityCylinderBound& out)
{
    // Xbox 0x00055AA0
    // ESI is the 0x20-byte record. 0x00055B30 transforms the point at node+0x18,
    // that point plus the float at +0x10 on local x, and that point plus the
    // float at +0x14 on local y. The radius is the world length of the x
    // offset. The y offset's length is stored positive and negated; the record
    // y is the world point plus the negated length, and the height is the
    // positive length minus that negated length. Bytes are 4, 0x20, 1. The
    // floats at +0x10 and +0x18 are cleared. 0x00103BE0 transforms node+0x24
    // into a scratch this record does not read.
    const Vec3 base = xform_point(node, world, pose, node.point);
    Vec3 along_x = node.point;
    along_x.x += node.along_x;
    Vec3 along_y = node.point;
    along_y.y += node.along_y;
    const Vec3 world_x = xform_point(node, world, pose, along_x);
    const Vec3 world_y = xform_point(node, world, pose, along_y);
    const float reach = length3(Vec3{base.x - world_x.x, base.y - world_x.y, base.z - world_x.z});
    const float hy = length3(Vec3{base.x - world_y.x, base.y - world_y.y, base.z - world_y.z});
    const float down = -hy;
    out.kind = 4;
    out.size = 0x20;
    out.flag = 1;
    out.x = base.x;
    out.y = base.y + down;
    out.z = base.z;
    out.pad0 = 0.f;
    out.height = hy - down;
    out.pad1 = 0.f;
    out.radius = reach;
}

void fn_56720(const EntityShapeNode& node, const float* world, const EntityBonePose* pose,
              EntityCylinderBound& out)
{
    // Xbox 0x00056720
    // EDX is the shape node, ECX the pose, the stack argument the actor matrix,
    // and ESI the 0x20-byte record. 0x00056330 writes the sphere center and
    // returns its radius. The record stores that sphere as a cylinder: y is
    // the center minus the radius, the height is twice the radius, and the
    // radius is stored unchanged. Bytes are 4, 0x20, 1. The floats at +0x10
    // and +0x18 are cleared.
    Vec3 center;
    const float radius = sphere_metrics(node, world, pose, center);
    out.kind = 4;
    out.size = 0x20;
    out.flag = 1;
    out.x = center.x;
    out.y = center.y - radius;
    out.z = center.z;
    out.pad0 = 0.f;
    out.height = radius + radius;
    out.pad1 = 0.f;
    out.radius = radius;
}

namespace
{

// Xbox 0x001202C0. ECX is the cylinder array, EAX the count, EBX the source
// point, EDI the merged record. XZ stays on the source. Y runs from the lower
// of the source and every cylinder bottom to the higher of the source and every
// top. The radius is the largest horizontal reach (distance to a center plus
// that cylinder's radius). An empty array is a zero-height, zero-radius record.
EntityCylinderBound merge_cylinders(const Vec3& src, const EntityCylinderBound* shapes, i32 count)
{
    float min_y = src.y;
    float max_top = src.y;
    float reach = 0.f;
    if (shapes != nullptr && count > 0)
    {
        for (i32 i = 0; i < count; ++i)
        {
            const EntityCylinderBound& cyl = shapes[i];
            const float dx = cyl.x - src.x;
            const float dz = cyl.z - src.z;
            const float dist = std::sqrt(dx * dx + dz * dz) + cyl.radius;
            if (dist > reach)
            {
                reach = dist;
            }
            if (cyl.y <= min_y)
            {
                min_y = cyl.y;
            }
            const float top = cyl.y + cyl.height;
            if (max_top < top)
            {
                max_top = top;
            }
        }
    }
    EntityCylinderBound out{};
    out.kind = 4;
    out.size = 0x20;
    out.flag = 1;
    out.x = src.x;
    out.y = min_y;
    out.z = src.z;
    out.pad0 = 0.f;
    out.height = max_top - min_y;
    out.pad1 = 0.f;
    out.radius = reach;
    return out;
}

// Xbox 0x00120D10. ECX is the push, EAX the count, EDI the cylinder array, and
// the stack argument is the merged record. Each cylinder's position, and the
// merged record's position, gain the push. Radius and height stay put.
void shift_cylinders(EntityCylinderBound* shapes, i32 count, EntityCylinderBound& merged, const Vec3& push)
{
    if (shapes != nullptr && count > 0)
    {
        for (i32 i = 0; i < count; ++i)
        {
            shapes[i].x += push.x;
            shapes[i].y += push.y;
            shapes[i].z += push.z;
        }
    }
    merged.x += push.x;
    merged.y += push.y;
    merged.z += push.z;
}

} // namespace

i32 fn_120480(const Vec3& src, const Actor* actor, EntityCylinderBound* shapes, i32 count, Vec3& push_out)
{
    // Xbox 0x00120480
    // EAX is the source. Stack args are the actor, the cylinder array, the count,
    // and the summed push. DAT_00511188 is cleared, then 0x001202C0 merges the
    // array into one cylinder around the source. A null actor leaves the query
    // null; otherwise the query is source minus actor_world_position, and that
    // origin is still in the push slot when vtable +0x34 is entered. Each node
    // on DAT_00511B04 (object+0x110, next at +4) is called with the actor, the
    // query, the merged cylinder, the array, the count, and the push slot.
    // A hit stores the object at DAT_0051118C[index], bumps the index, adds the
    // push through 0x00120D10, and accumulates it. The return is 1 after any hit.
    g_contact_slot = 0;
    EntityCylinderBound merged = merge_cylinders(src, shapes, count);

    Vec3 origin{};
    Vec3 query_storage{};
    const Vec3* query = nullptr;
    if (actor != nullptr)
    {
        origin = actor_world_position(*actor);
        query_storage = Vec3{src.x - origin.x, src.y - origin.y, src.z - origin.z};
        query = &query_storage;
    }

    push_out.x = 0.f;
    push_out.y = 0.f;
    push_out.z = 0.f;
    i32 hit = 0;
    for (ShapeProbe* probe = g_world.shape; probe != nullptr; probe = probe->next)
    {
        if (probe->push == nullptr)
        {
            continue;
        }
        Vec3 delta = origin;
        const i32 ok = probe->push(*probe, actor, query, merged, shapes, count, delta);
        if (ok == 0)
        {
            continue;
        }
        if (g_contact_slot >= 0 && g_contact_slot < 64)
        {
            g_contact_owners[g_contact_slot] = probe;
        }
        g_contact_slot += 1;
        shift_cylinders(shapes, count, merged, delta);
        push_out.x += delta.x;
        push_out.y += delta.y;
        push_out.z += delta.z;
        hit = 1;
    }
    return hit;
}

void fn_1203e0(const Vec3& src, Vec3& dest, const Actor* actor, float height, float radius)
{
    // Xbox 0x001203E0
    // ESI is the source point and EDI is the destination. EDX is the actor
    // passed to 0x00120480. XMM1 is the cylinder height; the stack float is
    // the radius. The record is kind 4, size 0x20, flag 1, centered on ESI,
    // with the floats at +0x10 and +0x18 cleared. 0x00120480 sums one push per
    // hit; the destination is the source plus that sum.
    EntityCylinderBound bound{};
    bound.kind = 4;
    bound.size = 0x20;
    bound.flag = 1;
    bound.x = src.x;
    bound.y = src.y;
    bound.z = src.z;
    bound.pad0 = 0.f;
    bound.height = height;
    bound.pad1 = 0.f;
    bound.radius = radius;

    Vec3 push{};
    fn_120480(src, actor, &bound, 1, push);
    dest.x = src.x + push.x;
    dest.y = src.y + push.y;
    dest.z = src.z + push.z;
}

i32 fn_120760(const EntityCylinderBound& query, const EntityCylinderBound& obstacle, const Vec3& offset,
              Vec3& push)
{
    // Xbox 0x00120760
    // EBX is the query cylinder, EDI the obstacle, and the two stack arguments are
    // the offset and the push. The query point is the cylinder position plus the
    // offset. A miss on the vertical slabs, or an XZ distance at or beyond the
    // summed radius, returns 0. 0x00152B30 is the horizontal length. Under 1e-5
    // the X separation becomes 1 and the length becomes the summed radius minus
    // 0.01. 0x00120250 then scales that (dx, 0, dz) vector out to the summed
    // radius when its length is not zero. The push is the scaled point minus the
    // query point, with Y cleared. The contact midpoint is stored at
    // DAT_005111AC[DAT_00511188] and the index is left alone.
    const float point_x = offset.x + query.x;
    const float point_y = query.y + offset.y;
    const float point_z = query.z + offset.z;
    if (point_y > obstacle.y + obstacle.height || obstacle.y > point_y + query.height)
    {
        return 0;
    }

    const float combined = obstacle.radius + query.radius;
    float dx = point_x - obstacle.x;
    float dz = point_z - obstacle.z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist < kHitEps)
    {
        dx = 1.f;
        dist = combined - kHitPad;
    }
    if (combined <= dist)
    {
        return 0;
    }

    const float len2 = dx * dx + dz * dz;
    if (len2 != 0.f)
    {
        const float scale = combined / std::sqrt(len2);
        dx *= scale;
        dz *= scale;
    }
    push.x = (obstacle.x + dx) - point_x;
    push.y = 0.f;
    push.z = (obstacle.z + dz) - point_z;

    if (g_contact_slot >= 0 && g_contact_slot < 64)
    {
        const float mid_y = obstacle.height * 0.5f + obstacle.y;
        const float sample_y = offset.y + query.height * 0.5f + query.y;
        g_contact_points[g_contact_slot].x = (point_x - obstacle.x) * 0.5f + obstacle.x;
        g_contact_points[g_contact_slot].y = (sample_y - mid_y) * 0.5f + mid_y;
        g_contact_points[g_contact_slot].z = (point_z - obstacle.z) * 0.5f + obstacle.z;
    }
    return 1;
}

namespace
{

// DAT_00511A18. 0x001226C0 clears this, then 0x00122C10 indexes the pair array
// with it. A hit stores the object at DAT_00511A1C + index*8 and increments it.
// The pool pointer sits in the next dword (DAT_00511A20 for index 0).
// Pairs run from DAT_00511A1C up to the list head at DAT_00511AF4.
constexpr i32 kSegmentSlots = (0x511AF4 - 0x511A1C) / 8;
constexpr u16 kSegmentStride = 0x54;
constexpr u16 kSegmentGrow = 10;
constexpr i32 kSegmentPoolBytes = 0x358; // 0x10 header + 0x54 * 10

// Link walked from DAT_00511AF4. Retail stores the link at object+0x1e0 and the
// next pointer at +0x1e4. The query is vtable +0x20. The port keeps them together.
struct SegmentProbe
{
    i32 (*query)(SegmentProbe& self, Vec3& candidate, Vec3& kept, u32 param, i32 kind, u32 mask,
                 u8* record) = nullptr;
    SegmentProbe* next = nullptr;
};

// Header of the 0x54-byte record pool. begin is the first record (block+0x10).
// 0x00072CB0 copies that record with 0x000FCD70 after a hit.
struct SegmentPool
{
    u16 stride = 0;
    u16 grow = 0;
    u8* begin = nullptr;
    u8* cursor = nullptr;
    u8* limit = nullptr;
    u8* block = nullptr;
    i32 block_size = 0;
    bool live = false;
};

struct SegmentSlot
{
    SegmentProbe* object = nullptr;
    SegmentPool pool{};
};

i32 g_segment_count = 0;
SegmentSlot g_segment_slots[kSegmentSlots]{};

void bind_segment_pool(SegmentPool& pool, u8* block, i32 block_size, i32 used)
{
    pool.block = block;
    pool.block_size = block_size;
    pool.stride = kSegmentStride;
    pool.grow = kSegmentGrow;
    std::memcpy(block, &pool.stride, sizeof(pool.stride));
    std::memcpy(block + 2, &pool.grow, sizeof(pool.grow));
    pool.begin = block + 0x10;
    const i32 stride = static_cast<i32>(pool.stride);
    pool.cursor = pool.begin + static_cast<ptrdiff_t>(stride) * used;
    const i32 capacity = (block_size - 0x10) / stride;
    pool.limit = pool.begin + static_cast<ptrdiff_t>(stride) * capacity;
}

// Xbox 0x000662A0. EAX is the pool. used = (cursor - begin) / stride.
// The new count is used plus the u16 at +2. 0x000F73A0 reallocates, begin
// becomes the new header+0x10, and the cursor stays on the same record.
void grow_segment_pool(SegmentPool& pool)
{
    const i32 stride = pool.stride == 0 ? 1 : static_cast<i32>(pool.stride);
    i32 used = 0;
    if (pool.begin != nullptr && pool.cursor != nullptr && pool.cursor >= pool.begin)
    {
        used = static_cast<i32>(pool.cursor - pool.begin) / stride;
    }
    const i32 new_count = used + static_cast<i32>(pool.grow);
    const i32 new_bytes = stride * new_count + 0x10;
    u8* fresh = static_cast<u8*>(fn_f72d0(new_bytes));
    if (fresh == nullptr)
    {
        return;
    }
    if (pool.block != nullptr && pool.block_size > 0)
    {
        const i32 copy = pool.block_size < new_bytes ? pool.block_size : new_bytes;
        std::memcpy(fresh, pool.block, static_cast<size_t>(copy));
        fn_663e0(pool.block);
    }
    bind_segment_pool(pool, fresh, new_bytes, used);
}

// Xbox 0x00122C10. The index is DAT_00511A18. A missing pool is one 0x358-byte
// block: stride 0x54 and ten records. An existing pool rewinds its cursor.
SegmentPool* prepare_segment_pool()
{
    const i32 index = g_segment_count;
    if (index < 0 || index >= kSegmentSlots)
    {
        return nullptr;
    }
    SegmentPool& pool = g_segment_slots[index].pool;
    if (!pool.live)
    {
        u8* block = static_cast<u8*>(fn_f72d0(kSegmentPoolBytes));
        if (block == nullptr)
        {
            return nullptr;
        }
        bind_segment_pool(pool, block, kSegmentPoolBytes, 0);
        pool.live = true;
    }
    else
    {
        pool.cursor = pool.begin;
    }
    return &pool;
}

u8* bump_segment_record(SegmentPool& pool)
{
    if (pool.cursor == pool.limit)
    {
        grow_segment_pool(pool);
    }
    if (pool.cursor == nullptr || pool.limit == nullptr || pool.cursor >= pool.limit)
    {
        return nullptr;
    }
    u8* record = pool.cursor;
    pool.cursor = record + pool.stride;
    record[0x26] = 0;
    return record;
}

} // namespace

i32 fn_1226c0(Vec3& kept, Vec3& candidate, u32 param, i32 kind, u32 mask)
{
    // Xbox 0x001226C0
    // EBX is the point that a move under 2e-7 (DAT_0032B27C) overwrites with the
    // candidate. The four stack dwords are the candidate, an opaque param, a kind
    // (4 or 0xC), and a mask. DAT_00511A18 is cleared first. A longer move asks
    // 0x00122C10 for the record pool, bumps one 0x54-byte record, and clears the
    // byte at +0x26. Each node on DAT_00511AF4 (object+0x1e0, next at +4) is
    // called through vtable +0x20 with the candidate, EBX, param, kind, mask, and
    // that record. The last node that returns non-zero is stored at
    // DAT_00511A1C[count] and the count is incremented. The return is 1 when any
    // node hit. The first record stays at pool begin for 0x000FCD70.
    g_segment_count = 0;
    const float dx = candidate.x - kept.x;
    const float dy = candidate.y - kept.y;
    const float dz = candidate.z - kept.z;
    const float dist2 = dz * dz + dy * dy + dx * dx;
    // comiss / jb: the long path runs when the epsilon is below dist2, including NaN.
    if (!(dist2 <= kLookEps))
    {
        SegmentPool* pool = prepare_segment_pool();
        if (pool == nullptr)
        {
            return 0;
        }
        u8* record = bump_segment_record(*pool);
        if (record == nullptr)
        {
            return 0;
        }

        SegmentProbe* found = nullptr;
        i32 hit = 0;
        for (SegmentProbe* probe = g_world.segment; probe != nullptr; probe = probe->next)
        {
            if (probe->query == nullptr)
            {
                continue;
            }
            const i32 ok = probe->query(*probe, candidate, kept, param, kind, mask, record);
            if (ok != 0)
            {
                found = probe;
                hit = 1;
            }
        }
        if (found != nullptr)
        {
            const i32 slot = g_segment_count;
            if (slot >= 0 && slot < kSegmentSlots)
            {
                g_segment_slots[slot].object = found;
            }
            g_segment_count = slot + 1;
        }
        return hit;
    }

    kept.x = candidate.x;
    kept.y = candidate.y;
    kept.z = candidate.z;
    return 0;
}

namespace
{

// DAT_0050e240 holds four floor grids. The count is DAT_0050e250. The Y bands
// are two floats at DAT_0050e254 and two at DAT_0050e25c. Origins are the
// dwords at DAT_0050e264 and DAT_0050e26c.
constexpr i32 kNavSlots = 4;
constexpr i32 kNavBands = 2;
constexpr float kNavBandPad = 2.f; // DAT_0032B110

struct NavCell
{
    i16 bottom = 0; // record +2
    i16 top = 0;    // record +0xe
};

struct NavGrid
{
    i32 width = 0; // +0
    i32 depth = 0; // +4
    i32 cell = 0;  // +8, divisor for world XZ
    std::vector<NavCell> cells; // +0x10, stride 0x12, index width * z + x
};

NavGrid* g_nav_grids[kNavSlots]{};
i32 g_nav_count = 0;
float g_nav_min_y[kNavBands]{};
float g_nav_max_y[kNavBands]{};
i32 g_nav_origin_x = 0;
i32 g_nav_origin_z = 0;

// DAT_00314EB4, stride 8. The low float is the radius bump. Index is
// class + state * 3, and only state 1..4 with class 1..3 are read.
constexpr float kRadiusBump[16] = {
    0.f, 0.f, 0.f, 0.f, 5.f, 5.f, 5.f, 0.f, 5.f, 0.f, 0.f, 5.f, 0.f, 0.f, 0.f, 0.f,
};

constexpr float kCellLift = 5.f;    // DAT_0032B184
constexpr float kCellHeight = 10.f; // DAT_0032B10C
constexpr float kCellRadius = 4.f;  // DAT_00434018

// 0x00110BF0 called as (-1, 0). The -1 never matches a floor, so the cell
// flag test is skipped. A Y inside [min, max + 2] returns that floor. Otherwise
// the last floor whose min is strictly below the point wins, or 0 when none do.
i32 nav_floor_index(const Vec3& point)
{
    i32 best = 0;
    const i32 count = g_nav_count < kNavBands ? g_nav_count : kNavBands;
    for (i32 i = 0; i < count; ++i)
    {
        if (point.y >= g_nav_min_y[i] && (g_nav_max_y[i] + kNavBandPad) >= point.y)
        {
            return i;
        }
        if (point.y > g_nav_min_y[i])
        {
            best = i;
        }
    }
    return best;
}

// 0x00110EF0. EAX is the floor, EBX the point. A null grid or an XZ cell
// outside the grid returns -1. Truncation matches cvttss2si.
i32 nav_cell_index(i32 floor, const Vec3& point)
{
    if (floor < 0 || floor >= kNavSlots)
    {
        return -1;
    }
    const NavGrid* grid = g_nav_grids[floor];
    if (grid == nullptr || grid->cell == 0)
    {
        return -1;
    }
    const i32 cx = (trunc_i32(point.x) - g_nav_origin_x) / grid->cell;
    const i32 cz = (trunc_i32(point.z) - g_nav_origin_z) / grid->cell;
    if (cx < 0 || cx >= grid->width || cz < 0 || cz >= grid->depth)
    {
        return -1;
    }
    return grid->width * cz + cx;
}

bool nav_cell(i32 floor, i32 index, NavCell& out)
{
    if (floor < 0 || floor >= kNavSlots)
    {
        return false;
    }
    const NavGrid* grid = g_nav_grids[floor];
    if (grid == nullptr || index < 0 || static_cast<size_t>(index) >= grid->cells.size())
    {
        return false;
    }
    out = grid->cells[static_cast<size_t>(index)];
    return true;
}

// 0x00145FF0. EDI is the case link, BX is the actor word at +6.
i32 class_from_link(const EntityCaseLink& link, u16 key)
{
    if (link.classes.empty())
    {
        return 0;
    }
    for (const EntityClassSlot& slot : link.classes)
    {
        if (slot.key == key)
        {
            return slot.id;
        }
    }
    return link.classes[0].id;
}

} // namespace

void fn_13d2c0(const Vec3& point, const Actor* actor, EntityCylinderBound& out)
{
    // Xbox 0x0013D2C0
    // EAX is the point. The actor is the first stack argument; null leaves the
    // radius at 4. The second argument is the 0x20-byte cylinder. 0x00110BF0
    // picks the floor and 0x00110EF0 the cell, twice. The short at cell+2 is
    // the bottom. The short at cell+0xe minus that bottom is the height when
    // the second lookup hits and the difference is >= 0. A miss, or a negative
    // difference, uses point Y + 5 and height 10. The floats at +0x10 and +0x18
    // stay 0. Radius is DAT_00434018 plus the DAT_00314EB4 bump for case state
    // 1..4 and class 1..3, plus another 5 when bit 0 of bits byte +5 is set.
    const i32 floor = nav_floor_index(point);
    float bottom = point.y + kCellLift;
    float height = kCellHeight;
    const i32 cell = nav_cell_index(floor, point);
    NavCell sample{};
    if (cell >= 0 && nav_cell(floor, cell, sample))
    {
        const float y0 = static_cast<float>(sample.bottom);
        float span = point.y - y0;
        const i32 cell_b = nav_cell_index(floor, point);
        NavCell sample_b{};
        if (cell_b >= 0 && nav_cell(floor, cell_b, sample_b))
        {
            span = static_cast<float>(sample_b.top) - y0;
        }
        if (span >= 0.f)
        {
            bottom = y0;
            height = span;
        }
    }

    float bump = 0.f;
    if (actor != nullptr && actor->euler_out != nullptr)
    {
        const ActorEulerOut& host = *actor->euler_out;
        u8 state = 0;
        i32 cls = 0;
        if (host.case_link != nullptr)
        {
            state = host.case_link->state;
            if (host.actor != nullptr)
            {
                cls = class_from_link(*host.case_link, host.actor->class_key);
            }
        }
        if (state > 0 && state < 5 && cls > 0 && cls < 4)
        {
            bump = kRadiusBump[cls + static_cast<i32>(state) * 3];
        }
        if (host.bits != nullptr && host.bits->bytes != nullptr && (host.bits->bytes[5] & 1u) != 0)
        {
            bump += kCellLift;
        }
    }

    out.kind = 4;
    out.size = 0x20;
    out.flag = 1;
    out.x = point.x;
    out.y = bottom;
    out.z = point.z;
    out.pad0 = 0.f;
    out.height = height;
    out.pad1 = 0.f;
    out.radius = kCellRadius + bump;
}

namespace
{

// Names at DAT_004245A8. 0x00056DF0 widens one entry per slot into DAT_004e5f20.
constexpr i32 kTextTokenCount = 0x5C;
constexpr i32 kTextTokenAlloc = 0x86;
constexpr i32 kTextIconMin = 0x36;

const char* const kTextTokenNames[kTextTokenCount] = {
    "boy",           "girl",          "butler",        "cook",
    "groundskeeper", "housekeeper",   "krackpot",      "baroness",
    "baron",         "crone",         "piratecaptain", "skeletonbad",
    "mrribs",        "pirate",        "littleguy",     "zombiestandard",
    "hunchback",     "binklogo",      "SpeakerMAX",    "left",
    "right",         "centre",        "scalex",        "scaley",
    "seed",          "page",          "para",          "nopara",
    "linek",         "linel",         "linec",         "liner",
    "end",           "nohead",        "nobutton",      "settime",
    "scale",         "tab",           "pausestats",    "bronzereq",
    "silverreq",     "goldreq",       "platinumreq",   "bronzetime",
    "silvertime",    "goldtime",      "platinumtime",  "finalscore",
    "finaltime",     "hightime",      "highscore",     "hightrophy",
    "finaltrophy",   "invokemap",     "IconMIN",       "EventMIN",
    "cross",         "dontgethit",    "findkeyghouly", "findkey",
    "imp",           "killall",       "killany",       "minimumdamage",
    "mummy",         "nodamage",      "onetype",       "skeleton",
    "survive",       "timelimit",     "unarmedonly",   "weapononly",
    "limitedattacks", "EventMAX",     "PauseMIN",      "clock",
    "faints",        "prisoners",     "skull",         "bonusbooks",
    "PauseMAX",      "ChallengeMIN",  "bronzetrophy",  "silvertrophy",
    "goldtrophy",    "platinumtrophy", "ChallengeMAX", "WeaponMIN",
    "interacthand",  "WeaponMAX",     "IconMAX",       "sfx",
};

// DAT_004e5f20. Wide copies, stride 4, count 0x5C.
u16* g_text_tokens[kTextTokenCount]{};

i32 g_text_ticks = 0;      // DAT_004e61c8
i32 g_text_read_page = 0;  // DAT_004ee1e0. Indexes the flag byte at page+0x1000.
i32 g_text_write_page = 0; // DAT_004ee1e4. Indexes the 0x1002-byte pages at DAT_004e61d0.
i32 g_text_page_count = 0; // DAT_004ee1e8. Writers reject a count above 8.
i32 g_text_draw_mode = 0;  // DAT_004ee1ec. 1 and 3 select the sprite scale.
i32 g_markup_gate = 0;     // DAT_004ee290
i32 g_sprite_gate = 0;     // DAT_004ee294. Nonzero runs the sprite tick.
float g_text_scale = 0.f;  // DAT_004ee298. Pool float at DAT_0032B254.
i32 g_text_pad_29c = 0;    // DAT_004ee29c
i32 g_text_page_idle = 0;  // DAT_004ee2a0
void* g_text_link = nullptr; // DAT_004ee2a8
i32 g_text_mark_b0 = 0;    // DAT_004ee2b0
i32 g_text_mark_b4 = 0;    // DAT_004ee2b4

void* g_text_sprite = nullptr; // DAT_004e60a4
i32 g_text_sprite_id = 0;      // DAT_004e60a0
i32 g_text_sprite_hold = 0;    // DAT_004e60a8
i32 g_text_sprite_pair = 0;    // DAT_004e60ac
i32 g_text_sprite_phase = 0;   // DAT_004e60b8

void* g_text_icon = nullptr; // DAT_004e60c4. Glyph record; +8 and +0xA are ushorts.
i32 g_text_icon_id = 0;      // DAT_004e60c0. IconMIN until a later token replaces it.

// 0x00056DF0. Current heap slot (DAT_00507808) supplies every block. Each name
// is sign-extended into u16s and terminated. The pointer is stored even when
// the following copy would fault, so a null block is stored and left alone.
void text_build_tokens()
{
    for (i32 i = 0; i < kTextTokenCount; ++i)
    {
        u16* block = static_cast<u16*>(fn_f72d0(kTextTokenAlloc - 4));
        g_text_tokens[i] = block;
        if (block == nullptr)
        {
            continue;
        }
        const char* name = kTextTokenNames[i];
        i32 length = 0;
        while (name[length] != '\0')
        {
            length += 1;
        }
        for (i32 c = 0; c < length; ++c)
        {
            block[c] = static_cast<u16>(static_cast<i32>(static_cast<signed char>(name[c])));
        }
        block[length] = 0;
    }
}

// 0x00057400. A live sprite would be released by 0x00035C00 (EBX is the object);
// the resource list is not in this file, so the slot is cleared. The scene load
// at DAT_00506c74 and the fallback load of DAT_0050ea9c are discarded. The three
// ids become -1 and the phase becomes 0.
void text_reset_sprite()
{
    g_text_sprite_hold = -1;
    if (g_text_sprite != nullptr)
    {
        g_text_sprite = nullptr;
    }
    g_text_sprite_id = -1;
    g_text_sprite_pair = -1;
    g_text_sprite_phase = 0;
}

} // namespace

void fn_57d40()
{
    // Xbox 0x00057D40
    // ESI is zero and EAX is 1. The scale is the float at DAT_0032B254 (0.1).
    // 0x00056DF0 fills the token table, then 0x00057400 resets the sprite.
    // The icon pointer is cleared after those calls, and the icon id is IconMIN.
    g_text_ticks = 0;
    g_text_read_page = 0;
    g_text_write_page = 0;
    g_text_page_count = 0;
    g_text_draw_mode = 0;
    g_markup_gate = 1;
    g_sprite_gate = 1;
    g_text_scale = std::bit_cast<float>(0x3DCCCCCDu);
    g_text_pad_29c = 0;
    g_text_page_idle = 0;
    g_text_link = nullptr;
    g_text_mark_b0 = 0;
    g_text_mark_b4 = 0;
    text_build_tokens();
    text_reset_sprite();
    g_text_icon = nullptr;
    g_text_icon_id = kTextIconMin;
}

namespace
{

// DAT_00509afc. 0x000FD5C0 enables the pass when active is set and gate is clear.
// colour bytes are B, G, R, A (0x000FD5E3 reads them in that order).
struct LightVolumePass
{
    i32 active = 0;    // +0
    i32 gate = 0;      // +4
    u32 color = 0;     // +8 DAT_00509b04
    float scale = 0.f; // +0xc DAT_00509b08
    float range = 0.f; // +0x10 DAT_00509b0c
};

static_assert(offsetof(LightVolumePass, color) == 8);
static_assert(offsetof(LightVolumePass, range) == 0x10);

// DAT_00509c20, stride 0x25c. 0x000FE1D0 clears +0 and +0x1e8 and writes the
// group word at +0x1f0. Twenty-five records end at DAT_0050d71c.
struct LightVolumeRecord
{
    u32 head = 0; // +0
    u8 gap_4[0x1e8 - 4]{};
    u32 tail = 0; // +0x1e8
    u8 gap_1ec[4]{};
    u16 kind = 0; // +0x1f0
    u8 gap_1f2[0x25c - 0x1f2]{};
};

static_assert(sizeof(LightVolumeRecord) == 0x25c);
static_assert(offsetof(LightVolumeRecord, tail) == 0x1e8);
static_assert(offsetof(LightVolumeRecord, kind) == 0x1f0);

// DAT_0030ca80, three groups, stride 8. count is the dword; kind is the word.
struct LightVolumeGroup
{
    u32 count;
    u16 kind;
};

constexpr LightVolumeGroup kLightVolumeGroups[] = {
    {8, 0x100},
    {8, 0x80},
    {9, 0x40},
};

constexpr i32 kLightVolumeSlots = 0x28;
constexpr i32 kLightVolumeRecords = 8 + 8 + 9;
static_assert(kLightVolumeRecords == 0x19);
static_assert(std::bit_cast<u32>(1.f) == 0x3f800000u);
static_assert(std::bit_cast<u32>(1000.f) == 0x447a0000u);

// 0x0015AB90. Both arguments are words. ((height + 1) * width * 3) << 4,
// then rounded up to 32. A 16x16 grid is 0x3300 bytes.
constexpr u32 light_grid_bytes(u32 width, u32 height)
{
    const u32 cells = ((height & 0xffffu) + 1u) * (width & 0xffffu);
    const u32 bytes = (cells * 3u) << 4;
    return (bytes + 0x1fu) & ~0x1fu;
}

static_assert(light_grid_bytes(0x10, 0x10) == 0x3300u);

LightVolumePass g_light_pass{};
float g_light_tint[4]{};          // DAT_00509b20
float g_light_intensity = 0.f;    // DAT_00509b4c
float g_light_angle_inner = 0.f;  // DAT_00509b50. Pool float DAT_0032b160.
float g_light_angle_outer = 0.f;  // DAT_00509b54. Pool float DAT_0032b15c.
i32 g_light_mark = 0;             // DAT_00509b58
i32 g_light_paused = 0;           // DAT_00509b70
i32 g_light_running = 0;          // DAT_00509b74
i32 g_light_live = 0;             // DAT_00509b78
void* g_light_slots[kLightVolumeSlots]{}; // DAT_00509b7c

void* g_light_grid = nullptr; // DAT_004d9658
u32 g_light_grid_bytes = 0;   // DAT_004d965c
u8 g_light_grid_flag = 0;     // DAT_004d9660

using LightAllocFn = void* (*)(i32);
using LightFreeFn = void (*)(void*);
LightAllocFn g_light_alloc = nullptr; // DAT_0054d94c. Thunk at 0x000F9C10.
LightFreeFn g_light_free = nullptr;   // DAT_0054d954

LightVolumeRecord g_light_records[kLightVolumeRecords]{};
i32 g_light_filled = 0;                         // DAT_0050d71c. Compared with 0x19.
i32 g_light_filled_gate = 0;                    // DAT_0050d720
u8 g_light_order[kLightVolumeRecords]{};        // DAT_0050d724
i32 g_light_mark_740 = 0;                       // DAT_0050d740
u8 g_light_flag_760 = 0;                        // DAT_0050d760
u8 g_light_flag_761 = 0;
u8 g_light_flag_762 = 0;
u8 g_light_flag_763 = 0;                        // DAT_0050d763

void* g_light_table_slots[4]{}; // DAT_0050d770
i32 g_light_table_count = 0;    // DAT_0050d780
void* g_light_table = nullptr;  // DAT_0050d798
i32 g_light_table_span = 0;     // DAT_0050d79c. Byte count from 0x00035D90 divided by 140.

void light_volume_release(void* block)
{
    // 0x000D8AC0. EDI is the block. 0x000F7100 selects the heap, 0x0014D360
    // frees it, and the previous heap index is restored.
    fn_663e0(block);
}

void light_clear_slots()
{
    // 0x000FDCD0. Writes the forty pointers, then the count.
    g_light_live = 0;
    for (i32 i = 0; i < kLightVolumeSlots; ++i)
    {
        g_light_slots[i] = nullptr;
    }
}

void light_init_records()
{
    // 0x000FE1D0. Each group from DAT_0030ca80 fills that many records.
    i32 index = 0;
    for (const LightVolumeGroup& group : kLightVolumeGroups)
    {
        for (u32 n = 0; n < group.count; ++n)
        {
            LightVolumeRecord& rec = g_light_records[index];
            rec.head = 0;
            rec.tail = 0;
            rec.kind = group.kind;
            index += 1;
        }
    }
    for (i32 i = 0; i < kLightVolumeRecords; ++i)
    {
        g_light_order[i] = static_cast<u8>(i);
    }
    g_light_filled = 0;
    g_light_filled_gate = 0;
    g_light_mark_740 = 0;
    g_light_flag_760 = 0;
    g_light_flag_761 = 0;
    g_light_flag_762 = 0;
    g_light_flag_763 = 0x50;
}

void light_bind_table()
{
    // 0x000FEF60. Clears the four slots and the count, then looks up
    // aid_misc_ghoulies_background_lightvolumetable (DAT_0030ca98) through
    // 0x00035E90. The open directory at DAT_004df250 is not in this file, so
    // the table stays null. 0x00035D90 returns 0 for that miss and the span
    // is that count divided by 140. A null lookup calls 0x00032700 with 2.
    for (i32 i = 0; i < 4; ++i)
    {
        g_light_table_slots[i] = nullptr;
    }
    g_light_table_count = 0;
    g_light_table = nullptr;
    g_light_table_span = 0;
}

} // namespace

void fn_fd620()
{
    // Xbox 0x000FD620
    // 0x000FDCD0 clears the slot table. 0x0015AB90 sizes a 16x16 grid and
    // 0x00177980 allocates it (a zero protect word becomes 0xBC800000, then
    // 0x000F7270). 0x0015AC60 stores the block, 0x0015AC80 stores the allocator
    // thunk 0x000F9C10 (fn_f72d0), and 0x0015AC90 stores the releaser 0x000D8AC0.
    // The pass globals are planted around 0x000FE1D0 and 0x000FEF60.
    light_clear_slots();
    const u32 bytes = light_grid_bytes(0x10, 0x10);
    void* block = std::malloc(bytes);
    g_light_grid = block;
    g_light_grid_bytes = bytes;
    g_light_grid_flag = 0;
    g_light_alloc = &fn_f72d0;
    g_light_free = &light_volume_release;
    g_light_pass.active = 0;
    g_light_pass.gate = 0;
    g_light_pass.color = 0xffc0c0c0u;
    g_light_pass.scale = 1.f;
    g_light_pass.range = 1000.f;
    light_init_records();
    light_bind_table();
    g_light_tint[0] = 1.f;
    g_light_tint[1] = 1.f;
    g_light_tint[2] = 1.f;
    g_light_tint[3] = 1.f;
    g_light_intensity = 1.f;
    g_light_angle_inner = std::bit_cast<float>(0x3eb2b8c3u);
    g_light_angle_outer = std::bit_cast<float>(0x3f32b8c3u);
    g_light_mark = 1;
    g_light_paused = 0;
    g_light_running = 1;
}

namespace
{

// DAT_0054dd7c. Shared LCG. The float is the mantissa of 1.0, so the unit
// value sits in [0, 1).
u32 g_amb_rng = 0;

u32 amb_lcg()
{
    // Xbox 0x00118470
    g_amb_rng = g_amb_rng * 0x19660Du + 0x3C6EF35Fu;
    return g_amb_rng;
}

float amb_rand01()
{
    // Xbox 0x00118470
    const u32 bits = (amb_lcg() & 0x7fffffu) | 0x3f800000u;
    return std::bit_cast<float>(bits) - 1.f;
}

// DAT_004df358. The 0x44-byte misc record
// aid_misc_ghoulies_audio_generalsettings. Game init loads it before cam_11c480.
struct AudioGeneralSettings
{
    float wind_lo = 12.f; // +0x1c
    float wind_hi = 20.f; // +0x20
    float step_lo = 80.f; // +0x24
    float step_hi = 90.f; // +0x28
    float span_lo = 1.f;  // +0x2c
    float span_hi = 1.5f; // +0x30
    i32 count_lo = 5;     // +0x34
    i32 count_hi = 10;    // +0x38
};

constexpr AudioGeneralSettings kAudioGeneral{};

static_assert(std::bit_cast<u32>(kAudioGeneral.wind_lo) == 0x41400000u);
static_assert(std::bit_cast<u32>(kAudioGeneral.wind_hi) == 0x41a00000u);
static_assert(std::bit_cast<u32>(kAudioGeneral.step_lo) == 0x42a00000u);
static_assert(std::bit_cast<u32>(kAudioGeneral.step_hi) == 0x42b40000u);
static_assert(std::bit_cast<u32>(kAudioGeneral.span_hi) == 0x3fc00000u);
static_assert(std::bit_cast<u32>(120.f) == 0x42f00000u);
static_assert(std::bit_cast<u32>(300.f) == 0x43960000u);
static_assert(std::bit_cast<u32>(1.5f) == 0x3fc00000u);

// Wind and rain bed at DAT_0050e7a0. The two words are voice indices into
// the table at 0x004E0CE8 (stride 0x24). 0xFFFF means no voice.
i32 g_amb_counter = 0;     // DAT_0050e7a0. cam_11c480 increments this after the call.
i32 g_amb_hold = 0;        // DAT_0050e7a4
i32 g_amb_armed = 0;       // DAT_0050e7a8
float g_amb_wait = 0.f;    // DAT_0050e7ac
float g_amb_wait_scale = 0.f; // DAT_0050e7b0. Pool float DAT_0032b400 (120).
float g_amb_span = 0.f;    // DAT_0050e7b4. Pool float DAT_0032b180 (300).
float g_amb_level = 0.f;   // DAT_0050e7b8
float g_amb_blend = 0.f;   // DAT_0050e7bc
float g_amb_fade = 0.f;    // DAT_0050e7c0
float g_amb_pitch = 0.f;   // DAT_0050e7c4
float g_amb_goal = 0.f;    // DAT_0050e7c8
float g_amb_rate = 0.f;    // DAT_0050e7cc. Pool float DAT_0032b210 (1.5).
i32 g_amb_gate = 0;        // DAT_0050e7d0
u16 g_amb_rain = 0xFFFF;   // DAT_0050e7d4. XACT_SOUNDBANK_GENVIRONMENT_RAINEXTRA.
u8 g_amb_rain_vol = 0;     // DAT_0050e7d6
u16 g_amb_wind = 0xFFFF;   // DAT_0050e7d8. XACT_SOUNDBANK_GENVIRONMENT_WINDEXTRA.
u8 g_amb_wind_vol = 0;     // DAT_0050e7da
i32 g_amb_mark = 0;        // DAT_0050e7dc
u16 g_amb_extra = 0xFFFF;  // DAT_0050e7e0
float g_amb_extra_wait = 0.f; // DAT_0050e7e4
i32 g_amb_extra_index = 0; // DAT_0050e7e8
i32 g_amb_extra_prev = 0;  // DAT_0050e7ec
Vec3 g_amb_extra_color{};  // DAT_0050e7f0

float g_amb_step_wait = 0.f;  // DAT_0050e7fc
float g_amb_step_timer = 0.f; // DAT_0050e800
float g_amb_step_span = 0.f;  // DAT_0050e804
i32 g_amb_step_left = 0;      // DAT_0050e808
i32 g_amb_step_count = 0;     // DAT_0050e80c
Vec3 g_amb_step_from{};       // DAT_0050e810
Vec3 g_amb_step_to{};         // DAT_0050e81c
Vec3 g_amb_step_now{};        // DAT_0050e828

// Phase block. 0x001185E0 advances DAT_0050e968 under the two gates.
i32 g_amb_count = 0;          // DAT_0050e930
i32 g_amb_vary = 0;           // DAT_0050e944
float g_amb_vary_scale = 0.f; // DAT_0050e948
i32 g_amb_phase_on = 0;       // DAT_0050e950
i32 g_amb_phase_gate = 0;     // DAT_0050e954
float g_amb_phase_a = 0.f;    // DAT_0050e964
float g_amb_phase = 0.f;      // DAT_0050e968
float g_amb_phase_len = 0.f;  // DAT_0050e96c
float g_amb_phase_jitter = 0.f; // DAT_0050e970. Pool float DAT_0032b268.
float g_amb_phase_rate = 0.f; // DAT_0050e974
u32 g_amb_phase_color = 0;    // DAT_0050e978. Dword at DAT_00314f4c.
u8 g_amb_phase_mark = 0;      // DAT_0050e97c

// 0x00118AE0 fills these from the loaded ambience. They stay zero until that
// pass, so the copies below are zero and the RNG still advances.
float g_amb_color_lo[3]{}; // DAT_0050ea04, +0x08, +0x0c
float g_amb_color_hi[3]{}; // DAT_0050ea10, +0x14, +0x18
Vec3 g_amb_palette[8]{};   // DAT_0050ea1c, stride 12

// 0x00039690. A live voice in states 2..7 is moved toward stop. The voice
// table is the XACT block, which this file does not hold. The init writes
// 0xFFFF first, so the call is skipped.
void amb_release(u16& handle)
{
    // Xbox 0x00118470
    if (handle == 0xFFFF)
    {
        return;
    }
    handle = 0xFFFF;
}

// 0x00117980. One unit random times the 120 scale, then the bed floats drop
// to zero. A live wind or rain voice is released and the word becomes 0xFFFF.
void amb_reset_beds()
{
    // Xbox 0x00118470
    g_amb_wait = amb_rand01() * g_amb_wait_scale;
    g_amb_armed = 0;
    g_amb_level = 0.f;
    g_amb_pitch = 0.f;
    g_amb_blend = 0.f;
    g_amb_fade = 0.f;
    g_amb_goal = 0.f;
    amb_release(g_amb_wind);
    amb_release(g_amb_rain);
}

// rand <= 0.5 keeps the low endpoint. COMISS of 0.5 against the unit random.
float amb_endpoint(float lo, float hi)
{
    // Xbox 0x00118470
    if (0.5f >= amb_rand01())
    {
        return lo;
    }
    return hi;
}

// 0x00118D10. ECX is the colour. trunc(rand * 3) picks the axis that is
// lerped; the other two take one endpoint. EAX returns the byte at
// DAT_0050ea00, which 0x00118290 ignores.
void amb_pick_color(Vec3& out)
{
    // Xbox 0x00118470
    const float* lo = g_amb_color_lo;
    const float* hi = g_amb_color_hi;
    const i32 choice = static_cast<i32>(amb_rand01() * 3.f);
    if (choice == 0)
    {
        const float t = amb_rand01();
        out.x = (hi[0] - lo[0]) * t + lo[0];
        out.y = amb_endpoint(lo[1], hi[1]);
        out.z = amb_endpoint(lo[2], hi[2]);
    }
    else if (choice == 1)
    {
        out.x = amb_endpoint(lo[0], hi[0]);
        const float t = amb_rand01();
        out.y = (hi[1] - lo[1]) * t + lo[1];
        out.z = amb_endpoint(lo[2], hi[2]);
    }
    else if (choice == 2)
    {
        out.x = amb_endpoint(lo[0], hi[0]);
        out.y = amb_endpoint(lo[1], hi[1]);
        const float t = amb_rand01();
        out.z = (hi[2] - lo[2]) * t + lo[2];
    }
}

// 0x00117F20. Lerps the step delay, span, and count out of the general
// settings, then copies two different palette rows (stride 12, eight rows).
void amb_sample_step()
{
    // Xbox 0x00118470
    const AudioGeneralSettings& settings = kAudioGeneral;
    g_amb_step_wait = (settings.step_hi - settings.step_lo) * amb_rand01() + settings.step_lo;
    const float span = (settings.span_hi - settings.span_lo) * amb_rand01() + settings.span_lo;
    g_amb_step_span = span;
    g_amb_step_timer = span;
    const float count = static_cast<float>(settings.count_hi - settings.count_lo) * amb_rand01()
        + static_cast<float>(settings.count_lo);
    const i32 count_i = static_cast<i32>(count);
    g_amb_step_count = count_i;
    g_amb_step_left = count_i;
    const i32 first = static_cast<i32>(amb_rand01() * 8.f);
    i32 second = first;
    do
    {
        second = static_cast<i32>(amb_rand01() * 8.f);
    }
    while (second == first);
    g_amb_step_from = g_amb_palette[first];
    g_amb_step_now = g_amb_palette[first];
    g_amb_step_to = g_amb_palette[second];
}

// 0x00118290. Lerps the wind-extra delay, then draws a cue index in [3, 11)
// that differs from the previous one. DAT_0050e7f0 is filled by 0x00118D10.
// The voice word is cleared to 0xFFFF.
void amb_sample_extra()
{
    // Xbox 0x00118470
    const AudioGeneralSettings& settings = kAudioGeneral;
    g_amb_extra_wait = (settings.wind_hi - settings.wind_lo) * amb_rand01() + settings.wind_lo;
    const i32 previous = g_amb_extra_prev;
    i32 index = previous;
    do
    {
        index = static_cast<i32>(amb_rand01() * 8.f + 3.f);
        g_amb_extra_index = index;
    }
    while (index == previous);
    g_amb_extra_prev = index;
    amb_pick_color(g_amb_extra_color);
    amb_release(g_amb_extra);
    g_amb_extra = 0xFFFF;
}

// DAT_0050eaa0, Xbox stride 0x2c, eight records. DAT_0050ec00 is the count
// that follows them. 0x00119160 clears only the object pointer at +0.
struct LookupRecord
{
    void* object = nullptr; // Xbox +0
};

constexpr i32 kLookupRecords = 8;

// Index i (12 bits) becomes A8R8G8B8 with R = low nibble, G = middle, B = high.
// ((i >> 4 & 0xf) | (i & 0xf) << 8) << 8 | (i >> 8 & 0xf)
constexpr u32 lookup_texel(u32 index)
{
    // Xbox 0x00119160
    const u32 lo = index & 0xfu;
    const u32 mid = (index >> 4) & 0xfu;
    const u32 hi = (index >> 8) & 0xfu;
    return (lo << 16) | (mid << 8) | hi;
}

static_assert(lookup_texel(0) == 0u);
static_assert(lookup_texel(1) == 0x00010000u);
static_assert(lookup_texel(0x10) == 0x00000100u);
static_assert(lookup_texel(0x100) == 0x00000001u);
static_assert(lookup_texel(0x123) == 0x00030201u);
static_assert(lookup_texel(0xfff) == 0x000f0f0fu);

LookupRecord g_lookup_records[kLookupRecords]{};
i32 g_lookup_count = 0; // DAT_0050ec00
u32* g_lookup_tex = nullptr; // DAT_0050ec04. 4096 A8R8G8B8 texels.

} // namespace

void fn_118470()
{
    // Xbox 0x00118470
    g_amb_wait_scale = 120.f;
    g_amb_span = 300.f;
    g_amb_rate = 1.5f;
    g_amb_wind_vol = 0x7f;
    g_amb_rain_vol = 0x7f;
    g_amb_vary_scale = 0.f;
    g_amb_phase = 0.f;
    g_amb_phase_a = 0.f;
    g_amb_counter = 0;
    g_amb_hold = 0;
    g_amb_mark = 0;
    g_amb_gate = 0;
    g_amb_wind = 0xFFFF;
    g_amb_rain = 0xFFFF;
    g_amb_count = 0;
    g_amb_vary = 0;
    g_amb_phase_on = 0;
    g_amb_phase_gate = 0;
    g_amb_phase_len = 1.f;
    g_amb_phase_jitter = std::bit_cast<float>(0x3e99999au);
    g_amb_phase_rate = 1.f;
    g_amb_phase_color = 0xff000000u;
    g_amb_phase_mark = 0;
    g_amb_extra = 0xFFFF;
    g_amb_extra_prev = -1;
    amb_reset_beds();
    amb_sample_step();
    amb_sample_extra();
}

void fn_119160()
{
    // Xbox 0x00119160
    // D3DDevice_CreateTexture2(0x1000, 1, 1, 1, 0, D3DFMT_A8R8G8B8, 3).
    // GetLevelDesc's 0x1c-byte result is unused. LockRect then writes 4096
    // texels. XGSwizzleRect(bits, 0, NULL, bits, 0x1000, 1, NULL, 4) runs
    // in place; a one-row NV2A swizzle leaves that order unchanged.
    constexpr u32 kWidth = 0x1000;
    u32* texels = static_cast<u32*>(std::malloc(kWidth * sizeof(u32)));
    g_lookup_tex = texels;
    if (texels != nullptr)
    {
        for (u32 i = 0; i < kWidth; ++i)
        {
            texels[i] = lookup_texel(i);
        }
    }
    for (LookupRecord& rec : g_lookup_records)
    {
        rec.object = nullptr;
    }
    g_lookup_count = 0;
}

namespace
{

// Intrusive link. 0x000F3C30 appends EDX to the head in EAX. The head object
// is only these two pointers; records that hang off it are larger.
struct AidLink
{
    AidLink* prev = nullptr; // +0
    AidLink* next = nullptr; // +4
};

// 0x124-byte aid record allocated by 0x000F4E50 when the name is 'aid_'.
// 0x000F4EE0 reads the link and the four blocks, then frees the record.
struct AidRecord
{
    AidLink link;
    void* block[4]{}; // Xbox +8, +0xc, +0x10, +0x14
};

// DAT_00510568. 0x00028640 links the object at body+0xb98 here.
AidLink g_side_list{};
// DAT_00510570. DAT_00510574 is the first aid record.
AidLink g_aid_list{};
// DAT_00510578. 0x00028830 increments this when the word at body+0xcdc is 1.
i32 g_aid_body_count = 0;

// 0x000F37B0. EBX is the block. The pointer at +0 is freed, then the block.
void release_aid_block(void* block)
{
    // Xbox 0x0011B630
    void* inner = *static_cast<void**>(block);
    fn_663e0(inner);
    fn_663e0(block);
}

// 0x000F4EE0. EAX is the record. A null prev skips the unlink; the four
// blocks are still released and the record is freed.
void release_aid_record(AidRecord* node)
{
    // Xbox 0x0011B630
    if (node->link.prev != nullptr)
    {
        node->link.prev->next = node->link.next;
        if (node->link.next != nullptr)
        {
            node->link.next->prev = node->link.prev;
        }
        node->link.prev = nullptr;
        node->link.next = nullptr;
    }
    for (i32 i = 0; i < 4; ++i)
    {
        void* block = node->block[i];
        if (block != nullptr)
        {
            release_aid_block(block);
        }
        node->block[i] = nullptr;
    }
    fn_663e0(node);
}

} // namespace

void fn_1233c0()
{
    // Xbox 0x001233C0
    // cam_11c480 calls this before the aid-list release. Every head from
    // DAT_00511AA0 through DAT_00511B58 is stored as zero. The nodes stay
    // allocated; later walks no longer find them.
    g_world = {};
}

void fn_11b630()
{
    // Xbox 0x0011B630
    // The next pointer is taken before 0x000F4EE0, which unlinks and frees
    // the record. The side list at DAT_00510568 is cleared without a walk.
    AidLink* link = g_aid_list.next;
    while (link != nullptr)
    {
        AidLink* next = link->next;
        release_aid_record(reinterpret_cast<AidRecord*>(link));
        link = next;
    }
    g_aid_list.prev = nullptr;
    g_aid_list.next = nullptr;
    g_side_list.prev = nullptr;
    g_side_list.next = nullptr;
    g_aid_body_count = 0;
}

namespace
{

// DAT_00506c74. 0x000F2B60 allocates 0xe bytes: kind 9, size 2, and the
// byte pointer aimed at the inline payload. The two bytes are scene flags.
struct SceneFlagBlock
{
    i32 kind = 0;      // +0
    i32 size = 0;      // +4
    u8* bytes = nullptr; // +8
};

SceneFlagBlock* g_scene_flags = nullptr; // DAT_00506c74

i32 g_scene_time = 0;   // DAT_00506c78. Sum of the truncated float at DAT_004312d0.
i32 g_scene_acc_7c = 0; // DAT_00506c7c
i32 g_scene_acc_80 = 0; // DAT_00506c80
i32 g_scene_acc_84 = 0; // DAT_00506c84
i32 g_scene_acc_88 = 0; // DAT_00506c88
i32 g_scene_acc_8c = 0; // DAT_00506c8c
u16 g_scene_gate = 0;   // DAT_00506c90. Nonzero runs the update at 0x000F2C40.

// DAT_00511b64. 0x000E270F stores a heap scene name here. 0x001234B0 stores
// the C string 0x00035E90 writes from the loaded aid (record +8).
const char* g_scene_name_src = nullptr;

// DAT_00511b68. Called with &DAT_00511b64 before that pointer is replaced.
using SceneNameRelease = void (*)(const char** slot);
SceneNameRelease g_scene_name_release = nullptr;

// DAT_004dd8fc. 0x00032700 stores the code 0x001234B0 passes (2).
i32 g_asset_fault_code = 0;

// DAT_00510ed8. 0x80 bytes, then the mode dword at DAT_00510f58.
// 0x0012C2D0 passes this address to the script lookup.
constexpr i32 kSceneNameBytes = 0x80;
char g_scene_name[kSceneNameBytes]{};
i32 g_scene_name_mode = 0; // DAT_00510f58

// DAT_00430d08. Three dwords, then an 0x80-byte path at DAT_00430d14.
// DAT_00430d94 sits immediately after that path.
i32 g_script_id = 0;        // DAT_00430d08
i32 g_script_state = 0;     // DAT_00430d0c
i32 g_script_count = 0;     // DAT_00430d10
char g_script_path[0x80]{}; // DAT_00430d14
i32 g_script_busy = 0;      // DAT_00430d94. 0x0008F675 sets this to 1.
i32 g_script_busy_next = 0; // DAT_00430d98

// 'aid_' as a little-endian dword. 0x0011EB30 clears the name when it matches.
constexpr u32 kAidPrefix = 0x5f646961u;
static_assert(kAidPrefix == (static_cast<u32>('a') | (static_cast<u32>('i') << 8) |
                             (static_cast<u32>('d') << 16) | (static_cast<u32>('_') << 24)));

// 0x0011EB30. EAX is the source string. The stack dword is stored at
// DAT_00510f58. A null source is left uncopied; Xbox would fault on it.
void scene_copy_name(const char* src, i32 mode)
{
    // Xbox 0x0011EAD0
    u32 head = 0;
    std::memcpy(&head, g_scene_name, sizeof(head));
    if (head == kAidPrefix)
    {
        g_scene_name[0] = '\0';
    }
    if (src != nullptr)
    {
        i32 i = 0;
        while (i < kSceneNameBytes - 1 && src[i] != '\0')
        {
            g_scene_name[i] = src[i];
            i += 1;
        }
        g_scene_name[i] = '\0';
    }
    g_script_id = 0;
    g_script_state = 0;
    g_script_count = 0;
    g_script_path[0] = '\0';
    if (g_script_busy == 0)
    {
        g_scene_acc_8c = 0;
    }
    g_script_busy = 0;
    g_script_busy_next = 0;
    g_scene_name_mode = mode;
}

// 0x00035940. EBX is the held pointer. 0x00035C00 drops one ref on the aid
// whose record+8 matches it. The aid directory is not in this file, so the
// ref walk has nothing to find and the slot is cleared.
void scene_release_aid(const char** slot)
{
    // Xbox 0x001234B0
    if (slot == nullptr)
    {
        return;
    }
    *slot = nullptr;
}

// 0x00032700. Stores the code at DAT_004dd8fc. DAT_004dd8f8 is BSS, so retail
// then spins and does not reach the stores after the call. The port records
// the code and returns so the hold is still published.
void scene_report_missing(i32 code)
{
    // Xbox 0x001234B0
    g_asset_fault_code = code;
}

// 0x00035E90. The name is the first stack argument. The out slot is the
// saved ECX, and a hit writes record+8 (a C string). DAT_004df250,
// DAT_004df244, and DAT_004df248 are not open here, so the slot stays null.
const char* scene_lookup_aid(const char* name)
{
    // Xbox 0x001234B0
    (void)name;
    return nullptr;
}

} // namespace

void fn_1234b0(const char* name)
{
    // Xbox 0x001234B0
    if (g_scene_name_release != nullptr && g_scene_name_src != nullptr)
    {
        g_scene_name_release(&g_scene_name_src);
    }

    // Xbox reads the first dword with no null test. A null name misses the
    // 'aid_' check instead of faulting.
    u32 head = 0;
    if (name != nullptr)
    {
        std::memcpy(&head, name, sizeof(head));
    }
    if (head != kAidPrefix)
    {
        g_scene_name_src = nullptr;
        g_scene_name_release = &scene_release_aid;
        return;
    }

    const char* resolved = scene_lookup_aid(name);
    if (resolved == nullptr)
    {
        scene_report_missing(2);
    }
    g_scene_name_src = resolved;
    g_scene_name_release = &scene_release_aid;
}

void fn_11ead0()
{
    // Xbox 0x0011EAD0
    // rep stos of the byte count at flag+4 into the pointer at flag+8.
    // 0x000F2B60 publishes that block; a missing one is left alone.
    SceneFlagBlock* block = g_scene_flags;
    if (block != nullptr && block->bytes != nullptr && block->size > 0)
    {
        std::memset(block->bytes, 0, static_cast<size_t>(block->size));
    }
    g_scene_gate = 0;
    g_scene_time = 0;
    g_scene_acc_7c = 0;
    g_scene_acc_80 = 0;
    g_scene_acc_84 = 0;
    g_scene_acc_88 = 0;
    g_scene_acc_8c = 0;
    scene_copy_name(g_scene_name_src, 1);
}

namespace
{

// DAT_00512140. Twenty records, stride 0x18, ending at DAT_00512320.
// +0 is the live flag, +4 the float the tick decays, +0xc the tag
// 0x00124320 posts, and +0x14 the voice key 0x001316F0 stored.
struct EffectSlot
{
    i32 live = 0;
    float time = 0.f;
    i32 value = 0;
    i32 tag = 0;
    i32 param = 0;
    i32 voice = 0;
};

constexpr i32 kEffectSlotCount = 20;
EffectSlot g_effect_slots[kEffectSlotCount]{};

// DAT_00512320. Pointers occupy DAT_00512324 and stop before DAT_00512344.
// The times at DAT_00512334 are a second array and this reset leaves them.
constexpr i32 kEffectSpawnCap = 4;
void* g_effect_spawn[kEffectSpawnCap]{};
i32 g_effect_spawn_count = 0;

// Voice record inside the span at [DAT_00545abc]+0. The key is the dword
// at +0. The aid 0x00035C00 releases sits at +0x88. Stride is 0x94.
struct EffectVoice
{
    i32 key = 0;
    void* aid = nullptr;
};

// Span allocated by 0x00130BB0. +0 is the stride word, +4/+8 the range.
struct EffectVoiceSpan
{
    u16 stride = 0x94;
    std::vector<EffectVoice> voices;
};

// DAT_00545abc. Null until that allocator runs. +0 is the span.
struct EffectBank
{
    EffectVoiceSpan* span = nullptr;
};

EffectBank* g_effect_bank = nullptr;

// 0x001319C0. EAX is the bank, ECX the slot key. The first matching record
// releases its aid, then the tail slides down by one stride and the end
// shrinks. A miss leaves the span alone. The caller ignores the return.
void effect_drop_voice(EffectBank& bank, i32 key)
{
    // Xbox 0x00123B30
    EffectVoiceSpan* span = bank.span;
    if (span == nullptr)
    {
        return;
    }
    std::vector<EffectVoice>& voices = span->voices;
    for (size_t i = 0; i < voices.size(); ++i)
    {
        if (voices[i].key != key)
        {
            continue;
        }
        // 0x00035C00. The aid directory is not in this file, so the hold
        // is cleared. 0x00165AF0 then memmoves the following records down.
        voices[i].aid = nullptr;
        voices.erase(voices.begin() + static_cast<std::ptrdiff_t>(i));
        return;
    }
}

} // namespace

void fn_123b30()
{
    // Xbox 0x00123B30
    // The bank is re-read after each drop. A null bank skips 0x001319C0
    // and the slot is still cleared. The float at +4 is a movss of 0.
    for (EffectSlot& slot : g_effect_slots)
    {
        if (g_effect_bank != nullptr)
        {
            effect_drop_voice(*g_effect_bank, slot.voice);
        }
        slot = {};
    }

    i32 count = g_effect_spawn_count;
    if (count > kEffectSpawnCap)
    {
        count = kEffectSpawnCap;
    }
    for (i32 i = 0; i < count; ++i)
    {
        g_effect_spawn[i] = nullptr;
    }
    g_effect_spawn_count = 0;
}

namespace
{

// DAT_00512520. Two bins, stride 0x14, ending at DAT_00512548.
// +0 is the cap 0x0012A7D0 subtracts from one list length before it
// walks the list at +0x10. The four list heads are left in place.
struct IdBin
{
    i32 cap = 0;
    u32 list_4 = 0;
    u32 list_8 = 0;
    u32 list_c = 0;
    u32 list_10 = 0;
};

static_assert(sizeof(IdBin) == 0x14);
static_assert(offsetof(IdBin, list_10) == 0x10);

constexpr i32 kIdBinCount = 2;
IdBin g_id_bins[kIdBinCount]{};

// DAT_0051254c. Ten records, stride 0x90, so the id words run from
// DAT_005125d8 and stop before DAT_00512b78. +0x8c is the id 0x0012A1E0
// stores from 0x00039830. 0xFFFF means that slot has none. The name at
// +0 and the live count at DAT_00512548 are not part of this reset.
struct IdRecord
{
    u8 head[0x8C]{};
    u16 id = 0;
    u8 tail[2]{};
};

static_assert(offsetof(IdRecord, id) == 0x8C);
static_assert(sizeof(IdRecord) == 0x90);

constexpr i32 kIdRecordCount = 10;
IdRecord g_id_records[kIdRecordCount]{};

} // namespace

void fn_129f30()
{
    // Xbox 0x00129F30
    for (IdRecord& record : g_id_records)
    {
        record.id = 0xFFFF;
    }
    g_id_bins[0].cap = 0x14;
    g_id_bins[1].cap = 6;
}

namespace
{

// Header of a record pool. Retail stores this in a 0x10 byte block from
// 0x000F72D0: stride, grow, then begin / cursor / limit. An empty pool
// points all three one past the header. The port keeps those cursors
// here; the block still carries the two words 0x000F73A0 copies on grow.
struct SavePool
{
    u16 stride = 0;
    u16 grow = 0;
    u8* begin = nullptr;
    u8* cursor = nullptr;
    u8* limit = nullptr;
    u8* block = nullptr;
    i32 block_bytes = 0;
};

// DAT_005462e4. +0 is the stride-0x18 slot pool. +8 is the stride-0x100
// name pool. +0x10 is the count 0x0010F2D0 increments. +0x14 is the float
// 0x00033BF2 decays by DAT_005096C0 while it stays above zero.
struct SaveScan
{
    SavePool* slots = nullptr;
    i32 unused_4 = 0;
    SavePool* names = nullptr;
    i32 unused_c = 0;
    i32 pending = 0;
    float timer = 0.f;
};

SaveScan* g_save_scan = nullptr;

void bind_save_pool(SavePool& pool, u8* block, i32 block_bytes, i32 used)
{
    pool.block = block;
    pool.block_bytes = block_bytes;
    std::memcpy(block, &pool.stride, sizeof(pool.stride));
    std::memcpy(block + 2, &pool.grow, sizeof(pool.grow));
    pool.begin = block + 0x10;
    const i32 stride = pool.stride == 0 ? 1 : static_cast<i32>(pool.stride);
    pool.cursor = pool.begin + static_cast<ptrdiff_t>(stride) * used;
    const i32 capacity = (block_bytes - 0x10) / stride;
    pool.limit = pool.begin + static_cast<ptrdiff_t>(stride) * capacity;
}

SavePool* make_save_pool(u16 stride, u16 grow)
{
    u8* block = static_cast<u8*>(fn_f72d0(0x10));
    if (block == nullptr)
    {
        return nullptr;
    }
    auto* pool = new SavePool;
    pool->stride = stride;
    pool->grow = grow;
    bind_save_pool(*pool, block, 0x10, 0);
    return pool;
}

void release_save_pool(SavePool* pool)
{
    if (pool == nullptr)
    {
        return;
    }
    if (pool->block != nullptr)
    {
        fn_663e0(pool->block);
    }
    delete pool;
}

// 0x000662A0 via 0x000F73A0. used = (cursor - begin) / stride. The new
// count is used plus the grow word. The cursor stays on the same record.
void grow_save_pool(SavePool& pool)
{
    const i32 stride = pool.stride == 0 ? 1 : static_cast<i32>(pool.stride);
    i32 used = 0;
    if (pool.begin != nullptr && pool.cursor != nullptr && pool.cursor >= pool.begin)
    {
        used = static_cast<i32>(pool.cursor - pool.begin) / stride;
    }
    const i32 new_count = used + static_cast<i32>(pool.grow);
    const i32 new_bytes = stride * new_count + 0x10;
    u8* fresh = static_cast<u8*>(fn_f72d0(new_bytes));
    if (fresh == nullptr)
    {
        return;
    }
    if (pool.block != nullptr && pool.block_bytes > 0)
    {
        const i32 copy = pool.block_bytes < new_bytes ? pool.block_bytes : new_bytes;
        std::memcpy(fresh, pool.block, static_cast<size_t>(copy));
        fn_663e0(pool.block);
    }
    bind_save_pool(pool, fresh, new_bytes, used);
}

u8* bump_save_record(SavePool& pool)
{
    if (pool.cursor == pool.limit)
    {
        grow_save_pool(pool);
    }
    if (pool.cursor == nullptr || pool.limit == nullptr || pool.cursor >= pool.limit)
    {
        return nullptr;
    }
    u8* record = pool.cursor;
    pool.cursor = record + pool.stride;
    std::memset(record, 0, pool.stride);
    return record;
}

// 0x0014C3AF. The file is UTF-16. The first wchar was already consumed by
// the 2-byte probe. The first "Name=" wins, including one sitting inside
// "TitleName=". The value runs until CR LF and is clipped to 0x7F wchars.
bool take_save_name(const std::vector<char16_t>& text, char16_t* dest)
{
    constexpr i32 kMax = 0x7F;
    for (size_t i = 0; i + 5 <= text.size(); ++i)
    {
        if (text[i] != u'N' || text[i + 1] != u'a' || text[i + 2] != u'm' || text[i + 3] != u'e' ||
            text[i + 4] != u'=')
        {
            continue;
        }
        const size_t value = i + 5;
        size_t end = value;
        while (end + 1 < text.size())
        {
            if (text[end] == u'\r' && text[end + 1] == u'\n')
            {
                size_t count = end - value;
                if (count > static_cast<size_t>(kMax))
                {
                    count = static_cast<size_t>(kMax);
                }
                for (size_t n = 0; n < count; ++n)
                {
                    dest[n] = text[value + n];
                }
                dest[count] = 0;
                return true;
            }
            end += 1;
        }
        return false;
    }
    return false;
}

// 0x0014C532. Directories only. Opens dirname\SaveMeta.xbx, requires a
// 2-byte read, then the Name field. A miss returns false so the walk
// continues. The wide value is what 0x0010F1C0 copies into the record.
bool read_save_meta_name(const std::filesystem::path& dir, char16_t* dest)
{
    const std::filesystem::path file = dir / "SaveMeta.xbx";
    std::ifstream in(file, std::ios::binary);
    if (!in)
    {
        return false;
    }
    unsigned char probe[2];
    in.read(reinterpret_cast<char*>(probe), 2);
    if (in.gcount() != 2)
    {
        return false;
    }
    std::vector<unsigned char> rest;
    rest.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    std::vector<char16_t> text(rest.size() / 2);
    for (size_t i = 0; i < text.size(); ++i)
    {
        text[i] = static_cast<char16_t>(rest[i * 2] | (static_cast<unsigned>(rest[i * 2 + 1]) << 8));
    }
    return take_save_name(text, dest);
}

// 0x0010F1C0. FindFirstFile of U:\*. Each directory whose SaveMeta.xbx
// yields a Name is one stride-0x100 record, copied with wcscpy. A missing
// drive returns -1 from the open and leaves the pool empty.
void scan_save_names(SavePool& names)
{
    std::error_code ec;
    const std::filesystem::directory_iterator end;
    std::filesystem::directory_iterator it(std::filesystem::path("U:\\"), ec);
    if (ec)
    {
        return;
    }
    for (; it != end; it.increment(ec))
    {
        if (ec)
        {
            break;
        }
        std::error_code dir_ec;
        if (!it->is_directory(dir_ec) || dir_ec)
        {
            continue;
        }
        char16_t name[0x80];
        name[0] = 0;
        if (!read_save_meta_name(it->path(), name))
        {
            continue;
        }
        u8* record = bump_save_record(names);
        if (record == nullptr)
        {
            return;
        }
        auto* dest = reinterpret_cast<char16_t*>(record);
        const char16_t* src = name;
        while ((*dest++ = *src++) != 0)
        {
        }
    }
}

} // namespace

void fn_10d3b0()
{
    // Xbox 0x0010D3B0
    // 0x000F72D0 allocates the 0x18 byte scan, then two 0x10 pool headers.
    // The first stride is 0x18 and the second is 0x100. Both grow words are
    // 10, and both cursors start empty. +4, +0xc, +0x10, and the float at
    // +0x14 are cleared, then 0x0010F1C0 fills the wide pool from U:\.
    // A second call drops the previous pools before the new allocations.
    if (g_save_scan != nullptr)
    {
        release_save_pool(g_save_scan->slots);
        release_save_pool(g_save_scan->names);
        delete g_save_scan;
        g_save_scan = nullptr;
    }

    auto* scan = new SaveScan;
    scan->slots = nullptr;
    SavePool* slots = make_save_pool(0x18, 10);
    if (slots == nullptr)
    {
        delete scan;
        return;
    }
    scan->slots = slots;
    scan->names = nullptr;
    SavePool* names = make_save_pool(0x100, 10);
    if (names == nullptr)
    {
        release_save_pool(slots);
        delete scan;
        return;
    }
    scan->names = names;
    scan->unused_4 = 0;
    scan->unused_c = 0;
    scan->pending = 0;
    scan->timer = 0.f;
    g_save_scan = scan;
    scan_save_names(*names);
}

} // namespace gbtg
