# Git 与 GitHub 工作流

远端：[Azl369/Evoly_Aura](https://github.com/Azl369/Evoly_Aura)（公开，已创建并完成首次推送）。
origin 已配置，main 跟踪 origin/main；核验记录见 [交付状态](DELIVERY_STATUS.md)。

## 提交身份

本仓库使用 Azl369 及 GitHub noreply 地址作为本地提交身份，不修改全局 Git 配置。
当前规划地址为 73180687+Azl369@users.noreply.github.com；后续可以按账户设置校正。

## 分支与提交

- main：经过检查的阶段成果。
- codex/<topic>：原型、功能、修复或文档工作分支。
- 提交示例：docs: initialize workspace and implementation plans。
- 每个提交保持一个完整意图；变更架构时同步 ADR 和计划。
- 不强推 main，不把普通同步写成历史重置。
- 功能完成后通过 PR 合并；初始空仓库的基线提交可直接推 main。
- 后续阶段可采用 v0.1.0 等标签；尚无应用时不发布虚假的可运行版本。

## 每次同步

~~~powershell
git status --short --branch
powershell -NoProfile -File .\tools\scripts\Validate-Workspace.ps1
git diff --check
git diff --stat
git add <本次明确修改的文件>
git diff --cached
git commit -m "docs: update implementation plan"
git push
~~~

已有远端时，开始工作前先 fetch 并检查分支差异。工作区干净时才使用 pull --ff-only。
出现分叉先检查历史，不自动 reset 或 force push。
“同步更新”指阶段性 commit + push；此工作区不安装定时自动提交任务。

## 首次 GitHub 操作（已完成，以下保留为恢复指南）

本地 CLI 检测到旧登录失效时，由用户在可信终端完成交互登录：

~~~powershell
& 'C:\Program Files\GitHub CLI\gh.exe' auth login --hostname github.com --git-protocol https --web
& 'C:\Program Files\GitHub CLI\gh.exe' auth status
& 'C:\Program Files\GitHub CLI\gh.exe' auth setup-git
~~~

浏览器必须确认登录账号为 Azl369。不在聊天、文件、提交或日志中粘贴令牌。

如果远端尚不存在，在项目目录执行：

~~~powershell
gh repo create Azl369/Evoly_Aura --public --source . --remote origin --push
~~~

或者在 GitHub 网页创建同名公开空仓库，不勾选 README、.gitignore 和许可证初始化项。
随后在本地添加 origin 并首次推送：

~~~powershell
git remote add origin https://github.com/Azl369/Evoly_Aura.git
git push -u origin main
~~~

如果 origin 已存在，先 git remote -v 核实；不要盲目覆盖。
如果远端已含文件和历史，先 fetch 检查再协调合并，不执行覆盖式初始化推送。

## 公开仓库约定

- 不提交账号配置、个人便签、文件路径样本、壁纸原图、凭据、性能转储。
- 工作流权限默认只读；初期 CI 只检查文档与配置，不发布应用。
- 分支保护与必需检查在仓库创建后按账户支持情况设置，不预设已经生效。
- 开源许可证由项目所有者选定；当前不擅自附加 MIT 或其他许可。
