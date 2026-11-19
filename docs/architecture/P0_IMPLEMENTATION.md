# P0 实现与资源生命周期

## 原生窗口

一个不可见的顶层 controller 接收托盘、快捷键、系统事件和统一定时器。
1–12 个小面板由 controller 维护；没有每面板进程和每面板永久线程。
HUD 顶部拖动，右键或托盘可切换材质和模式。关闭面板默认隐藏，托盘可恢复。

桌面挂载依次识别经典 WorkerW 与 Windows 11 Progman 子 WorkerW。
只在 Progman / WorkerW 中寻找桌面 DefView，避免把文件夹窗口的 DefView 当成桌面。
挂载失败回退 HUD。此实现仍依赖未公开 Shell 约定，不是稳定的 Windows 合同。
controller 接收 TaskbarCreated 后尝试恢复托盘及窗口；尚未实际验收 Explorer 重启。

## 绘制资源

软件：共享 Direct2D 软件 DC target、DirectWrite factory 与文本格式；
每面板一个 DIB / HDC，按实际物理像素分配，绘制后 UpdateLayeredWindow 提交。

GPU：共享 D3D11 BGRA 设备、Direct2D device、DirectComposition device；
每面板 target / visual / surface，BeginDraw 获得绑定该表面的 D2D context。
DCompositionCreateDevice2 请求 IDCompositionDesktopDevice，再查询 Device2 接口。
硬件创建失败才尝试 WARP，事件日志明确标记，不能把 WARP 结果称作硬件结果。

所有表面在尺寸变化或隐藏时释放；设备、字体与 CPU 壁纸缓存保留至进程退出。
显示恢复重新创建必要表面。系统图标在一个 renderer 内共享。
材质切换清理旧位图和表面，避免历史主题无限累积。

## 静态壁纸

IDesktopWallpaper 获取第一面板所属显示器的壁纸、位置和背景色。
WIC 将图像缩小至最长边不超过 1024；映射到最长边 640 的屏幕缓存。
低分辨率双向 box blur 后上传或拷贝至渲染资源，拖动时只改变采样坐标。
因此像素级结果是近似磨砂，不是系统实时背景采样。

当前原始图像拒绝超过 1 亿像素的输入；某些 WIC codec 仍可能内部解码原图，
所以“小输出缓存”不保证“小解码峰值”。后续必须用高分辨率壁纸和 ETW 检查峰值。
tile / span、无静态壁纸或读取失败，明确使用实色回退。

## 调度与测量

消息循环阻塞等待。分钟时钟通过 controller 统一调度；无恒定 FPS 循环。
隐藏停止时钟定时器；恢复后重新排期。
绘制计数、材质请求结果、DPI、屏幕规格与回退事件写入可选的本地 JSONL。
无壁纸路径、个人文件列表或用户文本。

独立 AuraMetrics 通过 PSAPI / PDH 读取进程与 GPU 计数，
QueryWorkingSet 的缓存分配发生在采样器，不计入 AuraProbe 的私有内存。
当前每秒采样会漏掉亚秒峰值。DWM 内存权限不足时返回 -1，报告为缺测。

## 已知原型边界

没有配置持久化、编辑器、文件管理和可扩展组件框架。
没有完整设备丢失重建；绘制错误记录事件后退出，避免失联窗口。
多屏与 DPI 消息处理仅是基础路径，不代表断屏、热插拔和所有缩放组合已验收。
系统 Acrylic 的亮度与可读性仍需独立视觉设计。
