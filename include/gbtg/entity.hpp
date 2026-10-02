#pragma once

#include "gbtg/types.hpp"

#include <string>
#include <vector>

namespace gbtg
{

// Rare entity split recovered from XBE type names: Body / Mind / Strategy.
// Types are C vtables filled by static ctors that call entity_type_fill_defaults (0x0001DA60)
// then overwrite slots. entityBodyCamera_ctor (0x00073190) is the first reconstructed example.

enum class EntityKind
{
    Body,
    Mind,
    Strategy,
};

struct Actor;

// Record at actor+0x88. +8 is the parent. +0x3c is the 3x4 that 0x00041730 applies.
struct ActorBind
{
    Actor* parent = nullptr; // +8. Non-null keeps the bind.
    float m[12] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f}; // +0x3c
};

// Node pointed at by actor+0x918. The first dword keeps a bind whose parent is null.
struct ActorLink
{
    void* head = nullptr; // +0
};

// Object at actor+0x95c. +8 is the actor 0x00041820 reads. +0x72c receives that world euler.
struct ActorEulerOut
{
    Actor* actor = nullptr; // +8
    Vec3 euler{};            // +0x72c
};

struct Actor
{
    Vec3 position{}; // Xbox actor+0x08
    Vec3 euler{};    // Xbox actor+0x14 when the object stores euler (0x00041820 reads three floats)
    Quat quat{0.f, 0.f, 0.f, 1.f}; // cutscene camera writes xyzw at +0x14 (0x0004D8CE)
    float fov = 0.9f;              // cutscene camera +0x24
    Actor* attach = nullptr;
    ActorBind* bind = nullptr; // Xbox actor+0x88
    i32 xform_valid = 0;       // Xbox actor+0x2a8. 0 means the matrix at +0x2ac is stale.
    ActorLink* link = nullptr; // Xbox actor+0x918
    ActorEulerOut* euler_out = nullptr; // Xbox actor+0x95c
};

struct Entity;
struct Message
{
    i32 id = 0;
    Vec3 vec{};
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
    bool listed = false;
};

const EntityType* find_entity_type(std::string_view name);
std::vector<const EntityType*> all_entity_types();
void register_entity_types();

Vec3 actor_world_position(const Actor& actor); // 0x000412A0
Vec3 actor_world_euler(const Actor& actor);    // 0x00041820

// 3x4 row-major (12 floats). 0x00041820 passes the block at bind+0xc.
// Returns pitch, yaw, roll in xyz. EBX is that out-vector on Xbox.
Vec3 fn_104040(const float m[12]); // 0x00104040
void fn_41730(Actor& actor, const Vec3& point); // 0x00041730 EDI actor, ESI point
void fn_663e0(void* block);                    // 0x000663E0 EAX = block
void fn_fcd70(void* dest, const void* src);    // 0x000FCD70 EAX dest, ECX source, 0x54 bytes

void entity_body_camera_sizeof(u32 out_sizes[2]); // 0x00073170
void entity_body_camera_tick(Entity& body);       // 0x000731F0
void entity_body_camera_shutdown(Entity& body);   // 0x00073300
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

// Focal is the caller float (camera +0x24, or the display record +0x34). -1 selects the table.
// aspect_slot is EDI (camera +0x30). fit_h / fit_y are the Xbox ECX / EAX outs from 0x000F5760.
// fit_w / fit_x are the 0x000F56C0 outs. fov_deg, pixel_aspect, and focal_px are the three floats.
void fn_f57f0(float focal, i32 aspect_slot, i32& fit_w, i32& fit_x, i32& fit_h, i32& fit_y, float& fov_deg,
              float& pixel_aspect, float& focal_px);

constexpr i32 kMsgCameraTranslate = 99;

} // namespace gbtg
