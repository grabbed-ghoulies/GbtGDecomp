#include "gbtg/anim.hpp"
#include "gbtg/font.hpp"
#include "gbtg/game.hpp"
#include "gbtg/gfx.hpp"
#include "gbtg/resource.hpp"
#include "gbtg/texture.hpp"

#include <algorithm>
#include <iostream>
#include <string>

namespace gbtg
{
namespace
{

constexpr const char* kNoScript = "There is no script for this scene!";
constexpr const char* kSceneOrder = "aid_aidlist_ghoulies_sceneorder_game";

Entity* spawn(Game& g, const char* type_name, Actor& actor)
{
    const EntityType* type = find_entity_type(type_name);
    g.entities.push_back(Entity{type, &actor, actor.position, actor.position, actor.euler, false});
    Entity& e = g.entities.back();
    if (e.type && e.type->tick)
    {
        e.type->tick(e);
    }
    return &e;
}

const BundleEntry* find_aid(const Game& g, std::string_view name)
{
    if (g.scene_bundle)
    {
        if (const BundleEntry* e = find_entry(*g.scene_bundle, name))
        {
            return e;
        }
    }
    return find_entry(g.aids.common, name);
}

void log_script_refs(Game& g, const SceneScript& script)
{
    for (const auto& aid : script.aids)
    {
        if (const BundleEntry* e = find_aid(g, aid))
        {
            if (e->type == ResourceType::AidList)
            {
                auto names = parse_aidlist(e->cpu);
                g.script_aidlists.push_back(aid);
                std::cout << "  aidlist " << aid << " (" << names.size() << ")\n";
                for (size_t i = 0; i < names.size() && i < 12; ++i)
                {
                    std::cout << "    [" << i << "] " << names[i] << "\n";
                }
                if (names.size() > 12)
                {
                    std::cout << "    ...\n";
                }
                continue;
            }
            std::cout << "  " << resource_type_name(e->type) << " " << aid << "\n";
            continue;
        }
        const u32 hash = aid_hash(aid);
        if (g.aids.demand_exists(hash))
        {
            std::cout << "  demand " << aid_hash_hex(hash) << " " << aid << "\n";
        }
        else
        {
            std::cout << "  ref " << aid << "\n";
        }
    }
}

void log_cutscene_begin(const Game& g)
{
    const auto& q = g.cutscenes;
    const Cutscene& c = q.items[q.index];
    std::cout << "cutscene begin [" << q.index << "/" << q.items.size() << "] " << c.name << " duration=" << c.duration
              << "s tracks=" << int(c.tracks) << " cameras=" << c.cameras.size() << " actors=" << c.actors.size()
              << " curves=" << c.camera.size() << "\n";
    Vec3 pos;
    Quat orient;
    float fov = 0.f;
    if (sample_cutscene_camera(c, q.time, pos, orient, fov))
    {
        std::cout << "  cam " << c.camera_name << " pos=" << pos.x << "," << pos.y << "," << pos.z << " quat=" << orient.x
                  << "," << orient.y << "," << orient.z << "," << orient.w << " fov=" << fov << "\n";
    }
}

void log_cutscene_end(const Game& g)
{
    const auto& q = g.cutscenes;
    std::cout << "cutscene end [" << q.index << "/" << q.items.size() << "] " << q.items[q.index].name << "\n";
}

void load_cutscene_reel(Game& g)
{
    g.cutscenes = {};
    g.cutscenes.loop = true;
    for (const auto& list_name : g.script_aidlists)
    {
        const BundleEntry* list = find_aid(g, list_name);
        if (!list || list->type != ResourceType::AidList)
        {
            continue;
        }
        const auto names = parse_aidlist(list->cpu);
        bool any_cutscene = false;
        for (const auto& n : names)
        {
            if (n.starts_with("aid_cutscene_"))
            {
                any_cutscene = true;
                break;
            }
        }
        if (!any_cutscene)
        {
            continue;
        }
        float total = 0.f;
        for (const auto& n : names)
        {
            const BundleEntry* e = find_aid(g, n);
            if (!e || e->type != ResourceType::Cutscene)
            {
                std::cout << "  cutscene missing " << n << "\n";
                continue;
            }
            Cutscene clip = parse_cutscene(e->cpu, e->name);
            total += clip.duration;
            g.cutscenes.items.push_back(std::move(clip));
        }
        std::cout << "cutscene reel " << list_name << " " << g.cutscenes.items.size() << " clips " << total << "s\n";
        for (size_t i = 0; i < g.cutscenes.items.size(); ++i)
        {
            const auto& c = g.cutscenes.items[i];
            std::cout << "  [" << i << "] " << c.duration << "s curves=" << c.camera.size() << " " << c.name << "\n";
        }
        break;
    }
}

void load_scene_models(Game& g)
{
    g.models.clear();
    for (const auto& aid : g.scene_script.aids)
    {
        if (!aid.starts_with("aid_model_"))
        {
            continue;
        }
        const BundleEntry* entry = find_aid(g, aid);
        if (!entry || entry->type != ResourceType::Model)
        {
            std::cout << "  model missing " << aid << "\n";
            continue;
        }
        try
        {
            Mesh mesh = parse_model(*entry);
            Vec3 lo{1e9f, 1e9f, 1e9f};
            Vec3 hi{-1e9f, -1e9f, -1e9f};
            for (const Vec3& p : mesh.positions)
            {
                lo.x = std::min(lo.x, p.x);
                lo.y = std::min(lo.y, p.y);
                lo.z = std::min(lo.z, p.z);
                hi.x = std::max(hi.x, p.x);
                hi.y = std::max(hi.y, p.y);
                hi.z = std::max(hi.z, p.z);
            }
            std::cout << "  model " << mesh.name << " verts " << mesh.positions.size() << " idx " << mesh.indices.size()
                      << " tex " << mesh.textures.size() << " bounds " << lo.x << "," << lo.y << "," << lo.z << " .. " << hi.x
                      << "," << hi.y << "," << hi.z << "\n";
            int lit_sm = 0;
            int detail_sm = 0;
            int bare_sm = 0;
            for (const Submesh& sm : mesh.submeshes)
            {
                if (sm.lightmap >= 0)
                {
                    ++lit_sm;
                }
                if (sm.detail >= 0)
                {
                    ++detail_sm;
                }
                if (sm.texture < 0)
                {
                    ++bare_sm;
                }
            }
            std::cout << "    submeshes " << mesh.submeshes.size() << " lit " << lit_sm << " detail " << detail_sm
                      << " bare " << bare_sm << "\n";
            if (!mesh.positions.empty())
            {
                g.models.push_back(std::move(mesh));
            }
        }
        catch (const std::exception& ex)
        {
            std::cout << "  model fail " << aid << ": " << ex.what() << "\n";
        }
    }
}

RgbaImage image_from_entry(const BundleEntry& entry)
{
    TextureHeader hdr;
    if (entry.cpu.size() < 28 || !parse_texture_header(entry.cpu, hdr))
    {
        throw Error("texture header");
    }
    std::span<const u8> gpu = entry.gpu;
    if (!entry.gpu_chunks.empty())
    {
        const auto& chunk = entry.gpu_chunks[0];
        if (size_t(chunk.offset) + chunk.size <= entry.gpu.size())
        {
            gpu = entry.gpu.subspan(chunk.offset, chunk.size);
        }
    }
    return decode_xbox_texture(hdr, gpu);
}

void load_ui(Game& g)
{
    g.ui.clear();
    if (!g.scene_bundle || g.scene_name.find("gameselect") == std::string::npos)
    {
        return;
    }
    for (const auto* entry : entries_of_type(*g.scene_bundle, ResourceType::Texture))
    {
        try
        {
            Game::UiImage image;
            image.name = entry->name;
            image.image = image_from_entry(*entry);
            g.ui.push_back(std::move(image));
        }
        catch (const std::exception& ex)
        {
            std::cout << "  ui fail " << entry->name << ": " << ex.what() << "\n";
        }
    }
    std::cout << "  ui textures " << g.ui.size() << "\n";
}

const Game::UiImage* find_ui(const Game& g, std::string_view key)
{
    for (const auto& image : g.ui)
    {
        if (image.name.find(key) != std::string::npos)
        {
            return &image;
        }
    }
    return nullptr;
}

void load_menu_textures(Game& g)
{
    if (find_ui(g, "gameselect_emptyslotbackground"))
    {
        return;
    }
    // Mode 1 binds these aids by name (0x000DD756). They live in the gameselect bundle.
    constexpr std::string_view kMenu = "aid_script_ghoulies_gameselect_gameselect";
    const auto rel = aid_to_bundle_rel(kMenu);
    if (!rel || !std::filesystem::exists(g.aids.bundle_path(*rel)))
    {
        return;
    }
    const Bundle& bundle = g.aids.load_bundle_rel(*rel);
    for (const auto* entry : entries_of_type(bundle, ResourceType::Texture))
    {
        try
        {
            Game::UiImage image;
            image.name = entry->name;
            image.image = image_from_entry(*entry);
            g.ui.push_back(std::move(image));
        }
        catch (const std::exception& ex)
        {
            std::cout << "  ui fail " << entry->name << ": " << ex.what() << "\n";
        }
    }
    std::cout << "  menu textures " << g.ui.size() << "\n";
}

void begin_frontend(Game& g)
{
    // 0x000E5AE0. Stream id 2 idle (0x00039D50 returns 0) jumps to 0x000E5B27:
    // widget vtable = mode[DAT_00429BC0]. Boot 0x000E5A80 set that mode to 1.
    // Attract is the title: the cutscene actor tracks (Rare book, studio book, logo book).
    // Slot select is mode 1. It stays down until A, so the books are the picture.
    if (g.scene_name.find("attract") != std::string::npos)
    {
        g.frontend_mode = 0;
        g.naming = false;
        std::cout << "title attract\n";
        return;
    }
    if (g.scene_name.find("gameselect") == std::string::npos)
    {
        g.frontend_mode = 0;
        g.naming = false;
        return;
    }
    load_menu_textures(g);
    g.frontend_mode = 1;
    g.naming = false;
    std::cout << "frontend mode 1 slot select\n";
}

GfxRect ui_letterbox()
{
    int width = 0;
    int height = 0;
    gfx_size(width, height);
    if (width <= 0 || height <= 0)
    {
        width = 960;
        height = 540;
    }
    const float frame = 640.f / 480.f;
    const float window = float(width) / float(height);
    GfxRect board;
    if (window > frame)
    {
        board.h = 1.f;
        board.w = frame / window;
        board.x = (1.f - board.w) * 0.5f;
    }
    else
    {
        board.w = 1.f;
        board.h = window / frame;
        board.y = (1.f - board.h) * 0.5f;
    }
    return board;
}

GfxRect ui_place(const GfxRect& board, float x, float y, float w, float h)
{
    GfxRect rect;
    rect.x = board.x + board.w * (x / 640.f);
    rect.y = board.y + board.h * (y / 480.f);
    rect.w = board.w * (w / 640.f);
    rect.h = board.h * (h / 480.f);
    return rect;
}

void draw_sprite(const Game::UiImage* image, const GfxRect& rect)
{
    if (image)
    {
        gfx_draw_sprite(image->image, rect);
    }
}

void load_scene_contents(Game& g, std::string_view name)
{
    script_set_default_name(g);
    g.slot = 0;
    g.slot_up = false;
    g.slot_down = false;
    g.slot_back = false;
    if (name.empty())
    {
        return;
    }
    script_set_name(g, name);
    if (const auto rel = aid_to_bundle_rel(g.scene_name))
    {
        const auto path = g.aids.bundle_path(*rel);
        if (std::filesystem::exists(path))
        {
            g.scene_bundle = &g.aids.load_bundle_rel(*rel);
            std::cout << "loaded " << *rel << " entries " << g.scene_bundle->entries.size() << "\n";
            auto scripts = entries_of_type(*g.scene_bundle, ResourceType::Script);
            if (scripts.empty())
            {
                std::cout << kNoScript << "\n";
            }
            for (const auto* e : scripts)
            {
                g.scene_script = parse_scene_script(e->cpu, e->name);
                std::cout << "  script " << e->name << " bytes " << e->cpu.size()
                          << " root=" << g.scene_script.root_offset << " aids " << g.scene_script.aids.size() << "\n";
                log_script_refs(g, g.scene_script);
            }
            for (const auto* e : entries_of_type(*g.scene_bundle, ResourceType::AidList))
            {
                bool already = false;
                for (const auto& n : g.script_aidlists)
                {
                    if (n == e->name)
                    {
                        already = true;
                        break;
                    }
                }
                if (already)
                {
                    continue;
                }
                auto names = parse_aidlist(e->cpu);
                g.script_aidlists.push_back(e->name);
                std::cout << "  aidlist " << e->name << " (" << names.size() << ")\n";
                for (size_t i = 0; i < names.size() && i < 12; ++i)
                {
                    std::cout << "    [" << i << "] " << names[i] << "\n";
                }
                if (names.size() > 12)
                {
                    std::cout << "    ...\n";
                }
            }
            load_cutscene_reel(g);
            load_scene_models(g);
            load_ui(g);
        }
        else
        {
            std::cout << "scene bundle missing: " << path.string() << "\n";
        }
    }
    begin_frontend(g);
}

// Keys built by FUN_00011870. Width is the dword stored at key+4.
// Pages are the three groups split by FUN_00017A40. Layout (FUN_00015C40):
// x starts at 0, y at 135, row pitch is +0xEC (42). The first key of a row then adds 4.
// Draw adds the 64px margin at DAT_0032B450.
struct KbKey
{
    u32 code;
    int width;
};

struct KbRow
{
    KbKey keys[12];
    int count;
};

constexpr int kKbPages = 3;
constexpr int kKbRows = 5;

constexpr KbRow kKb[kKbPages][kKbRows] = {
    {
        {{{0x10007, 166}, {'1', 34}, {'2', 34}, {'3', 34}, {'4', 34}, {'5', 34}, {'6', 34}, {'7', 34}, {'8', 34}, {'9', 34}, {'0', 34}}, 11},
        {{{0x10002, 166}, {'A', 34}, {'B', 34}, {'C', 34}, {'D', 34}, {'E', 34}, {'F', 34}, {'G', 34}, {'H', 34}, {'I', 34}, {'J', 34}}, 11},
        {{{0x10003, 166}, {'K', 34}, {'L', 34}, {'M', 34}, {'N', 34}, {'O', 34}, {'P', 34}, {'Q', 34}, {'R', 34}, {'S', 34}, {'T', 34}}, 11},
        {{{0x10005, 166}, {'U', 34}, {'V', 34}, {'W', 34}, {'X', 34}, {'Y', 34}, {'Z', 34}, {0x10000, 136}}, 8},
        {{{0x10006, 166}, {0x20, 204}, {0x22B2, 68}, {0x22B3, 68}}, 4},
    },
    {
        {{{0x10007, 166}, {'(', 34}, {')', 34}, {'%', 34}, {'\\', 34}, {'/', 34}}, 6},
        {{{0x10002, 166}, {'$', 34}, {0x00A3, 34}, {0x00A5, 34}, {':', 34}, {'\'', 34}, {'"', 34}}, 7},
        {{{0x10003, 166}, {'?', 34}, {'!', 34}, {0x00BF, 34}, {0x00A1, 34}, {'-', 34}, {'&', 34}}, 7},
        {{{0x10004, 166}, {'{', 34}, {'}', 34}, {0x00AB, 34}, {0x00BB, 34}, {',', 34}, {'.', 34}, {0x10000, 136}}, 8},
        {{{0x10006, 166}, {0x20, 204}, {0x22B2, 68}, {0x22B3, 68}}, 4},
    },
    {
        {{{0x10007, 166}, {'1', 34}, {'2', 34}, {'3', 34}, {'4', 34}, {'5', 34}, {'6', 34}, {'7', 34}, {'8', 34}, {'9', 34}, {'0', 34}}, 11},
        {{{0x10002, 166}, {0x00C0, 34}, {0x00C1, 34}, {0x00C2, 34}, {0x00C4, 34}, {0x00C3, 34}, {0x00C5, 34}, {0x00C6, 34}, {0x00C7, 34}, {0x00C8, 34}, {0x00C9, 34}, {0x00CA, 34}}, 12},
        {{{0x10003, 166}, {0x00CB, 34}, {0x00CC, 34}, {0x00CD, 34}, {0x00CE, 34}, {0x00CF, 34}, {0x00D1, 34}, {0x00D2, 34}, {0x00D3, 34}, {0x00D4, 34}, {0x00D5, 34}, {0x00D8, 34}}, 12},
        {{{0x10004, 166}, {0x00D6, 34}, {0x00DF, 34}, {0x00D9, 34}, {0x00DA, 34}, {0x00DB, 34}, {0x00DC, 34}, {0x10000, 136}}, 8},
        {{{0x10006, 166}, {0x20, 204}, {0x22B2, 68}, {0x22B3, 68}}, 4},
    },
};

const KbRow& kb_row(const Game& g)
{
    return kKb[g.kb_page][g.kb_row];
}

void kb_clamp_col(Game& g)
{
    const int n = kb_row(g).count;
    if (g.kb_col >= n)
    {
        g.kb_col = n - 1;
    }
    if (g.kb_col < 0)
    {
        g.kb_col = 0;
    }
}

void kb_open(Game& g)
{
    // 0x000E02D0 empty slot: object +0x94 = 4 (gameslotnaming).
    g.frontend_mode = 4;
    g.naming = true;
    g.kb_page = 0;
    g.kb_row = 0;
    g.kb_col = 0;
    g.kb_saved = 1;
    g.kb_shift = false;
    g.kb_caps = false;
    g.kb_len = g.slot_name_len[g.slot];
    g.kb_caret = g.kb_len;
    for (int i = 0; i < 64; ++i)
    {
        g.kb_text[i] = i < g.kb_len ? g.slot_name[g.slot][i] : 0;
    }
}

bool kb_blocked(const Game& g)
{
    // FUN_00017560. Symbols page cannot rest on Shift or Caps Lock.
    const KbRow& row = kb_row(g);
    if (g.kb_col < 0 || g.kb_col >= row.count)
    {
        return true;
    }
    const u32 code = row.keys[g.kb_col].code;
    return g.kb_page == 1 && (code == 0x10002 || code == 0x10003);
}

void kb_up_adjust(Game& g)
{
    // FUN_00016950, jump table 0x16B14. Adjust the column, then the caller steps the row.
    const int page = g.kb_page;
    const int col = g.kb_col;
    switch (g.kb_row)
    {
    case 0:
        if (page == 1)
        {
            if (col > 1 && col < 5)
            {
                g.kb_saved = col;
                g.kb_col = 1;
            }
            else if (col == 5)
            {
                g.kb_col = (g.kb_saved == 2 || g.kb_saved == 3) ? g.kb_saved : 1;
                g.kb_saved = 5;
            }
        }
        else if (col > 1 && col < 7)
        {
            g.kb_saved = col;
            g.kb_col = 1;
        }
        else if (col > 6 && col < 9)
        {
            g.kb_saved = col;
            g.kb_col = 2;
        }
        else if (col > 8)
        {
            g.kb_saved = col;
            g.kb_col = 3;
        }
        break;
    case 1:
        if (page == 2 && col == 11)
        {
            g.kb_saved = 11;
            g.kb_col = 10;
        }
        else if (page == 1 && col == 6)
        {
            g.kb_saved = 6;
            g.kb_col = 5;
        }
        break;
    case 3:
        if (col == 7)
        {
            int saved = g.kb_saved;
            if (page == 1)
            {
                if (saved > 6)
                {
                    saved = 6;
                }
            }
            else if (saved < 7)
            {
                saved = 7;
            }
            g.kb_col = saved;
        }
        break;
    case 4:
        if (col == 1)
        {
            int saved = g.kb_saved;
            if (saved > 6)
            {
                saved = 6;
            }
            g.kb_col = saved;
        }
        else if (col > 1)
        {
            g.kb_col = 7;
        }
        break;
    default:
        break;
    }
}

void kb_down_adjust(Game& g)
{
    // FUN_00016B30, jump table 0x16D2C.
    const int page = g.kb_page;
    const int col = g.kb_col;
    switch (g.kb_row)
    {
    case 0:
        if (page == 2 && col == 10)
        {
            g.kb_col = g.kb_saved > 10 ? g.kb_saved : 10;
        }
        else if (page == 1 && col == 5)
        {
            int saved = g.kb_saved < 5 ? 5 : g.kb_saved;
            g.kb_col = saved > 6 ? 6 : saved;
        }
        break;
    case 2:
        if (col > 7)
        {
            g.kb_saved = col;
            g.kb_col = 7;
        }
        break;
    case 3:
        if (col > 0 && col < 7)
        {
            g.kb_saved = col;
            g.kb_col = 1;
        }
        else if (col >= 7)
        {
            g.kb_col = g.kb_saved > 8 ? 3 : 2;
        }
        break;
    case 4:
        if (col == 1)
        {
            int saved = g.kb_saved;
            const int cap = page == 1 ? 5 : 6;
            if (saved > cap)
            {
                saved = cap;
            }
            g.kb_col = saved;
        }
        else if (col == 2)
        {
            if (page == 1)
            {
                g.kb_saved = 2;
                g.kb_col = 5;
            }
            else
            {
                int saved = g.kb_saved;
                if (saved > 8)
                {
                    saved = 8;
                }
                else if (saved <= 7)
                {
                    saved = 7;
                }
                g.kb_col = saved;
            }
        }
        else if (col == 3)
        {
            if (page == 1)
            {
                g.kb_saved = 3;
                g.kb_col = 5;
            }
            else
            {
                g.kb_col = g.kb_saved < 10 ? 9 : 10;
            }
        }
        break;
    default:
        break;
    }
}

void kb_move(Game& g, int dcol, int drow)
{
    if (dcol != 0)
    {
        // 0x00016D40 / 0x00016DA0 wrap inside the row, then 0x00017560 may repeat the step.
        for (int guard = 0; guard < 16; ++guard)
        {
            const int n = kb_row(g).count;
            g.kb_col = (g.kb_col + dcol + n) % n;
            if (!kb_blocked(g))
            {
                return;
            }
        }
        return;
    }
    // +0xF0 is 5. A blocked landing (Shift / Caps on the symbols page) repeats the move.
    for (int guard = 0; guard < 16; ++guard)
    {
        if (drow < 0)
        {
            kb_up_adjust(g);
            g.kb_row = g.kb_row == 0 ? kKbRows - 1 : g.kb_row - 1;
        }
        else
        {
            kb_down_adjust(g);
            g.kb_row = g.kb_row == kKbRows - 1 ? 0 : g.kb_row + 1;
        }
        if (!kb_blocked(g))
        {
            return;
        }
    }
}

char16_t kb_insert_char(const Game& g, u32 code)
{
    // Keys are stored as the uppercase / symbol code. Shift (cleared after one insert) or caps
    // keeps A-Z; otherwise letters go in lowercase. FUN_000176E0 does this case map.
    if (code >= 'A' && code <= 'Z' && !g.kb_shift && !g.kb_caps)
    {
        return char16_t(code + 32);
    }
    return char16_t(code);
}

void kb_activate(Game& g)
{
    const u32 code = kb_row(g).keys[g.kb_col].code;
    // 0x000152F0. Codes below 0x10000 (except the caret arrows) insert.
    if (code == 0x22B2)
    {
        if (g.kb_caret > 0)
        {
            --g.kb_caret;
        }
        return;
    }
    if (code == 0x22B3)
    {
        if (g.kb_caret < g.kb_len)
        {
            ++g.kb_caret;
        }
        return;
    }
    if (code < 0x10000)
    {
        if (g.kb_len >= 63)
        {
            return;
        }
        for (int i = g.kb_len; i > g.kb_caret; --i)
        {
            g.kb_text[i] = g.kb_text[i - 1];
        }
        g.kb_text[g.kb_caret] = kb_insert_char(g, code);
        ++g.kb_caret;
        ++g.kb_len;
        g.kb_text[g.kb_len] = 0;
        if (g.kb_shift)
        {
            g.kb_shift = false;
        }
        return;
    }
    if (code == 0x10000)
    {
        if (g.kb_caret <= 0)
        {
            return;
        }
        for (int i = g.kb_caret - 1; i < g.kb_len; ++i)
        {
            g.kb_text[i] = g.kb_text[i + 1];
        }
        --g.kb_caret;
        --g.kb_len;
        return;
    }
    if (code == 0x10002)
    {
        g.kb_shift = !g.kb_shift;
        return;
    }
    if (code == 0x10003)
    {
        g.kb_caps = !g.kb_caps;
        return;
    }
    // Page keys rewrite +0xBC. 0x10005 symbols, 0x10006 accents, 0x10004 alphabet.
    if (code == 0x10005)
    {
        g.kb_page = 1;
        kb_clamp_col(g);
        return;
    }
    if (code == 0x10006)
    {
        g.kb_page = 2;
        kb_clamp_col(g);
        return;
    }
    if (code == 0x10004)
    {
        g.kb_page = 0;
        kb_clamp_col(g);
        return;
    }
    if (code == 0x10007)
    {
        // FUN_000dd210. A nonempty name takes the 0x000DD270 path: mode 1 and status 4
        // (0x000DD6C0). An empty name does not occupy the slot.
        g.frontend_mode = 1;
        g.naming = false;
        if (g.kb_len > 0)
        {
            g.slot_name_len[g.slot] = g.kb_len;
            for (int i = 0; i < 64; ++i)
            {
                g.slot_name[g.slot][i] = g.kb_text[i];
            }
            g.slot_status[g.slot] = 4;
            std::cout << "slot " << g.slot << " named (" << g.kb_len << " chars)\n";
        }
    }
}

void gameselect_tick(Game& g, const InputState& in)
{
    if (g.frontend_mode == 0)
    {
        g.naming = false;
        // Title plays until A. That opens slot select on top of the same scene.
        if (in.accept && !g.slot_accept && g.scene_name.find("attract") != std::string::npos)
        {
            load_menu_textures(g);
            g.frontend_mode = 1;
            std::cout << "frontend mode 1 slot select\n";
        }
    }
    else if (g.frontend_mode == 4)
    {
        // 0x00015090. B cancels (codes 3 and 4 call 0x000DD550). D-pad moves. A activates.
        if (in.back && !g.slot_back)
        {
            g.frontend_mode = 1;
            g.naming = false;
        }
        else
        {
            if (in.left && !g.slot_left)
            {
                kb_move(g, -1, 0);
            }
            if (in.right && !g.slot_right)
            {
                kb_move(g, 1, 0);
            }
            if (in.up && !g.slot_up)
            {
                kb_move(g, 0, -1);
            }
            if (in.down && !g.slot_down)
            {
                kb_move(g, 0, 1);
            }
            if (in.accept && !g.slot_accept)
            {
                kb_activate(g);
            }
        }
    }
    else if (g.frontend_mode == 1)
    {
        // 0x000E0210: direction 8 wraps the index down, direction 4 wraps it up. Both end in state 6.
        if (in.down && !g.slot_down)
        {
            g.slot = (g.slot + 1) % 3;
        }
        if (in.up && !g.slot_up)
        {
            g.slot = (g.slot + 2) % 3;
        }
        // 0x000E02D0: empty slot sets mode 4. Status 2, 3, or 4 sets mode 3 (slot contents).
        if (in.accept && !g.slot_accept)
        {
            if (g.slot_status[g.slot] == 0)
            {
                kb_open(g);
            }
            else
            {
                g.frontend_mode = 3;
                g.contents_row = 0;
                g.naming = false;
                std::cout << "frontend mode 3 slot contents\n";
            }
        }
        // 0x000E05D0 copies aid_script_ghoulies_gameselect_backtoattract and asks for that script.
        // That bundle is not in this dump; the attract reel is scene_order[0], which is where the prompt returns.
        if (in.back && !g.slot_back && !g.scene_order.empty())
        {
            g.naming = false;
            load_scene_contents(g, g.scene_order[0]);
            cutscene_queue_start(g);
        }
    }
    else if (g.frontend_mode == 3)
    {
        // 0x000D9450. State 1 accept (0x000D94FD) pushes 6 into 0x000DA580, the play row.
        // B leaves the contents widget for slot select.
        if (in.down && !g.slot_down)
        {
            g.contents_row = (g.contents_row + 1) % 3;
        }
        if (in.up && !g.slot_up)
        {
            g.contents_row = (g.contents_row + 2) % 3;
        }
        if (in.accept && !g.slot_accept)
        {
            if (g.contents_row == 0)
            {
                g.frontend_mode = 6;
                std::cout << "frontend mode 6 play\n";
            }
            else if (g.contents_row == 1)
            {
                g.slot_status[g.slot] = 0;
                g.slot_name_len[g.slot] = 0;
                g.frontend_mode = 1;
                std::cout << "slot " << g.slot << " deleted\n";
            }
        }
        if (in.back && !g.slot_back)
        {
            g.frontend_mode = 1;
        }
    }
    else if (g.frontend_mode == 6)
    {
        // Play row (0x000D6410 / 0x000D67F0). A starts the saved game at chapter 1.
        // B returns to the contents rows.
        if (in.back && !g.slot_back)
        {
            g.frontend_mode = 3;
        }
        else if (in.accept && !g.slot_accept && g.scene_order.size() > 3)
        {
            std::cout << "play\n";
            g.frontend_mode = 0;
            g.naming = false;
            load_scene_contents(g, g.scene_order[3]);
            cutscene_queue_start(g);
        }
    }
    g.slot_down = in.down;
    g.slot_up = in.up;
    g.slot_left = in.left;
    g.slot_right = in.right;
    g.slot_accept = in.accept;
    g.slot_back = in.back;
}

// One row of the slot table in .data. Names start at VA 0x429048, stride 0x124.
// FUN_000de7b0 (t = 0) writes center (x, y) into the widget at +0x428 / +0x42C.
// FUN_000df9a0 case 6 and FUN_000de540 write the box at +0x510 / +0x514:
//   rest      = norm * {640,480} + DAT_00428FBC (12)
//   highlight = 0.25 * {640,480} + DAT_00428FC0 (20)
// The draw at 0x000DE070 places the quad at center - box/2. Scale * texture size is norm * {640,480}.
struct SlotPlate
{
    const char* texture;
    float center_x;
    float center_y;
    float rest_x;
    float rest_y;
    float hot_x;
    float hot_y;
};

constexpr float kFrameW = 640.f;
constexpr float kFrameH = 480.f;
constexpr float kRestBias = 12.f;
constexpr float kHotBias = 20.f;

constexpr SlotPlate kSlots[3] = {
    {"gameselect_oneframe", 130.f, 200.f, 0.16f, 0.16f, 0.25f, 0.25f},
    {"gameselect_twoframe", 320.f, 200.f, 0.16f, 0.16f, 0.25f, 0.25f},
    {"gameselect_threeframe", 510.f, 200.f, 0.16f, 0.16f, 0.25f, 0.25f},
};

void draw_plate(const Game& g, const GfxRect& board, int index, bool selected)
{
    const SlotPlate& slot = kSlots[index];
    // 0x000DEB20. Empty uses the slot's oneframe/twoframe/threeframe.
    // A named slot (status 4) uses the chapter table at 0x0042939C. A new game is chapter 1.
    const char* texture = g.slot_status[index] == 4 ? "gameselect_chapter1" : slot.texture;
    const float nx = selected ? slot.hot_x : slot.rest_x;
    const float ny = selected ? slot.hot_y : slot.rest_y;
    const float bias = selected ? kHotBias : kRestBias;
    const int box_w = int(nx * kFrameW + bias);
    const int box_h = int(ny * kFrameH + bias);
    const float left = slot.center_x - float(box_w / 2);
    const float top = slot.center_y - float(box_h / 2);
    draw_sprite(find_ui(g, texture), ui_place(board, left, top, nx * kFrameW, ny * kFrameH));
    if (!selected)
    {
        return;
    }
    // FUN_001439c0 hangs bigarrowup / bigarrowdown on the selected slot widget.
    const Game::UiImage* up = find_ui(g, "gameselect_bigarrowup");
    const Game::UiImage* down = find_ui(g, "gameselect_bigarrowdown");
    const float aw = up ? float(up->image.width) : 128.f;
    const float ah = up ? float(up->image.height) : 32.f;
    const float ax = slot.center_x - aw * 0.5f;
    draw_sprite(up, ui_place(board, ax, top - ah, aw, ah));
    const float dh = down ? float(down->image.height) : ah;
    draw_sprite(down, ui_place(board, ax, top + ny * kFrameH, aw, dh));
}

std::u16string key_label(const Game& g, u32 code)
{
    // FUN_00011150 writes the loctext key from the table at 0x338D78.
    // 0x10004 +0x28 Alphabet, 0x10005 +0x2C Symbols, 0x10006 +0x30 Accents, 0x10007 +0x34 Done.
    // Letters follow FUN_000176E0 (shift / caps).
    switch (code)
    {
    case 0x10007:
        return u"DONE";
    case 0x10006:
        return u"ACCENTS";
    case 0x10005:
        return u"SYMBOLS";
    case 0x10004:
        return u"ALPHABET";
    case 0x10003:
        return u"CAPS LOCK";
    case 0x10002:
        return u"SHIFT";
    case 0x10000:
        return u"BACKSPACE";
    case 0x20:
        return u"SPACE";
    case 0x22B2:
    case 0x22B3:
        return {};
    default:
        return std::u16string(1, kb_insert_char(g, code));
    }
}

const GloopyFont& gloopy_font(const Game& g)
{
    static GloopyFont font;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        const auto path = g.aids.game_dir / "font" / "069563d4";
        if (!font.load(path))
        {
            std::cerr << "gloopy font failed " << path.string() << "\n";
        }
    }
    return font;
}

void draw_glyphs(const GfxRect& board, const GloopyFont& font, std::u16string_view text, float pen_x, float pen_y,
                 float scale, int frame)
{
    // FUN_00051760 advances by x1-x0. FUN_000608d0 shifts the full cell left by x0
    // so texel x0 sits on the pen. The cell is the texture (32), not the ink box.
    // FUN_0005CA60 builds the quad around the center, and the half-cell bias in
    // FUN_0005E9F0 cancels that, leaving the top-left at (pen - x0*scale, pen_y).
    if (font.cell() <= 0 || scale == 0.f)
    {
        return;
    }
    const float em = float(font.cell()) * scale;
    float x = pen_x;
    for (char16_t ch : text)
    {
        if (const RgbaImage* glyph = font.image(ch, frame))
        {
            const float bx = float(font.bearing(ch)) * scale;
            gfx_draw_sprite(*glyph, ui_place(board, x - bx, pen_y, em, em));
        }
        x += float(font.advance(ch)) * scale;
    }
}

void draw_keyboard(const Game& g, const GfxRect& board)
{
    if (!g.naming)
    {
        return;
    }
    const GloopyFont& font = gloopy_font(g);
    const Game::UiImage* caret = find_ui(g, "virtkb_caret");
    const Game::UiImage* left = find_ui(g, "virtkb_arrowleft");
    const Game::UiImage* right = find_ui(g, "virtkb_arrowright");
    for (int r = 0; r < kKbRows; ++r)
    {
        const KbRow& row = kKb[g.kb_page][r];
        float x = 0.f;
        for (int c = 0; c < row.count; ++c)
        {
            const float w = float(row.keys[c].width);
            const float left_px = 64.f + x;
            const float top = 135.f + float(r) * 42.f;
            const u32 code = row.keys[c].code;
            const bool hot = r == g.kb_row && c == g.kb_col;
            if (code == 0x22B2 && left)
            {
                draw_sprite(left, ui_place(board, left_px, top, float(left->image.width), float(left->image.height)));
            }
            else if (code == 0x22B3 && right)
            {
                draw_sprite(right, ui_place(board, left_px, top, float(right->image.width), float(right->image.height)));
            }
            else
            {
                // FUN_00016EF0 submits every label with eax=1. FUN_00060070 then
                // settles on tile_count-1, and FUN_0005CA60 pins that index into
                // texture+0x13 so FUN_00053550's clock stays off. The key at
                // +0xC0/+0xC4 is left on the clock (frame -1, byte +0x13 stays 0xFF).
                const int frame = hot ? font.frame(g.time) : font.rest_frame();
                // FUN_00016EF0 submits sx = sy = 0.65 (DAT_0032B1EC).
                draw_glyphs(board, font, key_label(g, code), left_px, top, 0.65f, frame);
            }
            if (hot && caret)
            {
                const float cw = std::min(w, float(caret->image.width));
                const float ch = 8.f;
                draw_sprite(caret, ui_place(board, left_px + (w - cw) * 0.5f, top + 42.f - ch, cw, ch));
            }
            x += w;
            if (c == 0)
            {
                x += 4.f;
            }
        }
    }
    // 0x00015A10 submits the name with eax=1, so FUN_00060070 holds the last frame.
    // Pen is (130, 54), scale 1.45 (DAT_0032B538). 0x000174D0 sits the caret on that string.
    float name_x = 130.f;
    const int frame = font.rest_frame();
    const float name_scale = 1.45f;
    const float name_em = float(font.cell()) * name_scale;
    for (int i = 0; i < g.kb_len; ++i)
    {
        if (i == g.kb_caret && caret)
        {
            draw_sprite(caret, ui_place(board, name_x, 54.f, float(caret->image.width) * 0.45f, name_em));
        }
        if (const RgbaImage* glyph = font.image(g.kb_text[i], frame))
        {
            const float bx = float(font.bearing(g.kb_text[i])) * name_scale;
            gfx_draw_sprite(*glyph, ui_place(board, name_x - bx, 54.f, name_em, name_em));
        }
        name_x += float(font.advance(g.kb_text[i])) * name_scale;
    }
    if (g.kb_caret >= g.kb_len && caret)
    {
        draw_sprite(caret, ui_place(board, name_x, 54.f, float(caret->image.width) * 0.45f, name_em));
    }
}

void draw_contents(const Game& g, const GfxRect& board)
{
    // Chapter plate stays on the slot center from 0x00429048. The accept row is Play
    // (mode 6). Delete and Game Options are the other contents actions.
    draw_sprite(find_ui(g, "gameselect_emptyslotbackground"), board);
    draw_plate(g, board, g.slot, true);
    const GloopyFont& font = gloopy_font(g);
    const int frame = font.rest_frame();
    const float scale = 0.65f;
    static constexpr char16_t const* kRows[3] = {u"Play", u"Delete", u"GAME OPTIONS"};
    const int rows = g.frontend_mode == 6 ? 1 : 3;
    const float top = 300.f;
    const float step = 36.f;
    const float pen_x = 48.f;
    for (int i = 0; i < rows; ++i)
    {
        const bool hot = g.frontend_mode == 6 || i == g.contents_row;
        const int glyph = hot ? font.frame(g.time) : frame;
        const float y = top + float(i) * step;
        if (hot)
        {
            const Game::UiImage* arrow = find_ui(g, "gameselect_bigarrowup");
            const float ah = arrow ? float(arrow->image.height) : 32.f;
            draw_sprite(arrow, ui_place(board, pen_x - 36.f, y, ah, ah * 0.5f));
        }
        draw_glyphs(board, font, kRows[i], pen_x, y, scale, glyph);
    }
    if (g.slot_name_len[g.slot] > 0)
    {
        draw_glyphs(board, font, std::u16string_view(g.slot_name[g.slot], size_t(g.slot_name_len[g.slot])), pen_x,
                    48.f, scale, frame);
    }
}

void draw_ui(const Game& g)
{
    if (g.frontend_mode == 0)
    {
        return;
    }
    const GfxRect board = ui_letterbox();
    if (g.frontend_mode == 3 || g.frontend_mode == 6)
    {
        draw_contents(g, board);
        return;
    }
    // FUN_000df140: an empty slot's backdrop is emptyslotbackground, scaled to the 640x480 frame.
    draw_sprite(find_ui(g, "gameselect_emptyslotbackground"), board);
    for (int i = 0; i < 3; ++i)
    {
        if (i != g.slot)
        {
            draw_plate(g, board, i, false);
        }
    }
    draw_plate(g, board, g.slot, true);
    draw_keyboard(g, board);
}

void sync_cutscene_props(Game& g)
{
    const auto& q = g.cutscenes;
    if (!q.playing || q.items.empty() || !g.scene_bundle)
    {
        g.cutscene_props.clear();
        g.cutscene_props_index = static_cast<size_t>(-1);
        return;
    }
    if (g.cutscene_props_index == q.index)
    {
        return;
    }
    g.cutscene_props_index = q.index;
    g.cutscene_props.clear();
    const Cutscene& cut = q.items[q.index];
    std::cout << "title actors " << cut.actors.size() << "\n";
    for (size_t i = 0; i < cut.actors.size(); ++i)
    {
        const BundleEntry* entry = find_aid(g, cut.actors[i].model);
        if (!entry || entry->type != ResourceType::Model)
        {
            std::cout << "  actor missing " << cut.actors[i].model << "\n";
            continue;
        }
        try
        {
            Game::CutsceneProp prop;
            prop.mesh = parse_model(*entry);
            prop.actor = i;
            u16 max_bone = 0;
            for (u16 b : prop.mesh.bones)
            {
                max_bone = std::max(max_bone, b);
            }
            std::cout << "  " << cut.actors[i].model << " mina " << cut.actors[i].mina.size() << " joints "
                      << prop.mesh.joints.size() << " skinned " << prop.mesh.bones.size() << " maxbone " << max_bone
                      << "\n";
            g.cutscene_props.push_back(std::move(prop));
        }
        catch (const std::exception& ex)
        {
            std::cout << "  actor fail " << cut.actors[i].model << ": " << ex.what() << "\n";
        }
    }
}

float actor_curve(const CutsceneActor& actor, size_t index, float time, float duration, float fallback)
{
    if (index >= actor.curves.size() || actor.curves[index].keys.empty())
    {
        return fallback;
    }
    return sample_curve(actor.curves[index], time, duration);
}

void draw_cutscene_props(Game& g)
{
    sync_cutscene_props(g);
    const auto& q = g.cutscenes;
    if (!q.playing || q.index >= q.items.size())
    {
        return;
    }
    const Cutscene& cut = q.items[q.index];
    for (const auto& prop : g.cutscene_props)
    {
        if (prop.actor >= cut.actors.size())
        {
            continue;
        }
        const CutsceneActor& actor = cut.actors[prop.actor];
        const Vec3 origin{actor_curve(actor, 0, q.time, cut.duration, 0.f),
                          actor_curve(actor, 1, q.time, cut.duration, 0.f),
                          actor_curve(actor, 2, q.time, cut.duration, 0.f)};
        const Vec3 scale{actor_curve(actor, 3, q.time, cut.duration, 1.f),
                         actor_curve(actor, 4, q.time, cut.duration, 1.f),
                         actor_curve(actor, 5, q.time, cut.duration, 1.f)};
        if (!actor.mina.empty() && !prop.mesh.joints.empty() && !prop.mesh.bones.empty() && cut.duration > 0.f)
        {
            const auto pose = sample_mina(actor.mina, q.time / cut.duration);
            const auto bind = sample_mina(actor.mina, 0.f);
            const int n = int(std::min(pose.size(), std::min(prop.mesh.joints.size(), size_t(128))));
            float bones[128 * 16]{};
            if (n > 0)
            {
                skin_matrices(prop.mesh.joints, pose, bones, n, bind.size() >= size_t(n) ? &bind : nullptr);
                gfx_set_bones(bones, n);
            }
            else
            {
                gfx_set_bones(nullptr, 0);
            }
        }
        else
        {
            gfx_set_bones(nullptr, 0);
        }
        gfx_draw_mesh(prop.mesh, origin, scale);
    }
    gfx_set_bones(nullptr, 0);
}

} // namespace

