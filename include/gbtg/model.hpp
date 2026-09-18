#pragma once

#include "gbtg/types.hpp"
#include "gbtg/texture.hpp"
#include "gbtg/bundle.hpp"

namespace gbtg {

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

struct Vec2 {
    float u = 0, v = 0;
};

struct Mesh {
    std::string name;
    std::vector<Vec3> positions;
    std::vector<Vec2> uvs;
    std::vector<u16> indices;  // triangle list
    std::vector<RgbaImage> textures;
};

Mesh parse_model(const BundleEntry& entry);
std::vector<Mesh> extract_models(const Bundle& bundle);

}  // namespace gbtg
