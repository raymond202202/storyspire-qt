#include "AiPanel.h"
#include "FlareClient.h"
#include "BookTree.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextBrowser>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QDialog>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextBlockFormat>
#include <QFrame>
#include <QKeyEvent>

namespace {
// 对齐 flare src/tools/story.ts 的工具定义（宿主代理工具）
QJsonArray storyToolDefinitions() {
    const QByteArray raw = R"JSON([
  {"type":"function","function":{"name":"story_get_story","description":"获取当前故事的整体结构：标题、作者、卷、章节列表（含各章标题/字数）。写作前先了解故事背景与进度。无参数。","parameters":{"type":"object","properties":{},"required":[]}}},
  {"type":"function","function":{"name":"story_get_chapter","description":"获取某个章节的完整内容（标题 + 正文）。需要 chapterId。用于续写、润色、分析某章。","parameters":{"type":"object","properties":{"chapterId":{"type":"string","description":"章节 ID"}},"required":["chapterId"]}}},
  {"type":"function","function":{"name":"story_list_chapters","description":"列出当前故事所有章节（标题/字数/所属卷）。查看写作进度时用。无参数。","parameters":{"type":"object","properties":{},"required":[]}}},
  {"type":"function","function":{"name":"story_create_chapter","description":"创建新章节（AI 起草）。可指定标题；若给 content 则直接写入正文。","parameters":{"type":"object","properties":{"title":{"type":"string","description":"章节标题"},"content":{"type":"string","description":"可选：章节正文（AI 起草完成后写入）"}},"required":["title"]}}},
  {"type":"function","function":{"name":"story_update_chapter","description":"更新章节正文（润色、续写、修改后写回）。需要 chapterId 和 content。这是把 AI 生成内容写入故事的关键工具。","parameters":{"type":"object","properties":{"chapterId":{"type":"string","description":"章节 ID"},"content":{"type":"string","description":"新的章节正文（完整替换）"}},"required":["chapterId","content"]}}}
])JSON";
    return QJsonDocument::fromJson(raw).array();
}

QString escapeHtml(const QString &s) {
    return s.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"));
}
} // namespace

AiPanel::AiPanel(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // 标题行
    auto *head = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("✨ AI 助手（写作专家）"), this);
    head->addWidget(title);
    head->addStretch();
    m_status = new QLabel(QStringLiteral("启动中…"), this);
    m_status->setStyleSheet(QStringLiteral("color:#888;font-size:11px;"));
    head->addWidget(m_status);
    layout->addLayout(head);

    // 快捷动作
    auto *actions = new QHBoxLayout;
    auto *btnContinue = new QPushButton(QStringLiteral("✍️ 续写"), this);
    auto *btnPolish = new QPushButton(QStringLiteral("✨ 润色"), this);
    auto *btnExpand = new QPushButton(QStringLiteral("📖 扩写"), this);
    auto *btnInspiration = new QPushButton(QStringLiteral("💡 灵感创作"), this);
    actions->addWidget(btnContinue);
    actions->addWidget(btnPolish);
    actions->addWidget(btnExpand);
    actions->addWidget(btnInspiration);
    layout->addLayout(actions);
    connect(btnContinue, &QPushButton::clicked, this, &AiPanel::onQuickContinue);
    connect(btnPolish, &QPushButton::clicked, this, &AiPanel::onQuickPolish);
    connect(btnExpand, &QPushButton::clicked, this, &AiPanel::onQuickExpand);
    connect(btnInspiration, &QPushButton::clicked, this, &AiPanel::onQuickInspiration);

    // 消息区
    m_messages = new QTextBrowser(this);
    m_messages->setOpenExternalLinks(false);
    m_messages->setStyleSheet(QStringLiteral(
        "QTextBrowser{background:#ffffff;border:1px solid #e4e4ef;border-radius:6px;font-size:13px;}"));
    layout->addWidget(m_messages, 1);

    // 输入区
    auto *inputRow = new QHBoxLayout;
    m_input = new QTextEdit(this);
    m_input->setPlaceholderText(QStringLiteral("输入请求，Enter 发送，Shift+Enter 换行…"));
    m_input->setFixedHeight(64);
    inputRow->addWidget(m_input, 1);
    auto *btnCol = new QVBoxLayout;
    m_send = new QPushButton(QStringLiteral("发送"), this);
    m_stop = new QPushButton(QStringLiteral("停止"), this);
    m_stop->setEnabled(false);
    btnCol->addWidget(m_send);
    btnCol->addWidget(m_stop);
    inputRow->addLayout(btnCol);
    layout->addLayout(inputRow);

    connect(m_send, &QPushButton::clicked, this, &AiPanel::onSend);
    connect(m_stop, &QPushButton::clicked, this, &AiPanel::onStop);
    // Enter 发送 / Shift+Enter 换行
    m_input->installEventFilter(this);

    m_client = new FlareClient(this);
    connect(m_client, &FlareClient::eventReceived, this, &AiPanel::onEvent);
    connect(m_client, &FlareClient::serverFailed, this, &AiPanel::onServerFailed);
    connect(m_client, &FlareClient::serverStateChanged, this, &AiPanel::onServerStateChanged);

    m_tools = storyToolDefinitions();
    loadAlwaysDecisions();
    updateButtons();
}

