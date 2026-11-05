#pragma once
#include "gbtg/entity.h"

#include <array>

namespace gbtg
{
// The colour channels (0x3C bytes: an effect particle's +0x60, a debris record's +0x24, a trail record's +0x10):
// red, green, blue, alpha and a fifth, each approaching its target at its rate and kept in 0..255 (0x000F1F60).
// A zero rate leaves the channel alone. The functions loop over the arrays, and most other code wants one channel.
struct EffectColourChannels
{
    std::array<float, 5> values{}; // +0
    std::array<float, 5> targets{}; // +0x14
    std::array<float, 5> rates{}; // +0x28

    float& red() { return values[0]; }
    float& green() { return values[1]; }
    float& blue() { return values[2]; }
    float& alpha() { return values[3]; }
    float& unk_10() { return values[4]; }
    float red() const { return values[0]; }
    float green() const { return values[1]; }
    float blue() const { return values[2]; }
    float alpha() const { return values[3]; }
    float unk_10() const { return values[4]; }
    float& target_red() { return targets[0]; }
    float& target_green() { return targets[1]; }
    float& target_blue() { return targets[2]; }
    float& target_alpha() { return targets[3]; }
    float& target_unk_10() { return targets[4]; }
    float& alpha_rate() { return rates[3]; }
    float& unk_10_rate() { return rates[4]; }
};
static_assert(sizeof(EffectColourChannels) == 0x3C);
}
