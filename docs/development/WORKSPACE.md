# 工作区目录与职责

状态：目录已预留，应用实现尚未开始。

## 目录分工

| 路径 | 职责 |
| --- | --- |
| apps/host | 进程入口、单实例、依赖组装；不堆业务逻辑 |
| apps/editor | 按需配置 UI；框架待评估，不默认加载 Flutter |
| src/core | 面板模型、布局、吸附算法、调度策略 |
| src/platform/windows | HWND、托盘、快捷键、桌面挂载、DPI、系统事件 |
| src/render | 绘制资源、玻璃、文本、设备丢失恢复 |
| src/widgets | 内置组件、数据提供者、刷新策略 |
| src/persistence | 配置校验、迁移、原子保存、备份恢复 |
| include/aura | 确有跨模块需求的稳定接口，避免提前过度抽象 |
| assets/icons | 自有或明确可再分发的图标及来源 |
| assets/themes | 小体积主题资源；以数据配置为主 |
| config/examples | 可公开的配置格式草案 |
| tests/unit | 纯算法、配置迁移、调度等 |
| tests/integration | Windows 窗口和生命周期集成 |
| tests/performance | 性能场景驱动与回归检查 |
| benchmarks/scenarios | 可重复的场景定义 |
| benchmarks/results | 经审阅、脱敏的测量汇总 |
| tools/scripts | 本地检查、构建和测量脚本 |
| cmake | 后续构建辅助模块 |
| packaging/windows | 发布清单、图标、安装与卸载配置 |
| docs/architecture/adr | 带理由和替代方案的架构决策 |
| .github | CI、Issue 与 PR 模板 |

空目录通过 .gitkeep 跟踪；实现落地时删除对应占位文件。

## 本地路径

- 源码：F:\Evoly_Aura
- 推荐构建目录：D:\dev\build\Evoly_Aura
- 推荐下载与工具：D:\dev\installers、D:\dev 下的专用工具目录
- 推荐临时测量：D:\dev\tmp\Evoly_Aura

本机工具路径不得硬编码进可移植的构建文件。
未来用 CMakeUserPresets.json 或未跟踪的本机配置覆盖。
已有厂商管理的 Visual Studio 可以继续使用，新增可配置缓存仍优先 D 盘。

## 构建入口计划

CMake + MSVC + Windows SDK，C++20，先验证 Windows x64。
P0 建立最小构建后再提交 CMakeLists.txt 与可共享 CMakePresets.json。
当前不要把“目录存在”解释为“模块已实现”。
