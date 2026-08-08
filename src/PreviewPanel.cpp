#include "PreviewPanel.h"
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QSettings>
#include <QRegularExpression>
#include <QPainter>
#include <QTime>
#include <QDateTime>
#include <QFrame>

namespace {

// ── 手机壳（自绘外壳 + 屏幕内控件）──
// 对齐 Electron .phone-frame/.phone-screen/.phone-island/.phone-statusbar/.phone-content
class PhoneFrame : public QWidget {
public:
    explicit PhoneFrame(QWidget *parent = nullptr) : QWidget(parent) {
        m_screen = new QWidget(this);
        m_screen->setStyleSheet(QStringLiteral(
            "background: #ffffff; border-radius: 36px;"));

        m_island = new QLabel(m_screen);
        m_island->setFixedHeight(24);
        m_island->setStyleSheet(QStringLiteral(
            "background: #000; border-radius: 12px;"));

        m_time = new QLabel(m_screen);
        m_time->setStyleSheet(QStringLiteral(
            "color: #222; font-size: 10px; font-weight: 600; background: transparent;"));

        m_icons = new QLabel(m_screen);
        m_icons->setText(QStringLiteral("📶 🔋"));
        m_icons->setStyleSheet(QStringLiteral(
            "color: #222; font-size: 9px; background: transparent;"));

        m_title = new QLabel(m_screen);
        m_title->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        m_title->setWordWrap(true);
        m_title->setStyleSheet(QStringLiteral(
            "color: #111; font-size: 15px; font-weight: 600; background: transparent;"
            "padding: 0 4px;"));

        m_content = new QTextEdit(m_screen);
        m_content->setReadOnly(true);
        m_content->setFrameShape(QFrame::NoFrame);
        m_content->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_content->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_content->viewport()->setAutoFillBackground(false);
        m_content->setStyleSheet(QStringLiteral(
            "QTextEdit { background: transparent; border: none;"
            " font-family: Georgia, 'Noto Serif SC', serif; font-size: 16px;"
            " color: #333; }"));

        m_wordCount = new QLabel(m_screen);
        m_wordCount->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_wordCount->setStyleSheet(QStringLiteral(
            "color: #999; font-size: 10px; background: transparent;"));

        m_homeBar = new QLabel(m_screen);
        m_homeBar->setFixedHeight(4);
        m_homeBar->setStyleSheet(QStringLiteral(
            "background: rgba(0,0,0,0.22); border-radius: 2px;"));

        m_empty = new QLabel(m_screen);
        m_empty->setAlignment(Qt::AlignCenter);
        m_empty->setWordWrap(true);
        m_empty->setStyleSheet(QStringLiteral(
            "color: #bbb; font-size: 13px; line-height: 1.6; background: transparent;"));
        m_empty->hide();

        showEmpty();
    }

    void setDevice(int w, int h, int displayW) {
        m_devW = w;
        m_devH = h;
        m_displayW = displayW;
        updateSize();
    }
    void setTitle(const QString &t) {
        m_title->setText(t);
        updateSize(); // 标题换行后重排正文区域
    }
    void setPlainContent(const QString &text) { m_content->setPlainText(text); }
    void setWordCount(int n) { m_wordCount->setText(QStringLiteral("%1 字").arg(n)); }
    void setTime(const QString &t) { m_time->setText(t); }
    void showContent() {
        m_title->show();
        m_content->show();
        m_wordCount->show();
        m_homeBar->show();
        m_empty->hide();
    }
    void showEmpty() {
        m_title->hide();
        m_content->hide();
        m_wordCount->hide();
        m_homeBar->hide();
        m_empty->show();
    }
    void setEmptyText(const QString &t) { m_empty->setText(t); }

