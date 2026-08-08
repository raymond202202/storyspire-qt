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
    /** 最近选中/操作的章节 id（快捷动作上下文） */
    QString currentChapterId() const { return m_contextChapterId; }

    // ── AI 工具数据接口（Flare 宿主代理工具）──
    /** story_get_story：故事结构摘要（不含正文） */
    QJsonObject storySummary() const;
    /** story_get_chapter：按 id 或标题获取章节完整内容 */
    QJsonObject getChapter(const QString &chapterIdOrTitle) const;
    /** story_list_chapters：章节列表（id/标题/字数/卷） */
    QJsonArray listChapters() const;
    /** story_create_chapter：新建章节（返回新章节 id，失败返回空） */
    QString createChapter(const QString &title, const QString &content);

signals:
    void chapterSelected(const QString &bookId, const QString &chapterId, const QString &title, const QString &contentHtml);
    void chapterRenamed(const QString &bookId, const QString &chapterId, const QString &title);
    void chapterDeleted(const QString &bookId, const QString &chapterId);
    /** 章节内容被写回（含 AI 工具写回），供编辑器刷新 */
    void chapterContentApplied(const QString &bookId, const QString &chapterId);
    void booksChanged();

public slots:
    /** 编辑器实时写回：更新章节 content/wordCount/updatedAt 并落盘（用户编辑路径，不通知刷新） */
    void applyChapterContent(const QString &bookId, const QString &chapterId,
                             const QString &content, int wordCount);
    /** AI 工具写回：更新章节并落盘，随后 emit chapterContentApplied 通知界面刷新 */
    void applyChapterContentFromAi(const QString &bookId, const QString &chapterId,
                                   const QString &content, int wordCount);
    /** 重命名章节（标题栏编辑或右键菜单，与 Electron renameChapter 语义一致） */
    void renameChapter(const QString &bookId, const QString &chapterId, const QString &title);
    /** 新建书籍（菜单/快捷键） */
    void newBook() { onNewBook(); }
    /** 新建章节（菜单/快捷键） */
    void newChapter() { onNewChapter(); }
    /** 重命名当前选中章节（F2） */
    void renameCurrentChapter() { onRenameAction(); }
    /** 删除当前选中章节（菜单） */
    void deleteCurrentChapter() { onDeleteAction(); }

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
    /** 写回章节内容（notify=true 时 emit chapterContentApplied） */
    bool writeChapterContent(const QString &bookId, const QString &chapterId,
                             const QString &content, int wordCount, bool notify);
};
