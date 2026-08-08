#include "Exporter.h"
#include <QSaveFile>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>
#include <QDataStream>
#include <QTextDocument>
#include <QPdfWriter>
#include <QPageSize>
#include <QFont>
#include <QFontInfo>

// ── docx（OOXML zip，STORE 无压缩）内部工具 ──────────────────────────

namespace {

// 标准 zlib CRC32（查表法；QCryptographicHash 无 Crc32 枚举）
quint32 crc32Of(const QByteArray &data) {
    static quint32 table[256] = {0};
    static bool init = [] {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        return true;
    }();
    Q_UNUSED(init);
    quint32 crc = 0xFFFFFFFFu;
    for (char ch : data)
        crc = table[(crc ^ static_cast<quint8>(ch)) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

struct ZipEntry { QString name; QByteArray data; };

// 手写 zip 打包（STORE 无压缩）：本地文件头 + 数据 + 中央目录 + EOCD
QByteArray zipStore(const QVector<ZipEntry> &entries) {
    QByteArray body;      // 本地头 + 数据区
    QByteArray central;   // 中央目录
    quint32 offset = 0;
    for (const auto &e : entries) {
        const QByteArray name = e.name.toUtf8();
        const quint32 crc = crc32Of(e.data);
        const quint32 size = quint32(e.data.size());

        QByteArray lh; // PK\x03\x04
        lh.append(char(0x50)).append(char(0x4B)).append(char(0x03)).append(char(0x04));
        QDataStream dsl(&lh, QIODevice::Append);
        dsl.setByteOrder(QDataStream::LittleEndian);
        dsl << quint16(20) << quint16(0) << quint16(0) << quint16(0) << quint16(0x21)
            << crc << size << size
            << quint16(quint16(name.size())) << quint16(0);
        lh.append(name);

        QByteArray ch; // PK\x01\x02（46 字节固定头：ver-made/ver-needed/flags/method/time/date 各 2B + crc/csize/usize 各 4B + name/extra/comment/disk/internal 各 2B + external/offset 各 4B）
        ch.append(char(0x50)).append(char(0x4B)).append(char(0x01)).append(char(0x02));
        QDataStream dsc(&ch, QIODevice::Append);
        dsc.setByteOrder(QDataStream::LittleEndian);
        dsc << quint16(20) << quint16(20) << quint16(0) << quint16(0) << quint16(0) << quint16(0x21)
            << crc << size << size
            << quint16(quint16(name.size())) << quint16(0) << quint16(0) << quint16(0) << quint16(0)
            << quint32(0) << offset;
        ch.append(name);

        body.append(lh);
        body.append(e.data);
        central.append(ch);
        offset += quint32(lh.size() + e.data.size());
    }
    QByteArray eocd; // PK\x05\x06
    eocd.append(char(0x50)).append(char(0x4B)).append(char(0x05)).append(char(0x06));
    QDataStream dse(&eocd, QIODevice::Append);
    dse.setByteOrder(QDataStream::LittleEndian);
    dse << quint16(0) << quint16(0)
        << quint16(quint16(entries.size())) << quint16(quint16(entries.size()))
        << quint32(quint32(central.size())) << quint32(offset) << quint16(0);
    return body + central + eocd;
}

QString xmlEscape(const QString &s) {
    QString e = s;
    e.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    e.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    e.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    e.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    return e;
}

// 段落 XML：可选加粗/居中/前后间距（sz 单位 half-point，对齐 Electron docx 包的 size 参数）
QString docxParagraph(const QString &text, bool bold, bool center,
                      int sz, int before, int after) {
    QString ppr;
    if (center) ppr += QStringLiteral("<w:jc w:val=\"center\"/>");
    if (before > 0 || after > 0)
        ppr += QStringLiteral("<w:spacing w:before=\"%1\" w:after=\"%2\"/>").arg(before).arg(after);
    QString rpr;
    if (bold) rpr += QStringLiteral("<w:b/>");
    rpr += QStringLiteral("<w:sz w:val=\"%1\"/>").arg(sz);
    return QStringLiteral("<w:p>%1<w:r><w:rPr>%2</w:rPr><w:t xml:space=\"preserve\">%3</w:t></w:r></w:p>")
        .arg(ppr.isEmpty() ? QString() : QStringLiteral("<w:pPr>%1</w:pPr>").arg(ppr),
             rpr, xmlEscape(text));
}

} // namespace

QString Exporter::buildPdfHtml(const QString &title, const QString &author,
                               const QVector<QJsonObject> &chapters) {
    QStringList body;
    for (const auto &ch : chapters) {
        body << QStringLiteral("<h2 style=\"text-align:center;margin:28px 0 12px\">%1</h2>")
                    .arg(xmlEscape(ch.value(QStringLiteral("title")).toString()));
        const QStringList lines = stripHtml(ch.value(QStringLiteral("content")).toString())
                                      .split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            if (line.trimmed().isEmpty())
                body << QStringLiteral("<p><br/></p>");
            else
                body << QStringLiteral("<p>%1</p>").arg(xmlEscape(line));
        }
    }
    return QStringLiteral(
        "<!DOCTYPE html><html lang=\"zh\"><head><meta charset=\"utf-8\"><style>\n"
        "    body { font-family: 'Noto Serif CJK SC', 'Source Han Serif SC', serif; font-size: 12pt; line-height: 2; max-width: 720px; margin: 0 auto; padding: 24px; color: #222; }\n"
        "    h1 { text-align: center; margin-bottom: 8px; }\n"
        "    .author { text-align: center; color: #666; font-size: 11pt; margin-bottom: 24px; }\n"
        "    h2 { font-size: 14pt; }\n"
        "    p { margin: 0 0 8px; text-indent: 2em; }\n"
        "  </style></head><body>\n"
        "    <h1>%1</h1>\n"
        "    %2\n"
        "    %3\n"
        "  </body></html>")
        .arg(xmlEscape(title),
             author.isEmpty() ? QString()
                              : QStringLiteral("<div class=\"author\">作者：%1</div>").arg(xmlEscape(author)),
             body.join(QLatin1Char('\n')));
}

QByteArray Exporter::buildDocx(const QString &title, const QString &author,
                               const QVector<QJsonObject> &chapters) {
    // 对齐 Electron docx 分支结构：
    //  - 书标题：bold size 32(=16pt) 居中
    //  - 作者：size 20(=10pt) 居中
    //  - 章标题：bold size 24(=12pt)，spacing before 400(=20pt)
    //  - 正文行：size 22(=11pt)，spacing after 120(=6pt)，空行过滤（对齐 filter(l => l.trim())）
    QStringList docParts;
    docParts << docxParagraph(title.isEmpty() ? QStringLiteral("未命名小说") : title,
                              true, true, 32, 0, 0);
    if (!author.isEmpty())
        docParts << docxParagraph(QStringLiteral("作者：%1").arg(author), false, true, 20, 0, 0);
    for (const auto &ch : chapters) {
        docParts << docxParagraph(ch.value(QStringLiteral("title")).toString(), true, false, 24, 400, 0);
        const QStringList lines = stripHtml(ch.value(QStringLiteral("content")).toString())
                                      .split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            if (!line.trimmed().isEmpty())
                docParts << docxParagraph(line, false, false, 22, 0, 120);
        }
    }
    const QString documentXml = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body>%1"
        "<w:sectPr><w:pgSz w:w=\"11906\" w:h=\"16838\"/>"
        "<w:pgMar w:top=\"1440\" w:right=\"1440\" w:bottom=\"1440\" w:left=\"1440\" "
        "w:header=\"720\" w:footer=\"720\" w:gutter=\"0\"/></w:sectPr>"
        "</w:body></w:document>")
        .arg(docParts.join(QString()));

    const QString contentTypes = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
        "<Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>"
        "<Override PartName=\"/docProps/app.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>"
        "</Types>");

    const QString rels = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>"
        "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties\" Target=\"docProps/app.xml\"/>"
        "</Relationships>");

    const QString core = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "
        "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:dcterms=\"http://purl.org/dc/terms/\" "
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
        "<dc:title>%1</dc:title><dc:creator>%2</dc:creator>"
        "<dcterms:created xsi:type=\"dcterms:W3CDTF\">2026-01-01T00:00:00Z</dcterms:created>"
        "</cp:coreProperties>")
        .arg(xmlEscape(title), xmlEscape(author));

    const QString app = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\">"
        "<Application>StorySpire Qt</Application><AppVersion>0.1</AppVersion>"
        "</Properties>");

    return zipStore({
        {QStringLiteral("[Content_Types].xml"), contentTypes.toUtf8()},
        {QStringLiteral("_rels/.rels"), rels.toUtf8()},
        {QStringLiteral("word/document.xml"), documentXml.toUtf8()},
        {QStringLiteral("docProps/core.xml"), core.toUtf8()},
        {QStringLiteral("docProps/app.xml"), app.toUtf8()},
    });
}

