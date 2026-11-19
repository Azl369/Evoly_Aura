[CmdletBinding()]
param(
    [string]$BuildRoot = 'D:\dev\build\Evoly_Aura',
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
$taskSource = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$taskCmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($taskCmake) {
    $taskCmakePath = $taskCmake.Source
} else {
    $taskVswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $taskVswhere)) { throw 'Install MSVC Build Tools and CMake first; configurable tooling belongs on D drive.' }
    $taskVs = & $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $taskCmakePath = Join-Path $taskVs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}
if (-not (Test-Path $taskCmakePath)) { throw 'CMake was not found.' }
& $taskCmakePath -S $taskSource -B $BuildRoot -G 'Visual Studio 17 2022' -A x64 -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
& $taskCmakePath --build $BuildRoot --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
$taskCtest = Join-Path (Split-Path $taskCmakePath) 'ctest.exe'
& $taskCtest --test-dir $BuildRoot -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Native tests failed.' }
