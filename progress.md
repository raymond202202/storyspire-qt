# storyspire-qt 迭代进度

> 目标：把 Electron 版 StorySpire（写作专家）重写为 Qt 版（轻量写作工具）
> 参考源码：`~/hermes-projects/storyspire`（Electron 版，功能对齐源）
> **数据兼容**：读写 `~/.config/storyspire-data/books.json`（与 Electron 版同路径同格式，切换版本数据不丢）
> 规则：每轮实现后 cmake 构建验证 + git commit（本地，**禁止 git push**）；每轮结束更新本文件

## 阶段清单（按序迭代）

- [x] **阶段 0 骨架**：CMake + 主窗口（左书/章树 + 中编辑器）+ BookTree（读 books.json 多书/章节树 + 新建书/章）+ Editor（标题 + QTextEdit + 字数）+ 数据兼容（~/.config/storyspire-data/books.json，构建通过，二进制 135KB）
- [x] **阶段 1 编辑保存**：Editor 编辑内容实时写回 books.json（500ms 防抖写回 + 30s 兜底自动保存 + 切换章节时保存当前）；章节重命名（右键菜单/标题栏编辑，trim 后写盘）+ 删除（确认后移入回收站 trash，格式对齐 Electron TrashItem）
- [x] **阶段 2 灵感库**：右侧灵感面板（读 inspirations.json，纯数组格式与 Electron 兼容；搜索/分类筛选/新建/编辑/删除；新增置顶）
- [x] **阶段 3 AI 面板**：接 flare server（spawn node 子进程 'flare server'，JSON Lines 协议；写作专家 profile 生成于 ~/.config/storyspire-qt/story-expert.json；key 走 env/~/.storyspire/.env，flare 自读 ~/.flare/.env 兜底）；快捷动作 续写/润色/扩写/灵感创作；story_* 5 工具宿主代理执行；写回前确认（应用一次/本次会话/始终/拒绝，始终决策持久化 confirm.json）
- [x] **阶段 4 打磨**：浅色主题白底紫配 #6d4aff（全局 QSS）/ 状态栏保存状态（章节 + 保存状态同步）/ 字体设置（QFontDialog + QSettings 持久化）/ 菜单与快捷键（文件/编辑/帮助，Ctrl+S 保存、Ctrl+N 新建章节、F2 重命名）

## 迭代记录

| 轮次 | 时间 | 完成 | 构建 | 备注 |
|------|------|------|------|------|
| 0 | 22:16 | 骨架 | ✅ | 数据兼容 books.json |
| 1 | 23:54 | 编辑保存 | ✅ | 实时写回+自动保存+重命名/删除(回收站)；测试 harness 验证写回/trim/trash 保留 |
| 2 | 00:00 | 灵感库 | ✅ | 右侧灵感面板，兼容 inspirations.json；冒烟测试两数据文件 md5 不变 |
| 3 | 00:25 | AI 面板 | ✅ | flare server 集成；手动协议测试(握手+真实chat)通过；主程序 spawn 子进程正常；数据文件 md5 不变 |
| 4 | 00:35 | 打磨 | ✅ | 浅色主题#6d4aff/状态栏/字体/菜单快捷键；构建 0 错误；冒烟 md5 不变 |

## 完成状态

✅ **全部 4 个阶段完成**：骨架 → 编辑保存 → 灵感库 → AI 面板 → 打磨。
功能对齐 Electron 版 StorySpire（轻量版）：数据兼容 books.json / inspirations.json（同路径同格式，切换版本数据不丢）。
已知边界：章节 content 存纯文本（Electron HTML 显示为纯文本，不做富文本迁移）；AI 写回章节经确认后直接落盘，若写回的是当前编辑章节会刷新编辑器（用户未保存的输入将被 AI 内容替换，与 Electron 行为一致）。

## 构建命令

```bash
cd ~/hermes-projects/storyspire-qt
cmake -B build && cmake --build build -j$(nproc)
./build/storyspire-qt
```

## 铁律

- **禁止 git push**（用户明早验收后才决定是否推 GitHub）
- 不动其他仓库（storyspire/pulse/flare/json-viewer 只读参考）
- 每轮必须构建通过才 commit；构建失败修复后继续
- UI 浅色主题白底紫配 #6d4aff
- **不要做富文本/HTML 迁移**（QTextEdit 纯文本即可；content 字段存纯文本，Electron 版 HTML 内容显示为纯文本可接受）——复杂功能留给用户验收后交互式做
