#pragma once

#include "gbtg/game.h"
#include "gbtg/entity.h"

#include <array>
#include <bit>
#include <cstddef>
#include <vector>

namespace gbtg
{
namespace scene_detail
{
struct CameraWash;
struct CamPairObject;
struct CameraPairs;
struct CamChannel;
struct CamBindRow;
struct CameraChannels;
struct CameraPulse;
struct PulseNode;
struct TimerNote;
struct CameraTimers;
struct Mat34;
struct CamShapeNode;
struct CamSlotEntry;
struct CamSlotHeader;
struct CamBonePose;
struct CamAttach;
struct CamLink;
struct CamMarker;
struct CamMarkerBank;
struct CamSlotObject;
struct CamBoundRec;
struct CamBound;
struct CamLocator;
struct CamGateBlock;
struct CamGate;
struct CamFollow;
struct CamBlend;
struct CamBlendRef;
struct CamBind;
struct CamPicker;
struct RetireNode;
struct CamFocus;
struct CamAnchor;
struct ActorMarkWord;
struct ActorMark;
struct RangeAt28;
struct RangeLink;
struct BodyLink;
struct AltSlot;
struct AltAt18;
struct AltLink;
}
struct ScriptCond;
struct LiveScript;
struct SceneFlagBlock;
struct SceneSpanRecord;
struct SceneSpan;
struct TransitionSlot31478;
using namespace scene_detail;

namespace scene_detail
{
// Body-camera block at +0x94. camera_apply (0x00025AE0) ticks it once a frame.
// The object at camera+8 is the host read by 0x0001E770; those fields stay clear
// until that object exists, which keeps the wash idle.
struct CameraWash
{
    u16 sample = 0;      // +0x94
    u16 sample_max = 0;  // +0x96
    u8 state = 0;        // +0xA4
    u8 level = 0;        // +0xA5, 0..128
    u8 enabled = 0;      // +0xA6
    u8 latch = 0;        // +0xA7
    u8 blink = 0;        // +0xA8
    u8 blink2 = 0;       // +0xA9
    u8 blink2_on = 0;    // +0xAA
    float blink_time = 0.f;   // +0xAC
    float blink2_time = 0.f;  // +0xB0
    float hold = 0.f;         // +0xB4
    float fade = 0.f;         // +0xB8
    float wave = 0.f;         // +0xBC
    float wave_base = 0.f;    // +0xC0
    float wave_mag = 0.f;     // +0xC4
    float wave_phase = 0.f;   // +0xC8
    float wave_span = 0.f;    // +0xCC
    float wave_timer = 0.f;   // +0xD0
    float wave_rate = 0.f;    // +0xD4
    i32 host_block = 0;       // host+0x98C
    float host_push = 0.f;    // host+0x978
    u8 host_flag = 0;         // host+0x969
    bool host_link = false;   // host+0x964
    bool host_link_live = false; // [host+0x964]+0xB8
};

// camera_apply (0x00025AE0) calls 0x00025130 after the wash. camera+0x2d0
// gates it. camera+0x2d1 selects the bit pair and camera+0x2d2 is the value
// passed in AL. Host is camera+8. 0x000F96F0 stores that pair at +0x294 on
// the objects at host+0x204 and host+0x310, and caches the value per index
// in DAT_005078D8. Both objects stay null until that host is filled in.
struct CamPairObject
{
    u8 pairs[64] = {}; // +0x294, two bits per index
};

struct CameraPairs
{
    u8 enabled = 0; // +0x2d0
    u8 index = 0;   // +0x2d1
    u8 value = 0;   // +0x2d2
    CamPairObject* at_204 = nullptr;
    CamPairObject* at_310 = nullptr;
};

// Body-camera channels at +0x150 (count +0x154). camera_apply (0x00025AE0)
// calls 0x0001EEC0 when gate (+0xfc) is set, then 0x0001EC20 when the count
// is greater than 0. Host is camera+8: +0x50 is the bind list walked by
// 0x00030B40, +0x204 is the slot table (float* at +0x1cc, indexed by channel id).
// +0xe8 and +0xf4 are the two blend floats written into slot 0 by 0x0001EEC0.
struct CamChannel
{
    i32 id = 0;        // +0
    float value = 0.f; // +4
};

struct CamBindRow
{
    bool live = false;      // +0 -> +0 -> +0x20 all present
    std::vector<i32> words; // dword array at +0x20, indexed by channel id
};

struct CameraChannels
{
    std::vector<CamChannel> items;
    float blend[2] = {}; // +0xe8 and +0xf4
    u8 mark[2] = {};     // +0xec and +0xf8
    i32 gate = 0;        // +0xfc
    bool binds_live = false;
    std::vector<CamBindRow> binds;
    bool slots_live = false;
    std::vector<float*> slots;
};

struct CameraPulse
{
    i32 marker = 0;        // +0x164
    float primary = 0.f;   // +0x168
    float blend = 0.f;     // +0x16c
    float secondary = 0.f; // +0x174
    float latch = 0.f;     // +0x25c
    u8 counts[6] = {};     // +0x260 .. +0x265
    float host_rate = 0.f; // host+0x92c
    float host_gate = 0.f; // host+0x930
    void* host = nullptr;  // camera+8, payload of note 0x47
};

// DAT_00511AA4. The node sits at object+0x900; next is object+0x904.
// vtable+0xC receives (object, message). Note 0x47 passes {id, host}.
// 0x00122CA0 and 0x00124F30 pass a pointer to the message id.
struct PulseNode
{
    void (*handle)(PulseNode* self, i32 id, void* host) = nullptr;
    PulseNode* next = nullptr;
    u16 tag_6 = 0; // object+6
    u16 word_1fc = 0; // object+0x1FC
    void (*on_message)(PulseNode* self, const i32* msg) = nullptr; // vtable +0xC
};

// camera_apply (0x00025AE0) calls 0x00027960 with ESI = camera, after 0x0001FB40.
// The step is the same host rate as the pulse (camera+8: +0x930 gates, else +0x92c)
// times DAT_005096C0 * DAT_004312C4. +0x75c only clamps at 0. +0x758 runs while
// +0x760 is set and, on the frame it hits 0, calls that object's vtable+0xC with
// {5, 0}. +0x7e8 runs when +0x7e4 is set and +0x7e5 is clear. Hitting 0 sends
// that same note to +0x7f0 (0x00013B240), then 0x0001F2F0(0x3f) drops every
// pulse latch and may broadcast note 0x47, then clears +0x7e4.
struct TimerNote
{
    void (*on_note)(TimerNote* self, i32 id, i32 param) = nullptr; // vtable +0xC
};

struct CameraTimers
{
    float at_758 = 0.f;
    float at_75c = 0.f;
    TimerNote* at_760 = nullptr;
    u8 at_7e4 = 0;
    u8 at_7e5 = 0;
    float at_7e8 = 0.f;
    TimerNote* at_7f0 = nullptr;
};

// Focus cylinder built by 0x000426F0. camera_apply (0x00025AE0) reaches it
// through 0x00025C50 once the slot header at host+0x204 is live. Host is
// camera+8. +0x2ac is the actor matrix. +0x8fc is the scratch record list.
// DAT_0050DD80 is a BSS probe; the retail image leaves it at the origin, so an
// attached read is the matrix translation.
struct Mat34
{
    float m[12] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};
};

struct CamShapeNode
{
    u8 kind = 0; // 0 sphere (local point +0x14), 1 cylinder (local point +0x18)
    u32 mask = 0; // +4, tested against the caller's mask
    u16 bone = 0xFFFF; // +8, 0xFFFF skips the skeleton
    Vec3 point{};
    float radius_x = 0.f; // cylinder node+0x10, added to local x
    float half_y = 0.f;   // cylinder node+0x14, added to local y
    CamShapeNode* next = nullptr;
};

struct CamSlotEntry
{
    i32 type = 0; // 4 shape list, 5 default scales, 9 bone byte
    CamShapeNode* shape = nullptr; // type 4, first node (blob+8)
    bool scale_live = false;       // type 5 pointer present
    float height_scale = 0.f;      // +0x20
    float radius_scale = 0.f;      // +0x24
    const u8* bone_byte = nullptr; // type 9, second dword; byte 0 is the id
};

struct CamSlotHeader
{
    std::vector<CamSlotEntry> entries;
};

struct CamBonePose
{
    u16 bone = 0xFFFF; // +8, bone id when this object is the radius source
    float along_x = 0.f; // +0x10, sphere radius offset along local x
    bool table_live = false; // +4
    std::vector<Mat34> bones; // +0x10, stride 0x30, indexed by the shape bone
};

struct CamAttach
{
    bool child_live = false; // +8
};

struct CamLink
{
    bool live = false;
};

// [slot+0x1bc]+0x24. Each record is 0x20 bytes; the translation sits at +0x10.
struct CamMarker
{
    Vec3 point{};
};

struct CamMarkerBank
{
    std::vector<CamMarker> items;
};

struct CamSlotObject
{
    CamSlotHeader* header = nullptr; // +0
    CamBonePose* at_164 = nullptr;
    CamBonePose* at_168 = nullptr;
    CamMarkerBank* markers = nullptr; // +0x1bc
};

struct CamBoundRec
{
    u8 kind = 0; // 1 sphere, 4 cylinder
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float radius = 0.f; // +0x10
    float height = 0.f; // +0x14
    float reach = 0.f;  // +0x1c, cylinder radius
};

struct CamBound
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
    float radius = 0.f; // +0x1c
};

