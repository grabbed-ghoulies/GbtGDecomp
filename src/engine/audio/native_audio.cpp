#include "native_audio.h"
#include "gbtg/anim.h"

#include <cstdio>
#include <cstring>

namespace gbtg
{
NativeAudioBanks* g_native_audio = nullptr;

// Xbox 0x000FC120
char* read_audio_list_line(char*& cursor, const char* base, i32 size)
{
    while (true)
    {
        char* line;
        char* after;
        do
        {
            line = cursor;
            if (size <= i32(line - base))
            {
                return nullptr;
            }
            char value = *line;
            while (value != '\0' && *cursor != '\n')
            {
                if (size <= i32(cursor - base))
                {
                    return nullptr;
                }
                ++cursor;
                value = *cursor;
            }
            if (*cursor == '\n')
            {
                *cursor = '\0';
                *(cursor - 1) = '\0';
            }
            after = cursor + 1;
            cursor = after;
        }
        while (*line == '#');
        char* text = line;
        if (text < after)
        {
            do
            {
                if (' ' < *text)
                {
                    break;
                }
                ++text;
            }
            while (text < cursor);
        }
        if (text != after)
        {
            return line;
        }
    }
}

// Xbox 0x000390A0
AudioBankNames* add_audio_bank_name(const char* name, NativeAudioBanks& banks)
{
    // The record comes from the DAT_004E0CD0 arena; the host keeps it in a stable deque.
    auto& record = banks.bank_names.emplace_back();
    std::strncpy(record.name.data(), name, record.name.size() - 1);
    record.next = nullptr;
    record.cues = nullptr;
    if (g_audio_bank_names != nullptr)
    {
        AudioBankNames* tail = g_audio_bank_names;
        while (tail->next != nullptr)
        {
            tail = tail->next;
        }
        tail->next = &record;
        return &record;
    }
    g_audio_bank_names = &record;
    return &record;
}

// Xbox 0x00039170
void add_audio_cue_name(AudioBankNames& bank, const char* name, NativeAudioBanks& banks)
{
    auto& record = banks.cue_names.emplace_back();
    std::strncpy(record.name.data(), name, record.name.size() - 1);
    record.next = nullptr;
    AudioCueName* tail = bank.cues;
    if (tail == nullptr)
    {
        bank.cues = &record;
        return;
    }
    while (tail->next != nullptr)
    {
        tail = tail->next;
    }
    tail->next = &record;
}

// Xbox 0x000392D0
void register_audio_bank_names(const char* list, NativeAudioBanks& banks, NativeAudioBootBoundaries& boundaries)
{
    std::span<const u8> resource;
    if (std::memcmp(list, "aid_", 4) == 0)
    {
        resource = boundaries.resource(list);
        if (resource.data() == nullptr)
        {
            boundaries.resource_error(2);
        }
    }
    // The resource is read-only on the host; the lines are terminated in a copy the registry keeps.
    banks.list.assign(resource.begin(), resource.end());
    banks.list.push_back('\0');
    char* base = banks.list.data();
    const i32 size = i32(resource.size()); // 0x00035D90
    char* cursor = base;
    char* line = read_audio_list_line(cursor, base, size);
    banks.current_name = nullptr;
    while (line != nullptr)
    {
        char* tab = line;
        while (*tab != '\t')
        {
            ++tab;
        }
        *tab = '\0';
        if (banks.current_name == nullptr || compare_animation_resource_type(banks.current_name, line) != 0)
        {
            banks.current = add_audio_bank_name(line, banks);
            banks.current_name = line;
        }
        add_audio_cue_name(*banks.current, tab + 1, banks);
        line = read_audio_list_line(cursor, base, size);
    }
    // 0x00035C00 / 0x00035C60 release the resource; the arena shrink is host heap management.
}

// Xbox 0x0015CB10
void create_audio_sound_bank(std::span<const u8> image, const char* name, AudioCoreBank*& output,
    NativeAudioBanks& banks, NativeAudioBootBoundaries& boundaries)
{
    auto* bank = static_cast<AudioCoreBank*>(allocate_audio_core_object(0xA4, sizeof(AudioCoreBank), name, 1,
        "audioSoundBankPreLoadedCreate", 99, 0, banks.memory));
    if (image.data() == nullptr || image.empty())
    {
        // 0x0015BB30 loads the bank from its file.
        throw Error("sound bank file load 0x0015BB30 is not translated");
    }
    bank->image = image.data();
    bank->preloaded = 1;
    append_audio_core_link(banks.loaded, *bank);
    boundaries.create_sound_bank(image, bank->runtime.handle);
    for (u32 category = 0; category < 4; ++category)
    {
        // 0x0015D0E0
        boundaries.register_notification(bank->runtime.handle, category);
        ++banks.notifications;
    }
    bank->references = 1;
    ++banks.loaded_count;
    bank->cues = AudioCoreLink{};
    output = bank;
}

// Xbox 0x00039F50
void load_audio_sound_banks(const char* prefix, NativeAudioBanks& banks, NativeAudioBootBoundaries& boundaries)
{
    const auto lower = [](char value)
    {
        return '@' < value && value < '[' ? char(value + ' ') : value;
    };
    const AudioBankNames* record = nullptr;
    while (true)
    {
        do
        {
            record = record == nullptr ? g_audio_bank_names : record->next;
            if (record == nullptr)
            {
                return;
            }
        }
        while (lower(record->name[0]) != lower(*prefix));
        char name[128];
        std::snprintf(name, sizeof(name), "aid_xsoundbank_%s_%s", prefix, record->name.data() + 1);
        const auto resource = boundaries.resource(name);
        if (resource.data() == nullptr)
        {
            boundaries.resource_error(2);
            ++banks.bank_count;
            continue;
        }
        AudioCoreBank*& slot = banks.banks.at(size_t(banks.bank_count));
        create_audio_sound_bank(resource, record->name.data(), slot, banks, boundaries);
        banks.handles[slot->runtime.handle] = record;
        ++banks.bank_count;
    }
}

// Xbox 0x0003A280
void initialize_native_audio(NativeAudioBanks& banks, NativeAudioBootBoundaries& boundaries)
{
    g_native_audio = &banks;
    // 0x0015B730 installs the heap callbacks (banks.memory); 0x0015B750's XACT file callbacks are the host's.
    boundaries.create_engine(); // 0x0015B960
    // 0x0015B960 beside its engine boundary: deferred settings (DAT_00336140 is 1) and the unit scale DAT_0032B0F8.
    banks.listener.apply = 1;
    banks.listener.unit_scale = 1.f;
    // 0x00039EF0
    const auto effects = boundaries.resource("aid_xdsp_ghoulies_test");
    if (effects.data() == nullptr)
    {
        boundaries.resource_error(2);
    }
    boundaries.download_effects(effects);
    register_audio_bank_names("aid_xcuelist_ghoulies_default", banks, boundaries);
    load_audio_sound_banks("ghoulies", banks, boundaries);
    boundaries.register_wavebanks(); // 0x00039DD0
}

std::string native_audio_cue_name(const NativeAudioBanks& banks, void* handle, u32 index)
{
    const auto found = banks.handles.find(handle);
    if (found == banks.handles.end())
    {
        throw Error("audio cue names for an unknown sound bank handle");
    }
    const AudioCueName* cue = found->second->cues;
    for (u32 cursor = 0; cue != nullptr && cursor < index; ++cursor)
    {
        cue = cue->next;
    }
    if (cue == nullptr)
    {
        throw Error("audio cue index lies past its bank's names");
    }
    std::string name = std::string("XACT_SOUNDBANK_") + found->second->name.data() + "_" + cue->name.data();
    for (char& value : name)
    {
        if (value >= 'a' && value <= 'z')
        {
            value = char(value - 32);
        }
    }
    return name;
}
}
