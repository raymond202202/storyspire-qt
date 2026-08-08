#include "InspirationPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QMessageBox>
#include <QDateTime>
#include <QMenu>
#include <QDialog>
#include <QTextEdit>
#include <QLabel>
#include <QRegularExpression>

namespace {
const char *kCategories[] = {"character", "scene", "dialogue", "quote", "plot", "other"};
const char *kCategoryZh[] = {"人物", "场景", "对话", "名言", "情节", "其他"};
}

InspirationPanel::InspirationPanel(QWidget *parent) : QWidget(parent) {
    const QString configDir = QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"));
    m_path = (configDir.isEmpty() ? QDir::homePath() + "/.config" : configDir)
             + "/storyspire-data/inspirations.json";

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // 标题行
    auto *head = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("💡 灵感库"), this);
    head->addWidget(title);
    head->addStretch();
    m_add = new QPushButton(QStringLiteral("＋"), this);
    m_add->setFixedWidth(26);
    m_add->setToolTip(QStringLiteral("新建灵感"));
    head->addWidget(m_add);
    layout->addLayout(head);

    // 搜索
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("搜索灵感…"));
    layout->addWidget(m_search);

    // 分类筛选
    m_filter = new QComboBox(this);
    m_filter->addItem(QStringLiteral("全部分类"), QStringLiteral("all"));
    for (int i = 0; i < 6; ++i) {
        m_filter->addItem(QStringLiteral("%1").arg(QString::fromUtf8(kCategoryZh[i])),
                          QString::fromLatin1(kCategories[i]));
    }
    layout->addWidget(m_filter);

    // 列表
    m_list = new QListWidget(this);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setWordWrap(true);
    layout->addWidget(m_list, 1);

    connect(m_add, &QPushButton::clicked, this, &InspirationPanel::onAdd);
    connect(m_search, &QLineEdit::textChanged, this, &InspirationPanel::onSearchChanged);
    connect(m_filter, &QComboBox::currentIndexChanged, this, &InspirationPanel::onFilterChanged);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &InspirationPanel::onItemDoubleClicked);
    connect(m_list, &QListWidget::customContextMenuRequested, this, &InspirationPanel::onContextMenu);

    reload();
}

void InspirationPanel::reload() {
    m_inspirations = QJsonArray();
    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isArray()) {
            m_inspirations = doc.array();
        } else if (doc.isObject()) {
            // 兼容 { inspirations: [...] } 包装
            m_inspirations = doc.object().value("inspirations").toArray();
        }
        f.close();
    }
    rebuild();
}

void InspirationPanel::save() {
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile f(m_path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(m_inspirations).toJson(QJsonDocument::Indented));
        f.commit();
    }
}

QString InspirationPanel::categoryName(const QString &cat) const {
    for (int i = 0; i < 6; ++i) {
        if (cat == QLatin1String(kCategories[i])) return QString::fromUtf8(kCategoryZh[i]);
    }
    return QStringLiteral("其他");
}

QString InspirationPanel::categoryValue(const QString &zhName) const {
    for (int i = 0; i < 6; ++i) {
        if (zhName == QString::fromUtf8(kCategoryZh[i])) return QString::fromLatin1(kCategories[i]);
    }
    return QStringLiteral("other");
}

QString InspirationPanel::genId() {
    return QStringLiteral("i_%1").arg(QDateTime::currentMSecsSinceEpoch());
}

void InspirationPanel::rebuild() {
    m_list->clear();
    const QString q = m_search->text().trimmed().toLower();
    const QString filter = m_filter->currentData().toString();
    for (const auto &v : m_inspirations) {
        const QJsonObject o = v.toObject();
        const QString id = o.value("id").toString();
        const QString content = o.value("content").toString();
        const QString cat = o.value("category").toString();
        const QJsonArray tags = o.value("tags").toArray();
        if (filter != QStringLiteral("all") && cat != filter) continue;
        if (!q.isEmpty()) {
            bool hit = content.toLower().contains(q);
            for (const auto &t : tags) {
                if (t.toString().toLower().contains(q)) { hit = true; break; }
            }
            if (!hit) continue;
        }
        QString display = QStringLiteral("[%1] %2").arg(categoryName(cat), content);
        QStringList tagList;
        for (const auto &t : tags) tagList << QStringLiteral("#%1").arg(t.toString());
        if (!tagList.isEmpty()) display += QStringLiteral("\n") + tagList.join(QLatin1Char(' '));
        auto *item = new QListWidgetItem(display, m_list);
        item->setData(Qt::UserRole, id);
        item->setToolTip(content);
        item->setFlags(item->flags() | Qt::ItemIsSelectable);
    }
}

