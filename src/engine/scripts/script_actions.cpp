#include "../camera/entity_camera.h"
#include "../entities/mind_fields.h"
#include "../scene/chapter5_countdown.h"
#include "../scene/challenge_runtime.h"
#include "../camera/camera_mind_rig.h"
#include "gbtg/anim.h"
#include "gbtg/engine_internal.h"
#include "gbtg/entity.h"
#include "gbtg/gfx.h"
#include "gbtg/model.h"
#include "gbtg/movie.h"
#include "gbtg/resource.h"
#include "../transitions/scene_transitions.h"
#include "../entities/ghouly_requests.h"
#include "../scene/actor_loading.h"
#include "../scene/factory_scene_runtime.h"
#include "../scene/scene_controls.h"
#include "../audio/native_audio.h"
#include "../scene/executable_resources.h"
#include "../scene/scene_object_requests.h"
#include "../runtime/name_lookup.h"
#include "../entities/strategy_names.h"
#include "script_dialogue_actions.h"
#include "../scene/dialogue_scene_runtime.h"
#include "../scene/cutscene_scene_runtime.h"
#include "script_vm.h"
#include "../scene/ambient.h"
#include "../navigation/occupancy.h"
#include "../graphics/actor_effect_stream.h"
#include "../entities/actor_transform.h"
#include "../saves/runtime_profile.h"
#include "../entities/player_spawn.h"
#include "../entities/player_state.h"
#include "../entities/player_mind_messages.h"
#include "../entities/camera_director.h"
#include "../entities/actor_spawn_query.h"
#include "../entities/actor_scare_requests.h"
#include "../entities/entity.h"
#include "../scene/scene_actor_services.h"
#include "../scene/lighting.h"
#include "../graphics/light_cones.h"
#include "../scene/scene_markers.h"
#include "../frontend/dialogue_activation.h"
#include "script_conditions.h"
#include "../runtime/random_state.h"
#include "script_event_sequence.h"
#include "script_event_step_fields.h"
#include "script_event_tallies.h"
#include "../cutscenes/door_registration.h"
#include "../cutscenes/actor_tracks.h"
#include "gbtg/actor_spawn_factory.h"
#include "../navigation/grid_regions.h"
#include "../video/movie_startup.h"
#include "../video/movie_session.h"
#include "../video/native_movies.h"
#include "../entities/player_fields.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cerrno>
#include <charconv>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>

#if !defined(_WIN32)
#include <cfenv>
#endif

#if defined(_WIN32)
#include <excpt.h>
#include <float.h>
#include <malloc.h>
extern "C"
{
    __declspec(dllimport) void* __stdcall GetModuleHandleA(const char* name);
    __declspec(dllimport) void* __stdcall GetProcAddress(void* module, const char* name);
    __declspec(dllimport) unsigned long __stdcall GetCurrentThreadId();
    __declspec(dllimport) void __stdcall GetSystemTimeAsFileTime(void* time);
    __declspec(dllimport) unsigned long __stdcall GetTickCount();
    __declspec(dllimport) int __stdcall QueryPerformanceCounter(void* counter);
    __declspec(dllimport) void __stdcall RaiseException(unsigned long code, unsigned long flags, unsigned long count,
                                                        const std::uintptr_t* arguments);
    __declspec(dllimport) int __stdcall CloseHandle(void* handle);
    __declspec(dllimport) int __stdcall FlushFileBuffers(void* handle);
    __declspec(dllimport) unsigned long __stdcall GetLastError();
    __declspec(dllimport) int __stdcall ReadFile(void* handle, void* buffer, unsigned long bytes, unsigned long* read,
                                                 void* overlapped);
    __declspec(dllimport) int __stdcall WriteFile(void* handle, const void* buffer, unsigned long bytes,
                                                  unsigned long* written, void* overlapped);
    __declspec(dllimport) unsigned long __stdcall SetFilePointer(void* handle, long distance, long* high,
                                                                 unsigned long method);
    __declspec(dllimport) int __stdcall IsBadReadPtr(const void* lp, std::uintptr_t ucb);
    __declspec(dllimport) int __stdcall IsBadWritePtr(void* lp, std::uintptr_t ucb);
}
#endif


#include "../scene/scene_types.h"
#include "../camera/camera.h"
#include "../scene/scene_loading.h"
#include "script_actions.h"
#include "../entities/actor_lookup.h"
#include "script_conditions.h"

namespace gbtg
{
using namespace scene_detail;


// The C string at node + rel, read in place; one with no terminator before the resource ends raises.
std::string node_cstr(const SceneScript& script, const ScriptNode& node, u32 rel)
{
    const size_t begin = size_t(node.offset) + rel;
    for (size_t at = begin; at < script.bytes.size(); ++at)
    {
        if (script.bytes[at] == 0)
        {
            return std::string(reinterpret_cast<const char*>(script.bytes.data() + begin), at - begin);
        }
    }
    throw Error("script node string runs past its resource");
}


// Names at 0x0042A854. Count passed to 0x000FBFB0 is 0xE9.
const char* const kObjTagActor[0xE9] = {
    "objTag_Actor",
    "objTag_Actor_Baron",
    "objTag_Actor_BaronPlane",
    "objTag_Actor_Boy",
    "objTag_Actor_Butler",
    "objTag_Actor_Cameron",
    "objTag_Actor_Chicken",
    "objTag_Actor_Cook",
    "objTag_Actor_CookPlayable",
    "objTag_Actor_Doll",
    "objTag_Actor_Fiddler",
    "objTag_Actor_GhoulieGirl",
    "objTag_Actor_GiantHand",
    "objTag_Actor_Girl",
    "objTag_Actor_GrimReaper",
    "objTag_Actor_HauntedChair",
    "objTag_Actor_HauntedCoat",
    "objTag_Actor_HauntedDoor",
    "objTag_Actor_HauntedDoorRound",
    "objTag_Actor_HauntedPicture",
    "objTag_Actor_HauntedTV",
    "objTag_Actor_Hunchback",
    "objTag_Actor_Imp",
    "objTag_Actor_ImpChallenge",
    "objTag_Actor_ImpFire",
    "objTag_Actor_ImpFlying",
    "objTag_Actor_ImpNinja",
    "objTag_Actor_ImpNinjaBig",
    "objTag_Actor_ImpRoger",
    "objTag_Actor_JessieClyde",
    "objTag_Actor_Medusa",
    "objTag_Actor_MiniCooper",
    "objTag_Actor_MrsMop",
    "objTag_Actor_Mummy",
    "objTag_Actor_MummyCursed",
    "objTag_Actor_Pirate",
    "objTag_Actor_PirateLeader",
    "objTag_Actor_PrisonerChestChap",
    "objTag_Actor_PrisonerDoggyGirl",
    "objTag_Actor_PrisonerFilmReel",
    "objTag_Actor_PrisonerFridgeLady",
    "objTag_Actor_PrisonerFridgeLadyFat",
    "objTag_Actor_PrisonerLaundryLad",
    "objTag_Actor_PrisonerPlatterLad",
    "objTag_Actor_PrisonerSarco",
    "objTag_Actor_PrisonerSaunaGirl",
    "objTag_Actor_PrisonerSerpentYouth",
    "objTag_Actor_PrisonerThornGirl",
    "objTag_Actor_Ribs",
    "objTag_Actor_RibsFNPC",
    "objTag_Actor_RibsKey",
    "objTag_Actor_Skeleton",
    "objTag_Actor_SkeletonMaid",
    "objTag_Actor_Spider",
    "objTag_Actor_UglyGirl",
    "objTag_Actor_Vampire",
    "objTag_Actor_VampireChicken",
    "objTag_Actor_Warlock",
    "objTag_Actor_Worm",
    "objTag_Actor_Zombie",
    "objTag_Actor_zzz",
    "objTag_Avatar",
    "objTag_Avatar_DizzyBat",
    "objTag_Avatar_Fx_AnimEmitter",
    "objTag_Avatar_Fx_BurnUp",
    "objTag_Avatar_Fx_Explode",
    "objTag_Avatar_Fx_MotionBlur",
    "objTag_Avatar_HatstandBase",
    "objTag_Avatar_StandardLampBase",
    "objTag_Background",
    "objTag_BgBreakKey",
    "objTag_Body",
    "objTag_Body_Camera",
    "objTag_Body_Human",
    "objTag_Body_JessieClyde",
    "objTag_Body_Player",
    "objTag_Body_Vampire",
    "objTag_Body_Zombie",
    "objTag_Camera",
    "objTag_FxCustom_DustUp",
    "objTag_FxCustom_Electric",
    "objTag_FxCustom_FadeOut",
    "objTag_FxCustom_FadeToGrey",
    "objTag_FxCustom_Fire",
    "objTag_FxCustom_MutateHalo",
    "objTag_FxCustom_SolidFadeIn",
    "objTag_Mind",
    "objTag_Mind_Cameraman",
    "objTag_Mind_Cinema",
    "objTag_Mind_Human",
    "objTag_Mind_Player",
    "objTag_Misc_AnimDelete",
    "objTag_Misc_AnimatedLight",
    "objTag_Misc_ButtonBat",
    "objTag_Misc_ChickenEgg",
    "objTag_Misc_Furnace",
    "objTag_Misc_GhoulyKey",
    "objTag_Misc_KnockDownSphere",
    "objTag_Misc_MummyCursedSlabTop",
    "objTag_Misc_Prop",
    "objTag_Misc_SSSDoor",
    "objTag_Misc_SSSDropper",
    "objTag_Misc_Scare",
    "objTag_Misc_ScareFigure",
    "objTag_Misc_SpiderWeb",
    "objTag_Misc_SuperScaryShock",
    "objTag_Move",
    "objTag_Powerup",
    "objTag_SceneControl",
    "objTag_Spawn",
    "objTag_State",
    "objTag_Strategy",
    "objTag_Strategy_Director",
    "objTag_Strategy_HauntedCoat",
    "objTag_Strategy_HauntedDoor",
    "objTag_Strategy_Human",
    "objTag_Strategy_Player",
    "objTag_Weapon",
    "objTag_Weapon_Anchor",
    "objTag_Weapon_Ankh",
    "objTag_Weapon_Anvil",
    "objTag_Weapon_Bandage",
    "objTag_Weapon_BaronCane",
    "objTag_Weapon_Barrel",
    "objTag_Weapon_Bathtub",
    "objTag_Weapon_Bellows",
    "objTag_Weapon_Blunderbuss",
    "objTag_Weapon_Book",
    "objTag_Weapon_Bottle",
    "objTag_Weapon_BottleContainer",
    "objTag_Weapon_Broom",
    "objTag_Weapon_Burger",
    "objTag_Weapon_BurgerContainer",
    "objTag_Weapon_Cake",
    "objTag_Weapon_Candle",
    "objTag_Weapon_Candlestick",
    "objTag_Weapon_Chair",
    "objTag_Weapon_ChessBoard",
    "objTag_Weapon_ChestOfDrawers",
    "objTag_Weapon_ChristmasPudding",
    "objTag_Weapon_CoalScuttle",
    "objTag_Weapon_CookingPot",
    "objTag_Weapon_Crate",
    "objTag_Weapon_Crate2",
    "objTag_Weapon_Crutches",
    "objTag_Weapon_Cue",
    "objTag_Weapon_Cymbal",
    "objTag_Weapon_Drum",
    "objTag_Weapon_Dustbin",
    "objTag_Weapon_Dustpan",
    "objTag_Weapon_Egg",
    "objTag_Weapon_EggContainer",
    "objTag_Weapon_FeatherDuster",
    "objTag_Weapon_FeedingTrough",
    "objTag_Weapon_Figurine",
    "objTag_Weapon_FilmReel",
    "objTag_Weapon_FireBall",
    "objTag_Weapon_FireExtinguisher",
    "objTag_Weapon_FishingRod",
    "objTag_Weapon_FlourBag",
    "objTag_Weapon_FrenchStick",
    "objTag_Weapon_FruitBowl",
    "objTag_Weapon_FryingPan",
    "objTag_Weapon_Garlic",
    "objTag_Weapon_GiantScissors",
    "objTag_Weapon_GlassPane",
    "objTag_Weapon_Gnome",
    "objTag_Weapon_Gramophone",
    "objTag_Weapon_Guitar",
    "objTag_Weapon_HandSaw",
    "objTag_Weapon_HatStand",
    "objTag_Weapon_IroningBoard",
    "objTag_Weapon_JointOfMeat",
    "objTag_Weapon_LaboratoryBed",
    "objTag_Weapon_Ladder",
    "objTag_Weapon_LargePlate",
    "objTag_Weapon_LaundryBasket",
    "objTag_Weapon_Mailbox",
    "objTag_Weapon_Mirror",
    "objTag_Weapon_MummyCurse",
    "objTag_Weapon_Oar",
    "objTag_Weapon_OilDrum",
    "objTag_Weapon_PaintBall",
    "objTag_Weapon_PaintCan",
    "objTag_Weapon_PaintCanContainer",
    "objTag_Weapon_Painting",
    "objTag_Weapon_PicnicTable",
    "objTag_Weapon_PirateChest",
    "objTag_Weapon_PirateRum",
    "objTag_Weapon_PirateSword",
    "objTag_Weapon_Pitchfork",
    "objTag_Weapon_Plank",
    "objTag_Weapon_PlantPot",
    "objTag_Weapon_PopCan",
    "objTag_Weapon_PopCanGun",
    "objTag_Weapon_Pot",
    "objTag_Weapon_PotContainer",
    "objTag_Weapon_Pumpkin",
    "objTag_Weapon_Rake",
    "objTag_Weapon_RollingPin",
    "objTag_Weapon_SilverServiceTray",
    "objTag_Weapon_Skip",
    "objTag_Weapon_SmallTree",
    "objTag_Weapon_SnookerTable",
    "objTag_Weapon_Sofa",
    "objTag_Weapon_SofaButler",
    "objTag_Weapon_Spade",
    "objTag_Weapon_Speaker",
    "objTag_Weapon_SpecialPot",
    "objTag_Weapon_SpiderSpitBall",
    "objTag_Weapon_StandardLamp",
    "objTag_Weapon_Stool",
    "objTag_Weapon_SwordFish",
    "objTag_Weapon_Syringe",
    "objTag_Weapon_SyringeContainer",
    "objTag_Weapon_Table",
    "objTag_Weapon_TableLamp",
    "objTag_Weapon_Television",
    "objTag_Weapon_ToiletBrush",
    "objTag_Weapon_ToiletRoll",
    "objTag_Weapon_ToiletRollContainer",
    "objTag_Weapon_ToyBox",
    "objTag_Weapon_TreeStump",
    "objTag_Weapon_Trophy",
    "objTag_Weapon_Trunk",
    "objTag_Weapon_Umbrella",
    "objTag_Weapon_Vase",
    "objTag_Weapon_WarlockBolt",
    "objTag_Weapon_WashingPowder",
    "objTag_Weapon_WaterPistol",
    "objTag_Weapon_Welly",
    "objTag_Weapon_Wheelbarrow",
    "objTag_Weapon_ZombieArm",
};


SceneActorServices* script_scene_actors(Game& g)
{
    if (g.factory_scene == nullptr || g.factory_scene->actor_start != 1)
    {
        return nullptr;
    }
    return &g.factory_scene->start_actors(g);
}


// Xbox 0x00122D40
void message_runtime_marked_actors(SceneActorServices& actors, u16 tag, std::span<const u32> message)
{
    for (const auto& mark : actors.marked_actors())
    {
        if (read_u16(mark.bytes, 6) == tag)
        {
            actors.send_actor_message(mark.address, message);
        }
    }
}


// Xbox 0x000F96F0. AL is the value. The stack byte selects the pair at +0x294.
void entity_store_pair(std::span<u8> pairs, u8 index, u8 value)
{
    const int pair = int(index) * 2;
    const int byte_index = pair >> 3;
    const int shift = pair & 7;
    if (g_pair_cache[index] != value)
    {
        g_pair_cache[index] = value;
    }
    const u8 mask = static_cast<u8>(3u << shift);
    const u8 packed = static_cast<u8>(value << shift);
    u8 cur = pairs[byte_index];
    cur = static_cast<u8>(cur & static_cast<u8>(~mask));
    cur = static_cast<u8>(cur | packed);
    pairs[byte_index] = cur;
}




// Xbox 0x00117400. Bit 0 of the mask drives DAT_0050E764. Bit 1 drives
// DAT_0050E768. A rising edge (counter becomes 1) and a falling edge
// (counter becomes 0) walk DAT_00511B38. Note 0x26 when the flag is set,
// 0x27 when it is clear. Vtable +0xC runs when the word at link+0x3a8
// equals 2, or the dword at object+0x93c is nonzero, only for the bit-1
// edge. The other objects get the bit-0 edge.
// Xbox 0x00117400
void script_update_decay_latches(u32 mask, i32 flag)
{
    if (g_actor_pause)
    {
        g_actor_pause(static_cast<u8>(mask), flag);
        return;
    }
    // No native scene: DAT_00511B38 is empty, so only the counts change.
    RuntimePauseActor* none = nullptr;
    RuntimeActorPauseGroups groups{scene_detail::g_pause_references, none};
    change_runtime_actor_pause(static_cast<u8>(mask), flag, groups);
}


// Xbox 0x00124CC0. Handler 1. Type 0x12 calls 0x00117400. Type 0x90 retires
// the actor whose short at +6 matches the tag. Type 0x9F packs the byte at
// +0x4C into the pair selected by the byte at +0x48. Any other type returns 3.
// Xbox 0x00124CC0
int script_handle_actor_state(Game& g, const ScriptNode& node)
{
    const u32 type = node.type;
    if (type == 0x12)
    {
        const u32 word = node_u32(g.scene_script, node, 8);
        script_update_decay_latches(0xffff, word != 0 ? 1 : 0);
    }
    else if (type == 0x90)
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        const i32 found = binary_search_names(0xe9, kObjTagActor, key.c_str());
        const u16 id = found == -1 ? u16{0} : static_cast<u16>(found + 1);
        // Each DAT_00511B38 actor of the tag: 0x00112D10 on its +0x920 child, then vtable +0xC with 0x80 and
        // 0x0003ED70 while +0x908 is still null. With no native scene the list is empty.
        if (SceneActorServices* actors = script_scene_actors(g); actors != nullptr)
        {
            actors->retire_tagged_actors(id);
        }
    }
    else
    {
        if (type != 0x9f)
        {
            return 3;
        }
        const std::string key = node_cstr(g.scene_script, node, 8);
        const i32 found = binary_search_names(0xe9, kObjTagActor, key.c_str());
        const u16 id = found == -1 ? u16{0} : static_cast<u16>(found + 1);
        const u8 index = node_u8(g.scene_script, node, 0x48);
        const u8 value = node_u8(g.scene_script, node, 0x4c);
        // The DAT_00511B38 walk; with no native scene the list is empty.
        if (SceneActorServices* actors = script_scene_actors(g); actors != nullptr)
        {
            actors->set_tagged_actor_model_nodes(id, index, value);
        }
    }
    return 2;
}


