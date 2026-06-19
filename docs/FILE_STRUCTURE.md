# transTags 文件结构说明

此目录是准备上传 GitHub 的开源项目目录。项目名为 `transTags`，按系统拆分为 `transTags_windows` 和 `transTags_linux`。

## 总体结构

```text
transTags/
|-- README.md
|-- README_CN.md
|-- .gitignore
|-- .github/
|   `-- workflows/
|       `-- release.yml
|-- docs/
|   |-- assets/
|   |   `-- transTags_demo.png
|   |-- FILE_STRUCTURE.md
|   `-- GITHUB_UPLOAD.md
|-- transTags_windows/
|   |-- source/
|   |   |-- transTags_windows.sln
|   |   `-- transTags_windows/
|   |       |-- transTags_windows.cpp
|   |       |-- sqlite/
|   |       |   |-- sqlite3.c
|   |       |   `-- sqlite3.h
|   |       |-- transTags_windows.vcxproj
|   |       |-- transTags_windows.vcxproj.filters
|   |       |-- transTags_windows.rc
|   |       |-- transTags_windows.ico
|   |       |-- resource.h
|   |       `-- config.ini
|   `-- release/
|       |-- .gitkeep
|       |-- transTags_windows.exe
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
|   |-- release/
|   |   |-- .gitkeep
|   |   `-- transTags_linux.zip
|   `-- README_LINUX.md
```

`docs/assets/transTags_demo.png` 是 README 和 Release 页面使用的演示截图。

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

- `transTags_windows.cpp` 包含 Windows 版热键、透明度、鼠标穿透、置顶、居中、提示、便签窗口和便签管理查询逻辑。
- `sqlite/sqlite3.c`、`sqlite/sqlite3.h` 是 Windows 便签功能使用的 SQLite 单文件源码。
- `transTags_windows.vcxproj` 是 Visual Studio C++ 工程文件。
- `transTags_windows.rc`、`transTags_windows.ico`、`resource.h` 是资源文件。

Windows 版原理：通过 Win32 全局热键接收操作命令，使用窗口句柄修改透明度和扩展样式。鼠标穿透依赖 `WS_EX_TRANSPARENT`，窗口置顶依赖 topmost 窗口状态。
便签功能在本程序内创建独立 Win32 窗口，使用当前目录下的 `transTags_notes.sqlite` 持久化标题、正文、位置、大小和置顶状态，管理窗口通过 SQLite 查询标题和正文。

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
- `main.cpp` 包含 Linux 版热键、透明度、鼠标穿透、置顶、居中、提示、便签窗口和便签管理查询逻辑。
- Linux 窗口控制依赖 X11/XFixes/EWMH，完整功能需要 `Ubuntu on Xorg`。
Linux Qt 版原理：Qt 负责界面和 QtSql 便签数据库访问，X11 负责全局热键和目标窗口查找，XFixes/XShape 负责鼠标穿透，EWMH 属性负责透明度和窗口置顶。便签数据保存在本地 `transTags_notes.sqlite`。

## Release 下载入口

GitHub Release 会自动上传以下文件，README 中也提供了直接下载链接：

```text
transTags_windows/release/transTags_windows.exe
transTags_windows/release/transTags_windows.zip
transTags_linux/release/transTags_linux.zip
```

## GitHub 说明

源码仓库建议提交除 `release` 二进制以外的全部文件。`release` 目录中的 exe/zip 文件已经被 `.gitignore` 忽略，建议作为 GitHub Release 附件上传。
