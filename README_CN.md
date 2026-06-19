# transTags 中文说明

transTags 是一个轻量级窗口辅助工具，用热键控制目标窗口的透明度、鼠标穿透、窗口置顶和窗口居中。

项目按系统拆分：

- `transTags_windows`：Windows 版本，使用 Win32 API 实现。
- `transTags_linux`：Ubuntu/Linux 版本，主版本使用 C++/Qt Widgets + X11/XFixes/EWMH 实现。

## 主要功能

- 调整当前窗口透明度。
- 开启或解除鼠标穿透，让鼠标点击落到下方窗口。
- 锁定或取消窗口置顶。
- 将窗口移动到屏幕中央。
- 每次操作后在右下角显示短提示。

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
|   |-- legacy/       早期实验版本，仅作参考
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
transTags_windows/release/transTags_windows.zip
transTags_linux/release/transTags_linux.zip
```

这些文件不提交到源码仓库，建议上传到 GitHub Releases。

## 开源协议

本项目使用 MIT License，详见根目录 `LICENSE`。
