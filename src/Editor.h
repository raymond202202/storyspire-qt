#pragma once

#include <QWidget>

class QLineEdit;
class QTextEdit;
class QLabel;

class Editor : public QWidget {
    Q_OBJECT
public:
    explicit Editor(QWidget *parent = nullptr);

public slots:
    void loadChapter(const QString &bookId, const QString &chapterId,
                     const QString &title, const QString &contentHtml);

private:
    QLineEdit *m_title = nullptr;
    QTextEdit *m_body = nullptr;
    QLabel *m_wordCount = nullptr;
    QString m_bookId;
    QString m_chapterId;
};
