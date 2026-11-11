#include "dialogue_activation.h"
#include "dialogue_sound_tags.h"
#include "../camera/view_initialization.h"
#include "../scripts/script_actions.h"
#include "../cutscenes/actor_tracks.h"
#include "../cutscenes/event_runtime.h"
#include <bit>
#include <xmmintrin.h>

namespace gbtg
{
// Xbox 0x00058B80
void accept_runtime_dialogue_button(RuntimeDialogueActivationState& state)
{
    state.completion = 1;
}

// Xbox 0x0012B2A0
u32 advance_runtime_dialogue_timer(float& timer, float freeze, float dt, float scale)
{
    float step;
    if (!(freeze > 0.f))
    {
        step = dt * scale;
    }
    else
    {
        step = 0.f;
    }
    if (0.f < timer)
    {
        timer -= step;
        if (timer <= 0.f)
        {
            timer = 0.f;
            return 1;
        }
    }
    return 0;
}

// Xbox 0x00057450
void update_runtime_dialogue_speaker(DialogueOwnedResources& state, float& timer,
    u32& random, float freeze, float dt, float scale)
{
    if (state.speaker != -1)
    {
        if (state.speaker_state == 0)
        {
            random = random * 0x19660Du + 0x3C6EF35Fu;
            if (std::bit_cast<float>((random & 0x7FFFFF) | 0x3F800000u) - 1.f
                <= std::bit_cast<float>(0x3C23D70Au))
            {
                state.speaker_state = 1;
                timer = std::bit_cast<float>(0x3DCCCCCDu);
            }
        }
        else if (state.speaker_state == 1)
        {
            state.speaker_frame = static_cast<u32>(_mm_cvtt_ss2si(_mm_set_ss(
                (1.f - timer * 10.f) * float(std::bit_cast<i32>(state.speaker_height - 1)))));
            float step;
            if (!(freeze > 0.f))
            {
                step = dt * scale;
            }
            else
            {
                step = 0.f;
            }
            if (0.f < timer)
            {
                timer -= step;
                if (timer <= 0.f)
                {
                    state.speaker_state = 2;
                    state.speaker_frame = state.speaker_height - 1;
                    timer = std::bit_cast<float>(0x3DCCCCCDu);
                    return;
                }
            }
        }
        else if (state.speaker_state == 2)
        {
            state.speaker_frame = static_cast<u32>(_mm_cvtt_ss2si(_mm_set_ss(
                timer * 10.f * float(std::bit_cast<i32>(state.speaker_height - 1)))));
            float step;
            if (!(freeze > 0.f))
            {
                step = dt * scale;
            }
            else
            {
                step = 0.f;
            }
            if (0.f < timer)
            {
                timer -= step;
                if (timer <= 0.f)
                {
                    timer = 0.f;
                    state.speaker_state = 0;
                    state.speaker_frame = 0;
                    return;
                }
            }
        }
    }
}

// Xbox 0x00058AE0
void advance_runtime_dialogue_page(RuntimeDialogueActivationContext& context)
{
    auto& state = context.state;
    auto& tags = context.measurement.tags;
    if (*context.page.next_text != 0)
    {
        if (tags.state.heads != 0)
        {
            tags.trophies.resources.previous_speaker = tags.trophies.resources.speaker;
        }
        prepare_dialogue_page(context.page, context.page.next_text, context.settings.line_limit,
            context.wrap_width, context.measurement);
        context.settings.initial_alignment = tags.state.alignment;
        if (tags.state.heads != 0)
        {
            tags.trophies.resources.next_speaker = find_next_dialogue_speaker(
                context.page.text, tags.trophies.resources.speaker, tags.tokens);
        }
        set_runtime_dialogue_mode(1, context);
        return;
    }
    set_runtime_dialogue_mode(4, context);
}

// Xbox 0x000584D0
void set_runtime_dialogue_mode(i32 mode, RuntimeDialogueActivationContext& context)
{
    auto& state = context.state;
    auto& tags = context.measurement.tags;
    auto& resources = tags.trophies.registry;
    auto& error = tags.trophies.error;
    if (mode != state.mode)
    {
        if (state.mode == 0)
        {
            ++context.pause_count;
            ++context.frontend_pause_count;
            if ((context.scene_flags & 0x20) == 0)
            {
                script_update_decay_latches(0xFFFF, 0);
            }
            if (state.widget == nullptr)
            {
                create_resource_dialogue_widget(state.widget, 0, nullptr,
                    {float(context.viewport_width) * 0.5f, context.viewport_bottom - 16.f},
                    {0.f, -72.f}, {40.f, -36.f}, nullptr, 1.5f, 0.75f,
                    "menutext", "aid_loctext_ghoulies_menu_menutext", 0x80, context.allocation);
                const float duration = std::bit_cast<float>(0x3E99999Au);
                state.widget->duration = duration;
                if (duration < state.widget->progress)
                {
                    state.widget->progress = duration;
                }
                state.widget->pool->end_time = duration;
                attach_dialogue_widget_bat(*state.widget, 2.f, 0, context.bats);
            }
            context.scene_flags |= 0x40;
            context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
            const float random = std::bit_cast<float>((context.random & 0x7FFFFF) | 0x3F800000u) - 1.f;
            initialize_camera_view(state.camera, context.camera_name, 8);
            link_camera_view(0, state.camera);
            if ((context.mode_flags & 0x40) != 0)
            {
                set_cutscene_actor_replacement(context.replacement_source, context.replacement_target,
                    "aid_model_ghoulies_actor_dialogbat", "aid_model_ghoulies_actor_dialogbatgold");
            }
            state.cutscene = load_runtime_cutscene(context.page_names + i32(random) * 0x80,
                6, &state.camera, context.cutscenes);
            if (state.entries[state.read].flag == 0)
            {
                camera_bind_named_view(0, context.camera_name);
            }
            state.camera_selected = state.entries[state.read].flag == 0;
            reset_dialogue_page(context.page, state.entries[state.read].text.data(),
                state.entries[state.read].flag, context.wrap_width,
                context.settings, context.measurement);
        }
        else if (state.mode == 1)
        {
            state.elapsed = 0.f;
        }
        switch (mode)
        {
        case 0:
            state.visible = 0;
            set_dialogue_audio_duck(context.listener, 0, context.scene_flags,
                context.first_category, context.second_category);
            set_audio_reverb_preset(context.listener, state.saved_reverb, resources, error);
            --context.pause_count;
            context.scene_flags &= ~0x40u;
            --context.frontend_pause_count;
            release_runtime_cutscene(state.cutscene, context.cutscenes);
            state.cutscene = nullptr;
            camera_unlink_view(&state.camera);
            if (state.input.data() != nullptr && read_u32(state.input, 4) != 0)
            {
                pop_runtime_input_entry(context.input);
                release_runtime_input_device(state.input, context.input_release);
                state.input = {};
            }
            if (state.widget != nullptr)
            {
                destroy_dialogue_widget(state.widget, resources, *context.allocation.pools);
                state.widget = nullptr;
            }
            release_dialogue_speaker(tags.trophies.resources, resources, 0,
                tags.mode_flags, context.scene_flags);
            state.button = 0;
            release_dialogue_icon(tags.trophies.resources, resources);
            state.count = 0;
            state.write = 0;
            state.read = 0;
            for (auto& entry : state.entries)
            {
                entry.text[0] = 0;
            }
            if ((context.scene_flags & 0x20) == 0)
            {
                script_update_decay_latches(0xFFFF, 1);
                state.mode = mode;
                return;
            }
            break;
        case 1:
            state.duration = std::bit_cast<float>(0x3E99999Au);
            state.visible = 1;
            state.progress = 0.f;
            set_dialogue_button_enabled(tags.state.buttons, tags.state.buttons, state.button);
            if (state.button != 0)
            {
                set_dialogue_widget_prompt(*state.widget, context.prompt_parameters, context.allocation.widget);
                state.mode = mode;
                return;
            }
            break;
        case 2:
            if (tags.state.heads != 0)
            {
                request_dialogue_page_voice(tags.sound, context.voices);
            }
            state.duration = tags.state.time;
            state.visible = 1;
            state.progress = 1.f;
            state.completion = 0;
            state.mode = mode;
            return;
        case 3:
            state.button = tags.state.buttons != 0;
            state.duration = std::bit_cast<float>(0x3E99999Au);
            state.visible = 1;
            state.progress = 1.f;
            if (state.button != 0)
            {
                state.widget->requested_set = nullptr;
                set_dialogue_widget_mode(*state.widget, 3, context.allocation.widget);
            }
            if (tags.state.invoke_map != 0)
            {
                context.bats.objects.push_back(create_dialogue_map_control(context.bats.factory, context.bats.resources));
                state.mode = mode;
                return;
            }
            break;
        case 4:
            state.duration = 0.f;
            state.button = 0;
            state.visible = 0;
            state.progress = 0.f;
            state.mode = mode;
            return;
        case 5:
            tags.state.invoke_map = 0;
            state.map_retained = 0;
        }
        state.mode = mode;
    }
}

// Xbox 0x000582E0
u32 enqueue_runtime_dialogue(const char16_t* text, u8 flag, RuntimeDialogueActivationContext& context)
{
    auto& state = context.state;
    if (8 < std::bit_cast<i32>(state.count + 1))
    {
        return 0;
    }
    copy_xapi_wide_text(state.entries[state.write].text.data(), text);
    state.entries[state.write].flag = flag;
    ++state.write;
    if (7 < std::bit_cast<i32>(state.write))
    {
        state.write = 0;
    }
    ++state.count;
    if (state.mode == 0)
    {
        set_runtime_dialogue_mode(1, context);
    }
    if ((context.scene_flags & 0x20) == 0)
    {
        set_audio_category_volume(context.listener, context.first_category, 0);
        set_audio_category_volume(context.listener, context.second_category, 0x3F);
    }
    state.saved_reverb = context.listener.preset;
    set_audio_reverb_preset(context.listener, 1,
        context.measurement.tags.trophies.registry, context.measurement.tags.trophies.error);
    return 1;
}

// Xbox 0x000582A0
u32 enqueue_runtime_dialogue_key(const char* key, u8 flag, RuntimeDialogueActivationContext& context)
{
    if (8 < std::bit_cast<i32>(context.state.count + 1))
    {
        return 0;
    }
    request_namespaced_localtext("aid_loctext_ghoulies_dialog_dialogs", "dialogs", key,
        context.measurement.wide_scratch, context.measurement.localtext);
    return enqueue_runtime_dialogue(context.measurement.wide_scratch, flag, context);
}

// Xbox 0x000583A0
void close_runtime_dialogue(RuntimeDialogueActivationContext& context)
{
    if (context.state.mode != 0)
    {
        set_runtime_dialogue_mode(0, context);
    }
}

// Xbox 0x00057E60
void update_runtime_dialogue(RuntimeDialogueActivationContext& context)
{
    auto& state = context.state;
    auto& tags = context.measurement.tags;
    if (state.cutscene == nullptr)
    {
        if (state.count == 0)
        {
            return;
        }
        set_runtime_dialogue_mode(1, context);
    }
    if (tags.state.buttons != 0)
    {
        if (state.input.data() == nullptr)
        {
            const u32 stride = context.input.entries->stride;
            const u8* selected = nullptr;
            if ((context.input.entries->cursor - context.input.entries->begin) / stride != 0)
            {
                const u8* row = context.input.entries->begin + (context.input.selected - 1) * stride;
                selected = context.input.devices.at(read_u32({row, stride}, 0)).data();
            }
            if (read_u32({selected, 4}, 0) == 3)
            {
                state.input = create_runtime_keyboard_input(context.input_release);
            }
            else
            {
                state.input = create_runtime_selected_input(context.input_release);
            }
            // Native pointer mapping; the retail queue stores the allocated address.
            state.input_address = static_cast<u32>(reinterpret_cast<std::uintptr_t>(state.input.data()));
            context.input_addresses[state.input_address] = state.input;
            context.input_views[state.input_address] = {state.input, {nullptr, nullptr, nullptr, nullptr}};
            push_runtime_input_entry(state.input_address, context.camera_name, context.input);
        }
        refresh_runtime_input_device(state.input_address, context.frame, context.input, context.input_release);
        context.widget_update.input.devices = context.input.entries;
        context.widget_update.input.selected = &context.input.selected;
        context.widget_update.input.device_views = &context.input_views;
        const RuntimeDialogueInputDevice device{state.input, {nullptr, nullptr, nullptr, nullptr}};
        update_dialogue_widget(*state.widget, &device, context.widget_update);
    }
    if (state.cutscene->duration <= state.cutscene->time)
    {
        seek_runtime_cutscene(*state.cutscene, 0.f, 0.f, 0, context.cutscenes);
    }
    float step = 0.f;
    if (!(context.freeze > 0.f))
    {
        step = context.dt * context.scene_scale;
    }
    tick_runtime_cutscene(*state.cutscene, step, &state.camera, context.cutscenes);
    if (tags.state.heads != 0)
    {
        update_runtime_dialogue_speaker(tags.trophies.resources, state.speaker_timer,
            context.random, context.freeze, context.dt, context.scene_scale);
    }
    switch (state.mode)
    {
    case 1:
    {
        const float fraction = (state.duration - std::bit_cast<float>(0x3E99999Au))
            * std::bit_cast<float>(0xC0555555u);
        if (!(fraction > 1.f))
        {
            state.progress = 0.f;
            if (!(0.f > fraction))
            {
                state.progress = fraction;
            }
        }
        else
        {
            state.progress = 1.f;
        }
        if (advance_runtime_dialogue_timer(state.duration, context.freeze, context.dt, context.scene_scale) != 0)
        {
            set_runtime_dialogue_mode(2, context);
        }
        break;
    }
    case 2:
    {
        u32 done;
        if (tags.state.buttons == 0)
        {
            done = advance_runtime_dialogue_timer(state.duration, context.freeze, context.dt, context.scene_scale);
        }
        else
        {
            done = state.completion;
            if (state.button == 0)
            {
                step = 0.f;
                if (!(context.freeze > 0.f))
                {
                    step = context.dt * context.scene_scale;
                }
                state.elapsed += step;
                return;
            }
        }
        if (done != 0)
        {
            set_runtime_dialogue_mode(3, context);
        }
        step = 0.f;
        if (!(context.freeze > 0.f))
        {
            step = context.dt * context.scene_scale;
        }
        state.elapsed += step;
        return;
    }
    case 3:
    {
        const float fraction = 1.f - (state.duration - std::bit_cast<float>(0x3E99999Au))
            * std::bit_cast<float>(0xC0555555u);
        if (!(0.f > fraction))
        {
            state.progress = 1.f;
            if (!(fraction > 1.f))
            {
                state.progress = fraction;
            }
        }
        else
        {
            state.progress = 0.f;
        }
        if (advance_runtime_dialogue_timer(state.duration, context.freeze, context.dt, context.scene_scale) != 0)
        {
            if (tags.state.invoke_map != 0)
            {
                set_runtime_dialogue_mode(5, context);
                return;
            }
            advance_runtime_dialogue_page(context);
            return;
        }
        break;
    }
    case 4:
        if (1 < std::bit_cast<i32>(state.count))
        {
            state.entries[state.read].text[0] = 0;
            ++state.read;
            if (7 < std::bit_cast<i32>(state.read))
            {
                state.read = 0;
            }
            --state.count;
            reset_dialogue_page(context.page, state.entries[state.read].text.data(), state.entries[state.read].flag,
                context.wrap_width, context.settings, context.measurement);
            set_runtime_dialogue_mode(1, context);
            return;
        }
        set_runtime_dialogue_mode(0, context);
        return;
    case 5:
        if (state.map_retained != 0)
        {
            advance_runtime_dialogue_page(context);
            return;
        }
    }
}
}
