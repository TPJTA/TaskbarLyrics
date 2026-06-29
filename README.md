# Taskbar Lyrics

Taskbar Lyrics 是一个面向 Windows 11 的网易云音乐任务栏歌词插件。它运行在网易云音乐进程内部，不需要额外启动常驻程序，并在主显示器底部任务栏的可用区域内显示当前歌词和可选翻译。

> 当前为实验性版本，仅验证网易云音乐 Windows x64 客户端 `3.1.34.205281`。网易云升级后，内部 CEF/Redux 结构可能发生变化，请勿直接用于其他版本。

## 功能

- 网易云启动时自动加载，无需单独打开插件。
- 在 Windows 11 默认底部任务栏内显示歌词，不覆盖已有任务栏按钮。
- 主歌词与翻译最多两行；没有翻译时仅显示主歌词。
- 主歌词和翻译均使用微软雅黑 14 逻辑 px，并按 `DPI / 96` 缩放。
- 长歌词在当前歌词时长内完成一次水平滚动；暂停时冻结滚动位置。
- 切歌后立即清除旧歌词，并主动请求新歌词。
- 无需进入播放页面：插件会主动派发网易云内部歌词加载动作，并在无结果时自动重试。
- 自动适配浅色/深色任务栏，使用轻微文字阴影。
- 跟随任务栏自动隐藏；全屏应用出现时隐藏。
- 设置通过配置文件持久化。

## 右键菜单

右键单击歌词区域可打开原生 Windows 菜单：

- `显示翻译`
- `上一首`
- `播放` / `暂停`
- `下一首`

左键单击不执行操作。当前歌曲没有翻译时，`显示翻译`会被禁用，但不会修改已保存的偏好。

## 兼容性

当前仅支持：

- Windows 11
- 主显示器
- 位于屏幕底部的系统默认任务栏
- 网易云音乐 Windows x64 `3.1.34.205281`

暂不支持：

- Windows 10
- 侧边或顶部任务栏
- 副显示器任务栏歌词
- 第三方任务栏替代程序
- 32 位网易云音乐
- 其他网易云版本

## 工作原理

插件以 `msimg32.dll` 代理 DLL 的形式安装到网易云目录。网易云加载代理后，代理会把原有 GDI 导出转发给系统 `msimg32.dll`，并在主进程内启动 Taskbar Lyrics。

歌词桥接流程如下：

1. 挂接网易云创建 CEF 浏览器的入口。
2. 向网易云页面注入少量 JavaScript。
3. 从当前 Webpack runtime 获取 Redux store 和实时播放进度。
4. 检测到歌曲变化时主动派发 `async:lyric/fetchLyric`。
5. 通过私有页面标题消息把歌曲、歌词、翻译和播放状态传回原生 DLL。
6. 使用透明分层窗口在任务栏可用区域绘制歌词。

插件不会修改 `cloudmusic.exe`、`cloudmusic.dll` 或 `package/orpheus.ntpk`，也不会启动额外后台进程。歌词仍由网易云现有会话和内部歌词服务加载，以保留登录态、云盘歌曲及网易云自己的歌词解析行为。

## 构建环境

需要安装：

- Visual Studio 2022 或更高版本
- “使用 C++ 的桌面开发”工作负载
- MSVC x64 工具链
- Windows SDK
- CMake 3.25 或更高版本
- Ninja
- PowerShell 5.1 或更高版本

Visual Studio 自带的 CMake 和 Ninja 会由构建脚本自动发现，不需要手动配置环境变量。

## 构建

在 PowerShell 中进入项目目录：

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\build.ps1
```

构建脚本会：

1. 查找 Visual Studio 与 x64 C++ 工具链。
2. 使用 Ninja Multi-Config 配置 CMake。
3. 编译 Release 版本。
4. 运行代理 DLL 冒烟测试和桥接协议测试。

输出文件：

```text
build\Release\msimg32.dll
```

其他用法：

```powershell
.\scripts\build.ps1 -Configuration Debug
.\scripts\build.ps1 -Clean
```

## 安装

先从系统托盘彻底退出网易云音乐。若网易云安装在 `Program Files`，请使用管理员 PowerShell：

```powershell
.\scripts\install.ps1
```

安装脚本会优先从卸载注册表中读取网易云目录，并验证：

- `cloudmusic.exe`
- `cloudmusic.dll`
- `package\orpheus.ntpk`
- 客户端版本与 x64 架构

注册表无法定位时，脚本才会要求手动输入目录。

可先进行不写入文件的检查：

```powershell
.\scripts\install.ps1 -DryRun
```

也可以显式指定安装目录：

```powershell
.\scripts\install.ps1 -CloudMusicPath 'C:\Program Files\NetEase\CloudMusic'
```

安装器不会覆盖来源未知的现有 `msimg32.dll`。成功安装后会生成 `.taskbar-lyrics.install.json`，用于记录并验证插件文件。

## 卸载

先彻底退出网易云音乐，然后运行：

```powershell
.\scripts\uninstall.ps1
```

卸载器只会删除清单中属于 Taskbar Lyrics 且哈希匹配的文件。配置与日志会保留。

## 配置与日志

配置文件：

```text
%LOCALAPPDATA%\TaskbarLyrics\config.ini
```

日志文件：

```text
%LOCALAPPDATA%\TaskbarLyrics\logs\taskbar-lyrics.log
```

当前配置项：

```ini
[Display]
ShowTranslation=1
```

如果歌词长期停在“歌词加载中…”或网易云启动异常，请先查看日志，并确认客户端版本没有变化。

## 状态文本

- `歌词加载中…`：已检测到歌曲，正在等待歌词结果。
- `歌词加载失败`：歌词请求暂时失败，插件会等待网易云重试。
- `纯音乐，请欣赏`：当前歌曲没有可用歌词。
- 无歌曲：歌词窗口隐藏。

## 项目结构

```text
TaskbarLyrics/
├─ src/
│  ├─ bridge/     CEF、Webpack 与宿主状态桥接
│  ├─ core/       启动、配置与日志
│  ├─ proxy/      msimg32 代理 DLL
│  └─ ui/         任务栏布局与歌词窗口
├─ tests/         冒烟测试、桥接测试与窗口预览
├─ scripts/       构建、安装与卸载脚本
├─ docs/          需求、设计与架构决策
└─ third_party/   CEF 接口头文件及许可证
```

进一步阅读：

- [项目上下文与术语](./CONTEXT.md)
- [详细需求](./docs/requirements.md)
- [视觉设计](./docs/design.md)
- [架构决策记录](./docs/adr)

## 风险与故障隔离

Taskbar Lyrics 使用 DLL 代理和网易云内部 CEF/Redux 接口，属于非官方扩展。网易云更新可能导致插件失效，严重时可能引起客户端崩溃。

插件按“宿主优先”原则设计：挂接、桥接或窗口初始化失败时，应停用当前会话的插件功能并写入日志，不应阻止网易云正常启动。遇到异常时，可在网易云完全退出后运行卸载脚本恢复。

## 许可证与声明

本项目与网易、网易云音乐没有隶属或授权关系。“网易云音乐”及相关商标归其权利人所有。

CEF 相关文件遵循 [third_party/cef/LICENSE.txt](./third_party/cef/LICENSE.txt) 中的许可证。
