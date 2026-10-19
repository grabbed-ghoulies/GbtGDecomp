#include "button_bat_menu.h"
#include "dialogue_draw_context.h"
#include "dialogue_draw_outer.h"
#include "dialogue_text_metrics.h"
#include "dialogue_text_pool.h"
#include "frontend_state.h"
#include "../entities/actor_render_registration.h"
#include "../entities/ghouly_spawn_record.h"
#include "../scene/executable_resources.h"
#include "gbtg/entity.h"
#include "gbtg/resource_error.h"

#include <algorithm>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
constexpr u32 kAid = 0x5F646961u; // "aid_"

float exe_real(const GameSelectControlServices& s, u32 address)
{
    return std::bit_cast<float>(read_u32(s.image.memory(address, 4), 0));
}

u32 exe_word(const GameSelectControlServices& s, u32 address)
{
    return read_u32(s.image.memory(address, 4), 0);
}

constexpr i32 kNoItem = -2; // a null item pointer
constexpr i32 kListHead = -1; // the list head (+0x5C), which 0x00143B10 selects first

ButtonBatMenu& menu_of(GameSelectControlServices& s)
{
    if (s.frontend.bat_menu == nullptr)
    {
        throw Error("the button-bat menu (DAT_005461A4) is not open");
    }
    return *s.frontend.bat_menu;
}

i32 item_count(const ButtonBatMenu& menu) // 0x0002CC10, EAX the list
{
    return i32(menu.items.size());
}

// 0x0002CCB0, EAX the list, EDX the index: the item at an index, or none past the end.
i32 item_at(const ButtonBatMenu& menu, i32 index)
{
    return index < item_count(menu) ? index : kNoItem;
}

ButtonBatMenuItem* item(ButtonBatMenu& menu, i32 index)
{
    return 0 <= index && index < item_count(menu) ? menu.items[size_t(index)].get() : nullptr;
}

// A sprite starts showing (2) or hiding (4) from where it is: its timer becomes the time left of the other way.
void turn_sprite(ButtonBatMenuSprite& sprite, i32 state)
{
    sprite.state = state;
    sprite.timer = sprite.duration - sprite.timer;
}

// Xbox 0x00144780, EAX the texture name, ESI the sprite, EBX its full scale
void initialize_menu_sprite(ButtonBatMenuSprite& sprite, const char* name, const std::array<float, 2>& position,
    const std::array<float, 2>& full_scale, float duration, GameSelectControlServices& s)
{
    sprite.color = exe_word(s, 0x00314F48);
    sprite.position = {0.f, 0.f};
    sprite.scale = {0.f, 0.f};
    sprite.texture = {};
    std::span<const u8> texture;
    if (read_u32({reinterpret_cast<const u8*>(name), 4}, 0) == kAid)
    {
        texture = acquire_animation_resource(s.resources, name); // 0x00035E90
        if (texture.data() == nullptr)
        {
            resource_error(s.error, 2); // 0x00032700(2)
        }
    }
    sprite.texture = texture;
    sprite.scale = {0.f, 0.f};
    sprite.position = position;
    sprite.full_scale = full_scale;
    sprite.duration = duration;
    sprite.timer = 0.f;
    sprite.state = 1;
}

// Xbox 0x00144820, EAX the sprite
void release_menu_sprite(std::unique_ptr<ButtonBatMenuSprite>& sprite, GameSelectControlServices& s)
{
    release_animation_resource(s.resources, sprite->texture); // 0x00035C00
    sprite->texture = {};
    sprite.reset(); // HeapFree on the thread's heap (0x000F7100).
}

