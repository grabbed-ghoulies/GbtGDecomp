# Stages the shader cache a release ships beside gbtg.exe (<exe dir>\cache): shaders.gbtg plus exactly the
# spirv\ and dxbc\ entries its permutations compile to, built by this gbtg's own compilers. Stale entries from older
# builds never reach it, because each backend compiles from an empty cache in a scratch working directory.
#
# The shader warm-up compiles the manifest's permutations before the first frame, so each backend runs windowed
# for a few frames. dxbc\ is only written by GBTG_OPTIMIZE builds. Run it with the build you ship.
#
#   tools\build\stage_shader_cache.ps1 -Exe build-opt\Release\gbtg.exe -Out build-opt\Release\cache
#   tools\build\stage_shader_cache.ps1 -Exe build-opt\Release\gbtg.exe -Out out\shader-cache -Backends vulkan
#
# The staged spirv\ and shaders.gbtg also serve Linux (tools/linux/package.sh takes the folder).
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [Parameter(Mandatory = $true)][string]$Out,
    [string]$Manifest = "",
    [string]$GameDir = "",
    [string[]]$Backends = @("d3d11", "vulkan"),
    [int]$Frames = 5
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
if (-not $Manifest)
{
    $Manifest = Join-Path $root "cache\shaders.gbtg"
}
if (-not $GameDir)
{
    $GameDir = Join-Path $root "OriginalFiles"
}
$Exe = (Resolve-Path $Exe).Path
$Manifest = (Resolve-Path $Manifest).Path
$GameDir = (Resolve-Path $GameDir).Path
$exeDir = Split-Path $Exe -Parent
$Out = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Out)

# gbtg reads <exe dir>\cache before its working directory's: entries there would be hits, not compiles.
$shipped = Join-Path $exeDir "cache"
foreach ($kind in "spirv", "dxbc")
{
    if (Test-Path (Join-Path $shipped $kind))
    {
        throw "$shipped\$kind exists; remove it first so every permutation is compiled by this build."
    }
}

$work = Join-Path ([System.IO.Path]::GetTempPath()) ("gbtg-shader-cache-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path (Join-Path $work "cache") | Out-Null
try
{
    Copy-Item $Manifest (Join-Path $work "cache\shaders.gbtg")
    $log = Join-Path $exeDir "gbtg.log"
    foreach ($backend in $Backends)
    {
        Write-Host "staging $backend ..."
        $process = Start-Process -FilePath $Exe -WorkingDirectory $work -PassThru -Wait -NoNewWindow `
            -ArgumentList @("--windowed", "--gfx", $backend, "--game-dir", "`"$GameDir`"", "--frames", "$Frames")
        if ($process.ExitCode -ne 0)
        {
            throw "gbtg --gfx $backend exited with $($process.ExitCode); see $log"
        }
        $warm = Select-String -Path $log -Pattern "shader warm-up:" | Select-Object -Last 1
        if (-not $warm)
        {
            throw "gbtg --gfx $backend logged no shader warm-up; is the shader_warmup plugin built and enabled?"
        }
        Write-Host "  $($warm.Line.Trim())"
    }

    New-Item -ItemType Directory -Force -Path $Out | Out-Null
    foreach ($kind in "spirv", "dxbc")
    {
        $target = Join-Path $Out $kind
        if (Test-Path $target)
        {
            Remove-Item -Recurse -Force $target
        }
        $source = Join-Path $work "cache\$kind"
        if (Test-Path $source)
        {
            Copy-Item -Recurse $source $target
            Get-ChildItem $target -Filter *.tmp | Remove-Item -Force
        }
    }
    Copy-Item (Join-Path $work "cache\shaders.gbtg") (Join-Path $Out "shaders.gbtg") -Force

    $spirv = @(Get-ChildItem (Join-Path $Out "spirv") -File -ErrorAction SilentlyContinue).Count
    $dxbc = @(Get-ChildItem (Join-Path $Out "dxbc") -File -ErrorAction SilentlyContinue).Count
    Write-Host "staged $Out : shaders.gbtg, $spirv spirv, $dxbc dxbc"
    if ($Backends -contains "d3d11" -and $dxbc -eq 0)
    {
        Write-Warning "no dxbc entries: this build was not configured with GBTG_OPTIMIZE"
    }
}
finally
{
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}
