#include "scene_controls.h"

#include <bit>
#include <cstring>

namespace gbtg
{
NativeSceneControlOwner* g_scene_control_owner = nullptr;
SceneControlLists g_scene_controls;

namespace
{
NativeSceneControlOwner* installing = nullptr;

u32 word(std::span<const u8> bytes, size_t offset)
{
    return read_u32(bytes, offset);
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

std::span<u8> state_of(FactoryObject& object)
{
    return {object.bytes.get(), object.size};
}

void unlink(RuntimeActorLink& link)
{
    if (link.previous != nullptr)
    {
        link.previous->next = link.next;
    }
    if (link.next != nullptr)
    {
        link.next->previous = link.previous;
    }
    link.previous = nullptr;
    link.next = nullptr;
}

void append(RuntimeActorLink& head, RuntimeActorLink& link)
{
    RuntimeActorLink* tail = &head;
    while (tail->next != nullptr)
    {
        tail = tail->next;
    }
    tail->next = &link;
    link.previous = tail;
    link.next = nullptr;
}

// Xbox 0x00115D30
void scene_control_object_sizes(ObjectSizes& output)
{
    output.instance = 0x24;
    output.parameters = 8;
}

// Xbox 0x00080500
void walkabout_control_object_sizes(ObjectSizes& output)
{
    output.instance = 0x3C;
    output.parameters = 8;
}

bool construct_scene_control_view(FactoryObject& object, std::span<const u8>)
{
    auto& control = installing->instance(object);
    control.state = state_of(object);
    control.type = object.type;
    return construct_runtime_scene_control(control, installing->context.lists);
}

void destroy_scene_control_view(FactoryObject& object)
{
    destroy_runtime_scene_control(installing->instance(object));
    installing->forget(object); // Type 3 is the last scene-control destructor in the chain.
}

bool construct_walkabout_view(FactoryObject& object, std::span<const u8>)
{
    auto& control = installing->instance(object);
    control.state = state_of(object);
    control.type = object.type;
    return construct_runtime_walkabout_control(state_of(object), control, installing->context);
}

void destroy_walkabout_view(FactoryObject& object)
{
    destroy_runtime_walkabout_control(state_of(object), installing->instance(object), installing->context);
}

const ObjectFactoryVtable* scene_control_lifecycle()
{
    static const ObjectFactoryVtable table{0x0050E41C, scene_control_object_sizes, construct_scene_control_view,
        destroy_scene_control_view};
    return &table;
}

const ObjectFactoryVtable* walkabout_control_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505530, walkabout_control_object_sizes, construct_walkabout_view,
        destroy_walkabout_view};
    return &table;
}

const char* walkabout_name(u32 track, const SceneControlContext& context)
{
    if (track >= context.walkabout.size())
    {
        throw Error("walkabout track lies past the 0x002FFFB8 name table");
    }
    return context.walkabout[track];
}
}

NativeSceneControlOwner::NativeSceneControlOwner(SceneControlContext control_context)
    : context(std::move(control_context))
{
}

const RuntimeActorClock& SceneControlContext::frame_clock() const
{
    if (clock == nullptr)
    {
        throw Error("scene control tick before the scene's actor clock is bound");
    }
    return *clock;
}

AudioBankStreams& SceneControlContext::streams() const
{
    if (audio == nullptr)
    {
        throw Error("scene control audio before the scene's bank streams are bound");
    }
    return *audio;
}

void NativeSceneControlOwner::install(std::span<ObjectFactoryType> types)
{
    installing = this;
    types[3] = ObjectFactoryType{1, scene_control_lifecycle};
    types[0x91] = ObjectFactoryType{3, walkabout_control_lifecycle};
}

SceneControlInstance& NativeSceneControlOwner::instance(const FactoryObject& object)
{
    auto& slot = instances_[object.bytes.get()];
    if (slot == nullptr)
    {
        slot = std::make_unique<SceneControlInstance>();
    }
    return *slot;
}

SceneControlInstance& NativeSceneControlOwner::instance_of(RuntimeActorLink& link)
{
    for (auto& [bytes, control] : instances_)
    {
        if (&control->link == &link)
        {
            return *control;
        }
    }
    throw Error("scene control list node has no native control");
}

