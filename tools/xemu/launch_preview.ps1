param(
    [string]$GameDir = "",
    [string]$Xemu = "D:\Emulation\xemu\xemu.exe",
    [string]$Iso = "",
    [string]$Monitor = "127.0.0.1:4510",
    [switch]$KeepHdd,
    [switch]$SkipPatch,
    [switch]$SkipIso,
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$Root = [string]((Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path)
if (-not $GameDir) { $GameDir = Join-Path $Root "OriginalFiles" }
if (-not $Iso) { $Iso = Join-Path $Root "cache\GbtG_preview.iso" }

$running = @(Get-Process -Name xemu -ErrorAction SilentlyContinue)
if ($running.Count -gt 0) {
    if (-not $Force) {
        throw "xemu is already running (PID $($running.Id -join ', ')). Close that window, or re-run with -Force."
    }
    Write-Host "Stopping existing xemu (PID $($running.Id -join ', '))..."
    $running | Stop-Process -Force
    Start-Sleep -Seconds 1
}

if (-not $SkipPatch) {
    $patch = $null
    foreach ($cand in @(
            (Join-Path $Root "build\Release\gbtg-xemu-patch.exe")
            (Join-Path $Root "build\gbtg-xemu-patch.exe")
            (Join-Path $Root "out\Release\gbtg-xemu-patch.exe")
        )) {
        if (Test-Path $cand) { $patch = $cand; break }
    }
    if (-not $patch) { throw "gbtg-xemu-patch.exe not found. Build the repo, then re-run." }
    Write-Host "Applying preview patches..."
    & $patch --game-dir $GameDir
    if ($LASTEXITCODE -ne 0) { throw "gbtg-xemu-patch failed" }
}

if (-not $SkipIso) {
    & (Join-Path $PSScriptRoot "build_iso.ps1") -GameDir $GameDir -OutIso $Iso
}

if (-not (Test-Path $Xemu)) { throw "xemu not found: $Xemu" }
if (-not (Test-Path $Iso)) { throw "ISO not found: $Iso" }

$cfgSrc = Join-Path $env:APPDATA "xemu\xemu\xemu.toml"
$cfgDst = Join-Path $Root "cache\xemu_preview.toml"
New-Item -ItemType Directory -Force -Path (Join-Path $Root "cache") | Out-Null
if (Test-Path $cfgSrc) {
    Copy-Item $cfgSrc $cfgDst -Force
} else {
    @"
[sys.files]
bootrom_path = 'D:\Emulation\xemu\mcpx\mcpx_1.0.bin'
flashrom_path = 'D:\Emulation\xemu\bios\Complex_4627.bin'
hdd_path = 'D:\Emulation\xemu\hdd\xbox_hdd.qcow2'
"@ | Set-Content -Encoding utf8 $cfgDst
}

# One string: Start-Process -ArgumentList @() splits on commas, which breaks
# -machine xbox,short-animation=on and the QEMU monitor address.
$argLine = "-config_path `"$cfgDst`" -dvd_path `"$Iso`" -machine xbox,short-animation=on -monitor tcp:${Monitor},server,nowait"
if (-not $KeepHdd) { $argLine += " -snapshot" }

Write-Host "Launching xemu..."
$proc = Start-Process -FilePath $Xemu -ArgumentList $argLine -WorkingDirectory (Split-Path $Xemu) -PassThru
Start-Sleep -Seconds 2
if ($proc.HasExited) {
    $log = Join-Path (Split-Path $Xemu) "xemu.log"
    throw "xemu exited immediately (code $($proc.ExitCode)). See $log"
}
Write-Host "xemu PID $($proc.Id)  monitor $Monitor  (capture with tools\xemu\capture.ps1)"
Write-Host "VIEW is a baked look-at at {0,12,20}. After the hallway loads, the camera should sit on that pose."
