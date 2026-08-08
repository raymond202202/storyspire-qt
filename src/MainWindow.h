#pragma once

#include <QMainWindow>

class BookTree;
class Editor;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    BookTree *m_tree = nullptr;
    Editor *m_editor = nullptr;
};
