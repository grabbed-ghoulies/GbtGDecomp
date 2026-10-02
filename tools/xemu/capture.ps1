param(
    [string]$Out = "",
    [string]$Monitor = "127.0.0.1:4510"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $Out) { $Out = Join-Path $Root "dump\xemu_preview.png" }

New-Item -ItemType Directory -Force -Path (Split-Path $Out) | Out-Null
$abs = [System.IO.Path]::GetFullPath($Out).Replace("\", "/")

$hostPort = $Monitor.Split(":")
$tcp = New-Object System.Net.Sockets.TcpClient
$tcp.Connect($hostPort[0], [int]$hostPort[1])
$stream = $tcp.GetStream()
$sw = New-Object System.IO.StreamWriter($stream)
$sw.NewLine = "`n"
$sw.AutoFlush = $true
$sr = New-Object System.IO.StreamReader($stream)
Start-Sleep -Milliseconds 200
while ($stream.DataAvailable) { [void]$sr.Read() }
$sw.WriteLine("screendump `"$abs`"")
Start-Sleep -Milliseconds 400
$tcp.Close()
if (-not (Test-Path $Out)) { throw "screendump did not write $Out" }
Write-Host "wrote $Out"
