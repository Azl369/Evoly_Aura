[CmdletBinding()]
param(
    [string]$BuildRoot = 'D:\dev\build\Evoly_Aura',
    [string]$OutputRoot = 'D:\dev\tmp\Evoly_Aura\window-layers'
)
$ErrorActionPreference = 'Stop'
if (Get-Process Aura -ErrorAction SilentlyContinue) { throw 'Close Aura before isolated layer regression.' }
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
if (-not ('AuraLayerRegression' -as [type])) {
    Add-Type @'
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class AuraLayerRegression {
    public delegate bool Callback(IntPtr hwnd,IntPtr data);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X,Y; }
    public struct Panel { public IntPtr Hwnd,Parent; public int Left,Top,Right,Bottom; public bool Visible,Topmost; }
    [DllImport("user32.dll")] static extern bool EnumWindows(Callback cb,IntPtr data);
    [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr hwnd,Callback cb,IntPtr data);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr hwnd,StringBuilder name,int size);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern IntPtr FindWindow(string cls,string title);
    [DllImport("user32.dll",EntryPoint="GetWindowLongPtrW")] static extern IntPtr GetWindowLong(IntPtr hwnd,int index);
    [DllImport("user32.dll")] static extern IntPtr GetParent(IntPtr hwnd);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr hwnd,out Rect rect);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
    [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(Point point);
    [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr hwnd,uint flags);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool AllowSetForegroundWindow(uint pid);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out Point point);
    [DllImport("user32.dll")] static extern void mouse_event(uint flags,uint dx,uint dy,uint data,UIntPtr extra);
    [DllImport("user32.dll")] static extern IntPtr SendMessageTimeout(IntPtr hwnd,uint msg,UIntPtr wp,IntPtr lp,uint flags,uint timeout,out UIntPtr result);
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    public static bool Command(uint command) {
        UIntPtr result;var hwnd=FindWindow("AuraWorkspaceController",null);
        return hwnd!=IntPtr.Zero && SendMessageTimeout(hwnd,0x8002,new UIntPtr(command),IntPtr.Zero,3,5000,out result)!=IntPtr.Zero;
    }
    public static bool TrayClick() {
        UIntPtr result;var hwnd=FindWindow("AuraWorkspaceController",null);
        // Synthetic VERSION_4 selection; Test-RealTray separately clicks the Shell.
        return hwnd!=IntPtr.Zero && SendMessageTimeout(hwnd,0x8001,UIntPtr.Zero,new IntPtr((1<<16)|0x0400),3,5000,out result)!=IntPtr.Zero;
    }
    public static Panel[] Panels() {
        var panels=new List<Panel>();var seen=new HashSet<IntPtr>();
        Callback inspect=(hwnd,data)=>{
            var name=new StringBuilder(128);GetClassName(hwnd,name,128);
            if(name.ToString()=="AuraWorkspacePanel" && seen.Add(hwnd)) {
                Rect r;GetWindowRect(hwnd,out r);
                panels.Add(new Panel { Hwnd=hwnd,Parent=GetParent(hwnd),Left=r.Left,Top=r.Top,Right=r.Right,Bottom=r.Bottom,
                    Visible=IsWindowVisible(hwnd),Topmost=(GetWindowLong(hwnd,-20).ToInt64()&8)!=0 });
            }return true;
        };
        EnumWindows((hwnd,data)=>{inspect(hwnd,data);EnumChildWindows(hwnd,inspect,data);return true;},IntPtr.Zero);
        return panels.ToArray();
    }
    public static uint OwnerAt(int x,int y) {
        var hwnd=GetAncestor(WindowFromPoint(new Point{X=x,Y=y}),2);
        uint pid;GetWindowThreadProcessId(hwnd,out pid);return pid;
    }
    public static void Click(int x,int y) {
        SetCursorPos(x,y);mouse_event(2,0,0,0,UIntPtr.Zero);mouse_event(4,0,0,0,UIntPtr.Zero);
    }
    public static int EditorTopmost() {
        var hwnd=FindWindow("AuraNoteEditor",null);
        return hwnd==IntPtr.Zero?-1:(GetWindowLong(hwnd,-20).ToInt64()&8)!=0?1:0;
    }
    public static void CloseEditor() {
        UIntPtr result;var hwnd=FindWindow("AuraNoteEditor",null);
        if(hwnd!=IntPtr.Zero)SendMessageTimeout(hwnd,0x0010,UIntPtr.Zero,IntPtr.Zero,3,3000,out result);
    }
    public static bool Escape() {
        UIntPtr result;var hwnd=FindWindow("AuraWorkspacePanel",null);
        return hwnd!=IntPtr.Zero && SendMessageTimeout(hwnd,0x0100,new UIntPtr(27),IntPtr.Zero,3,3000,out result)!=IntPtr.Zero;
    }
}
'@
}
$null = [AuraLayerRegression]::SetThreadDpiAwarenessContext([IntPtr](-4))
$taskCursor = New-Object AuraLayerRegression+Point
$null = [AuraLayerRegression]::GetCursorPos([ref]$taskCursor)
$taskStamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')
$taskRun = Join-Path $OutputRoot $taskStamp
New-Item -ItemType Directory -Path $taskRun -Force | Out-Null
$taskReference = New-Object System.Windows.Forms.Form
$taskReference.FormBorderStyle = [System.Windows.Forms.FormBorderStyle]::None
$taskReference.StartPosition = [System.Windows.Forms.FormStartPosition]::Manual
$taskReference.Bounds = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
$taskReference.BackColor = [System.Drawing.Color]::FromArgb(160,30,20)
$taskReference.TopMost = $false
$taskReference.Show(); $taskReference.Activate()
[System.Windows.Forms.Application]::DoEvents()
$taskArgs = @('--duration',120,'--data-dir',('"' + (Join-Path $taskRun 'state') + '"'),
    '--log',('"' + (Join-Path $taskRun 'events.log') + '"'))
