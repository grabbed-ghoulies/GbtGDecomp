#include "gbtg/sound_runtime.hpp"
#include "gbtg/anim.hpp"

namespace gbtg
{

// Xbox 0x0012A970
bool actor_sound_is_active(const RuntimeActorSound& sound,
    const RuntimeActorSoundQueues& queues)
{
    for (const RuntimeActorSoundGroup& group : queues.groups)
    {
        const RuntimeSoundLink* root = sound.previous;
        if (root != nullptr)
        {
            while (root->previous != nullptr)
            {
                root = root->previous;
            }
        }
        if (root == &group.active)
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x0012A3D0
void release_actor_sound(RuntimeActorSound& sound, RuntimeActorSoundQueues& queues)
{
    if (actor_sound_is_active(sound, queues) && static_cast<i16>(sound.previous_bone) != -1)
    {
        stop_runtime_sound_voice(queues.voices[static_cast<i16>(sound.previous_bone)],
            0, queues.stopped_count, queues.boundaries);
        sound.previous_bone = 0xFFFF;
    }
    if (sound.previous != nullptr)
    {
        sound.previous->next = sound.next;
    }
    if (sound.next != nullptr)
    {
        sound.next->previous = sound.previous;
    }
    sound.previous = nullptr;
    sound.next = nullptr;
    sound.enabled = 1;
}

// Xbox 0x00106220
void add_sound_direction(Vec3& direction, float pitch, float yaw, float length)
{
    const float pitch_sin = static_cast<float>(retail_sin(pitch));
    const float pitch_cos = static_cast<float>(retail_cos(pitch));
    const float yaw_sin = static_cast<float>(retail_sin(yaw));
    const float yaw_cos = static_cast<float>(retail_cos(yaw));
    direction.y = static_cast<float>(static_cast<long double>(direction.y) -
        static_cast<long double>(pitch_sin) * length);
    direction.x = pitch_cos * length * yaw_sin + direction.x;
    direction.z = pitch_cos * length * yaw_cos + direction.z;
}

// Xbox 0x00129F70
void reset_actor_sound_queues(RuntimeActorSoundQueues& queues)
{
    for (RuntimeActorSoundGroup& group : queues.groups)
    {
        while (group.active.next != nullptr)
        {
            release_actor_sound(*static_cast<RuntimeActorSound*>(group.active.next), queues);
        }
        while (group.pending.next != nullptr)
        {
            RuntimeSoundLink& node = *group.pending.next;
            if (node.previous != nullptr)
            {
                node.previous->next = node.next;
            }
            if (node.next != nullptr)
            {
                node.next->previous = node.previous;
            }
            node.previous = nullptr;
            node.next = nullptr;
        }
    }
    for (i32 index = 0; index < queues.static_voice_count; ++index)
    {
        i16& voice = queues.static_voice_indices[index];
        if (voice != -1)
        {
            stop_runtime_sound_voice(queues.voices[voice], 0,
                queues.stopped_count, queues.boundaries);
        }
        voice = -1;
    }
    queues.static_voice_count = 0;
}

// Xbox 0x0015B8E0
u32 sound_runtime_error(const RuntimeSoundBoundaries& boundaries)
{
    boundaries.last_error();
    return 0x80004005;
}

// Xbox 0x0015CDD0
void pause_runtime_sound_cue(RuntimeSoundCue& cue, u32 paused,
    const RuntimeSoundBoundaries& boundaries)
{
    if (((cue.flags >> 2) & 1) != paused)
    {
        const i32 result = boundaries.pause_cue(cue.bank->handle, cue.handle, paused != 0);
        if (result < 0)
        {
            sound_runtime_error(boundaries);
            return;
        }
        cue.flags ^= (((paused & 0xFF) << 2) ^ cue.flags) & 4;
    }
}

// Xbox 0x0015CE30
void stop_runtime_sound_cue(RuntimeSoundCue& cue, i32 immediate, u32& stopped_count,
    const RuntimeSoundBoundaries& boundaries)
{
    const u32 flags = cue.flags;
    if ((flags & 2) == 0)
    {
        cue.flags = flags | 2;
        if (cue.handle != nullptr)
        {
            if ((flags & 4) != 0)
            {
                pause_runtime_sound_cue(cue, 0, boundaries);
                immediate = 1;
            }
            boundaries.stop_cue(cue.bank->handle, 0xFFFFFFFF, immediate != 0 ? 2 : 0, cue.handle);
        }
        ++stopped_count;
    }
}

// Xbox 0x00039690
void stop_runtime_sound_voice(RuntimeSoundVoice& voice, i32 immediate, u32& stopped_count,
    const RuntimeSoundBoundaries& boundaries)
{
    if (voice.cue == nullptr)
    {
        voice.state = 10;
    }
    else
    {
        switch (voice.state)
        {
        case 2:
        case 3:
            voice.state = 4;
            return;
        case 5:
        case 6:
        case 7:
            voice.state = 9;
            stop_runtime_sound_cue(*voice.cue, immediate, stopped_count, boundaries);
            return;
        }
    }
}

} // namespace gbtg
