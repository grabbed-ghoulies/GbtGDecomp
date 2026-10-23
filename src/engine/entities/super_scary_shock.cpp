#include "super_scary_shock.h"
#include "actor_render_registration.h"
#include "actor_transform.h"
#include "actor_world_lists.h"
#include "area_actor.h"
#include "../audio/bank_streams.h"
#include "../cutscenes/door_registration.h"
#include "../scene/trophy_heads_control.h"
#include "gbtg/entity.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = super_scary_shock_field;
namespace parameter = super_scary_shock_parameter;
constexpr const char* kDitty = "XACT_SOUNDBANK_GSTREAMSDITTIES_UGLYGIRLTURNAROUND"; // 0x00427438

template <u32 Offset>
const char* text(RuntimeSuperScaryShock& context, RetailRecord<Offset, 0x80> name)
{
    return reinterpret_cast<const char*>(record(context.bytes, name).data());
}

void copy_name(std::span<const u8> source, std::span<u8> target)
{
    size_t index = 0;
    do
    {
        target[index] = source[index];
    }
    while (source[index++] != 0);
}

void copy_text(std::span<u8> target, const char* source)
{
    std::memcpy(target.data(), source, std::strlen(source) + 1);
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

// The roll 0x000722B0 and 0x000728C0 inline: DAT_0054DD7C's next value in [0, 1).
float next_random(RuntimeSuperScaryShock& context)
{
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
}
}

// Xbox 0x00072250
void super_scary_shock_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = field::kSize;
    sizes.parameters = parameter::kSize;
}

// Xbox 0x000722B0
bool initialize_runtime_super_scary_shock(RuntimeSuperScaryShock& context, std::span<const u8> parameters)
{
    if (next_random(context) > get(parameters, parameter::chance))
    {
        return false;
    }
    auto bytes = context.bytes;
    set(bytes, field::reach, get(parameters, parameter::reach));
    copy_name(record(parameters, parameter::ghouly_name), record(bytes, field::ghouly_name));
    copy_name(record(parameters, parameter::ghouly_template), record(bytes, field::ghouly_template));
    set(bytes, field::clip_time_base, get(parameters, parameter::clip_time));
    set(bytes, field::clip_time_range, get(parameters, parameter::clip_time_range));
    copy_name(record(parameters, parameter::first_clip), record(bytes, field::first_clip));
    copy_name(record(parameters, parameter::attack_animation), record(bytes, field::attack_animation));
    copy_name(record(parameters, parameter::attack_events), record(bytes, field::attack_events));
    copy_name(record(parameters, parameter::second_clip), record(bytes, field::second_clip));
    copy_name(record(parameters, parameter::end_animation), record(bytes, field::end_animation));
    copy_name(record(parameters, parameter::end_events), record(bytes, field::end_events));
    set(bytes, field::challenge_word, get(parameters, parameter::challenge_word));
    set(bytes, field::ditty_started, 0u);
    set(bytes, field::ditty_held, 0u);
    set(bytes, field::ghouly, 0u);
    return true;
}

// Xbox 0x00072420
void destroy_runtime_super_scary_shock(RuntimeSuperScaryShock& context)
{
    if (get(context.bytes, field::ditty_held) == 0)
    {
        return;
    }
    AudioTrackedBank* bank = find_tracked_audio_bank(kDitty, context.streams); // 0x00038460
    if (bank == nullptr)
    {
        throw Error("Super Scary shock ditty is not tracked; 0x00072420 reads its +0x88");
    }
    bank->references = bank->references - 1;
    if (bank->references < 1)
    {
        if (bank->id != 0x12)
        {
            if (AudioTrackedBank* resumed = select_resume_audio_bank(*bank, context.streams); resumed != nullptr)
            {
                resume_tracked_audio_bank(*resumed, context.streams); // 0x00038590, 0x00038360
            }
        }
        detach_tracked_audio_bank(*bank, context.streams); // 0x00038260
        bank->id = 0;
    }
}

// Xbox 0x00072480
void update_runtime_super_scary_shock(RuntimeSuperScaryShock& context, const std::function<void()>& update_actor)
{
    if (get(context.bytes, field::ditty_started) == 0)
    {
        acquire_tracked_audio_bank(kDitty, 0xF, context.streams); // 0x00038910
        set(context.bytes, field::ditty_started, 1u);
        set(context.bytes, field::ditty_held, 1u);
    }
    update_actor(); // 0x000420D0
    watch_runtime_super_scary_shock(context); // 0x00072750
}

// Xbox 0x000724C0
void dispatch_runtime_super_scary_shock_message(RuntimeSuperScaryShock& context, std::span<u32> message)
{
    context.base_message(message); // 0x0003EF70
    auto bytes = context.bytes;
    switch (message[0])
    {
    case 0x23:
        unregister_actor_segment_probes(context.view.render); // 0x0003E850, EAX actor
        clear_actor_shape_probe(context.view.bat.actor, context.view.shape); // 0x0003C700
        return;
    case 0x88:
        watch_runtime_super_scary_shock(context); // 0x00072750, EBX actor
        return;
    case 0x8E:
        break;
    default:
        return;
    }
    switch (message[1])
    {
    case 0:
        play_runtime_super_scary_ghouly_clip(context, get(bytes, field::ghouly), get(bytes, field::clip_time),
            text(context, field::attack_animation), 1, text(context, field::attack_events)); // 0x00072980
        return;
    case 2:
        play_runtime_super_scary_ghouly_clip(context, get(bytes, field::ghouly), -1.f,
            text(context, field::end_animation), 0, text(context, field::end_events)); // 0x00072980
        return;
    case 4:
        release_tracked_audio_bank(kDitty, context.streams); // 0x00038970
        set(bytes, field::ditty_held, 0u);
        return;
    case 7:
    case 8:
    {
        std::array<u32, 0x42> end{0xA};
        context.message(get(bytes, field::ghouly), end); // Vtable +0xC.
        activate_actor_bind_tree(context.view.bat.actor); // 0x00112CC0, ESI the shock
        const u32 notice = 0xB3;
        context.queue_event({reinterpret_cast<const u8*>(&notice), sizeof(notice)}); // 0x00116810, EDI the notice
        return;
    }
    default:
        return;
    }
}

