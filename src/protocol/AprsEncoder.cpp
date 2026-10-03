/*
 * AprsEncoder.cpp - APRS position reports and the helpers around them.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "AprsEncoder.h"
#include "AX25Frame.h"

#include <QRegularExpression>
#include <QtMath>

#include <cmath>

namespace aprs {

bool isOverlay(const QString &table)
{
    if (table.size() != 1) return false;
    const QChar c = table.at(0);
    return c.isDigit() || (c >= 'A' && c <= 'Z');
}

int passcode(const QString &callsign)
{
    QString base = callsign.trimmed().toUpper().section('-', 0, 0);
    if (base.isEmpty() || base.size() > 9) return -1;
    for (const QChar c : base) {
        if (!(c.isLetterOrNumber() && c.unicode() < 128)) return -1;
    }
    int hash = 0x73e2;
    for (int i = 0; i < base.size(); i += 2) {
        hash ^= base.at(i).unicode() << 8;
        if (i + 1 < base.size()) hash ^= base.at(i + 1).unicode();
    }
    return hash & 0x7fff;
}

bool validateSymbol(const QString &table, const QString &code, QString *error)
{
    if (table.size() != 1 || code.size() != 1) {
        if (error) *error = QStringLiteral("Symbol table and code must each be one character");
        return false;
    }
    const QChar t = table.at(0);
    if (!(t == '/' || t == '\\' || t.isDigit() || (t >= 'A' && t <= 'Z'))) {
        if (error) *error = QStringLiteral("Symbol table must be '/', '\\', a digit or a capital letter");
        return false;
    }
    const ushort c = code.at(0).unicode();
    if (c < 0x21 || c > 0x7E) {
        if (error) *error = QStringLiteral("Symbol code must be a printable ASCII character");
        return false;
    }
    return true;
}


namespace {

// Split a decimal degree value into whole degrees and decimal minutes,
// rounded to two decimals.  Formatting 49.999999 naively gives "4960.00",
// which is not a coordinate; carrying the rounded minutes into the degrees
// avoids that.
void degreesMinutes(double value, int &degrees, double &minutes)
{
    value = std::fabs(value);
    degrees = static_cast<int>(value);
    minutes = std::round((value - degrees) * 60.0 * 100.0) / 100.0;
    if (minutes >= 60.0) {
        degrees += 1;
        minutes -= 60.0;
    }
}

QString twoDecimals(double minutes)
{
    // "05.2f": at least 5 characters, two decimals, zero padded.
    return QString::number(minutes, 'f', 2).rightJustified(5, QChar('0'));
}

} // namespace

QString formatLatitude(double latitude, QString *error)
{
    if (latitude < -90.0 || latitude > 90.0) {
        if (error) *error = QStringLiteral("Latitude %1 is outside -90..90").arg(latitude);
        return QString();
    }
    int degrees; double minutes;
    degreesMinutes(latitude, degrees, minutes);
    return QStringLiteral("%1%2%3")
        .arg(degrees, 2, 10, QChar('0'))
        .arg(twoDecimals(minutes))
        .arg(latitude >= 0 ? QStringLiteral("N") : QStringLiteral("S"));
}

QString formatLongitude(double longitude, QString *error)
{
    if (longitude < -180.0 || longitude > 180.0) {
        if (error) *error = QStringLiteral("Longitude %1 is outside -180..180").arg(longitude);
        return QString();
    }
    int degrees; double minutes;
    degreesMinutes(longitude, degrees, minutes);
    return QStringLiteral("%1%2%3")
        .arg(degrees, 3, 10, QChar('0'))
        .arg(twoDecimals(minutes))
        .arg(longitude >= 0 ? QStringLiteral("E") : QStringLiteral("W"));
}

QString formatCourseSpeed(double courseDeg, double speedKnots)
{
    int course = static_cast<int>(std::lround(courseDeg)) % 360;
    if (course < 0) course += 360;
    if (course == 0) course = 360;
    const int speed = qBound(0, static_cast<int>(std::lround(speedKnots)), 999);
    return QStringLiteral("%1/%2").arg(course, 3, 10, QChar('0')).arg(speed, 3, 10, QChar('0'));
}

QString formatAltitude(double metres)
{
    const int feet = qBound(0, static_cast<int>(std::lround(metres * 3.28084)), 999999);
    return QStringLiteral("/A=%1").arg(feet, 6, 10, QChar('0'));
}

bool locatorToLatLon(const QString &locator, double &latitude, double &longitude, QString *error)
{
    static const QRegularExpression re(QStringLiteral("^[A-R]{2}[0-9]{2}([A-X]{2})?$"),
                                       QRegularExpression::CaseInsensitiveOption);
    const QString text = locator.trimmed().toUpper();
    if (!re.match(text).hasMatch()) {
        if (error) *error = QStringLiteral("'%1' is not a Maidenhead locator; expected 4 or 6 "
                                           "characters such as JN33 or JN33RU").arg(locator);
        return false;
    }
    longitude = (text.at(0).unicode() - 'A') * 20.0 - 180.0;
    latitude = (text.at(1).unicode() - 'A') * 10.0 - 90.0;
    longitude += text.at(2).digitValue() * 2.0;
    latitude += text.at(3).digitValue() * 1.0;
    if (text.size() == 6) {
        longitude += (text.at(4).unicode() - 'A') * (2.0 / 24.0);
        latitude += (text.at(5).unicode() - 'A') * (1.0 / 24.0);
        longitude += (2.0 / 24.0) / 2.0;          // centre of the subsquare
        latitude += (1.0 / 24.0) / 2.0;
    } else {
        longitude += 1.0;                         // centre of the square
        latitude += 0.5;
    }
    return true;
}

QString latLonToLocator(double latitude, double longitude, int precision)
{
    if (latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0) return QString();
    const double lon = std::min(longitude + 180.0, 359.9999);
    const double lat = std::min(latitude + 90.0, 179.9999);
    QString text;
    text += QChar('A' + static_cast<int>(std::floor(lon / 20.0)));
    text += QChar('A' + static_cast<int>(std::floor(lat / 10.0)));
    text += QString::number(static_cast<int>(std::floor(std::fmod(lon, 20.0) / 2.0)));
    text += QString::number(static_cast<int>(std::floor(std::fmod(lat, 10.0))));
    if (precision >= 6) {
        text += QChar('a' + static_cast<int>(std::fmod(lon, 2.0) / (2.0 / 24.0)));
        text += QChar('a' + static_cast<int>(std::fmod(lat, 1.0) / (1.0 / 24.0)));
    }
    return text;
}

double distanceKm(double lat1, double lon1, double lat2, double lon2)
{
    const double radius = 6371.0;
    const double phi1 = qDegreesToRadians(lat1);
    const double phi2 = qDegreesToRadians(lat2);
    const double dphi = qDegreesToRadians(lat2 - lat1);
    const double dlambda = qDegreesToRadians(lon2 - lon1);
    const double a = std::sin(dphi / 2) * std::sin(dphi / 2)
                   + std::cos(phi1) * std::cos(phi2) * std::sin(dlambda / 2) * std::sin(dlambda / 2);
    return 2 * radius * std::asin(std::min(1.0, std::sqrt(a)));
}

QString sanitiseComment(const QString &textIn)
{
    QString text = textIn;
    text.replace('\r', ' ');
    text.replace('\n', ' ');
    QString cleaned;
    cleaned.reserve(text.size());
    for (const QChar c : text) {
        const ushort u = c.unicode();
        if (u == '|' || u == '~') continue;
        if (u < 0x20 || u > 0x7E) continue;
        cleaned.append(c);
    }
    return cleaned.trimmed().left(MaxCommentLen);
}

} // namespace aprs


// ---- PositionReport ------------------------------------------------------------------

QByteArray PositionReport::encode(QString *error, const QString &encoding) const
{
    if (!aprs::validateSymbol(symbolTable, symbolCode, error)) return QByteArray();

    const QString lat = aprs::formatLatitude(latitude, error);
    if (lat.isEmpty()) return QByteArray();
    const QString lon = aprs::formatLongitude(longitude, error);
    if (lon.isEmpty()) return QByteArray();

    QString body = (messagingCapable ? QStringLiteral("=") : QStringLiteral("!"))
                 + lat + symbolTable + lon + symbolCode;

    // Course and speed sit immediately after the symbol, before any comment
    // text, and only when both are known.
    if (courseDeg && speedKnots) {
        body += aprs::formatCourseSpeed(*courseDeg, *speedKnots);
    }
    const QString cleaned = aprs::sanitiseComment(comment);
    if (altitudeM) {
        body += aprs::formatAltitude(*altitudeM);
        // A space after the extension so clients that display the raw
        // comment do not run the altitude into the text.  Parsers look for
        // the literal "/A=" plus six digits, so this is safe.
        if (!cleaned.isEmpty()) body += ' ';
    }
    body += cleaned;
    return ax25::encodeText(body, encoding);
}

QString PositionReport::describe() const
{
    QStringList parts;
    parts << QStringLiteral("%1%2").arg(std::fabs(latitude), 0, 'f', 5)
                                    .arg(latitude >= 0 ? QStringLiteral("N") : QStringLiteral("S"));
    parts << QStringLiteral("%1%2").arg(std::fabs(longitude), 0, 'f', 5)
                                    .arg(longitude >= 0 ? QStringLiteral("E") : QStringLiteral("W"));
    const QString locator = aprs::latLonToLocator(latitude, longitude);
    if (!locator.isEmpty()) parts << locator;
    if (altitudeM) parts << QStringLiteral("%1 m").arg(std::lround(*altitudeM));
    if (speedKnots && courseDeg) {
        parts << QStringLiteral("%1 kn / %2deg").arg(std::lround(*speedKnots)).arg(std::lround(*courseDeg));
    }
    return parts.join(QStringLiteral("  "));
}
