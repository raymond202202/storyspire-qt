#!/usr/bin/env bash
# storyspire-qt 测试：Exporter 单测 + 大纲功能测试（临时目录隔离，不碰真实数据）
# 用法：bash tests/run_tests.sh
set -euo pipefail
cd "$(dirname "$0")/.."

QT_CFLAGS=$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets)
QT_LIBS=$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets)

echo "── Exporter 单测 ──"
g++ -std=c++17 -fPIC tests/exporter_test.cpp src/Exporter.cpp $QT_CFLAGS $QT_LIBS -o /tmp/ssq_exporter_test
QT_QPA_PLATFORM=offscreen /tmp/ssq_exporter_test

echo "── 大纲功能测试（隔离目录）──"
MOC=$(pkg-config --variable=libexecdir Qt6Core)/moc
"$MOC" src/BookTree.h -o /tmp/ssq_moc_BookTree.cpp
g++ -std=c++17 -fPIC tests/outline_test.cpp src/BookTree.cpp /tmp/ssq_moc_BookTree.cpp \
    $QT_CFLAGS $QT_LIBS -o /tmp/ssq_outline_test
QT_QPA_PLATFORM=offscreen /tmp/ssq_outline_test

echo "── 预览面板单测 ──"
MOC=$(pkg-config --variable=libexecdir Qt6Core)/moc
"$MOC" src/PreviewPanel.h -o /tmp/ssq_moc_PreviewPanel.cpp
g++ -std=c++17 -fPIC tests/preview_test.cpp src/PreviewPanel.cpp /tmp/ssq_moc_PreviewPanel.cpp \
    $QT_CFLAGS $QT_LIBS -o /tmp/ssq_preview_test
QT_QPA_PLATFORM=offscreen /tmp/ssq_preview_test

echo "── 数据文件 md5（确认未被测试改动）──"
md5sum "$HOME/.config/storyspire-data/books.json" "$HOME/.config/storyspire-data/inspirations.json"
echo "ALL TESTS DONE"