void script_set_default_name(Game& g)
{
    // Xbox 0x001287F0: copy default into DAT_00512494 and zero script globals.
    g.scene_name = kNoScript;
    g.scene_script = {};
    g.script_aidlists.clear();
    g.cutscenes = {};
    g.models.clear();
    g.cutscene_props.clear();
    g.cutscene_props_index = static_cast<size_t>(-1);
    g.ui.clear();
}

void script_set_name(Game& g, std::string_view name)
{
    // Xbox 0x00128AB0: EAX = C string, copies into DAT_00512494.
    g.scene_name = std::string(name);
}

void cutscene_queue_start(Game& g)
{
    auto& q = g.cutscenes;
    if (q.items.empty())
    {
        return;
    }
    q.index = 0;
    q.time = 0.f;
    q.playing = true;
    log_cutscene_begin(g);
}

void cutscene_queue_tick(Game& g, float dt)
{
    auto& q = g.cutscenes;
    if (!q.playing || q.items.empty())
    {
        return;
    }
    q.time += dt;
    int guard = 0;
    while (q.playing && q.items[q.index].duration > 0.f && q.time >= q.items[q.index].duration && guard < 64)
    {
        ++guard;
        q.time -= q.items[q.index].duration;
        log_cutscene_end(g);
        const size_t next = q.index + 1;
        if (next >= q.items.size())
        {
            if (!q.loop)
            {
                q.playing = false;
                q.time = 0.f;
                return;
            }
            std::cout << "cutscene reel loop\n";
            q.index = 0;
        }
        else
        {
            q.index = next;
        }
        log_cutscene_begin(g);
    }
    if (g.camera && g.camera->actor)
    {
        Vec3 pos;
        Quat orient;
        float fov = 0.9f;
        if (sample_cutscene_camera(q.items[q.index], q.time, pos, orient, fov))
        {
            g.camera->actor->position = pos;
            g.camera->actor->quat = orient;
            g.camera->actor->fov = fov;
        }
    }
}

