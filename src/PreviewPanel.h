#pragma once

#include <QWidget>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>

class QComboBox;
class QSpinBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QTimer;

/**
 * 实时预览面板 —— 手机形阅读视图（对齐 Electron PreviewPanel）
 * - 机型选择：6 个预设（iPhone 17 Pro Max / iPhone 17 / 16 / 14 / Galaxy S24 / Pixel 9）
 *   + 自定义尺寸（可保存命名）+ 已保存自定义机型（QSettings 持久化，对齐 localStorage）
 * - 实时渲染当前章节/草稿（草稿优先：编辑中的未保存内容也实时显示）
 * - 手机壳按机型宽高比绘制：灵动岛 / 状态栏时间 / 标题 / 正文（纯文本，HTML 剥离）/ 字数 / Home 条
 */
class PreviewPanel : public QWidget {
    Q_OBJECT
public:
    explicit PreviewPanel(QWidget *parent = nullptr);

    struct Device {
        QString id;
        QString name;
        int width = 0;        // 逻辑分辨率宽
        int height = 0;       // 逻辑分辨率高
        int displayWidth = 0; // 预览显示宽度（px）
    };

    /** 预设手机型号（对齐 Electron PHONE_PRESETS） */
    static const QVector<Device> &presets();
    /** HTML → 纯文本（对齐 Electron htmlToPlain，兼容旧 HTML 数据） */
    static QString htmlToPlain(const QString &html);
    /** 字数统计（中文字符数 + 英文单词数，与 Editor 一致） */
    static int countWords(const QString &text);

public slots:
    /** 实时预览（标题 + 内容；内容可为 HTML/纯文本，内部剥离标签） */
    void setPreview(const QString &title, const QString &content);
    /** 无活动内容时显示空态 */
    void setEmpty();

private slots:
    void onDeviceChanged(int index);
    void onCustomSizeChanged();
    void saveCustomDevice();
    void deleteCustomDevice();

private:
    QJsonArray loadCustomDevices() const;
    void storeCustomDevices(const QJsonArray &list);
    /** 重建机型下拉（保留当前选择），并在末尾追加「自定义尺寸」 */
    void rebuildDeviceCombo();
    /** 根据当前机型选择刷新手机壳宽高比与显示宽度 */
    void applyDevice();
    /** 刷新手机壳内容显示 */
    void refreshPhone();
    void updateFrameSize();
    void refreshClock();

    QComboBox *m_device = nullptr;
    QWidget *m_customRow = nullptr;
    QSpinBox *m_customW = nullptr;
    QSpinBox *m_customH = nullptr;
    QWidget *m_customNameRow = nullptr;
    QLineEdit *m_customName = nullptr;
    QWidget *m_savedCustomRow = nullptr;
    QLabel *m_savedCustomLabel = nullptr;
    QPushButton *m_savedCustomDel = nullptr;
    QString m_savedCustomId; // 当前选中的已保存自定义机型 id（空=未选中）
    QJsonArray m_customDevices;

    // ── 手机壳 ──
    QWidget *m_phoneFrame = nullptr; // PhoneFrame（内部类，自绘外壳）
    QTimer *m_clock = nullptr;

    int m_devW = 393;
    int m_devH = 852;
    int m_displayW = 290;
    QString m_title;
    QString m_contentText;

protected:
    void resizeEvent(QResizeEvent *event) override;
};
