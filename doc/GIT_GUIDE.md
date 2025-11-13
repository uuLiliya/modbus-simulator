# Git 命令参考指南

本文档提供了Git版本控制系统的常用命令和工作流程的快速参考。

## 目录

1. [基础配置命令](#基础配置命令)
2. [初始化和克隆](#初始化和克隆)
3. [查看状态和日志](#查看状态和日志)
4. [提交工作流](#提交工作流)
5. [分支管理](#分支管理)
6. [远程仓库操作](#远程仓库操作)
7. [撤销和恢复](#撤销和恢复)
8. [标签管理](#标签管理)
9. [其他常用命令](#其他常用命令)
10. [工作流示例](#工作流示例)

---

## 基础配置命令

配置Git用户信息和基本设置。

### 配置用户名和邮箱

```bash
# 设置全局用户名
git config --global user.name "Your Name"

# 设置全局邮箱
git config --global user.email "your.email@example.com"

# 为当前仓库设置用户名（会覆盖全局设置）
git config user.name "Your Name"

# 为当前仓库设置邮箱
git config user.email "your.email@example.com"
```

### 查看配置

```bash
# 查看全局配置
git config --global --list

# 查看当前仓库配置
git config --list

# 查看特定配置项
git config user.name
```

### 配置文本编辑器

```bash
# 设置全局默认编辑器（以nano为例）
git config --global core.editor "nano"

# 设置为vim
git config --global core.editor "vim"

# 设置为VS Code
git config --global core.editor "code --wait"
```

---

## 初始化和克隆

创建新的Git仓库或克隆现有仓库。

### 初始化新仓库

```bash
# 在当前目录初始化Git仓库
git init

# 初始化并创建新目录
git init my-project
cd my-project
```

### 克隆现有仓库

```bash
# 克隆仓库到本地
git clone https://github.com/user/repository.git

# 克隆到指定目录
git clone https://github.com/user/repository.git my-directory

# 克隆特定分支
git clone -b branch-name https://github.com/user/repository.git

# 浅克隆（只获取最近的提交历史）
git clone --depth 1 https://github.com/user/repository.git
```

---

## 查看状态和日志

检查工作区状态和提交历史。

### 查看状态

```bash
# 查看当前工作目录的状态
git status

# 简短状态输出
git status -s
```

### 查看提交日志

```bash
# 查看提交日志（完整信息）
git log

# 查看最近N条提交
git log -n 5

# 单行显示日志
git log --oneline

# 图形化显示分支历史
git log --graph --oneline --all

# 显示每次提交修改的文件
git log --name-status

# 显示提交详细变化
git log -p

# 按作者过滤
git log --author="Author Name"

# 按日期范围过滤
git log --since="2024-01-01" --until="2024-12-31"

# 显示特定文件的历史
git log -- path/to/file.txt
```

### 查看差异

```bash
# 查看未暂存的文件变化
git diff

# 查看已暂存的文件变化
git diff --cached

# 比较两个提交之间的差异
git diff commit1 commit2

# 查看特定文件在两个提交间的差异
git diff commit1 commit2 -- file.txt

# 显示统计信息
git diff --stat
```

### 查看特定提交

```bash
# 查看某个提交的详细信息
git show commit-hash

# 查看某个提交修改的特定文件
git show commit-hash:path/to/file
```

---

## 提交工作流

日常的暂存、提交和推送工作流。

### 查看工作区状态

```bash
# 查看未跟踪的文件
git status
```

### 暂存文件

```bash
# 暂存特定文件
git add file.txt

# 暂存多个文件
git add file1.txt file2.txt

# 暂存当前目录所有文件
git add .

# 暂存所有已跟踪文件的修改
git add -u

# 暂存所有文件（包括未跟踪）
git add -A

# 交互式暂存（选择要暂存的部分）
git add -p
```

### 查看暂存内容

```bash
# 查看暂存但未提交的更改
git diff --cached

# 查看已暂存文件列表
git diff --cached --name-only
```

### 提交更改

```bash
# 提交暂存的文件
git commit -m "Commit message"

# 提交并包含详细说明
git commit -m "Title" -m "Detailed description"

# 暂存所有已跟踪文件并提交（跳过git add）
git commit -a -m "Commit message"

# 修改最后一次提交（未推送前）
git commit --amend -m "New message"

# 修改最后一次提交但保持提交消息不变
git commit --amend --no-edit
```

### 推送到远程

```bash
# 推送当前分支到远程
git push

# 推送到指定远程的指定分支
git push origin branch-name

# 推送所有分支
git push -u origin --all

# 推送所有标签
git push --tags

# 强制推送（谨慎使用，可能覆盖他人工作）
git push -f

# 删除远程分支
git push origin --delete branch-name

# 推送标签到远程
git push origin tag-name
```

---

## 分支管理

创建、切换和管理分支。

### 列出分支

```bash
# 列出本地分支
git branch

# 列出所有分支（包括远程）
git branch -a

# 列出远程分支
git branch -r

# 显示每个分支最后一次提交
git branch -v
```

### 创建分支

```bash
# 基于当前分支创建新分支
git branch new-branch

# 基于特定提交创建新分支
git branch new-branch commit-hash

# 创建并切换到新分支
git checkout -b new-branch

# 使用switch创建并切换分支（推荐，Git 2.23+）
git switch -c new-branch
```

### 切换分支

```bash
# 切换到指定分支
git checkout branch-name

# 使用switch命令切换分支（推荐）
git switch branch-name

# 切换到上一个分支
git checkout -
```

### 重命名分支

```bash
# 重命名当前分支
git branch -m new-name

# 重命名指定分支
git branch -m old-name new-name
```

### 删除分支

```bash
# 删除本地分支（已合并）
git branch -d branch-name

# 强制删除本地分支（未合并）
git branch -D branch-name

# 删除远程分支
git push origin --delete branch-name
```

### 合并分支

```bash
# 将指定分支合并到当前分支
git merge branch-name

# 创建合并提交（即使是快进合并）
git merge --no-ff branch-name

# 取消合并
git merge --abort
```

### 变基分支

```bash
# 将当前分支变基到指定分支
git rebase branch-name

# 交互式变基（编辑提交）
git rebase -i HEAD~3

# 取消变基
git rebase --abort

# 继续变基（解决冲突后）
git rebase --continue
```

---

## 远程仓库操作

管理远程仓库连接和同步。

### 配置远程仓库

```bash
# 列出所有远程仓库
git remote

# 显示远程仓库详细信息
git remote -v

# 添加新的远程仓库
git remote add origin https://github.com/user/repository.git

# 修改远程仓库URL
git remote set-url origin https://github.com/user/new-repository.git

# 移除远程仓库
git remote remove origin

# 重命名远程仓库
git remote rename origin upstream
```

### 获取和拉取

```bash
# 从远程仓库获取更新（不合并）
git fetch

# 从指定远程获取更新
git fetch origin

# 获取所有远程分支
git fetch --all

# 拉取远程更新并合并到当前分支
git pull

# 拉取并变基到远程
git pull --rebase
```

### 查看远程跟踪分支

```bash
# 查看与远程的关系
git branch -u origin/branch-name

# 查看远程跟踪分支
git branch -vv
```

---

## 撤销和恢复

撤销更改和恢复文件。

### 撤销工作区修改

```bash
# 放弃对某文件的修改
git checkout -- file.txt

# 放弃所有修改（谨慎使用）
git checkout -- .

# 使用restore命令（推荐，Git 2.23+）
git restore file.txt

# 恢复所有文件
git restore .
```

### 撤销暂存

```bash
# 取消暂存特定文件
git reset HEAD file.txt

# 取消暂存所有文件
git reset HEAD

# 使用restore命令
git restore --staged file.txt
```

### 撤销提交

```bash
# 撤销最后一次提交，保留修改
git reset --soft HEAD~1

# 撤销最后一次提交，取消暂存，保留修改
git reset --mixed HEAD~1

# 撤销最后一次提交，丢弃所有修改
git reset --hard HEAD~1

# 撤销指定提交
git reset --hard commit-hash
```

### 恢复已删除的提交

```bash
# 查看所有引用日志
git reflog

# 恢复到指定的reflog条目
git reset --hard HEAD@{n}
```

### 使用revert撤销

```bash
# 创建新提交以撤销指定提交（推荐用于已推送的提交）
git revert commit-hash

# 撤销最后一次提交
git revert HEAD

# 自动完成revert（不打开编辑器）
git revert -n commit-hash
```

### 清理工作区

```bash
# 删除未跟踪的文件
git clean -f

# 删除未跟踪的文件和目录
git clean -fd

# 预览删除内容（不实际删除）
git clean -fdn

# 包括忽略文件
git clean -fdx
```

---

## 标签管理

标记重要的提交版本。

### 创建标签

```bash
# 创建轻量标签
git tag v1.0.0

# 创建注解标签（推荐）
git tag -a v1.0.0 -m "Release version 1.0.0"

# 为特定提交创建标签
git tag -a v1.0.0 -m "Release" commit-hash

# 签名标签（需要GPG）
git tag -s v1.0.0 -m "Signed release"
```

### 查看标签

```bash
# 列出所有标签
git tag

# 搜索标签
git tag -l "v1.*"

# 查看标签详细信息
git show v1.0.0

# 显示标签指向的提交
git tag -l --format='%(refname:short) %(objectname:short)'
```

### 删除标签

```bash
# 删除本地标签
git tag -d v1.0.0

# 删除远程标签
git push origin --delete v1.0.0

# 推送前删除远程标签
git push origin :v1.0.0
```

### 推送标签

```bash
# 推送特定标签
git push origin v1.0.0

# 推送所有标签
git push --tags

# 推送到指定远程
git push origin --tags
```

---

## 其他常用命令

### 搜索和查找

```bash
# 在代码中搜索字符串
git grep "search term"

# 搜索特定文件中的模式
git grep "pattern" -- "*.js"

# 查找删除某行代码的提交
git log -S "code snippet" -p

# 在提交消息中搜索
git log --grep="keyword"

# 按作者搜索
git log --author="author name"
```

### 文件历史

```bash
# 查看文件在不同提交间的变化
git diff commit1 commit2 -- file.txt

# 查看文件何时被删除
git log --diff-filter=D -- path/to/file

# 查看文件重命名历史
git log --follow -- path/to/file
```

### 提交统计

```bash
# 查看提交统计
git log --stat

# 查看贡献者统计
git shortlog -sn

# 统计代码行数变化
git diff --shortstat commit1 commit2

# 统计作者贡献
git log --numstat --pretty="%H" | awk '{a[$NF]++} END {for(i in a) print i, a[i]}'
```

### 其他有用命令

```bash
# 显示当前HEAD指向的提交
git rev-parse HEAD

# 显示当前分支名
git branch --show-current

# 显示仓库大小
git count-objects -v

# 检查仓库完整性
git fsck --full

# 查看配置中的所有别名
git config --get-regexp alias

# 创建命令别名
git config --global alias.co "checkout"
git config --global alias.st "status"
git config --global alias.br "branch"
git config --global alias.ci "commit"
```

---

## 工作流示例

### 常见开发工作流

#### 1. 新功能开发流程

```bash
# 1. 创建并切换到功能分支
git checkout -b feature/new-feature

# 2. 进行开发工作，周期性提交
git add .
git commit -m "实现新功能的第一部分"

git add .
git commit -m "实现新功能的第二部分"

# 3. 推送到远程
git push -u origin feature/new-feature

# 4. 完成后，创建合并请求或合并到主分支
git checkout main
git pull origin main
git merge feature/new-feature

# 5. 推送到远程
git push origin main

# 6. 删除功能分支
git branch -d feature/new-feature
git push origin --delete feature/new-feature
```

#### 2. 快速修复流程

```bash
# 1. 从主分支创建修复分支
git checkout main
git pull origin main
git checkout -b hotfix/critical-bug

# 2. 修复问题并提交
git add .
git commit -m "修复关键bug"

# 3. 合并到主分支
git checkout main
git merge hotfix/critical-bug

# 4. 也合并到开发分支
git checkout develop
git merge hotfix/critical-bug

# 5. 清理修复分支
git branch -d hotfix/critical-bug
```

#### 3. 发布版本流程

```bash
# 1. 创建发布分支
git checkout -b release/v1.0.0

# 2. 更新版本号
# 编辑版本文件...

git add .
git commit -m "Bump version to 1.0.0"

# 3. 创建标签
git tag -a v1.0.0 -m "Release version 1.0.0"

# 4. 合并到主分支
git checkout main
git merge release/v1.0.0

# 5. 推送
git push origin main
git push origin v1.0.0

# 6. 清理发布分支
git branch -d release/v1.0.0
```

#### 4. 提交前检查清单

```bash
# 1. 检查状态
git status

# 2. 查看修改内容
git diff

# 3. 暂存文件
git add .

# 4. 查看即将提交的内容
git diff --cached

# 5. 提交
git commit -m "Clear and descriptive commit message"

# 6. 推送前检查
git log -1

# 7. 推送
git push
```

#### 5. 合并冲突解决

```bash
# 1. 当发生冲突时
git status  # 查看冲突文件

# 2. 打开冲突文件进行编辑（编辑器中手动解决）
# 查找冲突标记：<<<<<<<, =======, >>>>>>>

# 3. 编辑完成后，暂存文件
git add conflicted-file.txt

# 4. 完成合并
git commit -m "Resolve merge conflict"

# 或使用工具解决冲突
git mergetool
```

#### 6. 同步上游仓库

```bash
# 1. 添加上游仓库（fork的项目）
git remote add upstream https://github.com/original-owner/repository.git

# 2. 获取上游更新
git fetch upstream

# 3. 切换到主分支
git checkout main

# 4. 变基上游主分支
git rebase upstream/main

# 5. 更新本地仓库
git push origin main -f

# 或使用合并
git merge upstream/main
git push origin main
```

---

## 提示和最佳实践

### 提交消息最佳实践

```bash
# ❌ 不好的提交消息
git commit -m "Fix stuff"

# ✅ 好的提交消息
git commit -m "Fix login button alignment on mobile devices"

# ✅ 更好的提交消息
git commit -m "Fix login button alignment on mobile devices

- Adjusted padding on smaller screens
- Updated media query breakpoint to 768px
- Tested on iPhone 12 and iPad mini"
```

### 常用别名设置

```bash
# 添加常用别名使工作更快捷
git config --global alias.co "checkout"
git config --global alias.br "branch"
git config --global alias.ci "commit"
git config --global alias.st "status"
git config --global alias.unstage "reset HEAD --"
git config --global alias.last "log -1 HEAD"
git config --global alias.visual "log --graph --oneline --all"
```

### 防止常见错误

```bash
# 避免合并错误
git pull --rebase  # 或设置为默认

# 避免推送到错误的分支
git push origin branch-name  # 显式指定

# 避免丢失提交
git reflog  # 查看所有操作记录

# 保护重要分支
git config branch.main.pushRemote origin
```

---

## 参考资源

- [Git官方文档](https://git-scm.com/doc)
- [GitHub帮助文档](https://docs.github.com/en)
- [Atlassian Git教程](https://www.atlassian.com/git/tutorials)
- [Git Cheat Sheet](https://github.github.com/training-kit/downloads/github-git-cheat-sheet/)

---

**最后更新**: 2024年

*本指南定期更新，欢迎提交改进建议*
