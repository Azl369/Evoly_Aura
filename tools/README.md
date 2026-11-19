# 工具

scripts/Validate-Workspace.ps1：只读检查目录、文档相对链接和 JSON 语法。
兼容 Windows PowerShell 5.1 / PowerShell 7，不下载工具、不安装依赖、不访问 GitHub。

本地示例：

~~~powershell
powershell -NoProfile -File .\tools\scripts\Validate-Workspace.ps1
~~~

后续构建、测量脚本在 P0 与相应实现一起增加。
