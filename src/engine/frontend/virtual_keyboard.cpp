#include "virtual_keyboard.h"
#include "dialogue_draw_context.h"
#include "dialogue_draw_outer.h"
#include "dialogue_input.h"
#include "dialogue_text_metrics.h"
#include "dialogue_text_pool.h"
#include "dialogue_widget_construction.h"
#include "dialogue_activation.h"
#include "dialogue_widget_state.h"
#include "gameselect_naming.h"
#include "game_names.h"
#include "hud_layout.h"
#include "../audio/event_voices.h"
#include "../cutscenes/event_runtime.h"
#include "../entities/actor_render_registration.h"
#include "../scene/attract_control.h"
#include "../scene/executable_resources.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
u32 g_key_labels = 0; // PTR_PTR_00338DCC: the XBE address of the language's key-name table.

using Text = std::array<char16_t, 0x800>; // The callers' 0x1000-byte stack buffers.

[[noreturn]] void untranslated(const char* what)
{
    throw Error(std::string(what) + " is not translated");
}

u32 exe_word(const GameSelectControlServices& s, u32 address)
{
    return read_u32(s.image.memory(address, 4), 0);
}

float frame_step(const GameSelectControlServices& s)
{
    if (s.freeze <= 0.f)
    {
        return s.dt * s.scene_scale;
    }
    return 0.f;
}

size_t wide_length(const char16_t* text) // lstrlenW
{
    return std::u16string_view(text).size();
}

void copy_wide(char16_t* output, const char16_t* input) // lstrcpyW
{
    while ((*output++ = *input++) != 0)
    {
    }
}

float text_width(const char16_t* text, float scale, GameSelectControlServices& s)
{
    return runtime_text_line_width(s.metrics, 0, text, scale); // 0x00060710, EAX 0, ECX the text, XMM6 the scale
}

float cell_height(const GameSelectControlServices& s)
{
    return float(s.metrics.fonts[0]->cell); // DAT_00504444 +0x10
}

// The unsigned key width as the x87 load (FILD, then +2^32 for a negative word) adds it.
float key_width(const VirtualKeyboardKey& key)
{
    return float(double(key.width));
}

VirtualKeyboardBoard& current_board(VirtualKeyboard& k)
{
    return k.boards.at(size_t(k.board));
}

VirtualKeyboardKey& selected_key(VirtualKeyboard& k)
{
    return current_board(k).at(size_t(k.row)).at(size_t(k.column));
}

RuntimeDialogueInputDevice attract_device()
{
    return {g_attract_input.device, {nullptr, nullptr, nullptr, nullptr}}; // DAT_005461A0
}

u32 edge(u16 mask, GameSelectControlServices& s)
{
    return runtime_input_button_edge(attract_device(), mask, s.attract.input_state); // 0x001083F0, EAX DAT_005461A0
}

std::array<float, 2> axes(GameSelectControlServices& s)
{
    std::array<float, 2> output{};
    runtime_input_axes(attract_device(), output, s.attract.input_state); // 0x001085F0, EDI the output
    return output;
}

void draw_sprite(std::span<const u8> texture, const float* scale, u32 color, const float* position,
    GameSelectControlServices& s)
{
    auto& draw = s.draw;
    begin_dialogue_sprite_draw(draw); // 0x0005CDC0
    draw_dialogue_texture(0, nullptr, const_cast<u8*>(texture.data()), scale, nullptr, 0.f, color, position,
        draw); // 0x0005CA60
    --draw.override_count;
    if (draw.override_count == 0)
    {
        draw.fog_disabled = 0;
    }
}

// Xbox 0x00011150, EAX the key, ECX the code, EDX the width.
VirtualKeyboardKey make_key(u32 code, u32 width)
{
    VirtualKeyboardKey key{code, width, 0, 0};
    const auto label = [](u32 offset) -> u32
    {
        return g_key_labels == 0 ? 0x0020A4F4u : offset;
    };
    switch (code)
    {
    case 0x20:
        key.label = label(0x18);
        break;
    case 0x10000:
        key.label = label(0x1C);
        break;
    case 0x10002:
        key.label = label(0x20);
        break;
    case 0x10003:
        key.label = label(0x24);
        break;
    case 0x10004:
        key.label = label(0x28);
        break;
    case 0x10005:
        key.label = label(0x2C);
        break;
    case 0x10006:
        key.label = label(0x30);
        break;
    case 0x10007:
        key.label = label(0x34);
        break;
    case 0x10008:
        key.label = 0x0020A668;
        break;
    case 0x10009:
        key.label = 0x0020A65C;
        break;
    case 0x1000A:
        key.label = 0x0020A64C;
        break;
    default:
        break;
    }
    return key;
}

// make_key's label words are table offsets until the table is read: the label is the table's entry.
VirtualKeyboardKey made_key(u32 code, u32 width, GameSelectControlServices& s)
{
    VirtualKeyboardKey key = make_key(code, width);
    if (g_key_labels != 0 && key.label != 0 && key.label < 0x40)
    {
        key.label = exe_word(s, g_key_labels + key.label);
    }
    return key;
}

// 0x00011870's boards for every layout but 1 and 2: letters, symbols, accents. Each row starts with a function key.
struct KeySpec
{
    u32 code;
    u32 width;
};
const std::vector<std::vector<std::vector<KeySpec>>>& default_boards()
{
    static const std::vector<std::vector<std::vector<KeySpec>>> boards{
        {
            {{0x10007, 0xA6}, {0x31, 0x22}, {0x32, 0x22}, {0x33, 0x22}, {0x34, 0x22}, {0x35, 0x22}, {0x36, 0x22},
                {0x37, 0x22}, {0x38, 0x22}, {0x39, 0x22}, {0x30, 0x22}},
            {{0x10002, 0xA6}, {0x41, 0x22}, {0x42, 0x22}, {0x43, 0x22}, {0x44, 0x22}, {0x45, 0x22}, {0x46, 0x22},
                {0x47, 0x22}, {0x48, 0x22}, {0x49, 0x22}, {0x4A, 0x22}},
            {{0x10003, 0xA6}, {0x4B, 0x22}, {0x4C, 0x22}, {0x4D, 0x22}, {0x4E, 0x22}, {0x4F, 0x22}, {0x50, 0x22},
                {0x51, 0x22}, {0x52, 0x22}, {0x53, 0x22}, {0x54, 0x22}},
            {{0x10005, 0xA6}, {0x55, 0x22}, {0x56, 0x22}, {0x57, 0x22}, {0x58, 0x22}, {0x59, 0x22}, {0x5A, 0x22},
                {0x10000, 0x88}},
            {{0x10006, 0xA6}, {0x20, 0xCC}, {0x22B2, 0x44}, {0x22B3, 0x44}},
        },
        {
            {{0x10007, 0xA6}, {0x28, 0x22}, {0x29, 0x22}, {0x25, 0x22}, {0x5C, 0x22}, {0x2F, 0x22}},
            {{0x10002, 0xA6}, {0x24, 0x22}, {0xA3, 0x22}, {0xA5, 0x22}, {0x3A, 0x22}, {0x27, 0x22}, {0x22, 0x22}},
            {{0x10003, 0xA6}, {0x3F, 0x22}, {0x21, 0x22}, {0xBF, 0x22}, {0xA1, 0x22}, {0x2D, 0x22}, {0x26, 0x22}},
            {{0x10004, 0xA6}, {0x7B, 0x22}, {0x7D, 0x22}, {0xAB, 0x22}, {0xBB, 0x22}, {0x2C, 0x22}, {0x2E, 0x22},
                {0x10000, 0x88}},
            {{0x10006, 0xA6}, {0x20, 0xCC}, {0x22B2, 0x44}, {0x22B3, 0x44}},
        },
        {
            {{0x10007, 0xA6}, {0x31, 0x22}, {0x32, 0x22}, {0x33, 0x22}, {0x34, 0x22}, {0x35, 0x22}, {0x36, 0x22},
                {0x37, 0x22}, {0x38, 0x22}, {0x39, 0x22}, {0x30, 0x22}},
            {{0x10002, 0xA6}, {0xC0, 0x22}, {0xC1, 0x22}, {0xC2, 0x22}, {0xC4, 0x22}, {0xC3, 0x22}, {0xC5, 0x22},
                {0xC6, 0x22}, {0xC7, 0x22}, {0xC8, 0x22}, {0xC9, 0x22}, {0xCA, 0x22}},
            {{0x10003, 0xA6}, {0xCB, 0x22}, {0xCC, 0x22}, {0xCD, 0x22}, {0xCE, 0x22}, {0xCF, 0x22}, {0xD1, 0x22},
                {0xD2, 0x22}, {0xD3, 0x22}, {0xD4, 0x22}, {0xD5, 0x22}, {0xD8, 0x22}},
            {{0x10004, 0xA6}, {0xD6, 0x22}, {0xDF, 0x22}, {0xD9, 0x22}, {0xDA, 0x22}, {0xDB, 0x22}, {0xDC, 0x22},
                {0x10000, 0x88}},
            {{0x10006, 0xA6}, {0x20, 0xCC}, {0x22B2, 0x44}, {0x22B3, 0x44}},
        },
    };
    return boards;
}