// One axis of a showing sprite: the fraction of the time gone toward the full scale, clamped between 0 and it.
float showing_scale(float timer, float duration, float full)
{
    float value = full;
    if (duration != 0.f)
    {
        const float grown = ((timer - duration) / (0.f - duration)) * full;
        if (full <= 0.f)
        {
            if (full <= grown)
            {
                value = grown;
                if (0.f < grown)
                {
                    value = 0.f;
                }
            }
        }
        else if (grown <= full)
        {
            value = grown;
            if (grown < 0.f)
            {
                value = 0.f;
            }
        }
    }
    return value;
}

// One axis of a hiding sprite: from the full scale back toward 0, clamped between 0 and it.
float hiding_scale(float timer, float duration, float full)
{
    if (duration == 0.f)
    {
        return 0.f;
    }
    float value = ((timer - duration) / (0.f - duration)) * (0.f - full) + full;
    if (0.f <= full)
    {
        if (value < 0.f)
        {
            return 0.f;
        }
        if (full < value)
        {
            value = full;
        }
    }
    else
    {
        if (0.f < value)
        {
            return 0.f;
        }
        if (value < full)
        {
            value = full;
        }
    }
    return value;
}

// Xbox 0x00144880, ECX the sprite: a showing sprite grows to its full scale, a hiding one shrinks to nothing.
void tick_menu_sprite(ButtonBatMenuSprite& sprite, const GameSelectControlServices& s)
{
    if (sprite.state == 2)
    {
        if (0.f < sprite.timer)
        {
            sprite.timer -= s.dt; // DAT_005096C0
        }
        if (sprite.timer <= 0.f)
        {
            sprite.state = 3;
            sprite.scale = sprite.full_scale;
            sprite.timer = 0.f;
            return;
        }
        sprite.scale[0] = showing_scale(sprite.timer, sprite.duration, sprite.full_scale[0]);
        sprite.scale[1] = showing_scale(sprite.timer, sprite.duration, sprite.full_scale[1]);
        return;
    }
    if (sprite.state != 4)
    {
        return;
    }
    if (0.f < sprite.timer)
    {
        sprite.timer -= s.dt;
    }
    if (sprite.timer <= 0.f)
    {
        sprite.state = 1;
        sprite.scale = {0.f, 0.f};
        sprite.timer = 0.f;
        return;
    }
    sprite.scale[0] = hiding_scale(sprite.timer, sprite.duration, sprite.full_scale[0]);
    sprite.scale[1] = hiding_scale(sprite.timer, sprite.duration, sprite.full_scale[1]);
}

// Xbox 0x001434E0, EAX the item
void release_menu_item(std::unique_ptr<ButtonBatMenuItem>& entry, GameSelectControlServices& s)
{
    if (entry->context != 0)
    {
        // 0x000663E0: HeapFree of the context block on its heap (the host heap boundary). A host item holds the
        // block's value (0x000E3560's scene ordinal) rather than its address, so there is no block to free.
        entry->context = 0;
    }
    if (entry->icon != nullptr)
    {
        release_menu_sprite(entry->icon, s); // 0x00144820
    }
    entry.reset(); // HeapFree on the thread's heap (0x000F7100).
}

void call(const std::function<void(u32)>& callback, const ButtonBatMenuItem& entry)
{
    if (callback)
    {
        callback(entry.context);
    }
}

// Xbox 0x00144660: the selected item's deselect callback (+0x214).
void deselect_item(ButtonBatMenu& menu)
{
    if (const auto* entry = item(menu, menu.selected); entry != nullptr)
    {
        call(entry->on_deselect, *entry);
    }
}

// Xbox 0x00144630: the selected item's select callback (+0x210).
void select_item(ButtonBatMenu& menu)
{
    if (const auto* entry = item(menu, menu.selected); entry != nullptr)
    {
        call(entry->on_select, *entry);
    }
}

// Xbox 0x00144320: the bat flies to the selected item; the menu waits for it (3).
void fly_bat_to_selection(ButtonBatMenu& menu, GameSelectControlServices& s)
{
    const auto& entry = *item(menu, menu.selected);
    menu.bat_target = entry.position;
    s.fly_bat(*menu.bat, ButtonBatPoint{menu.bat_target[0], menu.bat_target[1]}, 0.f); // 0x0006DA00, ESI EBX
    menu.state = 3;
}

