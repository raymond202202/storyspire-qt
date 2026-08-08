#include "MainWindow.h"
#include "BookTree.h"
#include "Editor.h"
#include "InspirationPanel.h"
#include "AiPanel.h"
#include "OutlinePanel.h"
#include <QSplitter>
#include <QTabWidget>
#include <QStatusBar>
#include <QLabel>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFontDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonObject>
#include <QDir>
#include <QVector>
#include <QPair>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("storyspire-qt"));
    resize(1400, 800);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    m_tree = new BookTree(this);
    m_editor = new Editor(this);
    m_inspiration = new InspirationPanel(this);
    m_ai = new AiPanel(this);
    m_ai->setBookTree(m_tree);
    m_outline = new OutlinePanel(this);
    m_outline->setBookTree(m_tree);

    auto *rightTabs = new QTabWidget(this);
    rightTabs->addTab(m_inspiration, QStringLiteral("💡 灵感"));
    rightTabs->addTab(m_ai, QStringLiteral("✨ AI 助手"));
    rightTabs->addTab(m_outline, QStringLiteral("📋 大纲"));

    splitter->addWidget(m_tree);
    splitter->addWidget(m_editor);
    splitter->addWidget(rightTabs);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setStretchFactor(2, 1);
    splitter->setSizes({260, 780, 300});
    setCentralWidget(splitter);

    // 点章节 → 编辑器加载
    connect(m_tree, &BookTree::chapterSelected, m_editor, &Editor::loadChapter);
    connect(m_tree, &BookTree::chapterSelected, this, [this](const QString &, const QString &,
                                                             const QString &title, const QString &) {
        m_statusChapter->setText(QStringLiteral("当前章节：%1").arg(title));
    });

    // 编辑器写回 → 树落盘（实时保存）
    connect(m_editor, &Editor::contentEdited, m_tree, &BookTree::applyChapterContent);
    // 标题栏编辑 → 重命名章节
    connect(m_editor, &Editor::titleEdited, m_tree, &BookTree::renameChapter);
    // 树侧重命名 → 同步编辑器标题栏
    connect(m_tree, &BookTree::chapterRenamed, m_editor, &Editor::updateTitle);
    // 删除章节 → 清空编辑器
    connect(m_tree, &BookTree::chapterDeleted, m_editor, &Editor::clearChapter);
    // 保存状态 → 状态栏
    connect(m_editor, &Editor::saveStateChanged, this, [this](const QString &text) {
        m_statusSave->setText(text);
    });
    // AI/外部写回章节 → 若为当前打开章节则刷新编辑器
    connect(m_tree, &BookTree::chapterContentApplied, this,
            [this](const QString &bookId, const QString &chapterId) {
                Q_UNUSED(bookId);
                if (chapterId == m_tree->currentChapterId()) {
                    const QJsonObject ch = m_tree->getChapter(chapterId);
                    if (ch.value("success").toBool()) {
                        m_editor->reloadContent(ch.value("content").toString());
                    }
                }
            });

    // 状态栏
    m_statusChapter = new QLabel(QStringLiteral("当前章节：无"), this);
    m_statusSave = new QLabel(QString(), this);
    statusBar()->addWidget(m_statusChapter, 1);
    statusBar()->addPermanentWidget(m_statusSave);
    statusBar()->addPermanentWidget(
        new QLabel(QStringLiteral("storyspire-qt v%1 · ~/.config/storyspire-data").arg(APP_VERSION), this));

    createMenus();
}

