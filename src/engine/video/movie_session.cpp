#include "movie_session.h"
#include <utility>
#include <cstring>

namespace gbtg
{
RuntimeMovieSession::RuntimeMovieSession(const std::filesystem::path& disc_root,
    MovieErrorContext& error, bool pal, bool pal_override,
    std::function<RuntimeMovieLockedSurface(void*)> lock_texture)
    : root_(std::filesystem::absolute(disc_root).string() + "\\"),
      worker_context_{worker_, worker_io_, counter_, {}, 0, diagnostic_, error},
      file_context_{file_io_, error, pending_, file_head_, file_count_, {}, 0,
          sectors_, &worker_context_},
      paths_{pal, pal_override, root_.c_str(), path_, error, {}, {}},
      startup_{paths_, file_context_, movie_head_, initialized_, preloaded_}
{
    initialize_performance_counter(counter_);
    bind_native_movie_file_io(file_io_, worker_io_);
    bind_native_file_worker_synchronization(worker_io_);
    const std::filesystem::path utility = std::filesystem::absolute("cache/xbox_utility").lexically_normal();
    auto map_utility = [utility](const char* path)
    {
        if (std::strncmp(path, "Z:\\", 3) == 0)
        {
            return (utility / (path + 3)).string();
        }
        return std::string(path);
    };
    auto create_file = worker_io_.create_file;
    worker_io_.create_file = [create_file, map_utility](const char* path, u32 access,
        u32 share, void* security, u32 creation, u32 attributes, std::uintptr_t template_file)
    {
        const std::string mapped = map_utility(path);
        return create_file(mapped.c_str(), access, share, security, creation, attributes, template_file);
    };
    auto attributes = worker_io_.attributes;
    worker_io_.attributes = [attributes, map_utility](const char* path)
    {
        const std::string mapped = map_utility(path);
        return attributes(mapped.c_str());
    };
    auto delete_file = file_io_.delete_file;
    file_io_.delete_file = [delete_file, map_utility](const char* path)
    {
        const std::string mapped = map_utility(path);
        return delete_file(mapped.c_str());
    };
    auto create_directory = worker_io_.create_directory;
    worker_io_.create_directory = [create_directory, map_utility](const char* path)
    {
        const std::string mapped = map_utility(path);
        return create_directory(mapped.c_str());
    };
    auto sector_size = file_io_.sector_size;
    file_io_.sector_size = [sector_size, utility](const char* drive)
    {
        const std::string mapped = drive[0] == 'Z' ? utility.root_path().string() : std::string(drive);
        return sector_size(mapped.c_str());
    };
    file_io_.format_utility = [utility]()
    {
        // The virtual Xbox utility volume is confined to this owned cache directory.
        // Filesystem exceptions retain actual host failure rather than simulated success.
        const auto cache = std::filesystem::weakly_canonical(
            std::filesystem::current_path() / "cache");
        const auto target = std::filesystem::weakly_canonical(utility);
        if (target.parent_path() != cache || target.filename() != "xbox_utility")
        {
            throw Error("Xbox utility volume resolves outside its owned cache directory");
        }
        std::filesystem::remove_all(target);
        std::filesystem::create_directories(utility);
    };
    boot_.disc_root = root_.c_str();
    boot_.root = boot_root_;
    initialize_runtime_movie_files(file_context_, boot_);
    g_runtime_file_copies = &copies_;
    pump_runtime_movie_files(file_context_);
    paths_.root = boot_root_;
    paths_.file_attributes = worker_io_.attributes;
    paths_.last_error = worker_io_.last_error;
    bind_native_movie_heap(startup_);
    bind_existing_movie_decoder(startup_);
    startup_.lock_texture = std::move(lock_texture);
}

RuntimeMovieSession::~RuntimeMovieSession()
{
    if (g_runtime_file_copies == &copies_)
    {
        g_runtime_file_copies = nullptr;
    }
    reset();
    if (preloaded_ != nullptr)
    {
        startup_.free_buffer(preloaded_);
    }
    delete[] boot_.first_buffer;
    delete[] boot_.second_buffer;
}

RuntimeMovieStartupContext& RuntimeMovieSession::startup()
{
    return startup_;
}

MovieFileContext& RuntimeMovieSession::files()
{
    return file_context_;
}

RuntimeFileCopyContext& RuntimeMovieSession::copies()
{
    return copies_;
}

void RuntimeMovieSession::pump()
{
    pump_runtime_movie_files(file_context_);
    RuntimeMovieState* movie = movie_head_.next;
    while (movie != nullptr)
    {
        tick_runtime_movie(*movie, startup_);
        movie = movie->next;
    }
}

void RuntimeMovieSession::reset()
{
    while (movie_head_.next != nullptr)
    {
        release_runtime_movie(*movie_head_.next, startup_);
    }
    pump_runtime_movie_files(file_context_);
    slots = {};
}
}
