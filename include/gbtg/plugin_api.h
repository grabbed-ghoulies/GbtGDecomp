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

/* A window message on Windows, as a window procedure receives it. Elsewhere GbtgOverlay.window_event gets the
 * SDL_Event itself. */
typedef struct GbtgWin32Message
{
    void* hwnd;
    uint32_t message;
    uintptr_t wparam;
    intptr_t lparam;
} GbtgWin32Message;

/* An overlay: host presentation drawn over each presented frame, such as the debug UI. Any hook may be NULL. */
typedef struct GbtgOverlay
{
    const char* id;
    /* Each window message or event before the game sees it: a GbtgWin32Message on Windows, an SDL_Event elsewhere.
     * Return 1 to take it from the game. */
    int (*window_event)(void* user, const void* event);
    /* 1 while the overlay takes the keyboard: the game reads no keys and Escape does not quit. */
    int (*captures_input)(void* user);
    /* On the game thread after each frame's tick, and before each repeated present between ticks, with the host
     * seconds since the last call. The previous build has been presented by then. */
    void (*build)(void* user, GbtgGame* game, float host_dt);
    /* 1 when render has something to draw. */
    int (*has_draws)(void* user);
    /* Draws over the frame about to be presented, possibly on a render thread. D3D11: `command` is NULL and the back
     * buffer is bound. Vulkan: `command` is the VkCommandBuffer, inside dynamic rendering onto a swapchain image of
     * GbtgGraphicsDevice.vk_color_format. */
    void (*render)(void* user, void* command);
    void* user;
} GbtgOverlay;

/* The graphics device and window (graphics_device), for an overlay's renderer. Vulkan handles are given as
 * pointers; VkFormat as its value. */
typedef struct GbtgGraphicsDevice
{
    uint32_t size; /* Set to sizeof(GbtgGraphicsDevice) before the call. */
    const char* backend; /* "d3d11" or "vulkan". */
    void* window; /* HWND on Windows, SDL_Window* elsewhere. */
    void* d3d11_device; /* ID3D11Device* */
    void* d3d11_context; /* ID3D11DeviceContext* (the immediate context) */
    uint32_t vk_api_version;
    void* vk_instance;
    void* vk_physical_device;
    void* vk_device;
    uint32_t vk_queue_family;
    void* vk_queue;
    int32_t vk_color_format;
    uint32_t vk_image_count;
} GbtgGraphicsDevice;

/* What the runtime is doing (runtime_info). Strings are valid until the next frame. */
typedef struct GbtgRuntimeInfo
{
    uint32_t size; /* Set to sizeof(GbtgRuntimeInfo) before the call. */
    const char* scene_name;
    uint32_t frame;
    uint32_t clock_frame;
    float clock_time;
    float dt; /* The clock's step and its uncapped value, in seconds. */
    float dt_raw;
    int target_hz;
    int pal;
    int script_phase;
    int script_hold;
    const char* requested_script;
    const char* presenter; /* The gfx backend. */
    int width;
    int height;
    int has_native_scene; /* The fields below up to delayed_active are 0 without one. */
    int actor_start;
    size_t native_objects;
    uint32_t registered_objects;
    size_t address_bindings;
    uint32_t loading;
    const char* last_missing_resource; /* "" when none. */
    size_t delayed_active;
    size_t delayed_slots;
    size_t cached_bundles; /* Not counting common. */
    size_t demand_resources;
} GbtgRuntimeInfo;

/* An object the native scene factory owns: its retail address token, type, size in bytes and vtable address. */
typedef struct GbtgNativeObject
{
    uint32_t address;
    uint32_t type;
    uint32_t size;
    uint32_t vtable;
} GbtgNativeObject;

/* A cached bundle: its name ("common" first), entries and heap sizes in bytes. */
typedef struct GbtgBundleInfo
{
    const char* name;
    size_t entries;
    size_t cpu_bytes;
    size_t gpu_bytes;
} GbtgBundleInfo;

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

    /* The host copies the struct; `id` and `user` must outlive the process. Returns 1 when added. */
    int (*add_overlay)(void* plugin, const GbtgOverlay* overlay);
    /* Fills `out` (set out->size first) and returns 1 when the run has a window on the D3D11 or Vulkan backend.
     * Valid from graphics_ready until graphics_shutdown returns. */
    int (*graphics_device)(GbtgGame* game, GbtgGraphicsDevice* out);

    /* Read-only diagnostics. runtime_info fills `out` (set out->size first). The visitors are called in order. */
    void (*runtime_info)(const GbtgGame* game, GbtgRuntimeInfo* out);
    void (*native_objects)(const GbtgGame* game, void (*visit)(void* context, const GbtgNativeObject* object),
        void* context);
    void (*delayed_callbacks)(const GbtgGame* game, void (*visit)(void* context, size_t slot, uint32_t frames_left),
        void* context);
    void (*resource_bundles)(const GbtgGame* game, void (*visit)(void* context, const GbtgBundleInfo* bundle),
        void* context);
    /* The scene scripts in the game's scene order. */
    size_t (*scene_count)(const GbtgGame* game);
    const char* (*scene_at)(const GbtgGame* game, size_t index);
    /* Queues `name` (an aid_script) the way the game requests its next script (0x0011EB30). */
    void (*request_scene)(GbtgGame* game, const char* name);
} GbtgPluginHost;

typedef int (*GbtgPluginLoad)(const GbtgPluginHost* host);
#define GBTG_PLUGIN_LOAD_SYMBOL "gbtg_plugin_load"

#ifdef __cplusplus
}
#endif
