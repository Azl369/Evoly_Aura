[CmdletBinding()]
param(
    [string]$BuildRoot = 'D:\dev\build\Evoly_Aura',
    [switch]$Desktop,
    [switch]$Preview
)
$ErrorActionPreference = 'Stop'
$taskExe = Join-Path $BuildRoot 'Release\Aura.exe'
if (-not (Test-Path -LiteralPath $taskExe)) {
    & (Join-Path $PSScriptRoot 'Build-Native.ps1') -BuildRoot $BuildRoot
}
if (-not (Test-Path -LiteralPath $taskExe)) { throw 'Aura.exe was not built.' }
if ($Desktop -and $Preview) { throw 'Choose either Desktop or Preview.' }
if ($Preview) {
    Start-Process -FilePath $taskExe -ArgumentList '--preview' -WindowStyle Hidden
} else {
    Start-Process -FilePath $taskExe -WindowStyle Hidden
}
