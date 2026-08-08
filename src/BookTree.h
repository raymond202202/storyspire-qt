#pragma once

#include <QWidget>
#include <QJsonArray>
#include <QJsonObject>

class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QTimer;

/**
 * 左侧书/章树 —— 数据兼容 Electron 版 StorySpire：
 * ~/.config/storyspire-data/books.json（{ books: [...], trash: [...] }）
 * 阶段1：编辑写回（updateChapter 语义）、章节重命名/删除（移入回收站）。
 */
class BookTree : public QWidget {
    Q_OBJECT
public:
    explicit BookTree(QWidget *parent = nullptr);
    ~BookTree() override;

    /** 加载/重新加载 books.json */
    void reload();
    /** 保存 books.json（原子写） */
    void save();
    /** 当前书籍（第一本） */
    QJsonObject currentBook() const;

signals:
    void chapterSelected(const QString &bookId, const QString &chapterId, const QString &title, const QString &contentHtml);
    void chapterRenamed(const QString &bookId, const QString &chapterId, const QString &title);
    void chapterDeleted(const QString &bookId, const QString &chapterId);
    void booksChanged();

public slots:
    /** 编辑器实时写回：更新章节 content/wordCount/updatedAt 并落盘 */
    void applyChapterContent(const QString &bookId, const QString &chapterId,
                             const QString &content, int wordCount);
    /** 重命名章节（标题栏编辑或右键菜单，与 Electron renameChapter 语义一致） */
    void renameChapter(const QString &bookId, const QString &chapterId, const QString &title);

private slots:
    void onItemClicked(QTreeWidgetItem *item, int column);
    void onNewBook();
    void onNewChapter();
    void onContextMenu(const QPoint &pos);
    void onRenameAction();
    void onDeleteAction();

private:
    QTreeWidget *m_tree = nullptr;
    QPushButton *m_newBook = nullptr;
    QPushButton *m_newChapter = nullptr;
    QJsonArray m_books;
    QJsonArray m_trash;
    QString m_path;
    QTimer *m_autoSaveTimer = nullptr;

    // 最近点击/右键的上下文
    QString m_currentBookId;
    QString m_contextBookId;
    QString m_contextChapterId;
    QString m_contextChapterVolumeId;

    void buildTree();
    QJsonObject findBook(const QString &id) const;
    int findBookIndex(const QString &id) const;
    void deleteChapterInternal(const QString &bookId, const QString &chapterId);
};
