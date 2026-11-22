[CmdletBinding()]
param(
    [string]$BuildRoot = 'D:\dev\build\Evoly_Aura',
    [string]$OutputRoot = 'D:\dev\tmp\Evoly_Aura\measurements',
    [ValidateSet('software','composition')][string]$Backend = 'software',
    [ValidateSet('solid','wallpaper','acrylic')][string]$Material = 'wallpaper',
    [ValidateRange(1,12)][int]$Panels = 6,
    [ValidateRange(1,86400)][int]$Seconds = 300,
    [ValidateRange(0,3600)][int]$WarmupSeconds = 60,
    [ValidateRange(1,3600)][int]$BaselineSeconds = 60,
    [ValidateRange(1,20)][int]$Repetitions = 3,
    [switch]$Desktop,
    [switch]$Hidden,
    [switch]$Exercise
)
$ErrorActionPreference = 'Stop'
if (($WarmupSeconds + $Seconds + 15) -gt 86400) { throw 'Total probe lifetime must not exceed 86400 seconds.' }
if (Get-Process AuraProbe -ErrorAction SilentlyContinue) { throw 'Close the existing AuraProbe before starting a controlled run.' }
$taskProbe = Join-Path $BuildRoot 'Release\AuraProbe.exe'
$taskSampler = Join-Path $BuildRoot 'Release\AuraMetrics.exe'
if (-not (Test-Path $taskProbe) -or -not (Test-Path $taskSampler)) { throw 'Build Release first.' }
$taskStamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')
$taskLabel = "$Backend-$Material-$Panels-$taskStamp"
$taskOutput = Join-Path $OutputRoot $taskLabel
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
$taskSource = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$taskGit = 'C:\Program Files\Git\cmd\git.exe'
if (-not (Test-Path $taskGit)) { $taskGit = 'git' }
$taskMetadata = [ordered]@{
    status = 'running'
    utc = $taskStamp
    commit = (& $taskGit -C $taskSource rev-parse HEAD)
    dirty = [bool](& $taskGit -C $taskSource status --porcelain)
    executableSha256 = (Get-FileHash $taskProbe -Algorithm SHA256).Hash
    backend = $Backend
    material = $Material
    panels = $Panels
    desktop = [bool]$Desktop
    hidden = [bool]$Hidden
    exercise = [bool]$Exercise
    warmupSeconds = $WarmupSeconds
    seconds = $Seconds
    repetitions = $Repetitions
    os = (Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber)
    cpu = (Get-CimInstance Win32_Processor | Select-Object Name,NumberOfLogicalProcessors)
    gpu = @(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,CurrentHorizontalResolution,CurrentVerticalResolution)
    memoryBytes = (Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory
}
$taskMetadata | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $taskOutput 'metadata.local.json') -Encoding utf8
$taskSession = (Get-Process -Id $PID).SessionId
$taskDwm = Get-Process dwm -ErrorAction SilentlyContinue | Where-Object SessionId -eq $taskSession | Select-Object -First 1
$taskDwmId = if ($taskDwm) { $taskDwm.Id } else { 0 }
try {
    for ($taskRun = 1; $taskRun -le $Repetitions; $taskRun++) {
        Write-Output "Run $taskRun/$Repetitions baseline ($BaselineSeconds seconds): $taskOutput"
        & $taskSampler --pid 0 --dwm-pid $taskDwmId --seconds $BaselineSeconds --output (Join-Path $taskOutput "baseline-$taskRun.csv")
        if ($LASTEXITCODE -ne 0) { throw 'Baseline sampler failed.' }
        $taskEvents = Join-Path $taskOutput "events-$taskRun.jsonl"
        $taskArguments = @('--backend', $Backend, '--material', $Material, '--panels', $Panels,
            '--duration', ($WarmupSeconds + $Seconds + 15), '--log', ('"' + $taskEvents + '"'))
        if ($Desktop) { $taskArguments += '--desktop' }
        if ($Hidden) { $taskArguments += '--hidden' }
        if ($Exercise) { $taskArguments += '--exercise' }
        $taskProcess = Start-Process -FilePath $taskProbe -ArgumentList $taskArguments -WindowStyle Hidden -PassThru
        try {
            & $taskSampler --pid $taskProcess.Id --dwm-pid $taskDwmId --seconds ($WarmupSeconds + $Seconds) --output (Join-Path $taskOutput "samples-$taskRun.csv")
            if ($LASTEXITCODE -ne 0) { throw 'Probe sampler failed.' }
            if (-not $taskProcess.WaitForExit(20000)) { throw 'Probe did not exit at its configured duration.' }
            if ($taskProcess.ExitCode -ne 0) { throw "Probe failed with exit code $($taskProcess.ExitCode)." }
            if (-not (Select-String -LiteralPath $taskEvents -Pattern '"event":"ready"' -Quiet)) { throw 'Missing readiness event.' }
            if ($Material -eq 'wallpaper' -and -not (Select-String -LiteralPath $taskEvents -Pattern '"event":"static_wallpaper_cached"' -Quiet)) {
                throw 'Requested wallpaper cache fell back; do not label this as a wallpaper benchmark.'
            }
            Write-Output "Completed run $taskRun."
        } finally {
            if (-not $taskProcess.HasExited) { Stop-Process -Id $taskProcess.Id }
        }
    }
    $taskMetadata.status = 'completed'
} catch {
    $taskMetadata.status = 'failed'
    throw
} finally {
    $taskMetadata | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $taskOutput 'metadata.local.json') -Encoding utf8
}
Write-Output "Raw local-only results: $taskOutput"