// Xbox 0x000176E0, EAX the code, EDX the keyboard: the character a letter key types under caps lock and shift.
u32 key_character(u32 code, const VirtualKeyboard& k)
{
    if (k.language == 2)
    {
        return code;
    }
    const u16 c = u16(code);
    bool lower;
    if (k.caps != 0)
    {
        lower = k.shift != 0;
    }
    else
    {
        lower = k.shift == 0;
    }
    if (lower)
    {
        if ((c < 0x41 || 0x5A < c) && (c < 0xC0 || 0xDD < c))
        {
            return c != 0x178 ? code : 0xFF;
        }
        return code + 0x20;
    }
    if ((c < 0x61 || 0x7A < c) && (c < 0xE0 || 0xFD < c))
    {
        return c != 0xFF ? code : 0x178;
    }
    return code - 0x20;
}

// Xbox 0x00016360, EAX the key, EDX the keyboard: the key's label, a localized key name or its character.
void key_label(const VirtualKeyboardKey& key, const VirtualKeyboard& k, char16_t* output,
    GameSelectControlServices& s)
{
    const auto* label = key.label == 0 ? nullptr
                                       : reinterpret_cast<const char16_t*>(s.image.memory(key.label, 2).data());
    if (label == nullptr || label[0] == 0)
    {
        output[0] = char16_t(key_character(key.code, k));
        output[1] = 0;
        return;
    }
    // IsBadReadPtr(DAT_005462C4, 1); the result is not used.
    const size_t length = wide_length(label);
    std::array<char, 0x100> narrow{};
    for (size_t index = 0; index < length; ++index)
    {
        narrow[index] = char(label[index]);
    }
    narrow[length] = 0;
    lookup_borrowed_localtext(s.text, "gameselect", narrow.data(), output, s.localtext); // 0x000FF120
}

// Xbox 0x00017560, ECX the keyboard: whether the selected key is skipped by the cursor.
bool key_skipped(VirtualKeyboard& k)
{
    const u32 code = selected_key(k).code;
    if (k.language == 2)
    {
        return code == 0;
    }
    if (k.board == 1 && (code == 0x10002 || code == 0x10003))
    {
        return true;
    }
    if (k.language == 7 && code == 0x10006)
    {
        return true;
    }
    if (k.board != 2)
    {
        return false;
    }
    if (k.shift != 0)
    {
        if (k.caps != 0 && k.shift != 0)
        {
            return false;
        }
    }
    else if (k.caps == 0)
    {
        return false;
    }
    return code == 0xDF;
}

// Xbox 0x00015010, ECX the keyboard, ESI the output: the caret's position after the cursor.
void caret_position(const VirtualKeyboard& k, float* output, GameSelectControlServices& s)
{
    float x = 130.f; // DAT_0032B53C.
    if (k.cursor != 0)
    {
        Text prefix{};
        std::copy(k.name.begin(), k.name.begin() + k.cursor, prefix.begin()); // lstrcpynW(.., cursor + 1)
        prefix[size_t(k.cursor)] = 0;
        x = text_width(prefix.data(), 1.45f, s) + 130.f; // DAT_0032B538, DAT_0032B53C.
    }
    output[0] = x;
    output[1] = cell_height(s) * 1.45f * 0.5f + 54.f; // DAT_0032B538, DAT_0032B0F4, DAT_0032B4C0.
}

// Xbox 0x00016900, EDI the keyboard: the caret bat flies to the caret.
void move_caret_bat(VirtualKeyboard& k, GameSelectControlServices& s)
{
    if (k.caret_bat != nullptr)
    {
        float target[2];
        caret_position(k, target, s); // 0x00015010, ESI the target
        s.fly_bat(*k.caret_bat, ButtonBatPoint{target[0], target[1]}, 1.25f); // 0x0006DA00
    }
}

// Xbox 0x00015A10, ESI the keyboard: the name's text record, made again.
void refresh_name_text(VirtualKeyboard& k, GameSelectControlServices& s)
{
    auto* allocation = k.pools[2]->allocation;
    allocation->cursor = allocation->begin;
    k.name_text = create_dialogue_text_record(*k.pools[2], k.name.data(), 130.f, 54.f, 1.45f, 1.45f, 1.f, 0, 0, 1,
        s.metrics, s.viewport_width, s.next_text_id, s.random); // 0x0005F5C0, EAX 1
}

// Xbox 0x00016EF0, ECX the keyboard, EDI the key: the key's label as a text record at (64 + x, y).
void make_key_text(VirtualKeyboard& k, VirtualKeyboardKey& key, float left, float y, float x,
    GameSelectControlServices& s)
{
    bool shown = true;
    Text label{};
    key_label(key, k, label.data(), s);
    const u32 code = key.code;
    if (code < 0x22B4)
    {
        if (0x22B1 < code)
        {
            return;
        }
        if (code != 0xDF)
        {
            goto make;
        }
    }
    else
    {
        if (code != 0x10006)
        {
            goto make;
        }
        if (k.language == 7)
        {
            shown = false;
        }
    }
    if (k.board == 2)
    {
        if (k.caps != 0)
        {
            if (k.shift == 0)
            {
                return;
            }
            if (k.caps != 0)
            {
                goto check;
            }
        }
        if (k.shift != 0)
        {
            return;
        }
    }
check:
    if (!shown)
    {
        return;
    }
make:
    key.text = create_dialogue_text_record(*k.pools[0], label.data(), left + x, y, 0.65f, 0.65f, 1.f, 0, 0, 1,
        s.metrics, s.viewport_width, s.next_text_id, s.random); // 0x0005F5C0, EAX 1
}

// The key walks of 0x00015C40 and its filtered copies: each row of the board from DAT_0032B570 down by +0xEC,
// each key from x 0 across by its width (the first key also by DAT_0032B1B0).
template <typename Visit>
void walk_keys(VirtualKeyboard& k, GameSelectControlServices& s, Visit visit)
{
    if (k.board == 4 || k.board == 3)
    {
        return;
    }
    float y = 135.f; // DAT_0032B570.
    for (u32 row = 0; row < k.rows; ++row)
    {
        auto& keys = current_board(k).at(row);
        float x = 0.f;
        i32 column = 0;
        for (auto& key : keys)
        {
            visit(key, row, column, x, y);
            x = key_width(key) + x;
            if (column == 0)
            {
                x = x + 4.f; // DAT_0032B1B0.
            }
            ++column;
        }
        y = k.row_spacing + y;
    }
}

