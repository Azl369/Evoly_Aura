[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RunDirectory,
    [string]$Output
)
$ErrorActionPreference = 'Stop'
$taskMetadata = Get-Content (Join-Path $RunDirectory 'metadata.local.json') -Raw -Encoding utf8 | ConvertFrom-Json
if ($taskMetadata.status -ne 'completed') { throw 'Cannot summarize an incomplete run.' }
function Get-AuraStats($Rows, [string]$Field) {
    $taskValues = @($Rows | ForEach-Object { [double]$_.$Field } | Where-Object { $_ -ge 0 } | Sort-Object)
    if ($taskValues.Count -eq 0) { return $null }
    return [ordered]@{
        samples = $taskValues.Count
        median = $taskValues[[Math]::Floor(($taskValues.Count - 1) / 2)]
        p95 = $taskValues[[Math]::Ceiling($taskValues.Count * 0.95) - 1]
        max = $taskValues[-1]
        min = $taskValues[0]
    }
}
$taskMetrics = @('private_ws_bytes','private_bytes','working_set_bytes','cpu_percent','handles','gdi','user',
    'gpu_dedicated_bytes','gpu_shared_bytes','dwm_private_bytes','dwm_working_set_bytes',
    'dwm_gpu_dedicated_bytes','dwm_gpu_shared_bytes','system_commit_bytes')
$taskRuns = @()
for ($taskIndex = 1; $taskIndex -le $taskMetadata.repetitions; $taskIndex++) {
    $taskRows = @(Import-Csv (Join-Path $RunDirectory "samples-$taskIndex.csv"))
    $taskSteady = @($taskRows | Where-Object { [double]$_.elapsed_s -gt $taskMetadata.warmupSeconds })
    if ($taskSteady.Count -ne $taskMetadata.seconds) { throw "Unexpected steady sample count in run $taskIndex." }
    $taskBaseline = @(Import-Csv (Join-Path $RunDirectory "baseline-$taskIndex.csv"))
    $taskStats = [ordered]@{}
    $taskBaselineStats = [ordered]@{}
    foreach ($taskMetric in $taskMetrics) {
        $taskStats[$taskMetric] = Get-AuraStats $taskSteady $taskMetric
        $taskBaselineStats[$taskMetric] = Get-AuraStats $taskBaseline $taskMetric
    }
    $taskEvents = @(Get-Content (Join-Path $RunDirectory "events-$taskIndex.jsonl") | ForEach-Object { $_ | ConvertFrom-Json })
    if ($taskEvents | Where-Object { $_.event -match 'error' }) { throw 'Probe reported an error.' }
    $taskRuns += [ordered]@{
        repetition = $taskIndex
        steady = $taskStats
        baseline = $taskBaselineStats
        sampledPeakPrivateBytes = (Get-AuraStats $taskRows 'private_bytes').max
        events = $taskEvents
    }
}
$taskReport = [ordered]@{
    note = 'Sampled peaks are not exact startup/decode peaks. No 8-hour certification. -1 counters are excluded.'
    metadata = $taskMetadata
    runs = $taskRuns
}
$taskJson = $taskReport | ConvertTo-Json -Depth 12
if ($Output) { $taskJson | Set-Content -LiteralPath $Output -Encoding utf8 } else { $taskJson }
