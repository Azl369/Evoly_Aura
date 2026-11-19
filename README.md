# Evoly Aura · 浮光桌面

面向 Windows 的低内存玻璃桌面空间：桌面陈列、悬浮 HUD、自由编排。

**当前阶段：工作区和计划已建立；尚未实现应用，也没有实测性能数据。**

项目名称为 Evoly Aura，目录与远端仓库名称统一为 Evoly_Aura。
远端仓库：[Azl369/Evoly_Aura](https://github.com/Azl369/Evoly_Aura)（公开，已完成首次同步）。

## 产品方向

- 桌面态：文件夹、快捷方式和信息面板融入壁纸。
- 悬浮态：选中的面板成为置顶 HUD，支持吸附和闲置淡化。
- 编辑态：调整布局、尺寸和主题，完成后锁定布局。
- 低占用优先：原生常驻核心，按事件刷新，共享资源，有界缓存。

继承 Evoly 的交互经验、视觉语义与玻璃质感；当前选择 C++ / Win32 常驻架构。
Flutter 编辑器仅保留为未来备选，不属于当前依赖。

## 文档入口

1. [产品范围](docs/product/PRODUCT_BRIEF.md)
2. [工作区目录与职责](docs/development/WORKSPACE.md)
3. [架构设计](docs/architecture/ARCHITECTURE.md)
4. [原生架构决策 ADR](docs/architecture/adr/0001-native-resident-core.md)
5. [玻璃材质与渲染策略](docs/architecture/RENDERING.md)
6. [性能预算与测量协议](docs/performance/PERFORMANCE_BUDGET.md)
7. [分阶段开发计划](docs/plans/ROADMAP.md)
8. [首个性能原型计划](docs/plans/P0_NATIVE_PROBE.md)
9. [测试与验收](docs/testing/VALIDATION.md)
10. [Git / GitHub 工作流](docs/development/GIT_WORKFLOW.md)
11. [交付与远端状态](docs/development/DELIVERY_STATUS.md)

## 工作区概览

~~~text
Evoly_Aura/
  apps/host/                原生常驻入口（预留）
  apps/editor/              按需编辑器（预留）
  src/                      核心、平台、渲染、组件、存储（预留）
  include/aura/             稳定跨模块接口（预留）
  assets/                   图标、主题；不存大体积演示媒体
  config/examples/          可公开的配置格式草案
  docs/                     产品、设计决策、计划和验收
  tests/                    单元、集成、性能测试（预留）
  benchmarks/               测量协议与汇总，不存机器原始大文件
  tools/scripts/            工作区检查与后续开发工具
  cmake/                    后续构建模块（预留）
  packaging/windows/        后续 Windows 发布配置（预留）
  .github/                  文档检查与协作模板
~~~

## 当前可执行检查

Windows PowerShell 5.1 或 PowerShell 7：

~~~powershell
powershell -NoProfile -File .\tools\scripts\Validate-Workspace.ps1
~~~

这只检查文档、目录约定、相对链接和 JSON；当前没有 CMake 构建入口或应用可执行文件。
运行时与工具链要求会在 P0 实验完成后固定。

## 开发约定

- 项目源码在 F:\Evoly_Aura；新增 SDK、下载、包缓存和构建输出优先放 D:\dev。
- main 保存可复查的阶段成果；日常开发使用 codex/ 前缀分支。
- 不提交凭据、个人数据、本地路径配置、编译产物或性能原始转储。
- 尚未选定开源许可证。公开可见不等于已经授予开源许可；选定许可证后再正式发布。