QString Exporter::writePdf(const QString &path, const QString &title, const QString &author,
                           const QVector<QJsonObject> &chapters) {
    if (path.isEmpty()) return QStringLiteral("路径为空");
    QDir().mkpath(QFileInfo(path).absolutePath());
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(24, 24, 24, 24), QPageLayout::Millimeter);
    writer.setResolution(150);
    QTextDocument doc;
    QFont font(QStringLiteral("Noto Serif CJK SC"));
    if (!QFontInfo(font).exactMatch()) font = QFont(QStringLiteral("Noto Sans CJK SC"));
    if (!QFontInfo(font).exactMatch()) font = QFont(QStringLiteral("WenQuanYi Micro Hei"));
    doc.setDefaultFont(font);
    doc.setHtml(buildPdfHtml(title, author, chapters));
    doc.print(&writer);
    // QTextDocument::print 无返回值；以产物文件校验成败
    QFileInfo fi(path);
    if (!fi.exists() || fi.size() <= 0) return QStringLiteral("PDF 生成失败：文件为空");
    return QString();
}


QString Exporter::stripHtml(const QString &html) {
    if (html.isEmpty()) return html;
    QString s = html;
    // 段落/标题 → 双换行（对齐 Electron htmlToText 的顺序与规则）
    s.replace(QRegularExpression(QStringLiteral("<h[1-6][^>]*>"), QRegularExpression::CaseInsensitiveOption),
              QStringLiteral("\n\n"));
    s.replace(QRegularExpression(QStringLiteral("</h[1-6]>"), QRegularExpression::CaseInsensitiveOption),
              QStringLiteral("\n\n"));
    s.replace(QRegularExpression(QStringLiteral("<p[^>]*>"), QRegularExpression::CaseInsensitiveOption),
              QStringLiteral("\n\n"));
    s.replace(QRegularExpression(QStringLiteral("<br\\s*/?>"), QRegularExpression::CaseInsensitiveOption),
              QStringLiteral("\n"));
    // 剩余标签全删
    s.replace(QRegularExpression(QStringLiteral("<[^>]+>")), QString());
    // 实体
    s.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    s.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    s.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    s.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    s.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    s.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    // 压缩连续换行（对齐 \n{3,} → \n\n）
    s.replace(QRegularExpression(QStringLiteral("\\n{3,}")), QStringLiteral("\n\n"));
    return s.trimmed();
}

