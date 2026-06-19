# 上传到 GitHub

下面以仓库名 `transTags` 为例。先在 GitHub 网页上创建一个空仓库，仓库名建议填写 `transTags`。

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
transTags_windows/release/transTags_windows_pin.exe
transTags_linux/release/transTags_linux.zip
```

网页操作：

1. 打开 GitHub 仓库页面。
2. 点击右侧或顶部的 `Releases`。
3. 点击 `Draft a new release`。
4. Tag 填 `v0.1.0`。
5. Title 填 `transTags v0.1.0`。
6. 把上面的 exe/zip 文件拖进去。
7. 点击 `Publish release`。

如果安装了 GitHub CLI，也可以用命令：

```powershell
cd "C:\Users\Sim\Desktop\嵌入式课程设计\transTags"
gh release create v0.1.0 `
  transTags_windows/release/transTags_windows.exe `
  transTags_windows/release/transTags_windows_pin.exe `
  transTags_linux/release/transTags_linux.zip `
  --title "transTags v0.1.0" `
  --notes "Initial open-source release."
```

## 开源协议

当前没有自动添加 `LICENSE` 文件。严格来说，没有许可证的公开仓库不等于别人可以自由使用代码。发布前建议选择一个许可证：

- MIT：简单宽松，适合大多数小工具。
- Apache-2.0：宽松，并包含专利授权条款。
- GPL-3.0：要求衍生作品继续开源。

选好后，在 GitHub 仓库页面点击 `Add file`，添加对应 `LICENSE` 文件即可。
