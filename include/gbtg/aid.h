#pragma once

#include "gbtg/bundle.h"
#include "gbtg/hash.h"

#include <map>
#include <optional>
#include <string>

namespace gbtg
{

// Xbox: snprintf("%s%s%s", root, "bundles\\", name) at 0x00033E00 / 0x00033FB0 / 0x00034290
// Xbox: sprintf("%s%s%08x", root, "demand\\", hash) at 0x00034FAA
struct AidStore
{
    std::filesystem::path game_dir;
    Bundle common;
    std::map<std::string, Bundle> bundles; // key = relative path under bundles/
    std::map<std::string, LooseResource> demand_resources;
    std::map<std::string, BundleEntry> demand_entries;

    void open(const std::filesystem::path& dir);
    const Bundle& load_bundle_rel(std::string_view relative);
    std::filesystem::path bundle_path(std::string_view relative) const;
    std::filesystem::path demand_path(u32 hash) const;
    bool demand_exists(u32 hash) const;
    const BundleEntry* load_aid(std::string_view name);
};

// Xbox: bundles\<type>\<rest>.bnl  from AID `aid_<type>_<rest>`.
std::optional<std::string> aid_to_bundle_rel(std::string_view aid);

} // namespace gbtg
