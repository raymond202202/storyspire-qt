#pragma once

#include <QMainWindow>

class BookTree;
class Editor;
class InspirationPanel;
class AiPanel;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    BookTree *m_tree = nullptr;
    Editor *m_editor = nullptr;
    InspirationPanel *m_inspiration = nullptr;
    AiPanel *m_ai = nullptr;
    QLabel *m_statusChapter = nullptr;
    QLabel *m_statusSave = nullptr;

    void createMenus();
};
