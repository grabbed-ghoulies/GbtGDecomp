#include "gbtg/catalog.h"
#include "gbtg/hash.h"
#include "gbtg/xbe.h"

#include <fstream>
#include <iostream>
#include <string>

namespace
{

std::filesystem::path find_arg(int argc, char** argv, const std::string& key, const std::string& def = {})
{
    for (int i = 1; i < argc - 1; ++i)
    {
        if (argv[i] == key)
        {
            return argv[i + 1];
        }
    }
    return def;
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const auto game = find_arg(argc, argv, "--game-dir", "OriginalFiles");
        auto manifest = find_arg(argc, argv, "--manifest");
        if (manifest.empty())
        {
            const auto cand = std::filesystem::path("manifests") / "retail_files.txt";
            if (std::filesystem::exists(cand))
            {
                manifest = cand;
            }
        }
        gbtg::CatalogOptions opt;
        opt.manifest = manifest;
        opt.compute_sha = true;
        for (int i = 1; i < argc; ++i)
        {
            if (std::string(argv[i]) == "--no-hash")
            {
                opt.compute_sha = false;
            }
        }

        const auto cat = gbtg::build_catalog(game, opt);
        std::cout << "game-dir: " << cat.game_dir.string() << "\n";
        std::cout << "files: " << cat.files.size() << "\n";
        if (!manifest.empty())
        {
            std::cout << "manifest verified: " << cat.verified << " mismatched: " << cat.mismatched << "\n";
        }

        int resolved = 0;
        for (const auto& f : cat.files)
        {
            if (!f.aid_name.empty())
            {
                ++resolved;
            }
        }
        std::cout << "hashed files with AID names: " << resolved << "\n";

        const auto xbe_path = cat.game_dir / "default.xbe";
        if (std::filesystem::exists(xbe_path))
        {
            const auto xbe = gbtg::parse_xbe(xbe_path);
            std::cout << "xbe title: " << xbe.title << " id=" << std::hex << xbe.title_id << std::dec << "\n";
            std::cout << "xbe debug: " << xbe.debug_path << "\n";
            std::cout << "xbe AID strings: " << xbe.aid_strings.size() << "\n";
            std::cout << "xbe sections: " << xbe.sections.size() << " libs: " << xbe.libraries.size() << "\n";
        }

        std::cout << "\nresolved AID files (first 40):\n";
        int shown = 0;
        for (const auto& f : cat.files)
        {
            if (f.aid_name.empty())
            {
                continue;
            }
            std::cout << "  " << gbtg::aid_hash_hex(f.aid_hash) << "  " << f.kind << "  " << f.relative << "  "
                      << f.aid_name << "\n";
            if (++shown >= 40)
            {
                break;
            }
        }

        const auto out = find_arg(argc, argv, "--out");
        if (!out.empty())
        {
            std::filesystem::create_directories(out);
            std::ofstream names(out / "aid_names.txt");
            names << "# hash name  (from default.xbe strings; no game bytes)\n";
            if (std::filesystem::exists(xbe_path))
            {
                const auto xbe = gbtg::parse_xbe(xbe_path);
                for (const auto& a : xbe.aid_strings)
                {
                    names << gbtg::aid_hash_hex(gbtg::aid_hash(a)) << "  " << a << "\n";
                }
            }
            std::ofstream resolved_map(out / "aid_resolved.txt");
            resolved_map << "# hash path name  (hashed disc files matched to XBE AID strings)\n";
            for (const auto& f : cat.files)
            {
                if (f.aid_name.empty())
                {
                    continue;
                }
                resolved_map << gbtg::aid_hash_hex(f.aid_hash) << "  " << f.relative << "  " << f.aid_name << "\n";
            }
            std::cout << "wrote name maps under " << out.string() << "\n";
        }
        return cat.mismatched ? 2 : 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "error: " << ex.what() << "\n";
        std::cerr << "usage: gbtg-catalog --game-dir <dir> [--manifest manifests/retail_files.txt] "
                     "[--out docs/generated] [--no-hash]\n";
        return 1;
    }
}
