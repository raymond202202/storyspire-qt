#pragma once

#include <QWidget>
#include <QJsonArray>

class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;

/**
 * 左侧书/章树 —— 数据兼容 Electron 版 StorySpire：
 * ~/.config/storyspire-data/books.json（{ books: [...], trash: [...] }）
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
    void booksChanged();

private slots:
    void onItemClicked(QTreeWidgetItem *item, int column);
    void onNewBook();
    void onNewChapter();

private:
    QTreeWidget *m_tree = nullptr;
    QPushButton *m_newBook = nullptr;
    QPushButton *m_newChapter = nullptr;
    QJsonArray m_books;
    QString m_path;

    void buildTree();
    QJsonObject findBook(const QString &id) const;
};
