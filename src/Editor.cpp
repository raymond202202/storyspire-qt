#include "Editor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QTimer>
#include <QTime>
#include <QSettings>
#include <QRegularExpression>
#include <QFont>

Editor::Editor(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    auto *row = new QHBoxLayout;
    m_title = new QLineEdit(this);
    m_title->setPlaceholderText(QStringLiteral("章节标题"));
    row->addWidget(m_title, 1);
    m_wordCount = new QLabel(QStringLiteral("0 字"), this);
    row->addWidget(m_wordCount);
    m_saveState = new QLabel(QStringLiteral(""), this);
    row->addWidget(m_saveState);
    layout->addLayout(row);

    m_body = new QTextEdit(this);
    m_body->setPlaceholderText(QStringLiteral("灵感一闪而过，先在这里写下来…"));
    layout->addWidget(m_body, 1);

    // 防抖自动保存：停止输入 500ms 后写回 books.json
    m_autoSave = new QTimer(this);
    m_autoSave->setSingleShot(true);
    m_autoSave->setInterval(500);

    connect(m_body, &QTextEdit::textChanged, this, [this]() {
        if (m_loading) return;
        m_dirty = true;
        m_wordCount->setText(QStringLiteral("%1 字").arg(countWords(m_body->toPlainText())));
        m_saveState->setText(QStringLiteral("● 未保存"));
        emit saveStateChanged(m_saveState->text());
        m_autoSave->start();
    });

    // 标题栏编辑（回车/失焦）→ 重命名章节
    connect(m_title, &QLineEdit::editingFinished, this, [this]() {
        if (m_loading) return;
        if (!m_chapterId.isEmpty()) {
            emit titleEdited(m_bookId, m_chapterId, m_title->text().trimmed());
        }
    });

    connect(m_autoSave, &QTimer::timeout, this, &Editor::flushContent);

    // 应用已保存的编辑器字体
    QSettings settings;
    const QFont savedFont = settings.value(QStringLiteral("editor/font")).value<QFont>();
    if (savedFont != m_body->font()) {
        m_body->document()->setDefaultFont(savedFont);
    }
}

void Editor::loadChapter(const QString &bookId, const QString &chapterId,
                         const QString &title, const QString &contentHtml) {
    // 切换章节前先保存当前章节
    flushContent();
    m_loading = true;
    m_bookId = bookId;
    m_chapterId = chapterId;
    m_title->setText(title);
    m_body->setPlainText(contentHtml);
    m_dirty = false;
    m_loading = false;
    m_wordCount->setText(QStringLiteral("%1 字").arg(countWords(contentHtml)));
    m_saveState->setText(QStringLiteral("✓ 已保存"));
    emit saveStateChanged(m_saveState->text());
}

void Editor::clearChapter() {
    m_loading = true;
    m_bookId.clear();
    m_chapterId.clear();
    m_dirty = false;
    m_title->clear();
    m_body->clear();
    m_wordCount->setText(QStringLiteral("0 字"));
    m_saveState->clear();
    emit saveStateChanged(QString());
    m_loading = false;
}

void Editor::updateTitle(const QString &title) {
    if (m_title->text() != title) m_title->setText(title);
}

void Editor::reloadContent(const QString &content) {
    if (m_loading || m_chapterId.isEmpty()) return;
    m_loading = true;
    m_body->setPlainText(content);
    m_dirty = false;
    m_loading = false;
    m_wordCount->setText(QStringLiteral("%1 字").arg(countWords(content)));
    m_saveState->setText(QStringLiteral("✓ 已保存（AI 写回）"));
    emit saveStateChanged(m_saveState->text());
}

void Editor::saveNow() {
    flushContent();
}

void Editor::setEditorFont(const QFont &font) {
    m_body->document()->setDefaultFont(font);
    QSettings settings;
    settings.setValue(QStringLiteral("editor/font"), font);
}

void Editor::flushContent() {
    if (!m_dirty || m_chapterId.isEmpty()) return;
    m_dirty = false;
    const QString text = m_body->toPlainText();
    emit contentEdited(m_bookId, m_chapterId, text, countWords(text));
    m_saveState->setText(QStringLiteral("✓ 已自动保存 %1")
                             .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
    emit saveStateChanged(m_saveState->text());
}

int Editor::countWords(const QString &text) const {
    // 与 Electron 版一致：中文字符数 + 英文单词数
    const int zh = text.count(QRegularExpression(QStringLiteral("[\\x{4e00}-\\x{9fff}\\x{3400}-\\x{4dbf}]")));
    const int en = text.split(QRegularExpression(QStringLiteral("[^a-zA-Z]+")), Qt::SkipEmptyParts).size();
    return zh + en;
}