// Locator record (stride 0x34, word 0 == 3). Mode 0 reads the bone at +2 and
// the position stored at +0x10 / +0x20 / +0x30.
struct CamLocator
{
    i16 bone = -1;
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

// camera+0x15c. +8 points at a byte block; +7 bit 0 keeps the stored follow cylinder.
struct CamGateBlock
{
    u8 bytes[8] = {};
};

struct CamGate
{
    CamGateBlock* at_8 = nullptr;
};

// host+0x8f8. Two cylinders (0x20 each) and the point 0x00025C50 posts through 0x0012B160.
struct CamFollow
{
    u8 mark = 0; // +0x19
    CamBound at_1c{}; // +0x1c
    CamBound at_3c{}; // +0x3c
    Vec3 at_5c{}; // +0x5c
};

// [[bind+0x58]+0]. +0x14 is the sample, +0x10c the other end, +0x1c the direction.
// +0x2a == 1 arms the end-of-travel note.
struct CamBlend
{
    float value = 0.f;    // +0x14
    float dir = 0.f;      // +0x1c
    u8 mark = 0;          // +0x2a
    float previous = 0.f; // +0x10c
};

struct CamBlendRef
{
    CamBlend* blend = nullptr;
};

// host+0x50. A null +0x58 faults on Xbox when the bind itself is live.
struct CamBind
{
    CamBlendRef* at_58 = nullptr;
};

// Bytes at picker+0x11 and picker+0x13.
struct CamPicker
{
    u8 count = 0;    // +0x11
    u8 selected = 0; // +0x13
};

// Node embedded at object+0x908. prev non-null means the object is already linked.
struct RetireNode
{
    RetireNode* prev = nullptr;
    RetireNode* next = nullptr;
};

struct CamFocus
{
    Vec3 position{}; // +0x08
    float extent_30 = 0.f; // +0x30
    float at_34 = 0.f; // +0x34, height factor of the follow cylinder
    float scale_38 = 0.f; // +0x38
    Vec3 at_3c{}; // +0x3c, copied to the follow packet +0x5c
    CamBind* bind = nullptr; // +0x50
    CamAttach* attach = nullptr; // +0x88
    float scale_200 = 0.f; // +0x200
    CamSlotObject* slots = nullptr; // +0x204
    CamPairObject* pair_204 = nullptr; // +0x204, byte pairs at +0x294
    CamPairObject* pair_310 = nullptr; // +0x310
    CamAnchor* anchors = nullptr; // +0x1f8, next link of the header at +0x1f4
    Vec3 ease{}; // +0x318
    Vec3 ease_to{}; // +0x324
    Mat34 world{}; // +0x2ac
    CamFollow* follow = nullptr; // +0x8f8
    RetireNode retire{}; // +0x908
    CamFocus* release_next = nullptr; // +0x91c
    CamFocus* release_child = nullptr; // +0x920
    float accum = 0.f; // +0x924
    i32 accum_n = 0; // +0x928
    float host_rate = 0.f; // +0x92c
    float host_gate = 0.f; // +0x930
    i32 pulse_on = 0; // +0x934
    i32 pulse_left = 0; // +0x938
    float pulse = 0.f; // +0x93c
    CamPicker* picker = nullptr; // +0x940
    u32 marker = 0; // +0x944
    u32 in_range = 0; // +0x948
    float marker_kick = 0.f; // +0x94c
    float range = 0.f; // +0x950
    CamLink* link = nullptr; // +0x918
    std::vector<CamBoundRec> recs; // +0x8fc
    u32 at_cec = 0; // +0xcec, low byte stored at packet +0x19
    u16 tag_6 = 0; // +6
    // +0x980. DAT_00511B38 is the link at object+0x934, so this is node+0x4c.
    EntityMaskSpan* span_980 = nullptr;
    i32 at_984 = 0; // +0x984, node+0x50
    i32 at_98c = 0; // +0x98c, node+0x58
    u16 word_1fc = 0; // +0x1FC, compared with the dword at node+0x48
    u16 word_cdc = 0; // +0xcdc, link+0x3a8, compared with 2
    Vec3 nudge_704{}; // camera+0x704, added to the stored follow position
    float mul_884 = 0.f; // camera+0x884
    float mul_888 = 0.f; // camera+0x888
    u32 mask_890 = 0; // camera+0x890, shape mask for the averaged bound
    CamGate* gate_15c = nullptr; // camera+0x15c
    u32 shape_mask = 0; // camera+0x88c, the mask 0x00025C50 passes with flag 0
    CamBound bound{};
    void (*on_note)(CamFocus* self, i32 id) = nullptr; // vtable +0xC
};

// Node allocated by 0x0011C0B0 (0x38 bytes) and linked at host+0x1f8.
// +0x14 selects the update. +0x18 is a locator pointer in mode 0 and a bone
// index in modes 1 and 2. +0x1c is the local point. +0x28 is the world point.
struct CamAnchor
{
    CamAnchor* next = nullptr; // +4
    CamFocus* host = nullptr;  // +0x10
    i32 mode = 0;              // +0x14
    CamLocator* locator = nullptr;
    u16 index = 0xFFFF; // +0x18
    Vec3 local{};       // +0x1c
    Vec3 out{};         // +0x28
};

// DAT_00511B40. The stored address is object+0x93C. Null, and the retail
// sentinel 0x93C, both leave the marker clear. The live byte is
// [[[actor+0x95C]+0x15C]+8]+5; bit 4 is the marker 0x000700B2 reads.
// 0x00132840 reads the word at [actor+0x95C]+0x94, which is node+0x20.
struct ActorMarkWord
{
    u16 word_94 = 0; // +0x94
    i32 int_258 = 0; // +0x258. 0x00140E30 requires this int to be 0.
};

struct ActorMark
{
    u8 byte = 0;
    Actor* actor = nullptr; // node - 0x93C. ESI for 0x000412A0.
    ActorMarkWord* at_20 = nullptr; // node+0x20, actor+0x95C
};

// DAT_00511B38. Each node is an object+0x934 link; next is at +4.
// Object at node+0x28 (object+0x95C). The word at +0x94 is the energy
// 0x00132DB0 tests.
struct RangeAt28
{
    u16 word_94 = 0; // +0x94
};

struct RangeLink
{
    RangeLink* next = nullptr;
    CamFocus* host = nullptr;
    void (*on_note)(CamFocus* self, i32 id) = nullptr;
    RangeAt28* at_28 = nullptr; // node+0x28
};

// DAT_00511B20. First node of the DAT_00511B1C list. The node is object+0x934
// and the next pointer is at +4. link-0x730 is the pair at object+0x204.
// link-0x624 is the pair at object+0x310.
struct BodyLink
{
    BodyLink* next = nullptr;
    CamPairObject* pair_204 = nullptr;
    CamPairObject* pair_310 = nullptr;
};

// DAT_00511B48. Next of the DAT_00511B44 head. The node is object+0x944 and
// the next pointer is at +4. object+6 is the actor class key. The byte at
// object+0x969 is node+0x25. The word at object+0x990 is node+0x4c. The
// pointer at object+0x958 is node+0x14. The pointer at object+0x95c is
// node+0x18. The word at that object's +0x94 is the count 0x00132680 reads.
struct AltSlot
{
    void* target = nullptr; // +8, vtable object
    void* flag_10 = nullptr; // +0x10
    u32 at_6c = 0; // +0x6c. Handler 15 type 0x8 stores the dword at node+0x48.
    void (*on_message)(void* self, const u32* msg) = nullptr; // vtable +0xC
};

// Object at node+0x18 (actor+0x95c).
struct AltAt18
{
    u16 word_94 = 0; // +0x94
};

struct AltLink
{
    AltLink* next = nullptr;
    Actor* actor = nullptr;
    u8 byte_969 = 0;
    u16 word_990 = 0;
    AltSlot* slot = nullptr;
    AltAt18* at_18 = nullptr; // node+0x18
    void (*on_message)(Actor* self, const u32* msg) = nullptr; // object vtable +0xC
};
}

