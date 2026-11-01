# Example plugins

Each folder is a standalone CMake project for one gbtg plugin library, written against `include/gbtg/plugin_api.h`
and built with `cmake/GbtgPlugin.cmake`. Copy one to start a plugin repository of your own; see
[docs/PLUGINS.md](../../docs/PLUGINS.md), "A plugin repository of its own".

| Example | Shows |
|---------|-------|
| [scene_log](scene_log) | A feature in plain C with boot and frame hooks, reading a `plugin.ini` setting |
| [dlc_story](dlc_story) | A data plugin (no library): a Features menu story and a scene of its own, as a recipe made with the inspector's scene builder |

Plugins only load in gbtg builds configured with `-DGBTG_RETAIL_GAME_ONLY=OFF`.
