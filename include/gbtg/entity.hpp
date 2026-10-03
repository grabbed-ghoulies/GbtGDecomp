#pragma once

#include "gbtg/types.hpp"

#include <string>
#include <vector>

namespace gbtg
{

// Rare entity split recovered from XBE type names: Body / Mind / Strategy.
// Types are C vtables filled by static ctors that call fn_1da60 (0x0001DA60)
// then overwrite slots. entityBodyCamera_ctor (0x00073190) is the first reconstructed example.

enum class EntityKind
{
    Body,
    Mind,
    Strategy,
};

struct Actor;

// Record at actor+0x88. 0x6c bytes. +8 is the parent. +0xc is the basis
// 0x00040FB0 multiplies. +0x3c is the inverse 0x00041730 applies.
// +0 / +1 select how 0x0003B770 fills the basis (parent clear / parent set).
// +2 bit 0 skips the column normalize. +4 is the bone record for modes 2 and 3.
struct ActorBind
{
    u8 mode_local = 0;  // +0
    u8 mode_parent = 0; // +1
    u8 flags = 0;       // +2
    void* bone = nullptr; // +4
    Actor* parent = nullptr; // +8. Non-null keeps the bind.
    float basis[12] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f}; // +0xc
    float m[12] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};      // +0x3c
};

// Node pointed at by actor+0x918. The first dword keeps a bind whose parent is null.
// Xbox recovers the owning actor as this pointer minus 0x918.
struct ActorLink
{
    void* head = nullptr; // +0
    Actor* owner = nullptr;
};

struct EntityCaseLink;
struct EntityBitBlock;

// Object at actor+0x95c. +8 is the actor 0x00041820 reads. +0x72c receives that world euler.
// +0x48 is the case link 0x0013D2C0 reads, and +0x15c is that function's bit block.
struct ActorEulerOut
{
    Actor* actor = nullptr; // +8
    EntityCaseLink* case_link = nullptr; // +0x48
    EntityBitBlock* bits = nullptr;      // +0x15c. Byte +5 bit 0 adds 5 to the cylinder radius.
    Vec3 position{};                     // +0x18, world position written by 0x0002B430
    Vec3 euler{};                        // +0x72c
};

struct Actor
{
    u16 class_key = 0; // Xbox actor+0x06. 0x00145FF0 matches this against the case-link slots.
    Vec3 position{}; // Xbox actor+0x08
    Vec3 euler{};    // Xbox actor+0x14 when the object stores euler (0x00041820 reads three floats)
    Quat quat{0.f, 0.f, 0.f, 1.f}; // cutscene camera writes xyzw at +0x14 (0x0004D8CE)
    float fov = 0.9f;              // cutscene camera +0x24
    Actor* attach = nullptr;
    ActorBind* bind = nullptr; // Xbox actor+0x88
    i32 xform_valid = 0;       // Xbox actor+0x2a8. 0 means the matrix at +0x2ac is stale.
    float xform[12] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};     // +0x2ac
    float xform_inv[12] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f}; // +0x2dc
    ActorLink* link = nullptr; // Xbox actor+0x918
    ActorEulerOut* euler_out = nullptr; // Xbox actor+0x95c
    float scale_x = 0.f;   // +0x30, axis scale for 0x00040FB0
    float radius_y = 0.f;  // +0x34, scaled by body+0x884 in message 99
    float radius_xz = 0.f; // +0x38, scaled by body+0x888
    float scale_200 = 0.f; // +0x200, multiplies the three axis scales
    Vec3 anchor{};         // +0x3c, copy of DAT_0050dd80
    Vec3 parked_pos{};     // +0xcc0
    Vec3 parked_euler{};   // +0xccc
    i32 parked_gate = 0;   // +0xcd8
    u32 intro_flags = 0;   // 0x00042500. Bit 2 arms the query in message 0x23.
};

struct Entity;
struct Message
{
    i32 id = 0;
    Vec3 vec{};
    // Xbox payload dword at +4 when the message carries a pointer or an integer.
    // Id 99's circle is separate: on Xbox those floats share this same payload.
    void* arg = nullptr;
    i32 arg_i = 0;
    u8 arg_byte = 0;
    Vec3 hit_center{};   // EDI+4 / +8 / +0xC from the payload (message +8 / +0xC / +0x10)
    float hit_extent_y = 0.f; // EDI+0x14
    float hit_radius = 0.f;   // EDI+0x1C
};

