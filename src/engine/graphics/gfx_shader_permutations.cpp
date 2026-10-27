#include "gfx_shader_permutations.h"

#include "gbtg/types.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

namespace gbtg
{
namespace
{
constexpr u32 kShaderPermutationFileVersion = 1;
constexpr u32 kShaderTranslatorVersion = 1; // Bump when shader generation or permutation normalization changes.
} // namespace

bool contains_program(const std::vector<std::vector<u32>>& programs, const std::vector<u32>& program)
{
    return std::find(programs.begin(), programs.end(), program) != programs.end();
}

void read_shader_permutation_manifest(const std::filesystem::path& path, ShaderPermutationManifest& manifest)
{
    std::ifstream input(path);
    if (!input)
    {
        throw Error("cannot open shader permutation manifest: " + path.string());
    }
    std::string line;
    std::string magic;
    u32 version = 0;
    if (!std::getline(input, line))
    {
        throw Error("unsupported or invalid shader permutation manifest header: " + path.string());
    }
    std::istringstream header(line);
    std::string extra;
    if (!(header >> magic >> version) || magic != "GBTG_SHADER_PERMUTATIONS"
        || version != kShaderPermutationFileVersion || (header >> extra))
    {
        throw Error("unsupported or invalid shader permutation manifest header: " + path.string());
    }
    if (!std::getline(input, line))
    {
        throw Error("shader permutation manifest is missing its translator version: " + path.string());
    }
    std::istringstream translator(line);
    std::string label;
    if (!(translator >> label >> version) || label != "translator" || version != kShaderTranslatorVersion
        || (translator >> extra))
    {
        throw Error("shader permutation manifest translator version mismatch: " + path.string());
    }
    size_t line_number = 2;
    while (std::getline(input, line))
    {
        ++line_number;
        if (line.empty())
        {
            continue;
        }
        std::istringstream record(line);
        std::string kind;
        size_t count = 0;
        if (!(record >> kind))
        {
            continue;
        }
        if (kind == "pixel")
        {
            std::array<u32, 60> definition{};
            for (u32& word : definition)
            {
                if (!(record >> std::hex >> word))
                {
                    throw Error("invalid pixel permutation at " + path.string() + ":" + std::to_string(line_number));
                }
            }
            if (std::find(manifest.pixel_definitions.begin(), manifest.pixel_definitions.end(), definition)
                == manifest.pixel_definitions.end())
            {
                manifest.pixel_definitions.push_back(definition);
            }
        }
        else if (kind == "vertex")
        {
            if (!(record >> std::dec >> count) || count == 0 || count > 65536)
            {
                throw Error("invalid vertex permutation length at " + path.string() + ":" + std::to_string(line_number));
            }
            std::vector<u32> program(count);
            for (u32& word : program)
            {
                if (!(record >> std::hex >> word))
                {
                    throw Error("invalid vertex permutation at " + path.string() + ":" + std::to_string(line_number));
                }
            }
            if (!contains_program(manifest.vertex_programs, program))
            {
                manifest.vertex_programs.push_back(std::move(program));
            }
        }
        else
        {
            throw Error("unknown shader permutation record at " + path.string() + ":" + std::to_string(line_number));
        }
        std::string extra;
        if (record >> extra)
        {
            throw Error("extra shader permutation data at " + path.string() + ":" + std::to_string(line_number));
        }
    }
    if (!input.eof())
    {
        throw Error("failed reading shader permutation manifest: " + path.string());
    }
}

void write_shader_permutation_manifest(const std::filesystem::path& path, const ShaderPermutationManifest& manifest)
{
    if (!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream output(path, std::ios::trunc);
    if (!output)
    {
        throw Error("cannot write shader permutation manifest: " + path.string());
    }
    output << "GBTG_SHADER_PERMUTATIONS " << kShaderPermutationFileVersion << "\n";
    output << "translator " << kShaderTranslatorVersion << "\n";
    for (const auto& definition : manifest.pixel_definitions)
    {
        output << "pixel";
        for (u32 word : definition)
        {
            output << ' ' << std::hex << word;
        }
        output << std::dec << '\n';
    }
    for (const auto& program : manifest.vertex_programs)
    {
        output << "vertex " << std::dec << program.size();
        for (u32 word : program)
        {
            output << ' ' << std::hex << word;
        }
        output << std::dec << '\n';
    }
    if (!output)
    {
        throw Error("failed writing shader permutation manifest: " + path.string());
    }
}

} // namespace gbtg