// Xbox 0x001077B0. The cstring arrives in EAX. 0x000FBFB0 searches the actor
// tags. A miss returns 0. A hit returns the 1-based index, zero-extended
// from 16 bits.
// Xbox 0x001077B0
u32 script_find_actor_tag(const char* key)
{
    const i32 found = binary_search_names(0xe9, kObjTagActor, key);
    if (found == -1)
    {
        return 0;
    }
    return u32(u16(found + 1));
}





// Xbox 0x00124E50. Handler 2. Types 0x32 through 0x38 store the dword at
// node+8 and return 2. Types 0x34 and 0x36 scale by 0x001077B0. That call
// passes the cstring at node+0xC in EAX. Type 0x37 stores the float at
// node+8 times 0.01 (0x0032B124). Any other type returns 3.
// Xbox 0x00124E50
int script_handle_parameters(Game& g, const ScriptNode& node)
{
    const u32 word = node_u32(g.scene_script, node, 8);
    switch (node.type)
    {
    case 0x32:
        scene_word(0x0050E568u) = word;
        return 2;
    case 0x33:
        scene_word(0x0050E56Cu) = word;
        return 2;
    case 0x34:
    {
        const std::string key = node_cstr(g.scene_script, node, 0xC);
        const u32 slot = script_find_actor_tag(key.c_str()) & 0xffffu;
        scene_word(0x0050E570u + slot * 4u) = word;
        return 2;
    }
    case 0x35:
        scene_word(0x0050E570u) = word;
        return 2;
    case 0x36:
    {
        const std::string key = node_cstr(g.scene_script, node, 0xC);
        const u32 slot = script_find_actor_tag(key.c_str()) & 0xffffu;
        scene_word(0x0050E660u + slot * 4u) = word;
        return 2;
    }
    case 0x37:
    {
        const float scaled = std::bit_cast<float>(word) * 0.01f;
        scene_word(0x0050E754u) = std::bit_cast<u32>(scaled);
        return 2;
    }
    case 0x38:
        scene_word(0x0050E758u) = word;
        return 2;
    default:
        return 3;
    }
}


// Xbox 0x00124F30. Handler 3. Type 0x5D searches the cstring at node+8 in the
// actor tags. A miss calls 0x00122CA0 with tag 0. A hit calls it with the
// 1-based index, zero-extended from 16 bits. Both pass a message id of 1.
// Type 0x71 loads the word at node+8 into DX for 0x00129ED0. A hit calls
// vtable +0xC with a message id of 0x13. Any other type returns 3.
// Xbox 0x00124F30
int script_handle_actor_messages(Game& g, const ScriptNode& node)
{
    if (node.type != 0x5d && node.type != 0x71)
    {
        return 3;
    }
    SceneActorServices* actors = script_scene_actors(g);
    if (actors == nullptr)
    {
        throw Error("script type " + std::to_string(node.type) + " without the native scene's actors");
    }
    if (node.type == 0x5d)
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        const std::array<u32, 1> message{1};
        const i32 found = binary_search_names(0xe9, kObjTagActor, key.c_str());
        if (found == -1)
        {
            actors->message_tagged_actors(0, message); // 0x00122CA0, EDI 0
            return 2;
        }
        actors->message_tagged_actors(static_cast<u16>(found + 1), message); // 0x00122CA0
        return 2;
    }
    const u32 actor = actors->find_actor_by_id(node_u16(g.scene_script, node, 8)); // 0x00129ED0
    if (actor != 0)
    {
        const std::array<u32, 1> message{0x13};
        actors->send_actor_message(actor, message); // Vtable +0xC
    }
    return 2;
}


// DAT_00512350. Records are 8 bytes. DAT_00512370 is the count, 0x20 bytes
// above the base, so four records sit in front of it. 0x00125070 drains the
// count. The port keeps the record that count indexes.
struct PairSlot
{
    u32 at_8 = 0;
    u32 at_c = 0;
};

std::vector<PairSlot> g_pair_slots;

i32 g_pair_slot_count = 0;


// Xbox 0x00124FC0. Handler 4. The dword at node+4 must be 0x98. Any other
// type returns 3. The second argument is the flag 0x00128A00 passes: nonzero
// when a DAT_0050E4DC node has 0x7D at +8. Zero stores the dword at node+8
// at DAT_00512350 + DAT_00512370*8, the dword at node+0xC in the next dword,
// then increments DAT_00512370. Nonzero walks DAT_00511B20. The byte at
// node+8 selects the pair. The byte at node+0xC is the value in AL for
// 0x000F96F0. The object is the dword at link-0x730, then the dword at
// link-0x624. A null object skips that call. Both paths return 2.
// Xbox 0x00124FC0
int script_handle_pair_updates(Game& g, const ScriptNode& node, int live_7d)
{
    if (node.type != 0x98)
    {
        return 3;
    }
    if (live_7d != 0)
    {
        const u8 index = node_u8(g.scene_script, node, 8);
        const u8 value = node_u8(g.scene_script, node, 0xC);
        SceneActorServices* actors = script_scene_actors(g);
        if (actors == nullptr)
        {
            throw Error("script type 0x98 walks DAT_00511B20 outside a native scene");
        }
        actors->set_body_list_model_nodes(index, value);
        return 2;
    }
    const size_t slot = size_t(g_pair_slot_count);
    g_pair_slots.resize(slot + 1);
    g_pair_slots[slot].at_8 = node_u32(g.scene_script, node, 8);
    g_pair_slots[slot].at_c = node_u32(g.scene_script, node, 0xC);
    g_pair_slot_count = g_pair_slot_count + 1;
    return 2;
}


// Xbox 0x001251E0. Handler 5: the challenge's set-up. The dword at node+4 is the type; 0x7A names the challenge
// (0x000FBFB0 over the nine challengeId_* names at 0x004232AC), 0x7B starts it, and the others add a requirement
// (0x0004ADD0) built on the stack: its op at +0, then a word or dword at +8 (+0xC for 0x80). Any other type returns
// 3.
int script_handle_condition_setup(Game& g, const ScriptNode& node)
{
    RuntimeChallengeRecord record{};
    const auto tag = [&g, &node]()
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        return u16(script_find_actor_tag(key.c_str())); // 0x001077B0, EAX node+8
    };
    switch (node.type)
    {
    case 0x7a:
    {
        static constexpr std::array<const char*, 9> kChallenges{"challengeId_Attacks", "challengeId_Buttons",
            "challengeId_Challenge21", "challengeId_Damage", "challengeId_Health", "challengeId_Kills",
            "challengeId_KillsBOB", "challengeId_Survive", "challengeId_Time"}; // 0x004232AC
        const std::string key = node_cstr(g.scene_script, node, 8);
        const int kind = binary_search_names(9, kChallenges.data(), key.c_str()); // 0x000FBFB0
        create_runtime_challenge(u16(kind), challenge_runtime("script type 0x7A")); // 0x0004A100
        return 2;
    }
    case 0x7b:
        start_runtime_challenge(challenge_runtime("script type 0x7B")); // 0x0004A240
        return 2;
    case 0x7c:
        record.op = 0x10; // challengeReqId_TimeCounter, from 0 seconds
        record.value = std::bit_cast<u32>(0.f);
        break;
    case 0x7d:
        record.op = 0x11; // challengeReqId_TimeLimit
        record.value = node_u32(g.scene_script, node, 8);
        break;
    case 0x7e:
        record.op = 5; // challengeReqId_DontLetActorGetHit
        record.value = tag();
        break;
    case 0x7f:
        if (tag() == 0)
        {
            return 2;
        }
        record.op = 9; // challengeReqId_KillAllType
        record.value = tag();
        break;
    case 0x80:
        record.op = 0xA; // challengeReqId_KillTotalType
        record.value = tag();
        record.word_c = node_u32(g.scene_script, node, 0x48);
        break;
    case 0x81:
        record.op = 0x12; // challengeReqId_WeaponsOnly
        break;
    case 0x82:
        record.op = 4; // challengeReqId_DontKill
        record.value = tag();
        break;
    case 0x83:
        record.op = 0xB; // challengeReqId_LimitedAttacks
        record.value = node_u32(g.scene_script, node, 8);
        break;
    case 0x84:
        record.op = 0xC; // challengeReqId_NoDamage
        break;
    case 0x85:
        record.op = 0; // challengeReqId_AttackCounter
        break;
    case 0x86:
        record.op = 3; // challengeReqId_DamageCounter
        break;
    default:
        return 3;
    }
    add_runtime_challenge_record(record, challenge_runtime("a challenge requirement script type")); // 0x0004ADD0
    return 2;
}


// Xbox 0x00125450. One node of the action walk. Type 0x3C stops the walk.
// The listed types copy a condition, then 0x000532B0 (ECX = cursor, EDX =
// 0x3C) moves the cursor to the block's closing 0x3C, so the runner advances
// past the condition body (types 0x3F, 0x41, 0x43, 0x44 and 0x6C included). Every other type returns 3. Types 0x40, 0x6A, 0x94,
// 0x95 and 0x99 store 0x001077B0's tag in the word at +8; 0x6A also copies the
// float at node+0x4C to +0x10.
// Xbox 0x00125450
int script_handle_condition_node(Game& g, const ScriptNode& node, u32& cursor)
{
    u32 opcode = 0;
    bool as_float = false;
    bool word_then_far = false;
    bool payload_at_8 = false;
    bool float_at_10 = false;
    bool word_and_float = false;
    bool tag_only = false;
    switch (node.type)
    {
    case 0x3C:
        return 1;
    case 0x3D:
        opcode = 0;
        as_float = true;
        break;
    case 0x3E:
        opcode = 1;
        payload_at_8 = true;
        break;
    case 0x3F:
        opcode = 2; // 0x001254D3: the link alone.
        break;
    case 0x40:
        opcode = 3;
        word_then_far = true;
        break;
    case 0x41:
        opcode = 4; // 0x001254F9: the dword at node+8 and the float at node+0xC.
        payload_at_8 = true;
        break;
    case 0x43:
        opcode = 6; // 0x00125543: the word at node+8 and the float at node+0xC.
        word_and_float = true;
        break;
    case 0x42:
    case 0x5A:
    case 0x5B:
    {
        // 0x00125536 / 0x001255F4 / 0x0012561B: opcode 5 (NumberOfKnockdowns), 0xB (SpecificGhouliesAlive) or 0xA
        // (SpecificGhouliesKilled), the link, the dword at node+8, 0x001077B0's tag for the name at node+0xC as a word
        // at +0xC and the float at node+0x4C at +0x10.
        const u32 code = node.type == 0x42 ? 5u : node.type == 0x5A ? 11u : 10u;
        ScriptCond rec{};
        cond_put_u32(rec, 0, code);
        cond_put_u32(rec, 4, node.link);
        cond_put_u32(rec, 8, node_u32(g.scene_script, node, 8));
        cond_put_u32(rec, 0xC, static_cast<u16>(script_find_actor_tag(node_cstr(g.scene_script, node, 0xC).c_str())));
        cond_put_u32(rec, 0x10, node_u32(g.scene_script, node, 0x4C));
        script_append_condition(rec, kScriptCondSize[code], code); // 0x00124C80, EDI the record
        script_seek_node_type(g.scene_script, cursor, 0x3C); // 0x000532B0
        return 2;
    }
    case 0x72:
    {
        // 0x001256CB: 0x001253F0 clears the record and stores opcode 0x11 (ObjectByMarkerBroken) and the link, then
        // the dword at node+8, the float at node+0xC and the dword at node+0x10.
        ScriptCond rec{};
        cond_put_u32(rec, 0, 17);
        cond_put_u32(rec, 4, node.link);
        cond_put_u32(rec, 8, node_u32(g.scene_script, node, 8));
        cond_put_u32(rec, 0xC, node_u32(g.scene_script, node, 0xC));
        cond_put_u32(rec, 0x10, node_u32(g.scene_script, node, 0x10));
        script_append_condition(rec, kScriptCondSize[17], 17); // 0x00124C80, EDI the record
        script_seek_node_type(g.scene_script, cursor, 0x3C); // 0x000532B0
        return 2;
    }
    case 0x44:
        opcode = 7; // 0x00125576: 0x001077B0's tag for the name at node+8.
        tag_only = true;
        break;
    case 0x47:
        opcode = 8;
        payload_at_8 = true;
        break;
    case 0x48:
        opcode = 9;
        payload_at_8 = true;
        break;
    case 0x6C:
        opcode = 16; // 0x00125748, scriptConditionId_GhoulyExistsInScene: the tag and the float at node+0x48.
        word_then_far = true;
        break;
    case 0x6A:
        opcode = 15; // scriptConditionId_GhoulyTriggered: the tag, node+0x48 and the float at node+0x4C.
        word_then_far = true;
        float_at_10 = true;
        break;
    case 0x94:
        opcode = 23;
        word_then_far = true;
        break;
    case 0x95:
        opcode = 24;
        word_then_far = true;
        break;
    case 0x99:
        opcode = 25;
        word_then_far = true;
        break;
    case 0x92:
        opcode = 21; // 0x001257F6, scriptConditionId_IsChallenge21Active: the link alone.
        break;
    case 0x93:
        opcode = 22; // 0x0012580F, scriptConditionId_IsChallenge21NotActive: the link alone.
        break;
    case 0x66:
        opcode = 13; // 0x00125726, scriptConditionId_PlayerDeflectionContact: the tag and the float at node+0x48.
        word_then_far = true;
        break;
    case 0x68:
        opcode = 14; // 0x0012573E, scriptConditionId_PlayerHitGhouly: the tag and the float at node+0x48.
        word_then_far = true;
        break;
    case 0x8B:
    {
        // 0x00125661: 0x00125420 clears the 0x20-byte record as opcode 0x13 with the link, then the dword at
        // node+8, 0x001077B0's tag for the name at node+0xC as a word at +0xC and the float at node+0x4C at +0x10.
        ScriptCond rec{};
        cond_put_u32(rec, 0, 0x13);
        cond_put_u32(rec, 4, node.link);
        cond_put_u32(rec, 8, node_u32(g.scene_script, node, 8));
        cond_put_u32(rec, 0xC, static_cast<u16>(script_find_actor_tag(node_cstr(g.scene_script, node, 0xC).c_str())));
        cond_put_u32(rec, 0x10, node_u32(g.scene_script, node, 0x4C));
        script_append_condition(rec, kScriptCondSize[0x13], 0x13); // 0x00124C80, EDI the record
        script_seek_node_type(g.scene_script, cursor, 0x3C); // 0x000532B0
        return 2;
    }
    case 0x75:
        opcode = 18; // 0x001257A6, scriptConditionId_ActorHasNoEnergy: the tag and the float at node+0x48.
        word_then_far = true;
        break;
    case 0x65:
        opcode = 12; // 0x0012569A, scriptConditionId_ObjectByMarkerRemoved: the dword at node+8 and the float at +0xC.
        payload_at_8 = true;
        break;
    case 0x8C:
        opcode = 20; // 0x001257DD, scriptConditionId_NoGhoulySpawnsActive: the link alone.
        break;
    case 0xA3:
        opcode = 26; // 0x001256FE, scriptConditionId_ObjectByMarkerNotBroken: the dword at node+8.
        payload_at_8 = true;
        break;
    default:
        return 3;
    }
    ScriptCond rec{};
    cond_put_u32(rec, 0, opcode);
    cond_put_u32(rec, 4, node.link);
    if (as_float)
    {
        const i32 ticks = i32(node_u32(g.scene_script, node, 8));
        cond_put_f32(rec, 8, float(ticks));
        cond_put_u32(rec, 0xC, node_u32(g.scene_script, node, 0xC));
    }
    else if (word_then_far)
    {
        // The word at +8 is 0x001077B0's tag for the name at node+8.
        cond_put_u32(rec, 8, static_cast<u16>(script_find_actor_tag(node_cstr(g.scene_script, node, 8).c_str())));
        cond_put_u32(rec, 0xC, node_u32(g.scene_script, node, 0x48));
        if (float_at_10)
        {
            cond_put_u32(rec, 0x10, node_u32(g.scene_script, node, 0x4C));
        }
    }
    else if (word_and_float)
    {
        cond_put_u32(rec, 8, node_u16(g.scene_script, node, 8));
        cond_put_u32(rec, 0xC, node_u32(g.scene_script, node, 0xC));
    }
    else if (tag_only)
    {
        cond_put_u32(rec, 8, static_cast<u16>(script_find_actor_tag(node_cstr(g.scene_script, node, 8).c_str())));
    }
    else if (payload_at_8)
    {
        cond_put_u32(rec, 8, node_u32(g.scene_script, node, 8));
        if (kScriptCondSize[opcode] > 0xC)
        {
            cond_put_u32(rec, 0xC, node_u32(g.scene_script, node, 0xC));
        }
    }
    script_append_condition(rec, kScriptCondSize[opcode], opcode);
    script_seek_node_type(g.scene_script, cursor, 0x3C);
    return 2;
}


// DAT_00512378. Type 0x29 sets this after 0x00117400 and clears it when 0x0005B780 reads the awaited door state.
i32 g_flag_2378 = 0;


