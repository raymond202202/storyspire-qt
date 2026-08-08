#pragma once

#include <QMainWindow>
#include <QVector>
#include <QJsonObject>
#include "Exporter.h"

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

    // ── 导出（对齐 Electron exporter.ts：txt / doc(RTF)，整书/单卷/单章/大纲）──
    /** 收集书籍章节（volumeId 为空=全部，否则按卷过滤） */
    static QVector<QJsonObject> collectChapters(const QJsonObject &book, const QString &volumeId);
    /** 导出通用流程：弹保存对话框 + 写文件 + 状态栏/错误提示 */
    void doExport(const QString &defaultName, const QString &filter,
                  Exporter::Format format, const QString &title,
                  const QString &author, const QVector<QJsonObject> &chapters);
    void exportWholeBook(Exporter::Format format);
    void exportCurrentVolume();
    void exportCurrentChapter();
    void exportOutlines(Exporter::Format format);
};
