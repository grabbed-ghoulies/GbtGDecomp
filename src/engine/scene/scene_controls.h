#pragma once

#include "gbtg/actor_runtime.h"
#include "gbtg/object_factory.h"
#include "../audio/bank_streams.h"
#include "../entities/actor_clock.h"
#include <unordered_map>
#include <vector>

namespace gbtg
{
// The scene-control lists. Objects link at +8 (pending, then active), +0x10 (updates) and +0x18 (disposal).
struct SceneControlLists
{
    RuntimeActorLink active{}; // DAT_0050E434; its next is DAT_0050E438, the list 0x0011E7F0 ticks.
    RuntimeActorLink pending{}; // DAT_0050E43C; its next is DAT_0050E440.
    RuntimeActorLink updates{}; // DAT_0050E444.
    RuntimeActorLink disposal{}; // DAT_0050E44C.
};
// Native state of one scene control (type 3 and its derived classes).
struct SceneControlInstance
{
    std::span<u8> state; // The factory object's bytes, which keep their address as the object moves.
    u16 type = 0;
    RuntimeActorLink link{}; // +8
    RuntimeActorLink update{}; // +0x10
    RuntimeActorLink disposal{}; // +0x18
    std::vector<u16> playlist; // Type 0x91 +0x24, the 0x00080670 shuffle.
};
struct SceneControlContext
{
    SceneControlLists& lists;
    u32& random; // DAT_0054DD7C.
    std::vector<const char*> walkabout; // 0x002FFFB8, rows of 0x80.
    const char* timer; // 0x002FFF38, XACT_SOUNDBANK_GSTREAMSWALK_CHAPTERFIVE_TIMER.
    std::function<i32()> chapter; // 0x0004B4D0 of the current script.
    // Bound when the scene's actor services start; the script can create controls before that.
    const RuntimeActorClock* clock = nullptr;
    AudioBankStreams* audio = nullptr;
    const RuntimeActorClock& frame_clock() const;
    AudioBankStreams& streams() const;
};
class NativeSceneControlOwner
{
public:
    explicit NativeSceneControlOwner(SceneControlContext context);
    NativeSceneControlOwner(const NativeSceneControlOwner&) = delete;
    NativeSceneControlOwner& operator=(const NativeSceneControlOwner&) = delete;
    // Types 3 (getter 0x00115D50, vtable 0x0050E41C) and 0x91 (getter 0x00080520, vtable 0x00505530).
    void install(std::span<ObjectFactoryType> types);
    SceneControlInstance& instance(const FactoryObject& object);
    SceneControlInstance& instance_of(RuntimeActorLink& link); // By the +8 link.
    SceneControlInstance& instance_of_update(RuntimeActorLink& link); // By the +0x10 link.
    // The scene object that owns a +0x18 disposal link.
    FactoryObject& object_of_disposal(RuntimeActorLink& link, std::vector<FactoryObject>& objects);
    void forget(const FactoryObject& object); // After its type 3 destructor.
    void message(FactoryObject& object, std::span<const u32> message); // Vtable +0xC.
    void tick(SceneControlInstance& control); // Vtable +0x10.
    SceneControlContext context;

private:
    std::unordered_map<const u8*, std::unique_ptr<SceneControlInstance>> instances_;
};
extern NativeSceneControlOwner* g_scene_control_owner; // The scene's owner while it is loaded.
extern SceneControlLists g_scene_controls;
bool construct_runtime_scene_control(SceneControlInstance& control, SceneControlLists& lists); // Xbox 0x00115DA0
void destroy_runtime_scene_control(SceneControlInstance& control); // Xbox 0x00115DE0
u32 retire_runtime_scene_control(SceneControlInstance& control, SceneControlLists& lists); // Xbox 0x00115E70, ESI
void message_runtime_scene_control(SceneControlInstance& control, std::span<const u32> message,
    SceneControlLists& lists); // Xbox 0x00115EE0
void tick_runtime_scene_control(std::span<u8> state, const RuntimeActorClock& clock); // Xbox 0x00115E30
void build_runtime_scene_playlist(std::span<u8> state, std::vector<u16>& playlist, u32 count,
    u32& random); // Xbox 0x00080670, state +0x24
bool construct_runtime_walkabout_control(std::span<u8> state, SceneControlInstance& control,
    SceneControlContext& context); // Xbox 0x00080570
void stop_runtime_walkabout_track(std::span<u8> state, SceneControlContext& context); // Xbox 0x00080810, ESI
void play_runtime_walkabout_track(std::span<u8> state, SceneControlInstance& control,
    SceneControlContext& context); // Xbox 0x000808B0, EAX
void destroy_runtime_walkabout_control(std::span<u8> state, SceneControlInstance& control,
    SceneControlContext& context); // Xbox 0x000805B0
void tick_runtime_walkabout_control(std::span<u8> state, SceneControlInstance& control,
    SceneControlContext& context); // Xbox 0x00080600
// 0x0011E7F0's walk of DAT_0050E438: each active control's vtable +0x10.
void tick_runtime_scene_control_list();
// The scene's named music stream (DAT_0050E4E0, 0x80 bytes): the current one is released, then `name` (ESI; may
// be null) is acquired with priority `id` and becomes current.
void play_scene_music_stream(const char* name, i32 id, char* current, AudioBankStreams& streams); // Xbox 0x00116D20
// Fight music: the row of DAT_0030E490 (25 rows of 0x82 bytes: actor tag word, stream name) whose tag matches
// (CX), else tag 1's row, played with priority 3.
void play_scene_fight_music(i16 tag, std::span<const u8> table, char* current,
    AudioBankStreams& streams); // Xbox 0x00116C50
}