// Slot at case-link+0x948, stride 0x10. +0 is the key, +4 is the class 0x00145FF0 returns.
struct EntityClassSlot
{
    u16 key = 0;
    u16 id = 0;
};

// Object at body+0x48 and at [actor+0x95c]+0x48. Case selectors read the byte at
// +0x93d. A null link is state 0. 0x00145FF0 walks the slots: a matching key
// returns that slot's class. An empty list returns 0. A miss returns the first
// slot's class, the word at +0x94c.
struct EntityCaseLink
{
    u8 state = 0; // +0x93d
    std::vector<EntityClassSlot> classes;
};

// Default Body method table. Xbox writes 24 dwords (0x60 bytes) through EAX.
// Offsets are the descriptor slots. A type ctor overwrites the ones it owns.
struct EntityMethods
{
    void (*query_size)(u32 out[2]) = nullptr; // +0x00
    void (*tick)(Entity& body) = nullptr; // +0x04
    void (*shutdown)(Entity& body) = nullptr; // +0x08
    bool (*handle_message)(Entity& body, const Message& msg) = nullptr; // +0x0C
    void (*relay_message)(Entity& body, const Message& msg) = nullptr; // +0x10
    void (*step_channels)(Entity& body) = nullptr; // +0x14
    void (*apply)(Entity& body) = nullptr; // +0x18
    i32 (*gate)() = nullptr; // +0x1C
    i32 (*pick_actor)(const Entity& body, i32 fallback) = nullptr; // +0x20
    i32 (*pick_gated)(const Entity& body) = nullptr; // +0x24
    i32 (*link_idle)(const Entity& body) = nullptr; // +0x28
    i32 (*pick_actor_b)(const Entity& body, i32 fallback) = nullptr; // +0x2C
    i32 (*pick_direct)(const Entity& body) = nullptr; // +0x30
    i32 (*pick_direct_b)(const Entity& body) = nullptr; // +0x34
    i32 (*pick_gated_b)(const Entity& body) = nullptr; // +0x38
    i32 (*pick_gated_c)(const Entity& body) = nullptr; // +0x3C
    i32 (*pick_gated_d)(const Entity& body) = nullptr; // +0x40
    i32 (*pick_direct_c)(const Entity& body) = nullptr; // +0x44
    i32 (*pick_range)(const Entity& body) = nullptr; // +0x48
    i32 (*fixed_id)() = nullptr; // +0x4C
    i32 (*pick_direct_d)(const Entity& body) = nullptr; // +0x50
    i32 (*pick_direct_e)(const Entity& body) = nullptr; // +0x54
    i32 (*pick_direct_f)(const Entity& body) = nullptr; // +0x58
    i32 (*pick_near)(const Entity& body) = nullptr; // +0x5C
};

struct EntityType
{
    const char* name = "";
    EntityKind kind = EntityKind::Body;
    u32 xbox_desc_va = 0;
    u32 xbox_size = 0;
    u32 xbox_extra = 0;
    void (*tick)(Entity& e) = nullptr;
    void (*shutdown)(Entity& e) = nullptr;
    bool (*handle_message)(Entity& e, const Message& msg) = nullptr;
    EntityMethods methods{};
};

// Object at body+0x744 and body+0x314. 0x000253B0 calls vtable +0xC with the message.
struct EntityChild
{
    void (*handle_message)(EntityChild& self, const Message& msg) = nullptr;
};

// Bit bytes at [body+0x15c]+8. 0x0001F030 clears one bit by index.
struct EntityBitBlock
{
    u8* bytes = nullptr; // +8
};

// Object at body+0x2c8. Message 0x47 inserts arg when live and gate are both set.
struct EntityWatch
{
    void* live = nullptr; // +8
    void* link = nullptr; // +0x10, required by message 6
    void* gate = nullptr; // +0x14
    std::vector<void*> tokens; // +0x18, unique dword list
};

// Node at body+0x894. +0 / +4 are the doubly linked view list. The name
// cam_func_48cf0 compares is the C string at Xbox +0x34.
struct EntityCamNode
{
    EntityCamNode* prev = nullptr; // +0
    EntityCamNode* next = nullptr; // +4
    char name[0x21]{};             // +0x34
};

