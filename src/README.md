# 源码模块预留

当前没有应用实现。职责详见 [工作区说明](../docs/development/WORKSPACE.md)。

依赖方向：apps 组装各模块；core 不依赖 Windows；platform / render 对接系统；
widgets 通过模型和事件连接；persistence 处理版本化配置。
新增抽象以实际原型需求为依据，避免提前建立复杂框架。
