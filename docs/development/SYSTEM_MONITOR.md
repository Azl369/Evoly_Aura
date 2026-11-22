# 系统监测卡片

新增第八张卡片，默认放在主屏工作区右上角。原 panel0–panel6 的布局、主题、置顶和便签继续读取，
新增 panel7 保存监测卡片的位置与大小；编辑布局时可拖动、缩放，重启后保留。

显示系统 CPU 利用率、GPU 最忙引擎占用率、已用物理内存、可用物理内存。
GB 按 1024³ 字节换算。已用 = 总物理内存 − 可用物理内存，不是 Aura 自身的内存。

## 采样与资源

- 每 2 秒采样一次，使用 Windows 线程池定时器，PDH 查询不在 UI 线程运行。
- CPU 使用 GetSystemTimes 的两次累计时间差；首次采样显示 `--`，下一周期更新。
- 内存使用 GlobalMemoryStatusEx，保留可用/总量原始字节数。
- GPU 使用英文 PDH 路径 `\GPU Engine(*)\Utilization Percentage`，兼容本地化系统的计数器名称。
  按 LUID、物理 GPU、引擎编号汇总进程实例，取最忙引擎，不把不同引擎的百分比相加。
  多 GPU 取所有适配器中最忙引擎；第一版尚无指定显卡选择。
- GPU 计数器不可用、驱动不支持或预热未完成时显示 `--`，不显示伪造的 0%。
- 隐藏卡片时取消定时器、等待回调结束、关闭 PDH 查询并释放最多 1 MiB 的查询数组缓冲。
  恢复后重新预热；只重绘监测卡片，不刷新所有卡片，不引入 WMI、外部脚本或额外常驻进程。
- CPU 第一版按 GetSystemTimes 的处理器组口径；超过 64 个逻辑处理器的完整跨组统计尚待验证。

这些是实现约束与功能验证，不代表已通过整套应用内存/CPU 性能预算。

## 验证

Build-Native.ps1 包含三组 CTest：原有图像、布局/持久化，以及新增实际采样/隐藏停止/恢复测试。
布局测试覆盖旧七卡片配置迁移，保证原坐标与置顶状态不被重置。

Capture-SystemMonitor.ps1 可在现有 Aura 保持运行时单独预览新卡片，默认 12 秒退出；
只用于临时截图，不属于生产应用的常驻进程。

~~~powershell
.\tools\scripts\Capture-SystemMonitor.ps1
~~~

官方口径参考：[GetSystemTimes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getsystemtimes)、
[GlobalMemoryStatusEx](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-globalmemorystatusex)、
[PDH 数组](https://learn.microsoft.com/en-us/windows/win32/api/pdh/nf-pdh-pdhgetformattedcounterarrayw)、
[GPU 最忙引擎说明](https://devblogs.microsoft.com/directx/gpus-in-the-task-manager/)。