struct Entity
{
    const EntityType* type = nullptr;
    Actor* actor = nullptr;
    Vec3 position{};     // Xbox body+0x18 (camera translate msg)
    Vec3 pose_position{}; // Xbox body+0x89C — world pos copied each tick
    Vec3 pose_euler{};    // Xbox body+0x8A8
    Quat pose_quat{0.f, 0.f, 0.f, 1.f};
    float pose_fov = 0.9f;
    EntityCamNode cam{}; // Xbox body+0x894
    bool listed = false;
    EntityCaseLink* case_link = nullptr; // Xbox body+0x48
    // Message targets. A null child skips the vtable +0xC forward in 0x000253B0.
    EntityChild* child_744 = nullptr; // +0x744
    EntityChild* child_314 = nullptr; // +0x314
    void* link_10 = nullptr;  // +0x10
    void* link_14 = nullptr;  // +0x14
    void* link_4c = nullptr;  // +0x4c
    void* link_50 = nullptr;  // +0x50
    void* link_54 = nullptr;  // +0x54
    void* link_270 = nullptr; // +0x270
    void* link_274 = nullptr; // +0x274
    void* link_718 = nullptr; // +0x718
    void* note_760 = nullptr; // +0x760
    void* note_7f0 = nullptr; // +0x7f0
    void* record_73c = nullptr; // +0x73c, gate for 0x00027510
    EntityWatch* watch = nullptr; // +0x2c8
    EntityBitBlock* bits = nullptr; // +0x15c, bytes at +8
    u8 motion_6a = 0; // +0x6a
    u8 motion_6b = 0; // +0x6b
    u8 mark_288 = 0;  // +0x288
    u8 mark_87e = 0;  // +0x87e
    u16 anim_94 = 0;  // +0x94
    u16 anim_96 = 0;  // +0x96
    u16 mark_748 = 0; // +0x748
    u16 mark_74a = 0; // +0x74a
    u16 mark_880 = 0; // +0x880
    i32 slot_70 = 0;  // +0x70
    i32 slot_740 = 0; // +0x740
    float timer_758 = 0.f; // +0x758
    float timer_7e8 = 0.f; // +0x7e8
    float scale_884 = 0.f; // +0x884
    float scale_888 = 0.f; // +0x888
    Vec3 anchor{};      // +0x318
    Vec3 world_euler{}; // +0x72c
    u8 latch[6]{};      // +0x260 .. +0x265
    // +0x8ac, +0x8c0, +0x8d4, +0x8e8, +0x8fc, +0x910. 0x00073A90 plants
    // 0x30-byte slices of one heap block. Index 0 is the base when it is live.
    void* block_8ac[6]{};
};

const EntityType* find_entity_type(std::string_view name);
std::vector<const EntityType*> all_entity_types();
void register_entity_types();

// EAX is the type descriptor. Writes the 24 default Body methods.
void fn_1da60(EntityMethods& methods);

Vec3 actor_world_position(const Actor& actor); // 0x000412A0
Vec3 actor_world_euler(const Actor& actor);    // 0x00041820

// 3x4 row-major (12 floats). 0x00041820 passes the block at bind+0xc.
// Returns pitch, yaw, roll in xyz. EBX is that out-vector on Xbox.
Vec3 fn_104040(const float m[12]); // 0x00104040
void fn_41730(Actor& actor, const Vec3& point); // 0x00041730 EDI actor, ESI point

// EAX is an optional position (null reads actor+0x08). out is the 3x4 at the
// second stack argument.
void fn_40fb0(Actor& actor, float out[12], const Vec3* position); // 0x00040FB0
void* fn_f72d0(i32 size);                      // 0x000F72D0 stack size, EAX = block
void fn_663e0(void* block);                    // 0x000663E0 EAX = block
void fn_73760(Entity& body);                   // 0x00073760 EAX = body
void fn_fcd70(void* dest, const void* src);    // 0x000FCD70 EAX dest, ECX source, 0x54 bytes

// 0x28 bytes at +0x28 inside the 0x54-byte record 0x000FCD70 copies.
// cam_72cb0 compares slot 0 (ECX) with slot 2 (EAX) when the slot index is 2.
struct EntityMatchBlock
{
    u8 mark = 0; // +0. Both sides must be 0.
    u8 pad[3]{};
    float value[9]{}; // +4 through +0x24
};

