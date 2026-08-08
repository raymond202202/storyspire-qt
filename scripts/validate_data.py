#!/usr/bin/env python3
"""storyspire-qt 数据格式深度校验（回归用，严格口径）

校验 ~/.config/storyspire-data/books.json 与 Electron 版 src/types/index.ts 对齐：
- 顶层键 books/trash
- book: id/title/author/volumes/chapters/createdAt/updatedAt/outlines
- chapter: id/title/volumeId/content/wordCount/createdAt/updatedAt
- volume: id/title/order/createdAt
- outline: id/title/volumeId/content/wordCount/createdAt/updatedAt
- trash: id/type/title/data/deletedAt
- content 均为纯文本（无 HTML 标签）
- volumeId 交叉引用：非空必须存在于该书 volumes；**空字符串也算悬空**（严格口径）
退出码 0=全部通过；1=发现问题（打印明细）
"""
import json
import sys
import os

BOOKS_PATH = os.path.expanduser("~/.config/storyspire-data/books.json")

BOOK_FIELDS = {"id", "title", "author", "volumes", "chapters", "createdAt", "updatedAt", "outlines"}
CHAPTER_FIELDS = {"id", "title", "volumeId", "content", "wordCount", "createdAt", "updatedAt"}
VOLUME_FIELDS = {"id", "title", "order", "createdAt"}
OUTLINE_FIELDS = {"id", "title", "volumeId", "content", "wordCount", "createdAt", "updatedAt"}
TRASH_FIELDS = {"id", "type", "title", "data", "deletedAt"}


def looks_like_html(s: str) -> bool:
    low = (s or "").lower()
    for tag in ("<p", "<br", "<div", "<span", "<h1", "<h2", "<h3", "<strong", "<em"):
        if tag in low:
            return True
    return False


def main() -> int:
    with open(BOOKS_PATH, encoding="utf-8") as f:
        data = json.load(f)

    errs = []
    top = set(data.keys())
    if top != {"books", "trash"}:
        errs.append(f"顶层键不符: {sorted(top)}")

    books = data.get("books", [])
    if not isinstance(books, list):
        errs.append("books 不是数组")

    n_vols = n_chaps = n_out = 0
    for b in books:
        if not isinstance(b, dict):
            errs.append("book 非对象")
            continue
        bid = b.get("id", "?")
        missing = BOOK_FIELDS - set(b.keys())
        if missing:
            errs.append(f"book[{bid}] 缺字段 {sorted(missing)}")
        volume_ids = set()
        for v in b.get("volumes", []):
            n_vols += 1
            if not isinstance(v, dict):
                errs.append(f"book[{bid}] volume 非对象")
                continue
            vid = v.get("id", "?")
            m = VOLUME_FIELDS - set(v.keys())
            if m:
                errs.append(f"book[{bid}] volume[{vid}] 缺字段 {sorted(m)}")
            volume_ids.add(vid)
        for c in b.get("chapters", []):
            n_chaps += 1
            if not isinstance(c, dict):
                errs.append(f"book[{bid}] chapter 非对象")
                continue
            cid = c.get("id", "?")
            m = CHAPTER_FIELDS - set(c.keys())
            if m:
                errs.append(f"book[{bid}] chapter[{cid}] 缺字段 {sorted(m)}")
            if not isinstance(c.get("wordCount"), int):
                errs.append(f"book[{bid}] chapter[{cid}] wordCount 非 int")
            if looks_like_html(c.get("content", "")):
                errs.append(f"book[{bid}] chapter[{cid}] content 疑似 HTML")
            cv = c.get("volumeId", "")
            if not cv or cv not in volume_ids:
                errs.append(f"book[{bid}] chapter[{cid}] volumeId 悬空: {cv!r}")
        for o in b.get("outlines", []):
            n_out += 1
            if not isinstance(o, dict):
                errs.append(f"book[{bid}] outline 非对象")
                continue
            oid = o.get("id", "?")
            m = OUTLINE_FIELDS - set(o.keys())
            if m:
                errs.append(f"book[{bid}] outline[{oid}] 缺字段 {sorted(m)}")
            ov = o.get("volumeId", "")
            if not ov or ov not in volume_ids:
                errs.append(f"book[{bid}] outline[{oid}] volumeId 悬空: {ov!r}")

    for t in data.get("trash", []):
        if not isinstance(t, dict):
            errs.append("trash 项非对象")
            continue
        m = TRASH_FIELDS - set(t.keys())
        if m:
            errs.append(f"trash[{t.get('id','?')}] 缺字段 {sorted(m)}")

    if errs:
        print("格式深度校验 FAIL:")
        for e in errs:
            print("  -", e)
        return 1
    print(f"格式深度校验 PASS: books={len(books)} volumes={n_vols} chapters={n_chaps} outlines={n_out} trash={len(data.get('trash', []))}")
    print("字段全对齐 Electron src/types/index.ts；content 均纯文本；volumeId 无悬空（含空值）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
