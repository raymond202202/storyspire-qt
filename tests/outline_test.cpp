// 大纲系统测试：addOutline/updateOutline/renameOutline/deleteOutline + 持久化
// 用临时 XDG_CONFIG_HOME 隔离，不碰真实 ~/.config/storyspire-data
#include "../src/BookTree.h"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <cstdio>

static int failures = 0;
#define CHECK(cond, name) do { if (cond) { std::printf("PASS: %s\n", name); } else { std::printf("FAIL: %s\n", name); ++failures; } } while (0)

static QString testDataPath;

static void seedData() {
    const QJsonObject vol{
        {QStringLiteral("id"), QStringLiteral("v1")},
        {QStringLiteral("title"), QStringLiteral("第一卷")},
        {QStringLiteral("order"), 1},
    };
    const QJsonObject book{
        {QStringLiteral("id"), QStringLiteral("b1")},
        {QStringLiteral("title"), QStringLiteral("测试书")},
        {QStringLiteral("author"), QStringLiteral("作者甲")},
        {QStringLiteral("volumes"), QJsonArray{vol}},
        {QStringLiteral("chapters"), QJsonArray{}},
        {QStringLiteral("outlines"), QJsonArray{}},
    };
    const QJsonObject root{
        {QStringLiteral("books"), QJsonArray{book}},
        {QStringLiteral("trash"), QJsonArray{}},
    };
    QFile f(testDataPath);
    f.open(QIODevice::WriteOnly);
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
}

int main(int argc, char **argv) {
    // 隔离测试目录
    const QString cfg = QStringLiteral("/tmp/storyspire-qt-test-config");
    QDir().mkpath(cfg + QStringLiteral("/storyspire-data"));
    qputenv("XDG_CONFIG_HOME", cfg.toUtf8());
    testDataPath = cfg + QStringLiteral("/storyspire-data/books.json");
    seedData();

    QApplication app(argc, argv);
    BookTree tree;
    tree.reload();

    // 1. addOutline
    const QString oid = tree.addOutline(QStringLiteral("  第一章大纲  "), QStringLiteral("开篇：主角登场。"));
    CHECK(!oid.isEmpty(), "addOutline 返回 id");
    QJsonArray outlines = tree.outlines();
    CHECK(outlines.size() == 1, "addOutline 列表 +1");
    const QJsonObject o = outlines.first().toObject();
    CHECK(o.value("title").toString() == QStringLiteral("第一章大纲"), "addOutline 标题 trim");
    CHECK(o.value("volumeId").toString() == QStringLiteral("v1"), "addOutline 默认卷=第一卷");
    CHECK(o.value("content").toString() == QStringLiteral("开篇：主角登场。"), "addOutline content");
    CHECK(o.value("wordCount").toInt() > 0, "addOutline wordCount>0");
    CHECK(!o.value("createdAt").toString().isEmpty() && !o.value("updatedAt").toString().isEmpty(),
          "addOutline createdAt/updatedAt");

    // 2. updateOutline
    tree.updateOutline(oid, QStringLiteral("开篇：主角登场。\n中段：冲突升级。"), 20);
    const QJsonObject o2 = tree.outlines().first().toObject();
    CHECK(o2.value("content").toString().contains(QStringLiteral("冲突升级")), "updateOutline content");
    CHECK(o2.value("wordCount").toInt() == 20, "updateOutline wordCount 指定值");

    // 3. renameOutline（trim + 空标题忽略）
    tree.renameOutline(oid, QStringLiteral("  新标题  "));
    CHECK(tree.outlines().first().toObject().value("title").toString() == QStringLiteral("新标题"),
          "renameOutline trim");
    tree.renameOutline(oid, QStringLiteral("   "));
    CHECK(tree.outlines().first().toObject().value("title").toString() == QStringLiteral("新标题"),
          "renameOutline 空标题忽略");

    // 4. deleteOutline → trash 带 _outline:true
    tree.deleteOutline(oid);
    CHECK(tree.outlines().isEmpty(), "deleteOutline 列表清空");

    // 5. 持久化：重新加载验证
    tree.reload();
    CHECK(tree.outlines().isEmpty(), "reload 后 outlines 为空（已删除）");
    QFile f(testDataPath);
    f.open(QIODevice::ReadOnly);
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    f.close();
    const QJsonArray trash = root.value(QStringLiteral("trash")).toArray();
    CHECK(trash.size() == 1, "trash 有 1 条");
    const QJsonObject item = trash.first().toObject();
    CHECK(item.value("type").toString() == QStringLiteral("outline"), "trash type=outline");
    CHECK(item.value("title").toString() == QStringLiteral("新标题"), "trash title");
    CHECK(item.value("data").toObject().value("_outline").toBool() == true, "trash data._outline=true");
    CHECK(!item.value("deletedAt").toString().isEmpty(), "trash deletedAt");

    // 6. 写回内容持久化（再建一个并写入，检查文件里 content 落盘）
    const QString oid2 = tree.addOutline(QStringLiteral("持久化大纲"), QStringLiteral("内容AB"));
    tree.updateOutline(oid2, QStringLiteral("内容ABC"), 3);
    tree.reload();
    CHECK(tree.outlines().size() == 1, "reload 后 outlines 保留");
    CHECK(tree.outlines().first().toObject().value("content").toString() == QStringLiteral("内容ABC"),
          "updateOutline 持久化到文件");

    // 清理测试目录
    QDir(cfg).removeRecursively();

    std::printf(failures == 0 ? "== ALL PASS ==\n" : "== %d FAILURES ==\n", failures);
    return failures == 0 ? 0 : 1;
}