    QSize sizeHint() const override {
        return QSize(m_displayW, m_displayW * m_devH / qMax(1, m_devW));
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#1c1c1e")));
        p.drawRoundedRect(rect(), 44, 44);
        // 内描边（对齐 inset 0 0 0 2px rgba(255,255,255,.08)）
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 22), 2));
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 43, 43);
    }

    void resizeEvent(QResizeEvent *) override { updateSize(); }

private:
    void updateSize() {
        const int pad = 10; // 外壳内边距（对齐 padding: 10px）
        const QRect scr(pad, pad, qMax(1, width() - 2 * pad), qMax(1, height() - 2 * pad));
        m_screen->setGeometry(scr);
        const int w = scr.width();
        const int h = scr.height();

        // 灵动岛（对齐 top:9px / width:28% / height:24px / border-radius:14px）
        const int islandW = w * 28 / 100;
        m_island->setGeometry((w - islandW) / 2, 9, islandW, 24);

        // 状态栏（时间左 / 图标右）
        m_time->setGeometry(18, 12, 80, 20);
        m_icons->setGeometry(w - 70, 12, 60, 20);

        // 标题（居中，按文本换行自适应高度）
        const int titleW = w - 28;
        const int titleH = qMax(20, m_title->heightForWidth(titleW) + 4);
        m_title->setGeometry(14, 42, titleW, titleH);

        // 正文（可滚动，占剩余空间；底部预留字数 + Home 条）
        m_content->setGeometry(14, 42 + titleH + 8, titleW, qMax(40, h - (42 + titleH + 8) - 36));
        m_wordCount->setGeometry(14, h - 28, titleW, 18);
        m_homeBar->setGeometry((w - 120) / 2, h - 15, 120, 4);
        m_empty->setGeometry(14, 42 + 20, titleW, qMax(40, h - 42 - 60));
    }

    QWidget *m_screen = nullptr;
    QLabel *m_island = nullptr;
    QLabel *m_time = nullptr;
    QLabel *m_icons = nullptr;
    QLabel *m_title = nullptr;
    QTextEdit *m_content = nullptr;
    QLabel *m_wordCount = nullptr;
    QLabel *m_homeBar = nullptr;
    QLabel *m_empty = nullptr;
    int m_devW = 393;
    int m_devH = 852;
    int m_displayW = 290;
};

} // namespace

// ── 预设机型（对齐 Electron PHONE_PRESETS）──
const QVector<PreviewPanel::Device> &PreviewPanel::presets() {
    static const QVector<Device> list = {
        { QStringLiteral("iphone17pm"), QStringLiteral("iPhone 17 Pro Max"), 440, 956, 310 },
        { QStringLiteral("iphone17"),   QStringLiteral("iPhone 17 / 16 Pro"), 402, 874, 300 },
        { QStringLiteral("iphone16"),   QStringLiteral("iPhone 16 / 15"), 393, 852, 290 },
        { QStringLiteral("iphone14"),   QStringLiteral("iPhone 14 / 13"), 390, 844, 285 },
        { QStringLiteral("galaxys24"),  QStringLiteral("Galaxy S24"), 384, 832, 280 },
        { QStringLiteral("pixel9"),     QStringLiteral("Pixel 9"), 412, 892, 295 },
    };
    return list;
}

// ── HTML → 纯文本（对齐 Electron htmlToPlain）──
QString PreviewPanel::htmlToPlain(const QString &html) {
    const QRegularExpression::PatternOptions ci = QRegularExpression::CaseInsensitiveOption;
    QString s = html;
    s.replace(QRegularExpression(QStringLiteral("<h[1-6][^>]*>"), ci), QStringLiteral("\n\n"));
    s.replace(QRegularExpression(QStringLiteral("</h[1-6]>"), ci), QStringLiteral("\n\n"));
    s.replace(QRegularExpression(QStringLiteral("<p[^>]*>"), ci), QStringLiteral("\n\n"));
    s.replace(QRegularExpression(QStringLiteral("<br\\s*/?>"), ci), QStringLiteral("\n"));
    s.replace(QRegularExpression(QStringLiteral("<[^>]+>")), QString());
    s.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    s.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    s.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    s.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    s.replace(QRegularExpression(QStringLiteral("\\n{3,}")), QStringLiteral("\n\n"));
    return s.trimmed();
}

