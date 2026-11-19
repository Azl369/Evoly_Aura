[CmdletBinding()]
param(
    [string]$BuildRoot = 'D:\dev\build\Evoly_Aura',
    [string]$OutputRoot = 'D:\dev\tmp\Evoly_Aura\workspace-test'
)
$ErrorActionPreference = 'Stop'
if (Get-Process Aura -ErrorAction SilentlyContinue) { throw 'Close Aura before interactive checks.' }
if (-not ('AuraWorkspaceTest' -as [type])) {
    Add-Type @'
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class AuraWorkspaceTest {
    public delegate bool Callback(IntPtr hwnd,IntPtr data);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X,Y; }
    public struct Panel { public IntPtr Hwnd; public int Left,Top,Right,Bottom; }
    [DllImport("user32.dll")] static extern bool EnumWindows(Callback cb,IntPtr data);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr hwnd,StringBuilder cls,int count);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern IntPtr FindWindow(string cls,string title);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd,out Rect rect);
    [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd,int id);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out Point point);
    [DllImport("user32.dll")] static extern IntPtr SendMessageTimeout(IntPtr hwnd,uint msg,UIntPtr wp,IntPtr lp,uint flags,uint timeout,out UIntPtr result);
    [DllImport("user32.dll",CharSet=CharSet.Unicode,EntryPoint="SendMessageW")] public static extern IntPtr SendText(IntPtr hwnd,uint msg,IntPtr wp,string text);
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    public static IntPtr Editor() { return FindWindow("AuraNoteEditor",null); }
    public static bool Message(IntPtr hwnd,uint msg,uint wp,int lp) {
        UIntPtr result; return SendMessageTimeout(hwnd,msg,new UIntPtr(wp),new IntPtr(lp),3,5000,out result)!=IntPtr.Zero;
    }
    public static bool Command(uint value) {
        return Message(FindWindow("AuraWorkspaceController",null),0x8002,value,0);
    }
    public static Panel[] Panels() {
        var list=new List<Panel>();
        EnumWindows((hwnd,data)=>{
            var cls=new StringBuilder(128);GetClassName(hwnd,cls,128);
            if(cls.ToString()=="AuraWorkspacePanel"){Rect r;GetWindowRect(hwnd,out r);
                list.Add(new Panel {Hwnd=hwnd,Left=r.Left,Top=r.Top,Right=r.Right,Bottom=r.Bottom});}
            return true;
        },IntPtr.Zero);
        return list.ToArray();
    }
    public static void Drag(IntPtr hwnd,int x,int y,int dx,int dy) {
        Rect r;GetWindowRect(hwnd,out r);
        SetCursorPos(r.Left+x,r.Top+y);
        Message(hwnd,0x0201,1,(y<<16)|(x&65535));
        SetCursorPos(r.Left+x+dx,r.Top+y+dy);
        Message(hwnd,0x0200,1,((y+dy)<<16)|((x+dx)&65535));
        Message(hwnd,0x0202,0,((y+dy)<<16)|((x+dx)&65535));
    }
}
'@
}
$null = [AuraWorkspaceTest]::SetProcessDpiAwarenessContext([IntPtr](-4))
$null = [AuraWorkspaceTest]::SetThreadDpiAwarenessContext([IntPtr](-4))
$taskCursor = New-Object AuraWorkspaceTest+Point
$null = [AuraWorkspaceTest]::GetCursorPos([ref]$taskCursor)
$taskStamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')
$taskRun = Join-Path $OutputRoot $taskStamp
$taskData = Join-Path $taskRun 'state'
New-Item -ItemType Directory -Path $taskRun -Force | Out-Null
$taskExe = Join-Path $BuildRoot 'Release\Aura.exe'
$taskArguments = @('--preview','--duration',60,'--data-dir',('"' + $taskData + '"'),
    '--log',('"' + (Join-Path $taskRun 'workspace.log') + '"'))
