#pragma once

#include <QWidget>

class QLineEdit;
class QTextEdit;
class QLabel;
class QTimer;

/**
 * 编辑器 —— 纯文本编辑（content 字段存纯文本，不做富文本/HTML 迁移）。
 * 编辑后实时写回 books.json（500ms 防抖自动保存）+ 切换章节时保存当前。
 */
class Editor : public QWidget {
    Q_OBJECT
public:
    explicit Editor(QWidget *parent = nullptr);

signals:
    /** 正文编辑后写回（自动保存/切换章节时发出） */
    void contentEdited(const QString &bookId, const QString &chapterId,
                       const QString &content, int wordCount);
    /** 标题编辑完成（回车/失焦） */
    void titleEdited(const QString &bookId, const QString &chapterId, const QString &title);

public slots:
    void loadChapter(const QString &bookId, const QString &chapterId,
                     const QString &title, const QString &contentHtml);
    /** 章节被删除时清空编辑区 */
    void clearChapter();
    /** 树侧右键重命名后同步标题栏 */
    void updateTitle(const QString &title);
    /** AI/外部写回当前章节后刷新正文（不触发写回） */
    void reloadContent(const QString &content);

private slots:
    void flushContent();

private:
    QLineEdit *m_title = nullptr;
    QTextEdit *m_body = nullptr;
    QLabel *m_wordCount = nullptr;
    QLabel *m_saveState = nullptr;
    QTimer *m_autoSave = nullptr;
    QString m_bookId;
    QString m_chapterId;
    bool m_loading = false;
    bool m_dirty = false;

    int countWords(const QString &text) const;
};
