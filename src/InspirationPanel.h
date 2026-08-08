#pragma once

#include <QWidget>
#include <QJsonArray>

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QComboBox;
class QPushButton;

/**
 * 右侧灵感库 —— 数据兼容 Electron 版：
 * ~/.config/storyspire-data/inspirations.json（纯数组，格式见 inspirationStore）
 * 分类：character/scene/dialogue/quote/plot/other
 */
class InspirationPanel : public QWidget {
    Q_OBJECT
public:
    explicit InspirationPanel(QWidget *parent = nullptr);

    /** 重新加载 inspirations.json */
    void reload();

private slots:
    void onAdd();
    void onEdit();
    void onDelete();
    void onItemDoubleClicked(QListWidgetItem *item);
    void onContextMenu(const QPoint &pos);
    void onSearchChanged(const QString &text);
    void onFilterChanged(int index);

private:
    QListWidget *m_list = nullptr;
    QLineEdit *m_search = nullptr;
    QComboBox *m_filter = nullptr;
    QPushButton *m_add = nullptr;
    QJsonArray m_inspirations;
    QString m_path;
    QString m_contextId;

    void save();
    void rebuild();
    QString categoryName(const QString &cat) const;
    QString categoryValue(const QString &zhName) const;
    static QString genId();
};