// DAT_0051237C. Records are 4 bytes: the word at node+8, then the word at
// node+0xC. DAT_005123AC is the count, 0x30 bytes above the base, so twelve
// records sit in front of it. The port keeps the record that count indexes.
struct TagPair
{
    u16 at_8 = 0;
    u16 at_c = 0;
};

std::vector<TagPair> g_tag_pairs;

i32 g_tag_pair_count = 0;


// DAT_005123B0. Records are 8 bytes. DAT_00512410 is the count, 0x60 bytes
// above the base. The word is at +0 and the dword is at +4.
struct QueuedTag
{
    u16 at_8 = 0;
    u32 at_c = 0;
};

std::vector<QueuedTag> g_queued_tags;

i32 g_queued_tag_count = 0;


// DAT_00512414. The next dword after DAT_00512410. Type 0x18 stores 1 here,
// then stores 0 when DAT_005064D8 is 3.
i32 g_flag_2414 = 0;


// DAT_00512418. Type 0x2a stores 1 after a live dword at node+0x10c, and
// stores 0 when 0x001412F0 returns nonzero.
i32 g_flag_2418 = 0;


// DAT_00512420. Records are 4 bytes: the byte at node+8, then the word at
// node+0xC. DAT_00512440 is the count, 0x20 bytes above the base, so eight
// records sit in front of it. Type 0x67 with a zero second argument appends
// one record. The port keeps the record that count indexes.
struct GateRec2420
{
    u8 at_8 = 0;
    u16 at_c = 0;
};

std::vector<GateRec2420> g_gate_2420;

i32 g_gate_2440 = 0;


// DAT_00512448. DAT_00512444 is the count, the dword immediately before this
// base. DAT_0051245C begins 0x14 bytes above the base, so five dwords sit in
// front of it. Type 0x9E stores the story bit at node+8 here.
std::vector<u32> g_mark_2448;

// DAT_0051245C: each DAT_00512448 bit's value before type 0x9E changed it.
std::vector<u32> g_mark_245c;

i32 g_mark_2444 = 0;

// Xbox 0x001272B0, EAX restore: the scene teardown's. With `restore` (DAT_00430D0C 1, the scene starting again) each
// bit type 0x9E changed goes back to its value before; either way the list empties.
void restore_script_story_bits(bool restore)
{
    if (!restore)
    {
        g_mark_2444 = 0;
        return;
    }
    for (i32 index = 0; index < g_mark_2444; ++index)
    {
        const u32 bit = g_mark_2448[size_t(index)];
        if (g_mark_245c[size_t(index)] == 0)
        {
            clear_runtime_story_bit(g_runtime_profile.progress.bytes, bit);
        }
        else
        {
            set_runtime_story_bit(g_runtime_profile.progress.bytes, bit);
        }
    }
    g_mark_2444 = 0;
}




// DAT_0050EA9C. Type 0x1A tests bit 0x10 of the low byte.
u32 g_flags_ea9c = 0;


// DAT_00506540. Type 0x54 stores the floats at node+8, +0xC, +0x10, and +0x14,
// then the dword at node+0x18 into DAT_00506550.
float g_script_6540 = 0.f;

float g_script_6544 = 0.f;

float g_script_6548 = 0.f;

float g_script_654c = 0.f;

u32 g_script_6550 = 0;


// Xbox 0x001259B0. Handler 7. The dword at node+4 is the type. Type 0x4F
// with a zero second argument stores the word at node+8 and the dword at
// node+0xC at DAT_005123B0 + DAT_00512410*8, then increments DAT_00512410.
// A nonzero second argument calls 0x001117E0 with the word at node+8 pushed
// and EBX the dword at node+0xC. Type 0x56 builds a message whose first dword
// is 0x67 and whose second dword is the word at node+8, then calls
// 0x00122F70 with that message in EDI. Type 0x29 with a nonzero second
// argument and a zero third argument reads DAT_00512378. Zero calls
// 0x000532E0 on the node and passes its return, the word at node+8 in EAX,
// the word at node+0xC, and the dword at node+0x14 to 0x0005B540. The dword
// at node+0x10 then selects the return: zero returns 2, nonzero calls
// 0x00117400(0xffff, 0), sets DAT_00512378, and returns 0. Nonzero calls
// 0x0005B780 with the word at node+8 in SI. A miss compares 8 with the dword
// at node+0xC. A hit compares the written word. A match calls
// 0x00117400(0xffff, 1), clears DAT_00512378, and returns 2. The other path
// stores those two words in the 4-byte record at DAT_0051237C and grows
// DAT_005123AC when the index has reached the count. Any other type returns
// 3. A handled store returns 2.
// Xbox 0x00125B80. Scene start (0x0011CBD0): every door's navigation regions follow its state (0x0005C020), the
// door pairs the script's head recorded set their doors (0x0005B540 with no hold), the queued 0x4F tags set their
// regions on every grid, both lists empty, and each DAT_00511B20 actor gets message 0x66.
// Xbox 0x00125B80
void script_apply_scene_door_states(ScriptDoorStartContext& context)
{
    apply_runtime_door_regions(context.doors); // 0x0005C020
    for (i32 index = 0; index < g_tag_pair_count; ++index)
    {
        const TagPair& pair = g_tag_pairs.at(size_t(index));
        set_runtime_door_state(pair.at_8, pair.at_c, 0, 0, context.doors);
    }
    for (i32 index = 0; index < g_queued_tag_count; ++index)
    {
        const QueuedTag& tag = g_queued_tags.at(size_t(index));
        set_all_navigation_regions_blocked(tag.at_8, i16(tag.at_c)); // 0x00036A00 on each grid, the dword pushed
    }
    g_tag_pair_count = 0;
    g_queued_tag_count = 0;
    context.notify_body_list(0x66);
}


// Xbox 0x000532E0. ECX is the node: true when it is the script's last action, followed by the end node or by a
// type 0xF gate and then the end node.
bool script_node_is_last_action(const SceneScript& script, const ScriptNode& node)
{
    if (node.type == 0)
    {
        return true;
    }
    auto next = script_node_at(script, node.link);
    if (!next)
    {
        throw Error("script node " + std::to_string(node.offset) + " links outside the script");
    }
    if (next->type == 0)
    {
        return true;
    }
    if (next->type != 0xF)
    {
        return false;
    }
    auto after = script_node_at(script, next->link);
    if (!after)
    {
        throw Error("script node " + std::to_string(next->offset) + " links outside the script");
    }
    return after->type == 0;
}


// Xbox 0x001259B0
int script_handle_action_links(Game& g, const ScriptNode& node, int live_7d, int ending)
{
    const u32 type = node.type;
    if (type != 0x29)
    {
        if (type == 0x4f)
        {
            if (live_7d == 0)
            {
                const size_t slot = size_t(g_queued_tag_count);
                g_queued_tags.resize(slot + 1);
                g_queued_tags[slot].at_8 = node_u16(g.scene_script, node, 8);
                g_queued_tags[slot].at_c = node_u32(g.scene_script, node, 0xC);
                g_queued_tag_count = g_queued_tag_count + 1;
                return 2;
            }
            // 0x001117E0 with the word at node+8 pushed and EBX the dword at node+0xC (its low word the blocked value).
            entity_detail::set_all_navigation_regions_blocked(node_u16(g.scene_script, node, 8),
                i16(node_u32(g.scene_script, node, 0xC)));
            return 2;
        }
        if (type == 0x56)
        {
            const std::array<u32, 2> message{0x67, u32(node_u16(g.scene_script, node, 8))};
            if (!g_script_body_list_message)
            {
                throw Error("script node 0x56 sends message 0x67 to DAT_00511B20 outside a native scene");
            }
            g_script_body_list_message(message); // 0x00122F70, EDI the message
            return 2;
        }
        return 3;
    }
    if ((live_7d != 0) && (ending == 0))
    {
        if (g_runtime_doors == nullptr)
        {
            throw Error("script door node 0x29 without the scene's DAT_004EE2B8 door context");
        }
        if (g_flag_2378 == 0)
        {
            const u32 last = script_node_is_last_action(g.scene_script, node) ? 1u : 0u; // 0x000532E0, ECX node
            set_runtime_door_state(node_u16(g.scene_script, node, 8), node_u16(g.scene_script, node, 0xC),
                node_u32(g.scene_script, node, 0x14), last, *g_runtime_doors); // 0x0005B540
            if (node_u32(g.scene_script, node, 0x10) == 0)
            {
                return 2;
            }
            script_update_decay_latches(0xffff, 0);
            g_flag_2378 = 1;
        }
        else
        {
            // 0x0005B780 with SI the door: a miss leaves 8 in AX.
            u16 state = 8;
            runtime_door_state(node_u16(g.scene_script, node, 8), state, *g_runtime_doors);
            if (u32(state) == node_u32(g.scene_script, node, 0xC))
            {
                script_update_decay_latches(0xffff, 1);
                g_flag_2378 = 0;
                return 2;
            }
        }
        return 0;
    }
    i32 index = 0;
    if (0 < g_tag_pair_count)
    {
        const u32 key = node_u32(g.scene_script, node, 8);
        do
        {
            if (u32(g_tag_pairs[size_t(index)].at_8) == key)
            {
                break;
            }
            index = index + 1;
        } while (index < g_tag_pair_count);
    }
    if (size_t(g_tag_pairs.size()) <= size_t(index))
    {
        g_tag_pairs.resize(size_t(index) + 1);
    }
    g_tag_pairs[size_t(index)].at_8 = node_u16(g.scene_script, node, 8);
    g_tag_pairs[size_t(index)].at_c = node_u16(g.scene_script, node, 0xC);
    if (g_tag_pair_count <= index)
    {
        g_tag_pair_count = g_tag_pair_count + 1;
    }
    return 2;
}


// Xbox 0x00126940, DI tag: the DAT_00511B48 actors of the tag (any for a tag below 2) get +0x96D set and +0x96F /
// +0x970 cleared; one whose +0x974 timer runs is stopped and sent message 0x55. With `keep` 0 the tag joins
// DAT_0050E22C.
void mark_runtime_one_type_actors(u16 tag, i32 keep)
{
    for (const auto* node = static_cast<const entity_detail::ListHead*>(entity_detail::g_world.alt.next);
         node != nullptr; node = static_cast<const entity_detail::ListHead*>(node->next))
    {
        if (!g_alt_node_class || !g_alt_node_actor || !g_alt_node_actor_bytes)
        {
            throw Error("0x00126940 walks DAT_00511B48 without the scene's actor views");
        }
        if (tag >= 2 && g_alt_node_class(node) != tag)
        {
            continue;
        }
        const std::span<u8> actor = g_alt_node_actor_bytes(node);
        player_record(actor).unk_96D = 1; // Node +0x29.
        player_record(actor).unk_96F = 0;
        player_record(actor).unk_970 = 0;
        if (0.f < player_record(actor).countdown_55) // Node +0x30.
        {
            std::memset(actor.data() + offsetof(PlayerRecord, countdown_55), 0, 4);
            std::array<u32, 0x42> message{0x55};
            g_script_actor_message(g_alt_node_actor(node), message); // Node - 0x944 vtable +0xC.
        }
    }
    if (keep == 0)
    {
        if (!g_script_exclude_actor_tag)
        {
            throw Error("0x00126940 appends to DAT_0050E22C outside a native scene");
        }
        g_script_exclude_actor_tag(tag);
    }
}