// Condition opcodes are the index into the 27-entry table at 0x00314258.
// The paired size is what 0x00124C80 copies into the stride-0x20 record.
struct ScriptCond
{
    u8 raw[0x20]{};
};

inline constexpr u32 kScriptCondSize[27] = {
    0x10, 0x10, 0x08, 0x10, 0x18, 0x14, 0x10, 0x0C, 0x0C, 0x0C, 0x14, 0x14, 0x10, 0x10,
    0x10, 0x14, 0x10, 0x1C, 0x10, 0x20, 0x08, 0x08, 0x08, 0x10, 0x10, 0x10, 0x0C,
};

// DAT_0050E4DC. 0x00116810 allocates the 0x214-byte node. +4 is next. +8 is
// the type 0x00128A00 compares with 0x7D before it calls the handlers.
// DAT_0050E4D8 is the anchor immediately before that head. 0x00116810 stores
// the anchor as the first node's prev, so 0x000E9210's store at prev+4 writes
// the new head.
struct LiveScript
{
    LiveScript* prev = nullptr;
    LiveScript* next = nullptr;
    u32 type = 0;
    // Xbox +0xC. 0x00132720 compares this word with the word at record+8.
    u16 word_c = 0;
    // Xbox +0x10. 0x00132D00 reads this dword: mode 1 copies it, mode 2
    // stores whether it is zero. The type dword is at +8, so this is the
    // dword at that address +8.
    u32 dword_10 = 0;
    // Xbox +0x14. 0x001168F0 compares this word with DX. The type dword is
    // at +8, so the compare is the word at that address +0xC.
    u16 word_14 = 0;
    std::array<u8, 0x200> message{};
    float timestamp = 0.f; // Xbox +0x210
};

