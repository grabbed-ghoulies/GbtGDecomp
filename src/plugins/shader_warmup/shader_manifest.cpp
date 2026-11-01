#include "shader_manifest.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace shader_warmup
{
namespace
{
constexpr uint32_t kShaderPermutationFileVersion = 1;
constexpr uint32_t kShaderTranslatorVersion = 1; // Bump when shader generation or permutation normalization changes.
} // namespace

bool contains_program(const std::vector<std::vector<uint32_t>>& programs, const std::vector<uint32_t>& program)
{
    return std::find(programs.begin(), programs.end(), program) != programs.end();
}

void read_shader_permutation_manifest(const std::filesystem::path& path, ShaderPermutationManifest& manifest)
{
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("cannot open shader permutation manifest: " + path.string());
    }
    std::string line;
    std::string magic;
    uint32_t version = 0;
    if (!std::getline(input, line))
    {
        throw std::runtime_error("unsupported or invalid shader permutation manifest header: " + path.string());
    }
    std::istringstream header(line);
    std::string extra;
    if (!(header >> magic >> version) || magic != "GBTG_SHADER_PERMUTATIONS"
        || version != kShaderPermutationFileVersion || (header >> extra))
    {
        throw std::runtime_error("unsupported or invalid shader permutation manifest header: " + path.string());
    }
    if (!std::getline(input, line))
    {
        throw std::runtime_error("shader permutation manifest is missing its translator version: " + path.string());
    }
    std::istringstream translator(line);
    std::string label;
    if (!(translator >> label >> version) || label != "translator" || version != kShaderTranslatorVersion
        || (translator >> extra))
    {
        throw std::runtime_error("shader permutation manifest translator version mismatch: " + path.string());
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
            std::array<uint32_t, 60> definition{};
            for (uint32_t& word : definition)
            {
                if (!(record >> std::hex >> word))
                {
                    throw std::runtime_error("invalid pixel permutation at " + path.string() + ":" + std::to_string(line_number));
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
                throw std::runtime_error("invalid vertex permutation length at " + path.string() + ":" + std::to_string(line_number));
            }
            std::vector<uint32_t> program(count);
            for (uint32_t& word : program)
            {
                if (!(record >> std::hex >> word))
                {
                    throw std::runtime_error("invalid vertex permutation at " + path.string() + ":" + std::to_string(line_number));
                }
            }
            if (!contains_program(manifest.vertex_programs, program))
            {
                manifest.vertex_programs.push_back(std::move(program));
            }
        }
        else
        {
            throw std::runtime_error("unknown shader permutation record at " + path.string() + ":" + std::to_string(line_number));
        }
        std::string extra;
        if (record >> extra)
        {
            throw std::runtime_error("extra shader permutation data at " + path.string() + ":" + std::to_string(line_number));
        }
    }
    if (!input.eof())
    {
        throw std::runtime_error("failed reading shader permutation manifest: " + path.string());
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
        throw std::runtime_error("cannot write shader permutation manifest: " + path.string());
    }
    output << "GBTG_SHADER_PERMUTATIONS " << kShaderPermutationFileVersion << "\n";
    output << "translator " << kShaderTranslatorVersion << "\n";
    for (const auto& definition : manifest.pixel_definitions)
    {
        output << "pixel";
        for (uint32_t word : definition)
        {
            output << ' ' << std::hex << word;
        }
        output << std::dec << '\n';
    }
    for (const auto& program : manifest.vertex_programs)
    {
        output << "vertex " << std::dec << program.size();
        for (uint32_t word : program)
        {
            output << ' ' << std::hex << word;
        }
        output << std::dec << '\n';
    }
    if (!output)
    {
        throw std::runtime_error("failed writing shader permutation manifest: " + path.string());
    }
}

} // namespace shader_warmup
