#include "gbtg/model.hpp"

#include <cmath>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: gbtg-model-check GAME_DIRECTORY\n";
        return 2;
    }
    size_t bundles = 0, models = 0, failures = 0, empty = 0, fallback = 0;
    std::cout << "bundle\tmodel\tvertices\tindices\tsubmeshes\tfallback_submeshes\tissues...\n";
    for (const auto& file : std::filesystem::recursive_directory_iterator(argv[1]))
    {
        if (!file.is_regular_file() || file.path().extension() != ".bnl")
        {
            continue;
        }
        try
        {
            const auto bundle = gbtg::load_bundle(file.path());
            ++bundles;
            for (const auto& entry : bundle.entries)
            {
                if (entry.type != gbtg::ResourceType::Model)
                {
                    continue;
                }
                ++models;
                try
                {
                    const auto mesh = gbtg::parse_model(entry);
                    for (const auto& p : mesh.positions)
                    {
                        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
                        {
                            throw std::runtime_error("non-finite position");
                        }
                    }
                    for (const auto index : mesh.indices)
                    {
                        if (index >= mesh.positions.size())
                        {
                            throw std::runtime_error("index outside vertex buffer");
                        }
                    }
                    for (const auto& uv : mesh.uvs)
                    {
                        if (!std::isfinite(uv.u) || !std::isfinite(uv.v))
                        {
                            throw std::runtime_error("non-finite texture coordinate");
                        }
                    }
                    size_t unsupported = 0;
                    for (const auto& part : mesh.submeshes)
                    {
                        if (size_t(part.first_index) + part.index_count > mesh.indices.size())
                        {
                            throw std::runtime_error("submesh outside index buffer");
                        }
                        unsupported += !part.has_pixel_shader || !part.direct_uv;
                    }
                    if (mesh.indices.empty())
                    {
                        ++empty;
                    }
                    if (unsupported)
                    {
                        ++fallback;
                    }
                    std::cout << file.path().filename().string() << '\t' << entry.name
                              << '\t' << mesh.positions.size() << '\t' << mesh.indices.size()
                              << '\t' << mesh.submeshes.size() << '\t' << unsupported;
                    for (const auto& issue : mesh.issues)
                    {
                        std::cout << '\t' << issue;
                    }
                    std::cout << '\n';
                }
                catch (const std::exception& ex)
                {
                    ++failures;
                    std::cerr << file.path().filename().string() << '\t' << entry.name << '\t' << ex.what() << '\n';
                }
            }
        }
        catch (const std::exception& ex)
        {
            ++failures;
            std::cerr << file.path().string() << '\t' << ex.what() << '\n';
        }
    }
    std::cerr << "bundles=" << bundles << " models=" << models << " failures=" << failures
              << " empty=" << empty << " fallback=" << fallback << '\n';
    return failures ? 1 : 0;
}
