#include "FlareClient.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QTimer>
#include <QDebug>

namespace {
// 写作专家 profile（对齐 flare examples/storyspire/expert.ts；tools 走 chat 请求声明）
const char *kExpertProfileTemplate = R"JSON({
  "name": "story 助手",
  "identity": "我是 story 助手，是集成到 storyspire-qt 里的 flare 写作专家",
  "flareIntro": "flare 是一款由我的作者开发的通用型 AI agent，story 助手集成并深度定制了它的写作专家能力，它的完整版功能更强大，如果您需要完整版的 flare 功能，您可以通过访问它的官网来获取，链接在这里：https://github.com/raymond202202/flare",
  "systemPrompt": "你是 story 助手，集成在 StorySpire 应用中的写作专家（基于 flare 引擎）。\n你帮助作者完成小说创作：起草章节、续写、润色、大纲建议、文风调整。\n工作原则：\n1. 写作前先用 story_get_story / story_list_chapters 了解故事结构、人物、当前进度，不要凭空写\n2. 续写/润色前先用 story_get_chapter 读取原文，保持风格与设定一致\n3. 生成内容后，用 story_create_chapter 或 story_update_chapter 写入章节，不要只给文字不落盘\n4. 涉及创作建议时给出可操作的具体方案（如大纲分点、冲突设计、文风示例），不要空泛\n5. 用户贴出片段要求润色时，保留原意与关键信息，只优化表达\n用中文回答用户的问题。",
  "storage": "__STORAGE__"
})JSON";

// story 工具定义（对齐 flare src/tools/story.ts，宿主代理工具走 tool_execute 事件）
const char *kStoryToolsJson = R"JSON([
  {
    "type": "function",
    "function": {
      "name": "story_get_story",
      "description": "获取当前故事的整体结构：标题、作者、卷、章节列表（含各章标题/字数）。写作前先了解故事背景与进度。无参数。",
      "parameters": { "type": "object", "properties": {}, "required": [] }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "story_get_chapter",
      "description": "获取某个章节的完整内容（标题 + 正文）。需要 chapterId。用于续写、润色、分析某章。",
      "parameters": {
        "type": "object",
        "properties": { "chapterId": { "type": "string", "description": "章节 ID" } },
        "required": ["chapterId"]
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "story_list_chapters",
      "description": "列出当前故事所有章节（标题/字数/所属卷）。查看写作进度时用。无参数。",
      "parameters": { "type": "object", "properties": {}, "required": [] }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "story_create_chapter",
      "description": "创建新章节（AI 起草）。可指定标题；若给 content 则直接写入正文。",
      "parameters": {
        "type": "object",
        "properties": {
          "title": { "type": "string", "description": "章节标题" },
          "content": { "type": "string", "description": "可选：章节正文（AI 起草完成后写入）" }
        },
        "required": ["title"]
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "story_update_chapter",
      "description": "更新章节正文（润色、续写、修改后写回）。需要 chapterId 和 content。这是把 AI 生成内容写入故事的关键工具。",
      "parameters": {
        "type": "object",
        "properties": {
          "chapterId": { "type": "string", "description": "章节 ID" },
          "content": { "type": "string", "description": "新的章节正文（完整替换）" }
        },
        "required": ["chapterId", "content"]
      }
    }
  }
])JSON";
}

FlareClient::FlareClient(QObject *parent) : QObject(parent) {
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &FlareClient::onReadyRead);
    connect(m_proc, &QProcess::errorOccurred, this, &FlareClient::onProcessError);
    connect(m_proc, &QProcess::finished, this, &FlareClient::onFinished);
    startServer();
}

FlareClient::~FlareClient() {
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->terminate();
        if (!m_proc->waitForFinished(1000)) m_proc->kill();
    }
}

bool FlareClient::isRunning() const {
    return m_proc && m_proc->state() == QProcess::Running;
}

