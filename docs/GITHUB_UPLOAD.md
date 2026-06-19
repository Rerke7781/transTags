# 上传到 GitHub

下面以仓库名 `transTags` 为例。先在 GitHub 网页上创建一个空仓库，仓库名建议填写 `transTags`。

## 项目原理简述

transTags 是一个通过系统窗口 API 控制其他窗口状态的小工具。Windows 版使用 Win32 API 修改窗口透明度、扩展样式和置顶状态；Linux Qt 版使用 Qt 做界面，用 X11/XFixes/EWMH 完成透明度、鼠标穿透、置顶和全局热键控制。

创建仓库时建议：

- 不勾选 `Add a README file`，因为本地已经有 `README.md`。
- 不勾选 `.gitignore`，因为本地已经有 `.gitignore`。
- License 可以先不选，或按你的开源意愿选择 MIT/Apache-2.0/GPL 等。

## 命令行上传源码

在 PowerShell 里执行：

```powershell
cd "C:\Users\Sim\Desktop\嵌入式课程设计\transTags"
git init
git branch -M main
git add .
git commit -m "Initial transTags release"
git remote add origin https://github.com/<你的GitHub用户名>/transTags.git
git push -u origin main
```

把 `<你的GitHub用户名>` 换成你的 GitHub 用户名。

如果这是你第一次在这台电脑使用 Git，`git commit` 可能要求配置用户名和邮箱：

```powershell
git config --global user.name "你的名字"
git config --global user.email "你的邮箱"
```

## 如果远程仓库不是空的

如果你创建仓库时已经添加了 README 或 LICENSE，首次推送可能提示历史不一致。可以先拉取再推送：

```powershell
cd "C:\Users\Sim\Desktop\嵌入式课程设计\transTags"
git pull origin main --allow-unrelated-histories
git push -u origin main
```

如果出现冲突，先解决冲突文件，再执行：

```powershell
git add .
git commit -m "Merge remote repository"
git push -u origin main
```

## 上传可执行文件和压缩包

源码推送完成后，发布文件建议放到 GitHub Releases。

本地发布文件位置：

```text
transTags_windows/release/transTags_windows.exe
transTags_windows/release/transTags_windows.zip
transTags_linux/release/transTags_linux.zip
```

仓库已经包含 `.github/workflows/release.yml`。推送 `v*` 标签后，GitHub Actions 会自动创建正式 GitHub Release，并把上面的 exe/zip 作为附件上传。README 的下载链接会指向最新 Release 附件。

网页操作：

1. 打开 GitHub 仓库页面。
2. 点击右侧或顶部的 `Releases`。
3. 点击 `Draft a new release`。
4. Tag 填当前版本号，例如 `v0.1.2`。
5. Title 填对应标题，例如 `transTags v0.1.2`。
6. 把上面的 exe/zip 文件拖进去。
7. 点击 `Publish release`。

如果安装了 GitHub CLI，也可以用命令：

```powershell
cd "C:\Users\Sim\Desktop\嵌入式课程设计\transTags"
gh release create v0.1.2 `
  transTags_windows/release/transTags_windows.exe `
  transTags_windows/release/transTags_windows.zip `
  transTags_linux/release/transTags_linux.zip `
  --title "transTags v0.1.2" `
  --notes "Initial open-source release."
```

## 开源协议

当前仓库已经使用 MIT License，许可证文件是根目录的 `LICENSE`。