$taskProcess = Start-Process (Join-Path $BuildRoot 'Release\Aura.exe') -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
function Invoke-AuraLayerCommand([uint32]$Value) {
    $null = [AuraLayerRegression]::AllowSetForegroundWindow([uint32]$taskProcess.Id)
    if (-not [AuraLayerRegression]::Command($Value)) { throw "Command failed: $Value" }
    [System.Windows.Forms.Application]::DoEvents()
    Start-Sleep -Milliseconds 120
}
function Activate-AuraReference {
    $taskPointX=$taskReference.Left+8
    $taskPointY=$taskReference.Top+8
    if ([AuraLayerRegression]::OwnerAt($taskPointX,$taskPointY) -ne $PID) { throw 'Reference click target is covered; no external click was sent.' }
    [AuraLayerRegression]::Click($taskPointX,$taskPointY)
    [System.Windows.Forms.Application]::DoEvents()
    Start-Sleep -Milliseconds 200
}
function Assert-AuraFloating {
    $taskCurrent = @([AuraLayerRegression]::Panels())
    if ($taskCurrent.Count -ne 8) { throw 'Panel count changed.' }
    if (@($taskCurrent | Where-Object { -not $_.Visible -or $_.Parent -ne [IntPtr]::Zero -or $_.Topmost }).Count) {
        throw 'Normal floating panel is hidden, parented, or permanently topmost.'
    }
}
function Read-AuraPixel([int]$X,[int]$Y) {
    $taskBitmap = New-Object System.Drawing.Bitmap(1,1)
    $taskGraphics = [System.Drawing.Graphics]::FromImage($taskBitmap)
    try {
        $taskGraphics.CopyFromScreen($X,$Y,0,0,$taskBitmap.Size)
        $taskColor = $taskBitmap.GetPixel(0,0)
        return @([int]$taskColor.R,[int]$taskColor.G,[int]$taskColor.B)
    } finally { $taskGraphics.Dispose(); $taskBitmap.Dispose() }
}
try {
    for ($taskTry=0;$taskTry -lt 100;$taskTry++) {
        $taskPanels = @([AuraLayerRegression]::Panels() | Sort-Object Top,Left)
        $taskReadyPath = Join-Path $taskRun 'events.log'
        $taskReady = (Test-Path $taskReadyPath) -and (Select-String -LiteralPath $taskReadyPath -Pattern '^workspace_ready$' -Quiet)
        if ($taskPanels.Count -eq 8 -and $taskReady) { break }
        if ($taskProcess.HasExited) { throw 'Aura exited during startup.' }
        Start-Sleep -Milliseconds 100
    }
    if ($taskPanels.Count -ne 8) { throw 'Default desktop panels missing.' }
    if (@($taskPanels | Where-Object { $_.Parent -eq [IntPtr]::Zero -or $_.Topmost -or -not $_.Visible }).Count) {
        $taskPanels | Format-Table Left,Top,Visible,Topmost,Parent
        throw 'Default desktop mode is not attached, visible and non-topmost.'
    }
    $taskClock = $taskPanels[2]
    $taskX = $taskClock.Right - 35
    $taskY = [int](($taskClock.Top + $taskClock.Bottom)/2)
    $taskReference.BringToFront(); $taskReference.Activate()
    $null=[AuraLayerRegression]::SetForegroundWindow($taskReference.Handle)
    [System.Windows.Forms.Application]::DoEvents()
    Start-Sleep -Milliseconds 150
    if ([AuraLayerRegression]::OwnerAt($taskX,$taskY) -ne $PID) { throw 'Controlled alpha background is not in front of the desktop.' }
    $null = [AuraLayerRegression]::AllowSetForegroundWindow([uint32]$taskProcess.Id)
    Invoke-AuraLayerCommand 103
    Assert-AuraFloating
    if ([AuraLayerRegression]::OwnerAt($taskX,$taskY) -ne $taskProcess.Id) { throw 'Floating card was not actually raised.' }
    $taskRed = @(Read-AuraPixel $taskX $taskY)
    $taskReference.BackColor = [System.Drawing.Color]::FromArgb(20,30,160)
    $taskReference.Refresh()
    [System.Windows.Forms.Application]::DoEvents()
    Start-Sleep -Milliseconds 200
    if ([AuraLayerRegression]::OwnerAt($taskX,$taskY) -ne $taskProcess.Id) { throw 'Reference obscured card during alpha test.' }
    $taskBlue = @(Read-AuraPixel $taskX $taskY)
    $taskDifference = [Math]::Abs($taskRed[0]-$taskBlue[0])+[Math]::Abs($taskRed[2]-$taskBlue[2])
    if ($taskDifference -lt 20) { throw 'Floating surface still paints cached wallpaper instead of live alpha.' }
    Activate-AuraReference
    if ([AuraLayerRegression]::OwnerAt($taskX,$taskY) -eq $taskProcess.Id) { throw 'Aura still blocks an activated normal application.' }
    Invoke-AuraLayerCommand 110
    if ([AuraLayerRegression]::EditorTopmost() -ne 0) { throw 'Note editor was missing or permanently topmost.' }
    [AuraLayerRegression]::CloseEditor()
    for ($taskCycle=0;$taskCycle -lt 8;$taskCycle++) {
        Invoke-AuraLayerCommand 102
        Invoke-AuraLayerCommand 101
        Invoke-AuraLayerCommand 103
        Assert-AuraFloating
        if ([AuraLayerRegression]::OwnerAt($taskX,$taskY) -ne $taskProcess.Id) { throw 'A restored floating card is not actually visible.' }
    }
    Invoke-AuraLayerCommand 102
    Invoke-AuraLayerCommand 101
    Invoke-AuraLayerCommand 100
    Assert-AuraFloating
    if ([AuraLayerRegression]::OwnerAt($taskX,$taskY) -ne $taskProcess.Id) { throw 'Tray reveal is not visible above reference app.' }
    Activate-AuraReference
    Start-Sleep -Milliseconds 100
    $taskReturned = @([AuraLayerRegression]::Panels())
    if (@($taskReturned | Where-Object { $_.Parent -ne [IntPtr]::Zero -or $_.Topmost -or -not $_.Visible }).Count) {
        throw 'Another app activation unexpectedly hid or reparented revealed cards.'
    }
    if ([AuraLayerRegression]::OwnerAt($taskX,$taskY) -eq $taskProcess.Id) { throw 'Revealed cards block the activated application.' }
    Invoke-AuraLayerCommand 121
    $taskPinned = @([AuraLayerRegression]::Panels() | Where-Object Topmost)
    if ($taskPinned.Count -ne 1 -or $taskPinned[0].Hwnd -ne $taskClock.Hwnd) { throw 'Explicit pin did not limit topmost to one card.' }
    Invoke-AuraLayerCommand 121
    if (@([AuraLayerRegression]::Panels() | Where-Object Topmost).Count) { throw 'Unpin failed to clear topmost band.' }
    $null = [AuraLayerRegression]::AllowSetForegroundWindow([uint32]$taskProcess.Id)
    if (-not [AuraLayerRegression]::TrayClick()) { throw 'Tray callback did not reveal cards.' }
    Start-Sleep -Milliseconds 150
    Assert-AuraFloating
    if (-not [AuraLayerRegression]::TrayClick()) { throw 'Repeated tray callback failed.' }
    Start-Sleep -Milliseconds 150
    Assert-AuraFloating
    Invoke-AuraLayerCommand 101
    if (@([AuraLayerRegression]::Panels() | Where-Object Visible).Count) { throw 'Explicit hide did not hide cards.' }
    if (-not [AuraLayerRegression]::TrayClick()) { throw 'Tray callback did not reveal cards a second time.' }
    Start-Sleep -Milliseconds 150
    Assert-AuraFloating
    if (-not [AuraLayerRegression]::Escape()) { throw 'Escape was not delivered.' }
    Start-Sleep -Milliseconds 150
    if (@([AuraLayerRegression]::Panels() | Where-Object Visible).Count) {
        throw 'Escape did not hide revealed cards.'
    }
    Invoke-AuraLayerCommand 104
    if (@([AuraLayerRegression]::Panels() | Where-Object Topmost).Count -ne 8) { throw 'Layout editor was not temporarily raised.' }
    Invoke-AuraLayerCommand 104
    if (@([AuraLayerRegression]::Panels() | Where-Object { $_.Parent -eq [IntPtr]::Zero -or $_.Topmost }).Count) {
        throw 'Finishing layout edit left cards permanently topmost.'
    }
    Invoke-AuraLayerCommand 107
    if (-not $taskProcess.WaitForExit(10000) -or $taskProcess.ExitCode -ne 0) { throw 'Aura did not exit cleanly.' }
    [ordered]@{
        normalFloatingTopmost = 0
        hiddenDesktopFloatingCycles = 8
        trayRevealAndHide = 'passed'
        explicitPinAndUnpin = 'passed'
        escapeAndEditFinish = 'passed'
        liveAlphaRedPixel = $taskRed
        liveAlphaBluePixel = $taskBlue
        channelDifference = $taskDifference
    } | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $taskRun 'result.json') -Encoding utf8
    Write-Output 'PASS: default desktop, non-topmost floating, real alpha, foreground app coverage without reparenting, repeated hidden mode switches, tray reveal/hide and single-card pin.'
    Write-Output "Local regression evidence: $taskRun"
} finally {
    if (-not $taskProcess.HasExited) { $null=[AuraLayerRegression]::Command(107); if (-not $taskProcess.WaitForExit(5000)) { Stop-Process -Id $taskProcess.Id } }
    $taskReference.Close(); $taskReference.Dispose()
    $null = [AuraLayerRegression]::SetCursorPos($taskCursor.X,$taskCursor.Y)
}
