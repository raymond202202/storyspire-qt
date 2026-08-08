#include "MainWindow.h"
#include "BookTree.h"
#include "Editor.h"
#include "InspirationPanel.h"
#include "AiPanel.h"
#include <QSplitter>
#include <QTabWidget>
#include <QStatusBar>
#include <QLabel>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFontDialog>
#include <QMessageBox>
#include <QJsonObject>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("storyspire-qt"));
    resize(1400, 800);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    m_tree = new BookTree(this);
    m_editor = new Editor(this);
    m_inspiration = new InspirationPanel(this);
    m_ai = new AiPanel(this);
    m_ai->setBookTree(m_tree);

    auto *rightTabs = new QTabWidget(this);
    rightTabs->addTab(m_inspiration, QStringLiteral("💡 灵感"));
    rightTabs->addTab(m_ai, QStringLiteral("✨ AI 助手"));

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
