# ADR-0001：原生常驻核心

日期：2026-11-19
状态：接受为原型方向，待 P0 数据确认具体渲染后端。

## 背景

用户要求 Windows 桌面美化、Evoly 风格的悬浮 HUD 与磨砂质感，同时要求尽量少的常驻内存。
主平台为 Windows，跨平台复用不是首要目标。

## 决策

采用 C++20 + Win32 常驻核心，Direct2D / DirectWrite 候选渲染。
DirectComposition 与系统 Acrylic 仅在证明兼容且成本可接受时采用。
设置编辑器不作为永久常驻依赖。

## 替代方案

| 方案 | 优点 | 当前未优先的原因 |
| --- | --- | --- |
| Flutter 常驻 | 可复用 Evoly 的 UI 经验与组件 | 引擎、运行时及渲染基础成本需要常驻 |
| Electron / WebView | UI 开发方便 | 浏览器进程与渲染环境不利于最低内存基线 |
| C# Native AOT + 原生 API | 开发效率、AOT 能减少部分开销 | GC / 运行时与 UI 依赖兼容仍需评估 |
| Rust + Win32 | 原生内存潜力、所有权约束 | 不天然比 C++ 更省，当前 Windows 集成收益不明确 |
| C++ + GDI / 软件绘制 | 极简界面可能更低基线 | 文字、透明和玻璃效果的质量与 CPU 成本需比较 |

C++ 不保证自动低内存；错误的缓存和大表面同样会超预算。

## 后果

- 可以精细控制资源，但窗口、输入和无障碍实现成本增加。
- Evoly 主要复用设计规则、状态机与经验，不直接依赖 Flutter 引擎。
- 首版限制组件类型，先建立测量协议再扩张。
- P0 若发现 GPU 基础成本超出收益，保留软件绘制的静态模式作为比较项。

## 证据与参考

- [Direct2D 资源复用](https://learn.microsoft.com/en-us/windows/win32/direct2d/improving-direct2d-performance)
- [WebView2 进程模型](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/process-model)
- [Native AOT 说明](https://learn.microsoft.com/en-us/dotnet/core/deploying/native-aot/)

以上来源说明机制，不提供本项目内存实测结论。
