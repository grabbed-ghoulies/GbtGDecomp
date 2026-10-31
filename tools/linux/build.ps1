# Builds the Linux (Steam Deck) gbtg inside the Steam Runtime 3 "sniper" SDK container and, with -Package, writes
# out/gbtg-linux-x86_64.tar.gz. Needs Docker. The build tree lives in a Docker volume (fast; the source is
# bind-mounted read-only). See docs/LINUX.md.
#
#   tools\linux\build.ps1                    # Release build of gbtg
#   tools\linux\build.ps1 -Package           # ...and the tarball
#   tools\linux\build.ps1 -Config Debug
#   tools\linux\build.ps1 -Retail            # the retail presenter path (GBTG_OPTIMIZE=OFF)
#
# Builds use GBTG_OPTIMIZE=ON by default: the Steam Deck's CPU needs the presenter optimizations
# (docs/RENDERING_PERFORMANCE_PLAN.md) to hold 30 fps in heavy scenes. Output matches the retail path.
param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$Config = "Release",
    [switch]$Package,
    [switch]$Retail,
    [string]$Image = "registry.gitlab.steamos.cloud/steamrt/sniper/sdk:latest",
    [string]$Volume = "gbtg-linux-build"
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$optimize = if ($Retail) { "OFF" } else { "ON" }
$build = "/build/" + $Config.ToLowerInvariant() + $(if ($Retail) { "-retail" } else { "" })
$script = @"
set -e
cmake -S /src -B $build -G Ninja -DCMAKE_BUILD_TYPE=$Config -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 -DGBTG_GFX=vulkan -DGBTG_WITH_SDL=ON -DGBTG_OPTIMIZE=$optimize -DBUILD_TESTING=OFF
cmake --build $build --target gbtg
"@
if ($Package)
{
    $script += "`nbash /src/tools/linux/package.sh $build /dist/gbtg-linux-x86_64.tar.gz"
}
New-Item -ItemType Directory -Force (Join-Path $root "out") | Out-Null
docker run --rm `
    -v "${root}:/src:ro" `
    -v "${Volume}:/build" `
    -v "$(Join-Path $root 'out'):/dist" `
    $Image bash -c ($script -replace "`r", "")
exit $LASTEXITCODE
