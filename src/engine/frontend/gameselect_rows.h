#pragma once
#include "gbtg/anim.h"
#include "gbtg/model_relocation.h"
#include "gbtg/resource_error.h"
#include "input_activation.h"
#include "../saves/runtime_profile.h"
#include "../saves/save_games.h"

#include <functional>
#include <memory>

namespace gbtg
{
// A text line inside a game-select widget (0x20C bytes in retail): wide text, the position it is centred on, and
// the id 0x0005F5C0 returns for it. The slot-select prompt and plate labels, the deletion title and prompt, and
// the contents chapter names, title, progress and completed lines all use this layout. Treating it as one shared
// struct is inferred from that.
struct GameSelectTextLine
{
    std::array<char16_t, 0x100> text{}; // +0
    float x = 0.f; // +0x200
    float y = 0.f; // +0x204
    u32 record = 0; // +0x208, 0x0005F5C0's text record id
};

// A sprite inside a game-select widget (0x18 bytes in retail): its texture, then where, how and in what color it
// is drawn. The draw calls 0x0005CA60 / 0x0005CE30 take the position and scale by address. Slot select's plates,
// discs and backgrounds, and contents' chapters, backgrounds, splat and challenge icon, all use this layout.
struct GameSelectSprite
{
    std::span<const u8> texture; // +0
    std::array<float, 2> position{}; // +4
    u32 color = 0; // +0xC, alpha in byte 3
    std::array<float, 2> scale{}; // +0x10
};

// A color word's alpha (byte 3), which the retail code reads and writes on its own.
inline u8 color_alpha(u32 color)
{
    return u8(color >> 24);
}

inline void set_color_alpha(u32& color, u8 alpha)
{
    color = (color & 0x00FFFFFFu) | (u32(alpha) << 24);
}

struct RuntimeDisplaySetupContext;
struct AudioBankStreams;
struct FrontendPauseDitty;
struct GameSelectControlState;
struct RuntimeFrontendState;
struct RuntimeMovieStartupContext;
struct RuntimeHudLayout;
struct RuntimeTextMetrics;
struct RuntimeTextFont;
struct RuntimeDialogueDrawContext;
struct RuntimeDialogueOuterDrawContext;
struct RuntimeDialogueWidget;
struct GhoulySpawnPool;
struct Actor;
struct ButtonBatScreen;
struct ButtonBatPoint;
struct RuntimeDialogueWidgetContext;
struct DialogueWidgetAllocationContext;
struct LocalTextResourceContext;
class SceneExecutableResources;
struct CutsceneRuntimeContext;
struct AttractControlServices;
struct AudioEventVoices;
struct TransitionBookServices;
struct RuntimeDialogueInputDevice;
// A front-end widget instance (a mode row's create). Each row's state derives from this.
struct GameSelectWidget
{
    virtual ~GameSelectWidget() = default;
};
using GameSelectWidgetHandle = std::unique_ptr<GameSelectWidget>;
// 0x000533A0 / 0x00053450: the single-frame texture object (0x30 bytes, header at +0x1C) in the scene's
// resource memory, so the presenter decodes it like any texture resource.
struct RuntimeScreenTextureBoundaries
{
    std::function<RuntimeModelAllocation(u32 size)> allocate; // 0x000F72D0
    std::function<void(u32 address)> free; // HeapFree
    std::function<RuntimeModelAllocation(u32 size, u32 alignment)> allocate_physical; // XPhysicalAlloc
    std::function<void(u32 address)> free_physical; // XPhysicalFree
    // XGSetTextureHeader: records the header and returns the data size.
    std::function<u32(u32 width, u32 height, u32 levels, u32 usage, u32 format, u32 pool, u32 texture)> header;
    std::function<void(u32 resource, u32 data)> register_resource; // D3DResource_Register
    std::function<u32(const u8* bytes)> address_of; // The native address of an object's bytes.
};
// What the game-select control and its widgets reach outside their objects. SceneActorServices binds it when
// the native scene starts.
struct GameSelectControlServices
{
    AnimationResourceRegistry& resources;
    ResourceErrorState& error;
    RuntimeProfile& profile; // DAT_00506C6C..DAT_00506C90
    u32& flags; // DAT_0050EA9C
    std::span<const u8>& text; // DAT_005462C4, aid_loctext_ghoulies_gameselect_gameselecttext
    const char* scene_order; // *DAT_00430CF8
    std::span<const u8>& scene_order_held; // DAT_00511B64
    u32& scene_order_release; // DAT_00511B68: 0, or 0x00035940 (release the held resource)
    i32& pause_count; // DAT_00510CEC
    RuntimeInputActivationContext& input;
    RuntimeInputReleaseContext& input_release;
    u32 frontend_input; // DAT_00429BBC, the keyboard device 0x000E5A80 creates at boot
    const u32& frame; // DAT_004DEA58
    i32& next_row; // DAT_00429BC0
    float& stream_wait; // DAT_005065C4
    float stream_wait_seconds; // DAT_0032B184
    std::function<i32(i32 id)> stream_busy; // 0x00039D50(id, 0, 0)
    AudioBankStreams& streams;
    FrontendPauseDitty& ditty; // DAT_00429BB8 / DAT_00429B38
    RuntimeScreenTextureBoundaries textures;
    RuntimeDisplaySetupContext& display;
    const float& freeze; // DAT_004312DC
    const float& dt; // DAT_005096C0
    const float& scene_scale; // DAT_004312C4
    const SceneExecutableResources& image; // The executable's .data tables and strings.
    RuntimeFrontendState& frontend; // The 0x000E5A80 globals, the movies and DAT_005065B8.
    RuntimeMovieStartupContext& movies; // The boot movie list.
    const std::array<u16, 2>& screen; // DAT_005096E8 / DAT_005096EA
    const RuntimeHudLayout& hud; // DAT_00509708..
    char16_t* selected_game; // DAT_00429BC4, 0x80 wide characters.
    i32* plate_flags; // DAT_00510F7C / DAT_00510F80
    const i32& transition; // DAT_0054632C, the scene transition state.
    GhoulySpawnPool*& text_pools; // DAT_005043CC, the 0x0005F390 registry.
    const RuntimeTextMetrics& metrics;
    float viewport_width;
    i32& next_text_id;
    u32& random; // DAT_0054DD7C
    std::span<const RuntimeTextFont* const> fonts;
    const u8* fallback_texture; // DAT_004E1C58
    RuntimeDialogueDrawContext& draw;
    RuntimeDialogueOuterDrawContext& outer;
    RuntimeDialogueWidgetContext& widgets;
    DialogueWidgetAllocationContext& widget_allocation;
    LocalTextResourceContext& localtext;
    // 0x000FF0A0: a localized string by resource, group and key, or null.
    std::function<const char16_t*(const char* resource, const char* group, const char* key)> lookup_text;
    // 0x0013B580 over the scene factory: a dialogue button bat at a position, scale and depth.
    std::function<Actor*(const std::array<float, 2>& position, float scale, float depth)> create_bat;
    // The scene actor's native bat state (actor +0x934..) and 0x0006DA00 over its animation.
    std::function<ButtonBatScreen&(Actor& bat)> bat_screen;
    std::function<void(Actor& bat, const ButtonBatPoint& target, float scale)> fly_bat;
    // The actor's vtable +0x14 draw for a view and pass.
    std::function<void(Actor& actor, i32 view, i32 pass)> draw_actor;
    // 0x00142460 over the scene's widget update, with the front-end device (DAT_00429BBC) or none.
    std::function<void(RuntimeDialogueWidget& widget, bool device)> update_prompts;
    std::function<u32(float* timer)> timer_expired; // 0x0012B2A0, EBX the timer
    // 0x001429E0 over the front-end device (DAT_00429BBC).
    std::function<void(std::array<float, 2>& direction)> frontend_direction;
    std::function<void(const char* name, i32 mode)> request_script; // 0x0011EB30
    char* transition_override; // DAT_0050E458
    CutsceneRuntimeContext& cutscenes; // The scene's cutscene runtime (0x0004D680 ..).
    AttractControlServices& attract; // 0x00143070 / 0x001430E0: the "gameselect" keyboard input mode.
    std::function<i32()> dialogue_page_hold; // DAT_004EE1EC, the scene dialogue's mode.
    AudioEventVoices& voices; // DAT_004E0CE8, the event voices.
    u32 language; // DAT_0042A710
    // The native address of a device the scene's input registry resolves, and its removal.
    std::function<u32(std::span<u8> device)> map_input;
    std::function<void(u32 address)> unmap_input;
    // 0x00142460 over the scene's widget update with a mapped device.
    std::function<void(RuntimeDialogueWidget& widget, u32 device)> update_widget;
    MovieFileContext& files; // The asynchronous file rows the save code reads and writes through.
    SaveImageBoundaries save_image; // 0x0010F320's texture and its XPR file.
    TransitionBookServices& books; // The menu book (0x00048260 ..).
    std::function<const RuntimeDialogueInputDevice&(u32 address)> input_view; // A mapped device's view.
    u32* port_masks; // DAT_0050E0F8, the players' port masks.
    const i32& paused; // DAT_00510CE8
    std::function<void(Actor& actor)> update_actor; // The actor's vtable +0x10.
    char* replacement_source; // DAT_004E1AC0, the cutscene actor model a replacement swaps out.
    char* replacement_target; // DAT_004E1A40
};
// One row of the mode table at 0x0030A000: create, destroy, open(owner, widget), close, tick, draw. A null entry
// is a row function that is not translated yet; reaching it raises with its address.
struct GameSelectWidgetRow
{
    std::array<u32, 6> address;
    GameSelectWidgetHandle (*create)(GameSelectControlServices& services);
    void (*open)(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
        GameSelectControlServices& services);
    void (*close)(GameSelectWidget& widget, GameSelectControlServices& services);
    void (*tick)(GameSelectWidget& widget, GameSelectControlServices& services);
    void (*draw)(GameSelectWidget& widget, GameSelectControlServices& services);
};
const GameSelectWidgetRow& gameselect_widget_row(u32 index); // 0x0030A000 + index * 0x18
// The front-end stick as up (1), down (2), left (4) or right (8), or 0 inside the dead zone.
u16 gameselect_direction_code(std::array<float, 2> direction,
    const GameSelectControlServices& services); // Xbox 0x001080B0, ESI the direction
u32 gameselect_widget_row_index(const GameSelectWidgetRow& row);
GameSelectWidgetHandle create_gameselect_widget(const GameSelectWidgetRow& row,
    GameSelectControlServices& services); // Row +0
void destroy_runtime_gameselect_widget(const GameSelectWidgetRow& row,
    GameSelectWidgetHandle& widget); // Row +4, Xbox 0x000D8AC0
void open_gameselect_widget(const GameSelectWidgetRow& row, std::span<u8> owner, GameSelectControlState& control,
    GameSelectWidget& widget, GameSelectControlServices& services); // Row +8
void close_gameselect_widget(const GameSelectWidgetRow& row, GameSelectWidget& widget,
    GameSelectControlServices& services); // Row +0xC
void tick_gameselect_widget(const GameSelectWidgetRow& row, GameSelectWidget& widget,
    GameSelectControlServices& services); // Row +0x10
void draw_gameselect_widget(const GameSelectWidgetRow& row, GameSelectWidget& widget,
    GameSelectControlServices& services); // Row +0x14
std::span<u8> create_runtime_screen_texture(u16 width, u16 height, u8 levels, u32 format, u32 data,
    const RuntimeScreenTextureBoundaries& boundaries); // Xbox 0x000533A0
void release_runtime_screen_texture(std::span<u8> texture,
    const RuntimeScreenTextureBoundaries& boundaries); // Xbox 0x00053450, EAX texture
bool runtime_texture_format_drawable(std::span<const u8> texture); // Xbox 0x00053680, EAX texture
}
