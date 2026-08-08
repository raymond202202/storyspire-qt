// Exporter 单元测试 + 真实数据冒烟（不修改 books.json）
#include "../src/Exporter.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <cstdio>

static int failures = 0;
#define CHECK(cond, name) do { if (cond) { qInfo().noquote() << "PASS:" << name; } else { qInfo().noquote() << "FAIL:" << name; ++failures; } } while (0)

int main() {
    // 显式日志 handler（无显示环境 qInfo 可能走系统日志）
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &msg) {
        std::fprintf(stderr, "%s\n", msg.toUtf8().constData());
        fflush(stderr);
    });
    // 1. stripHtml（对齐 Electron htmlToText 核心）
    const QString stripped = Exporter::stripHtml(
        QStringLiteral("<h1>标题</h1><p>第一段</p><br>第二行&nbsp;&amp;&lt;&gt;</p>"));
    CHECK(stripped.contains(QStringLiteral("标题")), "stripHtml 保留文字");
    CHECK(!stripped.contains(QStringLiteral("<p")) && !stripped.contains(QStringLiteral("<h1"))
              && !stripped.contains(QStringLiteral("</")),
          "stripHtml 去标签");
    CHECK(!stripped.contains(QStringLiteral("&nbsp;")), "stripHtml 实体转换");
    // 与 Electron htmlToText 一致：先删标签后转实体，&amp;&lt;&gt; 转为 &<>
    CHECK(stripped.contains(QStringLiteral("第二行 &<>")), "stripHtml 实体转字面符号");

    // 2. toRtf 转义
    const QString rtf = Exporter::toRtf(QStringLiteral("a\\b{c}\nd"));
    CHECK(rtf.startsWith(QStringLiteral("{\\rtf1\\ansi\\deff0")), "RTF 头");
    CHECK(rtf.contains(QStringLiteral("a\\\\b\\{c\\}\\par d")), "RTF 转义 \\ { } \\n");

    // 3. sanitizeFileName
    CHECK(Exporter::sanitizeFileName(QStringLiteral("a/b:c*d?e\"f<g>h|i")) ==
              QStringLiteral("a_b_c_d_e_f_g_h_i"),
          "sanitizeFileName 替换非法字符");

    // 4. buildTxt 格式（对齐 Electron：书名/作者头 + ==== 分隔章节）
    QVector<QJsonObject> chs;
    QJsonObject c1; c1[QStringLiteral("title")] = QStringLiteral("第一章"); c1[QStringLiteral("content")] = QStringLiteral("你好世界。");
    QJsonObject c2; c2[QStringLiteral("title")] = QStringLiteral("第二章"); c2[QStringLiteral("content")] = QStringLiteral("<p>带HTML的正文</p>");
    chs << c1 << c2;
    const QString txt = Exporter::buildTxt(QStringLiteral("测试书"), QStringLiteral("作者甲"), chs);
    CHECK(txt.startsWith(QStringLiteral("测试书\n作者：作者甲\n")), "txt 书头");
    CHECK(txt.contains(QStringLiteral("\n====================\n第一章\n====================\n\n你好世界。")), "txt 章节分隔");
    CHECK(txt.contains(QStringLiteral("带HTML的正文")) && !txt.contains(QStringLiteral("<p>")), "txt 兼容 HTML 旧数据");

    // 5. buildDoc 对齐 Electron doc 分支 + RTF
    const QString doc = Exporter::buildDoc(QStringLiteral("测试书"), QStringLiteral("作者甲"), chs);
    CHECK(doc.contains(QStringLiteral("第一章\\par \\par 你好世界。")), "doc 章节拼接+换行转义");

    // 6. 真实数据冒烟：整书导出（不改 books.json）
    const QString dataPath = QString::fromLocal8Bit(qgetenv("HOME")) + QStringLiteral("/.config/storyspire-data/books.json");
    QFile f(dataPath);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        const QJsonArray books = root.value(QStringLiteral("books")).toArray();
        if (!books.isEmpty()) {
            const QJsonObject book = books.first().toObject();
            QVector<QJsonObject> chapters;
            for (const auto &c : book.value(QStringLiteral("chapters")).toArray())
                chapters.append(c.toObject());
            const QString title = book.value(QStringLiteral("title")).toString();
            const QString outPath = QStringLiteral("/tmp/storyspire-qt-export-smoke.txt");
            const QString err = Exporter::writeFile(outPath, Exporter::buildTxt(title,
                                book.value(QStringLiteral("author")).toString(), chapters));
            QFile out(outPath);
            CHECK(err.isEmpty() && out.exists() && out.size() > 0, "真实数据整书导出 txt");
            if (!err.isEmpty()) qInfo().noquote() << "  err:" << err;
            // RTF 冒烟
            const QString docPath = QStringLiteral("/tmp/storyspire-qt-export-smoke.doc");
            const QString derr = Exporter::writeFile(docPath, Exporter::buildDoc(title,
                                book.value(QStringLiteral("author")).toString(), chapters));
            QFile dout(docPath);
            CHECK(derr.isEmpty() && dout.exists() && dout.size() > 0, "真实数据整书导出 doc(RTF)");
        } else {
            qInfo().noquote() << "SKIP: books.json 无书";
        }
    } else {
        qInfo().noquote() << "SKIP: 无 books.json";
    }

    // 7. 大纲导出（outlines 空 → buildTxt 仍可导出空章；主程序对空大纲有提示，这里只验证构造不崩）
    QVector<QJsonObject> empty;
    const QString outlineTxt = Exporter::buildTxt(QStringLiteral("测试书 - 大纲"), QString(), empty);
    CHECK(outlineTxt.contains(QStringLiteral("测试书 - 大纲")), "大纲空列表构造不崩");

    qInfo().noquote() << (failures == 0 ? "== ALL PASS ==" : QString("== %1 FAILURES ==").arg(failures));
    return failures == 0 ? 0 : 1;
}
