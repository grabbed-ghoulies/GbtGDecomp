#pragma once
#include "gbtg/anim.h"
#include "gbtg/entity.h"
#include "gbtg/object_factory.h"
#include "../frontend/gameselect_rows.h"

#include <array>
#include <span>

namespace gbtg
{
struct RuntimeCutsceneInstance;
struct RuntimeDialogueTextPool;
struct RuntimeDialogueWidget;
struct RuntimeShaderTextureParameter;
// The Gallery (factory type 0x9F, getter 0x00087880, vtable 0x00505688, 0x10B8 bytes): the art book opened from a
// game's features (aid_script_ghoulies_gameselect_unlockableart). Its pages are the aid_texture_ghoulies_
// unlockableart_* rows at 0x00427B60 (stride 0x80), one for each challenge finished, or all 20 once the progress
// bit 0x3D (the secret sequence) is set. The stick turns the pages, Y hides the prompts, and A or B turns back to
// the first page and leaves. Members are in retail order with their retail offsets; the words that were pointers
// are host pointers.
struct GalleryControlState
{
    // +0x24, 0x00087D40: 0 turning forward, 1 back, 2 opening, 3 open, 4 turning back to leave
    i32 state = 0;
    RuntimeCutsceneInstance* cutscene = nullptr; // +0x28, the artbookread* cutscene
    EntityCamNode camera{}; // +0x2C, "unlockableart"
    i32 prompts_made = 0; // +0x8C
    RuntimeShaderTextureParameter* back = nullptr; // +0x90, shader_page3 (the book page texture DAT_005047D8)
    RuntimeShaderTextureParameter* front = nullptr; // +0x94, the current page's shader
    RuntimeShaderTextureParameter* turning = nullptr; // +0x98, the page turning away
    i32 page = 0; // +0x9C
    std::span<const u8> texture; // +0xA0, the current page's art
    std::span<const u8> previous; // +0xA4, the page before it
    RuntimeDialogueWidget* prompts = nullptr; // +0xA8, "lockableArtMenu_basic"
    std::span<u8> input; // +0xAC, the selected-port device (0x00107D60), pushed as "Unlockable Art"
    u32 input_address = 0; // The scene input registry's address for it.
    RuntimeDialogueTextPool* pool = nullptr; // +0xB0
    std::array<char16_t, 0x800> caption{}; // +0xB4, the page's gameselect text
    float speed = 0.f; // +0x10B4, the leaving turns' speed: 1, then one more each page up to 5
};
void gallery_control_object_sizes(ObjectSizes& output); // Xbox 0x00087860
bool construct_runtime_gallery_control(GalleryControlState& control,
    GameSelectControlServices& services); // Xbox 0x000878D0
void destroy_runtime_gallery_control(GalleryControlState& control,
    GameSelectControlServices& services); // Xbox 0x00087900
void tick_runtime_gallery_control(GalleryControlState& control,
    GameSelectControlServices& services); // Xbox 0x000879D0
void draw_runtime_gallery_control(GalleryControlState& control,
    GameSelectControlServices& services); // Xbox 0x00087D20
void set_runtime_gallery_state(GalleryControlState& control, i32 state,
    GameSelectControlServices& services); // Xbox 0x00087D40, EDI the control
// The pages: one for each challenge finished (DAT_004E194C words of 6), or 20 with the progress bit 0x3D.
i32 count_runtime_gallery_pages(const GameSelectControlServices& services); // Xbox 0x000884B0
}
