#include "native_movies.h"
#include "../scene/executable_resources.h"
#include "gbtg/game.h"

namespace gbtg
{
NativeMovieRuntime* g_native_movie_runtime = nullptr;

NativeMovieRuntime::~NativeMovieRuntime()
{
    if (g_native_movie_runtime == this)
    {
        g_native_movie_runtime = nullptr;
    }
}

NativeMovieRuntime& native_movie_runtime()
{
    if (g_native_movie_runtime == nullptr)
    {
        throw Error("native movie runtime before boot 0x00033030");
    }
    return *g_native_movie_runtime;
}

std::shared_ptr<NativeMovieRuntime> boot_native_movies(const Game& game)
{
    if (game.executable_resources == nullptr)
    {
        throw Error("native movie boot needs the executable image for DAT_0042A180");
    }
    const auto& image = *game.executable_resources;
    auto movies = std::make_shared<NativeMovieRuntime>();
    // DAT_0042A180: {code, title, message} rows of 12 bytes up to a zero code.
    for (u32 row = 0x0042A180;; row += 12)
    {
        const auto words = image.memory(row, 12);
        const i32 code = i32(read_u32(words, 0));
        const u32 title = read_u32(words, 4);
        const u32 message = read_u32(words, 8);
        movies->messages.push_back(MovieErrorMessage{code, title == 0 ? nullptr : image.string(title),
            message == 0 ? nullptr : image.string(message)});
        if (code == 0)
        {
            break;
        }
    }
    movies->errors.messages = movies->messages.data();
    // Boot 0x00033030 runs 0x000E9CE0 and 0x000E9E60 over the disc root; the scene sets the texture lock.
    movies->session = std::make_unique<RuntimeMovieSession>(game.aids.game_dir, movies->errors, false, false,
        [](void*) -> RuntimeMovieLockedSurface
        {
            throw Error("movie frame copy 0x00107360 has no scene texture to lock");
        });
    g_native_movie_runtime = movies.get();
    return movies;
}

void tick_native_movie_list(NativeMovieRuntime& movies)
{
    if (movies.list_lock_a == 0 && movies.list_lock_b == 0)
    {
        auto& startup = movies.session->startup();
        for (RuntimeMovieState* movie = startup.head.next; movie != nullptr; movie = movie->next)
        {
            tick_runtime_movie(*movie, startup); // 0x00107420
        }
    }
}

// Xbox 0x00106C70
void release_finished_movie_events(std::array<RuntimeMovieEventSlot, 4>& slots,
    RuntimeMovieStartupContext& context)
{
    for (auto& slot : slots)
    {
        RuntimeMovieState* movie = slot.movie;
        if (movie != nullptr && 1 < slot.state &&
            (movie->unknown28 != 0 ||
                (movie->decoder != nullptr && movie->loop == 0 &&
                    context.decoder_frame_count(movie->decoder) <= context.decoder_frame_number(movie->decoder))) &&
            i32(movie->unknown20) < 1)
        {
            release_runtime_movie(*movie, context); // 0x001072B0
            slot.movie = nullptr;
            slot.id = 0;
            slot.state = 0;
        }
    }
}

// Xbox 0x00106E30
void release_movie_events(NativeMovieRuntime& movies)
{
    auto& startup = movies.session->startup();
    for (auto& slot : movies.session->slots)
    {
        if (slot.movie != nullptr)
        {
            release_runtime_movie(*slot.movie, startup); // 0x001072B0
            slot.movie = nullptr;
            slot.id = 0;
            slot.state = 0;
        }
    }
    movies.mode = 3;
    if (startup.preloaded != nullptr)
    {
        startup.free_buffer(startup.preloaded);
        startup.preloaded = nullptr;
    }
}

// Xbox 0x00106CD0
void copy_movie_event_frames(std::array<RuntimeMovieEventSlot, 4>& slots,
    const RuntimeSpecialTextures& hooks, RuntimeMovieStartupContext& context)
{
    for (auto& slot : slots)
    {
        if (slot.movie != nullptr)
        {
            const auto& hook = hooks[slot.id];
            if (hook.get_texture)
            {
                // 0x000528C0: the hooked texture's current D3D resource.
                const u32 texture = hook.get_texture(hook.model, hook.index);
                if (texture != 0)
                {
                    copy_runtime_movie_frame(*slot.movie, reinterpret_cast<void*>(std::uintptr_t(texture)),
                        context);
                }
            }
        }
    }
}
}
