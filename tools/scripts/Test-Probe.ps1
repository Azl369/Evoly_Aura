[CmdletBinding()]
param([string]$BuildRoot = 'D:\dev\build\Evoly_Aura',
      [string]$OutputRoot = 'D:\dev\tmp\Evoly_Aura\smoke')
$ErrorActionPreference = 'Stop'
if (Get-Process AuraProbe -ErrorAction SilentlyContinue) { throw 'An AuraProbe instance is already running.' }
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
$taskProbe = Join-Path $BuildRoot 'Release\AuraProbe.exe'
foreach ($taskBackend in @('software', 'composition')) {
    foreach ($taskMaterial in @('solid','wallpaper','acrylic')) {
        $taskLog = Join-Path $OutputRoot "$taskBackend-$taskMaterial.jsonl"
        $taskProcess = Start-Process $taskProbe -ArgumentList "--backend $taskBackend --material $taskMaterial --duration 3 --log $taskLog" -WindowStyle Hidden -PassThru
        if (-not $taskProcess.WaitForExit(15000)) {
            Stop-Process -Id $taskProcess.Id
            throw "Smoke timeout: $taskBackend / $taskMaterial"
        }
        if ($taskProcess.ExitCode -ne 0) { throw "Smoke failed; inspect $taskLog" }
        $taskEvents = @(Get-Content $taskLog | ForEach-Object { $_ | ConvertFrom-Json })
        if (-not ($taskEvents | Where-Object event -eq 'ready')) { throw 'Readiness missing.' }
        if (-not ($taskEvents | Where-Object { $_.event -eq 'draw_count' -and $_.value -ge 6 })) { throw 'Six panels did not draw.' }
        Write-Output "PASS $taskBackend / $taskMaterial"
    }
}
