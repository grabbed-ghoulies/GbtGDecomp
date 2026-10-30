#include "gallery_control.h"
#include "../cutscenes/actor_tracks.h"
#include "../cutscenes/event_runtime.h"
#include "../camera/view_initialization.h"
#include "../frontend/dialogue_draw_outer.h"
#include "../frontend/dialogue_input.h"
#include "../frontend/dialogue_text_pool.h"
#include "../frontend/dialogue_widget_construction.h"
#include "../frontend/dialogue_widget_state.h"
#include "../frontend/hud_layout.h"
#include "../frontend/input_activation.h"
#include "../graphics/shader_parameters.h"
#include "../saves/runtime_profile.h"
#include "../transitions/native_book.h"
#include "attract_control.h"
#include "executable_resources.h"
#include "../../gbtg/text/localtext_resources.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
constexpr u32 kPageTextures = 0x00427B60; // aid_texture_ghoulies_unlockableart_*, stride 0x80
constexpr const char* kTurn = "aid_cutscene_ghoulies_misc_artbookreadpageturn"; // 0x00427A60
constexpr const char* kTurnBack = "aid_cutscene_ghoulies_misc_artbookreadpageturnback"; // 0x00427AE0

float exe_real(const GameSelectControlServices& s, u32 address)
{
    return std::bit_cast<float>(read_u32(s.image.memory(address, 4), 0));
}

const char* page_texture_name(i32 page, const GameSelectControlServices& s)
{
    return s.image.string(kPageTextures + u32(page) * 0x80);
}

// The frame step: nothing while frozen (DAT_004312DC > 0), else DAT_005096C0 * DAT_004312C4.
float frame_step(const GameSelectControlServices& s)
{
    if (s.freeze <= 0.f)
    {
        return s.dt * s.scene_scale;
    }
    return 0.f;
}

EntityCamNode* camera_of(GalleryControlState& control)
{
    return &control.camera;
}

const RuntimeDialogueInputDevice& device(const GalleryControlState& control, const GameSelectControlServices& s)
{
    return s.input_view(control.input_address);
}

// Xbox 0x00088280, EAX the page: its art, or nothing for a row that is not an aid_ name.
std::span<const u8> load_page_texture(i32 page, GameSelectControlServices& s)
{
    const char* name = page_texture_name(page, s);
    if (read_u32({reinterpret_cast<const u8*>(name), 4}, 0) != 0x5F646961) // "aid_"
    {
        return {};
    }
    std::span<const u8> texture = acquire_animation_resource(s.resources, name); // 0x00035E90
    if (texture.data() == nullptr)
    {
        resource_error(s.error, 2); // 0x00032700(2)
    }
    return texture;
}

// Xbox 0x00088170, ESI the control: the three page shaders' parameters go.
void release_page_parameters(GalleryControlState& control)
{
    if (control.front != nullptr)
    {
        release_shader_parameter(control.front); // 0x000F85D0
        control.front = nullptr;
    }
    if (control.turning != nullptr)
    {
        release_shader_parameter(control.turning);
        control.turning = nullptr;
    }
    if (control.back != nullptr)
    {
        release_shader_parameter(control.back);
        control.back = nullptr;
    }
}

// The 0x000F86F0 body 0x00087FE0 inlines: the texture bound with mode 0 at time 0.
void bind_page(RuntimeShaderTextureParameter& parameter, u32 texture, GameSelectControlServices& s)
{
    set_shader_parameter_texture(parameter, texture, 0, s.books.texture_bytes);
}