// Xbox 0x00126140. Handler 8. The dword at node+4 is the type; the table covers types 0x17 through 0x88, and any
// other type, and the holes in that range, return 3. Type 0x17 opens the event (0x000D0530, 0x000D0550), type 0x18
// plays it, type 0x54 copies five fields into DAT_00506540, and the other handled types build an event step with
// their header (0x00125C80 .. 0x00126100) and queue it through 0x000D05D0. A handled type returns 2. The second
// argument is unused.
// Xbox 0x00126140
int script_handle_scene_actions(Game& g, const ScriptNode& node, int live_7d, int ending)
{
    (void)live_7d;
    auto push_named = [&](u32 gate, u32 key_at, u16* words, u32& count)
    {
        if (node_u32(g.scene_script, node, gate) != 0)
        {
            const std::string key = node_cstr(g.scene_script, node, key_at);
            words[count] = static_cast<u16>(script_find_actor_tag(key.c_str()));
            count = count + 1;
        }
    };
    switch (node.type)
    {
    case 0x17:
        if (ending != 0)
        {
            return 0;
        }
        if (node_u32(g.scene_script, node, 0x10) != 0)
        {
            reset_script_event_sequence(); // 0x000D0530
        }
        {
            const std::string name = node_cstr(g.scene_script, node, 0x14);
            begin_script_event_sequence(node_u16(g.scene_script, node, 8), node_u32(g.scene_script, node, 0xC),
                name.c_str()); // 0x000D0550, ESI node+0x14
        }
        return 2;
    case 0x18:
        if (g_flag_2414 == 0)
        {
            play_script_event_sequence(g); // 0x000D0680
            g_flag_2414 = 1;
            return 0;
        }
        if (g_event_sequence_state == 3)
        {
            g_flag_2414 = 0;
            return 2;
        }
        return 0;
    case 0x19:
    {
        // 0x00125C80 builds step type 0 with XMM0 = 1.5 (0x0032B210) at +4, then 0x000D05D0 queues it.
        EventStep step{};
        const float delay = 1.5f;
        step.delay.left = delay;
        queue_script_event_step(step);
        return 2;
    }
    case 0x1a:
    {
        // 0x00125D80 builds step type 4 with XMM0 = 0 at +4; +0x18 is the time limit at node+8. 0x000D05D0 queues it
        // unless bit 0x10 of DAT_0050EA9C is set.
        EventStep step{};
        const u32 type = 4;
        step.type = type;
        const u32 seconds = node_u32(g.scene_script, node, 8);
        step.timed.seconds = std::bit_cast<float>(seconds);
        if ((g_flags_ea9c & 0x10u) != 0)
        {
            return 2;
        }
        queue_script_event_step(step);
        return 2;
    }
    case 0x1b:
    {
        // 0x00125CC0 builds step type 1 with XMM0 = 0 at +4, then 0x000D05D0 queues it.
        EventStep step{};
        const u32 type = 1;
        step.type = type;
        queue_script_event_step(step);
        return 2;
    }
    case 0x1c:
    {
        // 0x00125D00 builds step type 2 with XMM0 = 1.5 (0x0032B210) at +4, clearing it from +0x18 to the type's
        // size (DAT_00428934); +0x18 is the tag of the name at node+8 and +0x1C is set when the dword at node+0x48
        // is 0. 0x000D05D0 queues it.
        EventStep step{};
        const u32 type = 2;
        const float delay = 1.5f;
        step.type = type;
        step.delay.left = delay;
        const u16 tag = static_cast<u16>(script_find_actor_tag(node_cstr(g.scene_script, node, 8).c_str()));
        step.tag.tag = tag;
        const u32 absent = node_u32(g.scene_script, node, 0x48) == 0 ? 1u : 0u;
        step.tag.seen = absent;
        queue_script_event_step(step);
        return 2;
    }
    case 0x1d:
    {
        // 0x00125D40 builds step type 3 with XMM0 = 1.5 (0x0032B210) at +4; +0x18 is the kill count 0x0001BB60
        // draws from [node+8, node+0xC + 1) and +0x1C the tag of the name at node+0x10. 0x000D05D0 queues it.
        EventStep step{};
        const u32 type = 3;
        const float delay = 1.5f;
        step.type = type;
        step.delay.left = delay;
        const i32 low = i32(node_u32(g.scene_script, node, 8));
        const i32 high = i32(node_u32(g.scene_script, node, 0xC)) + 1;
        const i32 count = draw_runtime_random_count(high, low, g_shared_random); // 0x0001BB60, ESI DAT_0054DD7C
        step.kill_count.required = count;
        const u16 tag = static_cast<u16>(script_find_actor_tag(node_cstr(g.scene_script, node, 0x10).c_str()));
        step.kill_count.tag = tag;
        queue_script_event_step(step);
        return 2;
    }
    case 0x1e:
    {
        // 0x00125DC0 builds step type 5 with XMM0 = 0 at +4; +0x18 is the time to survive at node+8. 0x000D05D0
        // queues it.
        EventStep step{};
        const u32 type = 5;
        step.type = type;
        const u32 seconds = node_u32(g.scene_script, node, 8);
        step.timed.seconds = std::bit_cast<float>(seconds);
        queue_script_event_step(step);
        return 2;
    }
    case 0x1f:
    {
        // 0x00126040 builds step type 0xF with XMM0 = 2.0 (0x0032B110) at +4, then 0x000D05D0 queues it.
        EventStep step{};
        const u32 type = 0xF;
        const float delay = 2.f;
        step.type = type;
        step.delay.left = delay;
        queue_script_event_step(step);
        return 2;
    }
    case 0x20:
    {
        // 0x00125E00 builds step type 6 with XMM0 = 1.5 (0x0032B210) at +4; +0x18 is the word at node+8.
        // 0x000D05D0 queues it.
        EventStep step{};
        const u32 type = 6;
        const float delay = 1.5f;
        step.type = type;
        step.delay.left = delay;
        const u32 word = u32(node_u16(g.scene_script, node, 8));
        step.done.unk_18 = word;
        queue_script_event_step(step);
        return 2;
    }
    case 0x21:
    {
        // 0x00125E40 builds step type 7 with XMM0 = 1.5 (0x0032B210) at +4; +0x18 is the tag of the name at node+8
        // and +0x1C is -1 (the total, kept on its first run). 0x000D05D0 queues it.
        EventStep step{};
        const u32 type = 7;
        const float delay = 1.5f;
        step.type = type;
        step.delay.left = delay;
        const u16 tag = static_cast<u16>(script_find_actor_tag(node_cstr(g.scene_script, node, 8).c_str()));
        step.spare_tag.tag = tag;
        step.spare_tag.kept_total = -1;
        queue_script_event_step(step);
        return 2;
    }
    case 0x22:
    {
        // 0x00125F00 builds step type 10 with XMM0 = 1.5 (0x0032B210) at +4; +0x18 is -1 (the total, kept on its
        // first run). 0x000D05D0 queues it.
        EventStep step{};
        const u32 type = 10;
        const float delay = 1.5f;
        step.type = type;
        step.delay.left = delay;
        step.unhit_since.kept_total = -1;
        queue_script_event_step(step);
        return 2;
    }
    case 0x23:
    {
        // 0x00125EC0 builds step type 9 with XMM0 = 1.5 (0x0032B210) at +4, then 0x000D05D0 queues it.
        EventStep step{};
        const u32 type = 9;
        const float delay = 1.5f;
        step.type = type;
        step.delay.left = delay;
        queue_script_event_step(step);
        return 2;
    }
    case 0x25:
    {
        // 0x00125E80 builds step type 8 with XMM0 = 1.5 (0x0032B210) at +4; +0x18 is 0, the named tags follow at
        // +0x1A with their count in the byte at +0x4C. 0x000D05D0 queues it.
        u16 words[5]{};
        u32 count = 0;
        push_named(0x48, 8, words, count);
        push_named(0x8c, 0x4c, words, count);
        push_named(0xd0, 0x90, words, count);
        push_named(0x114, 0xd4, words, count);
        push_named(0x158, 0x118, words, count);
        EventStep step{};
        const u32 type = 8;
        const float delay = 1.5f;
        step.type = type;
        step.delay.left = delay;
        std::memcpy(step.next_different.tags.data(), words, size_t(count) * 2);
        step.next_different.count = u8(count);
        queue_script_event_step(step);
        return 2;
    }
    case 0x26:
    {
        // 0x00125F40 builds step type 0xB with XMM0 = 1.5 (0x0032B210) at +4, then 0x000D05D0 queues it.
        EventStep step{};
        const u32 type = 0xB;
        const float delay = 1.5f;
        step.type = type;
        step.delay.left = delay;
        queue_script_event_step(step);
        return 2;
    }
    case 0x27:
    {
        // 0x00126080 builds step type 0x10 with XMM0 = 1.5 (0x0032B210) at +4; +0x18 is the dword at node+8.
        // 0x000D05D0 queues it.
        EventStep step{};
        const u32 type = 0x10;
        const float delay = 1.5f;
        const u32 value = node_u32(g.scene_script, node, 8);
        step.type = type;
        step.delay.left = delay;
        step.attack_limit.limit = std::bit_cast<i32>(value);
        queue_script_event_step(step);
        return 2;
    }
    case 0x28:
    {
        // 0x00125F80 builds step type 0xC with XMM0 = 1.5 (0x0032B210) at +4, then 0x000D05D0 queues it.
        EventStep step{};
        const u32 type = 0xC;
        const float delay = 1.5f;
        step.type = type;
        step.delay.left = delay;
        queue_script_event_step(step);
        return 2;
    }
    case 0x50:
    {
        // 0x00125FC0 builds step type 0xD (one type) with XMM0 = 1.5 (0x0032B210) at +4; +0x18 is one of the named
        // tags drawn at random (0x0001BB60) and +0x1C 0. 0x000D05D0 queues it and 0x00116C50 plays the tag's fight
        // music.
        u16 words[5]{};
        u32 count = 0;
        push_named(0x48, 8, words, count);
        push_named(0x8c, 0x4c, words, count);
        push_named(0xd0, 0x90, words, count);
        push_named(0x114, 0xd4, words, count);
        push_named(0x158, 0x118, words, count);
        EventStep step{};
        const u32 type = 0xD;
        const float delay = 1.5f;
        step.type = type;
        step.delay.left = delay;
        const i32 index = draw_runtime_random_count(i32(u16(count)), 0, g_shared_random); // 0x0001BB60, ECX count
        const u16 tag = words[size_t(index)];
        step.one_type.tag = tag;
        if (node_u32(g.scene_script, node, 0x15c) != 0)
        {
            mark_runtime_one_type_actors(tag, 0); // 0x00126940, DI the tag
        }
        queue_script_event_step(step);
        play_scene_fight_music(i16(tag), g.executable_resources->memory(0x0030E490, 0xCB2), g_name_e4e0,
            scene_music_streams(g)); // 0x00116C50, CX the tag
        return 2;
    }
    case 0x54:
        g_script_6540 = std::bit_cast<float>(node_u32(g.scene_script, node, 8));
        g_script_6544 = std::bit_cast<float>(node_u32(g.scene_script, node, 0xC));
        g_script_6548 = std::bit_cast<float>(node_u32(g.scene_script, node, 0x10));
        g_script_654c = std::bit_cast<float>(node_u32(g.scene_script, node, 0x14));
        g_script_6550 = node_u32(g.scene_script, node, 0x18);
        return 2;
    case 0x5f:
    {
        // 0x00126000 builds step type 0xE with XMM0 = 1.5 (0x0032B210) at +4; +0x18 is a count drawn from node+8 up
        // to node+0xC (0x0001BB60, ECX node+0xC + 1, EDX node+8, ESI DAT_0054DD7C). 0x000D05D0 queues it.
        const u32 lo = node_u32(g.scene_script, node, 8);
        const u32 hi = node_u32(g.scene_script, node, 0xC) + 1u;
        EventStep step{};
        const u32 type = 0xE;
        const float delay = 1.5f;
        step.type = type;
        step.delay.left = delay;
        const i32 drawn = draw_runtime_random_count(i32(hi), i32(lo), g_shared_random); // 0x0001BB60
        step.number.target = drawn;
        queue_script_event_step(step);
        return 2;
    }
    case 0x87:
    {
        // 0x001260C0 builds step type 0x11 with XMM0 = 1.5 (0x0032B210) at +4, then 0x000D05D0 queues it.
        EventStep step{};
        const u32 type = 0x11;
        const float delay = 1.5f;
        step.type = type;
        step.delay.left = delay;
        queue_script_event_step(step);
        return 2;
    }
    case 0x88:
    {
        // 0x00126100 builds step type 0x12 with XMM0 = 1.5 (0x0032B210) at +4. Each named tag with its kill count
        // (node+0x4C, +0x94, +0xDC, +0x124, +0x16C set) is a choice; one is drawn at random (0x0001BB60, ECX the
        // count) for +0x18 (the tag) and +0x1C (its count). With node+0x170 the tag's actors are marked
        // (0x00126940), then 0x000D05D0 queues the step and 0x00116C50 plays the tag's fight music.
        u16 words[5]{};
        u32 counts[5]{};
        u32 count = 0;
        const auto choice = [&](u32 enabled, u32 name, u32 kills)
        {
            if (node_u32(g.scene_script, node, enabled) == 0)
            {
                return;
            }
            const std::string key = node_cstr(g.scene_script, node, name);
            words[count] = static_cast<u16>(script_find_actor_tag(key.c_str())); // 0x001077B0
            counts[count] = node_u32(g.scene_script, node, kills);
            ++count;
        };
        choice(0x4c, 8, 0x48);
        choice(0x94, 0x50, 0x90);
        choice(0xdc, 0x98, 0xd8);
        choice(0x124, 0xe0, 0x120);
        choice(0x16c, 0x128, 0x168);
        EventStep step{};
        const u32 type = 0x12;
        const float delay = 1.5f;
        step.type = type;
        step.delay.left = delay;
        const i32 index = draw_runtime_random_count(i32(u16(count)), 0, g_shared_random); // 0x0001BB60, ECX count
        const u16 tag = words[size_t(index)];
        step.tag_kill_count.tag = tag;
        step.tag_kill_count.required = std::bit_cast<i32>(counts[size_t(index)]);
        if (node_u32(g.scene_script, node, 0x170) != 0)
        {
            mark_runtime_one_type_actors(tag, 0); // 0x00126940, DI the tag
        }
        queue_script_event_step(step);
        play_scene_fight_music(i16(tag), g.executable_resources->memory(0x0030E490, 0xCB2), g_name_e4e0,
            scene_music_streams(g)); // 0x00116C50, CX the tag
        return 2;
    }
    default:
        return 3;
    }
}


// Xbox 0x001269D0. Handler 9. The dword at node+4 is the type. Type 0x49 ORs
// the dword at node+8 into DAT_0050EA9C and returns 2. Type 0x4A clears those
// bits and returns 2. Any other type returns 3.
// Xbox 0x001269D0
int script_handle_flag_bits(Game& g, const ScriptNode& node)
{
    if (node.type == 0x49)
    {
        g_flags_ea9c = g_flags_ea9c | node_u32(g.scene_script, node, 8);
        return 2;
    }
    if (node.type != 0x4a)
    {
        return 3;
    }
    g_flags_ea9c = g_flags_ea9c & ~node_u32(g.scene_script, node, 8);
    return 2;
}


// Xbox 0x00126A10. Handler 10. The dword at node+4 is the type. The byte map
// at 0x00126D94 covers types 0x2A through 0x97. Any other type, and the holes
// in that range, return 3.
//
// Type 0x2A tests DAT_00512418. Zero calls 0x0013E8D0 with EBX = node+8,
// EDX = the dword at node+0x88, and node+0x8c on the stack.
// A zero dword at node+0x10c returns 2. Any other value stores 1
// and returns 0. Nonzero calls 0x001412F0; a ready load clears the flag and
// returns 2, while an unfinished load returns 0.
//
// Type 0x2B calls 0x000F4E50 with EAX = node+8 (the scene queues the record). That call is
// translated. The case returns 2.
//
// Type 0x51 calls 0x001077B0 with EAX = node+8. It walks DAT_00511B38. A
// matching word at object+6, and a zero dword at node+0x48 or that dword
// equal to the word at object+0x1FC, calls vtable +0xC with message id 1.
// An empty list returns 2. A finished walk returns 2.
//
// Type 0x58 builds the message {0x65, word}, the word from 0x0002B710 (the
// actorStrategyId named at node+0x48), calls 0x001077B0 with EAX = node+8,
// and passes that tag in EDI with the message in EBX to 0x00122D40, which
// sends it to each DAT_00511B38 actor with that tag. The case returns 2.
//
// Type 0x5C stores message id 0x36, calls 0x001077B0 with EAX = node+8, and
// passes that tag in EDI with the message in EBX to 0x00122D40, which sends
// it to each DAT_00511B38 actor with that tag. The case returns 2.
//
// Type 0x5E calls 0x001077B0 with EAX = node+8 and passes the tag, the
// dword at node+0x4c and XMM0 (the float at node+0x48) to 0x000F4930, the
// ghoul intro's caption. The case returns 2.
//
// Type 0x6B calls 0x001077B0 with EAX = node+8 and walks DAT_00511B48. A tag
// below 2, or a word at object+6 equal to the tag, stores the byte at
// node+0x48 into object+0x969. A live object+0x958 whose +0x10 is live and
// that 0x00029C80 (EDX 1) no longer accepts sends {0x64, 0} through its +8's
// vtable +0xC and clears +0x10. The case returns 2.
//
// Type 0x70 calls 0x001077B0 with EAX = node+8 and passes that tag in ESI,
// XMM1 the float at node+0x48 and XMM2 the float at node+0x4c to 0x0010F6C0.
// The case returns 2.
//
// Type 0x78 calls 0x001077B0 with EAX = node+8 and walks DAT_00511B48. A tag
// below 2, or a word at object+6 equal to the tag, calls 0x00019660 with EAX
// = the object and the dword at node+0x48 on the stack. The case returns 2.
//
// Type 0x97 calls 0x001077B0 with EAX = node+8 and walks DAT_00511B48. A tag
// below 2, or a word at object+6 equal to the tag, drains the goals
// (0x00028210), clears object+0x990 and sends message 0x4D with the dword at
// node+0x48, the world position (0x000412A0) and euler (0x00041820) through
// vtable +0xC. The case returns 2.
// Xbox 0x00126A10
int script_handle_actor_actions(Game& g, const ScriptNode& node)
{
    switch (node.type)
    {
    case 0x2a:
        if (g_flag_2418 == 0)
        {
            const std::string name = node_cstr(g.scene_script, node, 8);
            const u32 key = node_u32(g.scene_script, node, 0x88);
            const std::string extra = node_cstr(g.scene_script, node, 0x8c);
            request_ghoulybox(name.c_str(), key, extra.c_str());
            if (node_u32(g.scene_script, node, 0x10c) == 0)
            {
                return 2;
            }
            g_flag_2418 = 1;
            return 0;
        }
        if (actor_loading_complete())
        {
            g_flag_2418 = 0;
            return 2;
        }
        return 0;
    case 0x2b:
    {
        SceneActorServices* actors = script_scene_actors(g);
        if (actors == nullptr)
        {
            throw Error("script type 0x2B without the native scene's resources (0x000F4E50)");
        }
        actors->queue_aid_spawns(node_cstr(g.scene_script, node, 8).c_str()); // 0x000F4E50, EAX node+8
        return 2;
    }
    case 0x51:
    {
        // Message 1 to each DAT_00511B38 actor (node - 0x934) of the tag whose +0x1FC word is node+0x48, or any.
        const u16 tag = static_cast<u16>(script_find_actor_tag(node_cstr(g.scene_script, node, 8).c_str()));
        const u32 mark = node_u32(g.scene_script, node, 0x48);
        SceneActorServices* actors = script_scene_actors(g);
        if (actors == nullptr)
        {
            throw Error("script type 0x51 without the native scene's actors (DAT_00511B38)");
        }
        for (const auto& marked : actors->marked_actors())
        {
            if (read_u16(marked.bytes, 6) != tag)
            {
                continue;
            }
            if (mark != 0 && mark != u32(read_u16(marked.bytes, 0x1FC)))
            {
                continue;
            }
            const std::array<u32, 1> message{1};
            actors->send_actor_message(marked.address, message); // Vtable +0xC.
        }
        return 2;
    }
    case 0x58:
    {
        std::array<u32, 0x42> message{0x65, find_actor_strategy_id(node_cstr(g.scene_script, node, 0x48).c_str())}; // 0x0002B710
        const u16 tag = static_cast<u16>(script_find_actor_tag(node_cstr(g.scene_script, node, 8).c_str()));
        if (SceneActorServices* actors = script_scene_actors(g); actors != nullptr)
        {
            message_runtime_marked_actors(*actors, tag, message); // 0x00122D40
        }
        return 2;
    }
    case 0x5c:
    {
        std::array<u32, 0x42> message{0x36};
        const u16 tag = static_cast<u16>(script_find_actor_tag(node_cstr(g.scene_script, node, 8).c_str()));
        if (SceneActorServices* actors = script_scene_actors(g); actors != nullptr)
        {
            message_runtime_marked_actors(*actors, tag, message); // 0x00122D40
        }
        return 2;
    }
    case 0x5e:
    {
        const u16 tag = static_cast<u16>(script_find_actor_tag(node_cstr(g.scene_script, node, 8).c_str()));
        if (!g_script_ghoul_intro_caption)
        {
            throw Error("script node 0x5E queues a ghoul intro caption outside a native scene");
        }
        g_script_ghoul_intro_caption(tag, std::bit_cast<float>(node_u32(g.scene_script, node, 0x48)),
            std::bit_cast<float>(node_u32(g.scene_script, node, 0x4c))); // 0x000F4930, XMM0 node+0x48
        return 2;
    }
    case 0x6b:
    {
        // Each DAT_00511B48 actor of the tag (any for a tag below 2) takes the byte at node+0x48 at +0x969. One whose
        // mind (+0x958) has a target (+0x10) that 0x00029C80 no longer accepts tells its actor {0x64, 0} and drops
        // the target.
        const u16 tag = static_cast<u16>(script_find_actor_tag(node_cstr(g.scene_script, node, 8).c_str()));
        for (const auto* link = static_cast<const entity_detail::ListHead*>(entity_detail::g_world.alt.next);
             link != nullptr; link = static_cast<const entity_detail::ListHead*>(link->next))
        {
            if (!g_alt_node_class || !g_alt_node_actor_bytes || !g_alt_node_mind || !g_alt_node_target_eligible
                || !g_alt_node_message)
            {
                throw Error("script type 0x6B walks DAT_00511B48 without the scene's actor views");
            }
            if (tag >= 2 && g_alt_node_class(link) != tag)
            {
                continue;
            }
            const std::span<u8> actor = g_alt_node_actor_bytes(link);
            player_record(actor).provoked = node_u8(g.scene_script, node, 0x48);
            const std::span<u8> mind = g_alt_node_mind(link);
            if (player_record(actor).mind != 0 && !mind.empty() && mind_record(mind).target_record != 0
                && !g_alt_node_target_eligible(link)) // 0x00029C80, EDX 1
            {
                if (mind_record(mind).target_record != 0)
                {
                    const std::array<u32, 2> message{100, 0};
                    g_alt_node_message(link, message); // Mind +8 (the actor) vtable +0xC.
                }
                std::memset(mind.data() + 0x10, 0, 4);
            }
        }
        return 2;
    }
    case 0x70:
    {
        // 0x0010F6C0, SI the tag, XMM1 node+0x48, XMM2 node+0x4C: the tag's strategy class range.
        const u16 tag = static_cast<u16>(script_find_actor_tag(node_cstr(g.scene_script, node, 8).c_str()));
        set_runtime_strategy_class_range(tag, std::bit_cast<float>(node_u32(g.scene_script, node, 0x48)),
            std::bit_cast<float>(node_u32(g.scene_script, node, 0x4c)), g_strategy_class_ranges, g_strategy_class_range_count);
        return 2;
    }
    case 0x78:
    {
        // 0x00019660 (EAX the actor, the dword at node+0x48 on the stack) on each DAT_00511B48 actor of the tag, or
        // every one for a tag below 2.
        const u16 tag = static_cast<u16>(script_find_actor_tag(node_cstr(g.scene_script, node, 8).c_str()));
        const u8 value = node_u8(g.scene_script, node, 0x48);
        for (const auto* link = static_cast<const entity_detail::ListHead*>(entity_detail::g_world.alt.next);
             link != nullptr; link = static_cast<const entity_detail::ListHead*>(link->next))
        {
            if (!g_alt_node_class || !g_alt_node_actor || !g_alt_node_actor_bytes || !g_script_actor_message)
            {
                throw Error("script type 0x78 walks DAT_00511B48 without the scene's actor views");
            }
            if (tag >= 2 && g_alt_node_class(link) != tag)
            {
                continue;
            }
            const std::span<u8> actor = g_alt_node_actor_bytes(link);
            player_record(actor).unk_96D = value;
            player_record(actor).unk_96F = 0;
            player_record(actor).unk_970 = 0;
            if (player_record(actor).countdown_55 > 0.f)
            {
                std::memset(actor.data() + offsetof(PlayerRecord, countdown_55), 0, 4);
                std::array<u32, 0x42> message{0x55};
                g_script_actor_message(g_alt_node_actor(link), message); // Vtable +0xC
            }
        }
        return 2;
    }
    case 0x97:
    {
        // Each DAT_00511B48 actor of the tag (any for a tag below 2): its goals drained (0x00028210), +0x990 cleared
        // and message 0x4D with the dword at node+0x48 and its world position and euler (0x000412A0, 0x00041820).
        const u16 tag = static_cast<u16>(script_find_actor_tag(node_cstr(g.scene_script, node, 8).c_str()));
        for (const auto* link = static_cast<const entity_detail::ListHead*>(entity_detail::g_world.alt.next);
             link != nullptr; link = static_cast<const entity_detail::ListHead*>(link->next))
        {
            if (!g_alt_node_class || !g_alt_node_actor || !g_alt_node_actor_bytes || !g_script_actor_message
                || !g_script_drain_actor_goals)
            {
                throw Error("script type 0x97 walks DAT_00511B48 without the scene's actor views");
            }
            if (tag >= 2 && g_alt_node_class(link) != tag)
            {
                continue;
            }
            Actor& actor = g_alt_node_actor(link);
            g_script_drain_actor_goals(actor); // 0x00028210, ESI the actor
            std::memset(g_alt_node_actor_bytes(link).data() + offsetof(PlayerRecord, goal_latch), 0, 2);
            const Vec3 position = actor_world_position(actor); // 0x000412A0
            const Vec3 euler = actor_world_euler(actor); // 0x00041820
            std::array<u32, 0x42> message{0x4d, node_u32(g.scene_script, node, 0x48), std::bit_cast<u32>(position.x),
                std::bit_cast<u32>(position.y), std::bit_cast<u32>(position.z), std::bit_cast<u32>(euler.x),
                std::bit_cast<u32>(euler.y), std::bit_cast<u32>(euler.z), 0};
            g_script_actor_message(actor, message); // Vtable +0xC
        }
        return 2;
    }
    default:
        return 3;
    }
}