void Game::init(const std::filesystem::path& game_dir, std::string_view scene_select)
{
    // Xbox: _cinit runs entity type static ctors, then WinMain 0x00032390:
    //   gfx_init_32470, fs_init_33980, game_init_32630.
    register_entity_types();
    clock_init(clock);
    aids.open(game_dir);
    std::cout << "fs_init: " << aids.game_dir.string() << " common.bnl entries " << aids.common.entries.size() << "\n";
    std::cout << "clock " << clock_fps(clock) << " Hz dt=" << clock.dt << "\n";
    std::cout << "entity types " << all_entity_types().size() << "\n";

    script_set_default_name(*this);
    if (const BundleEntry* list = find_entry(aids.common, kSceneOrder))
    {
        scene_order = parse_aidlist(list->cpu);
        std::cout << kSceneOrder << " (" << scene_order.size() << ")\n";
        for (size_t i = 0; i < scene_order.size() && i < 12; ++i)
        {
            std::cout << "  [" << i << "] " << scene_order[i] << "\n";
        }
        if (scene_order.size() > 12)
        {
            std::cout << "  ...\n";
        }
        if (!scene_order.empty())
        {
            size_t index = 0;
            if (!scene_select.empty())
            {
                bool digits = true;
                for (char c : scene_select)
                {
                    if (c < '0' || c > '9')
                    {
                        digits = false;
                        break;
                    }
                }
                if (digits)
                {
                    index = std::min(scene_order.size() - 1, size_t(std::stoul(std::string(scene_select))));
                }
                else
                {
                    for (size_t i = 0; i < scene_order.size(); ++i)
                    {
                        if (scene_order[i].find(scene_select) != std::string::npos)
                        {
                            index = i;
                            break;
                        }
                    }
                }
                std::cout << "scene select [" << index << "] " << scene_order[index] << "\n";
            }
            load_scene_contents(*this, scene_order[index]);
        }
    }

    if (scene_name == kNoScript)
    {
        std::cout << kNoScript << "\n";
    }

    actors.reserve(16);
    entities.reserve(16);
    actors.emplace_back();
    camera = spawn(*this, "entityBodyCamera", actors.back());

    game_init_pump();
    std::cout << "game_init: io idle\n";
    cutscene_queue_start(*this);
}

