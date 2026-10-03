/*
 * AprsEncoder.h - APRS position reports and the helpers around them.
 *
 * Port of ax25chat/aprs.py.  Only the uncompressed position report without
 * timestamp is produced, which is the format every APRS client, digipeater
 * and igate understands.  Layout per the APRS Protocol Reference 1.0.1,
 * chapter 6:
 *
 *     = 4903.50N / 07201.75W -  comment
 *     ^ ^          ^ ^          ^
 *     | |          | |          `-- comment, free text
 *     | |          | `------------- symbol code, 1 character
 *     | |          `--------------- longitude, 9 characters, dddmm.hhE/W
 *     | `-------------------------- symbol table id, 1 character
 *     `---------------------------- data type identifier
 *
 * The identifier is '=' when the station can receive APRS messages and '!'
 * when it cannot.  Optional pieces, in the order APRS expects them: course
 * and speed as "088/036" right after the symbol, altitude as "/A=001234"
 * (feet) at the start of the comment.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

namespace aprs {

// Destination address of a position report.  APRS uses the destination as
// a software identifier; APZ... is the range for experimental software.
inline const QString DefaultTocall = QStringLiteral("APZ25C");

constexpr int MaxCommentLen = 43;   // recommended maximum for a position report

// ---- symbols ---------------------------------------------------------------------

// True when the table identifier is an overlay character (digit or capital).
bool isOverlay(const QString &table);

// A symbol is a table id ('/', '\\', a digit or a capital letter) plus a
// printable ASCII code.  Returns false and fills error otherwise.
bool validateSymbol(const QString &table, const QString &code, QString *error = nullptr);

// The APRS-IS passcode of a callsign: the base callsign (no SSID), upper
// case, folded two characters at a time into 0x73E2 and masked to 15 bits.
// -1 when the callsign is not a callsign.
int passcode(const QString &callsign);

// ---- coordinates ----------------------------------------------------------------

// "4903.50N", 8 characters.  Empty and error set when out of range.
QString formatLatitude(double latitude, QString *error = nullptr);
// "07201.75W", 9 characters.
QString formatLongitude(double longitude, QString *error = nullptr);
// "088/036": degrees true and knots.  A course of 0 means unknown in APRS,
// so due north is sent as 360.
QString formatCourseSpeed(double courseDeg, double speedKnots);
// "/A=001234", feet, clamped to 0..999999.
QString formatAltitude(double metres);

// ---- Maidenhead ------------------------------------------------------------------

// Centre of the square named by a 4 or 6 character locator.  The centre
// rather than the corner, because a locator names an area.
bool locatorToLatLon(const QString &locator, double &latitude, double &longitude,
                     QString *error = nullptr);
// 4 or 6 character locator for a position; empty when out of range.
QString latLonToLocator(double latitude, double longitude, int precision = 6);

// Great-circle distance in kilometres.
double distanceKm(double lat1, double lon1, double lat2, double lon2);

// ---- comments ---------------------------------------------------------------------

// APRS reserves '|' and '~' inside the information field, and a newline
// would end the frame early, so those are stripped; then trimmed and cut to
// MaxCommentLen.
QString sanitiseComment(const QString &text);

} // namespace aprs


// An uncompressed APRS position report without timestamp.
struct PositionReport
{
    double latitude = 0.0;
    double longitude = 0.0;
    QString symbolTable = QStringLiteral("/");
    QString symbolCode = QStringLiteral("-");
    QString comment;
    std::optional<double> altitudeM;
    std::optional<double> courseDeg;
    std::optional<double> speedKnots;
    bool messagingCapable = false;

    // The information field of the UI frame, or empty with error set.
    QByteArray encode(QString *error = nullptr, const QString &encoding = QStringLiteral("ascii")) const;

    // One line for the log, in human units.
    QString describe() const;
};