// Xbox 0x001443F0: every visible item's text ends and its icon hides; none stays visible.
void hide_items(ButtonBatMenu& menu, GameSelectControlServices& s)
{
    end_all_dialogue_text_records(*menu.pool, s.fonts, s.fallback_texture, s.random); // 0x0005F970
    for (auto& entry : menu.items)
    {
        if (entry->visible != 0)
        {
            if (entry->icon != nullptr)
            {
                turn_sprite(*entry->icon, 4);
            }
            call(entry->on_hide, *entry);
        }
        entry->visible = 0;
        entry->record = 0;
        entry->position = {0.f, 0.f};
    }
}

// Xbox 0x00144480, EAX the first item: up to visible_rows items from it, centred on the anchor.
void lay_out_items(ButtonBatMenu& menu, i32 first, GameSelectControlServices& s)
{
    const i32 rows = std::min(item_count(menu), menu.visible_rows);
    const float line = runtime_text_line_height(s.metrics, menu.font, menu.text_scale); // the 0x000606D0 inline
    const float x = menu.anchor[0];
    float y = (menu.anchor[1] - (float(rows - 1) * menu.spacing + float(rows) * line) * 0.5f) + line * 0.5f;
    i32 index = first;
    for (i32 row = 0; row < rows; ++row)
    {
        auto* entry = item(menu, index);
        if (entry == nullptr)
        {
            return;
        }
        entry->visible = 1;
        entry->position = {x, y};
        const float spacing = menu.spacing;
        if (entry->icon != nullptr)
        {
            entry->icon->position = {entry->icon_offset[0] + entry->position[0], entry->icon_offset[1] + y};
            turn_sprite(*entry->icon, 2);
        }
        call(entry->on_show, *entry);
        ++index;
        y = spacing + y + line;
    }
}

// Xbox 0x00144360: a text record for each visible item.
void create_item_records(ButtonBatMenu& menu, GameSelectControlServices& s)
{
    for (auto& entry : menu.items)
    {
        if (entry->visible != 0)
        {
            entry->record = create_dialogue_text_record(*menu.pool, entry->text.data(), entry->position[0],
                entry->position[1], menu.text_scale, menu.text_scale, 1.f, 1, 0, 1, s.metrics, s.viewport_width,
                s.next_text_id, s.random); // 0x0005F5C0, EAX 1
        }
    }
}

// Xbox 0x00144690: whether the last item is in the visible rows.
bool last_item_visible(ButtonBatMenu& menu)
{
    return item(menu, item_at(menu, item_count(menu) - 1))->visible != 0;
}

// Xbox 0x001446F0: without wrap, the arrows hide at the ends of the list and show away from them.
void update_arrows(ButtonBatMenu& menu)
{
    if (menu.wrap != 0)
    {
        return;
    }
    if (auto* down = menu.down_arrow.get(); down != nullptr)
    {
        if (last_item_visible(menu))
        {
            turn_sprite(*down, 4);
        }
        else if (down->state == 1)
        {
            turn_sprite(*down, 2);
        }
    }
    if (auto* up = menu.up_arrow.get(); up != nullptr)
    {
        if (menu.items.front()->visible != 0)
        {
            turn_sprite(*up, 4);
            return;
        }
        if (up->state == 1)
        {
            turn_sprite(*up, 2);
        }
    }
}

