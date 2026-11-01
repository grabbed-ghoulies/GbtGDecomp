#pragma once
// Inspector tools for authoring plugin loctext (docs/PLUGINS.md): plugin builds only.
#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "gbtg/types.h"

#include <filesystem>
#include <string>
#include <vector>

namespace gbtg
{
struct LoctextEntry
{
    std::string key; // e.g. "shaderwarmup__CompilingShaders"
    std::u16string text;
};

// A loose loctext resource file (type 16) holding `entries` in authoring order, in the layout the game's files use:
// no INFO name table, the LSBL string table sorted by string id (the key's runtime_localtext_hash, the next free id
// on a collision), then the key rows sorted by id with their authoring ordinals and the names in authoring order.
std::vector<u8> write_loctext(const std::vector<LoctextEntry>& entries);

// --write-loctext <source dir> --out <dir>: every <language>.txt in the source folder, each holding
// [aid_loctext_...] sections of key = value lines (UTF-8; \n and \\ escapes), becomes
// <dir>/loctext/<language>/<hash> per resource. Returns the process exit code.
int write_loctext_sources(const std::filesystem::path& source, const std::filesystem::path& out);

// --check-loctext <loctext dir>: decodes every resource under <dir>/<language>/ without an INFO name table (the
// layout write_loctext makes) and rebuilds its string and key tables, comparing the bytes. Returns the process
// exit code.
int check_loctext_writer(const std::filesystem::path& loctext);
}
#endif