// Xbox 0x00126E10. Handler 11. The dword at node+4 is the type. The byte map
// at 0x001271F0 covers types 0x4D through 0x9E. Any other type, and the holes
// in that range, return 3. Every handled type returns 2. The second argument
// is the flag 0x00128A00 passes: nonzero when a DAT_0050E4DC node has 0x7D
// at +8.
//
// Type 0x4D loads the word at node+8 into DX for 0x00129ED0. A hit takes that
// actor's world euler (0x00041820) and position (0x000412A0), adds the float at
// node+0x8C times 0.017453292 (0x0032B0FC) to the second euler float, and clones
// the template named at node+0xC there (0x00112C50, its +0x38 as the scale). ESI
// still holds the actor found, so 0x00112CC0 activates that actor, not the clone.
//
// Type 0x4E draws 0x00019200 against the float at node+0x10 times 0.01
// (0x0032B124) with ECX = 0x0054DD7C. A hit appends the words at node+8 and
// node+0xC as a DAT_00545C48 pair (0x001400A0).
//
// Type 0x60 loads the word at node+8 into DX for 0x00129ED0. A hit calls
// 0x00112CC0 with ESI set to that object.
//
// Type 0x62 calls 0x0011F1C0(0x13, 1) with AX 0. A nonzero AX walks the
// records at DAT_0051108C+8, comparing the word at record+6 with AX and the
// dword at record+0x28 with the dword at node+8. A hit calls 0x0013D250 with
// record+0xC and (0 - record+0x18, record+0x1C + pi, record+0x20). A miss
// calls 0x0011F1C0(0x13, 0) with AX the current id and stops when AX matches
// the first return or AX is 0.
//
// Type 0x67 with a zero second argument stores the byte at node+8 and the
// word at node+0xC into DAT_00512420 and increments DAT_00512440. Nonzero
// with a zero dword at node+0xC calls 0x0012C8A0 with the byte at node+8,
// 0x0012C950 and 4, which opens that category's collision events to the
// player. Any other dword calls 0x0012C8A0 with 0x0012C930 and 4, then
// 0x00122CE0 with EDI pointing at message id 0x4A.
//
// Type 0x6E calls 0x001249B0 with EDI = the dword at node+8 and EAX = 8.
//
// Type 0x73 calls 0x001400D0. CX is the word at node+8. The nine stack words
// are node+0xC through node+0x2C: one DAT_00545CCC group.
//
// Type 0x74 calls 0x00115F40 with EDI pointing at message id 1: each active
// scene control (DAT_0050E438) gets it through vtable +0xC.
//
// Type 0x8E calls 0x001189D0. XMM0 is the float at node+0xC, ECX is the
// dword at node+8, EAX points at node+0x1C, and the stack dwords are node+0x10,
// node+0x14, and node+0x18.
//
// Type 0x9D calls 0x00049D10. A zero return loads the dword at node+8 into ECX
// for 0x000F2A20. A nonzero return from that call increments DAT_00506C84 and
// calls 0x0012D7E0 with EDI = 1 and that count on the stack.
//
// Type 0x9E with a nonzero dword at node+0x10 stores the dword at node+8 at
// DAT_00512448 + DAT_00512444*4, and 0x000F29A0 (ECX that dword) stores its
// result in the parallel DAT_0051245C slot. DAT_00512444 is then incremented.
// A nonzero dword at node+0xC calls 0x000F29D0; zero calls 0x000F29F0. Both
// take ECX = the dword at node+8.
// Xbox 0x00126E10
int script_handle_resource_actions(Game& g, const ScriptNode& node, int live_7d)
{
    switch (node.type)
    {
    case 0x4d:
    {
        SceneActorServices* actors = script_scene_actors(g);
        if (actors == nullptr)
        {
            throw Error("script type 0x4D without the native scene's actors (0x00129ED0)");
        }
        const u32 address = actors->find_actor_by_id(node_u16(g.scene_script, node, 8));
        if (address == 0)
        {
            return 2;
        }
        const auto found = actors->actor_addresses().find(address);
        if (found == actors->actor_addresses().end() || found->second == nullptr)
        {
            throw Error("script type 0x4D found an actor with no native view");
        }
        const Actor& actor = *found->second;
        Vec3 euler = actor_world_euler(actor); // 0x00041820
        const Vec3 position = actor_world_position(actor); // 0x000412A0
        euler.y = std::bit_cast<float>(node_u32(g.scene_script, node, 0x8c)) * std::bit_cast<float>(0x3c8efa35u)
            + euler.y; // DAT_0032B0FC
        const std::string name = node_cstr(g.scene_script, node, 0xc);
        actors->create_template_actor(name.c_str(), position, euler, actor.radius_xz); // 0x00112C50, actor +0x38
        actors->activate_actor(address); // 0x00112CC0, ESI the actor found
        return 2;
    }
    case 0x4e:
    {
        const float chance = std::bit_cast<float>(node_u32(g.scene_script, node, 0x10))
            * std::bit_cast<float>(0x3c23d70au); // DAT_0032B124
        if (actor_event_chance(g_shared_random, chance)) // 0x00019200, ECX 0x0054DD7C
        {
            // 0x001400A0, CX the word at node+8 and the word at node+0xC on the stack.
            const size_t slot = size_t(g_marker_swap_count) * 2;
            if (slot + 1 >= g_marker_swap_ids.size())
            {
                throw Error("script type 0x4E overflows DAT_00545C48's pairs");
            }
            g_marker_swap_ids[slot] = node_u16(g.scene_script, node, 8);
            g_marker_swap_ids[slot + 1] = node_u16(g.scene_script, node, 0xc);
            g_marker_swap_count = g_marker_swap_count + 1;
        }
        return 2;
    }
    case 0x60:
    {
        // 0x00129ED0 (DX the word at +8), then 0x00112CC0 on the actor it finds.
        SceneActorServices* actors = script_scene_actors(g);
        if (actors == nullptr)
        {
            throw Error("script type 0x60 without the native scene's actors (0x00129ED0)");
        }
        const u32 actor = actors->find_actor_by_id(node_u16(g.scene_script, node, 8));
        if (actor != 0)
        {
            actors->activate_actor(actor);
        }
        return 2;
    }
    case 0x62:
    {
        // The type-0x13 marker whose dword at +0x28 is the dword at node+8 places the scene camera (0x0013D250) at
        // its +0xC position, facing (0 - +0x18, +0x1C + pi, +0x20).
        SceneActorServices* actors = script_scene_actors(g);
        if (actors == nullptr)
        {
            throw Error("script type 0x62 without the scene's marker records");
        }
        const u8* records = actors->marker_records();
        const u16 first = next_runtime_marker_id(0, 0x13, 1, records); // 0x0011F1C0, AX 0
        if (first == 0)
        {
            return 2;
        }
        const u32 wanted = node_u32(g.scene_script, node, 8);
        u16 id = first;
        while (true)
        {
            const u8* record = find_runtime_marker(records, i16(id));
            if (record == nullptr)
            {
                throw Error("script type 0x62 reads +0x28 of a missing marker (address zero)");
            }
            if (read_u32({record, 0x2C}, 0x28) == wanted)
            {
                const std::span<const u8> row{record, 0x24};
                const Vec3 position{read_f32(row, 0xC), read_f32(row, 0x10), read_f32(row, 0x14)};
                const Vec3 euler{0.f - read_f32(row, 0x18),
                    read_f32(row, 0x1C) + std::bit_cast<float>(0x40490FDBu), // DAT_0032B148
                    read_f32(row, 0x20)};
                actors->place_scene_camera(position, euler); // 0x0013D250
                return 2;
            }
            id = next_runtime_marker_id(id, 0x13, 0, records); // AX the current id
            if (id == first || id == 0)
            {
                return 2;
            }
        }
    }
    case 0x67:
        if (live_7d == 0)
        {
            const size_t slot = size_t(g_gate_2440);
            g_gate_2420.resize(slot + 1);
            g_gate_2420[slot].at_8 = node_u8(g.scene_script, node, 8);
            g_gate_2420[slot].at_c = node_u16(g.scene_script, node, 0xc);
            g_gate_2440 = g_gate_2440 + 1;
            return 2;
        }
    {
        SceneActorServices* actors = script_scene_actors(g);
        if (actors == nullptr)
        {
            throw Error("script type 0x67 without the native scene's actors (0x0012C8A0)");
        }
        const u8 category = node_u8(g.scene_script, node, 8);
        if (node_u32(g.scene_script, node, 0xc) == 0)
        {
            actors->change_script_shape_events(category, false); // 0x0012C8A0 with 0x0012C950
            return 2;
        }
        actors->change_script_shape_events(category, true); // 0x0012C8A0 with 0x0012C930
        const std::array<u32, 1> message{0x4A};
        broadcast_actor_world_message(message); // 0x00122CE0, EDI the message
        return 2;
    }
    case 0x6e:
        clear_occupancy_value_mask(node_u32(g.scene_script, node, 8), 8); // 0x001249B0, EDI the dword, EAX 8
        return 2;
    case 0x73:
    {
        // 0x001400D0: CX the word at node+8, then node+0xC..node+0x2C, as one ten-word DAT_00545CCC group.
        const size_t group = size_t(g_marker_group_count) * 10;
        if (group + 10 > g_marker_group_ids.size())
        {
            throw Error("script type 0x73 overflows DAT_00545CCC's groups");
        }
        for (size_t word = 0; word < 10; ++word)
        {
            g_marker_group_ids[group + word] = node_u16(g.scene_script, node, 8 + u32(word) * 4);
        }
        g_marker_group_count = g_marker_group_count + 1;
        return 2;
    }
    case 0x74:
    {
        // 0x00115F40, EDI the one-word stack message {1}: vtable +0xC of each DAT_0050E438 scene control.
        if (g_scene_control_owner == nullptr)
        {
            throw Error("script type 0x74 without the scene's controls");
        }
        const std::array<u32, 1> message{1};
        for (RuntimeActorLink* link = g_scene_controls.active.next; link != nullptr;)
        {
            RuntimeActorLink* next = link->next;
            g_scene_control_owner->message(g_scene_control_owner->instance_of(*link), message);
            link = next;
        }
        return 2;
    }
    case 0x8e:
    {
        const std::string name = node_cstr(g.scene_script, node, 0x1c);
        start_ambient_phase(node_u32(g.scene_script, node, 8),
            std::bit_cast<float>(node_u32(g.scene_script, node, 0xc)),
            std::bit_cast<float>(node_u32(g.scene_script, node, 0x10)),
            std::bit_cast<float>(node_u32(g.scene_script, node, 0x14)), node_u32(g.scene_script, node, 0x18),
            name.c_str(), native_audio_runtime().voices); // 0x001189D0
        return 2;
    }
    case 0x9d:
    {
        if (scene_end_flag_set()) // 0x00049D10
        {
            return 2;
        }
        // 0x000F2A20, ECX the dword at node+8: the story bit, if it was clear, is set.
        const u32 bit = node_u32(g.scene_script, node, 8);
        if (test_runtime_story_bit(g_runtime_profile.progress.bytes, bit))
        {
            return 2;
        }
        set_runtime_story_bit(g_runtime_profile.progress.bytes, bit);
        SceneActorServices* actors = script_scene_actors(g);
        if (actors == nullptr)
        {
            throw Error("script type 0x9D without the native scene's counters (0x0012D7E0)");
        }
        actors->count_script_challenge(); // DAT_00506C84, 0x0012D7E0
        return 2;
    }
    case 0x9e:
    {
        // ECX is the bit at +8 of the story block (*(DAT_00506C6C + 8)).
        const u32 bit = node_u32(g.scene_script, node, 8);
        if (node_u32(g.scene_script, node, 0x10) != 0)
        {
            const size_t slot = size_t(g_mark_2444);
            g_mark_2448.resize(slot + 1);
            g_mark_245c.resize(slot + 1);
            g_mark_2448[slot] = bit;
            g_mark_245c[slot] = test_runtime_story_bit(g_runtime_profile.progress.bytes, bit) ? 1 : 0; // 0x000F29A0
            g_mark_2444 = g_mark_2444 + 1;
        }
        if (node_u32(g.scene_script, node, 0xc) != 0)
        {
            set_runtime_story_bit(g_runtime_profile.progress.bytes, bit); // 0x000F29D0
            return 2;
        }
        clear_runtime_story_bit(g_runtime_profile.progress.bytes, bit); // 0x000F29F0
        return 2;
    }
    default:
        return 3;
    }
}


// DAT_0050E4E0. Name bytes, 0x80 long. The next dword is DAT_0050E560.
char g_name_e4e0[0x80]{};

i32 g_flag_e560 = 0;


// DAT_00512470. Type 0x13 sets this when the dword at node+0x88 is nonzero.
// Type 0x16 sets it when the dword at node+0x8C is nonzero. Type 0x53 sets it
// on the first visit.
i32 g_flag_2470 = 0;

// DAT_00512474. The native type-0x14 context shares this latch.
i32 g_flag_2474 = 0;

// DAT_0051248C. Type 0x13 compares this dword with 3 and would pass that
// flag twice to 0x0004EDA0. The byte at [DAT_0051248C]+0x30F144 is g_decay_arm.
i32 g_state_248c = 0;

i32 g_state_2490 = 0;

// DAT_004EE1EC. Type 0x14 returns 0 while this is nonzero. The text writer
// in entity.cpp stores 0.
i32 g_draw_e1ec = 0;


// DAT_00512478 / DAT_0051247C: the skip input 0x00127AC0 activates for a mode-3 cutscene.
i32 g_cutscene_skip_input = 0;
u32 g_cutscene_skip_entry = 0;