template <typename Accept>
void make_key_texts(VirtualKeyboard& k, GameSelectControlServices& s, Accept accept)
{
    walk_keys(k, s, [&](VirtualKeyboardKey& key, u32, i32, float x, float y)
    {
        if (accept(key.code))
        {
            make_key_text(k, key, 64.f, y, x, s); // 0x00016EF0
        }
    });
}

// The label walks over the board's rows (+0xF0) without positions.
template <typename Visit>
void each_key(VirtualKeyboard& k, Visit visit)
{
    if (k.board == 4 || k.board == 3)
    {
        return;
    }
    for (u32 row = 0; row < k.rows; ++row)
    {
        for (auto& key : current_board(k).at(row))
        {
            visit(key);
        }
    }
}

// Xbox 0x0005FB10: the pool's record with this id is removed.
void remove_key_text(RuntimeDialogueTextPool& pool, u32 id)
{
    remove_dialogue_text_record(pool, id); // 0x0005FB10
}

// Xbox 0x00015C40
void make_all_key_texts(VirtualKeyboard& k, GameSelectControlServices& s)
{
    make_key_texts(k, s, [](u32)
    {
        return true;
    });
}

// Xbox 0x00015D60: the letters' labels.
void make_letter_texts(VirtualKeyboard& k, GameSelectControlServices& s)
{
    make_key_texts(k, s, [](u32 code)
    {
        return 0x40 < code && code < 0x100;
    });
}

// Xbox 0x00015E90: the digits' labels.
void make_digit_texts(VirtualKeyboard& k, GameSelectControlServices& s)
{
    make_key_texts(k, s, [](u32 code)
    {
        return 0x2F < code && code < 0x3A;
    });
}

// Xbox 0x00015FC0: the symbols' labels.
void make_symbol_texts(VirtualKeyboard& k, GameSelectControlServices& s)
{
    make_key_texts(k, s, [](u32 code)
    {
        return (0x20 < code && code < 0x7F) || (0xA0 < code && code < 0x20AD);
    });
}

// Xbox 0x000160F0: the mode keys' labels.
void make_mode_texts(VirtualKeyboard& k, GameSelectControlServices& s)
{
    make_key_texts(k, s, [&k](u32 code)
    {
        return code == 0x10005 || code == 0x10004 || (code == 0x10006 && k.language != 7);
    });
}

// Xbox 0x00016230: the accented letters' labels.
void make_accent_texts(VirtualKeyboard& k, GameSelectControlServices& s)
{
    make_key_texts(k, s, [](u32 code)
    {
        return 0xA0 < code && code < 0x179;
    });
}

// Xbox 0x000163F0, EBX the keyboard: the digits' labels removed.
void remove_digit_texts(VirtualKeyboard& k)
{
    each_key(k, [&k](VirtualKeyboardKey& key)
    {
        if (0x2F < key.code && key.code < 0x3A)
        {
            remove_key_text(*k.pools[0], key.text); // 0x0005FB10
        }
    });
}

// Xbox 0x00016490: the letters' labels removed.
void remove_letter_texts(VirtualKeyboard& k)
{
    each_key(k, [&k](VirtualKeyboardKey& key)
    {
        if (0x40 < key.code && key.code < 0x100)
        {
            remove_key_text(*k.pools[0], key.text);
        }
    });
}

// Xbox 0x00016530: the symbols' labels removed.
void remove_symbol_texts(VirtualKeyboard& k)
{
    each_key(k, [&k](VirtualKeyboardKey& key)
    {
        if ((0x20 < key.code && key.code < 0x7F) || (0xA0 < key.code && key.code < 0x20AD))
        {
            remove_key_text(*k.pools[0], key.text);
        }
    });
}

// Xbox 0x000165E0: the mode keys' labels ended.
void end_mode_texts(VirtualKeyboard& k, GameSelectControlServices& s)
{
    each_key(k, [&](VirtualKeyboardKey& key)
    {
        if (key.code == 0x10006 || key.code == 0x10005 || key.code == 0x10004)
        {
            end_dialogue_text_record(*k.pools[0], key.text, s.fonts, s.fallback_texture, s.random); // 0x0005F7E0
        }
    });
}

// Xbox 0x00016690: the accented letters' labels removed.
void remove_accent_texts(VirtualKeyboard& k)
{
    each_key(k, [&k](VirtualKeyboardKey& key)
    {
        if (0xA0 < key.code && key.code < 0x179)
        {
            remove_key_text(*k.pools[0], key.text);
        }
    });
}

// Xbox 0x00016730, EDI the keyboard: the key bat flies over the selected key's label.
void move_key_bat(VirtualKeyboard& k, GameSelectControlServices& s)
{
    if (k.key_bat == nullptr || k.board == 4 || k.board == 3)
    {
        return;
    }
    float y = 135.f; // DAT_0032B570.
    for (u32 row = 0; row < k.rows; ++row)
    {
        auto& keys = current_board(k).at(row);
        float x = 0.f;
        i32 column = 0;
        for (auto& key : keys)
        {
            if (row == u32(k.row) && column == k.column)
            {
                const float left = x + 64.f; // DAT_0032B450.
                Text label{};
                key_label(key, k, label.data(), s); // 0x00016360
                k.key_target.x = text_width(label.data(), 0.65f, s) * 0.5f + left; // DAT_0032B1EC, DAT_0032B0F4.
                k.key_target.y = y + 18.f; // DAT_0032B67C.
                if (k.key_bat == nullptr)
                {
                    return;
                }
                s.fly_bat(*k.key_bat, k.key_target, 1.25f); // 0x0006DA00, EBX +0xFC
            }
            x = float(double(key_width(key)) + double(x));
            if (column == 0)
            {
                x = x + 4.f; // DAT_0032B1B0.
            }
            ++column;
        }
        y = k.row_spacing + y;
    }
}

// Xbox 0x00016E10, EDX the keyboard: the column a vertical move keeps. A key with a label keeps the old one.
void keep_column(VirtualKeyboard& k, GameSelectControlServices& s)
{
    const auto& key = selected_key(k);
    if (key.label != 0 && read_u16(s.image.memory(key.label, 2), 0) != 0)
    {
        return;
    }
    if (key.code != 0x22B2)
    {
        if (key.code != 0x22B3)
        {
            k.kept_column = k.column;
            return;
        }
        if (k.board == 1)
        {
            k.kept_column = 3;
            return;
        }
        k.kept_column = (9 < u32(k.kept_column) ? 1 : 0) + 9;
        return;
    }
    if (k.board == 1)
    {
        k.kept_column = 2;
        return;
    }
    k.kept_column = (7 < u32(k.kept_column) ? 1 : 0) + 7;
}
}