// Xbox 0x00144270: the next page from the item after the selection (the first with wrap), and that item selected.
void page_down(ButtonBatMenu& menu, GameSelectControlServices& s)
{
    const i32 next = menu.selected + 1;
    if (menu.wrap == 0 && next >= item_count(menu))
    {
        return;
    }
    hide_items(menu, s); // 0x001443F0
    i32 first = next < item_count(menu) ? next : kNoItem;
    if (menu.wrap != 0 && first == kNoItem)
    {
        first = item_count(menu) == 0 ? kNoItem : 0;
    }
    lay_out_items(menu, first, s); // 0x00144480, EAX the first
    create_item_records(menu, s); // 0x00144360
    deselect_item(menu);
    menu.selected = first;
    fly_bat_to_selection(menu, s); // 0x00144320
    select_item(menu);
}

// Xbox 0x00144180: the page ending above the selection (the last page with wrap at the first item).
void page_up(ButtonBatMenu& menu, GameSelectControlServices& s)
{
    if (menu.wrap == 0 && menu.selected <= 0)
    {
        return;
    }
    hide_items(menu, s); // 0x001443F0
    i32 first = menu.selected;
    if (menu.wrap == 0 || 0 < menu.selected)
    {
        // Up to visible_rows items back, stopping at the first.
        for (i32 step = 1; step <= menu.visible_rows; ++step)
        {
            if (0 <= menu.selected - step)
            {
                first = menu.selected - step;
            }
        }
    }
    else
    {
        const i32 count = item_count(menu);
        i32 last_page = count % menu.visible_rows;
        if (last_page == 0)
        {
            last_page = menu.visible_rows;
        }
        first = item_at(menu, count - 1);
        if (1 < last_page)
        {
            first -= last_page - 1;
        }
    }
    lay_out_items(menu, first, s); // 0x00144480, EAX the first
    create_item_records(menu, s); // 0x00144360
    deselect_item(menu);
}

// Xbox 0x00144080: a pending scroll (+0x4C) turns the page and moves the selection; the menu waits for the bat.
void scroll_menu(ButtonBatMenu& menu, GameSelectControlServices& s)
{
    if (menu.scroll == 1)
    {
        page_up(menu, s); // 0x00144180
        const i32 last = item_at(menu, item_count(menu) - 1);
        if (menu.wrap == 0 || 0 < menu.selected)
        {
            menu.selected -= 1;
        }
        else
        {
            menu.selected = last;
        }
        fly_bat_to_selection(menu, s); // 0x00144320
        select_item(menu);
        update_arrows(menu); // 0x001446F0
        menu.scroll = 0;
        menu.state = 3;
        return;
    }
    if (menu.scroll == 2)
    {
        page_down(menu, s); // 0x00144270
        update_arrows(menu); // 0x001446F0
        menu.scroll = 0;
        menu.state = 3;
        return;
    }
    menu.scroll = 0;
    menu.state = 3;
}

// Xbox 0x00144030: whether every visible item's text has finished appearing.
bool items_revealed(ButtonBatMenu& menu, GameSelectControlServices& s)
{
    for (const auto& entry : menu.items)
    {
        if (entry->visible != 0
            && dialogue_text_record_animating(*menu.pool, entry->record, s.fonts,
                s.fallback_texture) != 0) // 0x0005FFA0
        {
            return false;
        }
    }
    return true;
}
}

// Xbox 0x0013B9D0
void fly_runtime_bat_off_screen(Actor* bat, const float* point, GameSelectControlServices& services)
{
    auto& s = services;
    const float width = float(i32(s.screen[0])); // DAT_005096E8
    const float height = float(i32(s.screen[1])); // DAT_005096EA
    const float half = exe_real(s, 0x0032B0F4);
    ButtonBatPoint target{};
    target.x = width - point[0] > width * half ? width + exe_real(s, 0x0032B1E0) : exe_real(s, 0x0032B304);
    target.y = height - point[1] > height * half ? height + exe_real(s, 0x0032B1E0) : exe_real(s, 0x0032B304);
    if (bat != nullptr)
    {
        s.fly_bat(*bat, target, 1.f); // 0x0006DA00
    }
}

