#pragma once

#include <QWidget>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

class FlareClient;
class BookTree;
class QTextBrowser;
class QTextEdit;
class QPushButton;
class QLabel;

/**
 * AI 助手面板（写作专家）—— 接 flare server 子进程（JSON Lines 协议）。
 * 快捷动作：续写/润色/扩写/灵感创作；story_* 工具经宿主代理工具流执行；
 * 写回章节前用户确认：应用一次/本次会话允许/始终允许/拒绝（决策缓存：会话=内存，始终=配置文件）。
 */
class AiPanel : public QWidget {
    Q_OBJECT
public:
    explicit AiPanel(QWidget *parent = nullptr);
    ~AiPanel() override;

    /** 数据源（工具执行器用） */
    void setBookTree(BookTree *tree);

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    void onEvent(const QJsonObject &event);
    void onSend();
    void onStop();
    void onQuickContinue();
    void onQuickPolish();
    void onQuickExpand();
    void onQuickInspiration();
    void onServerFailed(const QString &message);
    void onServerStateChanged(bool running);

private:
    FlareClient *m_client = nullptr;
    BookTree *m_tree = nullptr;
    QTextBrowser *m_messages = nullptr;
    QTextEdit *m_input = nullptr;
    QPushButton *m_send = nullptr;
    QPushButton *m_stop = nullptr;
    QLabel *m_status = nullptr;
    bool m_loading = false;
    QString m_sessionId = QStringLiteral("story-ai");
    QJsonArray m_tools;
    QSet<QString> m_sessionAllowed;   // 本次会话允许写回的工具
    QSet<QString> m_alwaysAllowed;    // 始终允许写回的工具（持久化）

    QString buildContext() const;
    void appendUserMessage(const QString &text);
    void appendAssistantChunk(const QString &text);
    void beginAssistantMessage();
    void endAssistantMessage();
    void appendSystemMessage(const QString &text);
    void handleToolExecute(const QJsonObject &event);
    QJsonObject executeTool(const QString &name, const QJsonObject &args);
    int confirmWrite(const QString &tool, const QString &target, const QString &preview);
    void loadAlwaysDecisions();
    void saveAlwaysDecisions();
    void updateButtons();
    void sendPrompt(const QString &prompt);
    static QJsonObject makeResult(bool success, const QString &output,
                                  const QString &error = QString(), bool denied = false);
    static int countWords(const QString &text);
};
