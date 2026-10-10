#include "gbtg/bundle.h"
#include "gbtg/anim.h"
#include "gbtg/language.h"
#include "gbtg/resource_error.h"
#include "gbtg/dialogue.h"
#include "gbtg/engine_internal.h"
#include "gbtg/font.h"
#include "gbtg/gfx.h"
#include "gbtg/hash.h"
#include "gbtg/loctext.h"
#include "gbtg/movie.h"
#include "gbtg/resource.h"
#include "gbtg/texture.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <vector>


#include "frontend_types.h"
#include "frontend.h"
#include "frontend_captions.h"
#include "frontend_keyboard.h"
#include "frontend_slots.h"
#include "frontend_text.h"

namespace gbtg
{
using namespace frontend_detail;

namespace frontend_detail
{


// 0x0004CE26, type 0x15. The name at +0x10 is joined as "dialogs__<name>"
// (0x000FF0A0) and looked up in aid_loctext_ghoulies_dialog_dialogs.
// {page} starts another screen. {nobutton} hides the A prompt. {settime}
// is the attract hold; a line without it waits on A (0x000584D0).
struct DialogPage
{
    std::u16string text;
    std::string speaker;
    std::string cue;
    bool button = true;
};


struct MoveCaption
{
    float start = 0.f;
    float seconds = 0.f;
    // 0x000589F0 stores DAT_0032B248 (0.6). {scale} replaces it (0x000570C2).
    float scale = 0.6f;
    bool head = true;
    bool pause_audio = true;
    // Type 0x15 record +0xD0. 0x0004C4F0 plays it when the hold ends.
    std::string next_cue;
    std::string motif;
    u8 target = 0;
    std::vector<DialogPage> pages;
};


int g_caption_hold = -1;

int g_caption_page = 0;

float g_caption_left = 0.f;

bool g_caption_wait = false;

DialogueScreen g_dialogue_screen;

int g_dialogue_mode = 0;

float g_dialogue_timer = 0.f;

float g_dialogue_fade = 0.f;

std::string g_dialogue_motif;


void reset_caption_hold()
{
    g_dialogue_screen.close();
    g_dialogue_mode = 0;
    pcm_dialog_hold(false);
    if (!g_dialogue_motif.empty())
    {
        pcm_stop_cue(g_dialogue_motif.c_str());
        g_dialogue_motif.clear();
    }
    g_caption_hold = -1;
    g_caption_page = 0;
    g_caption_left = 0.f;
    g_caption_wait = false;
}


float tag_number(std::string_view tag)
{
    const size_t sp = tag.find(' ');
    if (sp == std::string_view::npos || sp + 1 >= tag.size())
    {
        return 0.f;
    }
    return std::strtof(std::string(tag.substr(sp + 1)).c_str(), nullptr);
}


bool speaker_token(std::string_view tag)
{
    // Tokens below SpeakerMAX (index 0x12). 0x00056F80 sends these to 0x000576D0.
    static const char* kNames[] = {
        "boy",           "girl",          "butler",        "cook",
        "groundskeeper", "housekeeper",   "krackpot",      "baroness",
        "baron",         "crone",         "piratecaptain", "skeletonbad",
        "mrribs",        "pirate",        "littleguy",     "zombiestandard",
        "hunchback",     "binklogo",
    };
    for (const char* name : kNames)
    {
        if (tag == name)
        {
            return true;
        }
    }
    return false;
}


void parse_dialog_line(std::string_view raw, MoveCaption& cap)
{
    std::string visible;
    std::string speaker;
    std::string cue;
    bool show_button = true;
    auto flush = [&]()
    {
        if (visible.empty())
        {
            return;
        }
        DialogPage page;
        page.text = utf8_to_u16(visible);
        page.speaker = speaker;
        page.cue = cue;
        page.button = show_button;
        cap.pages.push_back(std::move(page));
        visible.clear();
    };
    size_t i = 0;
    while (i < raw.size())
    {
        if (raw[i] != '{')
        {
            visible.push_back(raw[i]);
            ++i;
            continue;
        }
        const size_t end = raw.find('}', i + 1);
        if (end == std::string_view::npos)
        {
            break;
        }
        const std::string_view tag = raw.substr(i + 1, end - (i + 1));
        if (tag.rfind("settime", 0) == 0)
        {
            const float seconds = tag_number(tag);
            if (seconds > 0.f)
            {
                cap.seconds = seconds;
            }
        }
        else if (tag.rfind("scale ", 0) == 0 || tag == "scale")
        {
            const float scale = tag_number(tag);
            if (scale > 0.f)
            {
                cap.scale = scale;
            }
        }
        else if (tag == "page")
        {
            flush();
        }
        else if (tag.rfind("sfx ", 0) == 0)
        {
            // Xbox 0x00057B00 builds the XACT cue name; 0x00057D10 plays it
            // on entry to mode 2, rather than while measuring the text.
            cue = "XACT_SOUNDBANK_" + std::string(tag.substr(4));
        }
        else if (tag == "linek" || tag == "linel" || tag == "linec" || tag == "liner")
        {
            visible.push_back('\n');
        }
        else if (tag == "nobutton")
        {
            // 0x0005709D. DAT_004EE290 = 0, so the accept widget is not drawn.
            show_button = false;
        }
        else if (tag == "nohead")
        {
            // 0x000570A9. DAT_004EE294 = 0, so 0x000575F0 does not draw the head.
            cap.head = false;
        }
        else if (speaker_token(tag))
        {
            speaker = std::string(tag);
        }
        i = end + 1;
    }
    flush();
}


const LocTable& dialog_loctext(const Game& g)
{
    static LocTable table;
    static bool loaded = false;
    if (loaded)
    {
        return table;
    }
    loaded = true;
    // aid_loctext_ghoulies_dialog_dialogs
    const auto path = g.aids.game_dir / "loctext" / language_folder(g_language) / "0c50e153";
    if (!std::filesystem::exists(path))
    {
        return table;
    }
    try
    {
        table = parse_loctext(read_file(path), path.filename().string());
    }
    catch (const std::exception& ex)
    {
        std::cerr << "dialog loctext " << ex.what() << "\n";
    }
    return table;
}


std::string event_cstring(std::span<const u8> rec, size_t at)
{
    if (at >= rec.size())
    {
        return {};
    }
    size_t n = at;
    while (n < rec.size() && rec[n] != 0)
    {
        ++n;
    }
    return std::string(reinterpret_cast<const char*>(rec.data() + at), n - at);
}


const std::vector<MoveCaption>& move_captions(const Game& g)
{
    static std::string clip;
    static std::vector<MoveCaption> lines;
    if (!g.cutscenes.playing || g.cutscenes.index >= g.cutscenes.items.size() || g.scene_bundle == nullptr)
    {
        clip.clear();
        lines.clear();
        reset_caption_hold();
        return lines;
    }
    const std::string& name = g.cutscenes.items[g.cutscenes.index].name;
    if (name == clip)
    {
        return lines;
    }
    clip = name;
    lines.clear();
    reset_caption_hold();
    std::string prefix = name;
    const std::string from = "aid_cutscene_";
    if (prefix.rfind(from, 0) == 0)
    {
        prefix.replace(0, from.size(), "aid_cutsceneevents_");
    }
    const LocTable& table = dialog_loctext(g);
    for (const auto& e : g.scene_bundle->entries)
    {
        if (e.type != ResourceType::CutsceneEvents || e.name.rfind(prefix, 0) != 0)
        {
            continue;
        }
        if (e.name.size() > prefix.size() && e.name[prefix.size()] != '_')
        {
            continue;
        }
        size_t pos = 0;
        while (pos + 16 <= e.cpu.size())
        {
            const u32 size = read_u32(e.cpu, pos);
            if (size < 16 || pos + size > e.cpu.size())
            {
                break;
            }
            const auto rec = e.cpu.subspan(pos, size);
            const u32 type = read_u32(rec, 0x0c) & 0xffff;
            if (type == 0x15 && rec.size() > 0x10)
            {
                const std::string key = "dialogs__" + event_cstring(rec, 0x10);
                const auto it = table.keys_to_utf8.find(key);
                if (it != table.keys_to_utf8.end())
                {
                    MoveCaption cap;
                    // Attract bypasses the global pause in 0x000584D0.
                    // The event's own cutscene wait (0x0004E640) still applies.
                    cap.pause_audio = !scene_control_starts(g, "attract");
                    cap.start = read_f32(rec, 4);
                    // Xbox 0x0004CE26 / 0x0004D510: motif at +0x50,
                    // background byte at +0x150. 0x0004C4F0 releases it.
                    cap.motif = event_cstring(rec, 0x50);
                    if (cap.motif == "XACT_SOUNDBANK_GMISC_NOSOUND")
                    {
                        cap.motif.clear();
                    }
                    cap.target = rec.size() > 0x150 ? rec[0x150] : 0;
                    if (rec.size() > 0xd0)
                    {
                        const std::string cue = event_cstring(rec, 0xd0);
                        if (cue.rfind("XACT_SOUNDBANK_", 0) == 0 && cue != "XACT_SOUNDBANK_GMISC_NOSOUND")
                        {
                            cap.next_cue = cue;
                        }
                    }
                    parse_dialog_line(it->second, cap);
                    if (!cap.pages.empty())
                    {
                        lines.push_back(std::move(cap));
                    }
                }
            }
            pos += size;
        }
    }
    return lines;
}


// Xbox 0x00058C30. 0x00059880 measures the line count at +0x0C. The quad's
// -1/+1 corners cancel the half-size matrix in 0x0005CA60. 0x00060980 adds
// two pixels of spacing, and 0x0005E9F0 draws the five-pixel shadow first.
void draw_dialog_caption(const GloopyFont& font, std::u16string_view text, float scale)
{
    if (text.empty() || font.cell() <= 0 || scale == 0.f || g_dialogue_fade == 0.f)
    {
        return;
    }
    std::vector<std::u16string_view> rows;
    size_t at = 0;
    while (at <= text.size())
    {
        const size_t br = text.find(u'\n', at);
        const size_t end = br == std::u16string_view::npos ? text.size() : br;
        if (end > at)
        {
            rows.push_back(text.substr(at, end - at));
        }
        if (br == std::u16string_view::npos)
        {
            break;
        }
        at = br + 1;
    }
    if (rows.empty())
    {
        return;
    }
    constexpr float kFirst = 2.f;
    constexpr float kLift = 37.5f;
    // 0x58C30 scales both the font state and the centred draw matrix.
    const float draw = scale * g_dialogue_fade * g_dialogue_fade;
    const float spacing = 2.f * g_dialogue_fade;
    const float line_h = float(font.drop()) * draw * 1.1f;
    const float pen_y = (kFrameH - float(rows.size()) * line_h) * 0.5f + line_h * 0.5f;
    const int frame = font.rest_frame();
    for (size_t row = 0; row < rows.size(); ++row)
    {
        const std::u16string_view line = rows[row];
        const float y = pen_y + line_h * float(row);
        // 0x000596DB consumes the first-letter flag. 0x00057043 ({linek})
        // sets the line-break/alignment flags without re-arming that flag.
        const float big = row == 0 ? draw * kFirst : draw;
        float width = float(font.advance(line.front())) * big + spacing;
        for (size_t i = 1; i < line.size(); ++i)
        {
            width += float(font.advance(line[i])) * draw + spacing;
        }
        float x = (kFrameW - width) * 0.5f;
        for (size_t i = 0; i < line.size(); ++i)
        {
            const float size = i == 0 ? big : draw;
            const float gy = row == 0 && i == 0 ? y - kLift * draw : y;
            if (const RgbaImage* glyph = font.image(line[i], frame))
            {
                const float em = float(font.cell()) * size;
                const float gx = x - float(font.bearing(line[i])) * size;
                const float shadow = 5.f * g_dialogue_fade * g_dialogue_fade;
                gfx_sprite_submit(*glyph, gx + shadow, gy + shadow, em, em, 0.f, 0.f, 1.f, 1.f,
                                  {0.f, 0.f, 0.f, 128.f / 255.f});
                gfx_sprite_submit(*glyph, gx, gy, em, em, 0.f, 0.f, 1.f, 1.f,
                                  {1.f, 1.f, 1.f, 1.f});
            }
            x += float(font.advance(line[i])) * size + spacing;
        }
    }
}


// 0x000576D0 builds aid_texture_ghoulies_scoreboard_dialogheads_<token>
// (format at 0x00323588) and 0x000575F0 draws it. The position constants are
// DAT_0032B564 (320) and DAT_0032B2F0 (80). 0x0005CA60 centres the quad.
struct DialogHeadFrames
{
    std::vector<RgbaImage> frames;
};


const DialogHeadFrames& dialog_head_frames(const Game& g, const std::string& speaker)
{
    static std::map<std::string, DialogHeadFrames> cache;
    const auto found = cache.find(speaker);
    if (found != cache.end())
    {
        return found->second;
    }
    DialogHeadFrames result;
    const std::string name = "aid_texture_ghoulies_scoreboard_dialogheads_" + speaker;
    const auto path = g.aids.game_dir / "demand" / aid_hash_hex(aid_hash(name));
    if (std::filesystem::exists(path))
    {
        try
        {
            LooseResource file = parse_loose_resource(read_file(path));
            TextureHeader hdr;
            if (parse_texture_header(file.cpu, hdr))
            {
                std::span<const u8> gpu = file.gpu;
                if (!file.gpu_chunks.empty())
                {
                    const auto& chunk = file.gpu_chunks[0];
                    if (size_t(chunk.offset) + chunk.size <= file.gpu.size())
                    {
                        gpu = file.gpu.subspan(chunk.offset, chunk.size);
                    }
                }
                // 0x00057792 reads the frame count at texture+0x11. Each
                // frame occupies data_size bytes, as in the font textures.
                const size_t count = hdr.tile_count;
                for (size_t frame = 0; frame < count; ++frame)
                {
                    const size_t offset = size_t(hdr.texture_offset) + frame * hdr.data_size;
                    if (offset > gpu.size() || hdr.data_size > gpu.size() - offset)
                    {
                        throw Error("dialog head frame out of range");
                    }
                    TextureHeader tile = hdr;
                    tile.texture_offset = 0;
                    result.frames.push_back(decode_xbox_texture(tile, gpu.subspan(offset, hdr.data_size)));
                }
            }
        }
        catch (const std::exception& ex)
        {
            result.frames.clear();
            std::cerr << name << " " << ex.what() << "\n";
        }
    }
    return cache.emplace(speaker, std::move(result)).first->second;
}


struct DialogHeadAnimation
{
    std::string speaker;
    unsigned seed = 0;
    int state = 0;
    int frame = 0;
    float remaining = 0.f;
    u32 tick = static_cast<u32>(-1);
};

DialogHeadAnimation g_dialog_head_animation;


const RgbaImage* dialog_head(const Game& g, const std::string& speaker)
{
    const auto& frames = dialog_head_frames(g, speaker).frames;
    if (frames.empty())
    {
        return nullptr;
    }
    auto& anim = g_dialog_head_animation;
    if (anim.speaker != speaker)
    {
        anim.speaker = speaker;
        anim.state = 0;
        anim.frame = 0;
        anim.remaining = 0.f;
    }
    if (anim.tick != g.frame)
    {
        anim.tick = g.frame;
        // 0x00057450: probabilistic blink, then a 0.1-second forward
        // pass and a 0.1-second reverse pass. This uses the executable's
        // LCG formula with a host-local seed; shared RNG state is not ported.
        const int last = int(frames.size()) - 1;
        if (anim.state == 0)
        {
            anim.seed = anim.seed * 0x19660Du + 0x3C6EF35Fu;
            const float random = std::bit_cast<float>((anim.seed & 0x7fffffu) | 0x3f800000u) - 1.f;
            if (random <= 0.01f)
            {
                anim.state = 1;
                anim.remaining = 0.1f;
            }
        }
        else
        {
            const float fraction = anim.remaining * 10.f;
            anim.frame = int((anim.state == 1 ? 1.f - fraction : fraction) * float(last));
            anim.remaining -= g.clock.dt;
            if (anim.remaining <= 0.f)
            {
                if (anim.state == 1)
                {
                    anim.state = 2;
                    anim.remaining = 0.1f;
                    anim.frame = last;
                }
                else
                {
                    anim.state = 0;
                    anim.remaining = 0.f;
                    anim.frame = 0;
                }
            }
        }
    }
    return &frames[size_t(std::clamp(anim.frame, 0, int(frames.size()) - 1))];
}


void draw_move_captions(const Game& g, bool texture_pass)
{
    if (g_caption_hold < 0 || g_dialogue_mode == 0)
    {
        return;
    }
    const std::vector<MoveCaption>& lines = move_captions(g);
    if (size_t(g_caption_hold) >= lines.size())
    {
        return;
    }
    const MoveCaption& cap = lines[size_t(g_caption_hold)];
    if (cap.pages.empty())
    {
        return;
    }
    const int page = std::clamp(g_caption_page, 0, int(cap.pages.size()) - 1);
    const DialogPage& shown = cap.pages[size_t(page)];
    const float widget_scale = 1.5f * (g_dialogue_mode == 3 ? 0.f : g_dialogue_fade);
    const bool content = cap.target == 0 || texture_pass;
    if (content)
    {
        g_dialogue_screen.draw(cap.target != 0, !texture_pass && shown.button, widget_scale);
    }
    if (content && cap.head && !shown.speaker.empty())
    {
        if (const RgbaImage* head = dialog_head(g, shown.speaker))
        {
            const float w = float(head->width) * g_dialogue_fade;
            const float h = float(head->height) * g_dialogue_fade;
            gfx_sprite_submit(*head, 320.f - w * 0.5f, 80.f - h * 0.5f, w, h,
                              0.f, 0.f, 1.f, 1.f);
        }
    }
    const RgbaImage* button = nullptr;
    if (!texture_pass && shown.button && widget_scale >= 0.001f)
    {
        if (cap.target != 0)
        {
            g_dialogue_screen.draw_button(widget_scale);
        }
        button = joypad_icon(g, 0);
    }
    if (content)
    {
        draw_dialog_caption(dialog_font(g), shown.text, cap.scale);
    }
    if (button != nullptr)
    {
        const float w = float(button->width) * widget_scale;
        const float h = float(button->height) * widget_scale;
        gfx_sprite_submit(*button, 320.f - w * 0.5f, dialogue_button_y - h * 0.5f, w, h);
    }
}

} // namespace frontend_detail



// Xbox 0x00058150. Background dialogues render to the texture selected by
// 0x001417A0 before the book samples it; target 0 uses the normal HUD pass.
void prepare_dialogue_texture(const Game& g)
{
    if (g_dialogue_mode == 0 || g_caption_hold < 0)
    {
        return;
    }
    const auto& lines = move_captions(g);
    const u8 slot = lines.at(size_t(g_caption_hold)).target;
    if (slot == 0)
    {
        return;
    }
    const RgbaImage* target = nullptr;
    for (const auto& prop : g.cutscene_props)
    {
        for (const auto& texture : prop.mesh.textures)
        {
            if (texture.movie_slot == slot)
            {
                target = &texture;
            }
        }
    }
    for (const auto& mesh : g.models)
    {
        for (const auto& texture : mesh.textures)
        {
            if (texture.movie_slot == slot)
            {
                target = &texture;
            }
        }
    }
    if (target == nullptr)
    {
        throw Error("dialogue dynamic texture slot missing: " + std::to_string(slot));
    }
    gfx_begin_dialog_texture(slot, target->width, target->height);
    draw_move_captions(g, true);
    gfx_end_dialog_texture();
}


void arm_caption(const Game& g, const MoveCaption& cap)
{
    g_caption_page = 0;
    g_caption_left = 0.f;
    g_caption_wait = false;
    g_dialogue_screen.open(g);
    // Xbox 0x000584D0, mode 1. Mode 2 begins after the 0.3-second fade.
    g_dialogue_mode = 1;
    g_dialogue_timer = 0.3f;
    g_dialogue_fade = 0.f;
    const bool button = !cap.pages.empty() && cap.pages[0].button;
    g_caption_wait = button;
    if (cap.pause_audio)
    {
        pcm_dialog_hold(true);
    }
    if (!cap.motif.empty())
    {
        g_dialogue_motif = cap.motif;
        pcm_play_cue(g, g_dialogue_motif.c_str(), true);
    }
}


void caption_accept(const Game& g)
{
    if (!g_caption_wait || g_dialogue_mode != 2)
    {
        return;
    }
    const std::vector<MoveCaption>& lines = move_captions(g);
    if (g_caption_hold < 0 || size_t(g_caption_hold) >= lines.size())
    {
        g_caption_wait = false;
        return;
    }
    // Xbox 0x00058B80 / 0x00057E60: A requests mode 3, not immediate close.
    // Xbox 0x00142460 / 0x00141E20: the prompt plays its cue before its callback.
    pcm_play_owned_cue(g, "XACT_SOUNDBANK_GSUPERSCARYSTUF_RIGHTBUTTON");
    g_dialogue_mode = 3;
    g_dialogue_timer = 0.3f;
}


float attract_caption_dt(const Game& g, float dt)
{
    // Xbox 0x0004C850 -> 0x0004E640 pauses the event's cutscene at +0x158.
    // 0x0004DA50 releases it only after dialogue mode returns to zero.
    // This wait also applies to title captions; their audio keeps playing.
    // Book scenes leave the menu widget down (mode -1) and still show type 0x15.
    if (g.frontend_mode > 0)
    {
        return dt;
    }
    const std::vector<MoveCaption>& lines = move_captions(g);
    if (g_dialogue_mode != 0)
    {
        // Xbox 0x00057E60. The detached page advances during every hold.
        g_dialogue_screen.tick(dt);
        const MoveCaption& cap = lines.at(size_t(g_caption_hold));
        switch (g_dialogue_mode)
        {
        case 1:
            g_dialogue_fade = std::clamp((g_dialogue_timer - 0.3f) * -3.333333f, 0.f, 1.f);
            g_dialogue_timer -= dt;
            if (g_dialogue_timer <= 0.f)
            {
                g_dialogue_mode = 2;
                g_dialogue_fade = 1.f;
                g_caption_left = cap.seconds > 0.f ? cap.seconds : 0.1f;
                const auto& shown = cap.pages.at(size_t(g_caption_page));
                g_caption_wait = shown.button;
                if (cap.head && !shown.cue.empty())
                {
                    pcm_play_cue(g, shown.cue.c_str(), false);
                }
            }
            break;
        case 2:
            if (!g_caption_wait)
            {
                g_caption_left -= dt;
                if (g_caption_left <= 0.f)
                {
                    g_dialogue_mode = 3;
                    g_dialogue_timer = 0.3f;
                }
            }
            break;
        case 3:
            g_dialogue_fade = std::clamp(1.f - (g_dialogue_timer - 0.3f) * -3.333333f, 0.f, 1.f);
            g_dialogue_timer -= dt;
            if (g_dialogue_timer <= 0.f)
            {
                // Xbox 0x00058AE0: parse the next page or enter mode 4.
                if (g_caption_page + 1 < int(cap.pages.size()))
                {
                    ++g_caption_page;
                    g_dialogue_mode = 1;
                    g_dialogue_timer = 0.3f;
                    g_dialogue_fade = 0.f;
                }
                else
                {
                    g_dialogue_mode = 4;
                }
            }
            break;
        case 4:
            g_dialogue_mode = 0;
            g_caption_wait = false;
            g_caption_left = 0.f;
            g_dialogue_screen.close();
            if (!g_dialogue_motif.empty())
            {
                pcm_stop_cue(g_dialogue_motif.c_str());
                g_dialogue_motif.clear();
            }
            pcm_dialog_advance(g, cap.next_cue.c_str());
            break;
        }
        return 0.f;
    }
    const float time = g.cutscenes.time;
    for (int i = g_caption_hold + 1; i < int(lines.size()); ++i)
    {
        const float start = lines[size_t(i)].start;
        if (time < start && time + dt >= start)
        {
            g_caption_hold = i;
            arm_caption(g, lines[size_t(i)]);
            return start - time;
        }
        if (time >= start)
        {
            g_caption_hold = i;
            arm_caption(g, lines[size_t(i)]);
            return 0.f;
        }
    }
    return dt;
}

} // namespace gbtg