SceneControlInstance& NativeSceneControlOwner::instance_of_update(RuntimeActorLink& link)
{
    for (auto& [bytes, control] : instances_)
    {
        if (&control->update == &link)
        {
            return *control;
        }
    }
    throw Error("scene control update node has no native control");
}

FactoryObject& NativeSceneControlOwner::object_of_disposal(RuntimeActorLink& link,
    std::vector<FactoryObject>& objects)
{
    for (auto& [bytes, control] : instances_)
    {
        if (&control->disposal == &link)
        {
            for (auto& object : objects)
            {
                if (object.bytes.get() == bytes)
                {
                    return object;
                }
            }
        }
    }
    throw Error("scene control disposal node has no scene object");
}

void NativeSceneControlOwner::forget(const FactoryObject& object)
{
    instances_.erase(object.bytes.get());
}

void NativeSceneControlOwner::message(FactoryObject& object, std::span<const u32> words)
{
    // Types 3 and 0x91 share 0x00115EE0.
    message_runtime_scene_control(instance(object), words, context.lists);
}

void NativeSceneControlOwner::tick(SceneControlInstance& control)
{
    switch (control.type)
    {
    case 3:
        tick_runtime_scene_control(control.state, context.frame_clock());
        return;
    case 0x91:
        tick_runtime_walkabout_control(control.state, control, context);
        return;
    default:
        throw Error("scene control tick for factory type " + std::to_string(control.type));
    }
}

// Xbox 0x00115DA0
bool construct_runtime_scene_control(SceneControlInstance& control, SceneControlLists& lists)
{
    append(lists.pending, control.link);
    return true;
}

// Xbox 0x00115DE0
void destroy_runtime_scene_control(SceneControlInstance& control)
{
    unlink(control.link);
    unlink(control.disposal);
}

// Xbox 0x00115E70
u32 retire_runtime_scene_control(SceneControlInstance& control, SceneControlLists& lists)
{
    if (control.disposal.previous != nullptr)
    {
        return 0;
    }
    append(lists.disposal, control.disposal);
    unlink(control.link);
    return 1;
}

// Xbox 0x00115EE0
void message_runtime_scene_control(SceneControlInstance& control, std::span<const u32> message,
    SceneControlLists& lists)
{
    if (message[0] == 0x80)
    {
        retire_runtime_scene_control(control, lists);
    }
}

// Xbox 0x00115E30
void tick_runtime_scene_control(std::span<u8> state, const RuntimeActorClock& clock)
{
    const float time = read_f32(state, 0x20) + runtime_frame_step(clock);
    std::memcpy(state.data() + 0x20, &time, 4);
}

