param(
    [string]$GameDir = ""
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $GameDir) { $GameDir = Join-Path $Root "OriginalFiles" }
$patch = $null
foreach ($cand in @(
        (Join-Path $Root "build\Release\gbtg-xemu-patch.exe")
        (Join-Path $Root "build\gbtg-xemu-patch.exe")
    )) {
    if (Test-Path $cand) { $patch = $cand; break }
}
if (-not $patch) { throw "gbtg-xemu-patch.exe not found. Build first." }
& $patch --game-dir $GameDir
exit $LASTEXITCODE
