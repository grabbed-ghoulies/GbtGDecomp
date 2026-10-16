#pragma once
#include "native_book.h"

namespace gbtg
{
// The transition book's map route: the run between two spots of the book model's map block (type 0x16), drawn
// over the open spread.
void begin_book_map(RuntimeTransitionBook& book, i32 route, TransitionBookServices& services); // Xbox 0x00046F40
void close_book_map(RuntimeTransitionBook& book, TransitionBookServices& services); // Xbox 0x00047370, ESI the book
void release_book_map(RuntimeTransitionBook& book, TransitionBookServices& services); // Xbox 0x000473D0, ESI the book
u32 tick_book_map(RuntimeTransitionBook& book, TransitionBookServices& services); // Xbox 0x000475B0, EAX the book
void draw_book_map(RuntimeTransitionBook& book, TransitionBookServices& services); // Xbox 0x00047770, EAX the book
}