// Xbox 0x00143550
void create_runtime_button_bat_menu(const std::array<float, 2>& anchor, i32 visible_rows, float spacing,
    float text_scale, i32 font, float origin_x, float origin_y, float origin_z, u32 wrap, u32 owns_items,
    u32 arrows, GameSelectControlServices& services)
{
    auto& s = services;
    s.frontend.bat_menu = new ButtonBatMenu(); // 0x000F72D0(100)
    auto& menu = *s.frontend.bat_menu;
    menu.anchor = anchor;
    menu.visible_rows = visible_rows;
    menu.spacing = spacing;
    menu.text_scale = text_scale;
    menu.font = font;
    menu.pool_origin = {origin_x, origin_y, origin_z};
    menu.wrap = wrap;
    menu.owns_items = owns_items;
    menu.arrows = arrows;
    menu.up_arrow = nullptr;
    menu.down_arrow = nullptr;
    menu.bat_target = {0.f, 0.f};
    menu.bat_scale = text_scale * exe_real(s, 0x0032B110);
    menu.bat = s.create_bat(menu.bat_target, 0.f, 0.2f); // 0x0013B580, EAX +0x40
    if (menu.bat->scale_200 != menu.bat_scale)
    {
        menu.bat->scale_200 = menu.bat_scale;
        menu.bat->xform_valid = 0;
    }
    menu.selected = kNoItem;
    menu.scroll = 0;
    menu.bat_leaves = 0;
    menu.items.clear();
    allocate_dialogue_text_pool(menu.pool, menu.font, 1, 0x400C0, s.text_pools); // 0x0005F390
    menu.pool->origin = menu.pool_origin;
    menu.pool->end_time = exe_real(s, 0x0032B0F4);
    menu.state = 0;
}

// Xbox 0x00143680
void destroy_runtime_button_bat_menu(GameSelectControlServices& services)
{
    auto& s = services;
    auto& menu = menu_of(s);
    release_dialogue_text_pool(menu.pool, *s.text_pools); // 0x0005F460
    menu.pool = nullptr;
    if (menu.owns_items != 0)
    {
        for (auto& entry : menu.items)
        {
            release_menu_item(entry, s); // 0x001434E0
        }
    }
    if (menu.up_arrow != nullptr)
    {
        release_menu_sprite(menu.up_arrow, s); // 0x00144820
    }
    if (menu.down_arrow != nullptr)
    {
        release_menu_sprite(menu.down_arrow, s);
    }
    activate_actor_bind_tree(*menu.bat); // 0x00112D10, message 0x80 and 0x0003ED70: the 0x00112CC0 inline
    menu.bat = nullptr;
    delete s.frontend.bat_menu; // HeapFree on the thread's heap (0x000F7100)
    s.frontend.bat_menu = nullptr; // DAT_005461A4
}

// Xbox 0x00143320
std::unique_ptr<ButtonBatMenuItem> make_runtime_button_bat_menu_item(const char16_t* text,
    std::function<void(u32)> on_show, std::function<void(u32)> on_hide, std::function<void(u32)> on_select,
    std::function<void(u32)> on_deselect, std::function<void(u32)> on_activate, u32 context)
{
    auto entry = std::make_unique<ButtonBatMenuItem>(); // 0x000F72D0(0x23C)
    // lstrcpynW(+8, text, 0xFF): at most 0xFE characters and the terminator.
    size_t length = 0;
    while (length < 0xFE && text[length] != 0)
    {
        entry->text[length] = text[length];
        ++length;
    }
    entry->text[length] = 0;
    entry->on_hide = std::move(on_hide);
    entry->on_show = std::move(on_show);
    entry->on_select = std::move(on_select);
    entry->on_activate = std::move(on_activate);
    entry->on_deselect = std::move(on_deselect);
    entry->context = context;
    entry->position = {0.f, 0.f};
    entry->visible = 0;
    entry->record = 0;
    entry->icon = nullptr;
    return entry;
}

