#include "gbtg/entity.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/gfx.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <malloc.h>
#include <span>
#include <vector>

namespace gbtg
{
namespace
{

// Names at DAT_004245A8. 0x00056DF0 widens one entry per slot into DAT_004e5f20.
constexpr i32 kTextTokenCount = 0x5C;
constexpr i32 kTextTokenAlloc = 0x86;
constexpr i32 kTextIconMin = 0x36;

const char* const kTextTokenNames[kTextTokenCount] = {
    "boy",           "girl",          "butler",        "cook",
    "groundskeeper", "housekeeper",   "krackpot",      "baroness",
    "baron",         "crone",         "piratecaptain", "skeletonbad",
    "mrribs",        "pirate",        "littleguy",     "zombiestandard",
    "hunchback",     "binklogo",      "SpeakerMAX",    "left",
    "right",         "centre",        "scalex",        "scaley",
    "seed",          "page",          "para",          "nopara",
    "linek",         "linel",         "linec",         "liner",
    "end",           "nohead",        "nobutton",      "settime",
    "scale",         "tab",           "pausestats",    "bronzereq",
    "silverreq",     "goldreq",       "platinumreq",   "bronzetime",
    "silvertime",    "goldtime",      "platinumtime",  "finalscore",
    "finaltime",     "hightime",      "highscore",     "hightrophy",
    "finaltrophy",   "invokemap",     "IconMIN",       "EventMIN",
    "cross",         "dontgethit",    "findkeyghouly", "findkey",
    "imp",           "killall",       "killany",       "minimumdamage",
    "mummy",         "nodamage",      "onetype",       "skeleton",
    "survive",       "timelimit",     "unarmedonly",   "weapononly",
    "limitedattacks", "EventMAX",     "PauseMIN",      "clock",
    "faints",        "prisoners",     "skull",         "bonusbooks",
    "PauseMAX",      "ChallengeMIN",  "bronzetrophy",  "silvertrophy",
    "goldtrophy",    "platinumtrophy", "ChallengeMAX", "WeaponMIN",
    "interacthand",  "WeaponMAX",     "IconMAX",       "sfx",
};

// DAT_004e5f20. Wide copies, stride 4, count 0x5C.
u16* g_text_tokens[kTextTokenCount]{};

i32 g_text_ticks = 0;      // DAT_004e61c8
i32 g_text_read_page = 0;  // DAT_004ee1e0. Indexes the flag byte at page+0x1000.
i32 g_text_write_page = 0; // DAT_004ee1e4. Indexes the 0x1002-byte pages at DAT_004e61d0.
i32 g_text_page_count = 0; // DAT_004ee1e8. Writers reject a count above 8.
i32 g_text_draw_mode = 0;  // DAT_004ee1ec. 1 and 3 select the sprite scale.
i32 g_markup_gate = 0;     // DAT_004ee290
i32 g_sprite_gate = 0;     // DAT_004ee294. Nonzero runs the sprite tick.
float g_text_scale = 0.f;  // DAT_004ee298. Pool float at DAT_0032B254.
i32 g_text_pad_29c = 0;    // DAT_004ee29c
i32 g_text_page_idle = 0;  // DAT_004ee2a0
void* g_text_link = nullptr; // DAT_004ee2a8
i32 g_text_mark_b0 = 0;    // DAT_004ee2b0
i32 g_text_mark_b4 = 0;    // DAT_004ee2b4

void* g_text_sprite = nullptr; // DAT_004e60a4
i32 g_text_sprite_id = 0;      // DAT_004e60a0
i32 g_text_sprite_hold = 0;    // DAT_004e60a8
i32 g_text_sprite_pair = 0;    // DAT_004e60ac
i32 g_text_sprite_phase = 0;   // DAT_004e60b8

void* g_text_icon = nullptr; // DAT_004e60c4. Glyph record; +8 and +0xA are ushorts.
i32 g_text_icon_id = 0;      // DAT_004e60c0. IconMIN until a later token replaces it.

// 0x00056DF0. Current heap slot (DAT_00507808) supplies every block. Each name
// is sign-extended into u16s and terminated. The pointer is stored even when
// the following copy would fault, so a null block is stored and left alone.
void text_build_tokens()
{
    for (i32 i = 0; i < kTextTokenCount; ++i)
    {
        u16* block = static_cast<u16*>(engine_heap_allocate(kTextTokenAlloc - 4));
        g_text_tokens[i] = block;
        if (block == nullptr)
        {
            continue;
        }
        const char* name = kTextTokenNames[i];
        i32 length = 0;
        while (name[length] != '\0')
        {
            length += 1;
        }
        for (i32 c = 0; c < length; ++c)
        {
            block[c] = static_cast<u16>(static_cast<i32>(static_cast<signed char>(name[c])));
        }
        block[length] = 0;
    }
}

// 0x00057400. A live sprite would be released by 0x00035C00 (EBX is the object);
// the resource list is not in this file, so the slot is cleared. The scene load
// at DAT_00506c74 and the fallback load of DAT_0050ea9c are discarded. The three
// ids become -1 and the phase becomes 0.
void text_reset_sprite()
{
    g_text_sprite_hold = -1;
    if (g_text_sprite != nullptr)
    {
        g_text_sprite = nullptr;
    }
    g_text_sprite_id = -1;
    g_text_sprite_pair = -1;
    g_text_sprite_phase = 0;
}

} // namespace

// Xbox 0x00057D40
void text_initialize_tokens()
{
    // Xbox 0x00057D40
    // ESI is zero and EAX is 1. The scale is the float at DAT_0032B254 (0.1).
    // 0x00056DF0 fills the token table, then 0x00057400 resets the sprite.
    // The icon pointer is cleared after those calls, and the icon id is IconMIN.
    g_text_ticks = 0;
    g_text_read_page = 0;
    g_text_write_page = 0;
    g_text_page_count = 0;
    g_text_draw_mode = 0;
    g_markup_gate = 1;
    g_sprite_gate = 1;
    g_text_scale = std::bit_cast<float>(0x3DCCCCCDu);
    g_text_pad_29c = 0;
    g_text_page_idle = 0;
    g_text_link = nullptr;
    g_text_mark_b0 = 0;
    g_text_mark_b4 = 0;
    text_build_tokens();
    text_reset_sprite();
    g_text_icon = nullptr;
    g_text_icon_id = kTextIconMin;
}


} // namespace gbtg
