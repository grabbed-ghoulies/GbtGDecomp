#include "gfx_render_thread.h"

#if GBTG_OPTIMIZE
#include "render_profile.h"
#include "vertex_program_constants.h"

#include "gbtg/log.h"
#include "gbtg/texture.h"

#include <condition_variable>
#include <cstring>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace gbtg
{
namespace render_thread
{
namespace
{

// A batch goes to the render thread after this many commands or arena bytes, so submission starts while the presenter
// is still translating the frame.
constexpr size_t kBatchCommands = 128;
constexpr size_t kBatchBytes = size_t(16) << 20;
constexpr size_t kChunkBytes = size_t(1) << 20;
// Retained-geometry copies idle this many frames are dropped, as the presenter's step 7 entries are.
constexpr u64 kRetainedIdleFrames = 600;

// A retained-geometry draw's inputs and indices, copied once per id (GfxNativeDraw::retained_geometry).
struct RetainedCopy
{
    std::vector<GfxNativeInputs> inputs;
    std::vector<u32> indices;
    u64 last_used = 0;
};

// A vertex program's words at a stable address (the Vulkan backend's program memo keys on it), the backend's answer to
// gfx_native_vertex_program_supported, and the constant registers the program reads.
struct Program
{
    std::vector<u32> words;
    bool queried = false;
    bool supported = false;
    u32 constants_first = 0;
    u32 constants_count = 192;
};

// The image a backend sees for one game image. After it is first queued only the render thread changes it, through
// UpdateImage commands, so draws queued before a change keep the pixels they were queued with.
struct Proxy
{
    std::shared_ptr<RgbaImage> image;
    u32 serial = 0; // The source serial last queued.
    int width = 0; // The size of the pixels last queued, as the proxy will hold them.
    int height = 0;
};

enum class Kind : u8
{
    BeginFrame,
    DrawNative,
    BeginBatch,
    EndBatch,
    ClearNative,
    BeginTarget,
    BeginSceneTarget,
    ClearUiDepth,
    DrawSprite,
    SpriteClear,
    SpriteSubmit,
    UpdateImage,
    EndTargetInto,
    CopyUiInto,
    EndSceneTargetInto,
    ReleaseImage,
    EndFrame,
    PresentAgain
};

struct Command
{
    Kind kind = Kind::BeginFrame;
    GfxNativeDraw draw;                                // DrawNative: spans point into the batch arena or `retained`
    std::array<std::shared_ptr<RgbaImage>, 4> images; // The proxies this command uses
    std::shared_ptr<RetainedCopy> retained;
    RgbaImage pixels; // UpdateImage: the new contents, swapped into images[0]
    std::span<const std::array<i32, 4>> rectangles;
    u32 flags = 0;
    u32 color = 0;
    float depth = 0.f;
    // BeginTarget: width, height, depth; BeginSceneTarget: width, height (key in `flags`); ClearUiDepth: left, top,
    // right, bottom
    std::array<int, 4> values{};
    GfxRect rect;
    std::array<float, 8> sprite{}; // x, y, w, h, u0, v0, u1, v1
    std::array<float, 4> sprite_color{};
};

// The constants copied for the batch's last program draw (step 21).
struct ConstantsCopy
{
    const std::array<std::array<float, 4>, 192>* copy = nullptr;
    u64 id = 0;
    u32 first = 0;
    u32 count = 0;
};

// Commands and the arena their copied data lives in. Batches are recycled; command slots and chunks keep their storage.
struct Batch
{
    ConstantsCopy last_constants;
    std::vector<Command> commands;
    size_t count = 0;
    std::vector<std::unique_ptr<std::byte[]>> chunks;
    size_t chunk = 0;
    size_t used = 0;
    std::vector<std::unique_ptr<std::byte[]>> large;
    size_t bytes = 0;
    u64 presents = 0;
    u64 ends = 0;

    void* allocate(size_t size)
    {
        size = (size + 15) & ~size_t(15);
        bytes += size;
        if (size > kChunkBytes / 4)
        {
            large.emplace_back(new std::byte[size]);
            return large.back().get();
        }
        if (chunk < chunks.size() && used + size > kChunkBytes)
        {
            ++chunk;
            used = 0;
        }
        if (chunk == chunks.size())
        {
            chunks.emplace_back(new std::byte[kChunkBytes]);
            used = 0;
        }
        void* at = chunks[chunk].get() + used;
        used += size;
        return at;
    }

    template <class T>
    std::span<const T> copy(std::span<const T> source)
    {
        if (source.empty())
        {
            return {};
        }
        void* at = allocate(source.size_bytes());
        std::memcpy(at, source.data(), source.size_bytes());
        return {static_cast<const T*>(at), source.size()};
    }

    // Drops what the executed commands kept alive; storage stays for the next use.
    void reset()
    {
        for (size_t n = 0; n < count; ++n)
        {
            Command& command = commands[n];
            command.images = {};
            command.retained.reset();
            if (command.kind == Kind::UpdateImage)
            {
                command.pixels = {}; // The replaced pixels; slots do not keep image-sized buffers.
            }
        }
        count = 0;
        chunk = 0;
        used = 0;
        last_constants = {};
        large.clear();
        bytes = 0;
        presents = 0;
        ends = 0;
    }
};

// Between the game thread and the render thread, under `mutex`.
struct Shared
{
    std::mutex mutex;
    std::condition_variable work;
    std::condition_variable idle;
    std::deque<Batch*> pending;
    std::vector<Batch*> spare;
    bool busy = false;
    bool stop = false;
    bool discard = false; // Set when the thread is stopped without a shutdown; queued commands are dropped.
    bool failed = false;
    std::exception_ptr failure;
    u64 presents_done = 0;
    u64 ends_done = 0;
    render_profile::Frame profile; // Render-thread totals not yet added to the game thread's frame.
};

Shared shared;
const GfxBackend* inner = nullptr;
std::vector<std::unique_ptr<Batch>> batches;
Batch* current = nullptr;
bool running = false;
u64 presents_queued = 0;
u64 ends_queued = 0;
u64 frame_number = 0;
std::unordered_map<const u32*, std::unique_ptr<Program>> programs;
std::vector<std::unique_ptr<Program>> replaced_programs; // Earlier words at a reused address; queued draws may hold them
std::unordered_map<const RgbaImage*, Proxy> proxies;
std::unordered_map<u64, std::shared_ptr<RetainedCopy>> retained;
// Step 20: the backend ends a texture target into an image (gfx_end_native_target_into) whenever their sizes match,
// so the wrapper answers for it and queues the end instead of draining. Set for D3D11 and (step 23) Vulkan.
bool ends_into = false;
int target_width = 0; // The texture target gfx_begin_native_target last opened.
int target_height = 0;

void execute(Command& command)
{
    switch (command.kind)
    {
    case Kind::BeginFrame:
        inner->gfx_begin_frame();
        break;
    case Kind::DrawNative:
        inner->gfx_draw_native(command.draw);
        break;
    case Kind::BeginBatch:
        inner->gfx_begin_native_batch();
        break;
    case Kind::EndBatch:
        inner->gfx_end_native_batch();
        break;
    case Kind::ClearNative:
        inner->gfx_clear_native(command.rectangles, command.flags, command.color, command.depth);
        break;
    case Kind::BeginTarget:
        inner->gfx_begin_native_target(command.values[0], command.values[1], command.values[2] != 0);
        break;
    case Kind::BeginSceneTarget:
        inner->gfx_begin_native_scene_target(command.flags, command.values[0], command.values[1],
            command.values[2] != 0);
        break;
    case Kind::ClearUiDepth:
        inner->gfx_clear_ui_depth(command.values[0], command.values[1], command.values[2], command.values[3]);
        break;
    case Kind::DrawSprite:
        inner->gfx_draw_sprite(*command.images[0], command.rect);
        break;
    case Kind::SpriteClear:
        inner->gfx_sprite_clear();
        break;
    case Kind::SpriteSubmit:
    {
        const auto& s = command.sprite;
        inner->gfx_sprite_submit(*command.images[0], s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7],
                                 command.sprite_color);
        break;
    }
    case Kind::UpdateImage:
    {
        RgbaImage& image = *command.images[0];
        image.width = command.pixels.width;
        image.height = command.pixels.height;
        image.bgra.swap(command.pixels.bgra);
        image.serial = command.pixels.serial;
        image.linear = command.pixels.linear;
        image.texel_width = command.pixels.texel_width;
        image.texel_height = command.pixels.texel_height;
        image.movie_slot = command.pixels.movie_slot;
        image.movie_seen = command.pixels.movie_seen;
        image.movie_front = command.pixels.movie_front;
        break;
    }
    case Kind::EndTargetInto:
    {
        // Step 20: the proxy takes the serial the game gave the image, then the backend copies the target into it.
        RgbaImage& image = *command.images[0];
        image.serial = command.flags;
        if (!inner->gfx_end_native_target_into(image))
        {
            throw Error("gfx render thread: the backend declined a render target end queued for it");
        }
        break;
    }
    case Kind::CopyUiInto:
    {
        // Step 25: as EndTargetInto, the proxy takes the image's serial and the backend copies the picture into it.
        RgbaImage& image = *command.images[0];
        image.serial = command.flags;
        if (!inner->gfx_copy_ui_into(image))
        {
            throw Error("gfx render thread: the backend declined a back-buffer copy queued for it");
        }
        break;
    }
    case Kind::EndSceneTargetInto:
    {
        // Step 27: as EndTargetInto, for the target that shares the back buffer's depth-stencil.
        RgbaImage& image = *command.images[0];
        image.serial = command.flags;
        if (!inner->gfx_end_native_scene_target_into(image))
        {
            throw Error("gfx render thread: the backend declined a scene target end queued for it");
        }
        break;
    }
    case Kind::ReleaseImage:
        inner->gfx_release_sprite(*command.images[0]);
        break;
    case Kind::EndFrame:
        inner->gfx_end_frame();
        break;
    case Kind::PresentAgain:
        inner->gfx_present_again();
        break;
    }
}

void run()
{
    for (;;)
    {
        Batch* batch = nullptr;
        bool skip = false;
        {
            std::unique_lock lock(shared.mutex);
            shared.work.wait(lock, [] { return shared.stop || !shared.pending.empty(); });
            if (shared.pending.empty() || shared.discard)
            {
                return;
            }
            batch = shared.pending.front();
            shared.pending.pop_front();
            shared.busy = true;
            skip = shared.failed;
        }
        std::exception_ptr failure;
        if (!skip)
        {
            try
            {
                for (size_t n = 0; n < batch->count; ++n)
                {
                    execute(batch->commands[n]);
                }
            }
            catch (...)
            {
                failure = std::current_exception();
            }
        }
        const u64 presents = batch->presents;
        const u64 ends = batch->ends;
        batch->reset();
        {
            std::lock_guard lock(shared.mutex);
            if (failure)
            {
                shared.failed = true;
                shared.failure = failure;
            }
            if (ends > 0 && render_profile::enabled)
            {
                render_profile::merge(shared.profile, render_profile::frame);
                render_profile::frame = {};
            }
            shared.presents_done += presents;
            shared.ends_done += ends;
            shared.spare.push_back(batch);
            shared.busy = false;
        }
        shared.idle.notify_all();
    }
}

// Rethrows a render-thread failure on the game thread, once. Later commands are dropped.
void rethrow_locked()
{
    if (shared.failure)
    {
        std::exception_ptr failure = shared.failure;
        shared.failure = nullptr;
        std::rethrow_exception(failure);
    }
}

Batch* spare_locked()
{
    if (shared.spare.empty())
    {
        batches.push_back(std::make_unique<Batch>());
        return batches.back().get();
    }
    Batch* batch = shared.spare.back();
    shared.spare.pop_back();
    return batch;
}

void flush()
{
    if (current->count == 0)
    {
        return;
    }
    std::exception_ptr failure;
    {
        std::lock_guard lock(shared.mutex);
        shared.pending.push_back(current);
        current = spare_locked();
        std::swap(failure, shared.failure);
    }
    shared.work.notify_one();
    if (failure)
    {
        std::rethrow_exception(failure);
    }
}

// Waits until the render thread has run everything queued; backend calls are then safe on this thread.
void drain()
{
    flush();
    std::unique_lock lock(shared.mutex);
    shared.idle.wait(lock, [] { return shared.pending.empty() && !shared.busy; });
    rethrow_locked();
}

Command& push(Kind kind)
{
    if (current->count >= kBatchCommands || current->bytes >= kBatchBytes)
    {
        flush();
    }
    if (current->count == current->commands.size())
    {
        current->commands.emplace_back();
    }
    Command& command = current->commands[current->count++];
    command.kind = kind;
    return command;
}

void copy_image(RgbaImage& to, const RgbaImage& from)
{
    to.width = from.width;
    to.height = from.height;
    to.bgra.assign(from.bgra.begin(), from.bgra.end());
    to.serial = from.serial;
    to.linear = from.linear;
    to.texel_width = from.texel_width;
    to.texel_height = from.texel_height;
    to.movie_slot = from.movie_slot;
    to.movie_seen = from.movie_seen;
    to.movie_front = from.movie_front;
}

// The proxy for `source`. A new serial queues the new pixels ahead of the command that is about to use them.
const std::shared_ptr<RgbaImage>& image_proxy(const RgbaImage& source)
{
    auto [it, inserted] = proxies.try_emplace(&source);
    Proxy& proxy = it->second;
    if (inserted)
    {
        proxy.image = std::make_shared<RgbaImage>();
        copy_image(*proxy.image, source);
        proxy.serial = source.serial;
        proxy.width = source.width;
        proxy.height = source.height;
    }
    else if (proxy.serial != source.serial)
    {
        Command& update = push(Kind::UpdateImage);
        update.images[0] = proxy.image;
        copy_image(update.pixels, source);
        proxy.serial = source.serial;
        proxy.width = source.width;
        proxy.height = source.height;
    }
    return proxy.image;
}

Program& intern(std::span<const u32> words)
{
    std::unique_ptr<Program>& slot = programs[words.data()];
    if (slot && slot->words.size() == words.size()
        && std::memcmp(slot->words.data(), words.data(), words.size_bytes()) == 0)
    {
        return *slot;
    }
    if (slot)
    {
        replaced_programs.push_back(std::move(slot));
    }
    slot = std::make_unique<Program>();
    slot->words.assign(words.begin(), words.end());
    program_constant_range(slot->words, slot->constants_first, slot->constants_count);
    return *slot;
}

void age_retained()
{
    if (frame_number % 64 != 0)
    {
        return;
    }
    std::erase_if(retained, [](const auto& entry)
    {
        return frame_number - entry.second->last_used > kRetainedIdleFrames;
    });
    if (frame_number % 640 == 0)
    {
        log::verbose("gfx render thread: %zu images, %zu retained geometry, %zu programs, %zu batches",
                     proxies.size(), retained.size(), programs.size() + replaced_programs.size(), batches.size());
    }
}

// Blocks until every present queued so far has run.
void wait_presented()
{
    if (!running)
    {
        return;
    }
    flush();
    std::unique_lock lock(shared.mutex);
    shared.idle.wait(lock, [] { return shared.presents_done >= presents_queued; });
    rethrow_locked();
}

// Stops the thread when the process exits without gfx_shutdown; anything still queued is dropped.
struct Stopper
{
    std::thread thread;
    ~Stopper()
    {
        if (thread.joinable())
        {
            {
                std::lock_guard lock(shared.mutex);
                shared.stop = true;
                shared.discard = true;
            }
            shared.work.notify_all();
            thread.join();
        }
    }
};
Stopper worker;

} // namespace

bool gfx_init(const GfxConfig& cfg)
{
    if (!inner->gfx_init(cfg))
    {
        return false;
    }
    shared.stop = false;
    shared.discard = false;
    current = spare_locked();
    running = true;
    worker.thread = std::thread(run);
    log::info("gfx: %s backend on a render thread", inner->gfx_backend_name());
    return true;
}

void gfx_shutdown()
{
    std::exception_ptr failure;
    if (running)
    {
        try
        {
            drain();
        }
        catch (...)
        {
            failure = std::current_exception();
        }
        {
            std::lock_guard lock(shared.mutex);
            shared.stop = true;
        }
        shared.work.notify_all();
        worker.thread.join();
        running = false;
    }
    inner->gfx_shutdown();
    proxies.clear();
    retained.clear();
    programs.clear();
    replaced_programs.clear();
    if (failure)
    {
        std::rethrow_exception(failure);
    }
}

const char* gfx_backend_name()
{
    return inner->gfx_backend_name();
}

bool gfx_has_window()
{
    return inner->gfx_has_window();
}

void gfx_show_startup_loading_screen(const RgbaImage& text, float progress)
{
    if (running)
    {
        drain();
    }
    inner->gfx_show_startup_loading_screen(text, progress);
}

bool gfx_prepare_native_pixel_definition(const u32* definition)
{
    if (running)
    {
        drain();
    }
    return inner->gfx_prepare_native_pixel_definition(definition);
}

void gfx_compiled_shader_permutations(std::vector<std::vector<u32>>& vertex_programs,
                                      std::vector<std::array<u32, 60>>& pixel_definitions)
{
    if (running)
    {
        drain();
    }
    inner->gfx_compiled_shader_permutations(vertex_programs, pixel_definitions);
}

bool gfx_overlay_device(GfxOverlayDevice& device)
{
    return inner->gfx_overlay_device(device);
}

// Window events stay on the thread that created the window; the backend hands a resize to the render thread itself.
void gfx_pump(bool& quit)
{
    inner->gfx_pump(quit);
}

void gfx_begin_frame()
{
    if (!running)
    {
        inner->gfx_begin_frame();
        return;
    }
    {
        // One frame may still be presenting while this one is recorded.
        std::unique_lock lock(shared.mutex);
        shared.idle.wait(lock, [] { return shared.ends_done + 1 >= ends_queued; });
        rethrow_locked();
        if (render_profile::enabled)
        {
            render_profile::merge(render_profile::frame, shared.profile);
            shared.profile = {};
        }
    }
    ++frame_number;
    age_retained();
    push(Kind::BeginFrame);
}

void gfx_clear_ui_depth(int left, int top, int right, int bottom)
{
    if (!running)
    {
        inner->gfx_clear_ui_depth(left, top, right, bottom);
        return;
    }
    push(Kind::ClearUiDepth).values = {left, top, right, bottom};
}

void gfx_draw_sprite(const RgbaImage& image, const GfxRect& rect)
{
    if (!running)
    {
        inner->gfx_draw_sprite(image, rect);
        return;
    }
    std::shared_ptr<RgbaImage> proxy = image_proxy(image);
    Command& command = push(Kind::DrawSprite);
    command.images[0] = std::move(proxy);
    command.rect = rect;
}

void gfx_release_sprite(const RgbaImage& image)
{
    if (!running)
    {
        inner->gfx_release_sprite(image);
        return;
    }
    auto found = proxies.find(&image);
    if (found == proxies.end())
    {
        return; // The backend never saw it.
    }
    push(Kind::ReleaseImage).images[0] = std::move(found->second.image);
    proxies.erase(found);
}

void gfx_sprite_clear()
{
    if (!running)
    {
        inner->gfx_sprite_clear();
        return;
    }
    push(Kind::SpriteClear);
}

void gfx_sprite_submit(const RgbaImage& image, float x, float y, float w, float h, float u0, float v0, float u1,
                       float v1, std::array<float, 4> color)
{
    if (!running)
    {
        inner->gfx_sprite_submit(image, x, y, w, h, u0, v0, u1, v1, color);
        return;
    }
    std::shared_ptr<RgbaImage> proxy = image_proxy(image);
    Command& command = push(Kind::SpriteSubmit);
    command.images[0] = std::move(proxy);
    command.sprite = {x, y, w, h, u0, v0, u1, v1};
    command.sprite_color = color;
}

// The answer for a program is asked once, with the render thread idle, and kept beside its interned words.
bool gfx_native_vertex_program_supported(std::span<const u32> program)
{
    if (!running)
    {
        return inner->gfx_native_vertex_program_supported(program);
    }
    Program& interned = intern(program);
    if (!interned.queried)
    {
        drain();
        interned.supported = inner->gfx_native_vertex_program_supported(interned.words);
        interned.queried = true;
    }
    return interned.supported;
}

void gfx_validate_native_boundary()
{
    if (running)
    {
        drain();
    }
    inner->gfx_validate_native_boundary();
}

// Everything the draw points at is copied or pinned: the presenter reuses its scratch buffers for the next draw, and
// the next tick changes game memory, images and shader programs while this draw waits.
void gfx_draw_native(const GfxNativeDraw& draw)
{
    if (!running)
    {
        inner->gfx_draw_native(draw);
        return;
    }
    std::array<std::shared_ptr<RgbaImage>, 4> images;
    for (size_t stage = 0; stage < images.size(); ++stage)
    {
        if (draw.textures[stage] != nullptr)
        {
            images[stage] = image_proxy(*draw.textures[stage]);
        }
    }
    const bool program_draw = !draw.program_inputs.empty();
    Program* program = program_draw ? &intern(draw.vertex_program) : nullptr;
    std::shared_ptr<RetainedCopy> kept;
    if (program_draw && draw.retained_geometry != 0 && !draw.indices.empty())
    {
        std::shared_ptr<RetainedCopy>& entry = retained[draw.retained_geometry];
        if (!entry)
        {
            entry = std::make_shared<RetainedCopy>();
            entry->inputs.assign(draw.program_inputs.begin(), draw.program_inputs.end());
            entry->indices.assign(draw.indices.begin(), draw.indices.end());
        }
        entry->last_used = frame_number;
        kept = entry;
    }

    Command& command = push(Kind::DrawNative);
    Batch& batch = *current;
    command.draw = draw;
    command.images = std::move(images);
    for (size_t stage = 0; stage < command.images.size(); ++stage)
    {
        command.draw.textures[stage] = command.images[stage].get();
    }
    command.draw.vertices = batch.copy(draw.vertices);
    command.draw.pixel_shader = draw.pixel_shader != nullptr
        ? batch.copy(std::span<const u32>(draw.pixel_shader, 60)).data() : nullptr;
    if (program != nullptr)
    {
        command.draw.vertex_program = program->words;
        if (draw.vertex_constants != nullptr)
        {
            // The backend reads only the registers the program can (step 10), so only those are copied. Step 21:
            // draws with the constants id of the previous copy in this batch share it when it holds their registers.
            using Constants = std::array<std::array<float, 4>, 192>;
            const u32 first = program->constants_first;
            const u32 count = program->constants_count;
            ConstantsCopy& last = batch.last_constants;
            if (draw.vertex_constants_id != 0 && last.copy != nullptr && last.id == draw.vertex_constants_id
                && first >= last.first && first + count <= last.first + last.count)
            {
                command.draw.vertex_constants = last.copy;
            }
            else
            {
                auto* constants = static_cast<Constants*>(batch.allocate(sizeof(Constants)));
                std::memcpy(&(*constants)[first], &(*draw.vertex_constants)[first],
                            size_t(count) * sizeof((*constants)[0]));
                command.draw.vertex_constants = constants;
                last = {constants, draw.vertex_constants_id, first, count};
            }
        }
        if (kept)
        {
            command.draw.program_inputs = kept->inputs;
            command.draw.indices = kept->indices;
            command.retained = std::move(kept);
        }
        else
        {
            command.draw.program_inputs = batch.copy(draw.program_inputs);
            command.draw.indices = batch.copy(draw.indices);
        }
    }
    else
    {
        command.draw.vertex_program = {};
        command.draw.vertex_constants = nullptr;
        command.draw.indices = batch.copy(draw.indices);
    }
}

void gfx_begin_native_batch()
{
    if (!running)
    {
        inner->gfx_begin_native_batch();
        return;
    }
    push(Kind::BeginBatch);
}

void gfx_end_native_batch()
{
    if (!running)
    {
        inner->gfx_end_native_batch();
        return;
    }
    push(Kind::EndBatch);
}

void gfx_clear_native(std::span<const std::array<i32, 4>> rectangles, u32 flags, u32 color, float depth)
{
    if (!running)
    {
        inner->gfx_clear_native(rectangles, flags, color, depth);
        return;
    }
    Command& command = push(Kind::ClearNative);
    command.rectangles = current->copy(rectangles);
    command.flags = flags;
    command.color = color;
    command.depth = depth;
}

void gfx_begin_native_target(int width, int height, bool depth)
{
    if (!running)
    {
        inner->gfx_begin_native_target(width, height, depth);
        return;
    }
    push(Kind::BeginTarget).values = {width, height, depth ? 1 : 0, 0};
    target_width = width;
    target_height = height;
}

void gfx_end_native_target(RgbaImage& output)
{
    if (running)
    {
        drain();
    }
    inner->gfx_end_native_target(output);
}

// Step 20: the backend's draws see the image through its proxy, so the GPU copy is made into the proxy's texture, in
// queue order. The caller has just raised the image's serial; the proxy takes that serial without the image's pixels,
// which are the old ones, so no UpdateImage follows and later draws sample the copy. The backend keeps the copy for
// the proxy at that serial and reads nothing else of it. A proxy of another size first takes the image's pixels, so
// its size and buffer agree when the backend checks the size against the target's.
bool gfx_end_native_target_into(const RgbaImage& image)
{
    if (!running)
    {
        return inner->gfx_end_native_target_into(image);
    }
    if (!ends_into || image.width != target_width || image.height != target_height)
    {
        return false; // As the backend answers: it has no such path, or the sizes differ.
    }
    auto [it, inserted] = proxies.try_emplace(&image);
    Proxy& proxy = it->second;
    if (inserted)
    {
        proxy.image = std::make_shared<RgbaImage>();
        copy_image(*proxy.image, image);
    }
    else if (proxy.width != image.width || proxy.height != image.height)
    {
        Command& update = push(Kind::UpdateImage);
        update.images[0] = proxy.image;
        copy_image(update.pixels, image);
    }
    proxy.serial = image.serial;
    proxy.width = image.width;
    proxy.height = image.height;
    Command& command = push(Kind::EndTargetInto);
    command.images[0] = proxy.image;
    command.flags = image.serial;
    return true;
}

void gfx_read_ended_native_target(RgbaImage& output)
{
    if (running)
    {
        drain();
    }
    inner->gfx_read_ended_native_target(output);
}

void gfx_begin_native_scene_target(u32 key, int width, int height, bool own_depth)
{
    if (!running)
    {
        inner->gfx_begin_native_scene_target(key, width, height, own_depth);
        return;
    }
    Command& command = push(Kind::BeginSceneTarget);
    command.flags = key;
    command.values = {width, height, own_depth ? 1 : 0, 0};
}

void gfx_end_native_scene_target(RgbaImage& output)
{
    if (running)
    {
        drain();
    }
    inner->gfx_end_native_scene_target(output);
}

void gfx_read_native_target_depth(std::vector<u32>& output)
{
    if (running)
    {
        drain();
    }
    inner->gfx_read_native_target_depth(output);
}

void gfx_size(int& width, int& height)
{
    inner->gfx_size(width, height);
}

void gfx_end_frame()
{
    if (!running)
    {
        inner->gfx_end_frame();
        return;
    }
    push(Kind::EndFrame);
    ++current->presents;
    ++current->ends;
    ++presents_queued;
    ++ends_queued;
    flush();
}

void gfx_present_again()
{
    if (!running)
    {
        inner->gfx_present_again();
        return;
    }
    // At most one refresh waits, as a blocking present kept it before.
    wait_presented();
    push(Kind::PresentAgain);
    ++current->presents;
    ++presents_queued;
    flush();
}

bool gfx_readback(RgbaImage& out)
{
    if (running)
    {
        drain();
    }
    return inner->gfx_readback(out);
}

bool gfx_readback_ui(RgbaImage& out)
{
    if (running)
    {
        drain();
    }
    return inner->gfx_readback_ui(out);
}

// Read from the backend's configuration, which only the game thread changes.
void gfx_ui_frame_size(int& width, int& height)
{
    inner->gfx_ui_frame_size(width, height);
}

// Steps 25 and 27: a GPU copy into `image`, queued like gfx_end_native_target_into. The commands queued before it run
// first, and the proxy takes the image's serial without its pixels, so later draws sample the copy.
bool queue_copy_into(const RgbaImage& image, Kind kind)
{
    if (!ends_into || image.width <= 0 || image.height <= 0)
    {
        return false;
    }
    auto [it, inserted] = proxies.try_emplace(&image);
    Proxy& proxy = it->second;
    if (inserted)
    {
        proxy.image = std::make_shared<RgbaImage>();
        copy_image(*proxy.image, image);
    }
    else if (proxy.width != image.width || proxy.height != image.height)
    {
        Command& update = push(Kind::UpdateImage);
        update.images[0] = proxy.image;
        copy_image(update.pixels, image);
    }
    proxy.serial = image.serial;
    proxy.width = image.width;
    proxy.height = image.height;
    Command& command = push(kind);
    command.images[0] = proxy.image;
    command.flags = image.serial;
    return true;
}

bool gfx_copy_ui_into(const RgbaImage& image)
{
    if (!running)
    {
        return inner->gfx_copy_ui_into(image);
    }
    return queue_copy_into(image, Kind::CopyUiInto);
}

// Read from the backend's configuration, which only the game thread changes.
void gfx_scene_target_size(int& width, int& height)
{
    inner->gfx_scene_target_size(width, height);
}

bool gfx_end_native_scene_target_into(const RgbaImage& image)
{
    if (!running)
    {
        return inner->gfx_end_native_scene_target_into(image);
    }
    return queue_copy_into(image, Kind::EndSceneTargetInto);
}

const GfxBackend* backend_table(const GfxBackend& backend)
{
    static GfxBackend table = GBTG_GFX_BACKEND_TABLE("vulkan");
    // A gfx.h function this file does not define resolves to the public dispatcher, which would call itself.
#define GBTG_RENDER_THREAD_CHECK(fn)                                                           if (table.fn == &::gbtg::fn)                                                               {                                                                                              throw Error("gfx render thread: " #fn " is not wrapped");                              }
    GBTG_GFX_FUNCTIONS(GBTG_RENDER_THREAD_CHECK)
#undef GBTG_RENDER_THREAD_CHECK
    inner = &backend;
    table.name = backend.name;
    ends_into = std::strcmp(backend.name, "d3d11") == 0 || std::strcmp(backend.name, "vulkan") == 0;
    return &table;
}

} // namespace render_thread

const GfxBackend* gfx_render_thread_backend(const GfxBackend& backend)
{
    // Step 20: D3D11 too. Its immediate context is used by one thread at a time: the render thread, or the game
    // thread while the queue is drained (calls that return results, start-up overlay set-up). The window and its
    // messages stay on the game thread; the game's swap chain is windowed (borderless for full screen) and is
    // never resized, so Present on the render thread sends the window nothing to wait on.
    if (std::strcmp(backend.name, "vulkan") != 0 && std::strcmp(backend.name, "d3d11") != 0)
    {
        return nullptr;
    }
    return render_thread::backend_table(backend);
}

void gfx_render_thread_wait_presented()
{
    render_thread::wait_presented();
}

} // namespace gbtg
#endif