// ECX is left, EAX is right. Returns 1 when both marks are 0 and every
// left - right float sits in [-0.01, 0.01].
i32 fn_10ae90(const EntityMatchBlock& left, const EntityMatchBlock& right); // 0x0010AE90

void entity_body_camera_sizeof(u32 out_sizes[2]); // 0x00073170
void entity_body_camera_tick(Entity& body);       // 0x000731F0
void entity_body_camera_shutdown(Entity& body);   // 0x00073300

// Slot is the stack argument. name is EBX: the saved name, or the first
// record's name at +0x34. Returns 1 when a list record matches.
i32 cam_func_48cf0(i32 slot, const char* name); // 0x00048CF0

// EAX is the camera record at body+0x894.
void fn_48c70(EntityCamNode* node); // 0x00048C70
bool entity_body_camera_message(Entity& body, const Message& msg); // 0x00073320, id 99

// BanjoCam body passed to 0x00072BA0. Aim point is +0x2fc. +0x308 enables the look.
// Roll is the float at +0x8f4, copied into the actor euler's Z.
struct EntityLookAt
{
    Actor* actor = nullptr; // +8
    Vec3 aim{};              // +0x2fc
    u8 enabled = 0;          // +0x308
    float roll = 0.f;        // +0x8f4
};

// Absolute value. A non-negative input is returned unchanged.
float fn_152bb0(float value); // 0x00152BB0

// EAX is the direction (x, y, z). Writes yaw, then pitch. Returns 0 when XZ is
// quieter than 2e-7; pitch is still written.
i32 fn_105e20(float x, float y, float z, float& yaw, float& pitch); // 0x00105E20

void fn_72ba0(EntityLookAt& cam); // 0x00072BA0

// Channel keys on the camera (ESI for 0x0001D850, EAX for 0x0001EDE0).
// Two slots at +0xe4, stride 0x0c.
// value is the fade duration 0x0001D850 writes at +0. blend is the float at +4.
// mark is the byte at +8.
struct EntityChannelKey
{
    float value = 0.f;
    float blend = 0.f;
    u8 mark = 0;
};

struct EntityChannelAnim
{
    float timer = 0.f;     // +0x8c
    u8 mode = 0;           // +0x90
    u8 index = 0;          // +0x91
    u8 phase = 0;          // +0x92. 1 idles the stepper.
    float host_rate = 0.f; // camera+8 +0x92c
    float host_gate = 0.f; // camera+8 +0x930. Above 0 forces the rate to 0.
    EntityChannelKey keys[2]{};
};

// dt is DAT_005096C0. DAT_004312DC > 0 yields a zero step; otherwise dt * DAT_004312C4.
void fn_1d850(EntityChannelAnim& anim, float dt);

// EAX is the camera. 0x00072AC0 jumps here after 0x0001D850 when +0xfc is set.
void fn_1ede0(EntityChannelAnim& anim, float dt);

// Pair at camera+0x100 + bank*8. Ten banks occupy +0x100..+0x14c.
struct EntityChannelPair
{
    i32 id = 0;
    float value = 0.f;
};

// Row at camera+0x150, stride 0x0c. count is +0x154. bank index is +0x158.
struct EntityChannelRow
{
    i32 id = 0;
    float value = 0.f;  // +4
    float target = 0.f; // +8, filled from the selected bank then eased
};

struct EntityChannelEase
{
    std::vector<EntityChannelPair> banks[10]{};
    i32 bank = 0; // +0x158. Writers store 0, 4, 6, 7, 8, and 9.
    std::vector<EntityChannelRow> rows;
    float host_rate = 0.f; // camera+8 +0x92c
    float host_gate = 0.f; // camera+8 +0x930. Above 0 forces the rate to 0.
};

// EDX is the camera. 0x00072AC0 calls this when +0x154 > 0.
void fn_1ea90(EntityChannelEase& cam, float dt);

// Header at host+0x980 (the object at camera+8). Stride is the element size in bytes.
// The element is data + index * stride, and its first u16 is the code compared with 0x15.
// When end equals mark, retail loads that u16 from address 0 and faults.
struct EntityMaskSpan
{
    u16 stride = 0; // +0
    u16 mark = 0;   // +2
    u16 index = 0;  // +4
    u16 end = 0;    // +6
    const u8* data = nullptr; // +8
};

