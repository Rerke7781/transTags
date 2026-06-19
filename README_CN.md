# transTags 中文说明

[ [English](README.md) | 中文 ]

transTags 是一个轻量级窗口辅助工具，用热键控制目标窗口的透明度、鼠标穿透、窗口置顶和窗口居中。

项目按系统拆分：

- `transTags_windows`：Windows 版本，使用 Win32 API 实现。
- `transTags_linux`：Ubuntu/Linux 版本，主版本使用 C++/Qt Widgets + X11/XFixes/EWMH 实现。

## 下载

从最新 Release 选择对应系统的文件：

| 系统 | 直接下载 | 说明 |
| --- | --- | --- |
| Windows | [transTags_windows.exe](https://github.com/Rerke7781/transTags/releases/latest/download/transTags_windows.exe) | 单个可执行文件，下载后直接运行。 |
| Windows 整包 | [transTags_windows.zip](https://github.com/Rerke7781/transTags/releases/latest/download/transTags_windows.zip) | 包含 `transTags_windows.exe` 和默认 `config.ini`。 |
| Ubuntu/Linux | [transTags_linux.zip](https://github.com/Rerke7781/transTags/releases/latest/download/transTags_linux.zip) | Qt 版本源码包，需要在 `Ubuntu on Xorg` 下构建运行。 |

## 主要功能

- 调整当前窗口透明度。
- 开启或解除鼠标穿透，让鼠标点击落到下方窗口。
- 锁定或取消窗口置顶。
- 将窗口移动到屏幕中央。
- 每次操作后在右下角显示短提示。

## 原理简述

transTags 不向其他软件注入代码，而是调用系统提供的窗口控制接口。

- Windows 版使用 Win32 API 注册全局热键，获取目标窗口句柄，修改分层窗口透明度，通过 `WS_EX_TRANSPARENT` 实现鼠标穿透，并用置顶窗口标志实现锁定到最上层。
- Ubuntu/Linux 版用 Qt Widgets 实现托盘和设置窗口，底层通过 X11/XFixes/EWMH 修改窗口透明度、输入区域、活动窗口和置顶状态。完整功能需要在 `Ubuntu on Xorg` 会话下运行。

## 默认快捷键

| 快捷键 | 功能 |
| --- | --- |
| `Alt + ←` | 提高透明度，并自动开启鼠标穿透。 |
| `Alt + →` | 降低透明度。 |
| `Alt + ↑` | 切换鼠标穿透；已有穿透窗口时解除，没有时给当前目标窗口加穿透。 |
| `Alt + ↓` | 锁定/取消窗口置顶；优先作用于最近穿透窗口。 |
| `Ctrl + 小键盘 5` | 窗口居中。 |

## 目录结构

```text
transTags/
|-- transTags_windows/
|   |-- source/       Windows 源码和 Visual Studio 工程
|   `-- release/      Windows 本地发布文件
|-- transTags_linux/
|   |-- qt/           Ubuntu Qt 主版本
|   `-- release/      Linux 本地发布文件
|-- docs/             结构说明和 GitHub 上传说明
|-- README.md         英文说明
`-- README_CN.md      中文说明
```

更详细的文件说明见 `docs/FILE_STRUCTURE.md`。

## Windows 构建

使用 Visual Studio 打开：

```text
transTags_windows/source/transTags_windows.sln
```

选择 `Release` 配置后生成解决方案。

## Ubuntu/Linux 构建

推荐使用 Qt 版本：

```bash
cd transTags_linux/qt
chmod +x install_qt_deps_ubuntu.sh build.sh run.sh install_desktop_entry.sh
./install_qt_deps_ubuntu.sh
./build.sh
./install_desktop_entry.sh
./run.sh
```

Linux 完整功能需要在 `Ubuntu on Xorg` 会话中运行。Ubuntu 默认 Wayland 会限制普通 Qt 程序控制其他原生窗口，因此鼠标穿透、全局热键等功能可能无法完整工作。

## 发布包

本地发布包位置：

```text
transTags_windows/release/transTags_windows.exe
transTags_windows/release/transTags_windows.zip
transTags_linux/release/transTags_linux.zip
```

这些文件不提交到源码仓库，建议上传到 GitHub Releases。Windows 版只有一个可执行文件 `transTags_windows.exe`，zip 包只是把这个 exe 和默认配置文件打包在一起。

## 开源协议

本项目使用 MIT License，详见根目录 `LICENSE`。
