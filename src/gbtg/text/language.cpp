#include "gbtg/language.h"

#include <array>
#include <cstring>
#include <system_error>

namespace gbtg
{
namespace
{

// Folder names match event 0x1F's movie suffixes (0x0004CC60 switch).
constexpr std::array<const char*, 5> kLanguageFolders{"english", "french", "german", "spanish", "italian"};

bool equals_ignore_case(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i)
    {
        char x = a[i];
        char y = b[i];
        if (x >= 'A' && x <= 'Z')
        {
            x = static_cast<char>(x - 'A' + 'a');
        }
        if (y >= 'A' && y <= 'Z')
        {
            y = static_cast<char>(y - 'A' + 'a');
        }
        if (x != y)
        {
            return false;
        }
    }
    return true;
}

} // namespace

u32 g_language = 0;
char g_language_name[0x80] = "not set";
char g_loctext_folder[0x80] = "loctext\\";

u32 xbox_language_code(u32 language)
{
    // XC_LANGUAGE_ENGLISH 1, GERMAN 3, FRENCH 4, SPANISH 5, ITALIAN 6, by retail index.
    constexpr std::array<u32, 5> kCodes{1, 4, 3, 5, 6};
    return language < kCodes.size() ? kCodes[language] : kCodes[0];
}

namespace
{
void set_language_name(const char* name)
{
    std::strcpy(g_language_name, name);
}

// 0x0014B280 (GetFileAttributesA) and, on failure, GetLastError: the host's file system boundary.
u32 disc_file_attributes(const std::filesystem::path& path, u32& last_error)
{
    std::error_code ec;
    const auto status = std::filesystem::status(path, ec);
    if (ec || !std::filesystem::exists(status))
    {
        last_error = 3; // ERROR_PATH_NOT_FOUND
        return 0xFFFFFFFFu;
    }
    return std::filesystem::is_directory(status) ? 0x10u : 0x80u;
}
} // namespace

// Xbox 0x000FF1C0
void select_language(u32 xbox_language, const std::filesystem::path& disc_root)
{
    switch (xbox_language)
    {
    case 1:
        set_language_name("english");
        g_language = 0;
        break;
    case 2:
        set_language_name("japanese");
        g_language = 8;
        break;
    case 3:
        set_language_name("german");
        g_language = 2;
        break;
    case 4:
        set_language_name("french");
        g_language = 1;
        break;
    case 5:
        set_language_name("spanish");
        g_language = 3;
        break;
    case 6:
        set_language_name("italian");
        g_language = 4;
        break;
    case 7:
        set_language_name("korean");
        g_language = 6;
        break;
    case 8:
        set_language_name("chinese");
        g_language = 7;
        break;
    default:
        set_language_name("english");
        g_language = 0;
        break;
    }
    // sprintf("%sloctext\\%s", DAT_0054F5B0, the name).
    u32 last_error = 0;
    const u32 attributes = disc_file_attributes(disc_root / "loctext" / g_language_name, last_error);
    u32 result = 0;
    if (attributes == 0xFFFFFFFFu)
    {
        result = last_error;
    }
    else
    {
        result = (attributes & 0x10) != 0 ? 0x10Bu : 0u;
    }
    if (result != 0x10B)
    {
        set_language_name("english");
        g_language = 0;
    }
}

void restore_language_strings()
{
    std::strcpy(g_language_name, "not set");
    std::strcpy(g_loctext_folder, "loctext\\");
}

// Xbox 0x000FF040
void build_loctext_folder()
{
    std::strcat(g_loctext_folder, g_language_name);
    std::strcat(g_loctext_folder, "\\");
}

const char* language_folder(u32 language)
{
    return language < kLanguageFolders.size() ? kLanguageFolders[language] : kLanguageFolders[0];
}

std::optional<u32> parse_language_name(std::string_view name)
{
    for (u32 index = 0; index < kLanguageFolders.size(); ++index)
    {
        if (equals_ignore_case(name, kLanguageFolders[index]))
        {
            return index;
        }
    }
    return std::nullopt;
}

} // namespace gbtg