// DAT_00506c74. +8 is the flag-byte pointer. 0x00132ED0 tests bit 0x8 of the
// first byte. The payload is the two bytes 0x000F2B60 publishes.
struct SceneFlagBlock
{
    i32 kind = 0; // +0
    i32 size = 0; // +4
    u8* bytes = nullptr; // +8
};

// DAT_0050E0E8. The global holds the span. +0 is the stride word and +2 is
// the word beside it. +4 is the first record and +8 is the end. Each record
// is 0x50 bytes. +0 of a record is the key the walk matches.
// +0x48 selects the refresh. +0x4C stores the frame from DAT_004DEA58.
struct SceneSpanRecord
{
    void* key = nullptr;
    u8 gap[0x48 - sizeof(void*)]{};
    i32 selector = 0;
    u32 frame = 0;
};

struct SceneSpan
{
    u16 stride = 0;
    u16 word_2 = 0;
    SceneSpanRecord* begin = nullptr;
    SceneSpanRecord* end = nullptr;
};

// 'aid_' as a little-endian dword. 0x0011EB30 clears the name when it matches.
inline constexpr u32 kAidPrefixEd8 = 0x5f646961u;

// Image at DAT_00431478. Stride 0x110. +0 indexes the 0x18-byte records at
// DAT_003129AC. +4 is the aid string 0x0012C5D0 compares with 'aid_'.
// +0x104 is the dword copied into DAT_00546330. +0x10C is the gate
// 0x0012C340 tests. The eighteenth record is the sentinel whose index is -1.
struct TransitionSlot31478
{
    i32 handler = 0; // +0
    char aid[0x80]{}; // +4
    char closing_aid[0x80]{}; // +0x84
    u32 at_104 = 0; // +0x104
    i32 at_108 = 0; // +0x108
    i32 at_10c = 0; // +0x10C
};

