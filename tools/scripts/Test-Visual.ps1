[CmdletBinding()]
param(
    [string]$BuildRoot = 'D:\dev\build\Evoly_Aura',
    [string]$OutputRoot = 'D:\dev\tmp\Evoly_Aura\visual'
)
$ErrorActionPreference = 'Stop'
if (Get-Process AuraProbe -ErrorAction SilentlyContinue) { throw 'Close the running probe before controlled visual tests.' }
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
$taskReference = New-Object System.Windows.Forms.Form
$taskReference.FormBorderStyle = [System.Windows.Forms.FormBorderStyle]::None
$taskReference.StartPosition = [System.Windows.Forms.FormStartPosition]::Manual
$taskReference.TopMost = $true
$taskReference.Location = New-Object System.Drawing.Point(24,24)
$taskReference.ClientSize = New-Object System.Drawing.Size(900,600)
$taskGrid = New-Object System.Drawing.Bitmap(900,600)
$taskCanvas = [System.Drawing.Graphics]::FromImage($taskGrid)
try {
    for ($taskY = 0; $taskY -lt 600; $taskY += 40) {
        for ($taskX = 0; $taskX -lt 900; $taskX += 40) {
            $taskBrush = if ((($taskX + $taskY) / 40) % 2) { [System.Drawing.Brushes]::Navy } else { [System.Drawing.Brushes]::Gold }
            $taskCanvas.FillRectangle($taskBrush,$taskX,$taskY,40,40)
        }
    }
} finally { $taskCanvas.Dispose() }
$taskReference.BackgroundImage = $taskGrid
$taskReference.Show()
$taskReference.Activate()
[System.Windows.Forms.Application]::DoEvents()
try {
    foreach ($taskMaterial in @('wallpaper','acrylic')) {
        $taskBackend = if ($taskMaterial -eq 'wallpaper') { 'software' } else { 'composition' }
        $taskLog = Join-Path $OutputRoot "$taskMaterial.jsonl"
        $taskArguments = @('--backend',$taskBackend,'--material',$taskMaterial,'--panels',1,
            '--duration',8,'--log',('"' + $taskLog + '"'))
        $taskProcess = Start-Process (Join-Path $BuildRoot 'Release\AuraProbe.exe') -ArgumentList $taskArguments -WindowStyle Hidden -PassThru
        try {
            for ($taskWait = 0; $taskWait -lt 15; $taskWait++) {
                [System.Windows.Forms.Application]::DoEvents()
                Start-Sleep -Milliseconds 100
            }
            & (Join-Path $PSScriptRoot 'Capture-Probe.ps1') -Output (Join-Path $OutputRoot "$taskMaterial-grid.png")
            while (-not $taskProcess.HasExited) {
                [System.Windows.Forms.Application]::DoEvents()
                Start-Sleep -Milliseconds 100
            }
            if ($taskProcess.ExitCode -ne 0) { throw 'Visual probe failed.' }
        } finally {
            if (-not $taskProcess.HasExited) { Stop-Process -Id $taskProcess.Id }
        }
    }
} finally {
    $taskReference.Close()
    $taskReference.Dispose()
    $taskGrid.Dispose()
}
Write-Output "Controlled local screenshots: $OutputRoot"