AiPanel::~AiPanel() = default;

void AiPanel::setBookTree(BookTree *tree) {
    m_tree = tree;
}

bool AiPanel::eventFilter(QObject *obj, QEvent *ev) {
    if (obj == m_input && ev->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(ev);
        if (key->key() == Qt::Key_Return && !(key->modifiers() & Qt::ShiftModifier)) {
            onSend();
            return true;
        }
    }
    return QWidget::eventFilter(obj, ev);
}

void AiPanel::updateButtons() {
    m_send->setEnabled(!m_loading && !m_input->toPlainText().trimmed().isEmpty());
    m_stop->setEnabled(m_loading);
    m_status->setText(m_loading ? QStringLiteral("AI 思考中…")
                     : (m_client && m_client->isRunning() ? QStringLiteral("flare 已连接") : QStringLiteral("flare 未连接")));
}

void AiPanel::onServerFailed(const QString &message) {
    appendSystemMessage(QStringLiteral("❌ %1").arg(message));
    m_status->setText(QStringLiteral("flare 启动失败"));
}

void AiPanel::onServerStateChanged(bool running) {
    if (!running && !m_status->text().contains(QStringLiteral("失败"))) {
        m_status->setText(QStringLiteral("flare 已退出"));
    } else if (running) {
        m_status->setText(QStringLiteral("flare 已连接"));
    }
    updateButtons();
}

// ── 消息渲染 ──
void AiPanel::appendUserMessage(const QString &text) {
    QTextCursor cur = m_messages->textCursor();
    cur.movePosition(QTextCursor::End);
    cur.insertHtml(QStringLiteral(
        "<div style=\"background:#eef0ff;border-radius:6px;padding:8px;margin:4px 0;color:#333;\">%1</div>")
                       .arg(escapeHtml(text)));
    m_messages->setTextCursor(cur);
    m_messages->ensureCursorVisible();
}

void AiPanel::beginAssistantMessage() {
    QTextCursor cur = m_messages->textCursor();
    cur.movePosition(QTextCursor::End);
    cur.insertHtml(QStringLiteral(
        "<div style=\"background:#f7f5ff;border-radius:6px;padding:8px;margin:4px 0;color:#333;\">"));
    m_messages->setTextCursor(cur);
}

void AiPanel::appendAssistantChunk(const QString &text) {
    QTextCursor cur = m_messages->textCursor();
    cur.movePosition(QTextCursor::End);
    cur.insertHtml(escapeHtml(text));
    m_messages->setTextCursor(cur);
    m_messages->ensureCursorVisible();
}

void AiPanel::endAssistantMessage() {
    QTextCursor cur = m_messages->textCursor();
    cur.movePosition(QTextCursor::End);
    cur.insertHtml(QStringLiteral("</div>"));
    m_messages->setTextCursor(cur);
    m_messages->ensureCursorVisible();
}

void AiPanel::appendSystemMessage(const QString &text) {
    QTextCursor cur = m_messages->textCursor();
    cur.movePosition(QTextCursor::End);
    cur.insertHtml(QStringLiteral(
        "<div style=\"color:#8a8a9a;font-size:12px;margin:4px 0;\">%1</div>").arg(escapeHtml(text)));
    m_messages->setTextCursor(cur);
    m_messages->ensureCursorVisible();
}

// ── 发送 ──
void AiPanel::onSend() {
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty() || m_loading) return;
    m_input->clear();
    sendPrompt(text);
}

