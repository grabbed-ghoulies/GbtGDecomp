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
#include "entity.h"
#include "../camera/entity_camera.h"
#include "entity_collision.h"
#include "entity_heap.h"

namespace gbtg
{
using namespace entity_detail;

namespace entity_detail
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


bool body_gate(Entity&)
{
    // 0x000CFFD0. Default slot 7. Returns 1. Camera ctors replace it.
    return true;
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
    entity_initialize_body_methods(t.methods);
    t.tick = t.methods.tick;
    t.shutdown = t.methods.shutdown;
    t.handle_message = t.methods.handle_message;
    u32 sizes[2] = {};
    t.methods.query_size(sizes);
    t.xbox_size = sizes[0];
    t.xbox_extra = sizes[1];
    g_types.push_back(t);
}

} // namespace entity_detail


// Xbox 0x00073760
void entity_release_channel_slots(Entity& body)
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
        engine_heap_free(base);
    }
}


// Xbox 0x000FCD70
void entity_copy_match_block(void* dest, const void* src)
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

static_assert(std::bit_cast<u32>(0.01f) == 0x3C23D70Au);
  // 0x0032B124
static_assert(std::bit_cast<u32>(-0.01f) == 0xBC23D70Au);
 // 0x0032B270

// Xbox 0x0010AE90
i32 entity_match_blocks_equal(const EntityMatchBlock& left, const EntityMatchBlock& right)
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

namespace entity_detail
{


// DAT_0050dd80. Three BSS floats. Message 9 copies them onto the actor and the body.
Vec3 g_anchor{};


struct SegmentProbe;


WorldLists g_world{};
 // 0x0032B124, 0.01

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
        actor_set_world_position(*actor, actor->parked_pos);
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

} // namespace entity_detail
 // namespace

// Xbox 0x00152BB0
float engine_absolute(float value)
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


// Xbox 0x001534C0
float engine_reciprocal_sqrt(float value)
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


// Xbox 0x0001DA60
void entity_initialize_body_methods(EntityMethods& methods)
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
    // Retail stores 0x00072B10 at +0x18 and 0x00072CB0 at +0x1C.
    EntityType camera;
    camera.name = "entityBodyCamera";
    camera.kind = EntityKind::Body;
    camera.xbox_desc_va = 0x00504F80;
    entity_initialize_body_methods(camera.methods);
    camera.methods.query_size = entity_body_camera_sizeof;
    camera.methods.tick = entity_body_camera_tick;
    camera.methods.shutdown = entity_body_camera_shutdown;
    camera.methods.handle_message = entity_body_camera_message;
    camera.methods.apply = camera_capture_actor_pose;
    camera.methods.gate = camera_probe_actor_movement;
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





// Xbox 0x001233C0
void entity_clear_scene_lists()
{
    // Xbox 0x001233C0
    // cam_11c480 calls this before the aid-list release. Every head from
    // DAT_00511AA0 through DAT_00511B58 is stored as zero. The nodes stay
    // allocated; later walks no longer find them.
    g_world = {};
}

} // namespace gbtg
