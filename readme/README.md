# Notepad3 文档

本目录收录 Notepad3 及其配套文件浏览器 **MiniPath** 面向用户的参考文档。这里的页面涵盖最终用户关心的主题 —— 配置、命令行开关、快捷键、文件处理、加密、主题与常见问题 —— 并由项目主自述文件和应用内「帮助」菜单链接过来。

> 需要构建说明或贡献者指南？请查看仓库根目录下的项目[自述文件](../Readme.md)和 [CLAUDE.md](../CLAUDE.md) 开发者指南。安装包制作与替换 Windows 记事本的设置方法，参见[项目官网](https://rizonesoft.com/documents/notepad3/)。

## 目录

### 快速参考

- [KeyboardShortcuts.md](KeyboardShortcuts.md) —— Notepad3 编辑器的完整快捷键参考。
- [MenuEntriesAndCmds.md](MenuEntriesAndCmds.md) —— 菜单树与右键菜单中的全部命令。

### 配置与命令行

- [config/Configuration.md](config/Configuration.md) —— 便携配置文件 `Notepad3.ini` 的完整参考（`[Settings2]` 配置项、需重启生效的设置等）。
- [config/FileContentFlags.md](config/FileContentFlags.md) —— 写在文件内部的标记（模式行、BOM 等），用于改变 Notepad3 打开或着色该文件的方式。
- [cmdln/CmdLnOptions.md](cmdln/CmdLnOptions.md) —— `Notepad3.exe` 支持的全部命令行选项、开关与位置参数。

### 文件处理

- [paths/FilePathHandling.md](paths/FilePathHandling.md) —— Notepad3 如何把文本解析成文件：超链接、对话框、相对路径 / UNC / 长路径。
- [uchardet/EncodingDetection.md](uchardet/EncodingDetection.md) —— 编码自动检测（uchardet 与 BOM / 启发式规则的分层判断）。
- [encryption/Encryption.md](encryption/Encryption.md) —— 透明的 AES-256（Rijndael/CBC）加解密与 `np3encrypt` 命令行工具。

### 编辑器功能

- [focusedview/FocusedView.md](focusedview/FocusedView.md) —— 基于折叠的过滤视图，只显示与标记单词或选区匹配的行。
- [tinyexprcpp/TinyExprPP.md](tinyexprcpp/TinyExprPP.md) —— 内置的 TinyExpr++ 数学 / 表达式求值器，以及它在界面中的接入位置。
- [schema/CustomSchema.md](schema/CustomSchema.md) —— 配色方案、词法样式集、样式迷你语言与主题的导入导出。

### MiniPath

- [minipath/KeyboardShortcuts.md](minipath/KeyboardShortcuts.md) —— MiniPath 文件浏览器（用 `Ctrl+M` 启动）的快捷键参考。

### 常见问题

- [faq/FAQ.md](faq/FAQ.md) —— 常见问题，含从 Notepad2 / Notepad2-mod 迁移的说明。
