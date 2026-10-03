/*
 * Style.h - palette-aware colours.
 *
 * Port of ax25chat/ui/style.py.  The application uses the platform's native
 * Qt style, so it follows the desktop theme, including a light/dark switch
 * made while it is running.  The only colours defined are the ones that
 * carry meaning: transmitted versus received traffic in the conversation,
 * and the channel state indicator, in a light and a dark variant chosen from
 * the lightness of the window background.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QColor>
#include <QHash>
#include <QPalette>
#include <QString>

class QLabel;

namespace style {

bool isDark(const QPalette &palette);

// Colour set matching the current theme: tx, rx, beacon, error, system,
// monitor, dim, clear, busy, transmitting, offline.
QHash<QString, QString> colours(const QPalette &palette);

// A muted version of the normal text colour, from the theme's own disabled
// text role.
QColor dimColour(const QPalette &palette);

// Make a QLabel read as secondary text.  Safe to call again on a theme
// change: the font is shrunk once, the colour re-resolved every time.
void applyHintStyle(QLabel *label);

// Make a QLabel read as a heading, without imposing a font family.
void applyHeadingStyle(QLabel *label, int scale = 2);

} // namespace style
