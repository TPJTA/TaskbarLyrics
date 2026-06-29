# Taskbar Lyrics

让网易云音乐的同步歌词安静地待在 Windows 11 任务栏里。

Taskbar Lyrics 是一个运行在网易云音乐进程内的 Windows x64 插件。它不需要额外的常驻程序，会在主显示器底部任务栏的可用区域显示当前歌词和可选翻译。

> [!WARNING]
> 项目仍处于实验阶段，目前仅在网易云音乐 Windows x64 `3.1.34.205281` 上完成验证。它依赖客户端内部的 CEF、Webpack 和 Redux 结构；网易云音乐升级后，插件可能失效，极端情况下可能导致客户端崩溃。

## 功能

- 随网易云音乐自动加载和退出，不启动额外后台进程。
- 在任务栏的空闲区域显示歌词，不覆盖已有任务栏按钮。
- 支持单行原文以及原文、翻译双行显示。
- 长歌词按当前行持续时间横向滚动，暂停播放时同步冻结。
- 切歌后立即清除旧歌词，并主动请求新歌词。
- 自动适配浅色、深色任务栏和 DPI 缩放。
- 跟随任务栏自动隐藏，并在主显示器有全屏应用时隐藏。
- 通过原生右键菜单控制翻译、播放和切歌。
- 将显示偏好保存在独立配置文件中。

## 兼容性

| 项目 | 当前支持 |
| --- | --- |
| 操作系统 | Windows 11 |
| 网易云音乐 | Windows x64 `3.1.34.205281` |
| 任务栏位置 | 主显示器底部 |
| 系统任务栏 | Windows 11 默认任务栏 |
| 处理器架构 | x64 |

目前不支持 Windows 10、32 位网易云音乐、顶部或侧边任务栏、副显示器任务栏歌词，以及第三方任务栏替代程序。

## 安装

### 从 Release 安装

1. 在 GitHub Releases 下载最新的 `TaskbarLyrics-<版本>-win-x64.zip`。
2. 解压 ZIP。
3. 从系统托盘彻底退出网易云音乐。
4. 在解压目录打开 PowerShell。若网易云安装在 `Program Files`，请使用管理员 PowerShell。
5. 运行：

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\install.ps1
```

安装器会从 Windows 卸载注册表查找网易云音乐，并检查目标目录、x64 架构和现有代理 DLL。注册表无法定位时，才会要求手动输入安装目录。

你也可以先执行不写入文件的检查：

```powershell
.\scripts\install.ps1 -DryRun
```

或者显式指定网易云目录：

```powershell
.\scripts\install.ps1 -CloudMusicPath 'C:\Program Files\NetEase\CloudMusic'
```

安装器不会覆盖来源未知的 `msimg32.dll`。安装成功后，网易云目录中会生成 `.taskbar-lyrics.install.json`，用于记录和校验插件文件。

### 从源码安装

先完成[本地构建](#本地构建)，再运行：

```powershell
.\scripts\install.ps1
```

## 使用

启动网易云音乐并播放歌曲后，歌词会出现在主显示器任务栏的可用区域。

右键单击歌词可打开系统原生菜单：

- `显示翻译`
- `上一首`
- `播放` / `暂停`
- `下一首`

左键单击歌词不会执行操作。歌曲没有翻译时，`显示翻译`会暂时禁用，但不会改变已保存的显示偏好。

歌词状态说明：

| 显示内容 | 含义 |
| --- | --- |
| `歌词加载中…` | 已检测到歌曲，正在等待歌词数据 |
| `歌词加载失败` | 网易云歌词请求失败，插件会等待后续重试 |
| `纯音乐，请欣赏` | 当前歌曲没有可用歌词 |
| 不显示歌词窗口 | 当前没有加载歌曲，或任务栏区域不可用 |

## 配置与日志

配置文件：

```text
%LOCALAPPDATA%\TaskbarLyrics\config.ini
```

当前支持的配置：

```ini
[Display]
ShowTranslation=1
```

日志文件：

```text
%LOCALAPPDATA%\TaskbarLyrics\logs\taskbar-lyrics.log
```

如果歌词长时间停留在加载状态，或网易云音乐启动异常，请先：

1. 确认网易云音乐版本是否为已验证版本。
2. 查看上述日志文件。
3. 完全退出网易云音乐后卸载插件，确认问题是否消失。

## 卸载

从系统托盘彻底退出网易云音乐，然后在插件目录运行：

```powershell
.\scripts\uninstall.ps1
```

卸载器只删除安装清单中属于 Taskbar Lyrics 且哈希匹配的文件。配置和日志会保留。

## 工作原理

Taskbar Lyrics 以 `msimg32.dll` 代理 DLL 的形式安装到网易云音乐目录。客户端加载代理后，代理会继续向系统 `msimg32.dll` 转发标准 GDI 导出，并在 `DllMain` 之外启动插件逻辑。

运行链路如下：

```text
网易云音乐启动
    ↓
