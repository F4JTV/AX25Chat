/*
 * Style.cpp - palette-aware colours.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Style.h"

#include <QApplication>
#include <QLabel>
#include <QVariant>

namespace style {

namespace {

const QHash<QString, QString> &lightColours()
{
    static const QHash<QString, QString> map = {
        {"tx", "#1a7f37"}, {"rx", "#0969da"}, {"beacon", "#9a6700"}, {"error", "#b32626"},
        {"system", "#57606a"}, {"monitor", "#7c3aed"}, {"dim", "#57606a"}, {"clear", "#1a7f37"},
        {"busy", "#9a6700"}, {"transmitting", "#b32626"}, {"offline", "#6e7681"},
    };
    return map;
}

const QHash<QString, QString> &darkColours()
{
    static const QHash<QString, QString> map = {
        {"tx", "#3fb950"}, {"rx", "#58a6ff"}, {"beacon", "#d29922"}, {"error", "#f85149"},
        {"system", "#9198a1"}, {"monitor", "#a371f7"}, {"dim", "#9198a1"}, {"clear", "#3fb950"},
        {"busy", "#d29922"}, {"transmitting", "#f85149"}, {"offline", "#8b949e"},
    };
    return map;
}

} // namespace

bool isDark(const QPalette &palette)
{
    // Comparing text against background classifies mid-grey themes too.
    const int background = palette.color(QPalette::Window).lightness();
    const int text = palette.color(QPalette::WindowText).lightness();
    if (text != background) return text > background;
    return background < 128;
}

QHash<QString, QString> colours(const QPalette &palette)
{
    return isDark(palette) ? darkColours() : lightColours();
}

QColor dimColour(const QPalette &palette)
{
    QColor colour = palette.color(QPalette::Disabled, QPalette::WindowText);
    if (colour.alpha() == 0) {
        colour = palette.color(QPalette::WindowText);
        colour.setAlpha(160);
    }
    return colour;
}

void applyHintStyle(QLabel *label)
{
    if (!label) return;
    if (!label->property("hintFontApplied").toBool()) {
        QFont font = label->font();
        const int size = font.pointSize();
        if (size > 0) font.setPointSize(qMax(7, size - 1));
        label->setFont(font);
        label->setProperty("hintFontApplied", true);
    }
    // The source palette is the application's, not the label's: reading the
    // label's own WindowText back would compound the dimming on each call.
    const QPalette source = qApp ? qApp->palette() : label->palette();
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, dimColour(source));
    label->setPalette(palette);
}

void applyHeadingStyle(QLabel *label, int scale)
{
    if (!label) return;
    QFont font = label->font();
    font.setBold(true);
    const int size = font.pointSize();
    if (size > 0) font.setPointSize(size + scale);
    label->setFont(font);
}

} // namespace style
