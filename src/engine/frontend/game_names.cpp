#include "game_names.h"
#include "gameselect_rows.h"
#include "../saves/save_games.h"
#include "gbtg/entity.h"

#include <array>
#include <cwctype>
#include <string_view>

namespace gbtg
{
// Xbox 0x000FBE50
bool game_name_is_blank(const char16_t* name)
{
    const i32 length = i32(std::u16string_view(name).size()); // lstrlenW
    if (length == 0)
    {
        return true;
    }
    for (i32 index = 0; index < length; ++index)
    {
        if (std::iswctype(wint_t(name[index]), std::wctype("space")) == 0) // iswctype(.., _SPACE) at 0x00166BF9
        {
            return false;
        }
    }
    return true;
}

// Xbox 0x00141190
const char16_t* empty_slot_game_name(GameSelectControlServices& services)
{
    const char16_t* text = services.lookup_text("aid_loctext_ghoulies_savedgame", "supportsavedgame",
        "emptyslotname"); // 0x000FF0A0 into DAT_00507E78
    return text == nullptr ? u"ERROR" : text;
}

// Xbox 0x001411C0
i32 check_new_game_name(const char16_t* name, GameSelectControlServices& services)
{
    if (name == nullptr)
    {
        return 1;
    }
    if (name[0] == 0)
    {
        return 3;
    }
    if (game_name_is_blank(name)) // 0x000FBE50
    {
        return 4;
    }
    if (save_game_name_exists(name) || save_slot_loaded(name)) // 0x0010F530, 0x0010F620
    {
        return 2;
    }
    const std::u16string_view full(name);
    if (full == empty_slot_game_name(services)) // 0x00141190, lstrcmpW
    {
        return 5;
    }
    const std::u16string_view empty(empty_slot_game_name(services));
    const size_t found = full.find(empty); // wcsstr
    if (found == std::u16string_view::npos)
    {
        return 0;
    }
    // The name without the empty-slot name in it.
    std::array<char16_t, 0x80> rest{};
    const i32 length = i32(full.size());
    size_t written = 0;
    for (size_t index = 0; index < found; ++index)
    {
        rest[written++] = name[index];
    }
    i32 at = i32(found + empty.size());
    while (at <= length)
    {
        rest[written++] = name[at];
        ++at;
    }
    if (game_name_is_blank(rest.data())) // 0x000FBE50, EBX the copy
    {
        return 5;
    }
    return 0;
}
}
