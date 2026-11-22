[CmdletBinding()]
param([string]$BuildRoot='D:\dev\build\Evoly_Aura',[string]$OutputRoot='D:\dev\tmp\Evoly_Aura\real-tray')
$ErrorActionPreference='Stop'
if(Get-Process Aura -ErrorAction SilentlyContinue){throw 'Close Aura before the isolated real notification-area regression.'}
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Windows.Forms
if (-not ('AuraRealTray' -as [type])) {
Add-Type @'
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class AuraRealTray {
 public delegate bool CB(IntPtr h,IntPtr d);
 [StructLayout(LayoutKind.Sequential)]public struct Point{public int X,Y;}
 [StructLayout(LayoutKind.Sequential)]public struct Rect{public int Left,Top,Right,Bottom;}
 [DllImport("user32.dll")]static extern bool GetWindowRect(IntPtr h,out Rect r);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern IntPtr FindWindow(string cls,string title);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetClassName(IntPtr h,StringBuilder s,int count);
 [DllImport("user32.dll")]static extern bool EnumWindows(CB c,IntPtr d);
 [DllImport("user32.dll")]static extern bool EnumChildWindows(IntPtr h,CB c,IntPtr d);
 [DllImport("user32.dll")]public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")]static extern IntPtr GetParent(IntPtr h);
 [DllImport("user32.dll",EntryPoint="GetWindowLongPtrW")]static extern IntPtr GetLong(IntPtr h,int index);
 [DllImport("user32.dll")]static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")]static extern IntPtr WindowFromPoint(Point p);
 [DllImport("user32.dll")]static extern IntPtr GetAncestor(IntPtr h,uint f);
 [DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);
 [DllImport("user32.dll")]public static extern bool GetCursorPos(out Point p);
 [DllImport("user32.dll")]static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")]static extern void mouse_event(uint f,uint x,uint y,uint d,UIntPtr extra);
 [DllImport("user32.dll")]public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
 [DllImport("user32.dll")]static extern IntPtr SendMessageTimeout(IntPtr h,uint m,UIntPtr w,IntPtr l,uint f,uint t,out UIntPtr r);
 public static IntPtr Window(string cls){return FindWindow(cls,null);}
 public static bool EditorOpen(){return Window("AuraNoteEditor")!=IntPtr.Zero;}
 public static void Click(int x,int y,bool right){SetCursorPos(x,y);mouse_event(right?8u:2u,0,0,0,UIntPtr.Zero);mouse_event(right?16u:4u,0,0,0,UIntPtr.Zero);}
 public static void Restore(Point p){SetCursorPos(p.X,p.Y);}
 public static void Move(int x,int y){SetCursorPos(x,y);}
 public static uint OwnerAt(int x,int y){uint p;GetWindowThreadProcessId(GetAncestor(WindowFromPoint(new Point{X=x,Y=y}),2),out p);return p;}
 public static void ShowDesktop(){keybd_event(0x5b,0,0,UIntPtr.Zero);keybd_event(0x44,0,0,UIntPtr.Zero);keybd_event(0x44,0,2,UIntPtr.Zero);keybd_event(0x5b,0,2,UIntPtr.Zero);}
 [DllImport("user32.dll")]static extern void keybd_event(byte key,byte scan,uint flags,UIntPtr extra);
 [DllImport("user32.dll")]static extern int GetMenuItemCount(IntPtr menu);
 [DllImport("user32.dll")]static extern uint GetMenuItemID(IntPtr menu,int index);
 [DllImport("user32.dll")]static extern bool GetMenuItemRect(IntPtr window,IntPtr menu,uint item,out Rect rect);
 public static bool ClickMenuCommand(IntPtr popup,uint process,uint command){
  uint owner;GetWindowThreadProcessId(popup,out owner);if(owner!=process)return false;
  UIntPtr result;if(SendMessageTimeout(popup,0x01e1,UIntPtr.Zero,IntPtr.Zero,3,2000,out result)==IntPtr.Zero)return false;
  var menu=new IntPtr(unchecked((long)result.ToUInt64()));
  for(int i=0;i<GetMenuItemCount(menu);i++)if(GetMenuItemID(menu,i)==command){
   Rect r;if(!GetMenuItemRect(IntPtr.Zero,menu,(uint)i,out r))return false;
   int x=(r.Left+r.Right)/2,y=(r.Top+r.Bottom)/2;if(OwnerAt(x,y)!=process)return false;
   Click(x,y,false);return true;
  }return false;
 }
 public static bool Command(uint c){UIntPtr r;var h=Window("AuraWorkspaceController");return h!=IntPtr.Zero&&SendMessageTimeout(h,0x8002,new UIntPtr(c),IntPtr.Zero,3,5000,out r)!=IntPtr.Zero;}
 public static string State(){
  int count=0,visible=0,root=0,top=0,hittable=0;var seen=new HashSet<IntPtr>();
  CB cb=(h,d)=>{var s=new StringBuilder(128);GetClassName(h,s,128);
   if(s.ToString()=="AuraWorkspacePanel"&&seen.Add(h)){count++;if(IsWindowVisible(h))visible++;if(GetParent(h)==IntPtr.Zero)root++;if((GetLong(h,-20).ToInt64()&8)!=0)top++;
    Rect r;GetWindowRect(h,out r);if(WindowFromPoint(new Point{X=r.Left+(r.Right-r.Left)/2,Y=r.Top+(r.Bottom-r.Top)/2})==h)hittable++;}return true;};
  EnumWindows((h,d)=>{cb(h,d);EnumChildWindows(h,cb,d);return true;},IntPtr.Zero);
  var fg=new StringBuilder(128);GetClassName(GetForegroundWindow(),fg,128);
  return "panels="+count+" visible="+visible+" floating="+root+" topmost="+top+" hittable="+hittable+" foreground="+fg;
 }
}
'@
}
$null=[AuraRealTray]::SetThreadDpiAwarenessContext([IntPtr](-4))
$taskCursor=New-Object AuraRealTray+Point
$null=[AuraRealTray]::GetCursorPos([ref]$taskCursor)
$taskRun=Join-Path $OutputRoot ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))
New-Item -ItemType Directory -Path $taskRun -Force | Out-Null
$taskReference=$null
try{
 $taskReference=New-Object System.Windows.Forms.Form
 $taskReference.FormBorderStyle=[System.Windows.Forms.FormBorderStyle]::None
 $taskReference.StartPosition=[System.Windows.Forms.FormStartPosition]::Manual
 $taskReference.Bounds=[System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
 $taskReference.Show();$taskReference.Activate();[System.Windows.Forms.Application]::DoEvents()
 $taskArgs=@('--duration',180,'--data-dir',('"'+(Join-Path $taskRun 'state')+'"'),'--log',('"'+(Join-Path $taskRun 'events.log')+'"'))
 $taskProcess=Start-Process (Join-Path $BuildRoot 'Release\Aura.exe') -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
}catch{if($taskReference){$taskReference.Dispose()};throw}
$taskLog=Join-Path $taskRun 'real-input.log'
function Write-AuraTrayState([string]$Stage){
 $taskLine="$Stage : $([AuraRealTray]::State())"
 $taskLine | Add-Content -LiteralPath $taskLog -Encoding utf8
 Write-Output $taskLine
}
function Find-AuraTrayIcon {
 $taskHosts=@([AuraRealTray]::Window('Shell_TrayWnd'),[AuraRealTray]::Window('NotifyIconOverflowWindow'),[AuraRealTray]::Window('TopLevelWindowForOverflowXamlIsland'))
 foreach($taskHost in $taskHosts){
  if($taskHost -eq [IntPtr]::Zero -or -not [AuraRealTray]::IsWindowVisible($taskHost)){continue}
  $taskRoot=[System.Windows.Automation.AutomationElement]::FromHandle($taskHost)
  $taskItems=$taskRoot.FindAll([System.Windows.Automation.TreeScope]::Descendants,[System.Windows.Automation.Condition]::TrueCondition)
  foreach($taskItem in $taskItems){
   if($taskItem.Current.Name.StartsWith('Evoly Aura') -and $taskItem.Current.ControlType -eq [System.Windows.Automation.ControlType]::Button -and -not $taskItem.Current.IsOffscreen -and $taskItem.Current.BoundingRectangle.Width -gt 0){return $taskItem}
  }
 }
 return $null
}
function Click-AuraElement($Element,[bool]$Right=$false){
 $taskBounds=$Element.Current.BoundingRectangle
 $taskPoint=New-Object System.Windows.Point(($taskBounds.Left+$taskBounds.Width/2),($taskBounds.Top+$taskBounds.Height/2))
 [AuraRealTray]::Move([int]$taskPoint.X,[int]$taskPoint.Y)
 Start-Sleep -Milliseconds 250
 $taskHit=[System.Windows.Automation.AutomationElement]::FromPoint($taskPoint)
 $taskHitClass=$taskHit.Current.ClassName
 $taskMatched=$false
 for($taskAncestor=0;$taskAncestor -lt 12 -and $null -ne $taskHit;$taskAncestor++){
  if($taskHit.Current.Name -eq $Element.Current.Name){$taskMatched=$true;break}
  $taskHit=[System.Windows.Automation.TreeWalker]::RawViewWalker.GetParent($taskHit)
 }
 $taskOwner=[AuraRealTray]::OwnerAt([int]$taskPoint.X,[int]$taskPoint.Y)
 $taskOwnerName=(Get-Process -Id $taskOwner).ProcessName
 if(-not $taskMatched -and ($taskOwnerName -ne 'explorer' -or $taskHitClass -notmatch '^(Shell_TrayWnd|TopLevelWindowForOverflowXamlIsland|NotifyIconOverflowWindow|Xaml_WindowedPopupClass)$')){
  throw "Tray target is not hittable at $taskPoint; hit class=$taskHitClass. No click sent."
 }
 [AuraRealTray]::Click([int]$taskPoint.X,[int]$taskPoint.Y,$Right)
 Start-Sleep -Milliseconds 180
}
function Select-AuraTrayMenu([uint32]$Command){
 $taskIcon=Open-AuraTrayIcon;Click-AuraElement $taskIcon $true
 Start-Sleep -Milliseconds 350
 $taskMenu=@([System.Windows.Automation.AutomationElement]::RootElement.FindAll([System.Windows.Automation.TreeScope]::Children,[System.Windows.Automation.Condition]::TrueCondition) | Where-Object {$_.Current.ClassName -eq '#32768' -and $_.Current.ProcessId -eq $taskProcess.Id})
 if($taskMenu.Count -ne 1){throw 'Aura menu missing'}
 if(-not [AuraRealTray]::ClickMenuCommand([IntPtr]$taskMenu[0].Current.NativeWindowHandle,[uint32]$taskProcess.Id,$Command)){throw "Menu command $Command is not safely clickable"}
 Start-Sleep -Milliseconds 250
}
function Open-AuraTrayIcon {
 $taskIcon=Find-AuraTrayIcon
 if($null -ne $taskIcon){return $taskIcon}
 $taskRoot=[System.Windows.Automation.AutomationElement]::FromHandle([AuraRealTray]::Window('Shell_TrayWnd'))
 $taskButtons=$taskRoot.FindAll([System.Windows.Automation.TreeScope]::Descendants,
  [System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::ControlTypeProperty,[System.Windows.Automation.ControlType]::Button))
 foreach($taskButton in $taskButtons){
  if($taskButton.Current.Name.Contains([string][char]0x9690) -or $taskButton.Current.Name -match 'hidden'){
   Click-AuraElement $taskButton
   $taskIcon=Find-AuraTrayIcon
   if($null -ne $taskIcon){return $taskIcon}
  }
 }
 throw 'Aura icon not found in actual Windows notification area.'
}
try {
 for($taskTry=0;$taskTry -lt 100;$taskTry++){
  if($taskProcess.HasExited){throw 'Aura exited during startup'}
  $taskReady=Join-Path $taskRun 'events.log'
  if((Test-Path $taskReady) -and (Select-String -LiteralPath $taskReady -Pattern '^workspace_ready$' -Quiet)){break}
  Start-Sleep -Milliseconds 100
 }
 if(-not (Test-Path $taskReady) -or -not (Select-String -LiteralPath $taskReady -Pattern '^workspace_ready$' -Quiet)){throw 'Aura startup timed out'}
 Write-AuraTrayState 'initial'
 foreach($HideMethod in @('menu','reference','tray','tray-reference','desktop')){
 for($taskCycle=1;$taskCycle -le 3;$taskCycle++){
  $taskIcon=Open-AuraTrayIcon
  Click-AuraElement $taskIcon
  Write-AuraTrayState "click-$taskCycle-180ms"
  Start-Sleep -Milliseconds 600
  Write-AuraTrayState "click-$taskCycle-780ms"
  if([AuraRealTray]::State() -notmatch '^panels=7 visible=7 floating=7 topmost=0 hittable=7 '){throw "Not all seven cards are on screen after $HideMethod cycle $taskCycle"}
  if($HideMethod -eq 'tray'){$taskIcon=Open-AuraTrayIcon;Click-AuraElement $taskIcon}
  elseif($HideMethod -eq 'menu'){
   Select-AuraTrayMenu 101
  }
  elseif($HideMethod -eq 'desktop'){[AuraRealTray]::ShowDesktop();Start-Sleep -Milliseconds 500}
  else{
   if($HideMethod -eq 'tray-reference'){$null=Open-AuraTrayIcon}
   $taskX=$taskReference.Left+8;$taskY=$taskReference.Top+8
   if([AuraRealTray]::OwnerAt($taskX,$taskY) -ne $PID){throw 'Reference click target is covered; no click sent.'}
   [AuraRealTray]::Click($taskX,$taskY,$false)
   [System.Windows.Forms.Application]::DoEvents();Start-Sleep -Milliseconds 500
  }
  Write-AuraTrayState "$HideMethod-hide-$taskCycle"
  if($HideMethod -eq 'menu' -and [AuraRealTray]::State() -notmatch ' visible=0 '){throw 'Actual Hide menu did not hide cards'}
  if($HideMethod -match 'reference' -and [AuraRealTray]::State() -notmatch ' floating=0 '){throw 'Activating another application did not return cards to desktop'}
  if($HideMethod -eq 'tray' -and [AuraRealTray]::State() -notmatch '^panels=7 visible=7 floating=7 topmost=0 hittable=7 '){throw 'Repeated tray selection did not keep all cards revealed'}
 }
 }
 $null=[AuraRealTray]::Command(107)
 if(-not $taskProcess.WaitForExit(10000) -or $taskProcess.ExitCode -ne 0){throw 'Aura did not exit cleanly'}
 Write-Output 'PASS: actual Hide menu, other app activation, repeated tray selection, Win+D and overflow-to-app activation; all seven cards hittable after every reveal.'
} finally {
 if($taskProcess -and -not $taskProcess.HasExited){$null=[AuraRealTray]::Command(107);if(-not $taskProcess.WaitForExit(5000)){Stop-Process -Id $taskProcess.Id}}
 if($taskReference){$taskReference.Close();$taskReference.Dispose()}
 [AuraRealTray]::Restore($taskCursor)
}
Write-Output "Local real-input evidence: $taskRun"
