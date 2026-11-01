#pragma once
/* The C interface between gbtg and a plugin library (a .dll on Windows, a .so elsewhere). Plain C, so a plugin
 * may be written in C or C++ and built with any compiler for the same platform. See docs/PLUGINS.md.
 *
 * A plugin library exports one function, gbtg_plugin_load. gbtg calls it once at boot, before Game::init, with a
 * host table for that plugin. The plugin registers its features through the table and returns 1, or returns 0 to
 * stay unloaded. Libraries stay loaded until the process exits.
 *
 * Bump GBTG_PLUGIN_API_VERSION for any change that breaks an existing plugin. Adding a member at the end of
 * GbtgPluginHost does not: a plugin can test `size` before it uses one. */
#include <stddef.h>
#include <stdint.h>

#define GBTG_PLUGIN_API_VERSION 1

#if defined(_WIN32)
#define GBTG_PLUGIN_EXPORT __declspec(dllexport)
#else
#define GBTG_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/* The running game. Opaque: read it through the host table. */
typedef struct GbtgGame GbtgGame;

enum GbtgLogLevel
{
    GBTG_LOG_VERBOSE = 0, /* Only with --verbose. */
    GBTG_LOG_INFO = 1,
    GBTG_LOG_WARN = 2,
    GBTG_LOG_ERROR = 3
};

/* A feature: host code the game calls at fixed points. Any hook may be NULL. A library's features run whenever its
 * plugin is enabled. */
typedef struct GbtgFeature
{
    const char* id;
    const char* summary;
    /* Once, after Game::init. */
    void (*boot)(void* user, GbtgGame* game);
    /* Each frame, after frame_tick returns. */
    void (*frame)(void* user, GbtgGame* game);
    /* Once, after the graphics device is up and before the first frame. The loading screen and the shader calls
     * below are available from here. */
    void (*graphics_ready)(void* user, GbtgGame* game);
    /* Once, after the last frame and before the device is released. */
    void (*graphics_shutdown)(void* user, GbtgGame* game);
    void* user;
} GbtgFeature;

/* Receives the compiled shader permutations (compiled_shaders): each vertex program as its words, each pixel
 * definition as its 60 words. */
typedef struct GbtgShaderSink
{
    void (*vertex_program)(void* context, const uint32_t* words, size_t count);
    void (*pixel_definition)(void* context, const uint32_t* words);
    void* context;
} GbtgShaderSink;

typedef struct GbtgPluginHost
{
    uint32_t api_version; /* GBTG_PLUGIN_API_VERSION of the running gbtg. */
    uint32_t size; /* sizeof(GbtgPluginHost) in the running gbtg. */
    void* plugin; /* This plugin; pass it back to the calls that take it. */

    void (*log)(void* plugin, int level, const char* message);
    /* A value from this plugin's plugin.ini (section and key ignore case), or NULL. The pointer lives as long as
     * the process. */
    const char* (*setting)(void* plugin, const char* section, const char* key);
    /* This plugin's folder and the executable's folder, UTF-8. */
    const char* (*plugin_dir)(void* plugin);
    const char* (*executable_dir)(void* plugin);
    /* The host copies the struct; the strings and `user` must outlive the process. Returns 1 when added. */
    int (*add_feature)(void* plugin, const GbtgFeature* feature);

    /* The game's language as its loctext folder: "english", "french", "german", "spanish" or "italian". */
    const char* (*language)(void);
    /* `key` from the loctext resource `resource` (an aid_loctext_ name) in the game's language, looked up the way
     * the game looks up its own text (0x000FF410). A plugin ships the resource as loctext/<language>/<hash> in its
     * folder. Writes UTF-8 to `out` and returns 1, or returns 0 when the resource or key is missing. Valid after
     * boot. */
    int (*loctext)(GbtgGame* game, const char* resource, const char* key, char* out, size_t size);

    /* Valid from graphics_ready. */
    /* 1 when the game has a window, 0 for offscreen runs (screenshots, headless). */
    int (*has_window)(GbtgGame* game);
    /* Draws `text` (UTF-8, in the game's startup font) centred on black with a progress bar at `progress` (0..1),
     * and presents it. Returns 0 without a window or when the font has none of the text's glyphs. */
    int (*show_loading_screen)(GbtgGame* game, const char* text, float progress);
    /* Compiles a vertex program or a pixel definition before its first draw. 1 when compiled, 0 when the backend
     * cannot run the vertex program or runs the pixel definition on its generic shader, -1 when shaders cannot be
     * compiled ahead in this run (no device, --render-reference, --cpu-vertex). */
    int (*prepare_vertex_program)(GbtgGame* game, const uint32_t* words, size_t count);
    int (*prepare_pixel_definition)(GbtgGame* game, const uint32_t* words);
    /* Calls `sink` for every vertex program and pixel definition the backend has compiled so far. */
    void (*compiled_shaders)(GbtgGame* game, const GbtgShaderSink* sink);

    /* The loaded scene script's AID, "" before the first. Valid until the next frame. */
    const char* (*scene_name)(const GbtgGame* game);
    uint32_t (*frame)(const GbtgGame* game);
} GbtgPluginHost;

typedef int (*GbtgPluginLoad)(const GbtgPluginHost* host);
#define GBTG_PLUGIN_LOAD_SYMBOL "gbtg_plugin_load"

#ifdef __cplusplus
}
#endif
