#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace shader_warmup
{

// cache/shaders.gbtg: the vertex programs and pixel definitions a run compiled, so a later run can compile them
// before the first frame. The backends own the compiled shaders; this plugin owns the file.
struct ShaderPermutationManifest
{
    std::vector<std::vector<uint32_t>> vertex_programs;
    std::vector<std::array<uint32_t, 60>> pixel_definitions;
};

bool contains_program(const std::vector<std::vector<uint32_t>>& programs, const std::vector<uint32_t>& program);
// Adds the file's records to `manifest`, skipping duplicates. Throws on a missing or malformed file.
void read_shader_permutation_manifest(const std::filesystem::path& path, ShaderPermutationManifest& manifest);
void write_shader_permutation_manifest(const std::filesystem::path& path, const ShaderPermutationManifest& manifest);

} // namespace shader_warmup
