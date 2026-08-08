#include "BookTree.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QPushButton>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QDir>
#include <QMessageBox>
#include <QDateTime>
#include <QMenu>
#include <QDialog>
#include <QLineEdit>
#include <QTimer>
#include <QRegularExpression>

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
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_tree, 1);

    // 自动保存兜底：每 30s 落盘一次（与 Electron autoSaveInterval 一致）
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setInterval(30000);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &BookTree::save);
    m_autoSaveTimer->start();

    connect(m_newBook, &QPushButton::clicked, this, &BookTree::onNewBook);
    connect(m_newChapter, &QPushButton::clicked, this, &BookTree::onNewChapter);
    connect(m_tree, &QTreeWidget::itemClicked, this, &BookTree::onItemClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &BookTree::onContextMenu);

    reload();
}

BookTree::~BookTree() = default;

void BookTree::reload() {
    m_books = QJsonArray();
    m_trash = QJsonArray();
    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject()) {
            m_books = doc.object().value("books").toArray();
            m_trash = doc.object().value("trash").toArray();
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
    if (!m_books.isEmpty()) {
        m_currentBookId = m_books.first().toObject().value("id").toString();
    }
    buildTree();
}

void BookTree::save() {
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile f(m_path);
    if (f.open(QIODevice::WriteOnly)) {
        QJsonObject root;
        root["books"] = m_books;
        root["trash"] = m_trash;
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        f.commit();
    }
}

QJsonObject BookTree::currentBook() const {
    return m_books.first().toObject();
}

namespace {
int bookCountWords(const QString &text) {
    const int zh = text.count(QRegularExpression(QStringLiteral("[\\x{4e00}-\\x{9fff}\\x{3400}-\\x{4dbf}]")));
    const int en = text.split(QRegularExpression(QStringLiteral("[^a-zA-Z]+")), Qt::SkipEmptyParts).size();
    return zh + en;
}
} // namespace

QJsonObject BookTree::storySummary() const {
    QJsonObject out;
    if (m_books.isEmpty()) {
        out["success"] = false;
        out["error"] = QStringLiteral("还没有故事（先在应用里新建故事）");
        return out;
    }
    const QJsonObject book = m_books.first().toObject();
    out["success"] = true;
    out["title"] = book.value("title");
    out["author"] = book.value("author");
    QJsonArray vols;
    for (const auto &v : book.value("volumes").toArray()) {
        const QJsonObject vo = v.toObject();
        QJsonObject vv;
        vv["id"] = vo.value("id");
        vv["title"] = vo.value("title");
        vols.append(vv);
    }
    out["volumes"] = vols;
    QJsonArray chs;
    for (const auto &c : book.value("chapters").toArray()) {
        const QJsonObject co = c.toObject();
        QJsonObject cc;
        cc["id"] = co.value("id");
        cc["title"] = co.value("title");
        cc["volumeId"] = co.value("volumeId");
        cc["wordCount"] = co.value("wordCount");
        chs.append(cc);
    }
    out["chapters"] = chs;
    return out;
}

QJsonObject BookTree::getChapter(const QString &chapterIdOrTitle) const {
    QJsonObject out;
    if (m_books.isEmpty()) {
        out["success"] = false;
        out["error"] = QStringLiteral("没有故事");
        return out;
    }
    const QJsonObject book = m_books.first().toObject();
    const QJsonArray chapters = book.value("chapters").toArray();
    QJsonObject hit;
    for (const auto &c : chapters) {
        const QJsonObject co = c.toObject();
        if (co.value("id").toString() == chapterIdOrTitle) { hit = co; break; }
    }
    // 容错：支持按标题匹配（对齐 Electron storyTools）
    if (hit.isEmpty() && !chapterIdOrTitle.isEmpty()) {
        for (const auto &c : chapters) {
            const QJsonObject co = c.toObject();
            const QString t = co.value("title").toString();
            if (t == chapterIdOrTitle || t.contains(chapterIdOrTitle) ||
                (chapterIdOrTitle.contains(t) && chapterIdOrTitle.size() < 20)) {
                hit = co;
                break;
            }
        }
    }
    if (hit.isEmpty()) {
        out["success"] = false;
        out["error"] = QStringLiteral("未找到章节「%1」（先用 story_list_chapters 获取正确的 chapterId）").arg(chapterIdOrTitle);
        return out;
    }
    out["success"] = true;
    out["id"] = hit.value("id");
    out["title"] = hit.value("title");
    out["content"] = hit.value("content");
    out["wordCount"] = hit.value("wordCount");
    return out;
}