namespace
{
// Xbox 0x00016950, ECX the keyboard: up, with the boards' column remaps.
void move_up(VirtualKeyboard& k)
{
    if (k.language == 2)
    {
        do
        {
            k.row = (k.row == 0 ? i32(k.rows) : k.row) - 1;
        }
        while (key_skipped(k));
        return;
    }
    do
    {
        const i32 row = k.row;
        switch (row)
        {
        case 0:
        {
            const u32 column = u32(k.column);
            if (k.board == 1)
            {
                if (column < 2 || 4 < column)
                {
                    if (column == 5)
                    {
                        const i32 kept = k.kept_column;
                        if (kept == 2 || kept == 3)
                        {
                            k.column = kept;
                        }
                        else
                        {
                            k.column = 1;
                        }
                        k.kept_column = 5;
                    }
                }
                else
                {
                    k.kept_column = i32(column);
                    k.column = 1;
                }
            }
            else if (column < 2 || 6 < column)
            {
                if (column < 7 || 8 < column)
                {
                    if (8 < column)
                    {
                        k.kept_column = i32(column);
                        k.column = 3;
                    }
                }
                else
                {
                    k.kept_column = i32(column);
                    k.column = 2;
                }
            }
            else
            {
                k.kept_column = i32(column);
                k.column = 1;
            }
            break;
        }
        case 1:
            if (k.board == 2 && k.column == 0xB)
            {
                k.kept_column = 0xB;
                k.column = 10;
            }
            if (k.board == 1 && k.column == 6)
            {
                k.kept_column = 6;
                k.column = 5;
            }
            break;
        case 3:
            if (k.board == 1)
            {
                if (k.column == 7)
                {
                    u32 kept = u32(k.kept_column);
                    if (6 < kept)
                    {
                        kept = 6;
                    }
                    k.column = i32(kept);
                }
            }
            else if (k.column == 7)
            {
                u32 kept = u32(k.kept_column);
                if (kept < 7)
                {
                    kept = 7;
                }
                k.column = i32(kept);
            }
            break;
        case 4:
            if (u32(k.column) == 1)
            {
                u32 kept = u32(k.kept_column);
                if (6 < kept)
                {
                    kept = 6;
                }
                k.column = i32(kept);
            }
            else if (1 < u32(k.column))
            {
                k.column = 7;
            }
            break;
        default:
            break;
        }
        k.row = (row == 0 ? i32(k.rows) : row) - 1;
    }
    while (key_skipped(k));
}

// Xbox 0x00016B30, ECX the keyboard: down, with the boards' column remaps.
void move_down(VirtualKeyboard& k)
{
    const i32 last = i32(k.rows) - 1;
    if (k.language == 2)
    {
        do
        {
            k.row = k.row == last ? 0 : k.row + 1;
        }
        while (key_skipped(k));
        return;
    }
    do
    {
        const i32 row = k.row;
        switch (row)
        {
        case 0:
            if (k.board == 2 && k.column == 10)
            {
                u32 kept = u32(k.kept_column);
                if (kept < 0xB)
                {
                    kept = 10;
                }
                k.column = i32(kept);
            }
            if (k.board == 1 && k.column == 5)
            {
                u32 kept = u32(k.kept_column);
                if (kept < 6)
                {
                    kept = 5;
                }
                k.column = i32(kept);
                if (6 < kept)
                {
                    k.column = 6;
                }
            }
            break;
        case 2:
            if (7 < u32(k.column))
            {
                k.kept_column = k.column;
                k.column = 7;
            }
            break;
        case 3:
        {
            const u32 column = u32(k.column);
            if (column == 0 || 6 < column)
            {
                if (6 < column)
                {
                    k.column = (8 < u32(k.kept_column) ? 1 : 0) + 2;
                }
            }
            else
            {
                k.kept_column = i32(column);
                k.column = 1;
            }
            break;
        }
        case 4:
        {
            const i32 column = k.column;
            u32 kept = u32(k.kept_column);
            if (column == 1)
            {
                if (k.board == 1)
                {
                    if (5 < kept)
                    {
                        kept = 5;
                    }
                }
                else if (6 < kept)
                {
                    kept = 6;
                }
                k.column = i32(kept);
            }
            else if (column == 2)
            {
                if (k.board == 1)
                {
                    k.kept_column = 2;
                    k.column = 5;
                }
                else
                {
                    if (kept < 9)
                    {
                        if (kept < 8)
                        {
                            kept = 7;
                        }
                        else if (8 < kept)
                        {
                            kept = 8;
                        }
                    }
                    else
                    {
                        kept = 8;
                    }
                    k.column = i32(kept);
                }
            }
            else if (column == 3)
            {
                if (k.board != 1)
                {
                    k.column = i32(10 - (u32(k.kept_column) < 10 ? 1 : 0));
                }
                else
                {
                    k.kept_column = 3;
                    k.column = 5;
                }
            }
            break;
        }
        default:
            break;
        }
        k.row = row == last ? 0 : row + 1;
    }
    while (key_skipped(k));
}

i32 row_length(VirtualKeyboard& k)
{
    return i32(current_board(k).at(size_t(k.row)).size());
}

// Xbox 0x00016D40, ECX the keyboard: left.
void move_left(VirtualKeyboard& k, GameSelectControlServices& s)
{
    do
    {
        i32 column = k.column;
        if (column == 0)
        {
            column = row_length(k);
        }
        k.column = column - 1;
    }
    while (key_skipped(k));
    keep_column(k, s); // 0x00016E10
}

// Xbox 0x00016DA0, ECX the keyboard: right.
void move_right(VirtualKeyboard& k, GameSelectControlServices& s)
{
    do
    {
        if (k.column == row_length(k) - 1)
        {
            k.column = 0;
        }
        else
        {
            ++k.column;
        }
    }
    while (key_skipped(k));
    keep_column(k, s);
}

// Xbox 0x00017620, EAX the keyboard: off skipped keys, and back inside a shorter row.
void settle_cursor(VirtualKeyboard& k, GameSelectControlServices& s)
{
    while (key_skipped(k))
    {
        ++k.column;
        k.kept_column = k.column;
        move_key_bat(k, s); // 0x00016730
    }
    if (!k.boards.empty())
    {
        const i32 length = row_length(k);
        if (u32(length - 1) < u32(k.column))
        {
            k.column = length - 1;
            k.kept_column = length - 1;
            move_key_bat(k, s);
        }
    }
}

// Xbox 0x00014AB0: the prompts ("VirtualKeyboardMenu", 0x0020AAD8) under the keyboard.
void make_prompts(VirtualKeyboard& k, GameSelectControlServices& s)
{
    char resource[0x80]{};
    std::memcpy(resource, "aid_loctext_ghoulies_gameselect_gameselecttext", 47); // 0x0020A574
    const std::array<float, 2> anchor{s.hud.inner_left + 100.f, // DAT_0032B12C.
        s.hud.inner_bottom - 16.f}; // DAT_0032B1F4.
    const std::array<float, 2> row_step{0.f, -28.8f}; // DAT_0032B620.
    const std::array<float, 2> origin{16.4f, -10.4f}; // DAT_0032B61C, DAT_0032B618.
    if (s.text.data() == nullptr) // IsBadReadPtr(DAT_005462C4, 1)
    {
        return;
    }
    std::array<char16_t, 0x200> scratch{};
    const u8* rows = s.image.memory(0x0020AAD8, 0x310).data();
    const float width = measure_dialogue_widget_width(resource, rows, 0.65f, 0.7f, "gameselect", scratch.data(),
        s.widgets); // 0x001428F0, ECX the rows
    const std::array<float, 2> column{width, 0.f};
    create_dialogue_widget(k.prompts, 1, rows, anchor, row_step, origin, &column, 0.65f, 0.7f, "gameselect", s.text,
        0x400C0, s.widget_allocation); // 0x00141E20, EAX +0x10C
    k.prompts->pool->origin = {3.f, 0.7f, std::bit_cast<float>(3u)}; // DAT_0032B260, DAT_0032B264.
    k.prompts->sound_enabled = 0;
}

// Xbox 0x00011870, ECX the name: the name trimmed to the box, the textures and the boards.
void lay_out_keyboard(const char16_t* name, VirtualKeyboard& k, GameSelectControlServices& s)
{
    k.row_spacing = 42.f; // DAT_0032B574.
    k.row = 0;
    k.column = 0;
    k.kept_column = 1;
    k.board = 0;
    k.caps = 0;
    k.shift = 0;
    k.fresh = 1;
    k.rows = 5;
    k.width_limit = float(i32(s.screen[0])) - 260.f; // DAT_0032B654.
    k.name[0] = 0;
    if (wide_length(name) >= k.name.size())
    {
        throw Error("the default game name runs past the keyboard's 0x40-character buffer");
    }
    copy_wide(k.name.data(), name);
    const float scale = 1.45f; // DAT_0032B538.
    while (text_width(k.name.data(), scale, s) > k.width_limit)
    {
        k.name[wide_length(k.name.data()) - 1] = 0;
    }
    k.cursor = i32(wide_length(k.name.data()));
    const auto texture = [&s](const char* aid) -> std::span<const u8>
    {
        const auto loaded = acquire_animation_resource(s.resources, aid);
        if (loaded.data() == nullptr)
        {
            resource_error(s.error, 2);
        }
        return loaded;
    };
    if (k.caret.data() == nullptr)
    {
        k.caret = texture("aid_texture_ghoulies_gameselect_virtkb_caret"); // 0x0020A60C
    }
    if (k.arrow_left.data() == nullptr)
    {
        k.arrow_left = texture("aid_texture_ghoulies_gameselect_virtkb_arrowleft");
    }
    if (k.arrow_right.data() == nullptr)
    {
        k.arrow_right = texture("aid_texture_ghoulies_gameselect_virtkb_arrowright");
    }
    k.boards.clear();
    if (k.language == 1)
    {
        return;
    }
    if (k.language == 2)
    {
        untranslated("0x00012920 (the Japanese keyboard layout)");
    }
    for (const auto& spec : default_boards())
    {
        VirtualKeyboardBoard board;
        board.reserve(k.rows); // 0x000178C0
        for (const auto& keys : spec)
        {
            VirtualKeyboardRow row;
            row.reserve(0xE); // 0x00017790
            for (const auto& key : keys)
            {
                row.push_back(made_key(key.code, key.width, s)); // 0x00011150, 0x00017860
            }
            board.push_back(row); // 0x000179A0
        }
        k.boards.push_back(board); // 0x00017A40
    }
}

// Xbox 0x00014C10: the pad's request this frame. The buttons give 1..10; the stick or pad direction 0xB..0xE, held
// directions repeating after DAT_0032B3E8, then every DAT_0020AE84 seconds.
u32 read_keyboard_input(VirtualKeyboard& k, GameSelectControlServices& s)
{
    u32 action = 0;
    const std::array<std::pair<u16, u32>, 10> buttons{{{0x10, 2}, {0x20, 4}, {0x100, 1}, {0x200, 3}, {0x400, 5},
        {0x800, 6}, {0x1000, 8}, {0x2000, 7}, {0x4000, 9}, {0x8000, 10}}};
    for (const auto& [mask, request] : buttons)
    {
        if (edge(mask, s) != 0)
        {
            action = request;
        }
    }
    const float high = 0.25f; // DAT_0032B104.
    const float low = -0.25f; // DAT_0032B3F0.
    const auto quiet = [&]()
    {
        if (axes(s)[1] > high || low > axes(s)[1] || low > axes(s)[0])
        {
            return false;
        }
        if (axes(s)[0] > high) // 0x001432E0
        {
            return false;
        }
        return edge(4, s) == 0 && edge(8, s) == 0 && edge(1, s) == 0 && edge(2, s) == 0; // 0x00143280, 0x001432A0, 0x001432C0
    };
    const auto direction = [&]()
    {
        if (axes(s)[1] > high || edge(1, s) != 0)
        {
            action = 0xB;
        }
        if (low > axes(s)[1] || edge(2, s) != 0)
        {
            action = 0xC;
        }
        if (low > axes(s)[0] || edge(4, s) != 0)
        {
            action = 0xD;
        }
        if (axes(s)[0] > high || edge(8, s) != 0)
        {
            action = 0xE;
        }
    };
    if (quiet())
    {
        k.repeat = 0.f;
        return action;
    }
    if (k.repeat == 0.f)
    {
        direction();
        k.repeat = 0.333f; // DAT_0032B3E8.
        return action;
    }
    const float step = frame_step(s);
    if (0.f < k.repeat)
    {
        k.repeat -= step;
        if (!(0.f > k.repeat))
        {
            return action;
        }
        k.repeat = 0.f;
        direction();
        k.repeat = 0.085f; // DAT_0020AE84.
    }
    return action;
}

void press_key(VirtualKeyboard& k, u32 code, GameSelectControlServices& s);
void release_keyboard_bats(VirtualKeyboard& k);
void show_name_error(const char* key, GameSelectControlServices& s);

// Xbox 0x00015260, ECX the keyboard: the selected key, then the button sound.
void press_selected_key(VirtualKeyboard& k, GameSelectControlServices& s)
{
    press_key(k, selected_key(k).code, s); // 0x000152F0, EAX the code
    const i16 voice = allocate_audio_event_voice(0, s.voices); // 0x00039830, EBX 0
    if (voice != -1)
    {
        s.voices.voices[size_t(voice)].stop = 1; // DAT_004E0CEC + voice * 0x24
        prepare_audio_event_voice(voice, "XACT_SOUNDBANK_GSUPERSCARYSTUF_RIGHTBUTTON", s.voices); // 0x00039A20
    }
}

// Xbox 0x00015090, EAX the keyboard: the request (from 0x00014C10) carried out.
void handle_keyboard_request(VirtualKeyboard& k, u32 action, GameSelectControlServices& s)
{
    if (k.done != 0)
    {
        return;
    }
    switch (action)
    {
    case 1:
    case 2:
        press_selected_key(k, s);
        return;
    case 3:
    case 4:
        leave_runtime_game_naming(s); // 0x000DD550
        return;
    case 5:
        if (k.language == 2)
        {
            switch (k.board)
            {
            case 3:
                press_key(k, 0x10009, s);
                return;
            case 4:
                press_key(k, 0x1000A, s);
                return;
            case 5:
                press_key(k, 0x10008, s);
                return;
            default:
                return;
            }
        }
        if (k.language == 7)
        {
            press_key(k, 0x10004 + (k.board != 1 ? 1 : 0), s);
            return;
        }
        switch (k.board)
        {
        case 0:
            press_key(k, 0x10005, s);
            return;
        case 1:
            press_key(k, 0x10006, s);
            return;
        case 2:
            press_key(k, 0x10004, s);
            return;
        default:
            return;
        }
    case 9:
        press_key(k, 0x22B2, s);
        return;
    case 10:
        press_key(k, 0x22B3, s);
        return;
    case 0xB:
        move_up(k); // 0x00016950
        move_key_bat(k, s); // 0x00016730
        return;
    case 0xC:
        move_down(k); // 0x00016B30
        move_key_bat(k, s);
        return;
    case 0xD:
        move_left(k, s); // 0x00016D40
        move_key_bat(k, s);
        return;
    case 0xE:
        move_right(k, s); // 0x00016DA0
        move_key_bat(k, s);
        return;
    default:
        return;
    }
}

// Xbox 0x00015220, ECX the keyboard: layout 1's toggles and its held key.
void handle_toggles(VirtualKeyboard& k, GameSelectControlServices& s)
{
    if ((k.buttons & 0x200) != 0)
    {
        k.word_124 ^= 1;
    }
    if ((k.buttons & 0x100) != 0)
    {
        k.word_128 ^= 1;
    }
    if (k.word_120 != 0)
    {
        press_key(k, k.word_120, s); // 0x000152F0, EAX +0x120
    }
}

// Clears the default name when a key first edits it.
void forget_default_name(VirtualKeyboard& k)
{
    k.name[0] = 0;
    k.cursor = 0;
    k.fresh = 0;
}

// The board's mode key in row 3 and the board switch key in row 4 (0x000152F0's 0x10004..0x10006 tails).
void set_mode_key(VirtualKeyboard& k, size_t board, u32 code, u32 label_offset, GameSelectControlServices& s)
{
    auto& key = k.boards.at(board).at(3).at(0);
    key.code = code;
    key.width = 0xA6;
    key.label = g_key_labels == 0 ? 0x0020A4F4u : exe_word(s, g_key_labels + label_offset);
}

// Xbox 0x000152F0, ECX the keyboard, EAX the key code.
void press_key(VirtualKeyboard& k, u32 code, GameSelectControlServices& s)
{
    if (code == 0 || (code < 0x10000 && code != 0x22B2 && code != 0x22B3))
    {
        if (wide_length(k.name.data()) < 0x40)
        {
            Text edited{};
            if (k.fresh != 0)
            {
                forget_default_name(k);
            }
            copy_wide(edited.data(), k.name.data());
            Text tail{};
            const char16_t* rest = edited.data() + k.cursor;
            if (rest[0] != 0)
            {
                copy_wide(tail.data(), rest);
            }
            const i32 at = k.cursor;
            const char16_t typed = char16_t(key_character(code, k)); // 0x000176E0, EAX the code, EDX the keyboard
            const bool more = tail[0] != 0;
            edited[size_t(at)] = typed;
            edited[size_t(at) + 1] = 0;
            if (more)
            {
                edited[size_t(at) + 1] = 0;
                std::u16string joined(edited.data());
                joined += tail.data(); // lstrcatW
                std::copy(joined.begin(), joined.end(), edited.begin());
                edited[joined.size()] = 0;
            }
            if (text_width(edited.data(), 1.45f, s) < k.width_limit) // DAT_0032B538.
            {
                copy_wide(k.name.data(), edited.data());
                ++k.cursor;
                move_caret_bat(k, s); // 0x00016900
                if (k.shift != 0)
                {
                    k.shift = 0;
                    remove_letter_texts(k); // 0x00016490
                    make_letter_texts(k, s); // 0x00015D60
                }
                refresh_name_text(k, s); // 0x00015A10
                return;
            }
        }
        return;
    }
    if (code < 0x10005)
    {
        if (code == 0x10004)
        {
            if (k.fresh != 0)
            {
                forget_default_name(k);
                move_caret_bat(k, s);
                refresh_name_text(k, s);
            }
            remove_accent_texts(k); // 0x00016690
            remove_symbol_texts(k); // 0x00016530
            end_mode_texts(k, s); // 0x000165E0
            k.board = 0;
            set_mode_key(k, 0, 0x10005, 0x2C, s);
            current_board(k).at(4).at(0) = made_key(0x10006, 0xA6, s); // 0x00011150
            make_letter_texts(k, s); // 0x00015D60
            make_digit_texts(k, s); // 0x00015E90
            make_mode_texts(k, s); // 0x000160F0
            settle_cursor(k, s); // 0x00017620
            return;
        }
        if (0x10001 < code)
        {
            if (code == 0x10002)
            {
                k.shift = k.shift == 0 ? 1 : 0;
            }
            else
            {
                if (code != 0x10003)
                {
                    return;
                }
                k.caps = k.caps == 0 ? 1 : 0;
            }
            if (k.board == 2)
            {
                remove_accent_texts(k); // 0x00016690
                make_accent_texts(k, s); // 0x00016230
                return;
            }
            remove_letter_texts(k); // 0x00016490
            make_letter_texts(k, s); // 0x00015D60
            return;
        }
        if (code == 0x10001)
        {
            if (0 < i32(wide_length(k.name.data())))
            {
                k.name[size_t(k.cursor)] = 0;
            }
            return;
        }
        if (code == 0x22B2)
        {
            if (0 < k.cursor)
            {
                k.fresh = 0;
                --k.cursor;
                move_caret_bat(k, s);
            }
            return;
        }
        if (code == 0x22B3)
        {
            k.fresh = 0;
            if (k.cursor < i32(wide_length(k.name.data())))
            {
                ++k.cursor;
                move_caret_bat(k, s);
            }
            return;
        }
        if (code == 0x10000 && 0 < k.cursor)
        {
            if (k.fresh != 0)
            {
                forget_default_name(k);
                move_caret_bat(k, s);
                refresh_name_text(k, s);
                return;
            }
            Text tail{};
            const char16_t* rest = k.name.data() + k.cursor;
            if (rest != nullptr && rest[0] != 0) // 0x000FBE30
            {
                copy_wide(tail.data(), rest);
            }
            Text edited{};
            std::copy(k.name.begin(), k.name.begin() + (k.cursor - 1), edited.begin()); // lstrcpynW(.., cursor)
            edited[size_t(k.cursor - 1)] = 0;
            if (tail[0] != 0)
            {
                std::u16string joined(edited.data());
                joined += tail.data();
                std::copy(joined.begin(), joined.end(), edited.begin());
                edited[joined.size()] = 0;
            }
            copy_wide(k.name.data(), edited.data());
            --k.cursor;
            move_caret_bat(k, s);
            refresh_name_text(k, s);
        }
        return;
    }
    switch (code)
    {
    case 0x10005:
        if (k.fresh != 0)
        {
            forget_default_name(k);
            move_caret_bat(k, s);
            refresh_name_text(k, s);
        }
        remove_accent_texts(k); // 0x00016690
        remove_digit_texts(k); // 0x000163F0
        remove_letter_texts(k); // 0x00016490
        end_mode_texts(k, s); // 0x000165E0
        k.board = 1;
        set_mode_key(k, 1, 0x10004, 0x28, s);
        current_board(k).at(4).at(0) = made_key(0x10006, 0xA6, s);
        make_symbol_texts(k, s); // 0x00015FC0
        make_mode_texts(k, s); // 0x000160F0
        settle_cursor(k, s);
        return;
    case 0x10006:
    {
        if (k.fresh != 0)
        {
            forget_default_name(k);
            move_caret_bat(k, s);
            refresh_name_text(k, s);
        }
        remove_letter_texts(k); // 0x00016490
        remove_symbol_texts(k); // 0x00016530
        end_mode_texts(k, s); // 0x000165E0
        k.board = 2;
        set_mode_key(k, 2, 0x10004, 0x28, s);
        auto& key = current_board(k).at(4).at(0);
        key.code = 0x10005;
        key.width = 0xA6;
        key.label = g_key_labels == 0 ? 0x0020A4F4u : exe_word(s, g_key_labels + 0x2C);
        make_accent_texts(k, s); // 0x00016230
        make_digit_texts(k, s); // 0x00015E90
        make_mode_texts(k, s); // 0x000160F0
        settle_cursor(k, s);
        return;
    }
    case 0x10007:
    {
        const i32 check = check_new_game_name(k.name.data(), s); // 0x001411C0, EAX +0x24
        switch (check)
        {
        case 0:
        {
            k.prompts->requested_set = nullptr; // +0x950
            set_dialogue_widget_mode(*k.prompts, 3, s.widgets); // 0x00142C20, EAX 3, ESI +0x10C
            k.done = 1;
            for (auto* pool : {k.pools[2], k.pools[0]})
            {
                pool->allocation->cursor = pool->allocation->begin;
                pool->records = {pool->allocation->begin, 0};
            }
            release_keyboard_bats(k); // 0x00017490, EDI the keyboard
            if (k.prompts != nullptr)
            {
                destroy_dialogue_widget(k.prompts, s.resources, *s.text_pools); // 0x00142070
                k.prompts = nullptr;
            }
            commit_runtime_game_name(k.name.data(), s); // 0x000DD210
            return;
        }
        case 2:
            show_name_error("errormessages_notuniquegamename", s); // 0x000582A0(0), ECX 0x0020A554
            return;
        case 3:
            show_name_error("errormessages_namenotset", s); // ECX 0x0020A538
            return;
        case 4:
            show_name_error("errormessages_nameallwhitespace", s); // ECX 0x0020A518
            return;
        case 5:
            show_name_error("errormessages_nameisreserved", s); // ECX 0x0020A4F8
            return;
        default:
            return;
        }
    }
    case 0x10008:
        k.board = 3;
        return;
    case 0x10009:
        k.board = 4;
        return;
    case 0x1000A:
        k.board = 5;
        return;
    default:
        return;
    }
}

// Xbox 0x00017490, EDI the keyboard: the key and caret bats released.
void release_keyboard_bats(VirtualKeyboard& k)
{
    if (k.key_bat != nullptr)
    {
        activate_actor_bind_tree(*k.key_bat); // 0x00112CC0
        k.key_bat = nullptr;
    }
    if (k.caret_bat != nullptr)
    {
        activate_actor_bind_tree(*k.caret_bat);
        k.caret_bat = nullptr;
    }
}

// 0x000582A0(0), PUSH EBP 0: the dialogs page with a key from aid_loctext_ghoulies_dialog_dialogs.
void show_name_error(const char* key, GameSelectControlServices& s)
{
    if (s.cutscenes.dialogue == nullptr)
    {
        throw Error(std::string("the keyboard's name error ") + key + ": the scene dialogue (0x000582A0) is not bound");
    }
    enqueue_runtime_dialogue_key(key, 0, *s.cutscenes.dialogue); // 0x000582A0
}

// Xbox 0x00016FC0: a key's panel, unless its color is the hidden one (bytes 00 00 FF FF).
void draw_key_panel(VirtualKeyboard& k, float x, float y, const VirtualKeyboardKey& key, u32 color,
    GameSelectControlServices& s)
{
    if ((color & 0xFF) == 0 && ((color >> 8) & 0xFF) == 0 && ((color >> 16) & 0xFF) == 0xFF
        && (color >> 24) == 0xFF)
    {
        return;
    }
    if (key.code == 0)
    {
        return;
    }
    Text label{};
    key_label(key, k, label.data(), s); // 0x00016360
    const float scale = 0.65f; // DAT_0032B1EC.
    const float pad = 10.f; // DAT_0032B10C.
    const float width = text_width(label.data(), scale, s) + pad;
    const float height = cell_height(s) * scale + pad;
    const float inset = 5.f; // DAT_0032B184.
    draw_runtime_dialogue_panel(x - inset, y - inset, width, height, 0.25f, color, s.outer); // 0x00061360
}

// ECX the keyboard: an arrow key's texture over its label (one retail function per arrow).
// Xbox 0x00015A60
// Xbox 0x00015B50
void draw_arrow(VirtualKeyboard& k, const VirtualKeyboardKey& key, std::span<const u8> arrow, float x, float y,
    GameSelectControlServices& s)
{
    Text label{};
    key_label(key, k, label.data(), s); // 0x00016360
    const float scale = 0.65f; // DAT_0032B1EC.
    const float half = 0.5f; // DAT_0032B0F4.
    const float position[2]{text_width(label.data(), scale, s) * half + x, cell_height(s) * scale * half + y};
    const float one = 1.f; // DAT_0032B0F8.
    const float scales[2]{one, one};
    if (arrow.data() != nullptr)
    {
        draw_sprite(arrow, scales, 0xFFFFFFFFu, position, s); // 0x0005CDC0, 0x0005CA60
    }
}

// Xbox 0x000170A0, ECX the keyboard: the panel behind a name not yet edited.
void draw_name_highlight(VirtualKeyboard& k, GameSelectControlServices& s)
{
    const float scale = 1.45f; // DAT_0032B538.
    const float pad = 8.f; // DAT_0032B26C.
    const i32 width = i32(float(i32(text_width(k.name.data(), scale, s))) + pad);
    const i32 height = i32(cell_height(s) * scale + pad);
    draw_runtime_dialogue_panel(126.f, 50.f, float(width), float(height), 0.25f, exe_word(s, 0x0020AE74), s.outer);
}

// EDX the keyboard: a bat at scale DAT_0032B114, pass 3 (one retail function per bat).
// Xbox 0x00017110
// Xbox 0x00017190
void draw_bat(Actor* bat, GameSelectControlServices& s)
{
    if (bat == nullptr || (s.flags & 0x40080u) != 0)
    {
        return;
    }
    const float scale = 1.25f; // DAT_0032B114.
    if (bat->scale_200 != scale)
    {
        bat->scale_200 = scale;
        bat->xform_valid = 0;
    }
    s.draw_actor(*bat, i32(s.outer.current_display_address - 0x507018u) / 0x1E4, 3); // Vtable +0x14
}

// Xbox 0x000174D0, EDI the keyboard: the caret, then the caret bat.
void draw_caret(VirtualKeyboard& k, GameSelectControlServices& s)
{
    const float scale = 1.45f; // DAT_0032B538.
    const float scales[2]{scale, scale};
    float position[2];
    caret_position(k, position, s); // 0x00015010
    if (k.caret.data() != nullptr)
    {
        draw_sprite(k.caret, scales, 0xFFFFFFFFu, position, s);
    }
    draw_bat(k.caret_bat, s); // 0x00017190
}

// Xbox 0x00017210, EAX the keyboard: the name box, the caret and every key's panel.
void draw_keys(VirtualKeyboard& k, GameSelectControlServices& s)
{
    const float pad = 16.f; // DAT_0032B1F4.
    const i32 height = i32(cell_height(s) * 1.45f + pad); // DAT_0032B538.
    const i32 width = i32(k.width_limit + pad);
    draw_runtime_dialogue_panel(122.f, 46.f, float(width), float(height), 0.25f, exe_word(s, 0x0020AE70),
        s.outer); // 0x00061360
    if (k.fresh != 0)
    {
        draw_name_highlight(k, s); // 0x000170A0
    }
    draw_caret(k, s); // 0x000174D0
    const u32 plain = exe_word(s, 0x0020AE6C);
    const u32 lit = exe_word(s, 0x0020AE68);
    walk_keys(k, s, [&](VirtualKeyboardKey& key, u32 row, i32 column, float x, float y)
    {
        u32 color = plain;
        const u32 code = key.code;
        if (code > 0x10003)
        {
            if (code == 0x10006 && k.language == 7)
            {
                color = exe_word(s, 0x0020AE7C);
            }
        }
        else if (code == 0x10003 || code == 0x10002)
        {
            const u32 toggle = code == 0x10003 ? k.caps : k.shift;
            switch (k.board)
            {
            case 0:
            case 2:
                if (toggle != 0)
                {
                    color = lit;
                }
                break;
            case 1:
                color = exe_word(s, 0x0020AE78);
                break;
            default:
                break;
            }
        }
        else if (code == 0xDF && k.board == 2)
        {
            if (k.caps != 0)
            {
                if (k.shift == 0)
                {
                    color = exe_word(s, 0x0020AE7C);
                }
            }
            else if (k.shift != 0)
            {
                color = exe_word(s, 0x0020AE7C);
            }
        }
        if (row == u32(k.row) && column == k.column)
        {
            color = lit;
        }
        const float left = float(double(x) + double(64.f)); // DAT_0032B450.
        draw_key_panel(k, left, y, key, color, s); // 0x00016FC0
        if (key.code == 0x22B2)
        {
            draw_arrow(k, key, k.arrow_left, left, y, s); // 0x00015A60
        }
        else if (key.code == 0x22B3)
        {
            draw_arrow(k, key, k.arrow_right, left, y, s); // 0x00015B50
        }
    });
}
}