// Xbox 0x001439C0
void add_runtime_button_bat_menu_item(std::unique_ptr<ButtonBatMenuItem> entry, GameSelectControlServices& services)
{
    auto& s = services;
    auto& menu = menu_of(s);
    menu.items.insert(menu.items.begin(), std::move(entry)); // linked first, after the head
    if (menu.arrows != 0 && menu.up_arrow == nullptr && menu.down_arrow == nullptr
        && menu.visible_rows < item_count(menu)) // 0x0002CC10
    {
        const float line = runtime_text_line_height(s.metrics, menu.font, menu.text_scale); // 0x000606D0
        const float half = (float(menu.visible_rows - 1) * menu.spacing + float(menu.visible_rows) * line) * 0.5f;
        const std::array<float, 2> up{menu.anchor[0], ((menu.anchor[1] - half) - menu.spacing) + line * 0.5f};
        const std::array<float, 2> down{menu.anchor[0], menu.spacing + menu.anchor[1] + line * 0.5f + half};
        const std::array<float, 2> full{menu.text_scale, menu.text_scale};
        menu.up_arrow = std::make_unique<ButtonBatMenuSprite>(); // 0x000F72D0(0x2C)
        initialize_menu_sprite(*menu.up_arrow, s.image.string(0x003157D0), up, full, 0.5f, s); // 0x00144780
        menu.down_arrow = std::make_unique<ButtonBatMenuSprite>();
        initialize_menu_sprite(*menu.down_arrow, s.image.string(0x00315850), down, full, 0.5f, s);
    }
}

// Xbox 0x00143B10
void open_runtime_button_bat_menu(GameSelectControlServices& services)
{
    auto& menu = menu_of(services);
    if (menu.state != 0)
    {
        return;
    }
    menu.state = 1;
    menu.selected = kListHead;
    page_down(menu, services); // 0x00144270
    if (menu.wrap != 0 && menu.up_arrow != nullptr)
    {
        turn_sprite(*menu.up_arrow, 2);
    }
    if (menu.down_arrow != nullptr)
    {
        turn_sprite(*menu.down_arrow, 2);
    }
}

// Xbox 0x00143B80
void close_runtime_button_bat_menu(GameSelectControlServices& services)
{
    auto& menu = menu_of(services);
    menu.state = 4;
    hide_items(menu, services); // 0x001443F0
    if (menu.bat_leaves != 0)
    {
        fly_runtime_bat_off_screen(menu.bat, menu.bat_target.data(), services); // 0x0013B9D0, ECX EAX
    }
    if (menu.up_arrow != nullptr)
    {
        turn_sprite(*menu.up_arrow, 4);
    }
    if (menu.down_arrow != nullptr)
    {
        turn_sprite(*menu.down_arrow, 4);
    }
}

// Xbox 0x00143790
void tick_runtime_button_bat_menu(GameSelectControlServices& services)
{
    auto& s = services;
    auto& menu = menu_of(s);
    if (s.paused != 0 && menu.bat != nullptr) // DAT_00510CE8
    {
        s.update_actor(*menu.bat); // Vtable +0x10
    }
    bool settled = false;
    if (menu.state == 1)
    {
        settled = true;
    }
    else if (menu.state == 3)
    {
        const i32 bat = s.bat_screen(*menu.bat).state; // +0x934
        settled = bat == 1 || bat == 3;
    }
    else if (menu.state == 4 && menu.pool->allocation->cursor <= menu.pool->allocation->begin)
    {
        menu.state = 0; // the pool's records are all gone
    }
    if (settled && items_revealed(menu, s)) // 0x00144030
    {
        menu.state = 2;
    }
    if (menu.state != 0)
    {
        for (auto& entry : menu.items)
        {
            if (entry->icon != nullptr)
            {
                tick_menu_sprite(*entry->icon, s); // 0x00144880
            }
        }
    }
    if (menu.up_arrow != nullptr)
    {
        tick_menu_sprite(*menu.up_arrow, s);
    }
    if (menu.down_arrow != nullptr)
    {
        tick_menu_sprite(*menu.down_arrow, s);
    }
}

