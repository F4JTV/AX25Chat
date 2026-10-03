/*
 * ModemConfigFile.h - where the modem's direwolf.conf lives, and a starter
 * file for a fresh installation.
 *
 * Port of the configuration-file helpers of ax25chat/direwolf.py.  The
 * process supervision that surrounded them is gone: the modem is inside the
 * program, and direwolf.conf is simply the file it loads.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QString>
#include <QStringList>

namespace modemconf {

// direwolf.conf in our own settings folder: the default location.
QString userPath();

// Writability is tested by creating a file, not by inspecting permissions:
// on Windows a directory can report as writable and refuse the write.
bool directoryIsWritable(const QString &directory);

// Write a minimal working direwolf.conf, without overwriting an existing
// one.  Returns the path actually written, which falls back to userPath()
// when the requested directory is read-only; empty with error set on
// failure.
QString writeStarter(const QString &path, const QString &callsign, QString *error = nullptr);

// Write (or rewrite) a direwolf.conf from a few choices, for interfaces
// that do not let the operator edit the file: audio on the default device,
// MODEM speed 1200 or 9600, PTT none (VOX), cm108 (USB sound card GPIO),
// rts or dtr (USB serial adapter).  Returns the path written, empty with
// error set on failure.
QString writeGenerated(const QString &path, const QString &callsign, int speed, const QString &ptt,
                       int gpio, QString *error = nullptr);

// Common locations for direwolf.conf, best first; empty when none exists.
QString defaultFile();

// The CHANNEL numbers declared in a direwolf.conf, in file order.
QList<int> channelsIn(const QString &path);

} // namespace modemconf
