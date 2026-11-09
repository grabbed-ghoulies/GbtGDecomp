#pragma once

#include "gbtg/types.h"

#include <filesystem>
#include <optional>
#include <string_view>

namespace gbtg
{

// DAT_0042A710. Retail language index: 0 English, 1 French, 2 German,
// 3 Spanish, 4 Italian. The host sets it from gbtg.ini before Game::init.
extern u32 g_language;

// OriginalFiles/loctext folder for a language index; "english" when unknown.
const char* language_folder(u32 language);

// Case-insensitive language name ("French") to its retail index.
std::optional<u32> parse_language_name(std::string_view name);

extern char g_language_name[0x80]; // 0x0042A610, "not set" until 0x000FF1C0 names the language.
extern char g_loctext_folder[0x80]; // 0x0042A690, "loctext\"; 0x000FF040 appends the language's folder.

// The XGetLanguage boundary's answer (XC_LANGUAGE_*) for the host's language setting, a retail index.
u32 xbox_language_code(u32 language);
// fs_init: the language XGetLanguage reports names the loctext folder and sets the index (DAT_0042A710); English
// (index 0) when the disc has no loctext folder for it. `disc_root` is DAT_0054F5B0.
void select_language(u32 xbox_language, const std::filesystem::path& disc_root); // Xbox 0x000FF1C0
// Boot: the language's folder and a backslash are appended to "loctext\".
void build_loctext_folder(); // Xbox 0x000FF040
// Host re-entry (a second Game::init): the two strings above as the image holds them.
void restore_language_strings();

} // namespace gbtg