// Xbox 0x00143840
void draw_runtime_button_bat_menu(GameSelectControlServices& services)
{
    auto& s = services;
    auto& menu = menu_of(s);
    if (menu.state == 0)
    {
        return;
    }
    for (auto* arrow : {menu.up_arrow.get(), menu.down_arrow.get()})
    {
        if (arrow != nullptr && arrow->texture.data() != nullptr && (arrow->scale[0] != 0.f || arrow->scale[1] != 0.f))
        {
            draw_runtime_dialogue_sprite(arrow->texture.data(), arrow->position.data(), arrow->scale.data(), 0.f,
                arrow->color, -1, s.draw); // 0x0005CE30
        }
    }
    if (menu.bat != nullptr && s.dialogue_page_hold() == 0) // DAT_004EE1EC
    {
        s.draw_actor(*menu.bat, i32(s.outer.current_display_address - 0x507018u) / 0x1E4, 3); // Vtable +0x14
    }
    for (auto& entry : menu.items)
    {
        auto* icon = entry->icon.get();
        if (icon != nullptr && icon->texture.data() != nullptr && (icon->scale[0] != 0.f || icon->scale[1] != 0.f))
        {
            auto& draw = s.draw;
            begin_dialogue_sprite_draw(draw); // 0x0005CDC0
            draw_dialogue_texture(-1, nullptr, const_cast<u8*>(icon->texture.data()), icon->scale.data(), nullptr,
                0.f, icon->color, icon->position.data(), draw); // 0x0005CA60
            --draw.override_count; // DAT_00509B5C
            if (draw.override_count == 0)
            {
                draw.fog_disabled = 0; // DAT_00509B00
            }
        }
    }
}

// Xbox 0x00143BF0
void move_runtime_button_bat_menu_up(GameSelectControlServices& services)
{
    auto& s = services;
    auto& menu = menu_of(s);
    if (item_count(menu) == 1 || menu.state != 2)
    {
        return;
    }
    if (menu.selected <= 0)
    {
        // The first item (its previous link is the head).
        if (menu.wrap == 0)
        {
            return;
        }
        if (item_count(menu) <= menu.visible_rows)
        {
            deselect_item(menu); // 0x00144660
            menu.selected = item_at(menu, item_count(menu) - 1); // 0x0002CCB0
            fly_bat_to_selection(menu, s); // 0x00144320
            select_item(menu); // 0x00144630
            return;
        }
    }
    else if (item(menu, menu.selected - 1)->visible != 0)
    {
        deselect_item(menu);
        menu.selected -= 1;
        fly_bat_to_selection(menu, s);
        select_item(menu);
        return;
    }
    menu.scroll = 1;
    scroll_menu(menu, s); // 0x00144080
}

// Xbox 0x00143CB0
void move_runtime_button_bat_menu_down(GameSelectControlServices& services)
{
    auto& s = services;
    auto& menu = menu_of(s);
    if (item_count(menu) == 1 || menu.state != 2)
    {
        return;
    }
    const i32 next = menu.selected + 1;
    if (next >= item_count(menu))
    {
        if (menu.wrap == 0)
        {
            return;
        }
        if (item_count(menu) <= menu.visible_rows)
        {
            deselect_item(menu); // 0x00144660
            menu.selected = 0; // +0x60, the first item
            fly_bat_to_selection(menu, s); // 0x00144320
            select_item(menu); // 0x00144630
            return;
        }
    }
    else if (item(menu, next)->visible != 0)
    {
        deselect_item(menu);
        menu.selected = next;
        fly_bat_to_selection(menu, s);
        select_item(menu);
        return;
    }
    menu.scroll = 2;
    scroll_menu(menu, s); // 0x00144080
}