// Xbox 0x0004EFE0
bool scene_cutscene_finished(const SceneCutsceneRuntime& state)
{
    if (-1 < state.index)
    {
        const auto& instance = *state.instances[size_t(state.index)];
        if (instance.duration <= instance.time)
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x000F2BB0, ECX the bit.
bool scene_flag_bit_set(u32 bit)
{
    return (g_scene_flags->bytes[bit >> 3] & (1u << (bit & 7))) != 0;
}

// Xbox 0x00087830. 0x00123A30 reads the scene record DAT_00510E58 against the overrides at 0x00430DA0.
i32 scene_player_character(const Game& g)
{
    i32 character = i32(find_player_scene_override(g.scene_script.name.c_str(),
        std::span<const u8, 0x210>(g.executable_resources->memory(0x00430DA0, 0x210).data(), 0x210)));
    if (character == 0)
    {
        if ((g_scene_flags->bytes[0] & 8) != 0 || (g_flags_ea9c & 0x200000u) != 0)
        {
            return 2;
        }
        character = 1;
    }
    return character;
}

// Xbox 0x00127AC0
void begin_cutscene_skip_input(Game& g)
{
    g_cutscene_skip_input = 0;
    g_cutscene_skip_entry = 0;
    if (g_state_248c == 3 && (g_flags_ea9c & 0x1000u) == 0)
    {
        // The scene's input stacks (DAT_0050E0E8, the devices) through the dialogue's bound contexts.
        auto& input = scene_dialogue_context(g).dialogue;
        const std::span<u8> device = create_runtime_selected_input(input.input_release); // 0x00107D60, ESI DAT_0051247C
        // Native pointer mapping; the retail DAT_0051247C holds the allocated address.
        g_cutscene_skip_entry = static_cast<u32>(reinterpret_cast<std::uintptr_t>(device.data()));
        input.input_addresses[g_cutscene_skip_entry] = device;
        input.input_views[g_cutscene_skip_entry] = {device, {nullptr, nullptr, nullptr, nullptr}};
        push_runtime_input_entry(g_cutscene_skip_entry, "cutscene skip", input.input); // 0x00109340, ESI 0x0032A8C0
        g_cutscene_skip_input = 1;
    }
}

// Xbox 0x00127B10
bool cutscene_skip_pressed(Game& g)
{
    if (g_cutscene_skip_input != 0)
    {
        auto& input = scene_dialogue_context(g).dialogue;
        refresh_runtime_input_device(g_cutscene_skip_entry, input.frame, input.input, input.input_release);
        if ((g_flags_ea9c & 0x40u) == 0)
        {
            return runtime_input_button_edge(input.input_views.at(g_cutscene_skip_entry), 0x10,
                input.widget_update.input) != 0; // 0x001083F0
        }
    }
    return false;
}

// Xbox 0x00127B50
void end_cutscene_skip_input(Game& g)
{
    if (g_cutscene_skip_input != 0)
    {
        auto& input = scene_dialogue_context(g).dialogue;
        pop_runtime_input_entry(input.input); // 0x001093C0
        const std::span<u8> device = input.input_addresses.at(g_cutscene_skip_entry);
        switch (read_u32(device, 0))
        {
        case 1:
            release_runtime_simple_input(device); // 0x00107DD0
            break;
        case 2:
            release_runtime_simple_input(device);
            break;
        case 3:
            release_runtime_simple_input(device);
            break;
        case 4:
            release_runtime_composite_input(device, input.input_release); // 0x00107960
            break;
        case 5:
            release_runtime_actor_input(device, input.input_release); // 0x00107B60
            break;
        }
        input.input_views.erase(g_cutscene_skip_entry);
        input.input_addresses.erase(g_cutscene_skip_entry);
        g_cutscene_skip_entry = 0;
        g_cutscene_skip_input = 0;
    }
}

// Xbox 0x000532E0. A final cutscene followed by the end gate keeps its
// instance alive for the scene transition; other cutscenes release it.
// Xbox 0x000532E0
bool cutscene_retained_for_transition(const SceneScript& script, const ScriptNode& current)
{
    if (current.type != 0)
    {
        auto next = script_node_at(script, current.link);
        if (!next)
        {
            throw Error("script end test outside resource");
        }
        if (next->type != 0)
        {
            if (next->type != 0xf)
            {
                return false;
            }
            auto end = script_node_at(script, next->link);
            if (!end)
            {
                throw Error("script end gate outside resource");
            }
            return end->type == 0;
        }
    }
    return true;
}


// Xbox 0x00127340. Handler 12. The dword at node+4 is the type. The byte map
// at 0x00127458 covers types 0x3A through 0x8D. Any other type, and the holes
// in that range, return 3. Every handled type returns 2. The second and third
// arguments from 0x00128A00 are unused.
//
// Type 0x3A plays the stream named at node+8 through 0x00116D20 with priority 3.
//
// Type 0x3B releases the current music stream DAT_0050E4E0 when there is one
// (0x00116D20's first half, inlined in retail) and returns 2.
//
// Type 0x45 tests DAT_0050E560. Nonzero falls through and returns 2. Zero
// calls 0x00116C90, stores 1, and returns 2.
//
// Type 0x55 looks up the actor tag named at node+8 (0x001077B0) and plays its
// fight music (0x00116C50).
//
// Type 0x57 plays the stream named at node+8 through 0x00116D20, priority 2.
//
// Type 0x8D queues the stream named at node+8 (0x00038720) with id 6, then
// falls through and returns 2.
// Xbox 0x00116C90
void create_scene_ambient_tune(Game& g)
{
    // A 0x80-byte name buffer, zero past the string; 0x00107720 clones, 0x00107520 creates, the clone is freed.
    std::array<char, 0x80> name{};
    std::strcpy(name.data(), "aid_objparams_ghoulies_scenecontrol_ambient_tune");
    if (g.factory_scene == nullptr || g.executable_resources == nullptr)
    {
        throw Error("ambient tune creation without the scene's object factory");
    }
    ClonedExecutableResource parameters = clone_executable_resource(g.executable_resources->pools, name.data());
    if (parameters.bytes == nullptr)
    {
        throw Error("ambient tune objparams are missing; 0x00107520 reads the null clone");
    }
    FactoryObject object = create_factory_object(g.factory_scene->factory, {parameters.bytes.get(), parameters.size});
    if (object.bytes != nullptr)
    {
        g.factory_scene->keep_object(std::move(object));
    }
}

// Xbox 0x00116E00. The scene-start reset of the script's runtime: the two pause counts (DAT_0050E764, DAT_0050E768;
// the last scene's teardown can release a pause it no longer holds), the entry pose flag and the VM words
// (DAT_0050E784..DAT_0050E79C, DAT_0050E798 4), the stream the last scene left in DAT_0050E4E0 (released inline, as
// 0x00116D20's first half does), the spawn words (0x00116DA0), the live script list (DAT_0050E4D8, DAT_0050E4DC), a
// new DAT_00512B74 pool, the event sequence (DAT_005064D8..), the DAT_0052C274 statistics (0x0012AF10), the score
// words (DAT_00545970..DAT_00545980), the challenge's words (DAT_004E17C8 1, DAT_004E17CC -1, DAT_004E17D0,
// DAT_004E19A8, DAT_004E19A0, DAT_004E19A4, DAT_004E1A34), the script name (0x001287F0) and a new DAT_0051234C pool.
// The teardown (0x00117330) has already freed both pools and the live list, so the new pools start empty.
void reset_scene_script_runtime(Game& g)
{
    scene_detail::g_pause_references = {}; // DAT_0050E764, DAT_0050E768
    g_scene_entry_set = 0; // DAT_0050E784
    g_script_e788 = 0; // DAT_0050E788
    g_script_e78c = 0; // DAT_0050E78C
    g_script_e798 = 4; // DAT_0050E798
    g_script_e79c = 0; // DAT_0050E79C
    g_script_e794 = 0; // DAT_0050E794
    if (g_name_e4e0[0] != 0)
    {
        play_scene_music_stream(nullptr, 0, g_name_e4e0, native_audio_runtime().streams);
    }
    reset_scene_spawn_words(); // 0x00116DA0
    g_live_anchor.prev = nullptr; // DAT_0050E4D8
    g_live_scripts = nullptr; // DAT_0050E4DC
    g_ghouly_controllers.clear(); // DAT_00512B74, stride 8, growth 10
    g_event_sequence_state = 0; // DAT_005064D8
    g_event_sequence.door = 0; // DAT_005064DC
    g_event_sequence.unk_52c = 0; // DAT_0050652C
    g_event_sequence.door_delay = 0.f; // DAT_00506534
    g_event_sequence.icon_delay = 0.f; // DAT_00506538
    g_event_sequence.waiting = 0; // DAT_0050653C
    script_initialize_statistics(g_script_tallies.first); // 0x0012AF10, EAX DAT_0052C274
    if (g.factory_scene == nullptr || g.factory_scene->actor_services == nullptr)
    {
        throw Error("0x00116E00 clears DAT_00545970..DAT_0054597C without the boot scene services");
    }
    g.factory_scene->actor_services->reset_scene_score_words(); // DAT_00545970, DAT_00545974, DAT_0054597C
    g_broken_background_parts = 0; // DAT_00545978
    g_running_score = 0; // DAT_00545980
    auto& challenge = g_challenge_run;
    challenge.state = 1; // DAT_004E17C8
    challenge.kind = 0xFFFFFFFFu; // DAT_004E17CC
    challenge.mode = 0; // DAT_004E17D0
    challenge.choice = 0; // DAT_004E19A8
    challenge.widget = nullptr; // DAT_004E19A0
    challenge.input = {}; // DAT_004E19A4
    challenge.input_address = 0;
    challenge.reset_notice = 0; // DAT_004E1A34
    script_set_default_name(g); // 0x001287F0
    g_script_conds.clear(); // DAT_0051234C, stride 0x20, growth 2
}

// The native scene's bank streams (DAT_004DF370..), bound once its actor services start.
AudioBankStreams& scene_music_streams(Game& g)
{
    if (g.factory_scene == nullptr || g.factory_scene->controls == nullptr)
    {
        throw Error("scene music without the native scene's bank streams");
    }
    return g.factory_scene->controls->context.streams();
}

// Xbox 0x00127340
int script_handle_event_actions(Game& g, const ScriptNode& node)
{
    switch (node.type)
    {
    case 0x3a:
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        play_scene_music_stream(key.c_str(), 3, g_name_e4e0, scene_music_streams(g)); // 0x00116D20
        return 2;
    }
    case 0x3b:
        if (g_name_e4e0[0] != 0)
        {
            // The release half of 0x00116D20, inlined here.
            play_scene_music_stream(nullptr, 0, g_name_e4e0, scene_music_streams(g));
            return 2;
        }
        break;
    case 0x45:
        if (g_flag_e560 == 0)
        {
            create_scene_ambient_tune(g);
            g_flag_e560 = 1;
            return 2;
        }
        break;
    case 0x55:
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        const u32 tag = script_find_actor_tag(key.c_str()); // 0x001077B0
        play_scene_fight_music(i16(tag), g.executable_resources->memory(0x0030E490, 0xCB2), g_name_e4e0,
            scene_music_streams(g)); // 0x00116C50
        return 2;
    }
    case 0x57:
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        play_scene_music_stream(key.c_str(), 2, g_name_e4e0, scene_music_streams(g)); // 0x00116D20
        return 2;
    }
    case 0x8d:
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        request_audio_stream(key.c_str(), 6, scene_music_streams(g)); // 0x00038720
        break;
    }
    default:
        return 3;
    }
    return 2;
}


// Xbox 0x001274B0. Handler 13. The dword at node+4 is the type. The byte map
// at 0x00127A2C covers types 0x13 through 0x9C. Slots are 0x13, 0x14, 0x15,
// 0x16, 0x53, 0x9B, and 0x9C. Any other type returns 3. The second argument
// from 0x00128A00 is unused. The third argument is the value that walk passes.
//
// Type 0x13 starts the named cutscene and waits for its instance to finish.
// A terminal cutscene keeps the instance alive through the scene transition.
// The optional skip widget is still owned by the untranslated UI path.
//
// Type 0x14 delegates to execute_runtime_script_dialogue, which translates
// the tag, end-flag, once-only, variant and enqueue branches. Activation,
// update and draw share the scene binding; missing ownership is an error.
// A nonzero third argument returns 2 before looking up that binding.
//
// Type 0x15 calls 0x00106D50 with EDI = node+8, the byte at node+0x88, the
// dword at node+0x8C and 1, then 0x00106DD0 with DL = that byte. The case
// returns 2.
//
// Type 0x16 with a nonzero third argument returns 2. Nonzero DAT_00512470
// calls 0x0004EFE0. A zero return returns 0. A nonzero return clears the
// flag, calls 0x0004ECE0, and returns 2. A zero flag with a nonzero dword at
// node+0x8C stores 1 and returns 0. Otherwise the case returns 2.
//
// Type 0x53 plays the player's walk-in cutscene (node+8) at the first kind 0xC marker, turned by pi, with the
// relic candle's scene control while it plays; when it ends (unless it is the scene's last action) the player marker
// runs again (0x00127FF0) and the candle's control is disposed.
//
// Type 0x9B calls 0x000FB120 with 1.0, EAX = node+8, and ECX = a stack
// vector. Type 0x9C calls 0x000493D0 with EDX = node+8 and EAX = that
// vector. DAT_00511B40 null, or the sentinel 0x93C, zeros the vector; any
// other pointer calls 0x000412A0 with ESI = that pointer minus 0x93C and EDI
// = the vector. Both cases return 2.
// DAT_00512480: the relic candle's scene control while a walk-in cutscene plays, as its retail address.
u32 g_walk_in_candle = 0;

// Script type 0x53 with a zero third argument, in 0x001274B0.
int play_script_walk_in_cutscene(Game& g, const ScriptNode& node)
{
    auto& cutscenes = scene_cutscene_runtime(g);
    SceneActorServices* actors = script_scene_actors(g);
    if (actors == nullptr)
    {
        throw Error("script walk-in cutscene (type 0x53) without the native scene's actors");
    }
    if (g_flag_2470 == 0)
    {
        const std::string name = node_cstr(g.scene_script, node, 8);
        RuntimeCutsceneInstance* instance = start_scene_cutscene(name.c_str(), 0, 1, cutscenes); // 0x0004EDA0
        auto& runtime = cutscenes.bindings.runtime;
        const auto& view = runtime.views.at(instance->resource);
        auto& rows = runtime.instances.at(instance);
        Actor* relic = find_cutscene_actor_by_model(*instance, view.tracks.data(), rows.actors.data(),
            "aid_model_ghoulies_cutscene_reliccandle"); // 0x0004EA50
        if (relic == nullptr)
        {
            g_walk_in_candle = 0;
        }
        else
        {
            // 0x00107720 clones the objparams, +8 takes the relic, 0x00107520 creates it, 0x000663E0 frees the clone.
            ClonedExecutableResource parameters = clone_executable_resource(g.executable_resources->pools,
                "aid_objparams_ghoulies_scenecontrol_candle");
            u32 relic_address = 0;
            for (const auto& [address, actor] : actors->actor_addresses())
            {
                if (actor == relic)
                {
                    relic_address = address;
                }
            }
            if (relic_address == 0)
            {
                throw Error("the walk-in cutscene's relic candle has no retail address");
            }
            std::memcpy(parameters.bytes.get() + 8, &relic_address, 4);
            FactoryObject* object = g.factory_scene->keep_object(create_factory_object(g.factory_scene->factory,
                {parameters.bytes.get(), parameters.size}));
            g_walk_in_candle = object == nullptr || object->bytes == nullptr ? 0
                : g.factory_scene->address_of(object->bytes.get());
        }
        const u8* records = actors->marker_records();
        const u16 id = next_runtime_marker_id(0, 0xC, 1, records); // 0x0011F1C0
        if (id != 0)
        {
            Vec3 position;
            Vec3 euler;
            read_scene_marker_position(records, i16(id), position); // 0x0011F260
            read_scene_marker_euler(records, i16(id), euler); // 0x0011F2A0
            euler.y = add_wrapped_angle(euler.y, std::bit_cast<float>(0x40490FDBu)); // 0x00100DF0
            place_scene_cutscene(position, cutscenes); // 0x0004EF20
            turn_scene_cutscene(euler, cutscenes); // 0x0004EF80
        }
        g_flag_2470 = 1;
        return 0;
    }
    if (!scene_cutscene_finished(cutscenes)) // 0x0004EFE0
    {
        return 0;
    }
    if (script_node_is_last_action(g.scene_script, node)) // 0x000532E0, ECX node
    {
        return 2;
    }
    g_flag_2470 = 0;
    release_scene_cutscenes(cutscenes); // 0x0004ECE0
    actors->restart_marker_player(); // 0x00127FF0
    if (g_walk_in_candle == 0)
    {
        return 2;
    }
    for (auto& object : g.factory_scene->objects)
    {
        if (object.bytes != nullptr && g.factory_scene->address_of(object.bytes.get()) == g_walk_in_candle)
        {
            dispose_runtime_scene_control(g.factory_scene->controls->instance(object), g_scene_controls); // 0x00116140
            break;
        }
    }
    g_walk_in_candle = 0;
    return 2;
}

// Xbox 0x001274B0
int script_handle_cutscene_actions(Game& g, const ScriptNode& node, int ending)
{
    switch (node.type)
    {
    case 0x13:
        if (ending == 0)
        {
            auto& cutscenes = scene_cutscene_runtime(g);
            if (g_flag_2470 == 0)
            {
                if (scene_player_character(g) == 2)
                {
                    // 0x0004E610 with the 0x80-byte stack copies of the two names.
                    std::array<char, 0x80> boy{"aid_model_ghoulies_actor_boy"};
                    std::array<char, 0x80> girl{"aid_model_ghoulies_actor_girl"};
                    set_cutscene_actor_replacement(cutscenes.bindings.runtime.replacement_source,
                        cutscenes.bindings.runtime.replacement_target, boy.data(), girl.data());
                }
                const i32 skip_mode = g_state_248c == 3 ? 1 : 0;
                start_scene_cutscene(node_cstr(g.scene_script, node, 8).c_str(), skip_mode, skip_mode, cutscenes);
                if (node_u32(g.scene_script, node, 0x88) != 0)
                {
                    g_flag_2470 = 1;
                    script_update_decay_latches(node_u32(g.scene_script, node, 0x8c) == 0 ? 0xffffu : 2u, 0);
                    if (!scene_flag_bit_set(1) || scene_end_flag_set())
                    {
                        begin_cutscene_skip_input(g);
                    }
                    return 0;
                }
            }
            else
            {
                if (!scene_cutscene_finished(cutscenes))
                {
                    if (g_cutscene_skip_input == 0)
                    {
                        return 0;
                    }
                    if (!cutscene_skip_pressed(g))
                    {
                        return 0;
                    }
                }
                if (!cutscene_retained_for_transition(g.scene_script, node))
                {
                    g_flag_2470 = 0;
                    release_scene_cutscenes(cutscenes);
                    script_update_decay_latches(node_u32(g.scene_script, node, 0x8c) == 0 ? 0xffffu : 2u, 1);
                    if (scene_dialogue_context(g).dialogue.state.mode != 0)
                    {
                        close_runtime_dialogue(scene_dialogue_context(g).dialogue);
                    }
                    if (g_cutscene_skip_input != 0)
                    {
                        end_cutscene_skip_input(g);
                        return 2;
                    }
                }
            }
        }
        return 2;
    case 0x14:
        if (ending != 0)
        {
            return 2;
        }
        // Native dependency binding, not a substitute implementation of the
        // retail dialogue calls. Missing ownership must not skip this node.
        return execute_runtime_script_dialogue(g.scene_script.bytes.data() + node.offset,
            ending, scene_dialogue_context(g));
    case 0x15:
    {
        // 0x00106D50 (EDI node+8, the byte at node+0x88, the dword at node+0x8C and 1) loads the movie event into
        // DAT_0050DD90, then 0x00106DD0 (DL that byte) starts it.
        if (g.native_movies == nullptr || g.native_movies->session == nullptr)
        {
            throw Error("script type 0x15 loads a movie event without the boot movie session");
        }
        auto& movies = *g.native_movies;
        const std::string name = node_cstr(g.scene_script, node, 8);
        const u8 id = node_u8(g.scene_script, node, 0x88);
        load_runtime_movie_event(name.c_str(), id, i32(node_u32(g.scene_script, node, 0x8c)), 1,
            movies.session->slots, movies.session->startup()); // 0x00106D50
        start_runtime_movie_event(id, movies.mode, movies.session->slots); // 0x00106DD0, DL the id
        return 2;
    }
    case 0x16:
        if (ending == 0)
        {
            if (g_flag_2470 != 0)
            {
                auto& cutscenes = scene_cutscene_runtime(g);
                if (!scene_cutscene_finished(cutscenes)) // 0x0004EFE0
                {
                    return 0;
                }
                g_flag_2470 = 0;
                release_scene_cutscenes(cutscenes); // 0x0004ECE0
                return 2;
            }
            if (node_u32(g.scene_script, node, 0x8c) != 0)
            {
                g_flag_2470 = 1;
                return 0;
            }
        }
        return 2;
    case 0x53:
        if (ending != 0)
        {
            return 2;
        }
        return play_script_walk_in_cutscene(g, node);
    case 0x9b:
    case 0x9c:
    {
        // The DAT_00511B40 player's position (0x000412A0, ESI the node - 0x93C), or zeros for null or the 0x93C
        // sentinel. Type 0x9B shakes the camera by the name at node+8 there (0x000FB120 with 1.0); type 0x9C plays
        // that camera effect there (0x000493D0, EDX node+8, EAX the point).
        SceneActorServices* actors = script_scene_actors(g);
        if (actors == nullptr)
        {
            throw Error("script types 0x9B / 0x9C read DAT_00511B40 outside a native scene");
        }
        Vec3 position{};
        if (const u32 link = actors->player_link(); link != 0 && link != 0x93Cu)
        {
            const Actor* player = nullptr;
            for (const auto& mark : actors->marked_actors())
            {
                if (mark.address == link - 0x93C)
                {
                    player = mark.actor;
                }
            }
            if (player == nullptr)
            {
                throw Error("DAT_00511B40 player is not on DAT_00511B38");
            }
            position = actor_world_position(*player); // 0x000412A0
        }
        const std::string name = node_cstr(g.scene_script, node, 8);
        if (node.type == 0x9b)
        {
            actors->load_camera_shake(name.c_str(), position, 1.f); // 0x000FB120
        }
        else
        {
            actors->load_camera_effect(name.c_str(), position); // 0x000493D0
        }
        return 2;
    }
    default:
        return 3;
    }
}


