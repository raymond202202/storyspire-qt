#include "OutlinePanel.h"
#include "BookTree.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QListWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QMessageBox>
#include <QDialog>
#include <QJsonArray>
#include <QRegularExpression>

OutlinePanel::OutlinePanel(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto *row = new QHBoxLayout;
    m_newBtn = new QPushButton(QStringLiteral("＋ 新建大纲"), this);
    m_renameBtn = new QPushButton(QStringLiteral("✏️ 重命名"), this);
    m_deleteBtn = new QPushButton(QStringLiteral("🗑 删除"), this);
    row->addWidget(m_newBtn);
    row->addWidget(m_renameBtn);
    row->addWidget(m_deleteBtn);
    layout->addLayout(row);

    m_list = new QListWidget(this);
    layout->addWidget(m_list, 1);

    m_title = new QLineEdit(this);
    m_title->setPlaceholderText(QStringLiteral("大纲标题"));
    layout->addWidget(m_title);

    m_body = new QPlainTextEdit(this);
    m_body->setPlaceholderText(QStringLiteral("在这里写大纲…（独立于正文，可单独导出）"));
    layout->addWidget(m_body, 3);

    auto *bottom = new QHBoxLayout;
    m_wordCount = new QLabel(QStringLiteral("0 字"), this);
    m_saveState = new QLabel(QString(), this);
    bottom->addWidget(m_wordCount);
    bottom->addStretch();
    bottom->addWidget(m_saveState);
    layout->addLayout(bottom);

    // 内容编辑 500ms 防抖写回（与 Editor 一致）
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(500);
    connect(m_debounce, &QTimer::timeout, this, &OutlinePanel::flushContent);

    connect(m_newBtn, &QPushButton::clicked, this, &OutlinePanel::onNewOutline);
    connect(m_renameBtn, &QPushButton::clicked, this, &OutlinePanel::onRenameOutline);
    connect(m_deleteBtn, &QPushButton::clicked, this, &OutlinePanel::onDeleteOutline);
    connect(m_list, &QListWidget::currentRowChanged, this, &OutlinePanel::onItemSelected);
    connect(m_body, &QPlainTextEdit::textChanged, this, [this]() {
        if (m_loading || m_outlineId.isEmpty()) return;
        m_dirty = true;
        m_saveState->setText(QStringLiteral("● 未保存"));
        const int wc = countWords(m_body->toPlainText());
        m_wordCount->setText(QStringLiteral("%1 字").arg(wc));
        m_debounce->start();
    });
    connect(m_title, &QLineEdit::returnPressed, this, &OutlinePanel::onTitleEdited);
    connect(m_title, &QLineEdit::editingFinished, this, &OutlinePanel::onTitleEdited);
}

void OutlinePanel::setBookTree(BookTree *tree) {
    m_tree = tree;
    reload();
}

void OutlinePanel::reload() {
    m_loading = true;
    const QString keepId = m_outlineId;
    m_list->clear();
    m_outlineId.clear();
    if (m_tree) {
        const QJsonArray outlines = m_tree->outlines();
        for (const auto &v : outlines) {
            const QJsonObject o = v.toObject();
            const int wc = o.value("wordCount").toInt();
            m_list->addItem(QStringLiteral("📋 %1  · %2 字").arg(o.value("title").toString()).arg(wc));
            m_list->item(m_list->count() - 1)->setData(Qt::UserRole, o.value("id").toString());
        }
        // 恢复之前选中
        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->item(i)->data(Qt::UserRole).toString() == keepId) {
                m_list->setCurrentRow(i);
                break;
            }
        }
    }
    m_loading = false;
    if (m_list->currentRow() < 0 && m_list->count() > 0) {
        m_list->setCurrentRow(0);
    } else if (m_list->count() == 0) {
        m_title->clear();
        m_body->clear();
        m_wordCount->setText(QStringLiteral("0 字"));
        m_saveState->setText(QString());
        m_title->setEnabled(false);
        m_body->setEnabled(false);
    } else {
        m_title->setEnabled(true);
        m_body->setEnabled(true);
    }
}

QJsonObject OutlinePanel::currentOutline() const {
    if (!m_tree || m_outlineId.isEmpty()) return QJsonObject();
    const QJsonArray outlines = m_tree->outlines();
    for (const auto &v : outlines) {
        const QJsonObject o = v.toObject();
        if (o.value("id").toString() == m_outlineId) return o;
    }
    return QJsonObject();
}

void OutlinePanel::onItemSelected(int row) {
    if (m_loading || row < 0) return;
    flushContent(); // 切换前保存当前
    QListWidgetItem *item = m_list->item(row);
    if (!item) return;
    m_outlineId = item->data(Qt::UserRole).toString();
    const QJsonObject o = currentOutline();
    m_loading = true;
    m_title->setText(o.value("title").toString());
    m_body->setPlainText(o.value("content").toString());
    m_wordCount->setText(QStringLiteral("%1 字").arg(o.value("wordCount").toInt()));
    m_saveState->setText(QStringLiteral("已保存"));
    m_dirty = false;
    m_loading = false;
    m_title->setEnabled(true);
    m_body->setEnabled(true);
}

