#pragma once

#include "gbtg/types.h"

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

} // namespace gbtg