// Xbox 0x00087FE0, EAX the control: the book model's page shaders take the current and previous art (which shader
// shows which depends on the turn's direction) and the back page the book page texture.
void bind_page_textures(GalleryControlState& control, i32 forward, GameSelectControlServices& s)
{
    const auto& view = s.cutscenes.views.at(control.cutscene->resource);
    auto& rows = s.cutscenes.instances.at(control.cutscene);
    Actor* book = find_cutscene_actor_by_model(*control.cutscene, view.tracks.data(), rows.actors.data(),
        "aid_model_ghoulies_cutscene_artbook"); // 0x0004EA50, 0x00427960
    release_page_parameters(control); // 0x00088170
    RuntimeModelInstance& model = *book->model->runtime; // +0x204
    if (control.texture.data() != nullptr)
    {
        control.front = create_shader_parameter(model, forward == 0 ? "shader_page2" : "shader_page1",
            "colour0"); // 0x000F8540
        bind_page(*control.front, s.books.texture_address(control.texture), s);
    }
    if (control.previous.data() != nullptr)
    {
        control.turning = create_shader_parameter(model, forward == 0 ? "shader_page1" : "shader_page2", "colour0");
        bind_page(*control.turning, s.books.texture_address(control.previous), s);
    }
    if (control.back == nullptr)
    {
        control.back = create_shader_parameter(model, "shader_page3", "colour0");
        bind_page(*control.back, s.books.page_textures[0], s); // DAT_005047D8
    }
}

// Xbox 0x000881C0, ESI the control, EAX the step: the page moves on (wrapping), the current art becomes the previous
// and the new page's art loads; the art before that is released, and unloaded once nothing holds it.
void turn_page(GalleryControlState& control, i32 step, GameSelectControlServices& s)
{
    const i32 page = control.page + step;
    control.page = page;
    const i32 count = count_runtime_gallery_pages(s);
    if (page >= count || page < 0)
    {
        control.page = 0;
        if (page < count)
        {
            control.page = count_runtime_gallery_pages(s) - 1;
            if (control.page < 0)
            {
                control.page = 0;
            }
        }
    }
    const std::span<const u8> old = control.previous;
    control.previous = control.texture;
    control.texture = load_page_texture(control.page, s); // The 0x00088280 body inline.
    if (old.data() != nullptr)
    {
        release_animation_resource(s.resources, old); // 0x00035C00, EBX the old art
        u32 referenced = 0;
        animation_resource_held(s.resources, old, referenced); // 0x00036210
        if (referenced == 0)
        {
            unload_animation_resource(s.resources, old); // 0x00035C60
        }
    }
}

// Xbox 0x000883A0, EDX the texture name: the gameselect text keyed by the name after its last '_'.
void page_caption(const char* name, char16_t* output, GameSelectControlServices& s)
{
    const char* key = name + std::strlen(name);
    while (*key != '_')
    {
        --key;
    }
    request_namespaced_localtext("aid_loctext_ghoulies_gameselect_gameselecttext", "gameselect", key + 1, output,
        s.localtext); // 0x000FF0A0, 0x0020A574, 0x003241E0
}

// Xbox 0x000883F0, ESI the control: the page's caption replaces the old one.
void show_page_caption(GalleryControlState& control, GameSelectControlServices& s)
{
    end_all_dialogue_text_records(*control.pool, s.fonts, s.fallback_texture, s.random); // 0x0005F970
    page_caption(page_texture_name(control.page, s), control.caption.data(), s); // 0x000883A0
    create_dialogue_text_record(*control.pool, control.caption.data(), float(s.screen[0]) * exe_real(s, 0x0032B0F4),
        float(s.screen[1]) * exe_real(s, 0x0032B168), 1.f, 1.f, 7.f, 1, 0, 0, s.metrics, s.viewport_width,
        s.next_text_id, s.random); // 0x0005F5C0, EAX 0, DAT_005096E8, DAT_005096EA
}

