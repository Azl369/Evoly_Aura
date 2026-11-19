[CmdletBinding()]
param(
    [string]$BuildRoot = 'D:\dev\build\Evoly_Aura',
    [string]$OutputRoot = 'D:\dev\tmp\Evoly_Aura\workspace-preview'
)
$ErrorActionPreference = 'Stop'
if (Get-Process Aura -ErrorAction SilentlyContinue) { throw 'Close Aura before running the controlled preview.' }
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
if (-not ('AuraWorkspaceCapture' -as [type])) {
    Add-Type @'
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class AuraWorkspaceCapture {
    public delegate bool Callback(IntPtr hwnd,IntPtr data);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
    [DllImport("user32.dll")] static extern bool EnumWindows(Callback cb,IntPtr data);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr hwnd,StringBuilder cls,int count);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern IntPtr FindWindow(string cls,string title);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr hwnd,out Rect rect);
    [DllImport("user32.dll")] static extern IntPtr SendMessageTimeout(IntPtr hwnd,uint msg,UIntPtr wp,IntPtr lp,uint flags,uint timeout,out UIntPtr result);
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr context);
    public static Rect[] Panels() {
        var list=new List<Rect>();
        EnumWindows((hwnd,data)=>{var cls=new StringBuilder(128);GetClassName(hwnd,cls,128);
            if(cls.ToString()=="AuraWorkspacePanel"){Rect r; if(GetWindowRect(hwnd,out r))list.Add(r);}return true;},IntPtr.Zero);
        return list.ToArray();
    }
    public static bool Command(uint command) {
        UIntPtr result;
        var hwnd=FindWindow("AuraWorkspaceController",null);
        return hwnd!=IntPtr.Zero && SendMessageTimeout(hwnd,0x8002,new UIntPtr(command),IntPtr.Zero,3,3000,out result)!=IntPtr.Zero;
    }
}
'@
}
$null = [AuraWorkspaceCapture]::SetProcessDpiAwarenessContext([IntPtr](-4))
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
$taskWork = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
$taskReference = New-Object System.Windows.Forms.Form
$taskReference.FormBorderStyle = [System.Windows.Forms.FormBorderStyle]::None
$taskReference.StartPosition = [System.Windows.Forms.FormStartPosition]::Manual
$taskReference.Bounds = $taskWork
$taskReference.TopMost = $true
$taskBackground = New-Object System.Drawing.Bitmap($taskWork.Width,$taskWork.Height)
$taskGraphics = [System.Drawing.Graphics]::FromImage($taskBackground)
$taskGradient = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    (New-Object System.Drawing.Rectangle(0,0,$taskWork.Width,$taskWork.Height)),
    [System.Drawing.Color]::FromArgb(19,31,45),
    [System.Drawing.Color]::FromArgb(57,80,91),35)
$taskGraphics.FillRectangle($taskGradient,0,0,$taskWork.Width,$taskWork.Height)
$taskGradient.Dispose(); $taskGraphics.Dispose()
$taskReference.BackgroundImage = $taskBackground
$taskReference.Show()
[System.Windows.Forms.Application]::DoEvents()
$taskArguments = @('--preview','--duration',60,'--data-dir',('"' + (Join-Path $OutputRoot 'state') + '"'),
    '--log',('"' + (Join-Path $OutputRoot 'workspace.log') + '"'))
$taskProcess = Start-Process (Join-Path $BuildRoot 'Release\Aura.exe') -ArgumentList $taskArguments -WindowStyle Hidden -PassThru
function Save-AuraWorkspaceImage([string]$Name) {
    $taskRects = @([AuraWorkspaceCapture]::Panels())
    if ($taskRects.Count -ne 7) { throw "Expected seven panels, found $($taskRects.Count)." }
    $taskLeft = ($taskRects | Measure-Object Left -Minimum).Minimum - 20
    $taskTop = ($taskRects | Measure-Object Top -Minimum).Minimum - 20
    $taskRight = ($taskRects | Measure-Object Right -Maximum).Maximum + 20
    $taskBottom = ($taskRects | Measure-Object Bottom -Maximum).Maximum + 20
    $taskWidth = [int]($taskRight-$taskLeft)
    $taskHeight = [int]($taskBottom-$taskTop)
    if ($taskWidth -lt 1 -or $taskHeight -lt 1 -or $taskWidth -gt 8192 -or $taskHeight -gt 8192) {
        throw "Invalid workspace capture dimensions: $taskWidth x $taskHeight"
    }
    $taskBitmap = New-Object System.Drawing.Bitmap($taskWidth,$taskHeight)
    $taskCanvas = [System.Drawing.Graphics]::FromImage($taskBitmap)
    try {
        $taskCanvas.CopyFromScreen([int]$taskLeft,[int]$taskTop,0,0,$taskBitmap.Size)
        $taskBitmap.Save((Join-Path $OutputRoot $Name),[System.Drawing.Imaging.ImageFormat]::Png)
    } finally { $taskCanvas.Dispose(); $taskBitmap.Dispose() }
}
try {
    for ($taskWait=0;$taskWait -lt 20;$taskWait++) {
        [System.Windows.Forms.Application]::DoEvents()
        Start-Sleep -Milliseconds 100
    }
    Save-AuraWorkspaceImage 'workspace.png'
    $null = [AuraWorkspaceCapture]::Command(104)
    Start-Sleep -Milliseconds 300
    Save-AuraWorkspaceImage 'workspace-edit.png'
    $null = [AuraWorkspaceCapture]::Command(104)
    $null = [AuraWorkspaceCapture]::Command(106)
    Start-Sleep -Milliseconds 300
    Save-AuraWorkspaceImage 'workspace-theme.png'
    $null = [AuraWorkspaceCapture]::Command(107)
    if (-not $taskProcess.WaitForExit(10000) -or $taskProcess.ExitCode -ne 0) { throw 'Workspace did not exit normally.' }
} finally {
    if (-not $taskProcess.HasExited) { Stop-Process -Id $taskProcess.Id }
    $taskReference.Close(); $taskReference.Dispose(); $taskBackground.Dispose()
}
Write-Output "Native workspace previews saved to $OutputRoot"