void AiPanel::sendPrompt(const QString &prompt) {
    appendUserMessage(prompt);
    m_loading = true;
    updateButtons();
    beginAssistantMessage();
    m_client->sendChat(m_sessionId, prompt, buildContext(), m_tools);
}

void AiPanel::onStop() {
    m_client->sendCancel(m_sessionId);
}

// ── 快捷动作 ──
QString AiPanel::buildContext() const {
    if (!m_tree) return QStringLiteral("（StorySpire 中还没有故事）");
    const QJsonObject sum = m_tree->storySummary();
    if (!sum.value("success").toBool()) return QStringLiteral("（StorySpire 中还没有故事）");
    QStringList parts;
    parts << QStringLiteral("【当前故事】《%1》%2")
                 .arg(sum.value("title").toString(),
                      sum.value("author").toString().isEmpty()
                          ? QString()
                          : QStringLiteral("（作者：%1）").arg(sum.value("author").toString()));
    const QJsonArray chs = sum.value("chapters").toArray();
    if (!chs.isEmpty()) {
        QStringList list;
        const int n = qMin(chs.size(), 15);
        for (int i = 0; i < n; ++i) {
            const QJsonObject c = chs.at(i).toObject();
            list << QStringLiteral("  - id=%1 | 标题：%2（%3 字）")
                        .arg(c.value("id").toString(), c.value("title").toString())
                        .arg(c.value("wordCount").toInt());
        }
        QString head = QStringLiteral("【章节（前 %1，写作时用 id 字段调用工具）】").arg(n);
        if (chs.size() > n) head += QStringLiteral("（共 %1 章）").arg(chs.size());
        parts << head + QStringLiteral("\n") + list.join(QLatin1Char('\n'));
    } else {
        parts << QStringLiteral("【章节】还没有章节");
    }
    return parts.join(QStringLiteral("\n\n"));
}

void AiPanel::onQuickContinue() {
    if (m_loading || !m_tree) return;
    const QJsonObject ch = m_tree->getChapter(m_tree->currentChapterId());
    if (!ch.value("success").toBool()) {
        appendSystemMessage(QStringLiteral("请先打开一个章节"));
        return;
    }
    const QString text = ch.value("content").toString().left(2000);
    sendPrompt(QStringLiteral("以下是小说当前内容：\n%1\n\n请续写接下来的内容（300-500字）：").arg(text));
}

void AiPanel::onQuickPolish() {
    if (m_loading || !m_tree) return;
    const QJsonObject ch = m_tree->getChapter(m_tree->currentChapterId());
    if (!ch.value("success").toBool()) {
        appendSystemMessage(QStringLiteral("请先打开一个章节"));
        return;
    }
    const QString text = ch.value("content").toString().left(2000);
    sendPrompt(QStringLiteral("请润色以下小说内容，改善语言表达和文学性：\n%1").arg(text));
}

void AiPanel::onQuickExpand() {
    if (m_loading || !m_tree) return;
    const QJsonObject ch = m_tree->getChapter(m_tree->currentChapterId());
    if (!ch.value("success").toBool()) {
        appendSystemMessage(QStringLiteral("请先打开一个章节"));
        return;
    }
    const QString text = ch.value("content").toString().left(2000);
    sendPrompt(QStringLiteral("请扩写以下内容，增加细节描写和情感刻画：\n%1").arg(text));
}

void AiPanel::onQuickInspiration() {
    if (m_loading) return;
    // 读灵感库前 3 条（轻量直读，与 InspirationPanel 同路径）
    const QString configDir = QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"));
    const QString path = (configDir.isEmpty() ? QDir::homePath() + "/.config" : configDir)
                         + "/storyspire-data/inspirations.json";
    QStringList snippet;
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
        for (int i = 0; i < qMin(arr.size(), 3); ++i) {
            const QJsonObject o = arr.at(i).toObject();
            snippet << QStringLiteral("[%1] %2").arg(o.value("category").toString(),
                                                      o.value("content").toString());
        }
        f.close();
    }
    if (snippet.isEmpty()) {
        appendSystemMessage(QStringLiteral("灵感库是空的，先去右侧「灵感」添加几条"));
        return;
    }
    sendPrompt(QStringLiteral("以下是一些灵感片段：\n%1\n\n请根据这些灵感创作一个段落或章节开头（300-500字）：")
                   .arg(snippet.join(QLatin1Char('\n'))));
}

