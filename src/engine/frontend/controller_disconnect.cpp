#include "controller_disconnect.h"

#include "dialogue_input.h"
#include "dialogue_text_pool.h"
#include "hud_layout.h"
#include "input_activation.h"
#include "../audio/bank_streams.h"
#include "../entities/ghouly_spawn_record.h"
#include "../scene/scene.h"
#include "../transitions/native_book.h"
#include "../video/native_movies.h"
#include "../../gbtg/text/localtext_resources.h"
#include "gbtg/anim.h"
#include "gbtg/log.h"
#include "gbtg/resource_error.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace gbtg
{
ControllerDisconnect g_controller_disconnect;

namespace
{
// A device on the input stack by the word its entry names it by: its native view, or an activation entry's own
// record (as SceneActorServices::start_pressed resolves them).
RuntimeDialogueInputDevice input_device_view(u32 address, TransitionBookServices& books)
{
    if (books.input_views != nullptr)
    {
        if (const auto view = books.input_views->find(address); view != books.input_views->end())
        {
            return view->second;
        }
    }
    if (books.input_addresses != nullptr)
    {
        if (const auto record = books.input_addresses->find(address); record != books.input_addresses->end())
        {
            return RuntimeDialogueInputDevice{record->second, {nullptr, nullptr, nullptr, nullptr}};
        }
    }
    throw Error("0x00033480 reads an input stack device with no native view");
}

TransitionBookServices& bound_inputs(ControllerDisconnectServices& services, const char* caller)
{
    TransitionBookServices& books = services.books;
    if (books.input == nullptr || books.input_release == nullptr || books.input_addresses == nullptr
        || books.input_views == nullptr || books.text_pools == nullptr)
    {
        throw Error(std::string(caller) + " without the scene's input stack and text pools bound");
    }
    return books;
}

std::span<u8> create_runtime_input_for_kind(i32 kind, TransitionBookServices& books);

// Xbox 0x00107900, EDI the output: kind 4, 0x18 bytes and active, holding four kind 5 devices (+8..+0x14) that each
// name it at their +0xB8.
std::span<u8> create_runtime_input_group(TransitionBookServices& books)
{
    std::span<u8> group{static_cast<u8*>(engine_heap_allocate(0x18)), 0x18};
    std::memset(group.data(), 0, 0x18);
    const u32 active = 1;
    const u32 kind = 4;
    std::memcpy(group.data() + 4, &active, 4);
    std::memcpy(group.data(), &kind, 4);
    const u32 group_address = static_cast<u32>(reinterpret_cast<std::uintptr_t>(group.data()));
    u32 slot = 8;
    i32 remaining = 4;
    do
    {
        const std::span<u8> child = create_runtime_input_for_kind(5, books); // 0x00108180
        const u32 child_address = static_cast<u32>(reinterpret_cast<std::uintptr_t>(child.data()));
        (*books.input_addresses)[child_address] = child;
        (*books.input_views)[child_address] = RuntimeDialogueInputDevice{child, {nullptr, nullptr, nullptr, nullptr}};
        std::memcpy(group.data() + slot, &child_address, 4);
        slot += 4;
        --remaining;
        std::memcpy(child.data() + 0xB8, &group_address, 4);
    } while (remaining != 0);
    return group;
}

// Xbox 0x00108180: the device 0x000337B0 makes for the top entry's kind; ESI the output (DAT_004DEA28).
std::span<u8> create_runtime_input_for_kind(i32 kind, TransitionBookServices& books)
{
    switch (kind)
    {
    case 1:
        return create_runtime_actor_base_input(*books.input_release); // 0x00107830
    case 2:
        return create_runtime_selected_input(*books.input_release); // 0x00107D60
    case 3:
        return create_runtime_keyboard_input(*books.input_release); // 0x00107EF0
    case 4:
        return create_runtime_input_group(books); // 0x00107900
    case 5:
        return create_runtime_actor_input(*books.input_release); // 0x00107AE0
    default:
        // DAT_004DEA28 stays 0, and 0x000337B0 pushes and resets a device at address zero.
        throw Error("0x00108180 with input kind " + std::to_string(kind) + " makes no device");
    }
}
}

void boot_controller_disconnect(const RuntimeHudLayout& hud)
{
    // fs_init 0x00033980, after its 0x00033030.
    auto& c = g_controller_disconnect;
    c.x = hud.center_x; // DAT_00509730
    c.text = nullptr;
    c.input = {};
    c.input_address = 0;
    c.silenced = 0;
    c.hold = 0;
    c.was_paused = 0;
    c.y = hud.center_y; // DAT_00509734
    c.line = 0xFFFFFFFFu;
    c.phase = 4;
}

// Xbox 0x001095F0
u32 top_input_entry_device(const RuntimeDialogueInputState& input)
{
    const GhoulySpawnPool& entries = *input.devices; // DAT_0050E0E8
    if ((entries.cursor - entries.begin) / entries.stride != 0)
    {
        return read_u32({entries.begin + (*input.selected - 1) * entries.stride, entries.stride}, 0); // DAT_0050E0EC
    }
    return 0;
}

// Xbox 0x00038A70
void hold_silenced_audio_banks(i32 hold, AudioBankStreams& banks)
{
    AudioTrackedBank* bank = nullptr;
    for (AudioTrackedBank& candidate : banks.banks) // DAT_004DF370 .. DAT_004DFFA0, stride 0x9C
    {
        if (candidate.id != 0)
        {
            bank = &candidate;
            break;
        }
    }
    while (bank != nullptr)
    {
        if (bank->id == 0x12)
        {
            if (hold == 0)
            {
                resume_tracked_audio_bank(*bank, banks); // 0x00038360
            }
            else
            {
                pause_tracked_audio_bank(*bank, banks); // 0x00038300
            }
        }
        bank = next_tracked_audio_bank(bank, banks); // 0x00038400
    }
}

// Xbox 0x00033720
void silence_for_controller_prompt(ControllerDisconnectServices& services)
{
    g_controller_disconnect.silenced = 1; // DAT_004DEA20
    acquire_tracked_audio_bank("XACT_SILENCE", 0x13, services.banks); // 0x00038910, EAX 0x00218168
    hold_silenced_audio_banks(1, services.banks); // 0x00038A70, EDI 1
    services.flags |= 0x80u;
}

// Xbox 0x00033750
void restore_after_controller_prompt(ControllerDisconnectServices& services)
{
    g_controller_disconnect.silenced = 0; // DAT_004DEA20
    // 0x00038460 on "XACT_SILENCE", then its reference dropped and, at none left, the bank resumed and detached:
    // 0x00038970's body, inlined.
    release_tracked_audio_bank("XACT_SILENCE", services.banks);
    hold_silenced_audio_banks(0, services.banks); // 0x00038A70, EDI 0
    services.flags &= 0xFFFFFF7Fu;
}

// Xbox 0x00033610
void write_controller_prompt_text(std::span<const u8> device, char16_t* output,
    ControllerDisconnectServices& services)
{
    TransitionBookServices& books = services.books;
    const std::span<const u8> resource = acquire_animation_resource(books.resources,
        "aid_loctext_ghoulies_pause_nocontroller"); // 0x00035E90, 0x002181E8; never released
    if (resource.data() == nullptr)
    {
        resource_error(books.error, 2); // 0x00032700
    }
    u32 mask;
    switch (read_u32(device, 0))
    {
    case 2:
        mask = services.input.selected_mask; // DAT_0050E0F8
        break;
    case 3:
        mask = 0xFFFFFFFFu;
        break;
    case 5:
        mask = runtime_input_port_mask(device, *books.input); // 0x00107C60, EAX the device
        break;
    default:
        mask = 0;
        break;
    }
    const char* key = nullptr;
    switch (mask)
    {
    case 0xFFFFFFFFu:
        key = "PortAny"; // 0x0031C34C
        break;
    case 1:
        key = "Port1"; // 0x0031C330
        break;
    case 2:
        key = "Port2"; // 0x0031C328
        break;
    case 4:
        key = "Port3"; // 0x0031C320
        break;
    case 8:
        key = "Port4"; // 0x0031C318
        break;
    default:
        return;
    }
    lookup_borrowed_localtext(resource, "noControllerText", key, output, books.localtext); // 0x000FF120, 0x0031C338
}

// Xbox 0x000337B0
void set_controller_disconnect_phase(i32 phase, ControllerDisconnectServices& services)
{
    auto& c = g_controller_disconnect;
    if (phase == c.phase)
    {
        return;
    }
    if (c.phase == 3)
    {
        TransitionBookServices& books = bound_inputs(services, "0x000337B0 leaving phase 3");
        if (c.was_paused == 0)
        {
            services.flags &= 0xFFFFDFFFu;
        }
        restore_after_controller_prompt(services); // 0x00033750
        release_dialogue_text_pool(c.text, *books.text_pools); // 0x0005F460, EBX DAT_004DEA2C
        c.text = nullptr;
        if (read_u32(c.input, 4) != 0)
        {
            pop_runtime_input_entry(*books.input); // 0x001093C0
            release_runtime_input_device(c.input, *books.input_release); // 0x00108230, EAX DAT_004DEA28
            c.input = {};
            c.input_address = 0;
        }
        if (c.movies_held != 0)
        {
            scene_disable_overlays(); // 0x00106EC0
            c.movies_held = 0;
        }
    }
    c.phase = phase;
    if (phase == 2)
    {
        TransitionBookServices& books = bound_inputs(services, "0x000337B0 entering phase 2");
        log::info("controller prompt opens (connected ports 0x%x)", runtime_input_device_mask(services.input, 0));
        silence_for_controller_prompt(services); // 0x00033720
        c.was_paused = i32((services.flags >> 13) & 1u);
        if (c.was_paused == 0)
        {
            services.flags |= 0x2000u;
        }
        allocate_dialogue_text_pool(c.text, 0, 1, 0, books.text_pools); // 0x0005F390, ESI DAT_004DEA2C
        c.text->wrap_width = 375.f; // +0x10, DAT_0032B5C0
        if (c.input.data() == nullptr)
        {
            const u32 top = top_input_entry_device(services.input); // 0x001095F0
            if (top == 0)
            {
                throw Error("0x000337B0 reads the kind of a top input entry with no device");
            }
            const i32 kind = i32(read_u32(input_device_view(top, books).record, 0));
            c.input = kind == 4
                ? create_runtime_keyboard_input(*books.input_release) // 0x00107EF0, ESI DAT_004DEA28
                : create_runtime_input_for_kind(kind, books); // 0x00108180
            c.input_address = static_cast<u32>(reinterpret_cast<std::uintptr_t>(c.input.data()));
            (*books.input_addresses)[c.input_address] = c.input;
            (*books.input_views)[c.input_address] = RuntimeDialogueInputDevice{c.input,
                {nullptr, nullptr, nullptr, nullptr}};
            push_runtime_input_entry(c.input_address, "PLEASE_INSERT_PAD", *books.input); // 0x00109340, 0x0031C304
            reset_runtime_input_device(c.input, *books.input_release); // 0x00108270
        }
        // The stack buffer (0x400 bytes) is uninitialised in retail and keeps whatever it held when no key matches.
        std::array<char16_t, 0x200> line{};
        write_controller_prompt_text(c.input, line.data(), services); // 0x00033610, ECX DAT_004DEA28
        c.line = create_dialogue_text_record(*c.text, line.data(), c.x, c.y - 50.f, 0.75f, 0.75f, 3.f, 1, 1, 1,
            books.metrics, books.viewport_width, books.next_text_id, books.random); // 0x0005F5C0, DAT_0032B1E0
        if (native_movie_runtime().mode != 3) // DAT_0050DD8C
        {
            scene_enable_overlays(); // 0x00106E90
            c.movies_held = 1;
        }
    }
    else if (phase == 3)
    {
        TransitionBookServices& books = services.books;
        end_dialogue_text_record(*c.text, c.line, books.fonts, books.fallback_texture, books.random); // 0x0005F7E0
    }
}

// Xbox 0x00033480
void tick_controller_disconnect(ControllerDisconnectServices& services)
{
    auto& c = g_controller_disconnect;
    switch (c.phase)
    {
    case 1:
    {
        if (c.requested != 0)
        {
            c.requested = 0;
            set_controller_disconnect_phase(2, services);
            return;
        }
        if (c.hold != 0)
        {
            return;
        }
        const GhoulySpawnPool& entries = *services.input.devices; // DAT_0050E0E8
        if ((entries.cursor - entries.begin) / entries.stride == 0)
        {
            return;
        }
        const u32 top = read_u32({entries.begin + (*services.input.selected - 1) * entries.stride, entries.stride},
            0); // DAT_0050E0EC
        if (top == 0)
        {
            return;
        }
        const RuntimeDialogueInputDevice device = input_device_view(top, services.books);
        if (read_u32(device.record, 4) == 0)
        {
            return;
        }
        if (runtime_input_device_presence(device, services.input) == 0) // 0x001082B0
        {
            set_controller_disconnect_phase(2, services);
        }
        return;
    }
    case 2:
    {
        TransitionBookServices& books = bound_inputs(services, "0x00033480 phase 2");
        refresh_runtime_input_device(c.input_address, books.frame(), *books.input, *books.input_release); // 0x00109470
        const RuntimeDialogueInputDevice device = input_device_view(c.input_address, books);
        if (runtime_input_button_edge(device, 0x10, services.input) != 0) // 0x001083F0, Start
        {
            set_controller_disconnect_phase(3, services);
        }
        return;
    }
    case 3:
        if (c.text->allocation->cursor <= c.text->allocation->begin) // *DAT_004DEA2C: +4 >= +8
        {
            set_controller_disconnect_phase(1, services);
        }
        return;
    case 4:
        if (runtime_input_device_mask(services.input, 0) != 0) // 0x001554C0(0)
        {
            c.phase = 1;
        }
        return;
    default:
        return;
    }
}
}
