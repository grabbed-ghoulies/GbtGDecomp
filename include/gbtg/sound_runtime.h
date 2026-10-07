#pragma once

#include "gbtg/types.h"
#include <functional>
#include <array>
#include <span>

namespace gbtg
{

struct Actor;
inline constexpr std::array<u8, 8> actor_sound_groups{0, 0, 0, 1, 0, 0, 0, 0}; // DAT_002FD200
struct RuntimeSoundLink
{
    RuntimeSoundLink* previous = nullptr;
    RuntimeSoundLink* next = nullptr;
};
struct RuntimeActorSound : RuntimeSoundLink
{
    u16 previous_bone = 0; // voice index +8
    Actor* owner = nullptr; // +0xC
    std::array<char, 128> name{}; // +0x10
    u16 request_id = 0; // +0x90
    u8 enabled = 0; // +0x92
    Vec3 position{}; // +0x94
    Vec3 direction{}; // +0xA0
    float elapsed = 0; // +0xAC
    float volume = 0; // +0xB0
};

struct RuntimeSoundBank
{
    void* handle = nullptr; // bank +0x18
};
struct RuntimeSoundCue
{
    RuntimeSoundBank* bank = nullptr; // +0x18
    void* handle = nullptr; // +0x24
    u32 flags = 0; // +0x34
};
struct RuntimeSoundVoice
{
    u32 state = 0; // +0
    RuntimeSoundCue* cue = nullptr; // +0x20
};
struct RuntimeSoundBoundaries
{
    std::function<i32(void*, void*, bool)> pause_cue; // IXACTSoundBank_PauseSoundCue
    std::function<void(void*, u32, u32, void*)> stop_cue; // IXACTSoundBank_Stop
    std::function<u32()> last_error; // XAPI GetLastError
};
struct RuntimeActorSoundGroup
{
    i32 capacity = 0; // group +0
    RuntimeSoundLink active; // DAT_00512524, stride 0x14
    RuntimeSoundLink pending; // DAT_0051252C
};
struct RuntimeActorSoundQueues
{
    std::array<RuntimeActorSoundGroup, 2> groups;
    std::span<RuntimeSoundVoice> voices;
    u32 stopped_count = 0;
    RuntimeSoundBoundaries boundaries;
    std::span<i16> static_voice_indices; // DAT_005125D8, host view of stride 0x90
    i32 static_voice_count = 0; // DAT_00512548
};
// The callbacks translate voice allocation/play and actor message 0x89 at
// the calls in 0x0012A7D0. Allocation failure is -1, as in 0x00039830.
void dispatch_pending_actor_sounds(RuntimeActorSoundGroup& group,
    const std::function<i16(RuntimeActorSound&)>& start,
    const std::function<void(Actor&, RuntimeActorSound&)>& started); // Xbox 0x0012A7D0
void reset_actor_sound_queues(RuntimeActorSoundQueues& queues); // Xbox 0x00129F70
bool actor_sound_is_active(const RuntimeActorSound& sound,
    const RuntimeActorSoundQueues& queues); // Xbox 0x0012A970
void release_actor_sound(RuntimeActorSound& sound,
    RuntimeActorSoundQueues& queues); // Xbox 0x0012A3D0
void queue_actor_sound(RuntimeActorSound& sound, i32 group, Actor* owner,
    u16 request_id, const Vec3* position, const Vec3* direction, float volume,
    const char* name, RuntimeActorSoundQueues& queues); // Xbox 0x0012A260
void add_sound_direction(Vec3& direction, float pitch, float yaw,
    float length); // Xbox 0x00106220
void request_actor_event_sound(std::span<const u8> event, Actor& actor,
    u32& random_state, RuntimeActorSoundQueues& queues); // Xbox 0x0003A830
void stop_actor_sound(Actor& actor, i32 slot,
    RuntimeActorSoundQueues& queues); // Xbox 0x00041E10
bool actor_sound_is_playing(const Actor& actor, i32 slot,
    const RuntimeActorSoundQueues& queues); // Xbox 0x00041C80
bool request_actor_sound(Actor& actor, i32 slot, const char* name,
    RuntimeActorSoundQueues& queues); // Xbox 0x00041D00
u32 sound_runtime_error(const RuntimeSoundBoundaries& boundaries); // Xbox 0x0015B8E0
void pause_runtime_sound_cue(RuntimeSoundCue& cue, u32 paused,
    const RuntimeSoundBoundaries& boundaries); // Xbox 0x0015CDD0
void stop_runtime_sound_cue(RuntimeSoundCue& cue, i32 immediate, u32& stopped_count,
    const RuntimeSoundBoundaries& boundaries); // Xbox 0x0015CE30
void stop_runtime_sound_voice(RuntimeSoundVoice& voice, i32 immediate, u32& stopped_count,
    const RuntimeSoundBoundaries& boundaries); // Xbox 0x00039690

} // namespace gbtg
