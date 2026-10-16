#include "native_book.h"
#include "book_map.h"
#include "../camera/display_view.h"
#include "../camera/runtime_display_setup.h"
#include "../camera/view_initialization.h"
#include "../cutscenes/event_pause.h"
#include "../cutscenes/event_runtime.h"
#include "../cutscenes/instance_construction.h"
#include "../frontend/dialogue_draw_context.h"
#include "../scene/executable_resources.h"
#include "gbtg/model_runtime.h"

#include <bit>
#include <cstring>

namespace gbtg
{
RuntimeTransitionBook* g_capture_book = nullptr;
u32 g_book_viewport_reset = 0;

namespace
{
constexpr u32 kClipTable = 0x00422078; // Stride 0x124: the cutscene, +0x80 the model, +0x100.. the row words.

u32 exe_word(const SceneExecutableResources& image, u32 address)
{
    return read_u32(image.memory(address, 4), 0);
}

u32 row(const RuntimeTransitionBook& book)
{
    return kClipTable + u32(book.clip) * 0x124;
}

float frame_step(const TransitionBookServices& s)
{
    if (s.freeze <= 0.f)
    {
        return s.dt * s.scene_scale;
    }
    return 0.f;
}

[[noreturn]] void untranslated(const char* what)
{
    throw Error(std::string(what) + " is not translated");
}

// Xbox 0x000F8540, EBX the model: the named shader's texture parameter of that name, or null.
BookShaderParameter* create_shader_parameter(RuntimeModelInstance& model, const char* shader, const char* name)
{
    const RuntimeModelTree* tree = model.source->tree; // *(*(model + 0) + 0x10) + 0x24
    if (tree == nullptr)
    {
        return nullptr;
    }
    RuntimeModelNode* node = nullptr;
    if (!find_runtime_model_named_node(*tree, shader, node) || node == nullptr) // 0x001568E0
    {
        return nullptr;
    }
    const RuntimeShaderCpuEntry* entry = nullptr;
    if (node->shader_cpu == nullptr || !find_runtime_shader_entry(*node->shader_cpu, name, entry)) // 0x0015B160
    {
        return nullptr;
    }
    if (entry->value == -1)
    {
        return nullptr;
    }
    auto* parameter = new BookShaderParameter; // 0x000F72D0(0x18)
    parameter->model = &model;
    parameter->shader = model.tree_storage.shader_nodes.at(node->copy_offset).primary; // model + node +0xC + 0x2A4
    parameter->index = u32(entry->value);
    return parameter;
}

// Xbox 0x0015B2B0: the shader instance's texture word for the parameter.
void set_shader_texture(RuntimeShaderInstance& shader, u32 index, u32 texture)
{
    shader.zero_words[index] = texture;
}

// Xbox 0x000F85D0, EAX the parameter.
void release_shader_parameter(BookShaderParameter* parameter)
{
    parameter->texture = 0;
    parameter->word_10 = 0;
    parameter->word_14 = 0;
    set_shader_texture(*parameter->shader, parameter->index, 0); // 0x0015B2B0
    delete parameter; // HeapFree
}

// Xbox 0x00048110: each page shader's colour0 gets a page texture (pages 2 and 3 DAT_005047E4, 1 and 4
// DAT_005047D8).
void bind_book_pages(RuntimeTransitionBook& book, TransitionBookServices& s)
{
    static constexpr std::array<std::pair<const char*, u8>, 4> kPages{
        {{"shader_page1", 0}, {"shader_page2", 1}, {"shader_page3", 1}, {"shader_page4", 0}}}; // 0x00423194
    RuntimeModelInstance& model = *book.model->model->runtime; // +0x20 +0x204
    for (size_t index = 0; index < kPages.size(); ++index)
    {
        const RuntimeModelTree* tree = model.source->tree;
        if (tree == nullptr)
        {
            continue;
        }
        RuntimeModelNode* node = nullptr;
        if (!find_runtime_model_named_node(*tree, kPages[index].first, node) || node == nullptr) // 0x001568E0
        {
            continue;
        }
        BookShaderParameter* parameter = create_shader_parameter(model, kPages[index].first, "colour0"); // 0x000F8540
        auto& page = book.pages[index];
        page.parameter = parameter;
        const u32 texture = kPages[index].second != 0 ? s.page_textures[1] : s.page_textures[0];
        page.texture = texture;
        parameter->texture = texture;
        parameter->word_10 = 0;
        parameter->word_14 = 0;
        u32 bound = 0;
        if (texture != 0)
        {
            u8* bytes = s.texture_bytes(texture);
            if (bytes[0x11] != 0)
            {
                bytes[0x13] = 0;
            }
            bound = parameter->texture;
        }
        set_shader_texture(*parameter->shader, parameter->index, bound); // 0x0015B2B0
        page.capture = 0;
        page.copy = 0;
    }
}

// Xbox 0x000481E0, EAX the book: the clip's camera pair on the model and its secondary model.
void apply_book_camera_pair(RuntimeTransitionBook& book, TransitionBookServices& s)
{
    if (i32(exe_word(s.image, row(book) + 0x118)) == -1) // 0x00422190
    {
        return;
    }
    const auto& view = s.cutscenes.views.at(book.cutscene->resource);
    auto& rows = s.cutscenes.instances.at(book.cutscene);
    Actor* actor = find_cutscene_actor_by_model(*book.cutscene, view.tracks.data(), rows.actors.data(),
        s.image.string(row(book) + 0x80)); // 0x0004EA50
    const u8 slot = s.image.memory(row(book) + 0x118, 1)[0];
    if (actor->model != nullptr && actor->model->runtime != nullptr)
    {
        set_runtime_model_node_state(*actor->model->runtime, slot, 1, s.node_states); // 0x000F96F0
    }
    if (RuntimeModelInstance* secondary = s.secondary_model(*actor); secondary != nullptr)
    {
        set_runtime_model_node_state(*secondary, slot, 1, s.node_states); // +0x310
    }
}

// Xbox 0x0004E640 over the cutscene's rows.
void pause_book_cutscene(RuntimeCutsceneInstance& cutscene, i32 paused, TransitionBookServices& s)
{
    CutsceneEventPauseContext pause{s.cutscenes.instances.at(&cutscene).actors.data(), s.cutscenes.animations,
        s.cutscenes.audio.voices, s.cutscenes.resume_callbacks};
    pause_cutscene_events(cutscene, paused, pause);
}

// Xbox 0x00061B40: the current view's fitted rectangle cleared to the colour, depth 1.
void clear_book_view(u32 color, TransitionBookServices& s)
{
    const auto& view = *entity_detail::g_display_view; // DAT_005077A8
    const std::array<i32, 4> rectangle{view.fitted_x, view.fitted_y, view.fitted_x + view.fitted_width,
        view.fitted_y + view.fitted_height}; // +0x58, +0x5C, +0x60, +0x64
    // The colour's bytes reorder from R G B A to the D3DCOLOR A R G B.
    const u32 swapped = ((((color >> 24) << 8 | (color & 0xFF)) << 8 | ((color >> 8) & 0xFF)) << 8)
        | ((color >> 16) & 0xFF);
    s.clear(1, rectangle.data(), 0xF3, swapped, 1.f, 0);
}

}

float transition_book_threshold(const RuntimeTransitionBook& book, size_t offset, const SceneExecutableResources& image)
{
    return std::bit_cast<float>(exe_word(image, row(book) + u32(offset)));
}

i32 transition_book_row_word(const RuntimeTransitionBook& book, size_t offset, const SceneExecutableResources& image)
{
    return i32(exe_word(image, row(book) + u32(offset)));
}

// Xbox 0x00048260
RuntimeTransitionBook* create_transition_book(i32 clip, u32 flags, const char* texture,
    TransitionBookServices& services)
{
    auto& s = services;
    auto* book = new RuntimeTransitionBook; // 0x000F72D0(0x1E2A4), zeroed
    book->clip = clip;
    book->flags = flags;
    book->real_88 = 0.f;
    book->finished = 0;
    book->holding = 0;
    book->paused = 0;
    book->route = 0;
    book->voice = -1;
    initialize_camera_view(book->camera, "transition", 0xC); // 0x000496C0, EAX 0x00322634
    link_camera_view(0, book->camera); // The slot-0 list insert and, with no active camera, 0x00048CF0.
    book->cutscene = load_runtime_cutscene(s.image.string(row(*book)), 0x36, &book->camera, s.cutscenes); // 0x0004D680
    seek_runtime_cutscene(*book->cutscene, transition_book_threshold(*book, 0x100, s.image), 0.f, 0,
        s.cutscenes); // 0x0004E4A0, DAT_00422178
    const auto& view = s.cutscenes.views.at(book->cutscene->resource);
    auto& rows = s.cutscenes.instances.at(book->cutscene);
    book->model = find_cutscene_actor_by_model(*book->cutscene, view.tracks.data(), rows.actors.data(),
        s.image.string(row(*book) + 0x80)); // 0x0004EA50, 0x004220F8
    bind_book_pages(*book, s); // 0x00048110
    if (texture != nullptr && transition_book_row_word(*book, 0x114, s.image) != -1) // 0x0042218C
    {
        untranslated("0x00048260's page texture (0x00035E90, 0x000F86F0)");
    }
    apply_book_camera_pair(*book, s); // 0x000481E0
    if ((book->flags & 1) != 0)
    {
        g_capture_book = book; // DAT_005462A0
    }
    return book;
}

// Xbox 0x000483E0
void tick_transition_book(RuntimeTransitionBook& book, TransitionBookServices& services)
{
    auto& s = services;
    if (book.cutscene == nullptr)
    {
        return;
    }
    tick_book_map(book, s); // 0x000475B0
    if (book.finished == 0)
    {
        const bool ended = tick_runtime_cutscene(*book.cutscene, frame_step(s), &book.camera, s.cutscenes); // 0x0004DA50, ESI +0x28
        i32 done = ended ? 1 : 0;
        auto& rows = s.cutscenes.instances.at(book.cutscene);
        const u8 tracks = book.cutscene->resource[3]; // *(+0x3C) + 3
        for (u8 index = 0; index < tracks; ++index)
        {
            s.update_actor(*rows.actors.at(index).actor); // 0x000420D0, row +0
        }
        if (book.holding == 0)
        {
            book.finished = done;
            return;
        }
        if (book.map_state == 0)
        {
            const float scale = 1.f / book.cutscene->duration; // DAT_0032B0F8
            const float threshold = transition_book_threshold(book, 0x104, s.image); // 0x0042217C
            if (book.cutscene->previous_time * scale < threshold && threshold <= book.cutscene->time * scale)
            {
                done = 1;
                if (book.paused == 0)
                {
                    pause_book_cutscene(*book.cutscene, 1, s); // 0x0004E640
                    book.paused = 1;
                }
                if (book.route != 0)
                {
                    begin_book_map(book, book.route_argument, s); // 0x00046F40
                    return;
                }
                book.finished = done;
            }
        }
        else if (book.map_ready != 0)
        {
            book.finished = 1;
        }
        return;
    }
    if (book.holding == 0 && book.cutscene->pending[2] != 0) // +0x158
    {
        if (book.paused != 0)
        {
            pause_book_cutscene(*book.cutscene, 0, s);
            book.paused = 0;
        }
        book.finished = 0;
    }
}

// Xbox 0x00048860, EAX the book
void release_transition_book(RuntimeTransitionBook* book, TransitionBookServices& services)
{
    auto& s = services;
    release_book_map(*book, s); // 0x000473D0
    camera_unlink_view(&book->camera); // 0x00048C70
    if (book == g_capture_book)
    {
        g_capture_book = nullptr;
    }
    if (book->cutscene != nullptr)
    {
        release_runtime_cutscene(book->cutscene, s.cutscenes); // 0x0004D720
    }
    for (auto& page : book->pages)
    {
        if (page.parameter != nullptr)
        {
            release_shader_parameter(page.parameter); // 0x000F85D0
        }
    }
    if (book->texture.data() != nullptr)
    {
        untranslated("0x00048860's page texture release (0x00035C00, 0x00036210, 0x00035C60)");
    }
    delete book; // HeapFree
}

// Xbox 0x00048550
void draw_transition_book(RuntimeTransitionBook& book, TransitionBookServices& services)
{
    auto& s = services;
    if (book.cutscene == nullptr)
    {
        return;
    }
    ++s.draw.override_count; // DAT_00509B5C
    g_book_viewport_reset = 1; // DAT_005077FC
    s.draw.fog_disabled = 1; // DAT_00509B00
    s.fog(0); // 0x00156EA0
    for (auto& page : book.pages)
    {
        if (page.capture == 0)
        {
            continue;
        }
        s.copy_back_buffer(page.texture); // D3DDevice_CopyRects(back buffer, .., the page's level 0)
        if (s.capture_configuration != 0)
        {
            untranslated("0x00048550's widescreen page redraw (0x0005CE80)");
        }
        page.capture = 0;
    }
    for (auto& page : book.pages)
    {
        if (page.copy != 0 && page.texture != 0)
        {
            clear_book_view(s.page_clear_color, s); // 0x00061B40, DAT_004231C4
            s.copy_back_buffer(page.texture);
            page.copy = 0;
        }
    }
    clear_book_view(s.clear_color, s); // 0x00061B40, DAT_005462A4
    if (select_display_camera(&book.camera, 0, s.display)) // 0x000489E0
    {
        rebuild_display_view(s.display); // 0x000F5E50
        extend_display_clip_limits(entity_detail::g_display_view->transform, s.display.scene_bounds,
            book.camera.blend, book.camera.clip_limit); // 0x0013C490
        entity_detail::g_display_view->near_clip = book.camera.clip_limit;
        entity_detail::g_display_view->far_clip = book.camera.blend;
        rebuild_display_projection(s.display); // 0x000F5FD0
        bind_display_projection(s.display); // 0x000F6170
    }
    draw_runtime_cutscene(*book.cutscene, s.view(), s.cutscenes); // 0x0004DBD0, EBX +0x24
    draw_book_map(book, s); // 0x00047770
    for (i32 slot = 0; slot < 4; ++slot)
    {
        if (entity_detail::g_cam_active[slot] != &book.camera)
        {
            continue;
        }
        if (camera_bind_named_view(slot, entity_detail::g_cam_saved_name[slot]) == 0) // 0x00048CF0
        {
            EntityCamNode* first = entity_detail::g_cam_anchor[slot].next; // DAT_004E1684 + slot * 8
            if (first == nullptr)
            {
                entity_detail::g_cam_active[slot] = nullptr;
            }
            else
            {
                camera_bind_named_view(slot, first->name);
            }
        }
        break;
    }
    --s.draw.override_count;
    if (s.draw.override_count == 0)
    {
        s.draw.fog_disabled = 0;
    }
}
}