QProcessEnvironment FlareClient::buildEnvironment() const {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (env.contains(QStringLiteral("DEEPSEEK_API_KEY"))) return env; // 环境变量优先

    // 从 ~/.storyspire/.env 读取（KEY=VALUE 行）
    QFile envFile(QDir::homePath() + QStringLiteral("/.storyspire/.env"));
    if (envFile.open(QIODevice::ReadOnly)) {
        const QStringList lines = QString::fromUtf8(envFile.readAll()).split(QLatin1Char('\n'));
        for (const QString &raw : lines) {
            const QString line = raw.trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq <= 0) continue;
            const QString key = line.left(eq).trimmed();
            QString value = line.mid(eq + 1).trimmed();
            if (value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')) && value.size() >= 2) {
                value = value.mid(1, value.size() - 2);
            }
            env.insert(key, value);
        }
    }
    return env;
}

void FlareClient::startServer() {
    // node 路径：系统 node（PATH）→ /usr/bin/node
    QString node = QStandardPaths::findExecutable(QStringLiteral("node"));
    if (node.isEmpty() && QFileInfo::exists(QStringLiteral("/usr/bin/node"))) {
        node = QStringLiteral("/usr/bin/node");
    }
    if (node.isEmpty()) {
        m_lastError = QStringLiteral("未找到 node（请安装 Node.js）");
        emit serverFailed(m_lastError);
        return;
    }

    const QString flareHome = QDir::homePath() + QStringLiteral("/hermes-projects/flare");
    const QString bin = flareHome + QStringLiteral("/bin/flare");
    if (!QFileInfo::exists(bin)) {
        m_lastError = QStringLiteral("未找到 flare（%1）").arg(bin);
        emit serverFailed(m_lastError);
        return;
    }

    // 写作专家 profile（写入应用配置目录；storage 展开为绝对路径）
    const QString profilePath = QDir::homePath() + QStringLiteral("/.config/storyspire-qt/story-expert.json");
    QDir().mkpath(QFileInfo(profilePath).absolutePath());
    const QString storage = QDir::homePath() + QStringLiteral("/.storyspire/story-ai.db");
    QString profileJson = QString::fromUtf8(kExpertProfileTemplate);
    profileJson.replace(QStringLiteral("__STORAGE__"), storage);
    QFile pf(profilePath);
    if (pf.open(QIODevice::WriteOnly)) {
        pf.write(profileJson.toUtf8());
        pf.close();
    }

    const QStringList args{bin, QStringLiteral("server"), QStringLiteral("-p"), profilePath,
                           QStringLiteral("-s"), storage};
    m_proc->setProgram(node);
    m_proc->setArguments(args);
    m_proc->setProcessEnvironment(buildEnvironment());
    m_proc->start();
    if (!m_proc->waitForStarted(3000)) {
        m_lastError = QStringLiteral("flare server 启动失败: %1").arg(m_proc->errorString());
        emit serverFailed(m_lastError);
        return;
    }
    m_lastError.clear();
    emit serverStateChanged(true);
}

void FlareClient::writeLine(const QJsonObject &obj) {
    if (!isRunning()) return;
    m_proc->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n");
}

void FlareClient::sendChat(const QString &sessionId, const QString &input,
                           const QString &context, const QJsonArray &tools) {
    QJsonObject req;
    req["type"] = QStringLiteral("chat");
    req["sessionId"] = sessionId;
    req["input"] = input;
    if (!context.isEmpty()) req["context"] = context;
    if (!tools.isEmpty()) req["tools"] = tools;
    writeLine(req);
}

void FlareClient::sendCancel(const QString &sessionId) {
    QJsonObject req;
    req["type"] = QStringLiteral("cancel");
    req["sessionId"] = sessionId;
    writeLine(req);
}

void FlareClient::sendToolResult(const QString &id, const QJsonObject &result) {
    QJsonObject req;
    req["type"] = QStringLiteral("tool_result");
    req["id"] = id;
    req["result"] = result;
    writeLine(req);
}

void FlareClient::onReadyRead() {
    m_buffer += m_proc->readAllStandardOutput();
    int idx;
    while ((idx = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(idx).trimmed();
        m_buffer.remove(0, idx + 1);
        if (line.isEmpty()) continue;
        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isObject()) {
            emit eventReceived(doc.object());
        }
    }
}

void FlareClient::onProcessError(QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
        m_lastError = QStringLiteral("flare server 启动失败: %1").arg(m_proc->errorString());
        emit serverFailed(m_lastError);
        emit serverStateChanged(false);
    }
}

void FlareClient::onFinished(int, QProcess::ExitStatus) {
    emit serverStateChanged(false);
}
