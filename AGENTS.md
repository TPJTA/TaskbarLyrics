# AGENTS.md

本文件适用于整个 Taskbar Lyrics 仓库，供自动化代码代理和参与开发的贡献者使用。

## 项目目标

Taskbar Lyrics 是一个 Windows 11 x64 插件，在网易云音乐宿主进程内运行，并将同步歌词显示在主显示器底部任务栏的可用区域。它不是独立桌面歌词程序，也不应引入常驻服务或额外播放器状态源。

当前验证的宿主版本是网易云音乐 Windows x64 `3.1.34.205281`。涉及 CEF、Webpack、Redux 或宿主 ABI 的代码均应视为版本敏感代码。

## 开始修改前

按任务需要阅读以下资料：

- `CONTEXT.md`：领域术语；讨论和文档优先使用其中定义的名称。
- `docs/requirements.md`：行为边界和验收要求。
- `docs/design.md`：歌词窗口的视觉约束。
- `docs/adr/`：已经接受的架构决策。

不要在未更新 ADR 的情况下悄悄推翻现有架构决策。

## 核心约束

- 仅支持 Windows x64；保持 Unicode 构建和 C++20。
- 插件与 Host Client 生命周期一致，不新增常驻进程。
- 不修改 `cloudmusic.exe`、`cloudmusic.dll` 或 `package/orpheus.ntpk`。
- 不绕过 Host Client 单独请求歌词 API；继续复用宿主的登录态、歌词请求和解析结果。
- `msimg32.dll` 必须保留系统模块的五个标准导出，并将调用转发到系统 DLL。
- 不在 `DllMain` 中执行阻塞、挂接或复杂初始化；继续通过安全的延迟启动路径初始化插件。
- 任一兼容性检查、桥接或 UI 初始化失败时，优先保证宿主启动，停用插件当前会话并记录日志。
- Lyric Window 是独立透明覆盖窗口，不要跨进程嵌入 Explorer。
- Lyric Window 不得覆盖已有任务栏控件；无可读空间、任务栏隐藏或主显示器全屏时应隐藏。
- 保持原文和翻译左对齐、整行切换以及独立的单次水平滚动，不增加逐字高亮。
- 右键使用原生 Windows 菜单；左键不执行操作。

## 目录职责

- `src/proxy/`：代理 DLL 入口、系统导出和加载边界。
- `src/core/`：启动、配置和日志基础设施。
- `src/bridge/`：CEF 探测、JavaScript 注入以及宿主状态协议。
- `src/ui/`：任务栏布局、可见性、绘制、滚动和右键菜单。
- `tests/`：代理加载测试、桥接协议测试和窗口预览程序。
- `scripts/`：可重复的构建、安装和卸载入口。
- `third_party/cef/`：第三方 CEF 接口与许可证；除明确升级依赖外不要修改。

新代码应放入最接近其职责的现有模块，不要把宿主探测、状态解析和窗口绘制混在同一层。

## 构建与验证

推荐从仓库根目录运行：

```powershell
.\scripts\build.ps1 -Configuration Release
```

该脚本会配置项目、构建并运行全部 CTest 测试。需要验证干净构建时运行：

```powershell
.\scripts\build.ps1 -Configuration Release -Clean
```

常用的单独验证命令：

```powershell
ctest --test-dir build -C Release --output-on-failure
.\build\Release\window_preview.exe
.\scripts\install.ps1 -DryRun
```

如果当前终端无法直接找到 `ctest`，使用 `scripts/build.ps1`；该脚本会定位 Visual Studio 随附的 CMake、CTest 和 Ninja。

提交代码前至少执行与改动范围匹配的验证：

- 修改代理或启动流程：运行 `proxy_smoke_test`。
- 修改桥接负载、解析或状态转换：运行 `host_bridge_test`，并补充对应协议用例。
- 修改布局、绘制、滚动或菜单：构建 `window_preview` 并进行人工预览。
- 修改安装逻辑：先运行 `install.ps1 -DryRun`，不得用测试覆盖未知的现有代理 DLL。
- 修改发布流程：确认 Release ZIP 仍包含 `build/Release/msimg32.dll`、运行时脚本、README 和 CEF 许可证。

## 代码风格

- 延续现有 C++ 风格：4 空格缩进、清晰的所有权、`noexcept`/`[[nodiscard]]` 在合适处使用。
- Win32 句柄和挂钩必须有明确的释放路径；失败分支不得遗留半初始化状态。
- 用户可见文本使用简体中文；源码、日志和内部标识保持现有英文命名习惯。
- PowerShell 脚本启用严格错误处理，并优先使用 `-LiteralPath` 处理文件路径。
- 文件删除和移动前验证目标路径；安装器永远不得覆盖来源未知的 `msimg32.dll`。
- 不提交 `build/`、`.vs/`、本机配置、日志或生成的 Release 包。

## 版本与发布

正式发布前同步更新：

1. `CMakeLists.txt` 中的 `project(... VERSION ...)`。
2. `scripts/install.ps1` 写入安装清单的版本。
3. 发布标签，例如 `v0.2.0`。

`.github/workflows/release.yml` 会拒绝版本不一致的标签。`v0.2.0-beta.1` 之类的标签允许使用同一基础版本，并会发布为 Pre-release。

优先使用 `.\scripts\release.ps1 -Version <version> -Publish` 完成版本同步、构建、提交、打标签和推送。该命令要求干净工作区；不要绕过它的工作区或标签检查。

不要手工提交构建产物来替代 Release 工作流。

## 文档维护

- 行为或兼容性变化时同步更新 `README.md` 和 `docs/requirements.md`。
- 领域概念发生变化时更新 `CONTEXT.md`，并沿用统一术语。
- 架构取舍发生变化时新增 ADR；保留旧 ADR，并更新其状态或通过新 ADR 取代。
- 不要宣称支持尚未实际验证的 Windows、任务栏或网易云音乐版本。

## 安全边界

这个项目运行在第三方宿主进程内，任何崩溃都可能影响用户正在使用的网易云音乐。避免引入不必要的全局挂钩、跨进程写入、未校验的指针访问以及阻塞 UI/CEF 线程的操作。

当“插件功能完整”和“宿主稳定”冲突时，选择宿主稳定，并留下可诊断日志。
