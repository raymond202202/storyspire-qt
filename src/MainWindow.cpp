#include "MainWindow.h"
#include "BookTree.h"
#include "Editor.h"
#include "InspirationPanel.h"
#include "AiPanel.h"
#include <QSplitter>
#include <QTabWidget>
#include <QStatusBar>
#include <QLabel>

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

    // 编辑器写回 → 树落盘（实时保存）
    connect(m_editor, &Editor::contentEdited, m_tree, &BookTree::applyChapterContent);
    // 标题栏编辑 → 重命名章节
    connect(m_editor, &Editor::titleEdited, m_tree, &BookTree::renameChapter);
    // 树侧重命名 → 同步编辑器标题栏
    connect(m_tree, &BookTree::chapterRenamed, m_editor, &Editor::updateTitle);
    // 删除章节 → 清空编辑器
    connect(m_tree, &BookTree::chapterDeleted, m_editor, &Editor::clearChapter);
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

    statusBar()->addWidget(new QLabel(QStringLiteral("storyspire-qt v%1 · 数据: ~/.config/storyspire-data/books.json").arg(APP_VERSION), this));
}