// DAT_00430d08. Types 0x46 and 0xA6 store 0 when the second argument is zero.
// The same path stores a zero byte at DAT_00430d14. 0x0011EB30 clears both
// from entity.cpp; that copy is private to that file.
i32 g_flag_30d08 = 0;

char g_byte_30d14[0x8C] = {}; // DAT_00430D14..DAT_00430DA0.

// Xbox 0x00123AD0, EAX name, ECX value
void remember_script_player_weapon(const char* name, i32 value)
{
    g_flag_30d08 = value;
    if (name != nullptr)
    {
        std::strcpy(g_byte_30d14, name);
        return;
    }
    g_byte_30d14[0] = 0;
}


// Xbox 0x00127BC0. Handler 14. The dword at node+4 is the type. The byte map
// at 0x00127F6C covers types 0x2C through 0xA6. Slot 14 of the table at
// 0x00127F30 returns 3, and the holes in that range use it. A type outside
// the range returns 3 as well. The second argument is the flag 0x00128A00
// passes. The third argument is the value that walk passes.
//
// Type 0x2C with a nonzero third argument returns 2. Zero calls 0x001292D0.
// A null return returns 2. A live object loads the dword at object+0x95C
// into EAX, calls 0x0001B810 with two stack zeros, calls 0x00112CC0, and
// returns 2.
//
// Type 0x2D with a nonzero third argument returns 2. Zero calls 0x00127FF0
// and returns 2.
//
// Type 0x2E calls 0x001292D0. A null return returns 2. A live object tests
// the dword at node+8. Zero calls 0x00042090. Nonzero calls 0x000420B0.
// EAX is the object and ECX is that dword. Both return 2.
//
// Type 0x2F calls 0x001292D0. A null return returns 2. A live object passes
// the stack slot at frame+0x1C to 0x00049DB0 in EAX, stores 7 in the next
// dword, and calls vtable +0xC with that slot (message 0x4D, goal 7).
//
// Type 0x30 calls 0x001292D0. A null return returns 2. A live object calls
// vtable +0xC with message id 0x32 and a second dword of 0.
//
// Type 0x31 with a zero second argument calls 0x00123AD0. EAX is node+8 and
// ECX is 0 (the weapon the player spawns with). A nonzero second argument calls
// 0x001292D0. A null return returns 2. A live object calls 0x0013A720 with
// EAX = node+8, ECX = the object, and a stack 1, then returns 2.
//
// Type 0x46 with a zero second argument stores 0 at DAT_00430d08 and a zero
// byte at DAT_00430d14, then returns 2. Nonzero calls 0x001292D0. A null
// return returns 2. A live object is pushed for 0x0013A7C0, then 0x0012ECC0
// runs, and the case returns 2.
//
// Type 0x6F calls 0x001292D0. A null return returns 0. A live object calls
// 0x00028360 with ESI set to that object. AX 0 returns 2. A nonzero AX
// returns 2 when the dword at object+0x984 is nonzero, and 0 when that
// dword is 0. Before a native scene start there is no player, so it holds.
//
// Type 0x76 calls 0x001292D0. A null return returns 2. A live object calls
// vtable +0xC with message id 0x42 and a second dword of 1.
//
// Type 0x77 is that same call with a second dword of 0.
//
// Type 0x91 calls 0x001292D0. A null return returns 2. A live object calls
// 0x00128DF0 with the object, two zeros, the address of the node pointer,
// 0x4240, and 0, with EAX 0 (no point). A nonzero return calls 0x000412A0
// with ESI set to that return and EDI aimed at the message's second dword.
// vtable +0xC then takes message id 0x37 and those three dwords (zeros when
// nothing was found).
//
// Type 0x96 calls 0x001292D0. A null return returns 2. A live object calls
// 0x0011F1C0 with EAX = 0 and stack words 2, 1. A zero AX returns 2. Nonzero
// compares the word at record+0x28 from 0x0011EF20 with the dword at node+8.
// A miss calls 0x0011F1C0 with EAX = the previous AX and stack words 2, 0,
// and returns 2 when that AX is 0. A hit calls 0x0002B430 with EAX = the
// object and ECX = record+0xC, then 0x0002B560 with ECX = record+0x18.
// The case returns 2.
//
// Type 0xA0 calls 0x001292D0. A null return returns 2. A live object stores
// 0x2C over the first argument and calls vtable +0xC with that address.
//
// Type 0xA6 with a zero second argument takes the same DAT_00430d08 store as
// type 0x46. Nonzero calls 0x001292D0. A null return returns 2. A live object
// is pushed for 0x0013A860. A zero return returns 2. Nonzero calls 0x0012ECC0
// and returns 2.
// Xbox 0x00127BC0
int script_handle_actor_effects(Game& g, const ScriptNode& node, int live_7d, int ending)
{
    switch (node.type)
    {
    case 0x2c:
        if (ending == 0)
        {
            SceneActorServices* actors = script_scene_actors(g);
            if (actors == nullptr)
            {
                throw Error("script type 0x2C reads the 0x001292D0 player outside a native scene");
            }
            actors->release_script_player_attachment(); // 0x001292D0, 0x0001B810, 0x00112CC0
        }
        return 2;
    case 0x2d:
        if (ending == 0)
        {
            SceneActorServices* actors = script_scene_actors(g);
            if (actors == nullptr)
            {
                throw Error("script type 0x2D (0x00127FF0) runs the player marker with no native scene");
            }
            actors->restart_marker_player(); // 0x00127FF0
            return 2;
        }
        return 2;
    case 0x2e:
    {
        // 0x001292D0's player: a zero dword at node+8 calls 0x00042090, nonzero 0x000420B0 (EAX the player, ECX the
        // dword).
        SceneActorServices* actors = script_scene_actors(g);
        if (actors == nullptr)
        {
            throw Error("script type 0x2E reads the 0x001292D0 player outside a native scene");
        }
        actors->hold_script_player_attachments(node_u32(g.scene_script, node, 8));
        return 2;
    }
    case 0x2f:
    {
        // 0x001292D0, then 0x00049DB0's message 0x4D with 7 in the next dword, through vtable +0xC.
        SceneActorServices* actors = script_scene_actors(g);
        const u32 link = actors == nullptr ? 0 : actors->player_link();
        if (link != 0)
        {
            std::array<u32, 0x42> message{};
            clear_runtime_goal_message(std::span<u32, 9>(message.data(), 9));
            message[1] = 7;
            actors->send_actor_message(link - 0x93C, message);
        }
        return 2;
    }
    case 0x30:
    {
        // 0x001292D0, then message 0x32 with a zero second dword through vtable +0xC.
        SceneActorServices* actors = script_scene_actors(g);
        const u32 link = actors == nullptr ? 0 : actors->player_link();
        if (link != 0)
        {
            std::array<u32, 0x42> message{0x32, 0};
            actors->send_actor_message(link - 0x93C, message);
        }
        return 2;
    }
    case 0x31:
    {
        const std::string name = node_cstr(g.scene_script, node, 8);
        if (live_7d == 0)
        {
            remember_script_player_weapon(name.c_str(), 0); // 0x00123AD0, EAX node+8, ECX 0
            return 2;
        }
        if (SceneActorServices* actors = script_scene_actors(g); actors != nullptr)
        {
            actors->give_script_player_weapon(name.c_str()); // 0x001292D0, 0x0013A720 with 1
        }
        return 2;
    }
    case 0x46:
        if (live_7d == 0)
        {
            g_flag_30d08 = 0;
            g_byte_30d14[0] = 0;
            return 2;
        }
        if (SceneActorServices* actors = script_scene_actors(g); actors != nullptr)
        {
            actors->release_script_player_weapon(); // 0x001292D0, 0x0013A7C0, 0x0012ECC0
        }
        return 2;
    case 0x6f:
        // The player wait over the natively started scene; before a native start there is no player to wait on.
        if (g.factory_scene != nullptr && g.factory_scene->actor_start == 1)
        {
            return g.factory_scene->start_actors(g).wait_script_player();
        }
        return 0;
    case 0x76:
    case 0x77:
    {
        // 0x001292D0, then message 0x42 through vtable +0xC: 1 for type 0x76, 0 for type 0x77 (mind +0x7C).
        SceneActorServices* actors = script_scene_actors(g);
        const u32 link = actors == nullptr ? 0 : actors->player_link();
        if (link == 0)
        {
            return 2;
        }
        std::array<u32, 0x42> message{0x42, node.type == 0x76 ? 1u : 0u};
        actors->send_actor_message(link - 0x93C, message);
        return 2;
    }
    case 0x91:
    {
        SceneActorServices* actors = script_scene_actors(g);
        if (actors == nullptr)
        {
            throw Error("script type 0x91 reads the 0x001292D0 player outside a native scene");
        }
        actors->point_script_player_at_nearest(); // 0x001292D0, 0x00128DF0, 0x000412A0, message 0x37
        return 2;
    }
    case 0x96:
    {
        // 0x001292D0's player goes to the type-2 marker whose word at +0x28 is the dword at node+8: 0x0002B430 with
        // the marker's +0xC position, then 0x0002B560 with its +0x18 euler.
        SceneActorServices* actors = script_scene_actors(g);
        if (actors == nullptr || actors->player_link() == 0)
        {
            return 2;
        }
        const u8* records = actors->marker_records();
        u16 id = next_runtime_marker_id(0, 2, 1, records); // 0x0011F1C0, AX 0
        if (id == 0)
        {
            return 2;
        }
        const u32 wanted = node_u32(g.scene_script, node, 8);
        while (true)
        {
            const u8* record = find_runtime_marker(records, i16(id)); // 0x0011EF20, SI id
            if (record == nullptr)
            {
                throw Error("script type 0x96 reads +0x28 of a missing marker (address zero)");
            }
            if (u32(read_u16({record, 0x2C}, 0x28)) == wanted)
            {
                const std::span<const u8> row{record, 0x24};
                actors->place_script_player({read_f32(row, 0xC), read_f32(row, 0x10), read_f32(row, 0x14)},
                    {read_f32(row, 0x18), read_f32(row, 0x1C), read_f32(row, 0x20)});
                return 2;
            }
            id = next_runtime_marker_id(id, 2, 0, records);
            if (id == 0)
            {
                return 2;
            }
        }
    }
    case 0xa0:
    {
        // 0x001292D0, then message 0x2C through vtable +0xC.
        SceneActorServices* actors = script_scene_actors(g);
        if (actors == nullptr)
        {
            throw Error("script type 0xA0 reads the 0x001292D0 player outside a native scene");
        }
        if (find_runtime_script_player(actors->player_link(), actors->actor_addresses()) != nullptr)
        {
            const std::array<u32, 0x42> message{0x2c};
            actors->send_actor_message(actors->player_link() - 0x93C, message);
        }
        return 2;
    }
    case 0xa6:
    {
        if (live_7d == 0)
        {
            g_flag_30d08 = 0;
            g_byte_30d14[0] = 0;
            return 2;
        }
        // 0x001292D0's player is pushed for 0x0013A860; a nonzero return calls 0x0012ECC0.
        SceneActorServices* actors = script_scene_actors(g);
        if (actors == nullptr)
        {
            throw Error("script type 0xA6 reads the 0x001292D0 player outside a native scene");
        }
        actors->drop_script_player_weapon();
        return 2;
    }
    default:
        return 3;
    }
}


// DAT_0050EA98. Type 0x4 stores the dword at node+8.
u32 g_flag_ea98 = 0;

// DAT_00427958. Type 0x6 stores the dword at node+8.
u32 g_flag_27958 = 0;

// DAT_00430D10. Type 0x7 stores the dword at node+8. entity.cpp keeps a
// private copy of this address.
i32 g_script_30d10 = 0;

// DAT_00507E78. Type 0xA clears the first word before 0x000FF0A0.

// Image at 0x00314DE8. Type 0x61 indexes it from four bases. The aid_ string
// at 0x00314E50 is the end of the numeric image.
constexpr u32 kTuneDe8 = 0x00314de8u;

constexpr u32 kScriptTune[] = {
    0x40a00000u, 0x41a00000u, 0x41f00000u, 0x42200000u, 0x00000000u,
    0x40e00000u, 0x41700000u, 0x41c80000u, 0x420c0000u, 0x00000000u,
    0x0000002du, 0x00000028u, 0x00000023u, 0x0000001eu, 0x00000019u,
    0x0000003cu, 0x00000000u, 0x3d4ccccdu, 0x3dcccccdu, 0x3e4ccccdu,
    0xbf800000u, 0x00000000u, 0x3d4ccccdu, 0x3dcccccdu, 0x3e4ccccdu,
    0x00000000u,
};


u32 script_tune(u32 base, u32 index)
{
    const u32 slot = (base - kTuneDe8) / 4u + index;
    if (slot >= std::size(kScriptTune))
    {
        return 0;
    }
    return kScriptTune[slot];
}


// DAT_00545AEC, DAT_00545AF0, DAT_00545AF4, DAT_00545AF8. Type 0x61 stores
// the four indexes. The looked-up words land at DAT_00434008, DAT_00434014,
// DAT_00434010, DAT_0043402C, and DAT_00434030.
u32& g_tune_5aec = reinterpret_cast<u32&>(entity_detail::g_look_profile);

// Xbox 0x0013CBD0
void reset_runtime_camera_view_tunables()
{
    g_tune_34008 = std::bit_cast<u32>(20.f); // DAT_0032B234.
    g_tune_3402c = 0;
    g_tune_34014 = std::bit_cast<u32>(15.f); // DAT_0032B214.
    g_tune_5aec = 1;
    g_tune_34010 = 0x2D;
    g_tune_5af0 = 0;
    g_tune_34030 = std::bit_cast<u32>(-1.f); // DAT_0032B120.
    g_tune_5af4 = 0;
    g_tune_5af8 = 0;
}

u32 g_tune_5af0 = 0;

u32 g_tune_5af4 = 0;

u32 g_tune_5af8 = 0;

u32& g_tune_34008 = g_camera_view_tunables[0];

u32& g_tune_34014 = g_camera_view_tunables[3];

u32& g_tune_34010 = g_camera_view_tunables[2];

u32& g_tune_3402c = g_camera_view_tunables[9];

u32& g_tune_34030 = g_camera_view_tunables[10];

// DAT_00545D20 and DAT_00545D24. 0x00140E30 clears both when the word at
// node+0x20 + 0x94 is 0. A live 0x001248D0(1, 0, 0) would set the first to 1, as the scene start
// (0x0011C72F) does.
// A live 0x001248D0(8, 0, 0) would set the second to 1.
i32 g_flag_5d20 = 0;

i32 g_flag_5d24 = 0;

// DAT_00545D28. Nonzero holds the previous tune indexes in the four words
// below. 0x00140E30 clears it after restoring those indexes.
i32 g_flag_5d28 = 0;

// DAT_00545D2C, DAT_00545D30, DAT_00545D34, DAT_00545D38. Saved copies of
// DAT_00545AEC, DAT_00545AF0, DAT_00545AF4, and DAT_00545AF8.
u32 g_tune_5d2c = 0;

u32 g_tune_5d30 = 0;

u32 g_tune_5d34 = 0;

u32 g_tune_5d38 = 0;

// DAT_00512134, DAT_00512138, DAT_00512104. Type 0x64.
u32 g_color_2134 = 0;

u32 g_flag_2138 = 0;