// Object at camera+8 read by 0x0001FE00. cam_72cb0 pushes the mask into 0x001226C0.
struct EntityMaskHost
{
    i32 flag = 0; // +0x93c. Nonzero clears bit 0x4000, the same mask as mode 2.
    EntityMaskSpan* span = nullptr; // +0x980
    i16 mode = 0; // +0xcdc
};

// EAX is the camera. Returns 0xC4000, or that mask with bit 0x4000, 0x40000, or 0x80000 cleared.
u32 fn_1fe00(const EntityMaskHost& host);

// Focal is the caller float (camera +0x24, or the display record +0x34). -1 selects the table.
// aspect_slot is EDI (camera +0x30). fit_h / fit_y are the Xbox ECX / EAX outs from 0x000F5760.
// fit_w / fit_x are the 0x000F56C0 outs. fov_deg, pixel_aspect, and focal_px are the three floats.
void fn_f57f0(float focal, i32 aspect_slot, i32& fit_w, i32& fit_x, i32& fit_h, i32& fit_y, float& fov_deg,
              float& pixel_aspect, float& focal_px);

constexpr i32 kMsgCameraTranslate = 99;

// Shape node passed to 0x00055AA0 and 0x00056720. The bone id is the u16 at +8.
// Kind 1 (cylinder) adds +0x10 and +0x14 to the local point at +0x18.
// Kind 0 (sphere) stores the local point in the three floats at +0x14, and
// adds +0x10 to local x. Both samples go through 0x00056790.
struct EntityShapeNode
{
    u8 kind = 0; // +0. 0 sphere, 1 cylinder.
    u8 next = 0; // +1
    u16 pad_2 = 0;
    u32 mask = 0; // +4
    u16 bone = 0xFFFF; // +8. 0xFFFF skips the skeleton.
    u16 pad_a = 0;
    u32 pad_c = 0; // +0xC
    float along_x = 0.f; // +0x10. Added to local x for both kinds.
    float along_y = 0.f; // +0x14. Cylinder half-extent. Sphere local-point x.
    Vec3 point{};        // +0x18. Cylinder local point. Sphere uses y and z here.
};

// Pose passed as the third argument. packed is the dword at +4.
// Non-zero: bone matrices are 3x4, stride 0x30, stored from +0x10.
// Zero: bone matrices are 4x4, stride 0x40, and 0x000315F0 transposes
// the first three columns into that multiply. mats holds the floats at +0x10.
struct EntityBonePose
{
    u32 tag = 0;
    u32 packed = 0;
    std::vector<float> mats;
};

// ESI for 0x00055AA0. 0x20 bytes. Y is the bottom; height is the full extent.
struct EntityCylinderBound
{
    u8 kind = 0; // 4
    u8 size = 0; // 0x20
    u8 flag = 0; // 1
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float pad0 = 0.f; // +0x10
    float height = 0.f; // +0x14
    float pad1 = 0.f; // +0x18
    float radius = 0.f; // +0x1C
};

// ESI is the record. world is the actor matrix (null skips it). pose is the
// skeleton (null, or bone 0xFFFF, skips it).
void fn_55aa0(const EntityShapeNode& node, const float* world, const EntityBonePose* pose,
              EntityCylinderBound& out);

// EDX is the shape node, ECX the pose, the stack argument the actor matrix, and
// ESI the record. Kind 0: the sphere from 0x00056330 is stored as a cylinder
// whose bottom is center.y minus the radius and whose height is twice the radius.
void fn_56720(const EntityShapeNode& node, const float* world, const EntityBonePose* pose,
              EntityCylinderBound& out);

// EAX is the source point. The actor is the first stack argument (null leaves the
// query null). shapes/count are the cylinder array 0x001202C0 merges, and push_out
// receives the sum of each hit. Returns 1 when any probe on DAT_00511B04 hits.
i32 fn_120480(const Vec3& src, const Actor* actor, EntityCylinderBound* shapes, i32 count,
              Vec3& push_out);

// ESI is the source point, EDI the destination. EDX is the actor 0x00120480
// receives (null skips the origin subtract). XMM1 is the cylinder height and
// the stack float is the radius. Bytes are 4, 0x20, 1.
void fn_1203e0(const Vec3& src, Vec3& dest, const Actor* actor, float height, float radius);

// EAX is the point. actor is the first stack argument (null skips the radius
// bump). out is the cylinder: kind 4, size 0x20, flag 1. Y is the nav-cell
// bottom, or the point Y plus 5 when that cell is missing.
void fn_13d2c0(const Vec3& point, const Actor* actor, EntityCylinderBound& out);