// Xbox 0x00080670
void build_runtime_scene_playlist(std::span<u8> state, std::vector<u16>& playlist, u32 count, u32& random)
{
    // +0x24 holds the playlist (0x000F72D0); the pool of remaining indices is a temporary allocation.
    playlist.assign(count, 0);
    put_word(state, 0x2C, count);
    put_word(state, 0x28, 0);
    std::vector<u16> pool;
    for (u32 index = 0; i32(index) < i32(count); ++index)
    {
        pool.push_back(u16(index));
    }
    u32 remaining = count;
    size_t slot = 0;
    while (0 < i32(remaining))
    {
        random = random * 0x19660D + 0x3C6EF35F;
        const float unit = std::bit_cast<float>((random & 0x7FFFFF) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
        const size_t picked = size_t(i32(unit * float(i32(remaining))));
        playlist[slot++] = pool.at(picked);
        pool.erase(pool.begin() + std::ptrdiff_t(picked));
        --remaining;
    }
}

// Xbox 0x00080570
bool construct_runtime_walkabout_control(std::span<u8> state, SceneControlInstance& control,
    SceneControlContext& context)
{
    build_runtime_scene_playlist(state, control.playlist, 0x2A, context.random);
    put_word(state, 0x30, 0xFFFFFFFFu);
    put_word(state, 0x34, 2);
    put_word(state, 0x38, context.chapter() == 9 ? 1 : 0);
    return true;
}

// Xbox 0x00080810
void stop_runtime_walkabout_track(std::span<u8> state, SceneControlContext& context)
{
    const u32 track = word(state, 0x30);
    if (track == 0xFFFFFFFFu)
    {
        return;
    }
    const char* name = word(state, 0x38) == 0 ? walkabout_name(track, context) : context.timer;
    AudioTrackedBank* bank = find_tracked_audio_bank(name, context.streams());
    if (bank == nullptr)
    {
        throw Error("walkabout track bank is not tracked; 0x00080810 reads its +0x88");
    }
    const i32 references = bank->references - 1;
    bank->references = references;
    if (references < 1)
    {
        if (bank->id != 0x12)
        {
            if (AudioTrackedBank* resumed = select_resume_audio_bank(*bank, context.streams()); resumed != nullptr)
            {
                resume_tracked_audio_bank(*resumed, context.streams());
            }
        }
        detach_tracked_audio_bank(*bank, context.streams());
        bank->id = 0;
    }
    put_word(state, 0x30, 0xFFFFFFFFu);
}

// Xbox 0x000808B0
void play_runtime_walkabout_track(std::span<u8> state, SceneControlInstance& control,
    SceneControlContext& context)
{
    stop_runtime_walkabout_track(state, context);
    const u32 position = word(state, 0x28);
    const u16 track = control.playlist.at(position);
    put_word(state, 0x28, position + 1);
    if (position + 1 == word(state, 0x2C))
    {
        put_word(state, 0x28, 0);
    }
    put_word(state, 0x30, track);
    if (word(state, 0x38) != 0)
    {
        acquire_tracked_audio_bank(context.timer, 2, context.streams());
        return;
    }
    acquire_tracked_audio_bank(walkabout_name(track, context), 2, context.streams());
}

// Xbox 0x000805B0
void destroy_runtime_walkabout_control(std::span<u8> state, SceneControlInstance& control,
    SceneControlContext& context)
{
    stop_runtime_walkabout_track(state, context);
    control.playlist.clear(); // HeapFree of +0x24.
}

// Xbox 0x00080600
void tick_runtime_walkabout_control(std::span<u8> state, SceneControlInstance& control,
    SceneControlContext& context)
{
    const float time = read_f32(state, 0x20) + runtime_frame_step(context.frame_clock());
    std::memcpy(state.data() + 0x20, &time, 4);
    const u32 countdown = word(state, 0x34);
    if (countdown != 0)
    {
        put_word(state, 0x34, countdown - 1);
        if (countdown - 1 == 0)
        {
            play_runtime_walkabout_track(state, control, context);
        }
    }
    const u32 track = word(state, 0x30);
    if (track != 0xFFFFFFFFu && runtime_audio_bank_state(walkabout_name(track, context), context.streams()) == 2)
    {
        play_runtime_walkabout_track(state, control, context);
    }
}

// Xbox 0x000389C0
i32 runtime_audio_bank_state(const char* name, AudioBankStreams& context)
{
    const AudioTrackedBank* bank = find_tracked_audio_bank(name, context);
    if (bank == nullptr)
    {
        return 1;
    }
    if (bank->silence != 0)
    {
        return 3;
    }
    if (bank->held != 0 || std::int8_t(bank->channel) == -1)
    {
        return 2;
    }
    return runtime_audio_channel_idle(std::int8_t(bank->channel), context) == 0 ? 3 : 2;
}

// Xbox 0x00038EF0
u32 runtime_audio_channel_idle(std::int8_t index, AudioBankStreams& context)
{
    const AudioTrackedChannel& channel = context.channels.at(size_t(index));
    if (channel.mode == 3)
    {
        return 0;
    }
    const AudioEventVoice& voice = context.voices.voices.at(size_t(channel.voice));
    if (voice.cue != nullptr)
    {
        const u32 state = voice.state;
        if (state == 2 || state == 3 || state == 5 || state == 6 || state == 7)
        {
            return 0;
        }
    }
    return 1;
}

void tick_runtime_scene_control_list()
{
    if (g_scene_control_owner == nullptr)
    {
        return;
    }
    auto& owner = *g_scene_control_owner;
    RuntimeActorLink* node = owner.context.lists.active.next;
    while (node != nullptr)
    {
        RuntimeActorLink* next = node->next;
        owner.tick(owner.instance_of(*node));
        node = next;
    }
}
}