// Xbox 0x000882C0: the "lockableArtMenu_basic" prompts.
void make_prompts(GalleryControlState& control, GameSelectControlServices& s)
{
    char resource[0x80]{};
    std::memcpy(resource, "aid_loctext_ghoulies_gameselect_gameselecttext", 47); // 0x0020A574
    const std::array<float, 2> anchor{float(s.screen[0]) * exe_real(s, 0x0032B168),
        s.hud.inner_bottom - exe_real(s, 0x0032B648)}; // DAT_005096E8, DAT_0050974C
    const std::array<float, 2> row_step{0.f, exe_real(s, 0x0032B644)};
    const std::array<float, 2> origin{exe_real(s, 0x0032B470), exe_real(s, 0x0032B4F4)};
    create_resource_dialogue_widget(control.prompts, 0, s.image.memory(0x003022D8, 0x310).data(), anchor, row_step,
        origin, nullptr, 0.75f, 0.75f, "gameselect", resource, 0x80, s.widget_allocation); // 0x00141DA0, ESI +0xA8, 0x003022D8
}

// Xbox 0x00087F20 / 0x00087F80, ECX the control: the stick is off centre and points `code` (8 right, 4 left).
bool stick_points(const GalleryControlState& control, u16 code, GameSelectControlServices& s)
{
    std::array<float, 2> direction{};
    dialogue_input_direction(device(control, s), direction, s.attract.input_state); // 0x001429E0, ESI +0xAC
    if (direction[0] == 0.f && direction[1] == 0.f)
    {
        return false;
    }
    return gameselect_direction_code(direction, s) == code; // 0x001080B0
}

// The page turned back with the leaving speed one faster (at most 5), and the caption ended.
void turn_back_to_leave(GalleryControlState& control, GameSelectControlServices& s)
{
    release_runtime_cutscene(control.cutscene, s.cutscenes); // 0x0004D720
    control.cutscene = load_runtime_cutscene(kTurnBack, 0, camera_of(control), s.cutscenes); // 0x0004D680
    turn_page(control, -1, s); // 0x000881C0, EAX -1
    bind_page_textures(control, 1, s); // 0x00087FE0(1)
    const float most = exe_real(s, 0x0032B184);
    control.speed = control.speed + exe_real(s, 0x0032B0F8);
    if (most <= control.speed)
    {
        control.speed = most;
    }
    end_all_dialogue_text_records(*control.pool, s.fonts, s.fallback_texture, s.random); // 0x0005F970
    control.state = 4;
}

// Xbox 0x00088480: back to game select through aid_script_ghoulies_gameselect_unlockableart_return.
void leave_gallery(GameSelectControlServices& s)
{
    std::strcpy(s.transition_override, "aid_script_ghoulies_gameselect_unlockableart_return"); // DAT_0050E458
    s.request_script("aid_script_ghoulies_gameselect_gameselect", 1); // 0x0011EB30, EAX 0x00428560
}
}

// Xbox 0x00087860
void gallery_control_object_sizes(ObjectSizes& output)
{
    output.instance = 0x10B8;
    output.parameters = 8;
}

// Xbox 0x000878D0
bool construct_runtime_gallery_control(GalleryControlState& control, GameSelectControlServices& services)
{
    set_runtime_gallery_state(control, 2, services); // 0x00087D40(2)
    ++services.pause_count; // DAT_00510CEC
    return true;
}

// Xbox 0x00087900
void destroy_runtime_gallery_control(GalleryControlState& control, GameSelectControlServices& services)
{
    auto& s = services;
    if (control.cutscene != nullptr)
    {
        release_runtime_cutscene(control.cutscene, s.cutscenes); // 0x0004D720
        control.cutscene = nullptr;
    }
    camera_unlink_view(&control.camera); // 0x00048C70
    pop_runtime_input_entry(s.input); // 0x001093C0
    switch (read_u32(control.input, 0))
    {
    case 1:
    case 2:
    case 3:
        release_runtime_simple_input(control.input); // 0x00107DD0
        break;
    case 4:
        release_runtime_composite_input(control.input, s.input_release); // 0x00107960
        break;
    case 5:
        release_runtime_actor_input(control.input, s.input_release); // 0x00107B60
        break;
    default:
        break;
    }
    s.unmap_input(control.input_address);
    control.input = {};
    control.input_address = 0;
    release_page_parameters(control); // 0x00088170
    if (control.prompts != nullptr)
    {
        destroy_dialogue_widget(control.prompts, s.resources, *s.text_pools); // 0x00142070
        control.prompts = nullptr;
    }
    if (control.texture.data() != nullptr)
    {
        release_animation_resource(s.resources, control.texture); // 0x00035C00
    }
    if (control.previous.data() != nullptr)
    {
        release_animation_resource(s.resources, control.previous);
    }
    if (control.pool != nullptr)
    {
        release_dialogue_text_pool(control.pool, *s.text_pools); // 0x0005F460
        control.pool = nullptr;
    }
    --s.pause_count; // DAT_00510CEC
}