$taskProcess = Start-Process $taskExe -ArgumentList $taskArguments -WindowStyle Hidden -PassThru
try {
    for ($taskTry=0;$taskTry -lt 100;$taskTry++) {
        $taskPanels = @([AuraWorkspaceTest]::Panels() | Sort-Object Top,Left)
        if ($taskPanels.Count -eq 7) { break }
        if ($taskProcess.HasExited) { throw 'Aura exited before readiness.' }
        Start-Sleep -Milliseconds 100
    }
    if ($taskPanels.Count -ne 7) { throw 'Seven workspace panels were not created.' }
    Start-Sleep -Milliseconds 500
    $taskDuplicate = Start-Process $taskExe -ArgumentList '--preview' -WindowStyle Hidden -PassThru
    if (-not $taskDuplicate.WaitForExit(10000) -or $taskDuplicate.ExitCode -ne 0) { throw 'Duplicate launch did not activate the existing instance.' }
    if (@([AuraWorkspaceTest]::Panels()).Count -ne 7) { throw 'Duplicate launch created extra panels.' }
    $taskClock = $taskPanels[1]
    if (-not [AuraWorkspaceTest]::Command(104)) { throw 'Edit command failed.' }
    [AuraWorkspaceTest]::Drag($taskClock.Hwnd,40,30,60,20)
    $taskMoved = New-Object AuraWorkspaceTest+Rect
    $null = [AuraWorkspaceTest]::GetWindowRect($taskClock.Hwnd,[ref]$taskMoved)
    if ($taskMoved.Left -eq $taskClock.Left) { throw 'Drag did not move the actual panel.' }
    Write-Output "Drag: ($($taskClock.Left),$($taskClock.Top)) -> ($($taskMoved.Left),$($taskMoved.Top))"
    if ([Math]::Abs($taskMoved.Left-$taskClock.Left-60) -gt 13 -or [Math]::Abs($taskMoved.Top-$taskClock.Top-20) -gt 13) {
        throw 'Drag did not preserve the requested screen-space offset.'
    }
    [AuraWorkspaceTest]::Drag($taskClock.Hwnd,($taskMoved.Right-$taskMoved.Left-12),($taskMoved.Bottom-$taskMoved.Top-12),50,30)
    $taskResized = New-Object AuraWorkspaceTest+Rect
    $null = [AuraWorkspaceTest]::GetWindowRect($taskClock.Hwnd,[ref]$taskResized)
    Write-Output "Resize: $($taskResized.Right-$taskResized.Left) x $($taskResized.Bottom-$taskResized.Top)"
    if (($taskResized.Right-$taskResized.Left) -le ($taskMoved.Right-$taskMoved.Left)) { throw 'Resize did not change physical width.' }
    $null = [AuraWorkspaceTest]::Command(104)
    $null = [AuraWorkspaceTest]::Command(106)
    $null = [AuraWorkspaceTest]::Command(109)
    Start-Sleep -Milliseconds 1200
    $null = [AuraWorkspaceTest]::Command(109)
    $null = [AuraWorkspaceTest]::Command(110)
    $taskEditor = [AuraWorkspaceTest]::Editor()
    if ($taskEditor -eq [IntPtr]::Zero) { throw 'Native note editor did not open.' }
    $taskNote = "Aura workspace" + [Environment]::NewLine + [char]0x6d6e + [char]0x5149
    $null = [AuraWorkspaceTest]::SendText([AuraWorkspaceTest]::GetDlgItem($taskEditor,3),0x000c,[IntPtr]::Zero,$taskNote)
    $null = [AuraWorkspaceTest]::Message($taskEditor,0x0111,1,0)
    if ((Get-Content -Raw -Encoding utf8 (Join-Path $taskData 'note.txt')) -cne $taskNote) { throw 'UTF-8 note content mismatch.' }
    $null = [AuraWorkspaceTest]::Command(101)
    foreach ($taskPanel in $taskPanels) { if ([AuraWorkspaceTest]::IsWindowVisible($taskPanel.Hwnd)) { throw 'Hidden panel remains visible.' } }
    $null = [AuraWorkspaceTest]::Command(100)
    foreach ($taskPanel in $taskPanels) { if (-not [AuraWorkspaceTest]::IsWindowVisible($taskPanel.Hwnd)) { throw 'Panel did not restore.' } }
    $null = [AuraWorkspaceTest]::Command(102)
    $taskAttached = 0
    foreach ($taskPanel in $taskPanels) { if ([AuraWorkspaceTest]::GetParent($taskPanel.Hwnd) -ne [IntPtr]::Zero) { $taskAttached++ } }
    if ($taskAttached -ne 7) { throw "Desktop attachment incomplete: $taskAttached/7." }
    $null = [AuraWorkspaceTest]::Command(103)
    $taskAfterMode = New-Object AuraWorkspaceTest+Rect
    $null = [AuraWorkspaceTest]::GetWindowRect($taskClock.Hwnd,[ref]$taskAfterMode)
    Write-Output "After desktop/HUD: ($($taskAfterMode.Left),$($taskAfterMode.Top))"
    if ([Math]::Abs($taskAfterMode.Left-$taskResized.Left) -gt 1 -or [Math]::Abs($taskAfterMode.Top-$taskResized.Top) -gt 1) {
        throw 'Desktop/HUD switching moved the panel.'
    }
    $null = [AuraWorkspaceTest]::Command(107)
    if (-not $taskProcess.WaitForExit(10000) -or $taskProcess.ExitCode -ne 0) { throw 'Aura did not exit normally.' }
    $taskIni = Get-Content -Raw (Join-Path $taskData 'workspace.ini')
    if ($taskIni -notmatch 'theme=1' -or $taskIni -notmatch 'tasks=7') { throw 'Appearance or onboarding persistence failed.' }
    $taskProcess = Start-Process $taskExe -ArgumentList $taskArguments -WindowStyle Hidden -PassThru
    Start-Sleep -Milliseconds 1500
    $taskRestored = @([AuraWorkspaceTest]::Panels())
    $taskFound = @($taskRestored | Where-Object { $_.Left -eq $taskResized.Left -and $_.Top -eq $taskResized.Top -and ($_.Right-$_.Left) -eq ($taskResized.Right-$taskResized.Left) })
    if (-not $taskFound.Count) { throw 'Moved and resized placement did not survive restart.' }
    if ((Get-Content -Raw -Encoding utf8 (Join-Path $taskData 'note.txt')) -cne $taskNote) { throw 'Note did not survive restart.' }
    $null = [AuraWorkspaceTest]::Command(107)
    if (-not $taskProcess.WaitForExit(10000) -or $taskProcess.ExitCode -ne 0) { throw 'Restarted Aura did not exit.' }
    Write-Output 'PASS: seven panels, real drag/resize, edit lock, theme, focus, UTF-8 note, hide/restore, desktop/HUD and restart persistence.'
    Write-Output "Local test state and logs: $taskRun"
} finally {
    if (-not $taskProcess.HasExited) { Stop-Process -Id $taskProcess.Id }
    $null = [AuraWorkspaceTest]::SetCursorPos($taskCursor.X,$taskCursor.Y)
}
