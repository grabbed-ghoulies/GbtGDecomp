#include "dialogue_tags.h"
#include "../runtime/crt_runtime.h"
#include "../entities/entity_heap.h"

namespace gbtg
{
// Text from PTR_DAT_004245A8, widened by retail 0x00056DF0 into DAT_004E5F20.
// This immutable native view does not replace that function's heap ownership.
const DialogueTokenTable g_retail_dialogue_tokens{
    u"boy", u"girl", u"butler", u"cook", u"groundskeeper", u"housekeeper",
    u"krackpot", u"baroness", u"baron", u"crone", u"piratecaptain", u"skeletonbad",
    u"mrribs", u"pirate", u"littleguy", u"zombiestandard", u"hunchback", u"binklogo",
    u"SpeakerMAX", u"left", u"right", u"centre", u"scalex", u"scaley", u"seed",
    u"page", u"para", u"nopara", u"linek", u"linel", u"linec", u"liner", u"end",
    u"nohead", u"nobutton", u"settime", u"scale", u"tab", u"pausestats",
    u"bronzereq", u"silverreq", u"goldreq", u"platinumreq", u"bronzetime",
    u"silvertime", u"goldtime", u"platinumtime", u"finalscore", u"finaltime",
    u"hightime", u"highscore", u"hightrophy", u"finaltrophy", u"invokemap",
    u"IconMIN", u"EventMIN", u"cross", u"dontgethit", u"findkeyghouly", u"findkey",
    u"imp", u"killall", u"killany", u"minimumdamage", u"mummy", u"nodamage",
    u"onetype", u"skeleton", u"survive", u"timelimit", u"unarmedonly", u"weapononly",
    u"limitedattacks", u"EventMAX", u"PauseMIN", u"clock", u"faints", u"prisoners",
    u"skull", u"bonusbooks", u"PauseMAX", u"ChallengeMIN", u"bronzetrophy",
    u"silvertrophy", u"goldtrophy", u"platinumtrophy", u"ChallengeMAX", u"WeaponMIN",
    u"interacthand", u"WeaponMAX", u"IconMAX", u"sfx"};

const DialogueTokenNames g_retail_dialogue_token_names{
    "boy",
    "girl",
    "butler",
    "cook",
    "groundskeeper",
    "housekeeper",
    "krackpot",
    "baroness",
    "baron",
    "crone",
    "piratecaptain",
    "skeletonbad",
    "mrribs",
    "pirate",
    "littleguy",
    "zombiestandard",
    "hunchback",
    "binklogo",
    "SpeakerMAX",
    "left",
    "right",
    "centre",
    "scalex",
    "scaley",
    "seed",
    "page",
    "para",
    "nopara",
    "linek",
    "linel",
    "linec",
    "liner",
    "end",
    "nohead",
    "nobutton",
    "settime",
    "scale",
    "tab",
    "pausestats",
    "bronzereq",
    "silverreq",
    "goldreq",
    "platinumreq",
    "bronzetime",
    "silvertime",
    "goldtime",
    "platinumtime",
    "finalscore",
    "finaltime",
    "hightime",
    "highscore",
    "hightrophy",
    "finaltrophy",
    "invokemap",
    "IconMIN",
    "EventMIN",
    "cross",
    "dontgethit",
    "findkeyghouly",
    "findkey",
    "imp",
    "killall",
    "killany",
    "minimumdamage",
    "mummy",
    "nodamage",
    "onetype",
    "skeleton",
    "survive",
    "timelimit",
    "unarmedonly",
    "weapononly",
    "limitedattacks",
    "EventMAX",
    "PauseMIN",
    "clock",
    "faints",
    "prisoners",
    "skull",
    "bonusbooks",
    "PauseMAX",
    "ChallengeMIN",
    "bronzetrophy",
    "silvertrophy",
    "goldtrophy",
    "platinumtrophy",
    "ChallengeMAX",
    "WeaponMIN",
    "interacthand",
    "WeaponMAX",
    "IconMAX",
    "sfx"};

// Xbox 0x00056DF0
void initialize_dialogue_tokens(DialogueTokenTable& output, const DialogueTokenNames& names)
{
    i32 index = 0;
    do
    {
        // The dump requests 0x86 including its four-byte heap footer.
        auto* token = static_cast<char16_t*>(engine_heap_allocate(0x82));
        const char* source = names[index];
        output[index] = token;
        const char* cursor = source;
        do
        {
            ++cursor;
        }
        while (cursor[-1] != 0);
        const i32 length = static_cast<i32>(cursor - (source + 1));
        i32 character = 0;
        if (0 < length)
        {
            do
            {
                token[character] = static_cast<char16_t>(static_cast<i16>(static_cast<signed char>(source[character])));
                ++character;
            }
            while (character < length);
        }
        ++index;
        token[length] = 0;
    }
    while (index < 0x5C);
}

// Xbox 0x000FBDD0
i32 compare_dialogue_wide_prefix(const char16_t* first, const char16_t* second, u32 count)
{
    u32 index = 0;
    if (count != 0)
    {
        u16 character;
        do
        {
            character = *first;
            const u16 other = *second;
            ++first;
            ++second;
            if (character != other)
            {
                return static_cast<i32>(character) - static_cast<i32>(other);
            }
            if (character == 0)
            {
                break;
            }
            ++index;
        }
        while (index < count);
    }
    return 0;
}

// Xbox 0x0014AA3D
u32 xapi_wide_text_length(const char16_t* text)
{
    if (text == nullptr)
    {
        return 0;
    }
    return scene_detail::crt_wide_string_length(reinterpret_cast<const u16*>(text));
}

// Xbox 0x00056F10
i32 find_dialogue_token(const char16_t* text, const DialogueTokenTable& tokens)
{
    i32 index = 0;
    do
    {
        const char16_t* token = tokens[index];
        const u32 count = xapi_wide_text_length(token);
        u32 matched = 0;
        const char16_t* cursor = text;
        if (count == 0)
        {
            return index;
        }
        char16_t first;
        char16_t second;
        while (true)
        {
            first = *cursor;
            second = *token;
            ++cursor;
            ++token;
            if (first != second)
            {
                break;
            }
            if (first == 0)
            {
                return index;
            }
            ++matched;
            if (count <= matched)
            {
                return index;
            }
        }
        if (first == second)
        {
            return index;
        }
        ++index;
        if (0x5B < index)
        {
            return 0x5C;
        }
    }
    while (true);
}

// Xbox 0x00057260
i32 find_next_dialogue_speaker(const char16_t* text, i32 current_speaker,
    const DialogueTokenTable& tokens)
{
    bool found_page = false;
    i32 index = 0;
    const char16_t* cursor = text;
    char16_t value = *text;
    do
    {
        if (value == 0)
        {
            return -1;
        }
        value = *cursor;
        while (value != u'{')
        {
            value = text[index + 1];
            ++index;
            if (value == 0)
            {
                return current_speaker;
            }
        }
        const i32 token = find_dialogue_token(text + index + 1, tokens);
        if (token == 0x19)
        {
            if (found_page)
            {
                return current_speaker;
            }
            found_page = true;
        }
        else
        {
            if (token == 0x20)
            {
                if (!found_page)
                {
                    return -1;
                }
                return current_speaker;
            }
            if (token < 0x12 && found_page)
            {
                return token;
            }
        }
        ++index;
        value = text[index];
        cursor = text + index;
    }
    while (true);
}

// Xbox 0x00058B50
void set_dialogue_button_enabled(i32 requested, i32 button_allowed, i32& enabled)
{
    if (requested == 0)
    {
        enabled = 0;
    }
    else
    {
        enabled = 1;
        if (button_allowed == 0)
        {
            enabled = 0;
        }
    }
}
}