void frame_tick(Game& g, float dt, const InputState& in)
{
    // Xbox 0x00033B50.
    clock_pad(g.clock, dt);    // 0x000FC030
    clock_frame_time(g.clock); // 0x00033C80
    g.time = g.clock.time;
    g.frame = g.clock.frame;
    if (in.quit)
    {
        g.running = false;
        return;
    }
    // 0x000FA3C0 / 0x000FAD80 pad process. Game-select reads up/down from that pad state.
    gameselect_tick(g, in);
    // 0x0015B780 audio(dt), 0x0003A040 listener, 0x000396F0 / 0x00038640 / 0x00038C20 XACT.
    cutscene_queue_tick(g, g.clock.dt);
    for (auto& e : g.entities)
    {
        if (e.type && e.type->tick)
        {
            e.type->tick(e);
        }
    }
    file_pump(); // 0x000E9E60
    // Present 0x00033480 / 0x00032AB0 / 0x00033A90. The PC backend draws the scene models
    // with the body camera; Xbox D3D8 would issue the original calls here.
    if (g.camera)
    {
        gfx_set_camera(g.camera->pose_position, g.camera->pose_quat, g.camera->pose_fov);
    }
    for (const Mesh& mesh : g.models)
    {
        gfx_draw_mesh(mesh);
    }
    draw_cutscene_props(g);
    draw_ui(g);
}

void Game::tick(float dt, const InputState& in)
{
    frame_tick(*this, dt, in);
}

} // namespace gbtg