// ── 字数统计（与 Editor::countWords 一致：中文 + 英文单词）──
int PreviewPanel::countWords(const QString &text) {
    const int zh = text.count(QRegularExpression(QStringLiteral("[\\x{4e00}-\\x{9fff}\\x{3400}-\\x{4dbf}]")));
    const int en = text.split(QRegularExpression(QStringLiteral("[^a-zA-Z]+")), Qt::SkipEmptyParts).size();
    return zh + en;
}

PreviewPanel::PreviewPanel(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // ── 头部：标题 + 机型选择 ──
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("📱 预览"), this);
    title->setStyleSheet(QStringLiteral("font-weight: 600; color: #333;"));
    header->addWidget(title);
    m_device = new QComboBox(this);
    m_device->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    header->addWidget(m_device, 1);
    layout->addLayout(header);

    // ── 自定义尺寸行（仅「自定义尺寸」时显示）──
    m_customRow = new QWidget(this);
    auto *cr = new QHBoxLayout(m_customRow);
    cr->setContentsMargins(0, 0, 0, 0);
    cr->setSpacing(4);
    m_customW = new QSpinBox(this);
    m_customW->setRange(200, 600);
    m_customW->setValue(375);
    m_customH = new QSpinBox(this);
    m_customH->setRange(400, 1200);
    m_customH->setValue(812);
    auto *saveBtn = new QPushButton(QStringLiteral("＋"), this);
    saveBtn->setToolTip(QStringLiteral("保存为自定义机型"));
    saveBtn->setFixedWidth(26);
    cr->addWidget(m_customW);
    cr->addWidget(new QLabel(QStringLiteral("×"), this));
    cr->addWidget(m_customH);
    cr->addWidget(new QLabel(QStringLiteral("px"), this));
    cr->addStretch(1);
    cr->addWidget(saveBtn);
    m_customRow->hide();
    layout->addWidget(m_customRow);

    // ── 保存命名行（点「＋」后出现）──
    m_customNameRow = new QWidget(this);
    auto *nr = new QHBoxLayout(m_customNameRow);
    nr->setContentsMargins(0, 0, 0, 0);
    nr->setSpacing(4);
    m_customName = new QLineEdit(this);
    m_customName->setPlaceholderText(QStringLiteral("机型名（如：我的平板）"));
    auto *confirmBtn = new QPushButton(QStringLiteral("✓"), this);
    confirmBtn->setToolTip(QStringLiteral("保存"));
    confirmBtn->setFixedWidth(26);
    nr->addWidget(m_customName, 1);
    nr->addWidget(confirmBtn);
    m_customNameRow->hide();
    layout->addWidget(m_customNameRow);

    // ── 已保存自定义机型行 ──
    m_savedCustomRow = new QWidget(this);
    auto *sr = new QHBoxLayout(m_savedCustomRow);
    sr->setContentsMargins(0, 0, 0, 0);
    sr->setSpacing(4);
    m_savedCustomLabel = new QLabel(this);
    m_savedCustomDel = new QPushButton(QStringLiteral("🗑"), this);
    m_savedCustomDel->setToolTip(QStringLiteral("删除此机型"));
    m_savedCustomDel->setFixedWidth(26);
    sr->addWidget(m_savedCustomLabel, 1);
    sr->addWidget(m_savedCustomDel);
    m_savedCustomRow->hide();
    layout->addWidget(m_savedCustomRow);

    // ── 手机壳（居中）──
    m_phoneFrame = new PhoneFrame(this);
    auto *centerBox = new QVBoxLayout;
    centerBox->addStretch(1);
    auto *hbox = new QHBoxLayout;
    hbox->addStretch(1);
    hbox->addWidget(m_phoneFrame);
    hbox->addStretch(1);
    centerBox->addLayout(hbox);
    centerBox->addStretch(1);
    layout->addLayout(centerBox, 1);

    // ── 状态栏时钟（每 30s 刷新，对齐 Electron 实时时间）──
    m_clock = new QTimer(this);
    m_clock->setInterval(30000);
    connect(m_clock, &QTimer::timeout, this, [this]() { refreshClock(); });
    m_clock->start();
    refreshClock();

    // ── 数据与信号 ──
    m_customDevices = loadCustomDevices();
    rebuildDeviceCombo();

    connect(m_device, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PreviewPanel::onDeviceChanged);
    connect(m_customW, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PreviewPanel::onCustomSizeChanged);
    connect(m_customH, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PreviewPanel::onCustomSizeChanged);
    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        m_customNameRow->setVisible(!m_customNameRow->isVisible());
        if (m_customNameRow->isVisible()) m_customName->setFocus();
    });
    connect(m_customName, &QLineEdit::returnPressed, this, &PreviewPanel::saveCustomDevice);
    connect(confirmBtn, &QPushButton::clicked, this, &PreviewPanel::saveCustomDevice);
    connect(m_savedCustomDel, &QPushButton::clicked, this, &PreviewPanel::deleteCustomDevice);

    setEmpty();
}

