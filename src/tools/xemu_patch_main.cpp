#include "gbtg/bundle.hpp"
#include "gbtg/xbe.hpp"

#include <filesystem>
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

void restore_then_backup(const std::filesystem::path& path)
{
    const auto original = std::filesystem::path(path.string() + ".original");
    if (std::filesystem::exists(original))
    {
        std::filesystem::copy_file(original, path, std::filesystem::copy_options::overwrite_existing);
        std::cout << "restored " << path.filename().string() << " from .original\n";
        return;
    }
    if (!std::filesystem::exists(path))
    {
        throw gbtg::Error("missing " + path.string());
    }
    std::filesystem::copy_file(path, original);
    std::cout << "wrote " << original.filename().string() << "\n";
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const auto game = arg(argc, argv, "--game-dir", "OriginalFiles");
        const auto common = game / "bundles" / "common.bnl";
        const auto xbe = game / "default.xbe";

        restore_then_backup(common);
        auto bnl = gbtg::load_bundle(common);
        gbtg::patch_aidlist_prefix(bnl, "aid_aidlist_ghoulies_sceneorder_game",
                                   "aid_script_ghoulies_chapter1_scene1_2playcam", 5);
        gbtg::save_bundle(bnl, common);
        std::cout << "patched sceneorder_game -> chapter1_scene1_2playcam\n";

        restore_then_backup(xbe);
        gbtg::apply_preview_freecam(xbe);
        std::cout << "patched default.xbe fly overlay (FPS freeze dt, body pose + follow eye)\n";
        std::cout << "listed in docs/ORIGINAL_FILES.md\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "error: " << ex.what() << "\n";
        std::cerr << "usage: gbtg-xemu-patch [--game-dir <dir>]\n";
        return 1;
    }
}
