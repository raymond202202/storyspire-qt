#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("storyspire-qt");
    app.setApplicationVersion(APP_VERSION);
    MainWindow w;
    w.show();
    return app.exec();
}
