#pragma once

#include <QString>
#include <QJsonObject>
#include <QVector>
#include <QByteArray>

/**
 * 导出工具 —— 对齐 Electron 版 StorySpire exporter.ts 语义：
 *  - txt ：书名 + 作者 + 每章标题（==== 分隔）+ 纯文本正文
 *  - docx：OOXML 包（zip STORE 存储，Word/WPS 可开），结构对齐 docx 分支
 *          （书标题居中加粗 16pt / 作者 10pt / 章标题加粗 12pt / 正文行 11pt）
 *  - doc ：RTF 格式（Word 97+ 可开），转义规则与 Electron toRtf 一致
 *  - pdf ：QPdfWriter 渲染（HTML 排版对齐 htmlForPdf：宋体风格、h1 居中、正文缩进 2em）
 *  - 大纲：book.outlines 作为章节导出（标题 "{书名} - 大纲"）
 *  - Qt 版章节 content 为纯文本；对历史 HTML 数据做轻量 stripHtml（对齐 htmlToText 核心）
 */
class Exporter {
public:
    enum class Format { Txt, Docx, Doc, Pdf };

    /** 章节纯文本化：去标签/实体（对齐 Electron htmlToText，Qt 版 content 本就是纯文本，仅兼容旧数据） */
    static QString stripHtml(const QString &html);

    /** 纯文本 → RTF（对齐 Electron toRtf） */
    static QString toRtf(const QString &text);

    /** 文件名安全化：替换 [\\\\/:*?"<>|] 为 _（对齐 Electron safeName） */
    static QString sanitizeFileName(const QString &name);

    /** 生成整书 txt 文本（含书名/作者头 + 章节 ==== 分隔，对齐 exportBook txt 分支） */
    static QString buildTxt(const QString &title, const QString &author,
                            const QVector<QJsonObject> &chapters);

    /** 生成 doc (RTF) 文本（对齐 exportBook doc 分支 + toRtf） */
    static QString buildDoc(const QString &title, const QString &author,
                            const QVector<QJsonObject> &chapters);

    /** 生成整书 docx 二进制（OOXML zip，STORE 无压缩；结构对齐 Electron docx 分支） */
    static QByteArray buildDocx(const QString &title, const QString &author,
                                const QVector<QJsonObject> &chapters);

    /** 生成整书 PDF 排版 HTML（对齐 Electron htmlForPdf：h1 居中 / .author / h2 章标题 / p 正文缩进） */
    static QString buildPdfHtml(const QString &title, const QString &author,
                                const QVector<QJsonObject> &chapters);

    /** 渲染 PDF 到 path（QPdfWriter + QTextDocument，A4、中文字体回退），成功返回空串 */
    static QString writePdf(const QString &path, const QString &title, const QString &author,
                            const QVector<QJsonObject> &chapters);

    /** 原子写文件（QSaveFile），成功返回空串，失败返回错误信息 */
    static QString writeFile(const QString &path, const QString &content);

    /** 原子写二进制文件（QSaveFile），成功返回空串，失败返回错误信息 */
    static QString writeFileBytes(const QString &path, const QByteArray &data);
};