// Xbox 0x00143D60. The item lists are walked through their previous links (+0); the head (-1) ends a walk.
void page_runtime_button_bat_menu_up(GameSelectControlServices& services)
{
    auto& s = services;
    auto& menu = menu_of(s);
    const i32 count = item_count(menu); // the inline 0x0002CC10
    if (count == 1 || menu.state != 2)
    {
        return;
    }
    i32 top = menu.selected;
    if (menu.wrap != 0)
    {
        if (count <= menu.visible_rows) // 0x0002CC10
        {
            if (menu.selected == 0)
            {
                return; // its previous link is the head
            }
            deselect_item(menu); // 0x00144660
            menu.selected = 0; // +0x60, the first item
            fly_bat_to_selection(menu, s); // 0x00144320
            select_item(menu); // The tail jump to 0x00144630.
            return;
        }
        // The topmost of the visible items above the selection.
        for (i32 above = top - 1; 0 <= above && item(menu, above)->visible != 0; --above)
        {
            top = above;
        }
    }
    else
    {
        i32 above = top - 1;
        if (above < 0)
        {
            return;
        }
        // Up through the visible items; one that is not visible pages up from the last visible one, and reaching
        // the first item with every item visible leaves the menu as it is.
        while (item(menu, above)->visible != 0)
        {
            top = above;
            --above;
            if (above < 0)
            {
                return;
            }
        }
    }
    menu.selected = top;
    // 0x00143E30: the page above, its last item then the topmost visible one selected. +0x4C stays 1.
    menu.scroll = 1;
    page_up(menu, s); // 0x00144180
    if (menu.wrap != 0 && menu.selected == 0)
    {
        menu.selected = item_at(menu, count - 1); // the inline 0x0002CC10 / 0x0002CCB0
    }
    else
    {
        menu.selected -= 1; // its previous link
    }
    for (i32 above = menu.selected - 1; 0 <= above && item(menu, above)->visible != 0; --above)
    {
        menu.selected = above;
    }
    fly_bat_to_selection(menu, s); // +0x40 from +0x224, 0x0006DA00(0), state 3
    select_item(menu); // +0x210 with +0x21C, unless the selection is the head
    update_arrows(menu); // The tail jump to 0x001446F0.
}

// Xbox 0x00143F30. The item lists are walked through their next links (+4); null ends a walk.
void page_runtime_button_bat_menu_down(GameSelectControlServices& services)
{
    auto& s = services;
    auto& menu = menu_of(s);
    const i32 count = item_count(menu); // the inline 0x0002CC10
    if (count == 1 || menu.state != 2)
    {
        return;
    }
    if (menu.wrap != 0)
    {
        if (count <= menu.visible_rows) // 0x0002CC10
        {
            if (count <= menu.selected + 1)
            {
                return;
            }
            deselect_item(menu); // 0x00144660
            menu.selected = count - 1; // the last item
            fly_bat_to_selection(menu, s); // 0x00144320
            select_item(menu); // The tail jump to 0x00144630.
            return;
        }
        // The last of the visible items below the selection, then the page below.
        i32 last = menu.selected;
        for (i32 below = last + 1; below < count && item(menu, below)->visible != 0; ++below)
        {
            last = below;
        }
        menu.selected = last;
        menu.scroll = 2;
        scroll_menu(menu, s); // The tail jump to 0x00144080.
        return;
    }
    // Down through the visible items; the first that is not visible pages down from the one above it.
    for (i32 below = menu.selected + 1; below < count; ++below)
    {
        if (item(menu, below)->visible == 0)
        {
            menu.selected = below - 1; // its previous link
            menu.scroll = 2;
            scroll_menu(menu, s); // 0x00144080
            update_arrows(menu); // The tail jump to 0x001446F0.
            return;
        }
    }
}
}
