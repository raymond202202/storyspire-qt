// Exporter 单元测试 + 真实数据冒烟（不修改 books.json）
#include "../src/Exporter.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <QGuiApplication>
#include <QProcess>
#include <cstdio>

static int failures = 0;
#define CHECK(cond, name) do { if (cond) { qInfo().noquote() << "PASS:" << name; } else { qInfo().noquote() << "FAIL:" << name; ++failures; } } while (0)

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv); // QPdfWriter/QTextDocument 需要 GUI 应用上下文
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

    // 8. buildDocx：zip 魔数 + OOXML 关键结构 + python3 zipfile 严格校验
    const QByteArray docx = Exporter::buildDocx(QStringLiteral("测试书"), QStringLiteral("作者甲"), chs);
    CHECK(docx.startsWith(QByteArray("PK\x03\x04")), "docx zip 魔数 PK\\x03\\x04");
    CHECK(docx.size() > 0, "docx 非空");
    CHECK(docx.contains("[Content_Types].xml") && docx.contains("word/document.xml"),
          "docx 含 OOXML 必需部件名");
    CHECK(docx.contains("<w:sz w:val=\"32\"/>"), "docx 书标题 16pt 加粗");
    CHECK(docx.contains("第一章") && docx.contains("你好世界。"), "docx 章标题+正文");
    const QString docxPath = QStringLiteral("/tmp/storyspire-qt-export-smoke.docx");
    const QString derr2 = Exporter::writeFileBytes(docxPath, docx);
    CHECK(derr2.isEmpty() && QFile::exists(docxPath), "docx 写盘");
    QProcess py; // 用 python3 zipfile 严格校验：zip 完整性 + 条目数 + document.xml 内容
    py.start(QStringLiteral("python3"),
             {QStringLiteral("-c"),
              QStringLiteral("import zipfile,sys; z=zipfile.ZipFile(sys.argv[1]); n=z.namelist(); "
                             "assert len(n)==5 and 'word/document.xml' in n and '[Content_Types].xml' in n; "
                             "d=z.read('word/document.xml').decode('utf-8'); "
                             "assert '第一章' in d and '你好世界。' in d and '作者：作者甲' in d; print('ZIP_OK')"),
              docxPath});
    py.waitForFinished(15000);
    CHECK(py.exitCode() == 0 && QString::fromUtf8(py.readAllStandardOutput()).contains(QStringLiteral("ZIP_OK")),
          "docx zipfile 严格校验（5 条目 + document.xml 内容）");

    // 9. buildPdfHtml：排版结构对齐 Electron htmlForPdf
    const QString pdfHtml = Exporter::buildPdfHtml(QStringLiteral("测试书"), QStringLiteral("作者甲"), chs);
    CHECK(pdfHtml.contains(QStringLiteral("<h1>测试书</h1>")), "pdf html 书名 h1");
    CHECK(pdfHtml.contains(QStringLiteral("作者：作者甲")), "pdf html 作者");
    CHECK(pdfHtml.contains(QStringLiteral("<h2 style=\"text-align:center")), "pdf html 章标题居中样式");
    CHECK(pdfHtml.contains(QStringLiteral("第一章")) && pdfHtml.contains(QStringLiteral("你好世界。")),
          "pdf html 章节内容");
    CHECK(pdfHtml.contains(QStringLiteral("text-indent: 2em")), "pdf html 正文缩进 2em");
    CHECK(pdfHtml.contains(QStringLiteral("<p>带HTML的正文</p>"))
              && !pdfHtml.contains(QStringLiteral("<p><p>")) && !pdfHtml.contains(QStringLiteral("</p></p>")),
          "pdf html 兼容 HTML 旧数据（strip 后单层段落）");

    // 10. writePdf：QPdfWriter 真实渲染到 /tmp
    const QString pdfPath = QStringLiteral("/tmp/storyspire-qt-export-smoke.pdf");
    const QString perr = Exporter::writePdf(pdfPath, QStringLiteral("测试书"), QStringLiteral("作者甲"), chs);
    QFile pdfOut(pdfPath);
    CHECK(perr.isEmpty() && pdfOut.exists() && pdfOut.size() > 0, "pdf 渲染写盘");
    if (pdfOut.open(QIODevice::ReadOnly)) {
        const QByteArray head = pdfOut.read(5);
        CHECK(head == QByteArray("%PDF-"), "pdf 文件头 %PDF-");
        pdfOut.close();
    }

    qInfo().noquote() << (failures == 0 ? "== ALL PASS ==" : QString("== %1 FAILURES ==").arg(failures));
    return failures == 0 ? 0 : 1;
}
