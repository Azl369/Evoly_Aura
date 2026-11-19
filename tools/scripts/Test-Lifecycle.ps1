[CmdletBinding()]
param(
    [string]$BuildRoot = 'D:\dev\build\Evoly_Aura',
    [string]$OutputRoot = 'D:\dev\tmp\Evoly_Aura\lifecycle',
    [ValidateSet('software','composition')][string]$Backend = 'software'
)
$ErrorActionPreference = 'Stop'
if (Get-Process AuraProbe -ErrorAction SilentlyContinue) { throw 'Close the running probe before lifecycle tests.' }
if (-not ('AuraLifecycleNative' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class AuraLifecycleNative {
    public delegate bool WindowCallback(IntPtr hwnd, IntPtr data);
    [DllImport("user32.dll")] static extern bool EnumWindows(WindowCallback callback, IntPtr data);
    [DllImport("user32.dll")] static extern bool UpdateWindow(IntPtr hwnd);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")] static extern IntPtr SendMessageTimeout(IntPtr hwnd, uint msg, UIntPtr wp, IntPtr lp, uint flags, uint timeout, out UIntPtr result);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll")] public static extern uint GetGuiResources(IntPtr process, uint flags);
    public static IntPtr Controller() { return FindWindow("EvolyAuraP0Controller", null); }
    public static bool Command(IntPtr hwnd, uint command) {
        UIntPtr result; return SendMessageTimeout(hwnd, 0x8002, new UIntPtr(command), IntPtr.Zero, 3, 3000, out result) != IntPtr.Zero;
    }
    public static void PaintPanels(uint pid) {
        EnumWindows((hwnd, data) => {
            uint owner; GetWindowThreadProcessId(hwnd, out owner);
            if (owner == pid) UpdateWindow(hwnd);
            return true;
        }, IntPtr.Zero);
    }
}
'@
}
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
$taskLog = Join-Path $OutputRoot "$Backend-events.jsonl"
$taskArguments = @('--backend',$Backend,'--material','wallpaper','--duration','120','--log',('"' + $taskLog + '"'))
$taskProcess = Start-Process (Join-Path $BuildRoot 'Release\AuraProbe.exe') -ArgumentList $taskArguments -WindowStyle Hidden -PassThru
try {
    $taskController = [IntPtr]::Zero
    for ($taskTry = 0; $taskTry -lt 100; $taskTry++) {
        $taskController = [AuraLifecycleNative]::Controller()
        if ($taskController -ne [IntPtr]::Zero) { break }
        if ($taskProcess.HasExited) { throw 'Probe exited during initialization.' }
        Start-Sleep -Milliseconds 100
    }
    [uint32]$taskOwner = 0
    $null = [AuraLifecycleNative]::GetWindowThreadProcessId($taskController, [ref]$taskOwner)
    if ($taskOwner -ne $taskProcess.Id) { throw 'Controller does not belong to the launched probe.' }
    Start-Sleep -Milliseconds 1000
    function Read-AuraState {
        $taskProcess.Refresh()
        [ordered]@{
            privateBytes = $taskProcess.PrivateMemorySize64
            handles = $taskProcess.HandleCount
            gdi = [AuraLifecycleNative]::GetGuiResources($taskProcess.Handle,0)
            user = [AuraLifecycleNative]::GetGuiResources($taskProcess.Handle,1)
        }
    }
    $taskStates = @((Read-AuraState))
    for ($taskBatch = 1; $taskBatch -le 2; $taskBatch++) {
        for ($taskCycle = 1; $taskCycle -le 100; $taskCycle++) {
            foreach ($taskCommand in @(102,103,101,100)) {
                if (-not [AuraLifecycleNative]::Command($taskController, $taskCommand)) { throw 'Mode or visibility command failed.' }
            }
            [AuraLifecycleNative]::PaintPanels([uint32]$taskProcess.Id)
        }
        Start-Sleep -Milliseconds 1000
        $taskStates += Read-AuraState
    }
    $taskEvents = @(Get-Content $taskLog | ForEach-Object { $_ | ConvertFrom-Json })
    if ($taskEvents | Where-Object { $_.event -match 'error' }) { throw 'Probe reported lifecycle errors.' }
    $taskDelta = [ordered]@{}
    foreach ($taskKey in @('privateBytes','handles','gdi','user')) {
        $taskDelta[$taskKey] = $taskStates[2][$taskKey] - $taskStates[1][$taskKey]
    }
    if ($taskDelta.handles -gt 10 -or $taskDelta.gdi -gt 2 -or $taskDelta.user -gt 2) {
        throw 'Resource counts did not stabilize between stress batches.'
    }
    [ordered]@{
        backend = $Backend
        completedCycles = 200
        definition = 'Each cycle requests desktop, HUD, hide, show. API/handle checks do not prove visual layering.'
        states = $taskStates
        secondBatchDelta = $taskDelta
        desktopHostEvents = @($taskEvents | Where-Object event -eq 'desktop_host_found').Count
        desktopFallbackEvents = @($taskEvents | Where-Object event -eq 'desktop_fallback_hud').Count
    } | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutputRoot "$Backend-summary.json") -Encoding utf8
    $null = [AuraLifecycleNative]::Command($taskController,107)
    if (-not $taskProcess.WaitForExit(10000) -or $taskProcess.ExitCode -ne 0) { throw 'Probe did not exit cleanly.' }
    Write-Output "PASS $Backend lifecycle: 200 desktop/HUD/hide/show cycles."
} finally {
    if (-not $taskProcess.HasExited) { Stop-Process -Id $taskProcess.Id }
}
