#include "gbtg/bundle.h"
#include "gbtg/loctext.h"
#include "gbtg/model.h"
#include "gbtg/resource.h"
#include "gbtg/texture.h"
#include "gbtg/wavebank.h"

#include <iostream>
#include <string>

namespace
{

std::filesystem::path arg(int argc, char** argv, const std::string& key, const std::string& def = {})
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

std::string safe_filename(std::string s)
{
    for (char& c : s)
    {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
        {
            c = '_';
        }
    }
    return s;
}

void dump_bundle_textures(const gbtg::Bundle& b, const std::filesystem::path& out)
{
    int n = 0;
    for (const auto* e : gbtg::entries_of_type(b, gbtg::ResourceType::Texture))
    {
        gbtg::TextureHeader hdr;
        if (e->cpu.size() < 28 || !gbtg::parse_texture_header(e->cpu, hdr))
        {
            continue;
        }
        try
        {
            std::span<const gbtg::u8> gpu = e->gpu;
            if (!e->gpu_chunks.empty())
            {
                const auto& g = e->gpu_chunks[0];
                if (size_t(g.offset) + g.size <= e->gpu.size())
                {
                    gpu = e->gpu.subspan(g.offset, g.size);
                }
            }
            auto img = gbtg::decode_xbox_texture(hdr, gpu);
            gbtg::write_bmp(out / "textures" / (safe_filename(e->name) + ".bmp"), img);
            ++n;
        }
        catch (const std::exception& ex)
        {
            std::cerr << "  texture fail " << e->name << ": " << ex.what() << "\n";
        }
    }
    std::cerr << "  dumped " << n << " textures from " << b.path << "\n";
}

void dump_bundle_models(const gbtg::Bundle& b, const std::filesystem::path& out)
{
    try
    {
        auto meshes = gbtg::extract_models(b);
        for (const auto& m : meshes)
        {
            std::string obj;
            obj += "# " + m.name + "\n";
            for (const auto& p : m.positions)
            {
                obj += "v " + std::to_string(p.x) + " " + std::to_string(p.y) + " " + std::to_string(p.z) + "\n";
            }
            for (const auto& t : m.uvs)
            {
                obj += "vt " + std::to_string(t.u) + " " + std::to_string(t.v) + "\n";
            }
            for (size_t i = 0; i + 2 < m.indices.size(); i += 3)
            {
                const auto a = m.indices[i] + 1;
                const auto bidx = m.indices[i + 1] + 1;
                const auto c = m.indices[i + 2] + 1;
                obj += "f " + std::to_string(a) + " " + std::to_string(bidx) + " " + std::to_string(c) + "\n";
            }
            gbtg::write_file(out / "models" / (safe_filename(m.name) + ".obj"),
                             std::span<const gbtg::u8>(reinterpret_cast<const gbtg::u8*>(obj.data()), obj.size()));
            int ti = 0;
            for (const auto& tex : m.textures)
            {
                gbtg::write_bmp(out / "models" / (safe_filename(m.name) + "_tex" + std::to_string(ti++) + ".bmp"), tex);
            }
        }
        std::cerr << "  dumped " << meshes.size() << " models from " << b.path << "\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "  model dump failed for " << b.path << ": " << ex.what() << "\n";
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::cerr << "gbtg-dump starting\n";
    try
    {
        const auto game = arg(argc, argv, "--game-dir", "OriginalFiles");
        const auto out = arg(argc, argv, "--out", "dump");
        std::filesystem::create_directories(out);

        std::cerr << "loading common.bnl\n";
        auto common = gbtg::load_bundle(game / "bundles" / "common.bnl");
        std::cerr << "  entries " << common.entries.size() << "\n";
        dump_bundle_textures(common, out);
        dump_bundle_models(common, out);

        {
            std::string listing;
            listing += "# common.bnl entries\n";
            for (const auto& e : common.entries)
            {
                listing += std::to_string(static_cast<unsigned>(e.type)) + "\t" + e.name + "\n";
            }
            for (const auto* e : gbtg::entries_of_type(common, gbtg::ResourceType::AidList))
            {
                listing += "# aidlist " + e->name + "\n";
                for (const auto& n : gbtg::parse_aidlist(e->cpu))
                {
                    listing += "  " + n + "\n";
                }
            }
            gbtg::write_file(
                out / "common_entries.txt",
                std::span<const gbtg::u8>(reinterpret_cast<const gbtg::u8*>(listing.data()), listing.size()));
            std::cerr << "  wrote common_entries.txt\n";
        }

        const auto demand_dir = game / "demand";
        if (std::filesystem::exists(demand_dir))
        {
            int tex = 0, parsed = 0;
            std::string listing;
            for (auto& p : std::filesystem::directory_iterator(demand_dir))
            {
                if (!p.is_regular_file())
                {
                    continue;
                }
                try
                {
                    auto res = gbtg::parse_loose_resource(gbtg::read_file(p.path()));
                    ++parsed;
                    listing += p.path().filename().string() + " type=" + gbtg::resource_type_name(res.header.type) +
                               " magic_ok=" + (res.header.magic == gbtg::kResourceMagic ? "1" : "0") + "\n";
                    if (res.header.type == gbtg::ResourceType::Texture && tex < 8)
                    {
                        gbtg::TextureHeader hdr;
                        if (res.cpu.size() >= 28 && gbtg::parse_texture_header(res.cpu, hdr))
                        {
                            auto img = gbtg::decode_xbox_texture(hdr, res.gpu);
                            gbtg::write_bmp(out / "demand" / (p.path().filename().string() + ".bmp"), img);
                            ++tex;
                        }
                    }
                }
                catch (const std::exception& ex)
                {
                    std::cerr << "  demand " << p.path().filename().string() << ": " << ex.what() << "\n";
                }
            }
            gbtg::write_file(
                out / "demand_index.txt",
                std::span<const gbtg::u8>(reinterpret_cast<const gbtg::u8*>(listing.data()), listing.size()));
            std::cerr << "  demand parsed " << parsed << " textures dumped " << tex << "\n";
        }

        const auto font_dir = game / "font";
        if (std::filesystem::exists(font_dir))
        {
            int parsed = 0;
            std::string listing;
            for (auto& p : std::filesystem::directory_iterator(font_dir))
            {
                if (!p.is_regular_file())
                {
                    continue;
                }
                try
                {
                    auto res = gbtg::parse_loose_resource(gbtg::read_file(p.path()));
                    ++parsed;
                    listing += p.path().filename().string() + " type=" + gbtg::resource_type_name(res.header.type) +
                               " cpu=" + std::to_string(res.cpu.size()) + " gpu=" + std::to_string(res.gpu.size()) +
                               "\n";
                }
                catch (const std::exception& ex)
                {
                    std::cerr << "  font " << p.path().filename().string() << ": " << ex.what() << "\n";
                }
            }
            gbtg::write_file(
                out / "font_index.txt",
                std::span<const gbtg::u8>(reinterpret_cast<const gbtg::u8*>(listing.data()), listing.size()));
            std::cerr << "  font containers parsed " << parsed << "\n";
        }

        const auto loc_dir = game / "loctext" / "english";
        if (std::filesystem::exists(loc_dir))
        {
            std::string all;
            int files = 0, keys = 0;
            for (auto& p : std::filesystem::directory_iterator(loc_dir))
            {
                if (!p.is_regular_file())
                {
                    continue;
                }
                try
                {
                    auto bytes = gbtg::read_file(p.path());
                    auto table = gbtg::parse_loctext(bytes, p.path().filename().string());
                    ++files;
                    for (const auto& [k, v] : table.keys_to_utf8)
                    {
                        all += k + "\t" + v + "\n";
                        ++keys;
                    }
                }
                catch (const std::exception& ex)
                {
                    std::cerr << "  loctext " << p.path().filename().string() << ": " << ex.what() << "\n";
                }
            }
            gbtg::write_file(out / "loctext_english.tsv",
                             std::span<const gbtg::u8>(reinterpret_cast<const gbtg::u8*>(all.data()), all.size()));
            std::cerr << "  loctext files " << files << " keys " << keys << "\n";
        }

        const auto wbdir = game / "xwavebank";
        if (std::filesystem::exists(wbdir))
        {
            int listed = 0;
            std::string listing;
            std::filesystem::path smallest;
            uintmax_t smallest_sz = ~uintmax_t(0);
            for (auto& p : std::filesystem::directory_iterator(wbdir))
            {
                if (!p.is_regular_file())
                {
                    continue;
                }
                const auto sz = p.file_size();
                listing += p.path().filename().string() + " size=" + std::to_string(sz) + "\n";
                if (sz < smallest_sz)
                {
                    smallest_sz = sz;
                    smallest = p.path();
                }
            }
            if (!smallest.empty())
            {
                try
                {
                    auto bank = gbtg::parse_wavebank(smallest, 4);
                    listing += "  parsed " + smallest.filename().string() +
                               " entries=" + std::to_string(bank.entries.size()) + "\n";
                    listed += int(bank.entries.size());
                    int dumped = 0;
                    for (const auto& e : bank.entries)
                    {
                        listing += "    " + e.name + " codec=" + std::to_string(e.codec) +
                                   " hz=" + std::to_string(e.samples_per_sec) + "\n";
                        if (e.pcm.empty() || dumped >= 4)
                        {
                            continue;
                        }
                        gbtg::write_wav(out / "wav" / (safe_filename(e.name) + ".wav"), e);
                        ++dumped;
                    }
                    std::cerr << "  wavebank " << smallest.filename().string() << " cues " << bank.entries.size()
                              << " wavs " << dumped << "\n";
                }
                catch (const std::exception& ex)
                {
                    std::cerr << "  wavebank " << smallest.filename().string() << ": " << ex.what() << "\n";
                }
            }
            gbtg::write_file(
                out / "wavebanks.txt",
                std::span<const gbtg::u8>(reinterpret_cast<const gbtg::u8*>(listing.data()), listing.size()));
            std::cerr << "  wavebank cues listed " << listed << "\n";
        }

        std::cerr << "done -> " << out.string() << "\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "error: " << ex.what() << "\n";
        std::cerr << "usage: gbtg-dump --game-dir <dir> --out dump\n";
        return 1;
    }
}