void MainWindow::createMenus() {
    // 文件
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    auto *actNewBook = fileMenu->addAction(QStringLiteral("新建书籍"));
    connect(actNewBook, &QAction::triggered, m_tree, &BookTree::newBook);
    auto *actNewChapter = fileMenu->addAction(QStringLiteral("新建章节"));
    actNewChapter->setShortcut(QKeySequence(QStringLiteral("Ctrl+N")));
    connect(actNewChapter, &QAction::triggered, m_tree, &BookTree::newChapter);
    fileMenu->addSeparator();
    auto *actSave = fileMenu->addAction(QStringLiteral("保存当前章节"));
    actSave->setShortcut(QKeySequence::Save);
    connect(actSave, &QAction::triggered, m_editor, &Editor::saveNow);
    fileMenu->addSeparator();

    // 导出（对齐 Electron exporter.ts：txt / docx / doc(RTF) / pdf，整书/单卷/单章/大纲）
    auto *exportMenu = fileMenu->addMenu(QStringLiteral("导出(&X)"));
    // 4 个范围子菜单，每个 4 种格式（txt / docx / doc / pdf）
    const struct { const char *label; Exporter::Format format; } fmtItems[] = {
        { "TXT…", Exporter::Format::Txt },
        { "DOCX…", Exporter::Format::Docx },
        { "DOC…", Exporter::Format::Doc },
        { "PDF…", Exporter::Format::Pdf },
    };
    auto addFormatItems = [this, exportMenu, &fmtItems](const QString &scope, auto trigger) {
        auto *sub = exportMenu->addMenu(scope);
        for (const auto &fi : fmtItems) {
            auto *act = sub->addAction(QStringLiteral("导出为 %1").arg(QString::fromUtf8(fi.label)));
            connect(act, &QAction::triggered, this, [this, trigger, f = fi.format]() {
                (this->*trigger)(f);
            });
        }
        return sub;
    };
    addFormatItems(QStringLiteral("整本书"), &MainWindow::exportWholeBook);
    addFormatItems(QStringLiteral("当前卷"), &MainWindow::exportCurrentVolume);
    addFormatItems(QStringLiteral("当前章节"), &MainWindow::exportCurrentChapter);
    addFormatItems(QStringLiteral("本书大纲"), &MainWindow::exportOutlines);
    fileMenu->addSeparator();

    auto *actQuit = fileMenu->addAction(QStringLiteral("退出"));
    actQuit->setShortcut(QKeySequence::Quit);
    connect(actQuit, &QAction::triggered, this, &QWidget::close);

    // 编辑
    auto *editMenu = menuBar()->addMenu(QStringLiteral("编辑(&E)"));
    auto *actRename = editMenu->addAction(QStringLiteral("重命名章节"));
    actRename->setShortcut(QKeySequence(QStringLiteral("F2")));
    connect(actRename, &QAction::triggered, m_tree, &BookTree::renameCurrentChapter);
    auto *actDelete = editMenu->addAction(QStringLiteral("删除章节"));
    connect(actDelete, &QAction::triggered, m_tree, &BookTree::deleteCurrentChapter);
    editMenu->addSeparator();
    auto *actFont = editMenu->addAction(QStringLiteral("编辑字体…"));
    connect(actFont, &QAction::triggered, this, [this]() {
        bool ok = false;
        const QFont font = QFontDialog::getFont(&ok, m_editor->font(), this, QStringLiteral("选择编辑器字体"));
        if (ok) m_editor->setEditorFont(font);
    });

    // 帮助
    auto *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    auto *actAbout = helpMenu->addAction(QStringLiteral("关于 storyspire-qt"));
    connect(actAbout, &QAction::triggered, this, []() {
        QMessageBox::about(nullptr, QStringLiteral("关于 storyspire-qt"),
                           QStringLiteral("storyspire-qt v%1\n轻量写作工具（Qt 6）\n数据：~/.config/storyspire-data/\nAI：flare 写作专家").arg(APP_VERSION));
    });
}

// ── 导出（对齐 Electron exporter.ts）──────────────────────────────

QVector<QJsonObject> MainWindow::collectChapters(const QJsonObject &book, const QString &volumeId) {
    QVector<QJsonObject> out;
    const QJsonArray chapters = book.value(QStringLiteral("chapters")).toArray();
    for (const auto &c : chapters) {
        const QJsonObject ch = c.toObject();
        if (volumeId.isEmpty() || ch.value(QStringLiteral("volumeId")).toString() == volumeId)
            out.append(ch);
    }
    return out;
}

QPair<QString, QString> MainWindow::exportExtFilter(Exporter::Format format) {
    switch (format) {
    case Exporter::Format::Txt:  return { QStringLiteral(".txt"),  QStringLiteral("文本文件 (*.txt)") };
    case Exporter::Format::Docx: return { QStringLiteral(".docx"), QStringLiteral("Word 文档 (*.docx)") };
    case Exporter::Format::Doc:  return { QStringLiteral(".doc"),  QStringLiteral("Word 文档 (*.doc)") };
    case Exporter::Format::Pdf:  return { QStringLiteral(".pdf"),  QStringLiteral("PDF 文档 (*.pdf)") };
    }
    return { QStringLiteral(".txt"), QStringLiteral("文本文件 (*.txt)") };
}