加载 msimg32 代理 DLL
    ↓
挂接 CEF 浏览器创建入口
    ↓
从 Webpack / Redux 获取歌曲、歌词和播放状态
    ↓
通过进程内桥接传给原生代码
    ↓
在任务栏范围内绘制透明歌词窗口
```

插件不会修改 `cloudmusic.exe`、`cloudmusic.dll` 或 `package/orpheus.ntpk`，也不会自行调用外部歌词 API。歌词仍由网易云音乐现有会话和内部歌词服务加载，因此可以复用登录态、云盘歌曲支持和客户端自身的歌词解析逻辑。

## 本地构建

### 环境要求

- Visual Studio 2022 或更高版本
- “使用 C++ 的桌面开发”工作负载
- MSVC x64 工具链
- Windows SDK
- CMake 3.25 或更高版本
- Ninja
- PowerShell 5.1 或更高版本

构建脚本会自动查找 Visual Studio 自带的 CMake、Ninja 和开发者环境。

### 构建与测试

在项目根目录运行：

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\build.ps1
```

该命令会配置 CMake、编译 Release 版本，并运行代理 DLL 冒烟测试和桥接协议测试。

输出文件：

```text
build\Release\msimg32.dll
```

其他构建方式：

```powershell
.\scripts\build.ps1 -Configuration Debug
.\scripts\build.ps1 -Configuration Release -Clean
```

构建后也可以启动独立窗口预览：

```powershell
.\build\Release\window_preview.exe
```

## 发布

仓库中的 [Release 工作流](./.github/workflows/release.yml) 会在推送 `v*` 标签时自动完成构建、测试、打包和 GitHub Release 创建。

例如发布 `0.1.0`：

```powershell
git tag v0.1.0
git push origin v0.1.0
```

标签、`CMakeLists.txt` 和 `scripts/install.ps1` 中的版本必须一致。带后缀的标签（例如 `v0.1.0-beta.1`）会创建为预发布版本。

Release 包含：

```text
TaskbarLyrics-v0.1.0-win-x64.zip
TaskbarLyrics-v0.1.0-win-x64.zip.sha256
```

## 项目结构

```text
TaskbarLyrics/
├─ .github/workflows/  GitHub Actions
├─ docs/               需求、视觉设计与架构决策
├─ scripts/            构建、安装和卸载脚本
├─ src/
│  ├─ bridge/          CEF、Webpack 与宿主状态桥接
│  ├─ core/            启动、配置与日志
│  ├─ proxy/           msimg32 代理 DLL
│  └─ ui/              任务栏布局与歌词窗口
├─ tests/              冒烟测试、桥接测试与窗口预览
└─ third_party/        CEF 接口头文件及许可证
```

开发前建议阅读：

- [项目上下文与统一术语](./CONTEXT.md)
- [详细需求](./docs/requirements.md)
- [视觉设计](./docs/design.md)
- [架构决策记录](./docs/adr)
- [代码代理协作说明](./AGENTS.md)

## 风险与恢复

Taskbar Lyrics 使用 DLL 代理以及网易云音乐未公开的内部接口，兼容性天然比普通桌面应用脆弱。

项目遵循“宿主优先”原则：兼容性检查、挂接、桥接或窗口初始化失败时，插件应停用当前会话并写入日志，而不是阻止网易云音乐启动。如果遇到异常，请完全退出网易云音乐后运行卸载脚本恢复。

## 许可证与声明

本项目与网易、网易云音乐没有隶属或授权关系。“网易云音乐”及相关商标归其权利人所有。

CEF 相关文件遵循 [third_party/cef/LICENSE.txt](./third_party/cef/LICENSE.txt) 中的许可证。仓库目前未提供项目自身的开源许可证；如需分发、修改或二次使用，请先向项目维护者确认授权范围。
