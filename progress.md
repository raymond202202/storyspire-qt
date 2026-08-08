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
- [x] **阶段 6 大纲系统**：右侧新增「📋 大纲」面板（独立于正文，对齐 Electron addOutline/updateOutline/renameOutline/deleteOutline）：新建（输入标题）/编辑（500ms 防抖写回 + 字数）/重命名/删除（移入 trash 带 _outline:true）；outlines 数组与 Electron 同格式；tests/outline_test.cpp 20 项单测全过；顺带修复阶段 0 遗留 bug：字数统计正则 \u4e00 对 QRegularExpression 无效（zh 恒为 0），改 \x{4e00} 后 Editor/BookTree/AiPanel/OutlinePanel 四处字数统计恢复正常
- [x] **阶段 5 导出**：文件→导出子菜单（对齐 Electron exporter.ts）：整书/单卷/单章/大纲 × TXT/DOCX/DOC(RTF)/PDF 四种格式；docx 为手写 OOXML zip（STORE，Word/WPS 可开，结构对齐 docx 分支）；pdf 走 QPdfWriter+QTextDocument（排版对齐 htmlForPdf）；stripHtml 兼容旧 HTML 数据、RTF 转义、文件名安全化、QSaveFile 原子写；tests/exporter_test.cpp 29 项单测全过
- [x] **阶段 4 打磨**：浅色主题白底紫配 #6d4aff（全局 QSS）/ 状态栏保存状态（章节 + 保存状态同步）/ 字体设置（QFontDialog + QSettings 持久化）/ 菜单与快捷键（文件/编辑/帮助，Ctrl+S 保存、Ctrl+N 新建章节、F2 重命名）
- [x] **阶段 7 手机预览**：右侧新增「📱 预览」面板（对齐 Electron PreviewPanel + PHONE_PRESETS）：6 预设机型（iPhone 17 Pro Max/17/16/14/Galaxy S24/Pixel 9）+ 自定义尺寸（可保存命名/删除，QSettings 持久化对齐 localStorage）；实时渲染当前章节/草稿（编辑中未保存内容也实时显示，草稿优先）；手机壳自绘（深色圆角外壳/灵动岛/状态栏时间/标题/正文/字数/Home 条），HTML 旧数据剥离为纯文本；Editor 新增 editingPreview 信号（每次输入即发）；tests/preview_test.cpp 单测通过

## 迭代记录

| 轮次 | 时间 | 完成 | 构建 | 备注 |
|------|------|------|------|------|
| 0 | 22:16 | 骨架 | ✅ | 数据兼容 books.json |
| 1 | 23:54 | 编辑保存 | ✅ | 实时写回+自动保存+重命名/删除(回收站)；测试 harness 验证写回/trim/trash 保留 |
| 2 | 00:00 | 灵感库 | ✅ | 右侧灵感面板，兼容 inspirations.json；冒烟测试两数据文件 md5 不变 |
| 3 | 00:25 | AI 面板 | ✅ | flare server 集成；手动协议测试(握手+真实chat)通过；主程序 spawn 子进程正常；数据文件 md5 不变 |
| 4 | 00:35 | 打磨 | ✅ | 浅色主题#6d4aff/状态栏/字体/菜单快捷键；构建 0 错误；冒烟 md5 不变 |
| 5 | 01:10 | 导出 | ✅ | 导出子菜单 txt/doc(RTF)/大纲，对齐 exporter.ts；单测 14 PASS；冒烟 md5 不变 |
| 6 | 01:30 | 大纲系统 | ✅ | 右侧📋大纲面板（新建/编辑/重命名/删除，trash 带_outline）；单测 20 PASS；修复字数正则bug(\\u→\\x{}) |
| 7 | 01:52 | 导出补全 | ✅ | 对齐 Electron 四格式：整书/卷/章/大纲 × TXT/DOCX/DOC/PDF；docx 手写 OOXML zip（修复中央目录缺 date 字段 bug）；pdf QPdfWriter 渲染；单测 29 PASS；真实数据 docx/pdf 冒烟通过；md5 不变 |
| 8 | 02:29 | 手机预览 | ✅ | 右侧📱预览面板：6预设机型+自定义尺寸(保存命名/删除,QSettings持久化)+实时渲染(编辑中未保存内容也显示,草稿优先)+手机壳自绘(灵动岛/状态栏时间/标题/正文/字数/Home条)；Editor 新增 editingPreview 信号(每次输入即发)；单测 preview_test PASS；冒烟 6s 无崩溃；md5 不变 |
| 9 | 03:08 | 全量回归验证 | ✅ | 无未完成阶段，本轮做验证：cmake 构建 0 错误；单测全过（Exporter/大纲 20 PASS/预览 PASS）；offscreen 冒烟 8s 无崩溃；books.json/inspirations.json md5 不变；工作区干净 |
| 10 | 03:40 | 全量回归验证 | ✅ | 无未完成阶段（7 阶段全部完成），本轮复查：cmake 构建 0 错误；单测全过（Exporter/大纲 18 PASS/预览 OK）；offscreen 冒烟 8s 无崩溃；books.json/inspirations.json md5 不变；git 工作区干净 |
| 11 | 次日 | 全量回归验证 | ✅ | 无未完成阶段，本轮复查：cmake 构建 0 错误；单测全过（Exporter/大纲 20 PASS/预览 OK）；offscreen 冒烟 8s 无崩溃；数据 md5 不变；深度校验 books.json 顶层键 books/trash、book 字段（id/title/author/volumes/chapters/createdAt/updatedAt/settings/outlines）、chapter 字段（id/title/volumeId/content/wordCount/createdAt/updatedAt）、outline 字段全部与 Electron src/types/index.ts 定义一致，无格式漂移；git 工作区干净 |