void MainWindow::doExport(const QString &defaultName, const QString &filter,
                          Exporter::Format format, const QString &title,
                          const QString &author, const QVector<QJsonObject> &chapters) {
    if (chapters.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("导出"),
                                 QStringLiteral("没有可导出的内容（章节为空）"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出"), QDir::homePath() + QLatin1Char('/') + defaultName, filter);
    if (path.isEmpty()) return; // 用户取消

    QString err;
    switch (format) {
    case Exporter::Format::Txt:
        err = Exporter::writeFile(path, Exporter::buildTxt(title, author, chapters));
        break;
    case Exporter::Format::Docx:
        err = Exporter::writeFileBytes(path, Exporter::buildDocx(title, author, chapters));
        break;
    case Exporter::Format::Doc:
        err = Exporter::writeFile(path, Exporter::buildDoc(title, author, chapters));
        break;
    case Exporter::Format::Pdf:
        err = Exporter::writePdf(path, title, author, chapters);
        break;
    }
    if (!err.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), err);
        return;
    }
    statusBar()->showMessage(QStringLiteral("已导出：%1（%2 章）").arg(path).arg(chapters.size()), 8000);
}

void MainWindow::exportWholeBook(Exporter::Format format) {
    const QJsonObject book = m_tree->currentBook();
    if (book.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("导出"), QStringLiteral("还没有书籍"));
        return;
    }
    const QString title = book.value(QStringLiteral("title")).toString();
    const auto extFilter = exportExtFilter(format);
    doExport(Exporter::sanitizeFileName(title) + extFilter.first, extFilter.second,
             format, title, book.value(QStringLiteral("author")).toString(),
             collectChapters(book, QString()));
}

void MainWindow::exportCurrentVolume(Exporter::Format format) {
    const QJsonObject book = m_tree->currentBook();
    if (book.isEmpty()) return;
    const QString volId = m_tree->currentVolumeId();
    if (volId.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("导出"),
                                 QStringLiteral("请先在左侧选中一个章节（以确定所属卷）"));
        return;
    }
    // 卷标题
    QString volTitle = volId;
    const QJsonArray vols = book.value(QStringLiteral("volumes")).toArray();
    for (const auto &v : vols) {
        const QJsonObject vo = v.toObject();
        if (vo.value(QStringLiteral("id")).toString() == volId) {
            volTitle = vo.value(QStringLiteral("title")).toString();
            break;
        }
    }
    const QString title = book.value(QStringLiteral("title")).toString();
    const auto extFilter = exportExtFilter(format);
    doExport(Exporter::sanitizeFileName(QStringLiteral("%1 - %2%3").arg(title, volTitle, extFilter.first)),
             extFilter.second, format,
             QStringLiteral("%1 - %2").arg(title, volTitle),
             book.value(QStringLiteral("author")).toString(), collectChapters(book, volId));
}

void MainWindow::exportCurrentChapter(Exporter::Format format) {
    const QJsonObject book = m_tree->currentBook();
    if (book.isEmpty()) return;
    const QString chId = m_tree->currentChapterId();
    if (chId.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("导出"),
                                 QStringLiteral("请先在左侧选中一个章节"));
        return;
    }
    const QVector<QJsonObject> all = collectChapters(book, QString());
    for (const auto &ch : all) {
        if (ch.value(QStringLiteral("id")).toString() == chId) {
            const QString title = book.value(QStringLiteral("title")).toString();
            const QString chTitle = ch.value(QStringLiteral("title")).toString();
            const auto extFilter = exportExtFilter(format);
            doExport(Exporter::sanitizeFileName(QStringLiteral("%1 - %2%3").arg(title, chTitle, extFilter.first)),
                     extFilter.second, format,
                     QStringLiteral("%1 - %2").arg(title, chTitle),
                     book.value(QStringLiteral("author")).toString(), QVector<QJsonObject>{ch});
            return;
        }
    }
    QMessageBox::information(this, QStringLiteral("导出"), QStringLiteral("未找到选中章节"));
}

void MainWindow::exportOutlines(Exporter::Format format) {
    const QJsonObject book = m_tree->currentBook();
    if (book.isEmpty()) return;
    const QJsonArray outlines = book.value(QStringLiteral("outlines")).toArray();
    if (outlines.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("导出"),
                                 QStringLiteral("这本书还没有大纲"));
        return;
    }
    QVector<QJsonObject> list;
    for (const auto &o : outlines) list.append(o.toObject());
    const QString title = book.value(QStringLiteral("title")).toString();
    const auto extFilter = exportExtFilter(format);
    doExport(Exporter::sanitizeFileName(QStringLiteral("%1 - 大纲%2").arg(title, extFilter.first)),
             extFilter.second,
             format, QStringLiteral("%1 - 大纲").arg(title),
             book.value(QStringLiteral("author")).toString(), list);
}
