#include "MainWindow.h"
#include "BookTree.h"
#include "Editor.h"
#include <QSplitter>
#include <QStatusBar>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("storyspire-qt"));
    resize(1200, 800);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    m_tree = new BookTree(this);
    m_editor = new Editor(this);
    splitter->addWidget(m_tree);
    splitter->addWidget(m_editor);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    setCentralWidget(splitter);

    // 点章节 → 编辑器加载
    connect(m_tree, &BookTree::chapterSelected, m_editor, &Editor::loadChapter);

    // 编辑器写回 → 树落盘（实时保存）
    connect(m_editor, &Editor::contentEdited, m_tree, &BookTree::applyChapterContent);
    // 标题栏编辑 → 重命名章节
    connect(m_editor, &Editor::titleEdited, m_tree, &BookTree::renameChapter);
    // 树侧重命名 → 同步编辑器标题栏
    connect(m_tree, &BookTree::chapterRenamed, m_editor, &Editor::updateTitle);
    // 删除章节 → 清空编辑器
    connect(m_tree, &BookTree::chapterDeleted, m_editor, &Editor::clearChapter);

    statusBar()->addWidget(new QLabel(QStringLiteral("storyspire-qt v%1 · 数据: ~/.config/storyspire-data/books.json").arg(APP_VERSION), this));
}
