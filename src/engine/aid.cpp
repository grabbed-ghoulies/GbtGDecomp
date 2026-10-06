#include "gbtg/aid.hpp"

namespace gbtg
{

void AidStore::open(const std::filesystem::path& dir)
{
    game_dir = dir;
    common = load_bundle(bundle_path("common.bnl"));
}

std::filesystem::path AidStore::bundle_path(std::string_view relative) const
{
    return game_dir / "bundles" / std::filesystem::path(relative);
}

std::filesystem::path AidStore::demand_path(u32 hash) const
{
    return game_dir / "demand" / aid_hash_hex(hash);
}

bool AidStore::demand_exists(u32 hash) const
{
    return std::filesystem::exists(demand_path(hash));
}

// Host resource ownership for the demand files used by the book transition.
// LooseResource retains the backing allocation for the entry's spans.
const BundleEntry* AidStore::load_aid(std::string_view name)
{
    if (const BundleEntry* entry = find_entry(common, name))
    {
        return entry;
    }
    for (const auto& [key, bundle] : bundles)
    {
        if (const BundleEntry* entry = find_entry(bundle, name))
        {
            return entry;
        }
    }
    const std::string key(name);
    if (const auto found = demand_entries.find(key); found != demand_entries.end())
    {
        return &found->second;
    }
    const auto path = demand_path(aid_hash(name));
    if (!std::filesystem::exists(path))
    {
        return nullptr;
    }
    auto [resource, inserted] = demand_resources.emplace(key, parse_loose_resource(read_file(path)));
    const LooseResource& file = resource->second;
    BundleEntry entry;
    entry.name = key;
    entry.hash = aid_hash(name);
    entry.type = file.header.type;
    entry.header = file.header;
    entry.gpu_chunks = file.gpu_chunks;
    entry.cpu = file.cpu;
    entry.gpu = file.gpu;
    return &demand_entries.emplace(key, std::move(entry)).first->second;
}

const Bundle& AidStore::load_bundle_rel(std::string_view relative)
{
    if (relative == "common.bnl")
    {
        return common;
    }
    const std::string key(relative);
    auto it = bundles.find(key);
    if (it != bundles.end())
    {
        return it->second;
    }
    bundles[key] = load_bundle(bundle_path(relative));
    return bundles[key];
}

std::optional<std::string> aid_to_bundle_rel(std::string_view aid)
{
    constexpr std::string_view kPfx = "aid_";
    if (aid.size() < 6 || !aid.starts_with(kPfx))
    {
        return std::nullopt;
    }
    const auto rest = aid.substr(kPfx.size());
    const auto us = rest.find('_');
    if (us == std::string_view::npos || us == 0 || us + 1 >= rest.size())
    {
        return std::nullopt;
    }
    std::string rel;
    rel.reserve(4 + rest.size() + 4);
    rel += "aid_";
    rel += rest.substr(0, us);
    rel += '/';
    rel += rest.substr(us + 1);
    rel += ".bnl";
    return rel;
}

} // namespace gbtg
