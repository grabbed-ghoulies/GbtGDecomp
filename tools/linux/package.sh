#!/bin/bash
# Stages a Linux (Steam Deck) release build into a tarball: gbtg, the shared libraries in thirdparty/ beside it, a launcher,
# third-party licences and a README. Runs inside the Steam Runtime sniper SDK container (see docs/LINUX.md).
# The disc dump is never packaged.
#
#   tools/linux/package.sh <build-dir> <output.tar.gz> [shader-cache-dir]
#
# shader-cache-dir is a folder staged by tools/build/stage_shader_cache.ps1; its shaders.gbtg and spirv/ ship in
# cache/ beside gbtg, which reads them before its own cache (dxbc/ is Windows-only and is left out).
set -euo pipefail

build="${1:?build directory}"
output="${2:?output tarball}"
shader_cache="${3:-}"
here="$(cd "$(dirname "$0")" && pwd)"
name="gbtg-linux-x86_64"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
root="$stage/$name"
mkdir -p "$root/licenses"

cp "$build/gbtg" "$root/"
strip --strip-unneeded "$root/gbtg"
# Third-party shared libraries live in thirdparty/ beside gbtg (its DT_RPATH is $ORIGIN/thirdparty).
# -P keeps the versioned symlinks (libavcodec.so.62 -> libavcodec.so.62.x.y).
mkdir -p "$root/thirdparty"
cp -P "$build"/thirdparty/lib*.so* "$root/thirdparty/"
cp "$here/gbtg.sh" "$root/gbtg.sh"
chmod +x "$root/gbtg" "$root/gbtg.sh"
cp "$here/README.txt" "$root/README.txt"
if [ -n "$shader_cache" ]; then
    mkdir -p "$root/cache"
    cp "$shader_cache/shaders.gbtg" "$root/cache/"
    cp -r "$shader_cache/spirv" "$root/cache/"
fi

copy_license()
{
    if [ -f "$1" ]; then
        cp "$1" "$root/licenses/$2"
    else
        echo "missing licence $1" >&2
        exit 1
    fi
}
copy_license "$build/ffmpeg_lgpl_build-prefix/src/ffmpeg_lgpl_build/COPYING.LGPLv2.1" FFmpeg-COPYING.LGPLv2.1
copy_license "$build/_deps/faudio-src/LICENSE" FAudio-LICENSE.txt
copy_license "$build/_deps/sdl3-src/LICENSE.txt" SDL3-LICENSE.txt
copy_license "$build/_deps/dxc_linux-src/LICENSE-LLVM.txt" DirectXShaderCompiler-LICENSE-LLVM.txt
copy_license "$build/_deps/dxc_linux-src/LICENSE-MS.txt" DirectXShaderCompiler-LICENSE-MS.txt
copy_license "$build/_deps/vma-src/LICENSE.txt" VulkanMemoryAllocator-LICENSE.txt
copy_license "$build/_deps/imgui-src/LICENSE.txt" DearImGui-LICENSE.txt
copy_license "$build/_deps/zlib-src/LICENSE" zlib-LICENSE.txt

# Retail files must never ship.
if find "$root" \( -iname '*.xbe' -o -iname '*.bnl' -o -iname '*.iso' -o -iname '*.original' -o -iname '*.bik' \) | grep -q .; then
    echo "refusing to package retail files" >&2
    exit 1
fi

mkdir -p "$(dirname "$output")"
tar -C "$stage" -czf "$output" "$name"
echo "packaged $output"