// Xbox 0x000110D0
VirtualKeyboard* create_virtual_keyboard(const char16_t* name, GameSelectControlServices& services)
{
    auto& s = services;
    auto* k = new VirtualKeyboard; // 0x000F72D0(300)
    // 0x00011270: the keyboard's own input device (0x00107EF0, ESI +0x110), pushed as "virtualkeyboard".
    k->layout = 0x00434118;
    k->row_spacing = 42.f; // DAT_0032B574.
    k->rows = 5;
    k->fresh = 1;
    k->input = create_runtime_keyboard_input(s.input_release);
    k->input_address = s.map_input(k->input);
    push_runtime_input_entry(k->input_address, "virtualkeyboard", s.input); // 0x00109340, ESI 0x0020A63C
    k->language = 0;
    k->layout = 0x00434118;
    g_key_labels = exe_word(s, 0x00434120);
    // 0x00011570
    allocate_dialogue_text_pool(k->pools[0], 0, 1, 0x400C0, s.text_pools);
    allocate_dialogue_text_pool(k->pools[1], 0, 1, 0x400C0, s.text_pools);
    allocate_dialogue_text_pool(k->pools[2], 0, 1, 0x400C0, s.text_pools);
    k->pools[0]->origin = {3.f, 0.7f, std::bit_cast<float>(3u)}; // DAT_0032B260, DAT_0032B264.
    switch (s.language)
    {
    case 0:
        k->language = 0;
        k->layout = 0x00434118;
        g_key_labels = exe_word(s, 0x00434120);
        break;
    case 1:
        k->language = 4;
        k->layout = 0x00434148;
        g_key_labels = exe_word(s, 0x00434150);
        break;
    case 2:
        k->language = 3;
        k->layout = 0x0043413C;
        g_key_labels = exe_word(s, 0x00434144);
        break;
    case 3:
        k->language = 5;
        k->layout = 0x00434154;
        g_key_labels = exe_word(s, 0x0043415C);
        break;
    case 4:
        k->language = 6;
        k->layout = 0x00434160;
        g_key_labels = exe_word(s, 0x00434168);
        break;
    case 6:
    case 7:
    case 8:
        k->language = 7;
        k->layout = 0x0043416C;
        g_key_labels = exe_word(s, 0x00434174);
        break;
    default:
        break;
    }
    make_prompts(*k, s); // 0x00014AB0
    lay_out_keyboard(name, *k, s); // 0x00011870
    end_all_dialogue_text_records(*k->pools[0], s.fonts, s.fallback_texture, s.random); // 0x0005F970
    k->board = 0;
    make_all_key_texts(*k, s); // 0x00015C40
    if (k->key_bat == nullptr)
    {
        k->key_bat = s.create_bat({0.f, 0.f}, 1.25f, 0.25f); // 0x0013B580
        k->key_target = {0.f, 0.f};
    }
    if (k->caret_bat == nullptr)
    {
        k->caret_bat = s.create_bat({0.f, 0.f}, 1.25f, 0.25f);
        k->caret_target = {0.f, 0.f};
    }
    move_key_bat(*k, s); // 0x00016730, EDI the keyboard
    refresh_name_text(*k, s);
    if (k->caret_bat != nullptr)
    {
        float target[2];
        caret_position(*k, target, s); // 0x00015010, ESI +0x104
        k->caret_target = {target[0], target[1]};
        s.fly_bat(*k->caret_bat, k->caret_target, 1.25f); // 0x0006DA00
    }
    return k;
}

