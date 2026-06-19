# transTags 文件结构说明

此目录是准备上传 GitHub 的开源项目目录。项目名为 `transTags`，按系统拆分为 `transTags_windows` 和 `transTags_linux`。

## 总体结构

```text
transTags/
|-- README.md
|-- README_CN.md
|-- .gitignore
|-- transTags_windows/
|   |-- source/
|   |   |-- transTags_windows.sln
|   |   `-- transTags_windows/
|   |       |-- transTags_windows.cpp
|   |       |-- transTags_windows.vcxproj
|   |       |-- transTags_windows.vcxproj.filters
|   |       |-- transTags_windows.rc
|   |       |-- transTags_windows.ico
|   |       |-- resource.h
|   |       `-- config.ini
|   `-- release/
|       |-- .gitkeep
|       |-- transTags_windows.exe
|       |-- transTags_windows_pin.exe
|       |-- config.ini
|       `-- transTags_windows.zip
|-- transTags_linux/
|   |-- qt/
|   |   |-- main.cpp
|   |   |-- CMakeLists.txt
|   |   |-- build.sh
|   |   |-- run.sh
|   |   |-- install_qt_deps_ubuntu.sh
|   |   |-- install_desktop_entry.sh
|   |   `-- README.md
|   |-- legacy/
|   |   |-- x11-c/
|   |   |   |-- transTags_linux.c
|   |   |   |-- Makefile
|   |   |   |-- install_deps_ubuntu.sh
|   |   |   `-- run.sh
|   |   `-- gnome-extension/
|   |       |-- extension.js
|   |       |-- metadata.json
|   |       |-- install_gnome_extension.sh
|   |       `-- schemas/
|   |-- release/
|   |   |-- .gitkeep
|   |   `-- transTags_linux.zip
|   `-- README_LINUX.md
`-- docs/
    |-- FILE_STRUCTURE.md
    `-- GITHUB_UPLOAD.md
```

## Windows 项目

主要源码：

```text
transTags_windows/source/transTags_windows/transTags_windows.cpp
```

工程入口：

```text
transTags_windows/source/transTags_windows.sln
```

本地发布文件：

```text
transTags_windows/release/
```

说明：

- `transTags_windows.cpp` 包含 Windows 版热键、透明度、鼠标穿透、置顶、居中和提示逻辑。
- `transTags_windows.vcxproj` 是 Visual Studio C++ 工程文件。
- `transTags_windows.rc`、`transTags_windows.ico`、`resource.h` 是资源文件。

## Linux 项目

当前主版本：

```text
transTags_linux/qt/
```

主要源码：

```text
transTags_linux/qt/main.cpp
```

构建入口：

```text
transTags_linux/qt/CMakeLists.txt
transTags_linux/qt/build.sh
```

说明：

- `qt/` 是当前推荐维护版本，界面由 C++/Qt Widgets 实现。
- Linux 窗口控制依赖 X11/XFixes/EWMH，完整功能需要 `Ubuntu on Xorg`。
- `legacy/x11-c/` 是早期 X11 C 版本，仅保留作参考。
- `legacy/gnome-extension/` 是早期 GNOME Shell 扩展方案，仅保留作参考。

## GitHub 说明

源码仓库建议提交除 `release` 二进制以外的全部文件。`release` 目录中的 exe/zip 文件已经被 `.gitignore` 忽略，建议作为 GitHub Release 附件上传。