QJsonArray BookTree::listChapters() const {
    QJsonObject out;
    if (m_books.isEmpty()) {
        out["success"] = false;
        out["error"] = QStringLiteral("没有故事");
        return QJsonArray{out};
    }
    const QJsonObject book = m_books.first().toObject();
    QJsonArray list;
    for (const auto &c : book.value("chapters").toArray()) {
        const QJsonObject co = c.toObject();
        QJsonObject cc;
        cc["id"] = co.value("id");
        cc["title"] = co.value("title");
        cc["wordCount"] = co.value("wordCount");
        cc["volumeId"] = co.value("volumeId");
        list.append(cc);
    }
    out["success"] = true;
    out["chapters"] = list;
    return QJsonArray{out};
}

QString BookTree::createChapter(const QString &title, const QString &content) {
    if (m_books.isEmpty()) return QString();
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    const int bookIdx = 0;
    QJsonObject book = m_books.at(bookIdx).toObject();

    QString volId = m_contextChapterVolumeId;
    if (volId.isEmpty()) {
        const QJsonArray vols = book.value("volumes").toArray();
        if (!vols.isEmpty()) volId = vols.first().toObject().value("id").toString();
    }
    if (volId.isEmpty()) {
        QJsonObject vol;
        vol["id"] = QStringLiteral("v_%1").arg(QDateTime::currentMSecsSinceEpoch());
        vol["title"] = QStringLiteral("第一卷");
        vol["order"] = 1;
        vol["createdAt"] = now;
        QJsonArray vols = book.value("volumes").toArray();
        vols.append(vol);
        book["volumes"] = vols;
        volId = vol.value("id").toString();
    }

    QJsonArray chapters = book.value("chapters").toArray();
    QJsonObject ch;
    ch["id"] = QStringLiteral("c_%1").arg(QDateTime::currentMSecsSinceEpoch());
    ch["title"] = title.trimmed().isEmpty() ? QStringLiteral("新章节") : title.trimmed();
    ch["volumeId"] = volId;
    ch["content"] = content;
    ch["wordCount"] = bookCountWords(content);
    ch["createdAt"] = now;
    ch["updatedAt"] = now;
    chapters.append(ch);
    book["chapters"] = chapters;
    book["updatedAt"] = now;
    m_books.replace(bookIdx, book);
    m_currentBookId = book.value("id").toString();
    m_contextChapterId = ch.value("id").toString();
    m_contextChapterVolumeId = volId;
    save();
    buildTree();
    emit chapterSelected(book.value("id").toString(), ch.value("id").toString(),
                         ch.value("title").toString(), content);
    emit chapterContentApplied(book.value("id").toString(), ch.value("id").toString());
    emit booksChanged();
    return ch.value("id").toString();
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
            const int wc = ch.value("wordCount").toInt();
            chItem->setText(0, QStringLiteral("📄 %1  · %2 字").arg(ch.value("title").toString()).arg(wc));
            chItem->setData(0, Qt::UserRole, ch.value("id").toString());
            chItem->setData(0, Qt::UserRole + 1, QStringLiteral("chapter"));
            chItem->setData(0, Qt::UserRole + 2, ch.value("title").toString());
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

int BookTree::findBookIndex(const QString &id) const {
    for (int i = 0; i < m_books.size(); ++i) {
        if (m_books.at(i).toObject().value("id").toString() == id) return i;
    }
    return -1;
}

void BookTree::onItemClicked(QTreeWidgetItem *item, int) {
    if (!item) return;
    const QString type = item->data(0, Qt::UserRole + 1).toString();
    const QString bookId = item->parent() ? item->parent()->data(0, Qt::UserRole).toString() : QString();
    if (type == QStringLiteral("book")) {
        m_currentBookId = item->data(0, Qt::UserRole).toString();
        m_contextChapterId.clear();
        m_contextChapterVolumeId.clear();
        return;
    }
    if (type != QStringLiteral("chapter")) return;
    const QString chId = item->data(0, Qt::UserRole).toString();
    const QJsonObject book = findBook(bookId);
    for (const auto &cVal : book.value("chapters").toArray()) {
        const QJsonObject ch = cVal.toObject();
        if (ch.value("id").toString() == chId) {
            m_currentBookId = bookId;
            m_contextBookId = bookId;
            m_contextChapterId = chId;
            m_contextChapterVolumeId = ch.value("volumeId").toString();
            emit chapterSelected(bookId, chId, ch.value("title").toString(), ch.value("content").toString());
            break;
        }
    }
}

void BookTree::applyChapterContent(const QString &bookId, const QString &chapterId,
                                   const QString &content, int wordCount) {
    writeChapterContent(bookId, chapterId, content, wordCount, false);
}

void BookTree::applyChapterContentFromAi(const QString &bookId, const QString &chapterId,
                                         const QString &content, int wordCount) {
    writeChapterContent(bookId, chapterId, content, wordCount, true);
}

bool BookTree::writeChapterContent(const QString &bookId, const QString &chapterId,
                                   const QString &content, int wordCount, bool notify) {
    const int bi = findBookIndex(bookId);
    if (bi < 0 || chapterId.isEmpty()) return false;
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonObject book = m_books.at(bi).toObject();
    QJsonArray chapters = book.value("chapters").toArray();
    bool changed = false;
    for (int i = 0; i < chapters.size(); ++i) {
        QJsonObject c = chapters.at(i).toObject();
        if (c.value("id").toString() == chapterId) {
            c["content"] = content;
            c["wordCount"] = wordCount;
            c["updatedAt"] = now;
            chapters.replace(i, c);
            changed = true;
            break;
        }
    }
    if (!changed) return false;
    book["chapters"] = chapters;
    book["updatedAt"] = now;
    m_books.replace(bi, book);
    save();
    if (notify) emit chapterContentApplied(bookId, chapterId);
    // 更新树上字数显示（不重建，保持展开状态）
    for (int b = 0; b < m_tree->topLevelItemCount(); ++b) {
        QTreeWidgetItem *bookItem = m_tree->topLevelItem(b);
        if (bookItem->data(0, Qt::UserRole).toString() != bookId) continue;
        for (int c = 0; c < bookItem->childCount(); ++c) {
            QTreeWidgetItem *chItem = bookItem->child(c);
            if (chItem->data(0, Qt::UserRole).toString() != chapterId) continue;
            const QString title = chItem->data(0, Qt::UserRole + 2).toString();
            chItem->setText(0, QStringLiteral("📄 %1  · %2 字").arg(title).arg(wordCount));
            break;
        }
        break;
    }
    return true;
}

void BookTree::renameChapter(const QString &bookId, const QString &chapterId, const QString &title) {
    if (title.trimmed().isEmpty() || chapterId.isEmpty()) return;
    const int bi = findBookIndex(bookId);
    if (bi < 0) return;
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QString newTitle = title.trimmed();
    QJsonObject book = m_books.at(bi).toObject();
    QJsonArray chapters = book.value("chapters").toArray();
    bool changed = false;
    for (int i = 0; i < chapters.size(); ++i) {
        QJsonObject c = chapters.at(i).toObject();
        if (c.value("id").toString() == chapterId) {
            if (c.value("title").toString() == newTitle) return; // 无变化
            c["title"] = newTitle;
            c["updatedAt"] = now;
            chapters.replace(i, c);
            changed = true;
            break;
        }
    }
    if (!changed) return;
    book["chapters"] = chapters;
    book["updatedAt"] = now;
    m_books.replace(bi, book);
    save();
    buildTree();
    emit chapterRenamed(bookId, chapterId, newTitle);
    emit booksChanged();
}

void BookTree::deleteChapterInternal(const QString &bookId, const QString &chapterId) {
    const int bi = findBookIndex(bookId);
    if (bi < 0 || chapterId.isEmpty()) return;
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonObject book = m_books.at(bi).toObject();
    QJsonArray chapters = book.value("chapters").toArray();
    QJsonObject target;
    QJsonArray filtered;
    for (const auto &v : chapters) {
        const QJsonObject c = v.toObject();
        if (c.value("id").toString() == chapterId) target = c;
        else filtered.append(v);
    }
    if (target.isEmpty()) return;
    // 移入回收站（与 Electron TrashItem 格式一致）
    QJsonObject item;
    item["id"] = QStringLiteral("trash_%1").arg(QDateTime::currentMSecsSinceEpoch());
    item["type"] = QStringLiteral("chapter");
    item["bookId"] = bookId;
    item["volumeId"] = target.value("volumeId").toString();
    item["title"] = target.value("title").toString();
    item["data"] = target;
    item["deletedAt"] = now;
    QJsonArray trash = m_trash;
    trash.prepend(item);
    m_trash = trash;

    book["chapters"] = filtered;
    book["updatedAt"] = now;
    m_books.replace(bi, book);
    save();
    buildTree();
    emit chapterDeleted(bookId, chapterId);
    emit booksChanged();
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
    m_currentBookId = book.value("id").toString();
    m_contextChapterId.clear();
    m_contextChapterVolumeId.clear();
    save();
    buildTree();
    emit booksChanged();
}

void BookTree::onNewChapter() {
    if (m_books.isEmpty()) return;
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    int bookIdx = 0;
    if (!m_currentBookId.isEmpty()) {
        const int idx = findBookIndex(m_currentBookId);
        if (idx >= 0) bookIdx = idx;
    }
    QJsonObject book = m_books.at(bookIdx).toObject();

    // 卷：优先当前选中章节所在卷 → 该书第一卷 → 自动新建第一卷
    QString volId = m_contextChapterVolumeId;
    if (volId.isEmpty()) {
        const QJsonArray vols = book.value("volumes").toArray();
        if (!vols.isEmpty()) volId = vols.first().toObject().value("id").toString();
    }
    if (volId.isEmpty()) {
        QJsonObject vol;
        vol["id"] = QStringLiteral("v_%1").arg(QDateTime::currentMSecsSinceEpoch());
        vol["title"] = QStringLiteral("第一卷");
        vol["order"] = 1;
        vol["createdAt"] = now;
        QJsonArray vols = book.value("volumes").toArray();
        vols.append(vol);
        book["volumes"] = vols;
        volId = vol.value("id").toString();
    }

    QJsonArray chapters = book.value("chapters").toArray();
    QJsonObject ch;
    ch["id"] = QStringLiteral("c_%1").arg(QDateTime::currentMSecsSinceEpoch());
    ch["title"] = QStringLiteral("新章节");
    ch["volumeId"] = volId;
    ch["content"] = QStringLiteral("");
    ch["wordCount"] = 0;
    ch["createdAt"] = now;
    ch["updatedAt"] = now;
    chapters.append(ch);
    book["chapters"] = chapters;
    book["updatedAt"] = now;
    m_books.replace(bookIdx, book);
    m_contextBookId = book.value("id").toString();
    m_contextChapterId = ch.value("id").toString();
    m_contextChapterVolumeId = volId;
    save();
    buildTree();
    // 新章节直接打开
    emit chapterSelected(book.value("id").toString(), ch.value("id").toString(),
                         ch.value("title").toString(), QString());
    emit booksChanged();
}

// ── 大纲（对齐 Electron addOutline/updateOutline/renameOutline/deleteOutline）──

QJsonArray BookTree::outlines() const {
    if (m_books.isEmpty()) return QJsonArray();
    return m_books.first().toObject().value("outlines").toArray();
}

QString BookTree::addOutline(const QString &title, const QString &content) {
    if (m_books.isEmpty()) return QString();
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonObject book = m_books.at(0).toObject();
    // 默认卷：当前选中章节所属卷，否则第一卷（对齐 Electron volumeId || story.volumes[0]?.id）
    QString volId = m_contextChapterVolumeId;
    const QJsonArray vols = book.value("volumes").toArray();
    if (volId.isEmpty() && !vols.isEmpty())
        volId = vols.first().toObject().value("id").toString();

    QJsonObject outline;
    outline["id"] = QStringLiteral("o_%1").arg(QDateTime::currentMSecsSinceEpoch());
    outline["title"] = title.trimmed().isEmpty() ? QStringLiteral("新大纲") : title.trimmed();
    outline["volumeId"] = volId;
    outline["content"] = content;
    outline["wordCount"] = bookCountWords(content);
    outline["createdAt"] = now;
    outline["updatedAt"] = now;

    QJsonArray outlines = book.value("outlines").toArray();
    outlines.append(outline);
    book["outlines"] = outlines;
    book["updatedAt"] = now;
    m_books.replace(0, book);
    save();
    emit booksChanged();
    return outline.value("id").toString();
}

void BookTree::updateOutline(const QString &outlineId, const QString &content, int wordCount) {
    if (m_books.isEmpty() || outlineId.isEmpty()) return;
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonObject book = m_books.at(0).toObject();
    QJsonArray outlines = book.value("outlines").toArray();
    bool changed = false;
    for (int i = 0; i < outlines.size(); ++i) {
        QJsonObject o = outlines.at(i).toObject();
        if (o.value("id").toString() == outlineId) {
            o["content"] = content;
            o["wordCount"] = wordCount;
            o["updatedAt"] = now;
            outlines.replace(i, o);
            changed = true;
            break;
        }
    }
    if (!changed) return;
    book["outlines"] = outlines;
    book["updatedAt"] = now;
    m_books.replace(0, book);
    save();
    emit booksChanged();
}

void BookTree::renameOutline(const QString &outlineId, const QString &title) {
    if (m_books.isEmpty() || outlineId.isEmpty()) return;
    const QString newTitle = title.trimmed();
    if (newTitle.isEmpty()) return;
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonObject book = m_books.at(0).toObject();
    QJsonArray outlines = book.value("outlines").toArray();
    for (int i = 0; i < outlines.size(); ++i) {
        QJsonObject o = outlines.at(i).toObject();
        if (o.value("id").toString() == outlineId) {
            o["title"] = newTitle;
            o["updatedAt"] = now;
            outlines.replace(i, o);
            break;
        }
    }
    book["outlines"] = outlines;
    book["updatedAt"] = now;
    m_books.replace(0, book);
    save();
    emit booksChanged();
}

void BookTree::deleteOutline(const QString &outlineId) {
    if (m_books.isEmpty() || outlineId.isEmpty()) return;
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonObject book = m_books.at(0).toObject();
    QJsonArray outlines = book.value("outlines").toArray();
    QJsonObject target;
    QJsonArray filtered;
    for (const auto &v : outlines) {
        const QJsonObject o = v.toObject();
        if (o.value("id").toString() == outlineId) target = o;
        else filtered.append(v);
    }
    if (target.isEmpty()) return;
    // 移入回收站：data 带 _outline:true（对齐 Electron deleteOutline）
    QJsonObject item;
    item["id"] = QStringLiteral("trash_%1").arg(QDateTime::currentMSecsSinceEpoch());
    item["type"] = QStringLiteral("outline");
    item["bookId"] = book.value("id").toString();
    item["volumeId"] = target.value("volumeId").toString();
    item["title"] = target.value("title").toString();
    QJsonObject data = target;
    data["_outline"] = true;
    item["data"] = data;
    item["deletedAt"] = now;
    QJsonArray trash = m_trash;
    trash.prepend(item);
    m_trash = trash;

    book["outlines"] = filtered;
    book["updatedAt"] = now;
    m_books.replace(0, book);
    save();
    emit booksChanged();
}

void BookTree::onContextMenu(const QPoint &pos) {
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (!item) return;
    const QString type = item->data(0, Qt::UserRole + 1).toString();
    const QString bookId = item->parent() ? item->parent()->data(0, Qt::UserRole).toString() : QString();
    if (type == QStringLiteral("book")) {
        m_contextBookId = item->data(0, Qt::UserRole).toString();
        m_contextChapterId.clear();
        m_contextChapterVolumeId.clear();
        QMenu menu(this);
        menu.addAction(QStringLiteral("＋ 新建章节"), this, &BookTree::onNewChapter);
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
        return;
    }
    if (type != QStringLiteral("chapter") || bookId.isEmpty()) return;
    m_contextBookId = bookId;
    m_contextChapterId = item->data(0, Qt::UserRole).toString();
    const QJsonObject book = findBook(bookId);
    for (const auto &cVal : book.value("chapters").toArray()) {
        const QJsonObject ch = cVal.toObject();
        if (ch.value("id").toString() == m_contextChapterId) {
            m_contextChapterVolumeId = ch.value("volumeId").toString();
            break;
        }
    }
    QMenu menu(this);
    menu.addAction(QStringLiteral("✏️ 重命名"), this, &BookTree::onRenameAction);
    menu.addAction(QStringLiteral("🗑 删除章节"), this, &BookTree::onDeleteAction);
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void BookTree::onRenameAction() {
    const QJsonObject book = findBook(m_contextBookId);
    QString oldTitle;
    for (const auto &v : book.value("chapters").toArray()) {
        const QJsonObject c = v.toObject();
        if (c.value("id").toString() == m_contextChapterId) {
            oldTitle = c.value("title").toString();
            break;
        }
    }
    // 中文按钮的简单输入对话框
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("重命名章节"));
    auto *lay = new QVBoxLayout(&dlg);
    auto *edit = new QLineEdit(oldTitle, &dlg);
    edit->selectAll();
    lay->addWidget(edit);
    auto *btnRow = new QHBoxLayout;
    auto *okBtn = new QPushButton(QStringLiteral("确定"), &dlg);
    auto *cancelBtn = new QPushButton(QStringLiteral("取消"), &dlg);
    btnRow->addStretch();
    btnRow->addWidget(okBtn);
    btnRow->addWidget(cancelBtn);
    lay->addLayout(btnRow);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(edit, &QLineEdit::returnPressed, &dlg, &QDialog::accept);
    if (dlg.exec() == QDialog::Accepted) {
        renameChapter(m_contextBookId, m_contextChapterId, edit->text());
    }
}

void BookTree::onDeleteAction() {
    const QJsonObject book = findBook(m_contextBookId);
    QString title;
    for (const auto &v : book.value("chapters").toArray()) {
        const QJsonObject c = v.toObject();
        if (c.value("id").toString() == m_contextChapterId) {
            title = c.value("title").toString();
            break;
        }
    }
    // 中文按钮的确认框（与 Electron ConfirmModal 对应）
    QMessageBox box(QMessageBox::Warning, QStringLiteral("删除章节"),
                    QStringLiteral("确定要删除章节「%1」吗？\n删除后将移入回收站。").arg(title),
                    QMessageBox::NoButton, this);
    auto *yesBtn = box.addButton(QStringLiteral("删除"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(yesBtn);
    box.exec();
    if (box.clickedButton() == yesBtn) {
        deleteChapterInternal(m_contextBookId, m_contextChapterId);
    }
}