QJsonArray PreviewPanel::loadCustomDevices() const {
    QSettings settings;
    const QByteArray raw = settings.value(QStringLiteral("preview/customDevices")).toByteArray();
    if (raw.isEmpty()) return {};
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) return {};
    return doc.array();
}

void PreviewPanel::storeCustomDevices(const QJsonArray &list) {
    QSettings settings;
    settings.setValue(QStringLiteral("preview/customDevices"),
                      QJsonDocument(list).toJson(QJsonDocument::Compact));
}

void PreviewPanel::rebuildDeviceCombo() {
    const QString cur = m_device ? m_device->currentData().toString()
                                 : QStringLiteral("iphone17pm");
    m_device->blockSignals(true);
    m_device->clear();
    for (const Device &d : presets())
        m_device->addItem(d.name, d.id);
    for (const auto &v : m_customDevices) {
        const QJsonObject o = v.toObject();
        m_device->addItem(QStringLiteral("📱 %1").arg(o.value(QStringLiteral("name")).toString()),
                          o.value(QStringLiteral("id")).toString());
    }
    m_device->addItem(QStringLiteral("自定义尺寸"), QStringLiteral("custom"));
    const int idx = m_device->findData(cur);
    m_device->setCurrentIndex(idx >= 0 ? idx : 0);
    m_device->blockSignals(false);
    onDeviceChanged(m_device->currentIndex());
}

void PreviewPanel::onDeviceChanged(int) {
    const QString id = m_device->currentData().toString();
    const bool isCustom = (id == QStringLiteral("custom"));
    m_customRow->setVisible(isCustom);

    // 已保存自定义机型行
    QString savedName;
    int sw = 0, sh = 0;
    m_savedCustomId.clear();
    for (const auto &v : m_customDevices) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("id")).toString() == id) {
            m_savedCustomId = id;
            savedName = o.value(QStringLiteral("name")).toString();
            sw = o.value(QStringLiteral("width")).toInt();
            sh = o.value(QStringLiteral("height")).toInt();
            break;
        }
    }
    m_savedCustomRow->setVisible(!savedName.isEmpty());
    if (!savedName.isEmpty()) {
        m_savedCustomLabel->setText(QStringLiteral("📱 %1（%2×%3）").arg(savedName).arg(sw).arg(sh));
    }
    applyDevice();
}