// ── 事件处理 ──
void AiPanel::onEvent(const QJsonObject &event) {
    const QString type = event.value("type").toString();
    if (type == QStringLiteral("text")) {
        appendAssistantChunk(event.value("content").toString());
    } else if (type == QStringLiteral("tool_call")) {
        // 工具调用声明（可忽略，tool_execute 才需要宿主执行）
    } else if (type == QStringLiteral("tool_execute")) {
        handleToolExecute(event);
    } else if (type == QStringLiteral("done")) {
        endAssistantMessage();
        m_loading = false;
        updateButtons();
    } else if (type == QStringLiteral("cancelled")) {
        endAssistantMessage();
        appendSystemMessage(QStringLiteral("⏹ 已取消"));
        m_loading = false;
        updateButtons();
    } else if (type == QStringLiteral("error")) {
        endAssistantMessage();
        appendSystemMessage(QStringLiteral("❌ %1").arg(event.value("message").toString()));
        m_loading = false;
        updateButtons();
    }
}

void AiPanel::handleToolExecute(const QJsonObject &event) {
    const QString id = event.value("id").toString();
    const QString name = event.value("name").toString();
    const QJsonObject args = event.value("args").toObject();
    appendSystemMessage(QStringLiteral("🔧 工具：%1").arg(name));
    const QJsonObject result = executeTool(name, args);
    if (result.value("success").toBool()) {
        QString out = result.value("output").toString();
        if (out.isEmpty()) out = QStringLiteral("完成");
        if (out.size() > 500) out = out.left(500) + QStringLiteral("…");
        appendSystemMessage(QStringLiteral("✅ %1：%2").arg(name, out));
    } else {
        appendSystemMessage(QStringLiteral("❌ %1：%2").arg(name, result.value("error").toString()));
    }
    m_client->sendToolResult(id, result);
}

QJsonObject AiPanel::executeTool(const QString &name, const QJsonObject &args) {
    if (!m_tree) return makeResult(false, QString(), QStringLiteral("故事数据未连接"));
    if (name == QStringLiteral("story_get_story")) {
        return m_tree->storySummary();
    }
    if (name == QStringLiteral("story_list_chapters")) {
        const QJsonArray arr = m_tree->listChapters();
        return arr.isEmpty() ? QJsonObject() : arr.first().toObject();
    }
    if (name == QStringLiteral("story_get_chapter")) {
        return m_tree->getChapter(args.value("chapterId").toString());
    }
    if (name == QStringLiteral("story_update_chapter")) {
        const QJsonObject ch = m_tree->getChapter(args.value("chapterId").toString());
        if (!ch.value("success").toBool()) return ch;
        const QString realId = ch.value("id").toString();
        const QString content = args.value("content").toString();
        const QString title = ch.value("title").toString();
        const QString preview = content.left(400) + (content.size() > 400 ? QStringLiteral("…") : QString());
        const int decision = confirmWrite(QStringLiteral("story_update_chapter"),
                                          QStringLiteral("更新章节「%1」").arg(title), preview);
        if (decision == 0) {
            return makeResult(false, QString(), QStringLiteral("用户拒绝写回"), true);
        }
        const int wc = countWords(content);
        m_tree->applyChapterContentFromAi(m_tree->currentBook().value("id").toString(),
                                          realId, content, wc);
        QJsonObject r;
        r["success"] = true;
        r["output"] = QStringLiteral("已更新章节「%1」（%2 字）").arg(title).arg(wc);
        r["chapterId"] = realId;
        r["title"] = title;
        r["wordCount"] = wc;
        r["updated"] = true;
        return r;
    }
    if (name == QStringLiteral("story_create_chapter")) {
        const QString title = args.value("title").toString();
        const QString content = args.value("content").toString();
        const QString preview = content.left(400) + (content.size() > 400 ? QStringLiteral("…") : QString());
        const int decision = confirmWrite(QStringLiteral("story_create_chapter"),
                                          QStringLiteral("新建章节「%1」").arg(title), preview);
        if (decision == 0) {
            return makeResult(false, QString(), QStringLiteral("用户拒绝写回"), true);
        }
        const QString newId = m_tree->createChapter(title, content);
        if (newId.isEmpty()) return makeResult(false, QString(), QStringLiteral("章节创建失败"));
        QJsonObject r;
        r["success"] = true;
        r["output"] = QStringLiteral("已创建章节「%1」").arg(title);
        r["chapterId"] = newId;
        r["title"] = title;
        r["wordCount"] = countWords(content);
        return r;
    }
    return makeResult(false, QString(), QStringLiteral("未知 story 工具: %1").arg(name));
}