void OutlinePanel::flushContent() {
    if (!m_tree || m_outlineId.isEmpty() || !m_dirty) return;
    const QString content = m_body->toPlainText();
    m_tree->updateOutline(m_outlineId, content, countWords(content));
    m_dirty = false;
    m_saveState->setText(QStringLiteral("已保存"));
    // 刷新列表中的字数
    QListWidgetItem *item = m_list->currentItem();
    if (item) {
        const QJsonObject o = currentOutline();
        item->setText(QStringLiteral("📋 %1  · %2 字")
                          .arg(o.value("title").toString())
                          .arg(o.value("wordCount").toInt()));
    }
}

void OutlinePanel::onTitleEdited() {
    if (!m_tree || m_outlineId.isEmpty()) return;
    const QString newTitle = m_title->text().trimmed();
    if (newTitle.isEmpty()) {
        const QJsonObject o = currentOutline();
        m_title->setText(o.value("title").toString());
        return;
    }
    m_tree->renameOutline(m_outlineId, newTitle);
    // 同步列表显示
    QListWidgetItem *item = m_list->currentItem();
    if (item) {
        const QJsonObject o = currentOutline();
        item->setText(QStringLiteral("📋 %1  · %2 字")
                          .arg(o.value("title").toString())
                          .arg(o.value("wordCount").toInt()));
    }
}

void OutlinePanel::onNewOutline() {
    if (!m_tree) return;
    // 输入标题（对齐 Electron SaveModal：空则默认“新大纲”）
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("新建大纲"));
    auto *lay = new QVBoxLayout(&dlg);
    auto *edit = new QLineEdit(&dlg);
    edit->setPlaceholderText(QStringLiteral("大纲标题（可留空）"));
    lay->addWidget(edit);
    auto *btnRow = new QHBoxLayout;
    auto *okBtn = new QPushButton(QStringLiteral("确定"), &dlg);
    auto *cancelBtn = new QPushButton(QStringLiteral("取消"), &dlg);
    btnRow->addStretch();
    btnRow->addWidget(okBtn);
    btnRow->addWidget(cancelBtn);
    lay->addLayout(btnRow);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(edit, &QLineEdit::returnPressed, &dlg, &QDialog::accept);
    if (dlg.exec() != QDialog::Accepted) return;

    const QString id = m_tree->addOutline(edit->text(), QString());
    if (id.isEmpty()) return;
    reload();
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(Qt::UserRole).toString() == id) {
            m_list->setCurrentRow(i);
            break;
        }
    }
    m_body->setFocus();
}

void OutlinePanel::onRenameOutline() {
    if (!m_tree || m_outlineId.isEmpty()) return;
    const QJsonObject o = currentOutline();
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("重命名大纲"));
    auto *lay = new QVBoxLayout(&dlg);
    auto *edit = new QLineEdit(o.value("title").toString(), &dlg);
    edit->selectAll();
    lay->addWidget(edit);
    auto *btnRow = new QHBoxLayout;
    auto *okBtn = new QPushButton(QStringLiteral("确定"), &dlg);
    auto *cancelBtn = new QPushButton(QStringLiteral("取消"), &dlg);
    btnRow->addStretch();
    btnRow->addWidget(okBtn);
    btnRow->addWidget(cancelBtn);
    lay->addLayout(btnRow);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(edit, &QLineEdit::returnPressed, &dlg, &QDialog::accept);
    if (dlg.exec() != QDialog::Accepted) return;
    const QString newTitle = edit->text().trimmed();
    if (newTitle.isEmpty()) return;
    m_tree->renameOutline(m_outlineId, newTitle);
    m_title->setText(newTitle);
    QListWidgetItem *item = m_list->currentItem();
    if (item) item->setText(QStringLiteral("📋 %1  · %2 字").arg(newTitle).arg(o.value("wordCount").toInt()));
}

void OutlinePanel::onDeleteOutline() {
    if (!m_tree || m_outlineId.isEmpty()) return;
    const QJsonObject o = currentOutline();
    QMessageBox box(QMessageBox::Warning, QStringLiteral("删除大纲"),
                    QStringLiteral("确定要删除大纲「%1」吗？\n删除后将移入回收站。").arg(o.value("title").toString()),
                    QMessageBox::NoButton, this);
    auto *yesBtn = box.addButton(QStringLiteral("删除"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(yesBtn);
    box.exec();
    if (box.clickedButton() != yesBtn) return;
    m_tree->deleteOutline(m_outlineId);
    m_outlineId.clear();
    m_dirty = false;
    reload();
}

int OutlinePanel::countWords(const QString &text) const {
    const int zh = text.count(QRegularExpression(QStringLiteral("[\\x{4e00}-\\x{9fff}\\x{3400}-\\x{4dbf}]")));
    const int en = text.split(QRegularExpression(QStringLiteral("[^a-zA-Z]+")), Qt::SkipEmptyParts).size();
    return zh + en;
}
