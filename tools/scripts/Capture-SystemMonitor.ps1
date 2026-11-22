[CmdletBinding()]
param([string]$BuildRoot='D:\dev\build\Evoly_Aura',[string]$OutputRoot='D:\dev\tmp\Evoly_Aura\system-preview')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;using System.Runtime.InteropServices;
public static class AuraMonitorCapture {
 [StructLayout(LayoutKind.Sequential)] public struct Rect{public int Left,Top,Right,Bottom;}
 [StructLayout(LayoutKind.Sequential)] public struct Point{public int X,Y;}
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern IntPtr FindWindow(string c,string t);
 [DllImport("user32.dll")]static extern bool GetWindowRect(IntPtr h,out Rect r);
 [DllImport("user32.dll")]static extern IntPtr WindowFromPoint(Point p);
 [DllImport("kernel32.dll",SetLastError=true)]public static extern bool GetExitCodeProcess(IntPtr h,out uint code);
 [DllImport("user32.dll")]public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
 public static bool Bounds(out Rect r){var h=FindWindow("AuraSystemPreview",null);return GetWindowRect(h,out r)&&WindowFromPoint(new Point{X=(r.Left+r.Right)/2,Y=(r.Top+r.Bottom)/2})==h;}
}
'@
$null=[AuraMonitorCapture]::SetThreadDpiAwarenessContext([IntPtr](-4))
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
$taskPreview=Start-Process (Join-Path $BuildRoot 'Release\AuraSystemTests.exe') -ArgumentList '--preview' -WindowStyle Hidden -PassThru
$taskPreviewHandle=$taskPreview.Handle
try{
 Start-Sleep -Seconds 6
 $taskRect=New-Object AuraMonitorCapture+Rect
 if(-not [AuraMonitorCapture]::Bounds([ref]$taskRect)){throw 'System card preview missing'}
 $taskBitmap=New-Object System.Drawing.Bitmap(($taskRect.Right-$taskRect.Left),($taskRect.Bottom-$taskRect.Top))
 $taskCanvas=[System.Drawing.Graphics]::FromImage($taskBitmap)
 try{
  $taskCanvas.CopyFromScreen($taskRect.Left,$taskRect.Top,0,0,$taskBitmap.Size)
  $taskBitmap.Save((Join-Path $OutputRoot 'system-monitor.png'),[System.Drawing.Imaging.ImageFormat]::Png)
 }finally{$taskCanvas.Dispose();$taskBitmap.Dispose()}
 if(-not $taskPreview.WaitForExit(15000)){throw 'System preview exit timed out'}
 [uint32]$taskExitCode=0
 if(-not [AuraMonitorCapture]::GetExitCodeProcess($taskPreviewHandle,[ref]$taskExitCode) -or $taskExitCode -ne 0){throw "System preview exit code: $taskExitCode"}
}finally{if(-not $taskPreview.HasExited){Stop-Process -Id $taskPreview.Id}}
Write-Output "Native system card preview: $OutputRoot\system-monitor.png"