void PreviewPanel::applyDevice() {
    const QString id = m_device->currentData().toString();
    // 预设
    for (const Device &d : presets()) {
        if (d.id == id) {
            m_devW = d.width;
            m_devH = d.height;
            m_displayW = d.displayWidth;
            updateFrameSize();
            return;
        }
    }
    // 已保存自定义机型（显示宽度对齐 Electron：min(310, max(240, w*0.7))）
    for (const auto &v : m_customDevices) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("id")).toString() == id) {
            m_devW = o.value(QStringLiteral("width")).toInt();
            m_devH = o.value(QStringLiteral("height")).toInt();
            m_displayW = qMin(310, qMax(240, int(m_devW * 0.7)));
            updateFrameSize();
            return;
        }
    }
    // 自定义尺寸（编辑中）
    m_devW = m_customW->value();
    m_devH = m_customH->value();
    m_displayW = qMin(310, qMax(240, int(m_devW * 0.7)));
    updateFrameSize();
}

void PreviewPanel::onCustomSizeChanged() {
    if (m_device->currentData().toString() != QStringLiteral("custom")) return;
    applyDevice();
}

void PreviewPanel::saveCustomDevice() {
    const QString name = m_customName->text().trimmed();
    if (name.isEmpty()) return;
    QJsonObject o;
    o.insert(QStringLiteral("id"), QStringLiteral("custom-%1").arg(QDateTime::currentMSecsSinceEpoch()));
    o.insert(QStringLiteral("name"), name);
    o.insert(QStringLiteral("width"), m_customW->value());
    o.insert(QStringLiteral("height"), m_customH->value());
    m_customDevices.append(o);
    storeCustomDevices(m_customDevices);
    m_customName->clear();
    m_customNameRow->hide();
    rebuildDeviceCombo();
    m_device->setCurrentIndex(m_device->findData(o.value(QStringLiteral("id")).toString()));
}

void PreviewPanel::deleteCustomDevice() {
    if (m_savedCustomId.isEmpty()) return;
    QJsonArray list;
    for (const auto &v : m_customDevices) {
        if (v.toObject().value(QStringLiteral("id")).toString() != m_savedCustomId)
            list.append(v);
    }
    m_customDevices = list;
    storeCustomDevices(list);
    rebuildDeviceCombo();
    m_device->setCurrentIndex(0);
}

void PreviewPanel::setPreview(const QString &title, const QString &content) {
    m_title = title;
    m_contentText = content;
    refreshPhone();
}

void PreviewPanel::setEmpty() {
    m_title.clear();
    m_contentText.clear();
    refreshPhone();
}

void PreviewPanel::refreshPhone() {
    auto *frame = static_cast<PhoneFrame *>(m_phoneFrame);
    const QString plain = htmlToPlain(m_contentText);
    frame->setTitle(m_title);
    if (plain.isEmpty()) {
        frame->showEmpty();
        frame->setEmptyText(
            m_title.isEmpty() ? QStringLiteral("还没有内容\n在中间写作区写下灵感\n或打开一个章节")
                              : QStringLiteral("（空白章节，开始写作吧）"));
        frame->setWordCount(0);
    } else {
        frame->showContent();
        frame->setPlainContent(plain);
        frame->setWordCount(countWords(plain));
    }
}

void PreviewPanel::updateFrameSize() {
    if (!m_phoneFrame) return;
    auto *frame = static_cast<PhoneFrame *>(m_phoneFrame);
    const int availW = qMax(220, width() - 40);
    const int w = qMin(availW, m_displayW);
    const int h = w * m_devH / qMax(1, m_devW);
    frame->setFixedSize(w, h);
    frame->setDevice(m_devW, m_devH, m_displayW);
}

void PreviewPanel::refreshClock() {
    auto *frame = static_cast<PhoneFrame *>(m_phoneFrame);
    if (frame) frame->setTime(QTime::currentTime().toString(QStringLiteral("HH:mm")));
}

void PreviewPanel::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateFrameSize();
}