// Xbox 0x00011800
void tick_virtual_keyboard(VirtualKeyboard& keyboard, GameSelectControlServices& services)
{
    auto& s = services;
    auto& k = keyboard;
    if (s.dialogue_page_hold() != 0)
    {
        return;
    }
    if (k.input.data() != nullptr && k.prompts != nullptr)
    {
        refresh_runtime_input_device(k.input_address, s.frame, s.input, s.input_release); // 0x00109470
        s.update_widget(*k.prompts, k.input_address); // 0x00142460, EAX +0x110, EDI +0x10C
    }
    const u32 action = read_keyboard_input(k, s); // 0x00014C10
    if (k.language == 1 && k.done == 0)
    {
        if (action != 4)
        {
            k.buttons = 0;
            handle_toggles(k, s); // 0x00015220
        }
        return;
    }
    handle_keyboard_request(k, action, s); // 0x00015090
}

// Xbox 0x00011110
void draw_virtual_keyboard(VirtualKeyboard& keyboard, GameSelectControlServices& services)
{
    auto& s = services;
    auto& k = keyboard;
    if (s.dialogue_page_hold() != 0)
    {
        return;
    }
    if (k.done == 0)
    {
        if (k.language != 1 && k.language != 2)
        {
            draw_keys(k, s); // 0x00017210
        }
        draw_bat(k.key_bat, s); // 0x00017110
    }
    else if (k.done == 1)
    {
        draw_bat(k.key_bat, s);
    }
    if (k.prompts != nullptr)
    {
        draw_runtime_dialogue_widget(*k.prompts, 0, s.outer); // 0x00142720
    }
}