// EBX is the query cylinder and EDI the obstacle. The offset is added to the
// query position. push is the horizontal correction (Y stays 0). Returns 1 when
// the vertical slabs overlap and the XZ gap is inside the summed radius.
i32 fn_120760(const EntityCylinderBound& query, const EntityCylinderBound& obstacle, const Vec3& offset,
              Vec3& push);

// EBX is the point replaced when the candidate sits within 2e-7. The first stack
// argument is that candidate. param, kind, and mask are the next three dwords.
// Camera callers pass the bits of the float at DAT_004274B8, kind 4, and the
// mask from 0x0001FE00. Returns 1 when a node on DAT_00511AF4 accepts the
// segment. A near miss copies the candidate onto EBX and returns 0.
i32 fn_1226c0(Vec3& kept, Vec3& candidate, u32 param, i32 kind, u32 mask);

// Clears the text-page cursor and the markup gates, builds the 92 wide token
// names (0x00056DF0), resets the sprite slots (0x00057400), and drops the
// loaded icon. DAT_004e60c0 becomes IconMIN (0x36).
void fn_57d40();

// cam_11c480 calls this once the script tables are clear. Zeros the 40
// light-volume slots, allocates the 16x16 grid, and plants the default
// colour, range, and cone angles.
void fn_fd620();

// cam_11c480 calls this after the light-volume reset. Plants the wind, rain,
// and random-ambience globals, then samples the first delays from
// aid_misc_ghoulies_audio_generalsettings.
void fn_118470();

// cam_11c480 calls this after the text-page reset. Creates the 4096x1
// A8R8G8B8 lookup, fills each texel from its index nibbles, swizzles that
// row, and clears the eight records at DAT_0050eaa0 plus the count.
void fn_119160();

// cam_11c480's first call. Zeros the intrusive heads from DAT_00511AA0 through
// DAT_00511B58: the latch list, eight bins, the segment and shape probes, and
// the body lists. Linked nodes are dropped from those heads.
void fn_1233c0();

// cam_11c480 calls this after 0x001233C0. Releases every aid record on
// DAT_00510574 through 0x000F4EE0, then zeros both list heads and the
// body count at DAT_00510578.
void fn_11b630();

// cam_11c480 calls this after 0x00119160. EAX is the aid name
// (DAT_00430cf8). When DAT_00511b68 and DAT_00511b64 are both set, the
// callback releases that hold. A first dword other than 'aid_' clears
// the pointer and installs 0x00035940. Otherwise 0x00035E90 writes the
// resolved name, and a null result calls 0x00032700 with 2.
void fn_1234b0(const char* name);

// cam_11c480 calls this after 0x001234B0. Zeros the flag bytes held by
// DAT_00506c74, the six counters at DAT_00506c78, and the word at
// DAT_00506c90. 0x0011EB30 then copies DAT_00511b64 into the 128-byte
// name at DAT_00510ed8 and clears the script slot at DAT_00430d08.
void fn_11ead0();

// cam_11c480 calls this after 0x0011EAD0. Allocates DAT_005462e4 (0x18):
// an empty stride-0x18 pool at +0, an empty stride-0x100 pool at +8, and
// zeros at +4, +0xc, +0x10, and the float at +0x14. 0x0010F1C0 then
// copies each U:\ directory's SaveMeta.xbx Name into that wide pool.
void fn_10d3b0();

// cam_11c480 calls this after 0x0011B630. Each of the ten records at
// DAT_0051254c (stride 0x90) stores 0xFFFF in the id at +0x8c. The cap
// at DAT_00512520 becomes 0x14 and the cap at DAT_00512534 becomes 6.
// The lists inside those two stride-0x14 bins, the names in the records,
// and the count at DAT_00512548 stay as they are.
void fn_129f30();

// cam_11c480 calls this after 0x00129F30. Each of the twenty slots at
// DAT_00512140 (stride 0x18) drops its voice key through 0x001319C0 when
// DAT_00545abc is set, then the slot is cleared. The four spawn pointers
// at DAT_00512324 are zeroed for the count at DAT_00512320, and that
// count becomes 0. The parallel times at DAT_00512334 stay as they are.
void fn_123b30();

} // namespace gbtg
