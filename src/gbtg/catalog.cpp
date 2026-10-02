#include "gbtg/catalog.hpp"
#include "gbtg/hash.hpp"
#include "gbtg/xbe.hpp"
#include "gbtg/resource.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace gbtg
{
namespace
{

std::string classify_magic(std::span<const u8> b, const std::string& rel)
{
    if (b.size() >= 4 && std::memcmp(b.data(), "XBEH", 4) == 0)
    {
        return "xbe";
    }
    if (b.size() >= 4 && std::memcmp(b.data(), "BIKi", 4) == 0)
    {
        return "bink";
    }
    if (b.size() >= 4 && std::memcmp(b.data(), "WBND", 4) == 0)
    {
        return "wbnd";
    }
    if (rel.size() >= 4 && rel.substr(rel.size() - 4) == ".bnl")
    {
        return "bnl";
    }
    if (b.size() >= 12 && read_u32(b, 8) == kResourceMagic)
    {
        return std::string("resource:") + resource_type_name(static_cast<ResourceType>(read_u32(b, 0)));
    }
    if (rel.find("Videos/") != std::string::npos || rel.find("Videos\\") != std::string::npos)
    {
        return "xmv";
    }
    return "other";
}

std::string to_slash(std::string s)
{
    for (char& c : s)
    {
        if (c == '\\')
        {
            c = '/';
        }
    }
    return s;
}

} // namespace

Catalog build_catalog(const std::filesystem::path& game_dir, const CatalogOptions& opt)
{
    Catalog cat;
    cat.game_dir = game_dir;
    if (!std::filesystem::exists(game_dir))
    {
        throw Error("game dir missing: " + game_dir.string());
    }

    std::unordered_map<u32, std::string> hash_to_name;
    const auto xbe_path = game_dir / "default.xbe";
    if (std::filesystem::exists(xbe_path))
    {
        try
        {
            auto xbe = parse_xbe(xbe_path);
            for (const auto& a : xbe.aid_strings)
            {
                hash_to_name[aid_hash(a)] = a;
            }
        }
        catch (...)
        {
        }
    }

    std::unordered_map<std::string, std::pair<u64, std::string>> manifest;
    if (!opt.manifest.empty() && std::filesystem::exists(opt.manifest))
    {
        std::ifstream in(opt.manifest);
        std::string line;
        while (std::getline(in, line))
        {
            if (line.empty() || line[0] == '#')
            {
                continue;
            }
            std::istringstream iss(line);
            std::string hash, path;
            u64 size = 0;
            if (iss >> hash >> size)
            {
                std::getline(iss, path);
                if (!path.empty() && path[0] == ' ')
                {
                    path.erase(0, 1);
                }
                if (!path.empty())
                {
                    manifest[to_slash(path)] = {size, hash};
                }
            }
        }
    }

    for (auto it = std::filesystem::recursive_directory_iterator(game_dir);
         it != std::filesystem::recursive_directory_iterator(); ++it)
    {
        if (!it->is_regular_file())
        {
            continue;
        }
        CatalogFile f;
        f.relative = to_slash(std::filesystem::relative(it->path(), game_dir).string());
        f.size = it->file_size();
        auto head = [&]()
        {
            std::ifstream in(it->path(), std::ios::binary);
            std::vector<u8> b(32);
            in.read(reinterpret_cast<char*>(b.data()), 32);
            b.resize(size_t(std::max<std::streamsize>(0, in.gcount())));
            return b;
        }();
        f.kind = classify_magic(head, f.relative);
        f.magic = head.size() >= 4 ? std::string(reinterpret_cast<char*>(head.data()), 4) : "";
        const auto stem = it->path().stem().string();
        u32 hv = 0;
        if (parse_hex8(stem, hv))
        {
            f.aid_hash = hv;
            auto found = hash_to_name.find(hv);
            if (found != hash_to_name.end())
            {
                f.aid_name = found->second;
            }
            else
            {
                cat.unresolved_hashes.push_back(stem);
            }
        }
        if (opt.compute_sha)
        {
            f.sha256 = sha256_file(it->path());
        }
        auto mit = manifest.find(f.relative);
        if (!manifest.empty())
        {
            if (mit == manifest.end())
            {
                ++cat.missing; // extra local file not in manifest: ignore
            }
            else if (mit->second.first == f.size && (!opt.compute_sha || mit->second.second == f.sha256))
            {
                ++cat.verified;
            }
            else
            {
                ++cat.mismatched;
            }
        }
        cat.files.push_back(std::move(f));
    }
    std::sort(cat.files.begin(), cat.files.end(),
              [](const CatalogFile& a, const CatalogFile& b) { return a.relative < b.relative; });
    return cat;
}

} // namespace gbtg
