#pragma once
#include "cell_registration.h"

namespace gbtg::entity_detail
{
// Retail 0x34-byte search state. Node rows are 0x14 bytes with their u16 index at +0xA.
struct NavigationPathSearch
{
    u32 word_0 = 0;
    u32 word_4 = 0;
    u32 count = 0; // +8.
    u8* nodes = nullptr; // +0xC.
    const NavGrid* grid = nullptr; // +0x10.
    void* scratch = nullptr; // +0x14, engine heap block.
    i32 first = 0; // +0x18.
    i32 last = 0; // +0x1C.
    u32 word_20 = 0;
    u32 word_24 = 0;
    u32 word_28 = 0;
    u32 word_2c = 0;
    u32 word_30 = 0;
};
void reset_navigation_path_search(NavigationPathSearch& search, u8* nodes, const NavGrid* grid,
    u32 count); // Xbox 0x00036AF0, ESI search, EBX count
u32 update_navigation_floor(const Vec3& point, i32& floor, NavigationCellRegistration*& records,
    NavigationPathSearch* search, i32 preferred, i32 check_cell); // Xbox 0x00110C90, ECX/EAX last two
}
