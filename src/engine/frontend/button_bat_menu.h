#pragma once
#include "gameselect_rows.h"

#include <functional>
#include <memory>
#include <vector>

namespace gbtg
{
struct Actor;
struct RuntimeDialogueTextPool;

// A slide-in sprite of the button-bat menu (0x2C bytes in retail, 0x00144780): its scale grows from 0 to its full
// scale over `duration` while showing and shrinks back while hiding (0x00144880).
struct ButtonBatMenuSprite
{
    i32 state = 0; // +0, 1 hidden, 2 showing, 3 shown, 4 hiding
    std::span<const u8> texture; // +4
    std::array<float, 2> position{}; // +8
    u32 color = 0; // +0x10, DAT_00314F48
    std::array<float, 2> scale{}; // +0x14, the drawn scale
    std::array<float, 2> full_scale{}; // +0x1C
    float duration = 0.f; // +0x24
    float timer = 0.f; // +0x28, counts down from duration
};

void initialize_menu_sprite(ButtonBatMenuSprite& sprite, const char* name, const std::array<float, 2>& position,
    const std::array<float, 2>& full_scale, float duration,
    GameSelectControlServices& s); // Xbox 0x00144780, EAX the name, ESI the sprite, EBX the full scale
void release_menu_sprite(std::unique_ptr<ButtonBatMenuSprite>& sprite,
    GameSelectControlServices& s); // Xbox 0x00144820, EAX the sprite
void tick_menu_sprite(ButtonBatMenuSprite& sprite, const GameSelectControlServices& s); // Xbox 0x00144880, ECX

// One entry of the button-bat menu (0x23C bytes in retail, 0x00143320). The callbacks get `context` (+0x21C).
struct ButtonBatMenuItem
{
    std::array<char16_t, 0x100> text{}; // +8, lstrcpynW of up to 0xFF characters
    std::function<void(u32 context)> on_show; // +0x208, when the item is laid out (0x00144480)
    std::function<void(u32 context)> on_hide; // +0x20C, when it leaves the visible rows (0x001443F0)
    std::function<void(u32 context)> on_select; // +0x210, when the bat reaches it
    std::function<void(u32 context)> on_deselect; // +0x214, when the selection leaves it
    std::function<void(u32 context)> on_activate; // +0x218, A on the item (0x000E4E40)
    u32 context = 0; // +0x21C
    u32 visible = 0; // +0x220, in the visible rows
    std::array<float, 2> position{}; // +0x224
    u32 record = 0; // +0x22C, the 0x0005F5C0 text record
    std::array<float, 2> icon_offset{}; // +0x230, from the item's position; never written by the translated code
    std::unique_ptr<ButtonBatMenuSprite> icon; // +0x238, none from 0x00143320
};

// The button-bat menu (100 bytes in retail at DAT_005461A4, 0x00143550): a column of text items with the dialogue
// bat as the cursor, scrolling `visible_rows` at a time, with up and down arrows when there are more items.
// Members are in retail order with their retail offsets.
struct ButtonBatMenu
{
    i32 state = 0; // +0, 0 closed, 1 opening, 2 idle, 3 the bat moving, 4 closing
    std::array<float, 2> anchor{}; // +4, the column's centre
    i32 visible_rows = 0; // +0xC
    float spacing = 0.f; // +0x10, between rows
    float text_scale = 0.f; // +0x14
    i32 font = 0; // +0x18
    std::array<float, 3> pool_origin{}; // +0x1C
    u32 wrap = 0; // +0x28, the selection wraps past either end
    u32 owns_items = 0; // +0x2C, 0x00143680 frees the items
    u32 arrows = 0; // +0x30, 0x001439C0 adds the scroll arrows
    std::unique_ptr<ButtonBatMenuSprite> up_arrow; // +0x34, aid_texture_ghoulies_gameselect_bigarrowup
    std::unique_ptr<ButtonBatMenuSprite> down_arrow; // +0x38, aid_texture_ghoulies_gameselect_bigarrowdown
    Actor* bat = nullptr; // +0x3C
    std::array<float, 2> bat_target{}; // +0x40, the selected item's position
    float bat_scale = 0.f; // +0x48, the text scale * DAT_0032B110
    u32 scroll = 0; // +0x4C, 1 up, 2 down, consumed by 0x00144080
    // +0x50: the selected item, as an index into items. -1 is the list head (+0x5C), which 0x00143B10 selects
    // before stepping to the first item and the callbacks skip; -2 is none.
    i32 selected = -2;
    u32 bat_leaves = 0; // +0x54, 0x00143B80 flies the bat off screen; nothing translated sets it
    RuntimeDialogueTextPool* pool = nullptr; // +0x58
    // +0x5C: the item list (its head link; +0x60 is the first item). 0x001439C0 links each new item first, so the
    // order is the reverse of the adds.
    std::vector<std::unique_ptr<ButtonBatMenuItem>> items;
};

// 0x0013B9D0, ECX the bat, EAX its point: the bat flies off the nearer screen edge.
void fly_runtime_bat_off_screen(Actor* bat, const float* point, GameSelectControlServices& services); // Xbox 0x0013B9D0

void create_runtime_button_bat_menu(const std::array<float, 2>& anchor, i32 visible_rows, float spacing,
    float text_scale, i32 font, float origin_x, float origin_y, float origin_z, u32 wrap, u32 owns_items,
    u32 arrows, GameSelectControlServices& services); // Xbox 0x00143550, EAX the anchor
void destroy_runtime_button_bat_menu(GameSelectControlServices& services); // Xbox 0x00143680
std::unique_ptr<ButtonBatMenuItem> make_runtime_button_bat_menu_item(const char16_t* text,
    std::function<void(u32)> on_show, std::function<void(u32)> on_hide, std::function<void(u32)> on_select,
    std::function<void(u32)> on_deselect, std::function<void(u32)> on_activate, u32 context); // Xbox 0x00143320
void add_runtime_button_bat_menu_item(std::unique_ptr<ButtonBatMenuItem> item,
    GameSelectControlServices& services); // Xbox 0x001439C0, EDI the item
void open_runtime_button_bat_menu(GameSelectControlServices& services); // Xbox 0x00143B10
void close_runtime_button_bat_menu(GameSelectControlServices& services); // Xbox 0x00143B80
void tick_runtime_button_bat_menu(GameSelectControlServices& services); // Xbox 0x00143790
void draw_runtime_button_bat_menu(GameSelectControlServices& services); // Xbox 0x00143840
void move_runtime_button_bat_menu_up(GameSelectControlServices& services); // Xbox 0x00143BF0
void move_runtime_button_bat_menu_down(GameSelectControlServices& services); // Xbox 0x00143CB0
// A page at a time: up to the page above (its first item), down to the page below (its last item). When every
// item fits on one page with wrap, they select the first or the last item.
void page_runtime_button_bat_menu_up(GameSelectControlServices& services); // Xbox 0x00143D60
void page_runtime_button_bat_menu_down(GameSelectControlServices& services); // Xbox 0x00143F30
}
