param(
    [string]$GameDir = "",
    [string]$OutIso = "",
    [string]$ExtractXiso = "",
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $GameDir) { $GameDir = Join-Path $Root "OriginalFiles" }
if (-not $OutIso) { $OutIso = Join-Path $Root "cache\GbtG_preview.iso" }
if (-not $ExtractXiso) { $ExtractXiso = Join-Path $Root "tools\extract-xiso\extract-xiso.exe" }
$Stage = Join-Path $Root "cache\xemu_root"

if (-not (Test-Path $GameDir)) { throw "Game dir not found: $GameDir" }
if (-not (Test-Path $ExtractXiso)) { throw "extract-xiso not found: $ExtractXiso" }

$xbe = Join-Path $GameDir "default.xbe"
$common = Join-Path $GameDir "bundles\common.bnl"
if ((-not $Force) -and (Test-Path $OutIso) -and (Test-Path $xbe) -and (Test-Path $common)) {
    $isoTime = (Get-Item $OutIso).LastWriteTimeUtc
    if ((Get-Item $xbe).LastWriteTimeUtc -le $isoTime -and (Get-Item $common).LastWriteTimeUtc -le $isoTime) {
        Write-Host "ISO up to date: $OutIso"
        return
    }
}

New-Item -ItemType Directory -Force -Path (Split-Path $OutIso) | Out-Null
if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory -Force -Path $Stage | Out-Null

Write-Host "Staging $GameDir -> $Stage (skipping *.original)"
robocopy $GameDir $Stage /E /XF *.original /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy failed with $LASTEXITCODE" }

Write-Host "Building XISO with extract-xiso..."
& $ExtractXiso -c $Stage $OutIso
if ($LASTEXITCODE -ne 0) { throw "extract-xiso failed with $LASTEXITCODE" }
Write-Host "Wrote $OutIso"
