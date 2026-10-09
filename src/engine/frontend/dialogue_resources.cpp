#include "dialogue_resources.h"
#include "../runtime/crt_format.h"
#include <cstdarg>

namespace gbtg
{
// Xbox 0x00035830
void format_dialogue_resource_name(char* output, const char* format, ...)
{
    std::va_list arguments;
    va_start(arguments, format);
    scene_detail::crt_format_bounded(output, 0x80, format, arguments);
    va_end(arguments);
    char character = *output;
    while (character != 0)
    {
        character = *output;
        if (character >= 'A' && character <= 'Z')
        {
            *output = character + 0x20;
        }
        ++output;
        character = *output;
    }
}

// Xbox 0x00057970
void select_dialogue_icon(DialogueOwnedResources& state, i32 requested, u8 group,
    char* name_buffer, const DialogueTokenNames& names,
    AnimationResourceRegistry& registry, ResourceErrorState& error)
{
    if (requested == static_cast<i32>(state.icon))
    {
        return;
    }
    if (state.icon_resource.data() != nullptr)
    {
        release_animation_resource(registry, state.icon_resource);
        state.icon_resource = {};
    }
    state.icon = requested;
    if (requested == 0x36)
    {
        return;
    }
    switch (group)
    {
    case 0:
        format_dialogue_resource_name(name_buffer, "aid_texture_ghoulies_scoreboard_eventicons_%s", names[requested]);
        break;
    case 1:
        format_dialogue_resource_name(name_buffer, "aid_texture_ghoulies_scoreboard_pauseicons_%s", names[requested]);
        break;
    case 2:
        format_dialogue_resource_name(name_buffer, "aid_texture_ghoulies_scoreboard_challenge_%s", names[requested]);
        break;
    case 3:
        format_dialogue_resource_name(name_buffer, "aid_texture_ghoulies_weapon_%s", names[requested]);
        break;
    case 4:
        format_dialogue_resource_name(name_buffer, "aid_texture_ghoulies_misc_%s", names[requested]);
        break;
    default:
        break;
    }
    // For an out-of-range group the dump still reads its unchanged stack
    // buffer. The native caller supplies that buffer; no name is invented.
    if (read_u32({reinterpret_cast<const u8*>(name_buffer), 4}, 0) == 0x5F646961u)
    {
        auto acquired = acquire_animation_resource(registry, name_buffer);
        if (acquired.data() == nullptr)
        {
            resource_error(error, 2);
        }
        state.icon_resource = acquired;
        return;
    }
    state.icon_resource = {};
}

// Xbox 0x000576D0
void select_dialogue_speaker(DialogueOwnedResources& state, i32 requested,
    u8 mode_flags, u32 scene_flags, const DialogueTokenNames& names,
    AnimationResourceRegistry& registry, ResourceErrorState& error)
{
    if (state.speaker_resource.data() != nullptr)
    {
        release_animation_resource(registry, state.speaker_resource);
        state.speaker_resource = {};
    }
    if ((mode_flags & 8) != 0 || (scene_flags & 0x200000) != 0)
    {
        if (requested == 0)
        {
            state.speaker = 1;
            goto load;
        }
        if (requested == 1)
        {
            state.speaker = 0;
            goto load;
        }
    }
    state.speaker = requested;
    if (requested == -1)
    {
        state.speaker_state = 0;
        return;
    }
load:
    char resource_name[0x84];
    format_dialogue_resource_name(resource_name, "aid_texture_ghoulies_scoreboard_dialogheads_%s", names[state.speaker]);
    std::span<const u8> acquired;
    if (read_u32({reinterpret_cast<const u8*>(resource_name), 4}, 0) == 0x5F646961u)
    {
        acquired = acquire_animation_resource(registry, resource_name);
        if (acquired.data() == nullptr)
        {
            resource_error(error, 2);
        }
    }
    if (acquired.data() != nullptr)
    {
        state.speaker_frame = 0;
        state.speaker_height = acquired[0x11];
    }
    state.speaker_state = 0;
    state.speaker_resource = acquired;
}

// Xbox 0x00057400
std::uintptr_t release_dialogue_speaker(DialogueOwnedResources& state,
    AnimationResourceRegistry& registry, std::uintptr_t mode_identity,
    u8 mode_flags, u32 scene_flags)
{
    state.previous_speaker = -1;
    if (state.speaker_resource.data() != nullptr)
    {
        release_animation_resource(registry, state.speaker_resource);
        state.speaker_resource = {};
    }
    std::uintptr_t result = mode_identity;
    if ((mode_flags & 8) == 0)
    {
        result = scene_flags;
    }
    state.speaker = -1;
    state.next_speaker = -1;
    state.speaker_state = 0;
    return result;
}

// Xbox 0x000577B0
void release_dialogue_icon(DialogueOwnedResources& state, AnimationResourceRegistry& registry)
{
    if (state.icon_resource.data() != nullptr)
    {
        release_animation_resource(registry, state.icon_resource);
        state.icon = 0x36;
        state.icon_resource = {};
    }
}
}
