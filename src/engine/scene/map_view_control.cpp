#include "map_view_control.h"
#include "scene_controls.h"
#include "../entities/player_state.h"
#include "../runtime/name_lookup.h"
#include "../transitions/book_map.h"

#include <cstring>

namespace gbtg
{
namespace
{
float book_position(const RuntimeTransitionBook& book)
{
    if (book.cutscene == nullptr)
    {
        throw Error("map view book has no cutscene at +0x24");
    }
    return book.cutscene->time / book.cutscene->duration;
}
}

// Xbox 0x00083E80
void map_view_control_object_sizes(ObjectSizes& output)
{
    output.instance = 0x30;
    output.parameters = 8;
}

// Xbox 0x00083EF0
bool construct_runtime_map_view_control(MapViewControlState& control, MapViewControlServices& services)
{
    for (const MapViewRoom& row : services.rooms)
    {
        if (!transition_aid_equal(row.room.data(), services.room)) // 0x000357D0, EAX row, EDX DAT_00510E58
        {
            continue;
        }
        RuntimeTransitionBook* book = create_transition_book(row.clip, 1, nullptr, services.books); // 0x00048260
        control.book = book;
        book->holding = 1;
        book->route = 1;
        book->route_argument = 0;
        book->map_from[0] = 0;
        book->map_to[0] = 0;
        const char* spot = services.image_string(row.spot);
        if (spot == nullptr)
        {
            book->map_from[0] = 0;
        }
        else
        {
            std::strncpy(book->map_from.data(), spot, book->map_from.size() - 1);
        }
        book->map_to[0] = 0;
        book->map_menu = 1;
        control.phase = 0;
        change_runtime_actor_pause(u8(0xFFFF), 0, services.pause_groups()); // 0x00117400
        control.retained = 0;
        return true;
    }
    return true;
}

// Xbox 0x00083FC0
void destroy_runtime_map_view_control(MapViewControlState& control, MapViewControlServices& services)
{
    release_transition_book(control.book, services.books); // 0x00048860
    control.book = nullptr;
    change_runtime_actor_pause(u8(0xFFFF), 1, services.pause_groups()); // 0x00117400
}

// Xbox 0x00083FE0
void tick_runtime_map_view_control(MapViewControlState& control, SceneControlInstance& instance,
    SceneControlLists& lists, MapViewControlServices& services)
{
    if (control.book == nullptr)
    {
        throw Error("0x00083FE0 ticks the map view's book through address zero (no map room matched)");
    }
    RuntimeTransitionBook& book = *control.book;
    tick_transition_book(book, services.books); // 0x000483E0
    const auto& image = services.books.image;
    i32 page;
    if (book_position(book) < transition_book_threshold(book, 0x104, image)
        || book_position(book) == transition_book_threshold(book, 0x104, image)) // 0x0042217C
    {
        page = transition_book_row_word(book, 0x10C, image); // 0x00422184
    }
    else if (book_position(book) < transition_book_threshold(book, 0x108, image)
        || book_position(book) == transition_book_threshold(book, 0x108, image)) // 0x00422180
    {
        page = -1;
    }
    else
    {
        page = transition_book_row_word(book, 0x110, image); // 0x00422188
    }
    if (page != -1)
    {
        book.pages.at(size_t(page)).capture = 1; // +0x94 + page * 0x10
    }
    if (control.phase == 0)
    {
        if (book.finished != 0)
        {
            book.holding = 0;
            close_book_map(book, services.books); // 0x00047370, ESI the book
            control.phase = 1;
        }
        return;
    }
    if (control.phase != 1)
    {
        return;
    }
    if (book.finished != 0)
    {
        dispose_runtime_scene_control(instance, lists); // The tail jump to 0x00116140, EAX the control
        return;
    }
    if (control.retained == 0 && book_position(book) > 0.699999988079071f) // DAT_0032B264.
    {
        services.map_retained = 1;
        control.retained = 1;
    }
}
}
