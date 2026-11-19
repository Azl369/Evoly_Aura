# 桌面框架使用与验证

## 启动

先运行 tools/scripts/Build-Native.ps1。
双击根目录 Start-Aura.cmd，或运行：

~~~powershell
.\tools\scripts\Start-Aura.ps1
~~~

默认悬浮预览便于立即看见效果。加 -Desktop 则进入桌面层。
直接运行 D:\dev\build\Evoly_Aura\Release\Aura.exe 默认进入桌面层，可按 Win+D 查看。

Aura.exe 是桌面框架，AuraProbe.exe 是独立诊断探针；二者职责分开。
重复启动 Aura 会显示已有实例，不创建第二套桌面卡片。

## 可以实际操作的内容

- 顶部「编辑布局」进入编辑模式：所有卡片暂时浮起，标题区拖动，右下角调整大小。
- 完成编辑后锁定位置，回到原先桌面/悬浮状态；布局自动保存。
- 常用入口打开文件资源管理器、下载、文档、项目主页、系统设置和回收站。
- 便签点击「编辑」或双击正文；原生编辑框支持中文、换行，点击保存后更新卡片。
- 专注卡片开始、暂停和继续 25 分钟计时；退出应用后不恢复剩余时间。
- 初次使用清单支持手动勾选，也随布局、便签和专注操作更新。
- 外观卡片选择四套强调色，调节玻璃浓度。
- 托盘单击显示/隐藏，右键切换模式、编辑、重置或退出。
- 卡片右键菜单提供单卡置顶；该项尚需完整人工回归。

## 数据

默认 %LOCALAPPDATA%\EvolyAura：
workspace.ini 保存版本化布局、主题、玻璃浓度、清单与单卡置顶状态；
note.txt 保存 UTF-8 便签。均先写临时文件，再替换目标文件。
配置不在源码仓库内，不同步到 GitHub。

--data-dir 可指定独立测试目录；--preview 预览；--duration 指定自动退出秒数；
--log 仅写入生命周期事件，不记录便签内容。

布局用逻辑像素保留三位小数，恢复时四舍五入至物理像素，
避免 125% DPI 下反复重启造成坐标漂移。

## 验证

~~~powershell
.\tools\scripts\Build-Native.ps1
.\tools\scripts\Test-Workspace.ps1
.\tools\scripts\Capture-Workspace.ps1
.\tools\scripts\Test-Probe.ps1
.\tools\scripts\Validate-Workspace.ps1
~~~

单元测试覆盖默认布局无重叠、命中区域、吸附及带小数坐标的布局/中文便签持久化。
Test-Workspace 实际驱动七个窗口：拖动、缩放、编辑锁定、主题、专注、
中文便签、隐藏/恢复、桌面/HUD 往返及进程重启后的布局恢复。
测试状态位于 D:\dev\tmp\Evoly_Aura，默认不触碰正式用户配置。

Capture-Workspace 在临时渐变背景上截取实际原生窗口，
便于检查间距、文字、玻璃边缘和编辑态。背景只用于巡检，不修改 Windows 壁纸。
屏幕截图不是 HTML 效果图；卡片里的玻璃采样仍来自本机静态壁纸。

## 当前边界

这是第一版效果框架。固定七块卡片、固定六个快捷入口；
尚未实现任意文件分类、组件增删、完整键盘导航/UI Automation、小屏和多屏编排。
编辑窗口使用标准原生控件，未套用玻璃皮肤。
桌面挂载仍依赖 Shell 层级约定；Explorer 重启、设备恢复、完整兼容矩阵待验收。
此阶段只执行构建、功能与截图验证，最终性能协议按用户要求后置。
