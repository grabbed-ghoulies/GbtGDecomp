#include "gbtg/language.h"

#include <array>

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
