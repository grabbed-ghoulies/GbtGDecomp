#pragma once

#include "gbtg/texture.hpp"

namespace gbtg
{

// Gloopy font (font/069563d4, type 21).
// FUN_000516a0: the CPU blob is the font object. Glyphs start at +0x18,
// count = last - first + 1, stride 0x18.
// FUN_00051760: advance = x1 - x0. A missing glyph advances by the cell width.
// FUN_00053550: eight 32×32 DXT3 frames (byte +0x11). The clock runs only when
// texture byte +0x13 is 0xFF (FUN_0005CA60 left the frame at -1):
// frame = (int)(time * byte +0x10) % count.
// FUN_00060070's settled result for a flag-1 string is the last frame.
class GloopyFont
{
  public:
    bool load(const std::filesystem::path& path);
    int cell() const { return cell_; }
    // CPU dword +0x10. 0x000809C0 filds it and subtracts it, times 1.125, from the HUD anchor.
    int drop() const { return drop_; }
    int frame(float time) const;
    int rest_frame() const { return frame_count_ > 0 ? frame_count_ - 1 : 0; }
    // Space (0x20) uses the advance of 'l' (FUN_000607f0). Other missing codes advance by cell().
    int advance(char32_t codepoint) const;
    int bearing(char32_t codepoint) const;
    const RgbaImage* image(char32_t codepoint, int frame) const;

  private:
    struct Glyph
    {
        bool present = false;
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        std::vector<RgbaImage> frames;
    };

    const Glyph* find(char32_t codepoint) const;

    int first_ = 0;
    int cell_ = 32;
    int drop_ = 32;
    int frame_count_ = 1;
    float frame_rate_ = 25.f;
    std::vector<Glyph> glyphs_;
};

} // namespace gbtg