// Xbox 0x000113C0
void release_virtual_keyboard(VirtualKeyboard& keyboard, GameSelectControlServices& services)
{
    auto& s = services;
    auto& k = keyboard;
    if (k.key_bat != nullptr)
    {
        activate_actor_bind_tree(*k.key_bat); // 0x00112CC0, ESI the bat
        k.key_bat = nullptr;
    }
    if (k.caret_bat != nullptr)
    {
        activate_actor_bind_tree(*k.caret_bat);
        k.caret_bat = nullptr;
    }
    if (k.prompts != nullptr)
    {
        destroy_dialogue_widget(k.prompts, s.resources, *s.text_pools); // 0x00142070
        k.prompts = nullptr;
    }
    auto device = k.input;
    switch (read_u32(device, 0))
    {
    case 1:
    case 2:
        clear_runtime_input_states(device); // 0x00107E20
        break;
    case 4:
        reset_runtime_composite_input(device, s.input_release); // 0x001079D0
        break;
    case 5:
        clear_runtime_actor_input_states(device); // 0x00107B90
        break;
    default:
        break;
    }
    pop_runtime_input_entry(s.input); // 0x001093C0
    switch (read_u32(device, 0))
    {
    case 1:
    case 2:
    case 3:
        release_runtime_simple_input(device); // 0x00107DD0
        break;
    case 4:
        release_runtime_composite_input(device, s.input_release); // 0x00107960
        break;
    case 5:
        release_runtime_actor_input(device, s.input_release); // 0x00107B60
        break;
    default:
        break;
    }
    s.unmap_input(k.input_address);
    k.input = {};
    k.input_address = 0;
    for (auto*& pool : k.pools)
    {
        release_dialogue_text_pool(pool, *s.text_pools); // 0x0005F460, EBX the pool
        pool = nullptr;
    }
    for (auto* texture : {&k.caret, &k.arrow_left, &k.arrow_right})
    {
        if (texture->data() != nullptr)
        {
            release_animation_resource(s.resources, *texture); // 0x00035C00
            *texture = {};
        }
    }
    k.boards.clear(); // 0x00018E10, then operator delete
}

// Xbox 0x000663E0
void destroy_virtual_keyboard(VirtualKeyboard*& keyboard)
{
    delete keyboard; // HeapFree on the block's heap (0x000F7100): the host heap boundary.
    keyboard = nullptr;
}
}
