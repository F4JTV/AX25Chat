/*
 * AprsPacket.h - everything worth extracting from one APRS information field.
 *
 * Port of DecodedPacket and WeatherData in ax25chat/aprs_decode.py, plus a
 * few fields Direwolf's decoder provides for free (manufacturer, Maidenhead
 * locator, frequency, PHG range, third-party header).
 *
 * Units: coordinates in decimal degrees (negative south and west), speed in
 * knots, course in degrees, altitude in metres, weather in metric.  APRS
 * carries imperial units on the wire; conversion happens once, in the
 * decoder, and nothing downstream sees Fahrenheit or miles.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QChar>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include <optional>

// A decoded weather report, in metric units.  A field the station did not
// send stays empty; a field written as dots or spaces means "no such
// sensor" and also stays empty, because reading it as zero would invent a
// measurement.
struct AprsWeather
{
    std::optional<int> windDirectionDeg;
    std::optional<double> windSpeedKmh;
    std::optional<double> windGustKmh;
    std::optional<double> temperatureC;
    std::optional<double> rainHourMm;
    std::optional<double> rain24hMm;
    std::optional<double> rainMidnightMm;
    std::optional<int> humidityPct;
    std::optional<double> pressureHpa;
    std::optional<int> luminosityWm2;
    std::optional<double> snow24hCm;
    std::optional<double> radiation;

    bool any() const;

    // One readable line, omitting whatever the station did not send.
    QString describe() const;

    // Decode the field codes in text.  Returns nullopt when nothing was
    // found.  rest receives what follows the readings: the station's own
    // comment.
    static std::optional<AprsWeather> parse(const QString &text, QString *rest = nullptr);
};


struct AprsPacket
{
    enum class Kind { Unknown, Position, Message, Status, Object, Item, Weather, Other };
    enum class MessageKind { None, Message, Ack, Rej, Bulletin, Invalid };

    Kind kind = Kind::Unknown;
    QChar dataType;                  // the data type identifier character
    QString description;             // human readable type name, short
    QString detail;                  // Direwolf's fuller description of the packet

    std::optional<double> latitude;
    std::optional<double> longitude;
    QChar symbolTable = '/';
    QChar symbolCode = '-';
    std::optional<double> courseDeg;
    std::optional<double> speedKnots;
    std::optional<double> altitudeM;
    QString comment;
    QString timestamp;
    QString status;                  // Mic-E message, or status report text

    // Messages
    QString addressee;
    QString text;
    QString messageId;
    MessageKind messageKind = MessageKind::None;
    QString ackOf;                   // reply-ack: acknowledges this message id
    bool replyAckCapable = false;

    // Objects and items
    QString name;
    bool alive = true;

    // Weather
    std::optional<AprsWeather> weather;

    // Extras from Direwolf's decoder
    QString maidenhead;
    QString manufacturer;            // from the destination "tocall"
    std::optional<double> frequencyMHz;
    std::optional<double> rangeKm;   // pre-computed radio range (PHG, RNG)
    bool thirdParty = false;         // wrapped in a third-party header

    QStringList errors;

    bool hasPosition() const { return latitude.has_value() && longitude.has_value(); }
    QString symbol() const { return QString(symbolTable) + QString(symbolCode); }

    static QString kindName(Kind kind);
    static QString messageKindName(MessageKind kind);
};
Q_DECLARE_METATYPE(AprsPacket)