void InspirationPanel::onAdd() {
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("新建灵感"));
    dlg.resize(380, 300);
    auto *lay = new QVBoxLayout(&dlg);
    auto *content = new QTextEdit(&dlg);
    content->setPlaceholderText(QStringLiteral("写点什么…"));
    lay->addWidget(content, 1);
    auto *row = new QHBoxLayout;
    auto *cat = new QComboBox(&dlg);
    for (int i = 0; i < 6; ++i) {
        cat->addItem(QString::fromUtf8(kCategoryZh[i]), QString::fromLatin1(kCategories[i]));
    }
    cat->setCurrentIndex(5); // 其他
    auto *tags = new QLineEdit(&dlg);
    tags->setPlaceholderText(QStringLiteral("标签（逗号分隔）"));
    row->addWidget(new QLabel(QStringLiteral("分类"), &dlg));
    row->addWidget(cat);
    row->addWidget(tags, 1);
    lay->addLayout(row);
    auto *btnRow = new QHBoxLayout;
    auto *ok = new QPushButton(QStringLiteral("保存"), &dlg);
    auto *cancel = new QPushButton(QStringLiteral("取消"), &dlg);
    btnRow->addStretch();
    btnRow->addWidget(ok);
    btnRow->addWidget(cancel);
    lay->addLayout(btnRow);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    content->setFocus();

    if (dlg.exec() != QDialog::Accepted) return;
    const QString text = content->toPlainText().trimmed();
    if (text.isEmpty()) return;
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonObject o;
    o["id"] = genId();
    o["content"] = text;
    o["category"] = cat->currentData().toString();
    QStringList tagList;
    const QStringList parts = tags->text().split(QRegularExpression(QStringLiteral("[,，\\s]+")), Qt::SkipEmptyParts);
    for (const auto &p : parts) tagList << p;
    QJsonArray tagArr;
    for (const auto &p : tagList) tagArr.append(p);
    o["tags"] = tagArr;
    o["createdAt"] = now;
    o["updatedAt"] = now;
    // 新增置顶（与 Electron addInspiration 一致）
    QJsonArray arr = m_inspirations;
    arr.prepend(o);
    m_inspirations = arr;
    save();
    rebuild();
}

void InspirationPanel::onEdit() {
    const QString id = m_contextId;
    if (id.isEmpty()) return;
    QJsonObject target;
    int idx = -1;
    for (int i = 0; i < m_inspirations.size(); ++i) {
        const QJsonObject o = m_inspirations.at(i).toObject();
        if (o.value("id").toString() == id) { target = o; idx = i; break; }
    }
    if (idx < 0) return;

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("编辑灵感"));
    dlg.resize(380, 300);
    auto *lay = new QVBoxLayout(&dlg);
    auto *content = new QTextEdit(&dlg);
    content->setPlainText(target.value("content").toString());
    lay->addWidget(content, 1);
    auto *row = new QHBoxLayout;
    auto *cat = new QComboBox(&dlg);
    for (int i = 0; i < 6; ++i) {
        cat->addItem(QString::fromUtf8(kCategoryZh[i]), QString::fromLatin1(kCategories[i]));
    }
    const int cur = cat->findData(target.value("category").toString());
    cat->setCurrentIndex(cur >= 0 ? cur : 5);
    auto *tags = new QLineEdit(&dlg);
    QStringList tagList;
    for (const auto &t : target.value("tags").toArray()) tagList << t.toString();
    tags->setText(tagList.join(QStringLiteral(", ")));
    tags->setPlaceholderText(QStringLiteral("标签（逗号分隔）"));
    row->addWidget(new QLabel(QStringLiteral("分类"), &dlg));
    row->addWidget(cat);
    row->addWidget(tags, 1);
    lay->addLayout(row);
    auto *btnRow = new QHBoxLayout;
    auto *ok = new QPushButton(QStringLiteral("保存"), &dlg);
    auto *cancel = new QPushButton(QStringLiteral("取消"), &dlg);
    btnRow->addStretch();
    btnRow->addWidget(ok);
    btnRow->addWidget(cancel);
    lay->addLayout(btnRow);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;
    const QString text = content->toPlainText().trimmed();
    if (text.isEmpty()) return;
    QJsonArray tagArr;
    const QStringList parts = tags->text().split(QRegularExpression(QStringLiteral("[,，\\s]+")), Qt::SkipEmptyParts);
    for (const auto &p : parts) tagArr.append(p);
    target["content"] = text;
    target["category"] = cat->currentData().toString();
    target["tags"] = tagArr;
    target["updatedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonArray arr = m_inspirations;
    arr.replace(idx, target);
    m_inspirations = arr;
    save();
    rebuild();
}

void InspirationPanel::onDelete() {
    const QString id = m_contextId;
    if (id.isEmpty()) return;
    QMessageBox box(QMessageBox::Warning, QStringLiteral("删除灵感"),
                    QStringLiteral("确定要删除这条灵感吗？"),
                    QMessageBox::NoButton, this);
    auto *yesBtn = box.addButton(QStringLiteral("删除"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(yesBtn);
    box.exec();
    if (box.clickedButton() != yesBtn) return;
    QJsonArray arr;
    for (const auto &v : m_inspirations) {
        if (v.toObject().value("id").toString() != id) arr.append(v);
    }
    m_inspirations = arr;
    save();
    rebuild();
}

void InspirationPanel::onItemDoubleClicked(QListWidgetItem *item) {
    if (!item) return;
    m_contextId = item->data(Qt::UserRole).toString();
    onEdit();
}

void InspirationPanel::onContextMenu(const QPoint &pos) {
    QListWidgetItem *item = m_list->itemAt(pos);
    if (!item) return;
    m_contextId = item->data(Qt::UserRole).toString();
    QMenu menu(this);
    menu.addAction(QStringLiteral("✏️ 编辑"), this, &InspirationPanel::onEdit);
    menu.addAction(QStringLiteral("🗑 删除"), this, &InspirationPanel::onDelete);
    menu.exec(m_list->viewport()->mapToGlobal(pos));
}

void InspirationPanel::onSearchChanged(const QString &) {
    rebuild();
}

void InspirationPanel::onFilterChanged(int) {
    rebuild();
}
