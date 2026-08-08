// PreviewPanel 纯逻辑单测：htmlToPlain / countWords / 预设机型（不实例化界面）
#include "../src/PreviewPanel.h"
#include <QString>
#include <cassert>
#include <cstdio>

int main() {
    // ── htmlToPlain（对齐 Electron htmlToPlain）──
    assert(PreviewPanel::htmlToPlain(
               QStringLiteral("<h1>标题</h1><p>第一段<br>第二行</p>")) ==
           QStringLiteral("标题\n\n第一段\n第二行"));
    assert(PreviewPanel::htmlToPlain(QStringLiteral("a&nbsp;&amp;&lt;&gt;b")) ==
           QStringLiteral("a &<>b"));
    assert(PreviewPanel::htmlToPlain(QStringLiteral("a\n\n\n\nb")) ==
           QStringLiteral("a\n\nb"));
    assert(PreviewPanel::htmlToPlain(QStringLiteral("<p>  段落  </p>")) ==
           QStringLiteral("段落"));
    // 纯文本（Qt 版 content 字段）→ 幂等
    assert(PreviewPanel::htmlToPlain(QStringLiteral("第一行\n第二行")) ==
           QStringLiteral("第一行\n第二行"));

    // ── countWords（中文 + 英文单词，与 Editor 一致）──
    assert(PreviewPanel::countWords(QStringLiteral("你好世界")) == 4);
    assert(PreviewPanel::countWords(QStringLiteral("hello world")) == 2);
    assert(PreviewPanel::countWords(QStringLiteral("你好 hello")) == 3);
    assert(PreviewPanel::countWords(QStringLiteral("")) == 0);

    // ── 预设机型（对齐 Electron PHONE_PRESETS）──
    const auto &ps = PreviewPanel::presets();
    assert(ps.size() == 6);
    assert(ps[0].id == QStringLiteral("iphone17pm"));
    assert(ps[0].name == QStringLiteral("iPhone 17 Pro Max"));
    assert(ps[0].width == 440 && ps[0].height == 956 && ps[0].displayWidth == 310);
    assert(ps[5].id == QStringLiteral("pixel9"));
    assert(ps[5].width == 412 && ps[5].height == 892 && ps[5].displayWidth == 295);

    std::printf("preview_test OK (htmlToPlain/countWords/presets 全部通过)\n");
    return 0;
}
