#pragma once

#include <QWidget>
#include <QJsonObject>

class QListWidget;
class QLineEdit;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class QTimer;
class BookTree;

/**
 * 大纲面板 —— 独立于正文存储（book.outlines，与 Electron 同格式）。
 * 数据接口走 BookTree（addOutline/updateOutline/renameOutline/deleteOutline，
 * 对齐 Electron storyStore 大纲语义）。
 */
class OutlinePanel : public QWidget {
    Q_OBJECT
public:
    explicit OutlinePanel(QWidget *parent = nullptr);
    void setBookTree(BookTree *tree);

public slots:
    /** 重新加载大纲列表（新建/删除大纲或外部变更后调用） */
    void reload();

private slots:
    void onNewOutline();
    void onRenameOutline();
    void onDeleteOutline();
    void onItemSelected(int row);
    void flushContent();
    void onTitleEdited();

private:
    BookTree *m_tree = nullptr;
    QListWidget *m_list = nullptr;
    QLineEdit *m_title = nullptr;
    QPlainTextEdit *m_body = nullptr;
    QLabel *m_wordCount = nullptr;
    QLabel *m_saveState = nullptr;
    QPushButton *m_newBtn = nullptr;
    QPushButton *m_renameBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;
    QTimer *m_debounce = nullptr;

    QString m_outlineId;
    bool m_loading = false;
    bool m_dirty = false;

    QJsonObject currentOutline() const;
    int countWords(const QString &text) const;
};
