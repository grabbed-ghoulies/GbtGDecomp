#pragma once
#include "gbtg/types.h"
#include <array>
#include <span>
#include <string>
#include <vector>
namespace gbtg
{
struct XboxVertexAddressValidation
{
    bool relative = false;
    u32 source = 0, reg = 0, component = 0;
    bool negate = false;
    i32 minimum = 192, maximum = -1;
};
std::vector<XboxVertexAddressValidation> xbox_vertex_address_validation(std::span<const u32> program);
void validate_xbox_vertex_address(std::span<const XboxVertexAddressValidation> validations,
    const std::array<std::array<float, 4>, 16>& inputs,
    const std::array<std::array<float, 4>, 192>& constants);
std::string xbox_vertex_hlsl(std::span<const u32> program);
}