| 12 | 次日 | 全量回归验证 | ✅ | 无未完成阶段，本轮复查：cmake 构建 0 错误；单测全过（Exporter/大纲 20 PASS/预览 OK）；offscreen 冒烟 8s 无崩溃；数据 md5 不变（books.json ac6da96e / inspirations.json 18258e3c）；格式深度校验 PASS（顶层键 books/trash、book/chapter/volume/outline/trash 字段全部对齐 Electron src/types/index.ts，content 均为纯文本无 HTML）；顺带清理 progress.md 历史遗留 56 个 NUL 字节；git 工作区干净 |
| 13 | 次日 | 全量回归验证 | ✅ | 无未完成阶段，本轮复查：cmake 构建 0 错误；单测全过（Exporter/大纲 20 PASS/预览 OK）；offscreen 冒烟 14s 无崩溃；数据 md5 不变（books.json ac6da96e / inspirations.json 18258e3c）；格式深度校验 PASS（新增 volumeId 悬空引用交叉检查，books=1 volumes=1 chapters=2 outlines=0 trash=1 字段全对齐 Electron types，content 均纯文本无 HTML）；git 工作区干净 |
| 14 | 次日 | 全量回归验证 | ✅ | 无未完成阶段，本轮复查：cmake 构建 0 错误；单测全过（Exporter/大纲 20 PASS/预览 OK）；offscreen 冒烟 14s 无崩溃；数据 md5 不变（books.json ac6da96e / inspirations.json 18258e3c）；新增 scripts/validate_data.py 固化严格口径深度校验（空 volumeId 也视为悬空）→ **发现 1 条历史遗留脏数据**：章节 c537abd9（UUID 格式 id，Electron 早期版本创建，非 Qt 引入）volumeId='' 悬空，Qt 版树中可见可编辑不丢数据，未擅自修改用户数据，待用户决定是否修复（修复方式：把该章节 volumeId 指向 dddce641 书的唯一卷 0767ad43）；git 工作区干净 |
| 15 | 次日 | 全量回归验证 | ✅ | 无未完成阶段，本轮复查：cmake 构建 0 错误；单测全过（Exporter 30 PASS/大纲 20 PASS/预览 OK）；offscreen 冒烟 14s 无崩溃（exit=124）；数据 md5 不变（books.json ac6da96e / inspirations.json 18258e3c）；格式深度校验仅剩第14轮已记录的历史遗留脏数据 1 条（章节 c537abd9 volumeId='' 悬空，Electron 早期创建，Qt 版可见可编辑不丢数据，未擅自修改，仍待用户决定）；git 工作区干净 |

## 完成状态

✅ **全部 7 个阶段完成**：骨架 → 编辑保存 → 灵感库 → AI 面板 → 打磨 → 导出 → 大纲系统 → 手机预览。
功能对齐 Electron 版 StorySpire（轻量版）：数据兼容 books.json / inspirations.json（同路径同格式，切换版本数据不丢）；导出 txt/doc 对齐 exporter.ts（整书/单卷/单章/大纲）；大纲独立存储 book.outlines 可编辑可导出。
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