// Xbox 0x00072750, EBX actor
void watch_runtime_super_scary_shock(RuntimeSuperScaryShock& context)
{
    if (runtime_area_dialogue_active(context.dialogue)) // 0x0005B830
    {
        return;
    }
    if (context.prompt_lock > -1 || get(context.bytes, field::ghouly) != 0 || (context.runtime_flags & 8u) != 0)
    {
        return;
    }
    const Vec3 position = actor_world_position(context.view.bat.actor); // 0x000412A0
    const Actor* player = context.player();
    if (player == nullptr)
    {
        return;
    }
    const Vec3 player_position = actor_world_position(*player); // 0x000412A0
    if (!(get(context.bytes, field::reach) > float(runtime_point_distance(player_position, position)))) // 0x00146CE0
    {
        return;
    }
    const u32 ghouly = spawn_runtime_super_scary_ghouly(context); // 0x000727F0
    set(context.bytes, field::ghouly, ghouly);
    if (ghouly == 0)
    {
        throw Error("Super Scary shock has no ghouly template; 0x00072750 messages the null object");
    }
    std::array<u32, 0x42> start{9};
    context.message(ghouly, start); // Vtable +0xC.
    start_runtime_super_scary_challenge(context); // 0x000728C0, EDX actor
}

// Xbox 0x000727F0, EAX actor
u32 spawn_runtime_super_scary_ghouly(RuntimeSuperScaryShock& context)
{
    const Vec3 position = actor_world_position(context.view.bat.actor); // 0x000412A0
    const Vec3 euler = actor_world_euler(context.view.bat.actor); // 0x00041820
    const char* name = text(context, field::ghouly_name);
    return context.create_from_template(text(context, field::ghouly_template), [&](std::span<u8> clone)
        {
            std::memcpy(clone.data() + 8, &position, 12);
            std::memcpy(clone.data() + 0x14, &euler, 12);
            copy_text(clone.subspan(0x28), name);
            put_word(clone, 0x2B0, 1);
            copy_text(clone.subspan(0x2B4), name);
        }); // 0x00107720, 0x00107520, 0x000663E0
}

// Xbox 0x000728C0, EDX actor
void start_runtime_super_scary_challenge(RuntimeSuperScaryShock& context)
{
    auto bytes = context.bytes;
    float time = get(bytes, field::clip_time_base);
    if (time != -1.f) // DAT_0032B120.
    {
        time = next_random(context) * get(bytes, field::clip_time_range) + get(bytes, field::clip_time_base);
    }
    set(bytes, field::clip_time, time);
    std::array<u8, 0x1D8> parameters{};
    build_runtime_scene_challenge_parameters(parameters, text(context, field::first_clip), get(bytes, field::ghouly),
        time, -1.f, nullptr, nullptr, 0, nullptr, get(bytes, field::challenge_word), 0, 0,
        text(context, field::second_clip), context.runtime_flags); // 0x000873A0
    context.create_object(parameters); // 0x00107520
}

// Xbox 0x00072980, EAX events
void play_runtime_super_scary_ghouly_clip(RuntimeSuperScaryShock& context, u32 ghouly, float time,
    const char* animation, u16 mode, const char* events)
{
    // {0xB, slot 0, the mix at +8}: "PxiM", -10 (DAT_0032B1FC), blend 0x23, the mode, 0x31, the events at +0x24 and
    // the animation at +0xB0.
    std::array<u32, 0x51> words{};
    words[0] = 0xB;
    words[1] = 0;
    const std::span<u8> mix{reinterpret_cast<u8*>(words.data()) + 8, words.size() * 4 - 8};
    put_word(mix, 0x00, 0x4D697850u);
    put_word(mix, 0x08, std::bit_cast<u32>(-10.f));
    put_word(mix, 0x0C, 0x23u);
    std::memcpy(mix.data() + 0x18, &mode, 2);
    put_word(mix, 0x1C, 0x31u);
    copy_text(mix.subspan(0x24), events);
    copy_text(mix.subspan(0xB0), animation);
    context.message(ghouly, words); // Vtable +0xC.
    if (time == -1.f) // DAT_0032B120.
    {
        return;
    }
    AnimationController* controller = context.actor_at(ghouly).animation.controller.get(); // +0x50.
    MinaPlayer* player = controller != nullptr && !controller->slots.empty() ? controller->slots[0].player.get()
                                                                            : nullptr; // +0x58.
    const float duration = player != nullptr ? player->duration : 0.f; // +0x20.
    const float position = 1.f - (time + 1.f / 15.f) / duration; // DAT_0032B128, DAT_0032B0F8.
    if (controller == nullptr)
    {
        return;
    }
    if (player != nullptr)
    {
        player->position = position; // +0x14.
        player->previous = position; // +0x10C.
    }
    controller->dirty = true; // +0x5C.
}
}
