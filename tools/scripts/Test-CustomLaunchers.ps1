[CmdletBinding()]
param(
 [string]$BuildRoot='D:\dev\build\Evoly_Aura_Launcher',
 [string]$BaselineExe='D:\dev\tmp\Evoly_Aura\launcher-regression\baseline\AuraBaseline.exe',
 [string]$OutputRoot='D:\dev\tmp\Evoly_Aura\launcher-regression',
 [switch]$SkipBaseline
)
$ErrorActionPreference='Stop'
if(Get-Process Aura,AuraBaseline -ErrorAction SilentlyContinue){throw 'Close Aura before isolated shortcut checks.'}
Add-Type -AssemblyName System.Windows.Forms
Add-Type @'
using System;using System.Text;using System.Collections.Generic;using System.Runtime.InteropServices;
public static class AuraLauncherUi {
 public delegate bool CB(IntPtr h,IntPtr d);
 [StructLayout(LayoutKind.Sequential)]public struct Rect{public int L,T,R,B;}
 [StructLayout(LayoutKind.Sequential)]public struct Point{public int X,Y;}
 [StructLayout(LayoutKind.Sequential)]public struct CopyData{public UIntPtr Tag;public uint Bytes;public IntPtr Value;}
 [DllImport("user32.dll")]static extern bool EnumWindows(CB cb,IntPtr d);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetClassName(IntPtr h,StringBuilder s,int n);
 [DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);
 [DllImport("user32.dll")]static extern bool GetWindowRect(IntPtr h,out Rect r);
 [DllImport("user32.dll")]static extern uint GetDpiForWindow(IntPtr h);
 [DllImport("user32.dll")]static extern IntPtr WindowFromPoint(Point p);
 [DllImport("user32.dll")]static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")]public static extern bool GetCursorPos(out Point p);
 [DllImport("user32.dll")]static extern void mouse_event(uint f,uint x,uint y,uint d,UIntPtr extra);
 [DllImport("user32.dll")]static extern IntPtr SendMessageTimeout(IntPtr h,uint m,UIntPtr w,IntPtr l,uint flags,uint timeout,out UIntPtr r);
 [DllImport("user32.dll",CharSet=CharSet.Unicode,EntryPoint="SendMessageW")]static extern IntPtr SendText(IntPtr h,uint m,IntPtr w,string text);
 [DllImport("user32.dll")]static extern IntPtr GetDlgItem(IntPtr h,int id);
 [DllImport("user32.dll")]static extern int GetMenuItemCount(IntPtr h);
 [DllImport("user32.dll")]static extern uint GetMenuItemID(IntPtr h,int i);
 [DllImport("user32.dll")]static extern bool GetMenuItemRect(IntPtr window,IntPtr menu,uint item,out Rect r);
 [DllImport("user32.dll")]public static extern bool AllowSetForegroundWindow(uint pid);
 [DllImport("user32.dll")]public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern IntPtr CreateWindowEx(uint ex,string cls,string title,uint style,int x,int y,int w,int h,IntPtr parent,IntPtr menu,IntPtr instance,IntPtr data);
 [DllImport("user32.dll")]public static extern bool DestroyWindow(IntPtr h);
 public static IntPtr CreateReference(){SetThreadDpiAwarenessContext(new IntPtr(-4));return CreateWindowEx(0x88,"STATIC","",0x90000100,4,4,20,20,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero);}
 public static IntPtr Owned(string cls,uint pid){IntPtr result=IntPtr.Zero;EnumWindows((h,d)=>{uint p;GetWindowThreadProcessId(h,out p);var name=new StringBuilder(128);GetClassName(h,name,128);if(p==pid&&name.ToString()==cls){result=h;return false;}return true;},IntPtr.Zero);return result;}
 public static IntPtr Launcher(uint pid){SetThreadDpiAwarenessContext(new IntPtr(-4));IntPtr result=IntPtr.Zero;EnumWindows((h,d)=>{uint p;GetWindowThreadProcessId(h,out p);var name=new StringBuilder(128);GetClassName(h,name,128);Rect r;GetWindowRect(h,out r);double scale=GetDpiForWindow(h)/96.0;if(p==pid&&name.ToString()=="AuraWorkspacePanel"&&Math.Abs((r.R-r.L)/scale-480)<2&&Math.Abs((r.B-r.T)/scale-240)<2){result=h;return false;}return true;},IntPtr.Zero);return result;}
 public static bool Message(IntPtr h,uint m,uint w,int l){UIntPtr r;return h!=IntPtr.Zero&&SendMessageTimeout(h,m,new UIntPtr(w),new IntPtr(l),3,5000,out r)!=IntPtr.Zero;}
 public static bool Command(uint pid,uint c){AllowSetForegroundWindow(pid);return Message(Owned("AuraWorkspaceController",pid),0x8002,c,0);}
 public static bool Tap(IntPtr h,double x,double y,bool right){SetThreadDpiAwarenessContext(new IntPtr(-4));Rect r;if(h==IntPtr.Zero||!GetWindowRect(h,out r))return false;double scale=GetDpiForWindow(h)/96.0;int px=r.L+(int)(x*scale),py=r.T+(int)(y*scale);if(WindowFromPoint(new Point{X=px,Y=py})!=h){var name=new StringBuilder(128);GetClassName(WindowFromPoint(new Point{X=px,Y=py}),name,128);Console.WriteLine("Guarded click: "+px+","+py+" expected="+h+" hit="+name);return false;}SetCursorPos(px,py);mouse_event(right?8u:2u,0,0,0,UIntPtr.Zero);mouse_event(right?16u:4u,0,0,0,UIntPtr.Zero);return true;}
 public static bool SelectMenu(uint pid,uint command){var h=Owned("#32768",pid);UIntPtr value;if(h==IntPtr.Zero||SendMessageTimeout(h,0x01e1,UIntPtr.Zero,IntPtr.Zero,3,2000,out value)==IntPtr.Zero)return false;var menu=new IntPtr(unchecked((long)value.ToUInt64()));for(int i=0;i<GetMenuItemCount(menu);i++)if(GetMenuItemID(menu,i)==command){Rect r;if(!GetMenuItemRect(IntPtr.Zero,menu,(uint)i,out r))return false;int x=(r.L+r.R)/2,y=(r.T+r.B)/2;uint p;GetWindowThreadProcessId(WindowFromPoint(new Point{X=x,Y=y}),out p);if(p!=pid)return false;SetCursorPos(x,y);mouse_event(2,0,0,0,UIntPtr.Zero);mouse_event(4,0,0,0,UIntPtr.Zero);return true;}return false;}
 public static bool Import(uint pid,string[] paths){byte[] bytes=Encoding.Unicode.GetBytes(string.Join("\0",paths)+"\0\0");var buffer=Marshal.AllocHGlobal(bytes.Length);var data=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(CopyData)));try{Marshal.Copy(bytes,0,buffer,bytes.Length);Marshal.StructureToPtr(new CopyData{Tag=new UIntPtr(0x41555241),Bytes=(uint)bytes.Length,Value=buffer},data,false);UIntPtr result;return SendMessageTimeout(Owned("AuraWorkspaceController",pid),0x4a,UIntPtr.Zero,data,3,5000,out result)!=IntPtr.Zero&&result.ToUInt64()==1;}finally{Marshal.FreeHGlobal(data);Marshal.FreeHGlobal(buffer);}}
 public static bool Rename(uint pid,string name){var h=Owned("AuraShortcutRename",pid);if(h==IntPtr.Zero)return false;SendText(GetDlgItem(h,3),12,IntPtr.Zero,name);return Message(h,0x111,1,0);}
 public static void Restore(Point p){SetCursorPos(p.X,p.Y);}
 public static bool FocusReference(IntPtr h){SetThreadDpiAwarenessContext(new IntPtr(-4));Rect r;if(!GetWindowRect(h,out r))return false;int x=(r.L+r.R)/2,y=(r.T+r.B)/2;if(WindowFromPoint(new Point{X=x,Y=y})!=h)return false;SetCursorPos(x,y);mouse_event(2,0,0,0,UIntPtr.Zero);mouse_event(4,0,0,0,UIntPtr.Zero);return true;}
}
'@
$null=[AuraLauncherUi]::SetThreadDpiAwarenessContext([IntPtr](-4))
$taskCursor=New-Object AuraLauncherUi+Point
$null=[AuraLauncherUi]::GetCursorPos([ref]$taskCursor)
$taskRun=Join-Path $OutputRoot ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))
New-Item -ItemType Directory -Path $taskRun -Force | Out-Null
$taskCurrent=$null
$taskSummary=[ordered]@{}
$taskReference=[AuraLauncherUi]::CreateReference()
[System.Windows.Forms.Application]::DoEvents()
function Start-LauncherInstance([string]$Exe,[string]$Name){
 $taskData=Join-Path $taskRun ($Name+'-state')
 $taskLog=Join-Path $taskRun ($Name+'.log')
 $script:taskCurrent=Start-Process $Exe -ArgumentList @('--preview','--duration',900,'--data-dir',('"'+$taskData+'"'),'--log',('"'+$taskLog+'"')) -WindowStyle Hidden -PassThru
 $null=$taskCurrent.Handle
 for($taskTry=0;$taskTry -lt 100;$taskTry++){
  if($taskCurrent.HasExited){throw 'Aura exited during startup'}
  if((Test-Path $taskLog) -and (Select-String -LiteralPath $taskLog -Pattern '^workspace_ready$' -Quiet)){break}
  Start-Sleep -Milliseconds 100
 }
 if([AuraLauncherUi]::Launcher([uint32]$taskCurrent.Id) -eq [IntPtr]::Zero){throw 'Launcher panel missing'}
 return $taskData
}
function Stop-LauncherInstance {
 if($taskCurrent -and -not $taskCurrent.HasExited){$null=[AuraLauncherUi]::Command([uint32]$taskCurrent.Id,107);if(-not $taskCurrent.WaitForExit(10000)){Stop-Process -Id $taskCurrent.Id;throw 'Aura did not exit cleanly'}}
}
function Measure-LauncherStage([string]$Stage){
 $taskCsv=Join-Path $taskRun ($Stage+'.csv')
 & (Join-Path $BuildRoot 'Release\AuraMetrics.exe') --pid $taskCurrent.Id --seconds 8 --interval-ms 1000 --output $taskCsv
 if($LASTEXITCODE -ne 0){throw "Metrics failed: $Stage"}
 $taskRows=@(Import-Csv -LiteralPath $taskCsv)
 $taskValue=[ordered]@{}
 foreach($taskColumn in @('private_ws_bytes','private_bytes','cpu_percent','handles','gdi','user','gpu_dedicated_bytes','gpu_shared_bytes','dwm_private_bytes','dwm_gpu_shared_bytes')){
  $taskValue[$taskColumn]=[Math]::Round(($taskRows | ForEach-Object {[double]$_.$taskColumn} | Measure-Object -Average).Average,3)
 }
 $script:taskSummary[$Stage]=$taskValue
 Write-Output ("Measured {0}: private WS {1:N2} MiB, private commit {2:N2} MiB, GDI {3}, USER {4}" -f $Stage,($taskValue.private_ws_bytes/1MB),($taskValue.private_bytes/1MB),$taskValue.gdi,$taskValue.user)
}
function Front-Launcher {
 if(-not [AuraLauncherUi]::FocusReference($taskReference)){throw 'Test focus target is covered; no external click sent'}
 [System.Windows.Forms.Application]::DoEvents()
 if(-not [AuraLauncherUi]::Command([uint32]$taskCurrent.Id,100)){throw 'Reveal failed'}
 Start-Sleep -Milliseconds 100
 return [AuraLauncherUi]::Launcher([uint32]$taskCurrent.Id)
}
function Tap-Launcher([double]$X,[double]$Y,[bool]$Right=$false){
 $taskWindow=Front-Launcher
 if(-not [AuraLauncherUi]::Tap($taskWindow,$X,$Y,$Right)){throw 'Launcher click target is covered; no external click sent'}
 Start-Sleep -Milliseconds 120
}
function Select-LauncherMenu([int]$Slot,[uint32]$Command){
 $taskX=24+($Slot%3)*(146.6666667)+68
 $taskY=62+[Math]::Floor($Slot/3)*68+30
 Tap-Launcher $taskX $taskY $true
 if(-not [AuraLauncherUi]::SelectMenu([uint32]$taskCurrent.Id,$Command)){throw "Shortcut menu command $Command missing"}
 Start-Sleep -Milliseconds 150
}
function Entry-Lines { return @(Get-Content -LiteralPath (Join-Path $taskData 'launchers.txt') -Encoding utf8 | Select-Object -Skip 1) }
function Import-Launchers([string[]]$Paths){if(-not [AuraLauncherUi]::Import([uint32]$taskCurrent.Id,$Paths)){throw 'Shortcut import failed'};Start-Sleep -Milliseconds 200}
try{
 if(-not $SkipBaseline -and (Test-Path -LiteralPath $BaselineExe)){
  $null=Start-LauncherInstance $BaselineExe 'baseline'
  Start-Sleep -Seconds 8
  Measure-LauncherStage 'baseline-default'
  Stop-LauncherInstance
 }
 $taskData=Start-LauncherInstance (Join-Path $BuildRoot 'Release\Aura.exe') 'updated'
 Start-Sleep -Seconds 8
 Measure-LauncherStage 'updated-default'
 $taskFixtures=Join-Path $taskRun 'fixtures';New-Item -ItemType Directory -Path $taskFixtures -Force | Out-Null
 $taskMarker=Join-Path $taskRun 'launched.marker'
 $taskShell=New-Object -ComObject WScript.Shell
 $taskLinks=@()
 for($taskI=0;$taskI -lt 102;$taskI++){
  $taskPath=Join-Path $taskFixtures ('Application-'+$taskI.ToString('000')+'.lnk')
  $taskLink=$taskShell.CreateShortcut($taskPath)
  $taskLink.TargetPath=Join-Path $BuildRoot 'Release\AuraLaunchTarget.exe'
  $taskLink.Arguments='"'+$taskMarker+'"'
  $taskLink.IconLocation=(Join-Path $env:WINDIR 'System32\shell32.dll')+','+($taskI%16)
  $taskLink.Save();$taskLinks+=$taskPath
  $null=[Runtime.InteropServices.Marshal]::FinalReleaseComObject($taskLink)
 }
 $null=[Runtime.InteropServices.Marshal]::FinalReleaseComObject($taskShell)
 Import-Launchers @($taskLinks[0],$taskLinks[1])
 if((Entry-Lines).Count -ne 8){throw 'Adding two entries failed'}
 Start-Sleep -Seconds 2
 Measure-LauncherStage 'two-custom-icons'
 Tap-Launcher 92 92
 for($taskTry=0;$taskTry -lt 50 -and -not (Test-Path $taskMarker);$taskTry++){Start-Sleep -Milliseconds 50}
 if(-not (Test-Path $taskMarker)){throw 'Clicking a shortcut did not launch the controlled target'}
 Select-LauncherMenu 0 302
 $taskRenamed='Renamed '+[char]0x5e94+[char]0x7528
 if(-not [AuraLauncherUi]::Rename([uint32]$taskCurrent.Id,$taskRenamed)){throw 'Rename dialog failed'}
 if((Get-Content -Raw -Encoding utf8 (Join-Path $taskData 'launchers.txt')) -notmatch [regex]::Escape($taskRenamed)){throw 'Chinese rename was not persisted'}
 Select-LauncherMenu 0 303
 if((Entry-Lines)[5] -notmatch [regex]::Escape($taskRenamed)){throw 'Move before across page boundary failed'}
 Select-LauncherMenu 5 304
 if((Entry-Lines)[6] -notmatch [regex]::Escape($taskRenamed)){throw 'Move after across page boundary failed'}
 Select-LauncherMenu 0 305
 if((Entry-Lines).Count -ne 7 -or -not (Test-Path $taskLinks[0])){throw 'Removing entry failed or removed the source shortcut'}
 Measure-LauncherStage 'after-launch-and-menu'
 Import-Launchers ([string[]]$taskLinks[2..101])
 if((Entry-Lines).Count -ne 107){throw 'Adding 100 shortcuts failed'}
 for($taskI=0;$taskI -lt 18;$taskI++){Tap-Launcher 38 216}
 for($taskI=0;$taskI -lt 18;$taskI++){Tap-Launcher 74 216;Start-Sleep -Milliseconds 220}
 Start-Sleep -Seconds 2
 Measure-LauncherStage 'loaded-100-icons'
 for($taskBatch=1;$taskBatch -le 2;$taskBatch++){
  for($taskI=0;$taskI -lt 20;$taskI++){
   Import-Launchers @($taskLinks[0])
   Select-LauncherMenu 5 305
   $null=[AuraLauncherUi]::Command([uint32]$taskCurrent.Id,106)
  }
  if((Entry-Lines).Count -ne 107){throw 'Repeated edit changed entry count'}
  Start-Sleep -Seconds 2
  Measure-LauncherStage ('after-edit-batch-'+$taskBatch)
 }
 if($taskSummary['after-edit-batch-2'].private_bytes-$taskSummary['after-edit-batch-1'].private_bytes -gt 8MB){throw 'Private commit keeps growing after repeated shortcut edits'}
 if($taskSummary['after-edit-batch-2'].gdi-$taskSummary['after-edit-batch-1'].gdi -gt 8){throw 'GDI resources keep growing'}
 Tap-Launcher 418 216
 $taskPicker=[AuraLauncherUi]::Owned('#32770',[uint32]$taskCurrent.Id)
 if($taskPicker -eq [IntPtr]::Zero){throw 'Native Add file picker did not open'}
 Measure-LauncherStage 'picker-open'
 $null=[AuraLauncherUi]::Message($taskPicker,0x10,0,0)
 Start-Sleep -Seconds 2
 if((Entry-Lines).Count -ne 107){throw 'Canceling picker changed shortcuts'}
 Measure-LauncherStage 'picker-closed'
 $null=[AuraLauncherUi]::Command([uint32]$taskCurrent.Id,101)
 Start-Sleep -Seconds 2
 Measure-LauncherStage 'hidden'
 Stop-LauncherInstance
 $taskArguments=@('--preview','--duration',90,'--data-dir',('"'+$taskData+'"'),'--log',('"'+(Join-Path $taskRun 'restart.log')+'"'))
 $script:taskCurrent=Start-Process (Join-Path $BuildRoot 'Release\Aura.exe') -ArgumentList $taskArguments -WindowStyle Hidden -PassThru
 $null=$taskCurrent.Handle;Start-Sleep -Seconds 3
 if((Entry-Lines).Count -ne 107 -or [AuraLauncherUi]::Launcher([uint32]$taskCurrent.Id) -eq [IntPtr]::Zero){throw 'Shortcut restart persistence failed'}
 Stop-LauncherInstance
 $taskSummary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $taskRun 'summary.json') -Encoding utf8
 Write-Output 'PASS: add/import, real controlled launch, Chinese rename, cross-page reorder, removal without deleting files, 100 entries, repeated edits/themes, native picker, hide/resume and restart persistence.'
 Write-Output "Private evidence and measurement samples: $taskRun"
}finally{
 if($taskCurrent -and -not $taskCurrent.HasExited){$null=[AuraLauncherUi]::Command([uint32]$taskCurrent.Id,107);if(-not $taskCurrent.WaitForExit(5000)){Stop-Process -Id $taskCurrent.Id}}
 [AuraLauncherUi]::Restore($taskCursor)
 $null=[AuraLauncherUi]::DestroyWindow($taskReference)
}
