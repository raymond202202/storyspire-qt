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

    statusBar()->addWidget(new QLabel(QStringLiteral("storyspire-qt v%1 · 数据: ~/.config/storyspire-data/books.json").arg(APP_VERSION), this));
}
