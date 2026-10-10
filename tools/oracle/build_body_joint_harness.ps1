param([string]$BuildDirectory = 'build')

$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$buildPath = (Resolve-Path (Join-Path $repository $BuildDirectory)).Path
$oraclePath = Join-Path $repository 'cache/oracle'
New-Item -ItemType Directory -Force -Path $oraclePath | Out-Null

# Reuse the built game's actual methods, excluding its process entry point.
# The core library alone does not contain the engine camera and actor modules.
$objects = Get-ChildItem (Join-Path $buildPath 'gbtg.dir/Release') -Filter '*.obj' |
    Where-Object Name -ne 'main.obj' | ForEach-Object { '"' + $_.FullName + '"' }
$response = Join-Path $oraclePath 'body_joint_objects.rsp'
Set-Content -LiteralPath $response -Value $objects -Encoding ascii
[xml]$project = Get-Content -Raw (Join-Path $buildPath 'gbtg.vcxproj')
$configuration = $project.Project.ItemDefinitionGroup |
    Where-Object { $_.Condition -match 'Release\|x64' }
$libraries = foreach ($dependency in $configuration.Link.AdditionalDependencies.Split(';'))
{
    if ($dependency -match '[\\/]')
    {
        '"' + (Join-Path $buildPath $dependency) + '"'
    }
    else
    {
        $dependency
    }
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$visualStudio)
{
    throw 'Visual Studio C++ tools were not found.'
}
$developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
$commands = @"
@echo off
call "$developerCommand" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cd /d "$repository"
cl /nologo /std:c++20 /O2 /MD /EHsc /Iinclude /Isrc /Isrc/engine tools/oracle/body_joint_harness.cpp /c /Fo:"$oraclePath/body_joint_harness.obj"
if errorlevel 1 exit /b %errorlevel%
link /nologo /OUT:"$oraclePath/body_joint_harness.exe" /OPT:REF "$oraclePath/body_joint_harness.obj" @"$response" $($libraries -join ' ')
exit /b %errorlevel%
"@
$batch = Join-Path $oraclePath 'build_body_joint_harness.bat'
Set-Content -LiteralPath $batch -Value $commands -Encoding ascii
& $batch
if ($LASTEXITCODE -ne 0)
{
    throw "Body joint harness build failed with exit code $LASTEXITCODE."
}