u32 g_flag_2104 = 0;
u32 g_camera_light_color = 0;
u32 g_camera_light_second_color = 0;
float g_camera_light_1c = 0.f;
float g_camera_light_20 = 0.f;
float g_camera_light_24 = 0.f;
float g_camera_light_28 = 0.f;


// DAT_00506C60. Type 0x89 stores node+8 * 60 + 0.99.
float g_script_6c60 = 0.f;

// DAT_005110B4. Type 0xA2 stores 1 when node+8 is 0.
u32 g_flag_10b4 = 0;

// DAT_0050EF1C. Type 0xA4 stores 1 when node+8 is nonzero.
u32 g_flag_ef1c = 0;

// DAT_00510650. Type 0xA5 stores 1 when node+8 is nonzero.
u32 g_flag_0650 = 0;


// Xbox 0x00128050. Handler 15. The dword at node+4 is the type. The byte map
// at 0x001286BC covers types 0x1 through 0xA5. Any other type, and the holes
// in that range, return 3. The second argument is the flag 0x00128A00 passes.
// The third argument is the value that walk passes. A handled type returns 2.
//
// Type 0x1 calls 0x0011C890 with EAX = node+8 to queue the background name.
//
// Type 0x2 with a zero second argument calls 0x0011C950 to queue an object
// name. A nonzero second argument calls 0x00107720 with EAX = node+8; a live
// clone goes to 0x00107520 and is freed by 0x000663E0.
//
// Type 0x4 stores the dword at node+8 into DAT_0050EA98.
// Types 0x5 and 0xC return 2.
// Type 0x6 stores the dword at node+8 into DAT_00427958.
// Type 0x7 stores the dword at node+8 into DAT_00430D10.
//
// Type 0x8 calls 0x001077B0 with EAX = node+8 and 0x0010F690 with that tag
// in ECX and the dword at node+0x48 on the stack, then walks DAT_00511B48: an
// object of the tag with a live +0x958 has the dword at node+0x48 stored at
// that pointer +0x6C.
//
// Type 0x9 calls 0x001077B0 with EAX = node+8, 0x0010F660 with that tag in
// ECX and the float at node+0x48 in XMM0, and 0x00122D40 with the tag in EDI
// and the message {0x4E, that float} in EBX.
//
// Type 0xA shows the scene's name (0x000FF0A0 into DAT_00507E78, 0x000FBE10,
// the DAT_00512484 title record, 0x0011DCF0) when the dword at node+0x48 is
// set and the third argument is zero.
//
// Type 0xB calls 0x00123AD0 with ECX = the byte at node+8 and EAX = node+9.
// Type 0x4B calls 0x000FD780 with ESI = node+8.
// Type 0x4C calls 0x0003A1F0 with ESI = the dword at node+8.
// Type 0x52 calls 0x0012AA50 with node+8 on the stack (not translated; it
// raises).
//
// Type 0x61 stores four indexes and the words they select from 0x00314DE8,
// 0x00314DFC, 0x00314E10, 0x00314E28, and 0x00314E38. The second float uses
// the index at node+8, the same index as the first.
//
// Type 0x64 packs the bytes at node+0x28, node+0x2C, and node+0x30 into
// DAT_00512134. DAT_00512138 is 1 when the dword at node+8 is nonzero.
// DAT_00512104 copies that dword. 0x001236D0 takes the packed bytes at
// node+0xC, node+0x10, node+0x14, and node+0x18 twice, then the dwords at
// node+0x1C, node+0x20, and node+0x24, with XMM0 the float at node+0x1C.
//
// Type 0x6D calls 0x001077B0 with EAX = node+8 and stores the word at
// DAT_0050E1B4 + DAT_0050E1B0*2, then increments the count.
//
// Type 0x79 calls 0x00114050 with ECX = the dword at node+8 and BL = 2 when
// that dword is 0, 1 otherwise.
//
// Type 0x89 stores the float at node+8 times 60 plus 0.99 into DAT_00506C60, then runs 0x000F27C0 with EAX = 1.
//
// Type 0x8A with a nonzero dword at node+8 stores 0 at DAT_00509B74 and 1 at
// DAT_00509B70. Zero stores 1 and 0.
//
// Type 0x8F turns (0, 1, 0) by the float at node+8 times 0.0055555557 times
// pi (0x001047E0), forms the light offset from that matrix and passes it to
// 0x000FE160.
//
// Type 0x9A calls 0x000F2740, the countdown's gold finish. Type 0xA1 calls
// 0x000FEA90 with EAX = the dword at node+8.
//
// Type 0xA2 stores 1 at DAT_005110B4 when the dword at node+8 is 0.
// Type 0xA4 stores 1 at DAT_0050EF1C when that dword is nonzero.
// Type 0xA5 stores 1 at DAT_00510650 when that dword is nonzero.
namespace
{
// The boot-owned scene services, without beginning a scene: the world actions below set boot state (the title, the
// reverb, the lighting, the tag tables) and run while the script loads, before the scene's actors start.
SceneActorServices& boot_scene_actors(Game& g)
{
    if (g.factory_scene == nullptr || g.factory_scene->actor_services == nullptr)
    {
        throw Error("script world actions have no boot scene services");
    }
    return *g.factory_scene->actor_services;
}
}

// Xbox 0x00128050
int script_handle_world_actions(Game& g, const ScriptNode& node, int live_7d, int ending)
{
    switch (node.type)
    {
    case 1:
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        request_scene_background(g_scene_object_requests, key.c_str());
        return 2;
    }
    case 2:
        if (live_7d == 0)
        {
            const std::string key = node_cstr(g.scene_script, node, 8);
            request_scene_object(g_scene_object_requests, key.c_str());
            return 2;
        }
        {
            // 0x00107720 clones the objparams, 0x00107520 creates the object and 0x000663E0 frees the clone.
            const std::string key = node_cstr(g.scene_script, node, 8);
            if (g.factory_scene == nullptr || g.executable_resources == nullptr)
            {
                throw Error("script object creation without the scene's object factory");
            }
            ClonedExecutableResource parameters = clone_executable_resource(g.executable_resources->pools,
                key.c_str());
            if (parameters.bytes != nullptr)
            {
                FactoryObject object = create_factory_object(g.factory_scene->factory,
                    {parameters.bytes.get(), parameters.size});
                if (object.bytes != nullptr)
                {
                    g.factory_scene->keep_object(std::move(object));
                }
            }
        }
        return 2;
    case 4:
        g_flag_ea98 = node_u32(g.scene_script, node, 8);
        return 2;
    case 5:
    case 0xc:
        return 2;
    case 6:
        g_flag_27958 = node_u32(g.scene_script, node, 8);
        return 2;
    case 7:
        g_script_30d10 = i32(node_u32(g.scene_script, node, 8));
        return 2;
    case 8:
    {
        // Each DAT_00511B48 actor of the tag with a mind (node +0x14, actor +0x958) has the mind's +0x6C, its target
        // range, set to node+0x48.
        const u16 tag = static_cast<u16>(script_find_actor_tag(node_cstr(g.scene_script, node, 8).c_str()));
        const u32 stored = node_u32(g.scene_script, node, 0x48);
        boot_scene_actors(g).add_mind_class_setting(tag, std::bit_cast<float>(stored)); // 0x0010F690
        for (const auto* link = static_cast<const entity_detail::ListHead*>(entity_detail::g_world.alt.next);
             link != nullptr; link = static_cast<const entity_detail::ListHead*>(link->next))
        {
            if (!g_alt_node_class || !g_alt_node_mind)
            {
                throw Error("script type 8 walks DAT_00511B48 without the scene's actor views");
            }
            if (g_alt_node_class(link) != tag)
            {
                continue;
            }
            const std::span<u8> mind = g_alt_node_mind(link);
            if (!mind.empty())
            {
                mind_record(mind).target_range = std::bit_cast<float>(stored);
            }
        }
        return 2;
    }
    case 9:
    {
        // The tag's animation scale (0x0010F660), then {0x4E, scale} to each marked actor of the tag (0x00122D40).
        const u16 tag = static_cast<u16>(script_find_actor_tag(node_cstr(g.scene_script, node, 8).c_str()));
        const u32 value = node_u32(g.scene_script, node, 0x48);
        SceneActorServices& actors = boot_scene_actors(g);
        actors.add_animation_class(tag, std::bit_cast<float>(value)); // 0x0010F660, XMM0 node+0x48
        const std::array<u32, 2> message{0x4E, value};
        message_runtime_marked_actors(actors, tag, message); // 0x00122D40, EDI the tag, EBX the message
        return 2;
    }
    case 0xa:
    {
        // The scene's name (0x000FF0A0 into DAT_00507E78, 0x0011DCF0), shown when node+0x48 is set and ending is 0.
        const std::string key = node_cstr(g.scene_script, node, 8);
        boot_scene_actors(g).show_script_scene_name(key.c_str(), node_u32(g.scene_script, node, 0x48),
            ending, std::bit_cast<float>(node_u32(g.scene_script, node, 0x4C)),
            std::bit_cast<float>(node_u32(g.scene_script, node, 0x50)));
        return 2;
    }
    case 0xb:
    {
        const std::string name = node_cstr(g.scene_script, node, 9);
        remember_script_player_weapon(name.c_str(), node_u8(g.scene_script, node, 8)); // 0x00123AD0, EAX node+9
        return 2;
    }
    case 0x4b:
        boot_scene_actors(g).apply_scene_lighting(
            std::span<const u8>(g.scene_script.bytes).subspan(node.offset + 8, 0x24)); // 0x000FD780, ESI node+8
        return 2;
    case 0x4c:
        boot_scene_actors(g).set_reverb_preset(i32(node_u32(g.scene_script, node, 8))); // 0x0003A1F0
        return 2;
    case 0x52:
    {
        const std::string name = node_cstr(g.scene_script, node, 8);
        boot_scene_actors(g).set_post_model(name.c_str()); // 0x0012AA50, node+8
        return 2;
    }
    case 0x61:
    {
        const u32 at_8 = node_u32(g.scene_script, node, 8);
        const u32 at_c = node_u32(g.scene_script, node, 0xc);
        const u32 at_10 = node_u32(g.scene_script, node, 0x10);
        const u32 at_14 = node_u32(g.scene_script, node, 0x14);
        g_tune_34008 = script_tune(0x00314de8u, at_8);
        g_tune_5aec = at_8;
        g_tune_5af0 = at_c;
        g_tune_34014 = script_tune(0x00314dfcu, at_8);
        g_tune_5af4 = at_10;
        g_tune_3402c = script_tune(0x00314e28u, at_10);
        g_tune_5af8 = at_14;
        g_tune_34010 = script_tune(0x00314e10u, at_c);
        g_tune_34030 = script_tune(0x00314e38u, at_14);
        return 2;
    }
    case 0x64:
    {
        const u32 color = u32(node_u8(g.scene_script, node, 0xc)) |
                          (u32(node_u8(g.scene_script, node, 0x10)) << 8) |
                          (u32(node_u8(g.scene_script, node, 0x14)) << 16) |
                          (u32(node_u8(g.scene_script, node, 0x18)) << 24);
        g_color_2134 = u32(node_u8(g.scene_script, node, 0x28)) |
                       (u32(node_u8(g.scene_script, node, 0x2c)) << 8) |
                       (u32(node_u8(g.scene_script, node, 0x30)) << 16);
        const u32 at_8 = node_u32(g.scene_script, node, 8);
        g_flag_2138 = at_8 != 0 ? 1u : 0u;
        g_flag_2104 = at_8;
        // 0x001236D0 with the colour twice, XMM0 and the third argument the float at +0x1C, then +0x20 and +0x24.
        const float at_1c = std::bit_cast<float>(node_u32(g.scene_script, node, 0x1c));
        g_camera_light_1c = at_1c;
        g_camera_light_20 = at_1c;
        g_camera_light_24 = std::bit_cast<float>(node_u32(g.scene_script, node, 0x20));
        g_camera_light_color = color;
        g_camera_light_second_color = color;
        g_camera_light_28 = std::bit_cast<float>(node_u32(g.scene_script, node, 0x24));
        return 2;
    }
    case 0x6d:
    {
        const u16 tag = static_cast<u16>(script_find_actor_tag(node_cstr(g.scene_script, node, 8).c_str()));
        boot_scene_actors(g).add_scale_exclusion(tag); // DAT_0050E1B4 + DAT_0050E1B0 * 2
        return 2;
    }
    case 0x79:
        boot_scene_actors(g).set_background_morphs_playing(
            node_u32(g.scene_script, node, 8) == 0 ? u8{2} : u8{1}); // 0x00114050, BL
        return 2;
    case 0x89:
    {
        const float value = std::bit_cast<float>(node_u32(g.scene_script, node, 8));
        g_script_6c60 = value * std::bit_cast<float>(0x42700000u) + std::bit_cast<float>(0x3f7d70a4u);
        set_runtime_chapter5_countdown_mode(1, chapter5_countdown("script type 0x89")); // 0x000F27C0, EAX 1
        return 2;
    }
    case 0x8a:
        if (node_u32(g.scene_script, node, 8) != 0)
        {
            g_light_running = 0; // DAT_00509B74
            g_light_paused = 1; // DAT_00509B70
            return 2;
        }
        g_light_running = 1;
        g_light_paused = 0;
        return 2;
    case 0x8f:
    {
        // The light offset turned about Y by node+8 degrees (DAT_0032B6C0 1/180, DAT_0032B148 pi): the turn's rows
        // applied to (0, 1, -1) plus its translation column, then 0x000FE160.
        const float angle = std::bit_cast<float>(node_u32(g.scene_script, node, 8)) *
                            std::bit_cast<float>(0x3bb60b61u) * std::bit_cast<float>(0x40490fdbu);
        float turn[12];
        build_runtime_axis_rotation(turn, Vec3{0.f, 1.f, 0.f}, angle); // 0x001047E0, ESI the turn
        const Vec3 direction{(turn[0] * 0.f - turn[2] * 1.f) + turn[3] + turn[1],
            (turn[4] * 0.f - turn[6] * 1.f) + turn[7] + turn[5], (turn[8] * 0.f - turn[10] * 1.f) + turn[11] + turn[9]};
        set_runtime_light_offset(&direction); // 0x000FE160, EAX the direction
        return 2;
    }
    case 0x9a:
        finish_runtime_chapter5_countdown(chapter5_countdown("script type 0x9A")); // 0x000F2740
        return 2;
    case 0xa1:
        set_light_filled_gate(i32(node_u32(g.scene_script, node, 8))); // 0x000FEA90, EAX node+8
        return 2;
    case 0xa2:
        g_flag_10b4 = node_u32(g.scene_script, node, 8) == 0 ? 1u : 0u;
        return 2;
    case 0xa4:
        g_flag_ef1c = node_u32(g.scene_script, node, 8) != 0 ? 1u : 0u;
        return 2;
    case 0xa5:
        g_flag_0650 = node_u32(g.scene_script, node, 8) != 0 ? 1u : 0u;
        return 2;
    default:
        return 3;
    }
}


// Xbox 0x00128770. Handler 16. The dword at node+4 is the type. Type 0x39
// loads the dword at node+8 into EDI and calls 0x0012D050. That call walks
// DAT_00511B28, the first node of DAT_00511B24. A zero EDI calls 0x00147860
// on each object. A nonzero EDI decrements the dword at object+0xAD0 and
// calls 0x0003EEB0 when that counter hits 0. The case returns 2. 0x0012D050
// is not translated, so the case raises.
//
// Type 0x59 stores 1 in the incoming argument slot, then calls 0x000FBFB0
// with count 0xE9, the names at 0x0042A854, and the cstring at node+8. A miss
// zeros EDI. A hit increments the index and zero-extends it into EDI. Both
// paths point EBX at that slot and call 0x00122FA0. That call walks
// DAT_00511B28, compares DI with the word at object+6, and calls vtable +0xC
// with EBX on a match. Both paths return 2.
// Any other type returns 3.
std::function<void(Actor& actor, std::span<u32> message)> g_script_actor_message;
std::function<void(Actor& actor)> g_script_drain_actor_goals;
std::function<void(std::span<const u32> message)> g_script_body_list_message;
std::function<void(u16 tag, float delay, float lifetime)> g_script_ghoul_intro_caption;

// Xbox 0x0001BB60, ECX high, EDX low, ESI the random state
i32 draw_runtime_random_count(i32 high, i32 low, u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    const float fraction = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    return i32(float(high - low) * fraction + float(low));
}

// Xbox 0x00122FA0, DI tag, EBX message
void message_runtime_tagged_list_actors(u16 tag, std::span<u32> message)
{
    auto* node = static_cast<ActorSpawnLink*>(entity_detail::g_world.body_b.next); // DAT_00511B28.
    while (node != nullptr)
    {
        Actor* actor = node->actor; // The object at node - 0x934.
        node = node->next;
        if (actor == nullptr)
        {
            throw Error("DAT_00511B24 node has no actor");
        }
        if (actor->class_key == tag)
        {
            if (!g_script_actor_message)
            {
                throw Error("0x00122FA0 sends a message with no scene bound");
            }
            g_script_actor_message(*actor, message); // Vtable +0xC.
        }
    }
}

// Xbox 0x00128770
int script_handle_broadcast_actions(Game& g, const ScriptNode& node)
{
    if (node.type == 0x39)
    {
        // 0x0012D050 with EDI the dword at node+8 walks DAT_00511B28.
        throw Error("script type 0x39 calls 0x0012D050 (EDI " + std::to_string(node_u32(g.scene_script, node, 8))
            + "), which is not translated");
    }
    if (node.type != 0x59)
    {
        return 3;
    }
    std::array<u32, 1> message{1}; // The incoming argument slot, EBX.
    const std::string key = node_cstr(g.scene_script, node, 8);
    const i32 found = binary_search_names(0xe9, kObjTagActor, key.c_str()); // 0x000FBFB0, names 0x0042A854
    const u16 tag = found == -1 ? 0 : static_cast<u16>(found + 1);
    message_runtime_tagged_list_actors(tag, message); // 0x00122FA0, DI tag
    return 2;
}

} // namespace gbtg
