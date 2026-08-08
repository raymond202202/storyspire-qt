#pragma once

#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>

class QTimer;

/**
 * Flare 宿主客户端 —— spawn node 子进程跑 'flare server'（JSON Lines 协议）。
 * 协议见 ~/hermes-projects/flare/docs/host-protocol.md
 * 启动：node <flare>/bin/flare server -p <profile.json> -s <storage>
 * API key：环境变量 DEEPSEEK_API_KEY 或 ~/.storyspire/.env（绝不硬编码）
 */
class FlareClient : public QObject {
    Q_OBJECT
public:
    explicit FlareClient(QObject *parent = nullptr);
    ~FlareClient() override;

    bool isRunning() const;
    QString lastError() const { return m_lastError; }

    /** 发起对话（流式）；tools 为宿主声明的工具定义 JSON 数组 */
    void sendChat(const QString &sessionId, const QString &input,
                  const QString &context = QString(),
                  const QJsonArray &tools = QJsonArray());
    /** 取消当前生成 */
    void sendCancel(const QString &sessionId);
    /** 回传工具执行结果 */
    void sendToolResult(const QString &id, const QJsonObject &result);

signals:
    /** 服务端事件：text/tool_call/tool_execute/tool_result/done/cancelled/error */
    void eventReceived(const QJsonObject &event);
    /** 子进程状态变化 */
    void serverStateChanged(bool running);
    void serverFailed(const QString &message);

private slots:
    void onReadyRead();
    void onProcessError(QProcess::ProcessError error);
    void onFinished(int exitCode, QProcess::ExitStatus status);

private:
    QProcess *m_proc = nullptr;
    QString m_lastError;
    QByteArray m_buffer;

    void startServer();
    void writeLine(const QJsonObject &obj);
    /** 从 ~/.storyspire/.env 或环境变量解析 API key，注入子进程环境 */
    QProcessEnvironment buildEnvironment() const;
};
