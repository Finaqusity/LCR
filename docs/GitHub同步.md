# GitHub 同步说明

> 仓库位置：`D:\0-Documents\电子设计\电赛校队招新_测量题`（本地已 `git init`，分支 **master**）。
> **2026-11-15 更新：远端已建好并完成首次推送 → https://github.com/Finaqusity/LCR （公开仓库）。** §1–§3 的“建远端/首次推送”流程保留作参考（换机器、重建仓库时用）；日常同步从 §4 开始，队友启动只需 `git clone https://github.com/Finaqusity/LCR.git`。
> 本文说明：远端创建与认证（§1–§3，已完成、备用）、首次推送、日常同步（§4 起）与冲突处理。

---

## 0. 先确认本地状态

```powershell
cd "D:\0-Documents\电子设计\电赛校队招新_测量题"
D:\Git\cmd\git.exe status
D:\Git\cmd\git.exe log --oneline -3
D:\Git\cmd\git.exe remote -v      # 现在应该是空的
```

工作区里有未提交的改动是正常的（新增文档、删除临时文件），下一步一起提交。

---

## 1. 在 GitHub 建一个空仓库

1. 打开 https://github.com/new
2. Repository name：例如 `lcr-meter`（或 `lcr-bridge-2026`）
3. 选 **Private**（比赛项目建议私有）或 Public 都行
4. **不要**勾选 "Add a README file"、".gitignore"、"license"（本地已有内容，勾了会冲突）
5. 点 Create repository，复制页面上的地址，形如：
   - HTTPS：`https://github.com/<你的用户名>/lcr-meter.git`
   - SSH：`git@github.com:<你的用户名>/lcr-meter.git`

---

## 2. 认证方式（二选一）

### 方式 A：HTTPS + Personal Access Token（最省事）

1. GitHub → Settings → Developer settings → **Personal access tokens → Tokens (classic)** → Generate new token
2. 勾选 `repo`（私有仓库必需），有效期选 90 天
3. 复制生成的 token（只显示一次）
4. 第一次 `git push` 时：
   - Username：你的 GitHub 用户名
   - Password：**粘贴 token**（不是账号密码）
5. Windows 会把它存进"凭据管理器"，之后不用再输。

> 常见错：`Support for password authentication was removed` —— 说明输的是账号密码，改输 token。

### 方式 B：SSH key

```powershell
ssh-keygen -t ed25519 -C "你的邮箱"      # 一路回车
Get-Content $env:USERPROFILE\.ssh\id_ed25519.pub   # 复制输出
```
GitHub → Settings → SSH and GPG keys → New SSH key → 粘贴 → 保存。
测试：`ssh -T git@github.com`，出现 `Hi <用户名>!` 即成功。
之后 remote 用 SSH 地址。

---

## 3. 首次推送

```powershell
cd "D:\0-Documents\电子设计\电赛校队招新_测量题"
$g = "D:\Git\cmd\git.exe"

& $g add -A
& $g commit -m "chore: 硬件定稿 + 接口/环境文档"
& $g remote add origin https://github.com/<你的用户名>/lcr-meter.git
& $g push -u origin master
```

> 分支名是 **master**（不是 main）。如果你希望用 main：
> ```powershell
> & $g branch -M main
> & $g push -u origin main
> ```
> 之后团队统一用同一个分支名即可。

推送成功后，刷新 GitHub 页面就能看到全部文件。

---

## 4. 日常同步（每个人每次开工/收工）

```powershell
$g = "D:\Git\cmd\git.exe"

# 开工：先把别人的改动拉下来（变基，保持历史干净）
& $g pull --rebase

# 干活……（编译、改代码）

# 收工：看改了什么，再提交
& $g status
& $g add -A
& $g commit -m "feat: ADC1/2 双同步 + TIM8 TRGO 触发"
& $g push
```

要点：
- **每次开工先 pull**，否则 push 会被拒（`rejected - non-fast-forward`）。
- 出现冲突：打开冲突文件改好后
  ```powershell
  & $g add <冲突文件>
  & $g rebase --continue
  ```
- 想撤销还没提交的改动：`& $g checkout -- <文件>`
- 想撤销最近一次提交（还没 push）：`& $g reset --soft HEAD~1`
- 改提交信息（还没 push）：`& $g commit --amend`

---

## 5. 提交什么、不提交什么

**已由 `.gitignore` 自动排除**（不用管）：
`firmware/build/`、`*.elf/hex/bin/map`、`build_pcb/img/`、`build_sch/pages|hwpages|sch/`、`__pycache__/`、`.vscode/`、`DeepSeek/`、`_ds/`、`*.log` 等。

**建议提交**：
- `firmware/` 源码（CMakeLists、Core、cmake/、链接脚本）
- `tools/` 脚本
- `docs/` 文档、根目录 `.docx/.pdf` 交付物
- `build_sch/*.py` 和 `build_sch/*.png`（原理图源与成品）
- `build_pcb/gerber/`（打样记录，很小）

**不要提交**：
- 编译产物、日志、临时 txt、下载的 PDF 手册（`_ds/`）、个人 IDE 配置
- 已经排除了，注意别用 `git add -f` 强加。

**大文件**：目前仓库约 25–30 MB（文档 + 图片 + HAL），GitHub 完全没问题。
如果以后要放 >50 MB 的东西（视频、大量裸数据），用 Git LFS：
```powershell
& $g lfs install
& $g lfs track "*.mp4" "*.bin"
& $g add .gitattributes
```

---

## 6. 三个人协作建议

1. **分支策略**：小项目建议"轻量分支"——每人一个长期分支（`dev-zhu`、`dev-fan`、`dev-jiang`），做完一个功能合回 `master`；或者干脆都在 `master` 上小步提交（每天至少 pull 一次）。
2. **谁改哪块先说清楚**：固件（`firmware/`）、硬件文档（`build_sch/`、`build_pcb/`）、文档（`docs/`）尽量不同时改同一文件。
3. **二进制文件（.docx/.pdf/.png）不能合并**：如果两个人同时改同一个 docx，Git 无法合并，只能二选一。约定：同一时间只有一个人改文档。
4. **提交信息**：`feat:` 新功能、`fix:` 修复、`docs:` 文档、`chore:` 杂项，一眼能看懂。
5. 每次打样/重要节点打 tag：
   ```powershell
   & $g tag -a v0.1-hw -m "硬件 v1 送样"
   & $g push --tags
   ```

---

## 7. 常见错误对照

| 报错 | 原因 | 解决 |
|---|---|---|
| `rejected ... non-fast-forward` | 远端有你没拉的提交 | `git pull --rebase` 后再 push |
| `Support for password authentication was removed` | 用了账号密码 | 改用 PAT（见 §2） |
| `fatal: remote origin already exists` | 已经加过 | `git remote set-url origin <新地址>` |
| `detached HEAD` | checkout 到了某个提交 | `git checkout master` 再继续 |
| 推送很慢/卡住 | 网络 | 重试；或换 SSH；或配置代理 |
| 误把 build 提交了 | 之前没 .gitignore | `git rm -r --cached firmware/build` 后提交 |
| 文件太大被拒（>100 MB） | 单文件超限 | 从历史里删掉并改 .gitignore，或用 LFS |

---

## 8. 给"新对话"的交接方式

新开一个对话做软件时，第一句可以直接写：

> 先读 `D:\0-Documents\电子设计\电赛校队招新_测量题\docs\硬件接口.md` 和 `docs\开发环境.md`，然后我们开始写固件。

仓库地址（推送成功后）也一并给出，方便换机器：
`https://github.com/<你的用户名>/lcr-meter`
