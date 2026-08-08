#include "Editor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QRegularExpression>

Editor::Editor(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    auto *row = new QHBoxLayout;
    m_title = new QLineEdit(this);
    m_title->setPlaceholderText(QStringLiteral("章节标题"));
    row->addWidget(m_title, 1);
    m_wordCount = new QLabel(QStringLiteral("0 字"), this);
    row->addWidget(m_wordCount);
    layout->addLayout(row);

    m_body = new QTextEdit(this);
    m_body->setPlaceholderText(QStringLiteral("灵感一闪而过，先在这里写下来…"));
    layout->addWidget(m_body, 1);

    connect(m_body, &QTextEdit::textChanged, this, [this]() {
        const QString text = m_body->toPlainText();
        const int zh = text.count(QRegularExpression(QStringLiteral("[\\u4e00-\\u9fff\\u3400-\\u4dbf]")));
        const int en = text.split(QRegularExpression(QStringLiteral("[^a-zA-Z]+")), Qt::SkipEmptyParts).size();
        m_wordCount->setText(QStringLiteral("%1 字").arg(zh + en));
    });
}

void Editor::loadChapter(const QString &bookId, const QString &chapterId,
                         const QString &title, const QString &contentHtml) {
    m_bookId = bookId;
    m_chapterId = chapterId;
    m_title->setText(title);
    m_body->setPlainText(contentHtml);
}
