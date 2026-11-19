# P0 原生构建与运行

要求：Windows 11 x64、Visual Studio 2022 C++ Build Tools、Windows SDK、CMake ≥ 3.24。
本机已使用现有 MSVC 19.44、Windows SDK 10.0.26100.0，不安装新的第三方依赖。

## 构建

~~~powershell
.\tools\scripts\Build-Native.ps1
~~~

默认生成至 D:\dev\build\Evoly_Aura。脚本自动发现系统或 Visual Studio 自带的 CMake，
运行 Release 构建与 CTest。可用 BuildRoot 参数覆盖路径。
CMakePresets.json 是可移植的替代入口，默认使用已忽略的源码树 out 目录；
在本机遵循 D 盘输出约定时优先使用脚本。

产物：
- Release\AuraProbe.exe：唯一常驻原型。
- Release\AuraMetrics.exe：独立进程采样工具，不属于常驻端。
- Release\AuraCoreTests.exe：图像映射、模糊边界与尺寸限制测试。

## 运行

~~~powershell
D:\dev\build\Evoly_Aura\Release\AuraProbe.exe --backend software --material wallpaper --panels 6
~~~

backend：software / composition。material：solid / wallpaper / acrylic。
panels：1–12；duration：0 表示用户退出，否则为不超过 86400 秒的自动退出。
--desktop 请求实验桌面挂载；失败时回退 HUD。
--hidden 从隐藏状态开始；--exercise 自动执行 100 次材质及尺寸变化，再隐藏与恢复。
--log 指定本地 JSONL 事件日志，不记录壁纸路径或用户文本。

托盘单击显示/隐藏，右键切换材质、桌面与 HUD 或退出。
Ctrl+Alt+F10 显示/隐藏，F11 切换模式，F12 退出。
HUD 顶部可拖动。原型不包含文件管理、便签、布局持久化或完整编辑器。

软件后端使用共享 Direct2D 软件 DC render target 与每窗口 DIB，
通过 UpdateLayeredWindow 提交；不主动创建 D3D 设备。
GPU 后端共享 D3D11 / D2D / DirectComposition 设备，小表面按窗分配。
静止仅分钟时钟刷新，无动画帧循环。隐藏释放表面与 GPU 位图，但保留设备基础成本。

Acrylic 仅在 composition 下尝试；软件后端实色回退。
API 成功不等同于已经证明实时模糊；需视觉与背景变化对照。
壁纸缓存支持 fill / fit / stretch / center，tile / span 或读取失败实色回退。
当前缓存以第一面板的显示器为准，跨屏重新采样与完整恢复仍待实现。

## 检查

~~~powershell
.\tools\scripts\Test-Probe.ps1
.\tools\scripts\Validate-Workspace.ps1
~~~

Test-Probe 顺序执行六个 UI 冒烟场景；不要在已有实例运行时启动。
GitHub CI 只执行 Release 编译和 core 测试，不把托管 CI 当作桌面视觉或精确性能环境。

## 测量

~~~powershell
.\tools\scripts\Measure-Probe.ps1 -Backend software -Material wallpaper
~~~

默认每次基线 60 秒、预热 60 秒、稳定采样 300 秒，共 3 次，约 22 分钟。
原始数据默认位于 D:\dev\tmp\Evoly_Aura\measurements，不进 Git。
metadata.local.json 记录提交、工作区是否干净、二进制哈希、硬件与参数。
脚本在每次测量后等待原型自动退出；异常仅停止本次启动的进程。

~~~powershell
.\tools\scripts\Summarize-Probe.ps1 -RunDirectory '<本地测量目录>' -Output '<本地汇总.json>'
~~~

快速排查可减少 Seconds / WarmupSeconds / BaselineSeconds / Repetitions；
必须在报告中标明探索性短测，不能当成完整 B0。
当前每秒采样可能遗漏亚秒级启动和解码峰值；精确峰值需要后续 ETW 补测。
DWM 进程内存若因权限不可读显示 -1，不当成 0。
GPU counter 是各适配器实例之和，不能与 CPU 计数器直接相加。

Capture-Probe.ps1 可仅截取一个原型面板进行本机视觉检查。
截图可能包含用户壁纸，默认不公开提交。
