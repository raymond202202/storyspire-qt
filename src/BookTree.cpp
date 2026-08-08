#include "BookTree.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QPushButton>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QMessageBox>
#include <QDateTime>

BookTree::BookTree(QWidget *parent) : QWidget(parent) {
    const QString configDir = QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"));
    m_path = (configDir.isEmpty() ? QDir::homePath() + "/.config" : configDir)
             + "/storyspire-data/books.json";

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *row = new QHBoxLayout;
    m_newBook = new QPushButton(QStringLiteral("＋ 新建书籍"), this);
    m_newChapter = new QPushButton(QStringLiteral("＋ 新建章节"), this);
    row->addWidget(m_newBook);
    row->addWidget(m_newChapter);
    layout->addLayout(row);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    layout->addWidget(m_tree, 1);

    connect(m_newBook, &QPushButton::clicked, this, &BookTree::onNewBook);
    connect(m_newChapter, &QPushButton::clicked, this, &BookTree::onNewChapter);
    connect(m_tree, &QTreeWidget::itemClicked, this, &BookTree::onItemClicked);

    reload();
}

BookTree::~BookTree() = default;

void BookTree::reload() {
    m_books = QJsonArray();
    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject()) {
            m_books = doc.object().value("books").toArray();
        } else if (doc.isArray()) {
            m_books = doc.array(); // 兼容旧格式
        }
        f.close();
    }
    if (m_books.isEmpty()) {
        // 自动创建默认书
        const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
        QJsonObject book;
        book["id"] = QStringLiteral("b_%1").arg(QDateTime::currentMSecsSinceEpoch());
        book["title"] = QStringLiteral("未命名小说");
        book["author"] = QStringLiteral("");
        book["createdAt"] = now;
        book["updatedAt"] = now;
        QJsonArray volumes;
        QJsonObject vol;
        vol["id"] = QStringLiteral("v_%1").arg(QDateTime::currentMSecsSinceEpoch());
        vol["title"] = QStringLiteral("第一卷");
        vol["order"] = 1;
        vol["createdAt"] = now;
        volumes.append(vol);
        book["volumes"] = volumes;
        QJsonArray chapters;
        QJsonObject ch;
        ch["id"] = QStringLiteral("c_%1").arg(QDateTime::currentMSecsSinceEpoch());
        ch["title"] = QStringLiteral("未命名");
        ch["volumeId"] = vol["id"];
        ch["content"] = QStringLiteral("");
        ch["wordCount"] = 0;
        ch["createdAt"] = now;
        ch["updatedAt"] = now;
        chapters.append(ch);
        book["chapters"] = chapters;
        book["outlines"] = QJsonArray();
        m_books.append(book);
        save();
    }
    buildTree();
}

void BookTree::save() {
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile f(m_path);
    if (f.open(QIODevice::WriteOnly)) {
        QJsonObject root;
        root["books"] = m_books;
        root["trash"] = QJsonArray();
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        f.commit();
    }
}

QJsonObject BookTree::currentBook() const {
    return m_books.first().toObject();
}

void BookTree::buildTree() {
    m_tree->clear();
    for (const auto &bVal : m_books) {
        const QJsonObject book = bVal.toObject();
        auto *bookItem = new QTreeWidgetItem(m_tree);
        bookItem->setText(0, QStringLiteral("📚 %1").arg(book.value("title").toString()));
        bookItem->setData(0, Qt::UserRole, book.value("id").toString());
        bookItem->setData(0, Qt::UserRole + 1, QStringLiteral("book"));
        const QJsonArray chapters = book.value("chapters").toArray();
        for (const auto &cVal : chapters) {
            const QJsonObject ch = cVal.toObject();
            auto *chItem = new QTreeWidgetItem(bookItem);
            chItem->setText(0, QStringLiteral("📄 %1").arg(ch.value("title").toString()));
            chItem->setData(0, Qt::UserRole, ch.value("id").toString());
            chItem->setData(0, Qt::UserRole + 1, QStringLiteral("chapter"));
        }
        bookItem->setExpanded(true);
    }
}

QJsonObject BookTree::findBook(const QString &id) const {
    for (const auto &bVal : m_books) {
        const QJsonObject book = bVal.toObject();
        if (book.value("id").toString() == id) return book;
    }
    return QJsonObject();
}

void BookTree::onItemClicked(QTreeWidgetItem *item, int) {
    if (!item) return;
    const QString type = item->data(0, Qt::UserRole + 1).toString();
    if (type != QStringLiteral("chapter")) return;
    const QString bookId = item->parent() ? item->parent()->data(0, Qt::UserRole).toString() : QString();
    const QJsonObject book = findBook(bookId);
    const QString chId = item->data(0, Qt::UserRole).toString();
    for (const auto &cVal : book.value("chapters").toArray()) {
        const QJsonObject ch = cVal.toObject();
        if (ch.value("id").toString() == chId) {
            emit chapterSelected(bookId, chId, ch.value("title").toString(), ch.value("content").toString());
            break;
        }
    }
}

void BookTree::onNewBook() {
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonObject book;
    book["id"] = QStringLiteral("b_%1").arg(QDateTime::currentMSecsSinceEpoch());
    book["title"] = QStringLiteral("未命名小说");
    book["author"] = QStringLiteral("");
    book["createdAt"] = now;
    book["updatedAt"] = now;
    book["volumes"] = QJsonArray();
    book["chapters"] = QJsonArray();
    book["outlines"] = QJsonArray();
    m_books.append(book);
    save();
    buildTree();
    emit booksChanged();
}

void BookTree::onNewChapter() {
    if (m_books.isEmpty()) return;
    QJsonObject book = m_books.first().toObject();
    QJsonArray chapters = book.value("chapters").toArray();
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QString volId = book.value("volumes").toArray().first().toObject().value("id").toString();
    QJsonObject ch;
    ch["id"] = QStringLiteral("c_%1").arg(QDateTime::currentMSecsSinceEpoch());
    ch["title"] = QStringLiteral("未命名");
    ch["volumeId"] = volId;
    ch["content"] = QStringLiteral("");
    ch["wordCount"] = 0;
    ch["createdAt"] = now;
    ch["updatedAt"] = now;
    chapters.append(ch);
    book["chapters"] = chapters;
    m_books.replace(0, book);
    save();
    buildTree();
    emit booksChanged();
}
