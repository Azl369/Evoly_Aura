[CmdletBinding()]
param([Parameter(Mandatory)][string]$Output)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
if (-not ('AuraCaptureNative' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class AuraCaptureNative {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
    public static IntPtr FindPanel() { return FindWindow("EvolyAuraP0Panel", null); }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr context);
}
'@
}
$null = [AuraCaptureNative]::SetProcessDpiAwarenessContext([IntPtr](-4))
$taskWindow = [AuraCaptureNative]::FindPanel()
if ($taskWindow -eq [IntPtr]::Zero) { throw 'No Aura probe panel found.' }
$taskRect = New-Object AuraCaptureNative+Rect
if (-not [AuraCaptureNative]::GetWindowRect($taskWindow, [ref]$taskRect)) { throw 'Cannot read probe bounds.' }
$taskBitmap = New-Object System.Drawing.Bitmap(($taskRect.Right - $taskRect.Left), ($taskRect.Bottom - $taskRect.Top))
$taskGraphics = [System.Drawing.Graphics]::FromImage($taskBitmap)
try {
    $taskGraphics.CopyFromScreen($taskRect.Left, $taskRect.Top, 0, 0, $taskBitmap.Size)
    $taskBitmap.Save($Output, [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $taskGraphics.Dispose()
    $taskBitmap.Dispose()
}
