# Example plugins

Each folder is a standalone CMake project for one gbtg plugin library, written against `include/gbtg/plugin_api.h`
and built with `cmake/GbtgPlugin.cmake`. Copy one to start a plugin repository of your own; see
[docs/PLUGINS.md](../../docs/PLUGINS.md), "A plugin repository of its own".

| Example | Shows |
|---------|-------|
| [scene_log](scene_log) | A feature in plain C with boot and frame hooks, reading a `plugin.ini` setting |

Plugins only load in gbtg builds configured with `-DGBTG_RETAIL_GAME_ONLY=OFF`.
