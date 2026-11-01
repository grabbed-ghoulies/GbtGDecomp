# Linux and Steam Deck

`gbtg` builds natively for Linux, aimed at the Steam Deck (SteamOS, x86-64, Vulkan through RADV). It runs the same
translated retail code as on Windows. Only the host boundaries differ:

| Boundary | Windows | Linux |
|---|---|---|
| Window, events | Win32 | SDL3 (Wayland or X11) |
| Presenter | D3D11 or Vulkan | Vulkan, surface from SDL |
| Shader compiler (Vulkan) | `dxcompiler.dll` | `libdxcompiler.so` (DXC's Linux release) |
| Controllers, keyboard | XInput, `GetAsyncKeyState` | SDL3 gamepads and keyboard state (`runtime/sdl_input.cpp`) |
| Audio | XAudio2, X3DAudio | FAudio over SDL3 audio (`audio/xaudio2_host.h`) |
| Bink movies | FFmpeg (BtbN LGPL build) | FFmpeg 8.1 built from source, Bink only |
| Structured exceptions | `__try` / `__finally` / `__except` | `try` / `catch` through `include/gbtg/host_seh.h` |
| Heap size, aligned heap | `_msize`, `_aligned_malloc` | `malloc_usable_size`, `posix_memalign` (`include/gbtg/host_heap.h`) |
| Movie and file worker IO | Win32 file API, overlapped | POSIX `open` / `pread` / `pwrite`, completing at once |
| Xbox paths | `D:\` is the host folder path | `D:\` maps to the game folder, `\` becomes `/` (`xbox_host_path`) |
| Settings, log, saves, cache | beside the executable (`cache/` in the working directory) | `~/.local/share/gbtg` |

## Building

Builds run in Valve's Steam Runtime 3 "sniper" SDK container, so the binary uses the libraries SteamOS provides
(glibc 2.31 and newer). The SDK's GCC 14 links the C++ runtime statically.

From Windows, with Docker:

```powershell
tools\linux\build.ps1             # Release gbtg in the gbtg-linux-build Docker volume
tools\linux\build.ps1 -Package    # ...and out\gbtg-linux-x86_64.tar.gz
```

On Linux, inside the container (`docker run -it -v "$PWD:/src" registry.gitlab.steamos.cloud/steamrt/sniper/sdk`):

```bash
cmake --preset linux-release
cmake --build --preset linux-game
bash tools/linux/package.sh out/linux-release out/gbtg-linux-x86_64.tar.gz
```

Linux builds use `GBTG_OPTIMIZE=ON` (the `linux-*` presets and `tools/linux/build.ps1`; `-Retail` builds the retail
presenter path). The Steam Deck's CPU is several times slower than a desktop's, and heavy 3D scenes need the
presenter optimizations, retained geometry above all (`docs/RENDERING_PERFORMANCE_PLAN.md`, step 7), to stay near
the 33 ms frame. Their output matches the retail path pixel for pixel.

CMake fetches SDL3, FAudio, VulkanMemoryAllocator, DXC's Linux release and the FFmpeg source. FFmpeg is configured
with only the Bink demuxer, the Bink video and audio decoders, file input, swscale and swresample, as shared LGPL
libraries. Every shared library is copied beside `gbtg`, whose only library search path is `$ORIGIN`.

## The package

`out/gbtg-linux-x86_64.tar.gz` holds `gbtg`, the shared libraries, `gbtg.sh` (starts `gbtg` from its own folder),
`README.txt` and `licenses/`. It never holds retail files; `package.sh` refuses to package any.

Steam Deck setup is in `tools/linux/README.txt`: add `gbtg.sh` as a non-Steam game in Desktop Mode, and point
`GameDir` (or an `OriginalFiles` folder beside `gbtg.sh`) at the disc dump.

## Settings and data

Settings, `gbtg.log`, `saves/` and `cache/` (shader permutations and the SPIR-V cache) live in `$GBTG_DATA_DIR`, else
`$XDG_DATA_HOME/gbtg`, else `~/.local/share/gbtg`. That folder is the working directory while the game runs; the
game folder is resolved before the change.

Linux defaults in `gbtg.ini` suit the Deck: `Width=1280`, `Height=800`, `Fullscreen=1`, `Widescreen=1` (the Xbox's
16:9 setting, letterboxed on the 16:10 panel), `VSync=1`. `--fullscreen` and `--windowed` override `Fullscreen`.
`[Game] GameDir` names the disc dump; `--game-dir` overrides it. `[Video] RenderThread=0` (or `--render-thread 0`)
keeps the Vulkan backend on the game thread; by default it runs on its own thread (`GBTG_OPTIMIZE` step 11).

Logic runs on 60 Hz fields with a 1/30 s swap step, paced by the clock rather than the swap interval, so the game
runs at the same speed on the 60 Hz LCD and 90 Hz OLED panels.

## Testing without a Deck

Headless runs need no display. Without an audio device, use SDL's disk driver, which writes the final mix to a
file:

```bash
SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=mix.raw ./gbtg --game-dir /game --headless --gfx null --frames 900
```

Vulkan screenshots need a driver. The sniper SDK has none; an Ubuntu 24.04 container with `mesa-vulkan-drivers`
(lavapipe) renders `--headless --gfx vulkan --screenshot` captures that match the Windows Vulkan presenter to a few
pixels.

## Not yet covered

- `gbtg-transition-tests` and the other test executables still build only on Windows.
- The retail heap that reserves memory below 4 GB (`xapi_runtime.cpp`) has no Linux implementation; the scenes run
  so far do not reach it.
- The file worker never creates its thread on the host. POSIX cannot suspend another thread, so its
  `ResumeThread` / `SuspendThread` boundaries raise if one ever exists.
