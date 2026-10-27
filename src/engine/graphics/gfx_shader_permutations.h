#pragma once

#include "gbtg/types.h"

#include <array>
#include <filesystem>
#include <vector>

namespace gbtg
{

// cache/shaders.gbtg: the vertex programs and pixel definitions a run compiled, so a later run can compile them
// before the first frame. Backends own the compiled shaders; this is the file format they share.
struct ShaderPermutationManifest
{
    std::vector<std::vector<u32>> vertex_programs;
    std::vector<std::array<u32, 60>> pixel_definitions;
};

bool contains_program(const std::vector<std::vector<u32>>& programs, const std::vector<u32>& program);
// Adds the file's records to `manifest`, skipping duplicates. Throws on a missing or malformed file.
void read_shader_permutation_manifest(const std::filesystem::path& path, ShaderPermutationManifest& manifest);
void write_shader_permutation_manifest(const std::filesystem::path& path, const ShaderPermutationManifest& manifest);

} // namespace gbtg
