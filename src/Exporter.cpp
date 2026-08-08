#include "Exporter.h"
#include <QSaveFile>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>

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
    if (path.isEmpty()) return QStringLiteral("路径为空");
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return QStringLiteral("无法写入：%1").arg(f.errorString());
    f.write(content.toUtf8());
    if (!f.commit())
        return QStringLiteral("写入失败：%1").arg(f.errorString());
    return QString();
}
