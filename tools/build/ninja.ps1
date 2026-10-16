# Configure and build a Ninja preset from any shell. Ninja presets need the MSVC
# x64 environment (cl, link, the Windows SDK, and the ninja that ships with VS);
# this loads it from the newest Visual Studio install through vswhere.
#
#   tools\build\ninja.ps1                                   # ninja-release, target gbtg
#   tools\build\ninja.ps1 -Preset ninja-relpdb -Build ninja-relpdb-tests
#   tools\build\ninja.ps1 -Preset ninja-release -Target gbtg-transition-tests
param(
    [string]$Preset = "ninja-release",
    [string]$Build = "",
    [string[]]$Target = @(),
    [switch]$ConfigureOnly
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")

if (-not $env:VCINSTALLDIR)
{
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere))
    {
        throw "vswhere.exe was not found; install Visual Studio 2022 with the C++ workload."
    }
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs)
    {
        throw "No Visual Studio install with the x64 C++ tools was found."
    }
    $vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
    # Import the environment vcvars64 produces into this PowerShell process.
    cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
        if ($_ -match "^([^=]+)=(.*)$")
        {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
}

Push-Location $root
try
{
    cmake --preset $Preset
    if ($LASTEXITCODE -ne 0)
    {
        exit $LASTEXITCODE
    }
    if ($ConfigureOnly)
    {
        exit 0
    }
    if ($Build)
    {
        cmake --build --preset $Build
    }
    else
    {
        if ($Target.Count -eq 0)
        {
            $Target = @("gbtg")
        }
        cmake --build (Join-Path $root "out\$Preset") --target $Target
    }
    exit $LASTEXITCODE
}
finally
{
    Pop-Location
}
