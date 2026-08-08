#include <QApplication>
#include "MainWindow.h"

namespace {
// 浅色主题：白底紫配 #6d4aff
const char *kStyleSheet = R"QSS(
* { font-size: 13px; }
QMainWindow, QWidget { background: #ffffff; color: #2c2c34; }
QWidget#BookTree, QWidget#InspirationPanel, QWidget#AiPanel { background: #fafaff; }

QPushButton {
    background: #ffffff; color: #3a3a46; border: 1px solid #dcdcec;
    border-radius: 5px; padding: 4px 12px;
}
QPushButton:hover { background: #f3f0ff; border-color: #6d4aff; }
QPushButton:pressed { background: #e6e0ff; }
QPushButton:disabled { color: #b8b8c8; background: #f5f5f8; border-color: #e8e8f0; }

QLineEdit, QTextEdit, QComboBox {
    background: #ffffff; border: 1px solid #dcdcec; border-radius: 5px; padding: 4px 6px;
    selection-background-color: #6d4aff; selection-color: #ffffff;
}
QLineEdit:focus, QTextEdit:focus, QComboBox:focus { border-color: #6d4aff; }

QTreeWidget, QListWidget {
    background: #ffffff; border: 1px solid #e4e4ef; border-radius: 6px;
    outline: 0;
}
QTreeWidget::item, QListWidget::item { padding: 3px 4px; border-radius: 4px; }
QTreeWidget::item:hover, QListWidget::item:hover { background: #f5f2ff; }
QTreeWidget::item:selected, QListWidget::item:selected {
    background: #e9e2ff; color: #4a2bd0;
}

QTabWidget::pane { border: 1px solid #e4e4ef; border-radius: 6px; top: -1px; }
QTabBar::tab {
    background: #f2f2f8; color: #6a6a78; border: 1px solid #e4e4ef;
    border-bottom: none; border-top-left-radius: 6px; border-top-right-radius: 6px;
    padding: 5px 14px; margin-right: 2px;
}
QTabBar::tab:selected { background: #ffffff; color: #6d4aff; font-weight: 600; }
QTabBar::tab:hover:!selected { background: #f7f4ff; }

QMenu { background: #ffffff; border: 1px solid #dcdcec; border-radius: 6px; padding: 4px; }
QMenu::item { padding: 5px 22px 5px 14px; border-radius: 4px; }
QMenu::item:selected { background: #e9e2ff; color: #4a2bd0; }
QMenu::separator { height: 1px; background: #e8e8f0; margin: 4px 8px; }

QMenuBar { background: #ffffff; }
QMenuBar::item { padding: 5px 10px; border-radius: 4px; }
QMenuBar::item:selected { background: #e9e2ff; }

QStatusBar { background: #fafaff; border-top: 1px solid #e4e4ef; color: #6a6a78; }
QStatusBar::item { border: none; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #d8d8e8; border-radius: 4px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: #6d4aff; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: #d8d8e8; border-radius: 4px; min-width: 30px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QToolTip { background: #2c2c34; color: #ffffff; border: none; padding: 4px 8px; border-radius: 4px; }
)QSS";
} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("storyspire-qt");
    app.setApplicationVersion(APP_VERSION);
    app.setStyleSheet(QString::fromUtf8(kStyleSheet));
    MainWindow w;
    w.show();
    return app.exec();
}
