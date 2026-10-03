/*
 * SymbolArt.cpp - rendering APRS symbols from the aprs.fi sprite sheets.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "SymbolArt.h"
#include "AprsEncoder.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QPainter>
#include <QStandardPaths>

namespace symbolart {

namespace {

constexpr int Columns = 16;
constexpr int Rows = 6;
constexpr int FirstCode = 0x21;
const QList<int> CandidateSizes = {64, 128, 56, 48, 256, 32, 24};

QString g_directory;
QHash<int, QPixmap> g_sheets;
QHash<QString, QPixmap> g_pixmaps;
int g_resolvedSize = 0;
bool g_searched = false;

QStringList candidateDirectories()
{
    QStringList dirs;
    if (!g_directory.isEmpty()) dirs << g_directory;
    const QString app = QCoreApplication::applicationDirPath();
    dirs << QDir(app).filePath(QStringLiteral("assets/aprs-symbols"))
         << QDir(app).filePath(QStringLiteral("../assets/aprs-symbols"))
         << QDir(app).filePath(QStringLiteral("../share/ax25chat/aprs-symbols"));
#ifdef AX25CHAT_DATA_DIR
    dirs << QStringLiteral(AX25CHAT_DATA_DIR "/aprs-symbols");
#endif
    // Compiled into the binary (Android, where nothing sits next to the
    // program): see AX25CHAT_EMBED_SYMBOLS in CMakeLists.txt.
    dirs << QStringLiteral(":/aprs-symbols");
    const QStringList data = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    for (const QString &d : data) dirs << QDir(d).filePath(QStringLiteral("aprs-symbols"));
    return dirs;
}

QString sheetPath(const QString &dir, int size, int table)
{
    return QDir(dir).filePath(QStringLiteral("aprs-symbols-%1-%2.png").arg(size).arg(table));
}

int findSize()
{
    if (g_searched) return g_resolvedSize;
    g_searched = true;
    for (const QString &dir : candidateDirectories()) {
        if (!QFileInfo(dir).isDir()) continue;
        for (const int size : CandidateSizes) {
            bool all = true;
            for (int t = 0; t < 3 && all; t++) all = QFileInfo(sheetPath(dir, size, t)).isFile();
            if (all) {
                g_directory = dir;
                g_resolvedSize = size;
                return size;
            }
        }
    }
    return 0;
}

QPixmap sheet(int table)
{
    if (g_sheets.contains(table)) return g_sheets.value(table);
    const int size = findSize();
    QPixmap pixmap;
    if (size) pixmap = QPixmap(sheetPath(g_directory, size, table));
    // A truncated download loads as a null pixmap; treat that as no artwork.
    g_sheets.insert(table, pixmap);
    return pixmap;
}

QPixmap cell(const QPixmap &sheet, QChar code)
{
    const int index = code.unicode() - FirstCode;
    if (index < 0 || index >= Columns * Rows) return QPixmap();
    const int cellW = sheet.width() / Columns;
    const int cellH = sheet.height() / Rows;
    return sheet.copy((index % Columns) * cellW, (index / Columns) * cellH, cellW, cellH);
}

} // namespace

QString directory()
{
    findSize();
    return g_directory;
}

void setDirectory(const QString &path)
{
    g_directory = path;
    clearCache();
}

bool available()
{
    return findSize() != 0;
}

int sheetResolution()
{
    return findSize();
}

QPixmap pixmap(const QString &table, const QString &code, int size)
{
    if (table.isEmpty() || code.isEmpty()) return QPixmap();
    const QString key = table + code + QString::number(size);
    if (g_pixmaps.contains(key)) return g_pixmaps.value(key);

    const bool overlay = aprs::isOverlay(table);
    const int tableId = (overlay || table == QStringLiteral("\\")) ? 1 : 0;
    const QPixmap base = sheet(tableId);
    if (base.isNull()) return QPixmap();
    QPixmap icon = cell(base, code.at(0));
    if (icon.isNull()) return QPixmap();

    if (overlay) {
        const QPixmap overlaySheet = sheet(2);
        if (!overlaySheet.isNull()) {
            const QPixmap glyph = cell(overlaySheet, table.at(0));
            if (!glyph.isNull()) {
                QPainter painter(&icon);
                painter.setRenderHint(QPainter::SmoothPixmapTransform);
                painter.drawPixmap(icon.rect(), glyph);
                painter.end();
            }
        }
    }
    const QPixmap scaled = icon.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    g_pixmaps.insert(key, scaled);
    return scaled;
}

QIcon icon(const QString &table, const QString &code, int size)
{
    const QPixmap p = pixmap(table, code, size);
    return p.isNull() ? QIcon() : QIcon(p);
}

void clearCache()
{
    g_sheets.clear();
    g_pixmaps.clear();
    g_resolvedSize = 0;
    g_searched = false;
}

QString attribution()
{
    return QStringLiteral("Symbols: aprs.fi set by Heikki Hannikainen OH7LZB and others. "
                          "See COPYRIGHT.md in the symbols folder.");
}

} // namespace symbolart