// ── 确认决策 ──
void AiPanel::loadAlwaysDecisions() {
    m_alwaysAllowed.clear();
    const QString path = QDir::homePath() + QStringLiteral("/.config/storyspire-qt/confirm.json");
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            if (it.value().toBool()) m_alwaysAllowed.insert(it.key());
        }
        f.close();
    }
}

void AiPanel::saveAlwaysDecisions() {
    const QString path = QDir::homePath() + QStringLiteral("/.config/storyspire-qt/confirm.json");
    QDir().mkpath(QFileInfo(path).absolutePath());
    QJsonObject obj;
    for (const QString &t : m_alwaysAllowed) obj[t] = true;
    QSaveFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        f.commit();
    }
}

int AiPanel::confirmWrite(const QString &tool, const QString &target, const QString &preview) {
    if (m_sessionAllowed.contains(tool)) return 2;
    if (m_alwaysAllowed.contains(tool)) return 3;

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("AI 请求写入章节"));
    dlg.resize(420, 260);
    auto *lay = new QVBoxLayout(&dlg);
    auto *targetLabel = new QLabel(QStringLiteral("目标：<b>%1</b>").arg(target.toHtmlEscaped()), &dlg);
    lay->addWidget(targetLabel);
    if (!preview.isEmpty()) {
        auto *prevLabel = new QLabel(&dlg);
        prevLabel->setText(QStringLiteral("内容预览：\n%1").arg(preview.toHtmlEscaped()));
        prevLabel->setWordWrap(true);
        prevLabel->setStyleSheet(QStringLiteral(
            "background:#f7f5ff;border:1px solid #e4e4ef;border-radius:4px;padding:6px;font-size:12px;"));
        lay->addWidget(prevLabel, 1);
    }
    auto *hint = new QLabel(QStringLiteral("是否将 AI 生成的内容应用到章节？"), &dlg);
    hint->setStyleSheet(QStringLiteral("color:#8a8a9a;font-size:11px;"));
    lay->addWidget(hint);
    auto *btnRow = new QHBoxLayout;
    auto *once = new QPushButton(QStringLiteral("应用一次"), &dlg);
    auto *session = new QPushButton(QStringLiteral("本次会话允许"), &dlg);
    auto *always = new QPushButton(QStringLiteral("始终允许"), &dlg);
    auto *deny = new QPushButton(QStringLiteral("拒绝"), &dlg);
    btnRow->addWidget(once);
    btnRow->addWidget(session);
    btnRow->addWidget(always);
    btnRow->addWidget(deny);
    lay->addLayout(btnRow);
    connect(once, &QPushButton::clicked, &dlg, [&dlg]() { dlg.done(1); });
    connect(session, &QPushButton::clicked, &dlg, [&dlg]() { dlg.done(2); });
    connect(always, &QPushButton::clicked, &dlg, [&dlg]() { dlg.done(3); });
    connect(deny, &QPushButton::clicked, &dlg, [&dlg]() { dlg.done(0); });

    const int decision = dlg.exec();
    if (decision == 2) m_sessionAllowed.insert(tool);
    if (decision == 3) {
        m_alwaysAllowed.insert(tool);
        saveAlwaysDecisions();
    }
    return decision;
}

QJsonObject AiPanel::makeResult(bool success, const QString &output,
                                const QString &error, bool denied) {
    QJsonObject r;
    r["success"] = success;
    r["output"] = output;
    if (!error.isEmpty()) r["error"] = error;
    if (denied) r["denied"] = true;
    return r;
}

int AiPanel::countWords(const QString &text) {
    const int zh = text.count(QRegularExpression(QStringLiteral("[\\x{4e00}-\\x{9fff}\\x{3400}-\\x{4dbf}]")));
    const int en = text.split(QRegularExpression(QStringLiteral("[^a-zA-Z]+")), Qt::SkipEmptyParts).size();
    return zh + en;
}