inline constexpr TransitionSlot31478 kTransition31478[] = {
    {0, "", "", 0x00000000u, 0, 1},
    {1, "", "", 0x00000000u, 0, 0},
    {1, "", "", 0x00000000u, 1, 0},
    {1, "aid_texture_ghoulies_transition_chaptertitles_chapter1", "", 0x00000000u, 2, 0},
    {1, "aid_texture_ghoulies_transition_chaptertitles_chapter2", "", 0x00000000u, 2, 0},
    {1, "aid_texture_ghoulies_transition_chaptertitles_chapter3", "", 0x00000000u, 2, 0},
    {1, "aid_texture_ghoulies_transition_chaptertitles_chapter4", "", 0x00000000u, 2, 0},
    {1, "aid_texture_ghoulies_transition_chaptertitles_chapter5", "", 0x00000000u, 2, 0},
    {1, "", "", 0x00000000u, 3, 0},
    {1, "", "", 0x00000000u, 4, 0},
    {1, "", "", 0x00000000u, 5, 0},
    {1, "", "", 0x00000000u, 6, 0},
    {1, "", "", 0x00000000u, 7, 0},
    {1, "aid_texture_ghoulies_unlockableart_whereitbegan", "", 0x00000000u, 13, 0},
    {1, "aid_texture_ghoulies_unlockableart_whereitbegan", "", 0x00000000u, 14, 0},
    {2, "aid_texture_ghoulies_transition_blood_down", "aid_texture_ghoulies_transition_blood_down", 0x3fc00000u, -1, 1},
    {3, "", "", 0x3e4ccccdu, 0, 1},
    {-1, "", "", 0x00000000u, 0, 0},
};

} // namespace gbtg