// Xbox 0x000879D0
void tick_runtime_gallery_control(GalleryControlState& control, GameSelectControlServices& services)
{
    auto& s = services;
    refresh_runtime_input_device(control.input_address, s.frame, s.input, s.input_release); // 0x00109470
    if (control.prompts != nullptr)
    {
        s.update_widget(*control.prompts, control.input_address); // 0x00142460, EAX +0xAC
    }
    if (control.state < 0 || 3 < control.state)
    {
        if (control.prompts != nullptr)
        {
            destroy_dialogue_widget(control.prompts, s.resources, *s.text_pools); // 0x00142070
            control.prompts = nullptr;
        }
    }
    else if (control.prompts_made == 0)
    {
        make_prompts(control, s); // 0x000882C0
        control.prompts_made = 1;
    }
    else if (runtime_input_button_edge(device(control, s), 0x800, s.attract.input_state) != 0) // 0x001083F0, Y
    {
        if (control.prompts != nullptr)
        {
            destroy_dialogue_widget(control.prompts, s.resources, *s.text_pools);
            control.prompts = nullptr;
        }
        else
        {
            make_prompts(control, s);
        }
    }
    switch (control.state)
    {
    case 0:
    case 1:
        if (tick_runtime_cutscene(*control.cutscene, frame_step(s), camera_of(control), s.cutscenes)) // 0x0004DA50
        {
            control.state = 3;
        }
        return;
    case 2:
    case 3:
    {
        tick_runtime_cutscene(*control.cutscene, frame_step(s), camera_of(control), s.cutscenes);
        if (stick_points(control, 8, s) && 1 < count_runtime_gallery_pages(s)) // 0x00087F20, 0x000884B0
        {
            release_runtime_cutscene(control.cutscene, s.cutscenes); // 0x0004D720
            control.cutscene = load_runtime_cutscene(kTurn, 0, camera_of(control), s.cutscenes); // 0x0004D680
            turn_page(control, 1, s); // 0x000881C0, EAX 1
            bind_page_textures(control, 0, s); // 0x00087FE0(0)
            show_page_caption(control, s); // 0x000883F0
            control.state = 0;
            return;
        }
        if (stick_points(control, 4, s) && 1 < count_runtime_gallery_pages(s)) // 0x00087F80
        {
            release_runtime_cutscene(control.cutscene, s.cutscenes);
            control.cutscene = load_runtime_cutscene(kTurnBack, 0, camera_of(control), s.cutscenes);
            turn_page(control, -1, s);
            bind_page_textures(control, 1, s);
            show_page_caption(control, s);
            control.state = 1;
            return;
        }
        if (runtime_input_button_held(device(control, s), 0x300, s.attract.input_state) != 0) // 0x00108300, A or B
        {
            if (control.page == 0)
            {
                leave_gallery(s); // The tail jump to 0x00088480.
                return;
            }
            turn_back_to_leave(control, s);
        }
        return;
    }
    case 4:
        if (tick_runtime_cutscene(*control.cutscene, control.speed * frame_step(s), camera_of(control), s.cutscenes))
        {
            if (control.page == 0)
            {
                leave_gallery(s); // 0x00088480
                return;
            }
            turn_back_to_leave(control, s);
        }
        return;
    default:
        return;
    }
}