QString Exporter::toRtf(const QString &text) {
    QString escaped = text;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('{'), QStringLiteral("\\{"));
    escaped.replace(QLatin1Char('}'), QStringLiteral("\\}"));
    escaped.replace(QLatin1Char('\n'), QStringLiteral("\\par "));
    return QStringLiteral("{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fnil\\fcharset134 SimSun;}}\\f0\\fs24 %1}")
        .arg(escaped);
}

QString Exporter::sanitizeFileName(const QString &name) {
    QString s = name;
    s.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    return s;
}

QString Exporter::buildTxt(const QString &title, const QString &author,
                           const QVector<QJsonObject> &chapters) {
    QStringList parts;
    QString header = title.isEmpty() ? QStringLiteral("未命名小说") : title;
    if (!author.isEmpty()) header += QStringLiteral("\n作者：%1").arg(author);
    parts << header + QStringLiteral("\n");
    const QString sep = QStringLiteral("=").repeated(20);
    for (const auto &ch : chapters) {
        const QString chTitle = ch.value(QStringLiteral("title")).toString();
        const QString content = stripHtml(ch.value(QStringLiteral("content")).toString());
        parts << QStringLiteral("\n%1\n%2\n%1\n\n%3").arg(sep, chTitle, content);
    }
    return parts.join(QStringLiteral("\n"));
}

QString Exporter::buildDoc(const QString &title, const QString &author,
                           const QVector<QJsonObject> &chapters) {
    Q_UNUSED(author);
    // 对齐 Electron doc 分支：parts = [`${exportTitle}\n`]，每章 push `\n${title}\n\n${content}`，join('\n')
    QStringList parts;
    parts << (title.isEmpty() ? QStringLiteral("未命名小说") : title) + QStringLiteral("\n");
    for (const auto &ch : chapters) {
        parts << QStringLiteral("\n%1\n\n%2")
                     .arg(ch.value(QStringLiteral("title")).toString(),
                          stripHtml(ch.value(QStringLiteral("content")).toString()));
    }
    return toRtf(parts.join(QStringLiteral("\n")));
}

QString Exporter::writeFile(const QString &path, const QString &content) {
    return writeFileBytes(path, content.toUtf8());
}

QString Exporter::writeFileBytes(const QString &path, const QByteArray &data) {
    if (path.isEmpty()) return QStringLiteral("路径为空");
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return QStringLiteral("无法写入：%1").arg(f.errorString());
    f.write(data);
    if (!f.commit())
        return QStringLiteral("写入失败：%1").arg(f.errorString());
    return QString();
}
