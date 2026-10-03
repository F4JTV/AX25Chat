/*
 * SymbolArt.h - rendering APRS symbols from the aprs.fi sprite sheets.
 *
 * Port of ax25chat/symbols.py.  The sheets come from
 * https://github.com/hessu/aprs-symbols and are fetched at build time
 * (scripts/fetch_symbols.sh) rather than committed, because their copyright
 * status is mixed.  Everything degrades gracefully: without the sheets,
 * pixmap() returns a null pixmap and callers show names and codes.
 *
 * Sheet layout: 16 columns by 6 rows, cell index = code - 0x21.  Three
 * sheets per size: -0 primary table, -1 alternate table, -2 overlay glyphs.
 * An overlay symbol is the alternate-table icon with the overlay character
 * composited on top.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QIcon>
#include <QPixmap>
#include <QString>

namespace symbolart {

// Directory searched for the sheets; the first that exists of the
// candidates in the source (assets/aprs-symbols beside the executable, in
// the application data locations, or a path set with setDirectory()).
QString directory();
void setDirectory(const QString &path);

bool available();
int sheetResolution();         // pixels per symbol in the sheets found, 0 if none

QPixmap pixmap(const QString &table, const QString &code, int size = 32);
QIcon icon(const QString &table, const QString &code, int size = 32);

void clearCache();
QString attribution();

} // namespace symbolart
