# Notepad3

**基于 Scintilla 的快速、轻量的 Windows 文本编辑器**

[English](Readme.en.md) · **简体中文**

[![License](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg?style=flat-square)](https://opensource.org/licenses/BSD-3-Clause)
[![CI](https://img.shields.io/github/actions/workflow/status/rizonesoft/Notepad3/build.yml?style=flat-square&label=CI)](https://github.com/rizonesoft/Notepad3/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/rizonesoft/Notepad3?style=flat-square&label=Release&color=0e7490)](https://rizonesoft.com/downloads/notepad3/)
[![Nightly](https://img.shields.io/github/v/release/rizonesoft/Notepad3?include_prereleases&style=flat-square&label=Nightly&color=6e40c9)](https://github.com/rizonesoft/Notepad3/releases)

[官网](https://rizonesoft.com/downloads/notepad3/) · [下载](https://github.com/rizonesoft/Notepad3/releases) · [文档](https://rizonesoft.com/documents/notepad3/) · [常见问题](readme/faq/FAQ.md) · [更新日志](https://rizonesoft.com/downloads/notepad3/update) · [赞助](https://github.com/sponsors/rizonesoft)

---

Notepad3 是一款免费、开源的 Windows 文本编辑器，支持语法高亮。它构建于 [Scintilla](https://www.scintilla.org/) 编辑组件和 [Lexilla](https://www.scintilla.org/Lexilla.html) 词法分析库之上，内存占用小，却足以胜任专业编程与日常文本编辑。

> Notepad3 基于 Florian Balmer 的 [Notepad2](https://www.flos-freeware.ch/notepad2.html) 和 XhmikosR 的 [Notepad2-mod](https://xhmikosr.github.io/notepad2-mod/) 代码开发；MiniPath 则源自 Florian Balmer 的 metapath。

<!-- TODO: 在此处补充 Notepad3 主窗口截图。
     将图片保存为 doc/screenshot.png 并取消注释下面这一行：
     ![Notepad3 截图](doc/screenshot.png)
-->

## 功能特性

### 编辑

- **代码折叠**，支持一键全部展开 / 折叠
- **书签**，在文件内快速定位
- **单词自动补全**，可配置触发补全的填充字符
- **标记所有匹配项** —— 高亮选中单词的全部出现位置，并显示匹配数量
- **查找与替换**，采用 [PCRE2](https://github.com/PCRE2Project/pcre2) 正则表达式引擎
- **[聚焦视图](readme/focusedview/FocusedView.md)** —— 只显示与当前单词 / 选区匹配的行（Ctrl+Alt+V），支持折叠、书签与高亮三种模式
- **[TinyExpr++](readme/tinyexprcpp/TinyExprPP.md)** —— 行内数学表达式求值，内置 80 多个函数（输入 `表达式=?` 即可就地计算，也可在行号中使用 `${expr}`）
- **撤销 / 重做**保留选区状态
- 未选中内容时，按 **Visual Studio 风格**复制 / 粘贴当前行
- **虚拟空格**矩形选区（Alt+拖拽）
- 可配置分隔符的**加速单词跳转**
- 在文档中直接**插入 GUID**
- **剪贴板监视模式**（`/b` 命令行开关）—— 每次剪贴板变化都会自动作为新条目追加到文档末尾；可随时通过 **编辑 → 停止剪贴板监视** 结束，无需关闭编辑器

### 语法高亮

支持 55 种以上语言，包括：

> ANSI Art、Apache 配置、汇编、AutoHotkey、AutoIt3、AviSynth、Awk、批处理、C/C++、C#、CMake、CoffeeScript、CSS、CSV、D、Dart、Diff、F77/Fortran、Go、HTML/XML、Java、JavaScript、JSON、Julia、Kotlin、KiXtart、LaTeX、Lua、Makefile、Markdown、MATLAB、Nim、NSIS、Pascal、Perl、PHP、PowerShell、Python、R/S-Plus、注册表、资源脚本、Ruby、Rust、Shell 脚本、SQL、SystemVerilog、Tcl、TOML、VBScript、VHDL、Verilog、Visual Basic、YAML 等

### 文件处理

- **[AES-256 Rijndael](readme/encryption/Encryption.md)** 加解密（编辑器内操作与命令行批处理工具均可）
- 由 [uchardet](https://www.freedesktop.org/wiki/Software/uchardet/) 驱动的 **[编码检测](readme/uchardet/EncodingDetection.md)**
- **文件变更监视**，检查间隔可配置
- **[`.LOG` 自动时间戳](readme/config/FileContentFlags.md#log-auto-timestamp)** —— 首行为 `.LOG` 的文件在打开时会自动追加当前日期与时间，与经典记事本的行为一致
- 支持 **[Emacs 文件变量](readme/config/FileContentFlags.md#emacs-file-variables)**（编码、模式、制表符宽度等）
- **文件历史**会保留光标位置与编码
- **便携设计** —— 可从 U 盘直接运行，设置使用相对路径保存

### 搜索

- **grepWin** —— 集成的文件内容搜索工具，支持正则表达式（**Ctrl+Shift+F**）
- **超链接热点高亮** —— Ctrl+单击在浏览器中打开，Alt+单击在编辑器中加载

### 用户界面

- **高 DPI 感知**，配备高清工具栏图标
- **深色模式**，配色可自定义
- **可自定义状态栏**，提供 16 个可配置字段（行、列、编码、[TinyExpr++](readme/tinyexprcpp/TinyExprPP.md) 求值、Unicode 码位等）
- **可自定义工具栏标签** —— 在图标旁显示功能名称
- **缩放**范围 10% ～ 1000%（Ctrl+滚轮或工具栏按钮）
- **窗口透明模式**，透明度可配置

### 本地化

提供 26 种语言翻译：

> 南非荷兰语、白俄罗斯语、中文（简体 / 繁体）、荷兰语、英语（美国 / 英国）、芬兰语、法语、德语、希腊语、印地语、匈牙利语、印尼语、意大利语、日语、韩语、波兰语、葡萄牙语（巴西 / 葡萄牙）、俄语、斯洛伐克语、西班牙语、瑞典语、土耳其语、越南语

### 配套工具

- **[MiniPath](minipath/)** —— 快速文件浏览插件（Ctrl+M）
- **[grepWin](grepWin/)** —— 强大的跨文件搜索与替换工具

## 系统要求

- **操作系统：** Windows 8.1 / 10 / 11
- **架构：** x86 (Win32)、x64、x64 (AVX2)、ARM64

## 下载与安装

| 渠道 | 链接 |
|------|------|
| **稳定版** | [rizonesoft.com/downloads/notepad3](https://rizonesoft.com/downloads/notepad3/) |
| **GitHub Releases** | [github.com/rizonesoft/Notepad3/releases](https://github.com/rizonesoft/Notepad3/releases) |
| **每日构建** | [GitHub 预发布版](https://github.com/rizonesoft/Notepad3/releases) |

Notepad3 完全**便携**，无需安装：解压压缩包后直接运行 `Notepad3.exe` 即可。设置保存在与可执行文件同目录的 `Notepad3.ini` 中。系统集成（替换 Windows 记事本）的方法参见[官方文档](https://rizonesoft.com/documents/notepad3/)。

## 从源码构建

Notepad3 使用 **Visual Studio 2022**（工具集 v143）构建，目标平台为 Windows SDK。

```powershell
# 还原 NuGet 包（首次构建时执行一次即可）
nuget restore Notepad3.sln

# 构建指定平台
Build\Build_x64.cmd Release

# 构建全部平台（Win32、x64、x64_AVX2、ARM64）
Build\BuildAll.cmd Release

# 也可以直接使用 MSBuild
msbuild Notepad3.sln /m /p:Configuration=Release /p:Platform=x64
```

构建前请先运行 `Version.ps1`，根据 `Versions\` 中的模板生成版本头文件。

## 配置

Notepad3 使用便携的 INI 文件保存全部设置。按 **Ctrl+F7** 可直接在编辑器中打开该文件。

- **界面设置**（`[Settings]`）—— 通过「菜单 → 设置」管理
- **高级设置**（`[Settings2]`）—— 需手动编辑，多数设置重启后生效
- **状态栏**（`[Statusbar Settings]`）—— 自定义字段的布局、顺序与宽度
- **工具栏标签**（`[Toolbar Labels]`）—— 在图标旁显示功能名称

📖 **完整配置参考：** [readme/config/Configuration.md](readme/config/Configuration.md)

🎨 **配色方案、样式与主题：** [readme/schema/CustomSchema.md](readme/schema/CustomSchema.md) —— 分层覆盖模型、样式迷你语言、`视图 → 自定义方案`（**F12**），以及自定义主题的导出 / 导入 / 收集方法。

📄 **文件内容标记**（`.LOG`、Emacs 文件变量、编码标签、shebang 词法提示）：[readme/config/FileContentFlags.md](readme/config/FileContentFlags.md) —— 写在文件内部、用于控制 Notepad3 打开或着色方式的标记。

## 命令行选项

Notepad3 继承了 Notepad2 / Notepad2-mod 丰富的命令行开关，并增加了若干 Notepad3 专有扩展。所有开关不区分大小写，前缀可使用 `/` 或 `-`。

```bat
Notepad3.exe /utf8sig /crlf d:\temp\Test.txt        :: 以 UTF-8 BOM + CRLF 打开
Notepad3.exe /g 250,5 src\Notepad3.c                :: 跳转到第 250 行第 5 列
Notepad3.exe /m TODO main.c                         :: 打开并查找第一个 "TODO"
Notepad3.exe /b /i                                  :: 剪贴板收集模式，启动时驻留托盘
                                                     :: （在最小化前的短暂窗口期内按住 Ctrl，
                                                     ::  即可让编辑器保持可见）
Notepad3.exe /v report.log                          :: 静默打印后退出
Notepad3.exe /?                                     :: 打开内置帮助对话框
```

功能一览：

- **文件参数与多文件** —— 位置路径、`+`、`-`、`/y`、`/z`
- **编码** —— `/ansi`、`/unicode`、`/unicodebe`、`/utf8`、`/utf8sig`、`/e <名称>`
- **换行符** —— `/crlf`、`/cr`、`/lf`
- **定位与搜索** —— `/g`、`/m`，支持正则 / 大小写 / 反斜杠修饰符
- **文件监视** —— `/l`、`/l0`、`/l1`
- **新建文档** —— `/q`、`/qs`、`/c`
- **剪贴板收集** —— `/b`（与 `/i` 组合时支持延迟最小化、按住 Ctrl 跳过）
- **词法分析器** —— `/s`、`/d`、`/h`、`/x`
- **窗口位置与行为** —— `/p`（含预设）、`/t`、`/i`、`/o`
- **窗口复用 / 单实例** —— `/n`、`/ns`、`/r`、`/rs`、`/rp`
- **打印** —— `/v`、`/vd`
- **INI 与提权** —— `/f`、`/f0`、`/u`
- **Shell 集成** —— `appid=`、`sysmru=`

⌨️ **完整参考：** [readme/cmdln/CmdLnOptions.md](readme/cmdln/CmdLnOptions.md) —— 每个开关的参数、示例、持久化说明，以及与 Notepad2 / Notepad2-mod 的兼容性。

## 菜单与命令

按应用内菜单树的结构，逐项说明 Notepad3 菜单和右键菜单中的所有条目，涵盖「文件」「编辑」「搜索」「视图」「外观」「设置」「帮助」以及四个右键菜单；每一项都附有快捷键、行为说明和相关深度文档的链接。

📋 **菜单参考：** [readme/MenuEntriesAndCmds.md](readme/MenuEntriesAndCmds.md)

## 文件路径处理

说明 Notepad3 如何把可点击链接、选中文本和形似路径的字符串解析为真实文件 —— 包括带行号的裸路径（`file.c:42`、`file.c(42)`）、`file:///` URL、相对路径的锚定规则（当前文档目录 → 工作目录）、环境变量展开，以及两个新增的选区命令「打开所选内容所在文件夹」/「从所选内容打开文件」。

🗂️ **路径处理参考：** [readme/paths/FilePathHandling.md](readme/paths/FilePathHandling.md)

## 键盘快捷键

Notepad3 支持的全部组合键 —— 文件、编辑、视图、搜索、自定义 —— 按菜单分组，并标注由 Notepad2 迁移到 Notepad3 时的按键调整。

⌨️ **Notepad3 快捷键：** [readme/KeyboardShortcuts.md](readme/KeyboardShortcuts.md)
🗂️ **MiniPath 快捷键：** [readme/minipath/KeyboardShortcuts.md](readme/minipath/KeyboardShortcuts.md)

## 常见问题

继承自 Notepad2 / Notepad2-mod 的常见问答，并针对 Notepad3 做了更新 —— 安装、便携设置、正则（PCRE2）、编码、文件变更通知、`.LOG` 与 Emacs 文件变量、替换 Windows 记事本等。

❓ **常见问题：** [readme/faq/FAQ.md](readme/faq/FAQ.md)

## 参与贡献

欢迎参与贡献，可以通过以下方式帮忙：

1. **报告问题** —— [提交 Issue](https://github.com/rizonesoft/Notepad3/issues)，并附上复现步骤
2. **提交 Pull Request** —— Fork 仓库、创建功能分支，然后向 `master` 提交 PR
3. **参与翻译** —— 在 `language\` 目录中添加或改进翻译
4. **推广项目** —— 给仓库点 Star 并分享给他人

需要支持请访问 [rizonesoft.com/contact-us](https://rizonesoft.com/contact-us)。

## 致谢

Notepad3 建立在以下项目的工作之上：

| 项目 | 作者 | 作用 |
|------|------|------|
| [Notepad2](https://www.flos-freeware.ch/notepad2.html) | Florian Balmer | 原始编辑器 |
| [Notepad2-mod](https://xhmikosr.github.io/notepad2-mod/) | XhmikosR | 扩展分支 |
| [Scintilla](https://www.scintilla.org/) 5.5.8 | Neil Hodgson | 编辑组件 |
| [Lexilla](https://www.scintilla.org/Lexilla.html) 5.4.6 | Neil Hodgson | 语法高亮 |
| [PCRE2](https://github.com/PCRE2Project/pcre2) 10.47 | PCRE2 Project | 正则引擎 |
| [TinyExpr++](https://blake-madden.github.io/tinyexpr-plusplus/) | Blake Madden | 表达式求值 |
| [uchardet](https://www.freedesktop.org/wiki/Software/uchardet/) | Mozilla / freedesktop | 编码检测 |
| Fugue Icons | Yusuke Kamiyamane | 工具栏图标 |

## 评测

- [Notepad3 是一款支持多种编程语言的高级文本编辑器](https://www.ghacks.net/2020/08/11/notepad3-is-an-advanced-text-editor-that-supports-many-programming-languages/) —— gHacks
- [Nsane Forums 上的 Notepad3 评测](https://www.nsaneforums.com/topic/382910-guidereview-notepad3-is-an-advanced-text-editor-that-supports-many-programming-languages/) —— Nsane Forums

## 许可证

Notepad3 采用 [BSD 3-Clause 许可证](Build/Docs/License.txt)。

Copyright © 2008–2026 [Rizonesoft](https://rizonesoft.com)。保留所有权利。

---

[![Sponsor](https://img.shields.io/badge/Sponsor-%E2%9D%A4-ea4aaa?style=flat-square&logo=github)](https://github.com/sponsors/rizonesoft)
[![Donate](https://img.shields.io/badge/Donate-PayPal-blue.svg?style=flat-square&logo=paypal)](https://www.paypal.com/donate/?hosted_button_id=7UGGCSDUZJPFE)