// Xbox 0x00087D20: the prompts, 0x00142720's immediate draw inline.
void draw_runtime_gallery_control(GalleryControlState& control, GameSelectControlServices& services)
{
    if (control.prompts != nullptr)
    {
        draw_runtime_dialogue_widget(*control.prompts, 1, services.outer);
    }
}

// Xbox 0x00087D40
void set_runtime_gallery_state(GalleryControlState& control, i32 state, GameSelectControlServices& services)
{
    auto& s = services;
    switch (state)
    {
    case 0:
        release_runtime_cutscene(control.cutscene, s.cutscenes); // 0x0004D720
        control.cutscene = load_runtime_cutscene(kTurn, 0, camera_of(control), s.cutscenes); // 0x0004D680
        turn_page(control, 1, s); // 0x000881C0, EAX 1
        bind_page_textures(control, 0, s); // 0x00087FE0(0)
        show_page_caption(control, s); // 0x000883F0
        control.state = state;
        return;
    case 1:
        release_runtime_cutscene(control.cutscene, s.cutscenes);
        control.cutscene = load_runtime_cutscene(kTurnBack, 0, camera_of(control), s.cutscenes);
        turn_page(control, -1, s);
        bind_page_textures(control, 1, s);
        show_page_caption(control, s);
        control.state = state;
        return;
    case 2:
    {
        control.input = create_runtime_selected_input(s.input_release); // 0x00107D60, ESI +0xAC
        control.input_address = s.map_input(control.input);
        push_runtime_input_entry(control.input_address, "Unlockable Art", s.input); // 0x00109340, ESI 0x00324258
        initialize_camera_view(control.camera, "unlockableart", 8); // 0x000496C0, EAX 0x00324248, CL 8
        link_camera_view(0, control.camera); // 0x00048C20, EDX +0x2C, ESI 0
        camera_bind_named_view(0, "unlockableart"); // 0x00048CF0, EBX 0x00324248
        control.page = 0;
        control.texture = load_page_texture(0, s); // 0x00088280, EAX 0
        const i32 last = count_runtime_gallery_pages(s) - 1; // 0x000884B0
        control.previous = load_page_texture(last < 0 ? 0 : last, s);
        control.cutscene = load_runtime_cutscene("aid_cutscene_ghoulies_misc_artbookreadstart", 0, camera_of(control),
            s.cutscenes); // 0x0004D680, 0x004279E0
        seek_runtime_cutscene(*control.cutscene, std::bit_cast<float>(0x3F7D70A4u), 0.f, 0, s.cutscenes); // 0x0004E4A0
        tick_runtime_cutscene(*control.cutscene, 0.f, camera_of(control), s.cutscenes); // 0x0004DA50(0), ESI +0x2C
        allocate_dialogue_text_pool(control.pool, 0, 1, 0x80, s.text_pools); // 0x0005F390, ESI +0xB0
        bind_page_textures(control, 1, s); // 0x00087FE0(1)
        show_page_caption(control, s); // 0x000883F0
        control.state = state;
        control.speed = exe_real(s, 0x0032B0F8);
        return;
    }
    case 4:
        turn_back_to_leave(control, s);
        return;
    default:
        control.state = state;
        return;
    }
}

// Xbox 0x000884B0
i32 count_runtime_gallery_pages(const GameSelectControlServices& services)
{
    i32 finished = 0;
    for (size_t index = 0; index < 0x15; ++index) // DAT_004E194C .. DAT_004E19A4
    {
        if (read_u32(g_game_progress.words_194c, index * 4) == 6)
        {
            ++finished;
        }
    }
    if ((services.profile.progress.bytes[7] & 0x20) != 0 || 0x13 < finished) // *(DAT_00506C6C + 8) + 7
    {
        finished = 0x14;
    }
    return finished;
}
}
