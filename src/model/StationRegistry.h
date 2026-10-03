/*
 * StationRegistry.h - stations heard.
 *
 * Port of ax25chat/stations.py.  Keeps what has been learned about each
 * callsign on the air: when it was last heard, how many times, the path it
 * arrived by, and its position when it has reported one.  Distance and
 * bearing are computed against our own position, so the list answers the
 * question an operator actually has: not "where is that station" but "how
 * far away is it and in which direction".
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "AprsPacket.h"

#include <QHash>
#include <QList>
#include <QString>

#include <functional>
#include <optional>

struct Station
{
    QString callsign;
    double firstHeard = 0.0;         // seconds
    double lastHeard = 0.0;
    int packets = 0;
    QString path;
    std::optional<double> latitude;
    std::optional<double> longitude;
    double positionAt = 0.0;
    QString symbolTable = QStringLiteral("/");
    QString symbolCode = QStringLiteral("-");
    std::optional<double> courseDeg;
    std::optional<double> speedKnots;
    std::optional<double> altitudeM;
    QString comment;
    QString status;
    QString lastMessage;
    QString weather;
    bool viaInternet = false;        // last heard through APRS-IS, not on the air

    bool hasPosition() const { return latitude && longitude; }
    QString symbol() const { return symbolTable + symbolCode; }

    double ageSeconds(double now) const { return std::max(0.0, now - lastHeard); }
    QString ageText(double now) const;      // "42s", "7m", "3h"

    // (distance km, bearing deg) from a reference point.
    std::optional<std::pair<double, double>> relativeTo(double latitude, double longitude) const;

    // One line for the station list.
    QString describe(double now, std::optional<std::pair<double, double>> reference = std::nullopt) const;
    QString tooltip() const;
};


class StationRegistry
{
public:
    using Clock = std::function<double()>;   // seconds

    explicit StationRegistry(int expiryMinutes = 120);

    int expiryMinutes = 120;

    void setClock(Clock clock);
    double now() const { return m_clock(); }

    int size() const { return m_stations.size(); }
    std::optional<Station> get(const QString &callsign) const;
    void clear();

    // Record that a callsign was heard, regardless of packet content.
    Station noteHeard(const QString &callsign, const QString &path = QString());

    // Merge a decoded APRS packet into what is known about a station.
    Station applyPacket(const QString &callsign, const AprsPacket &packet, const QString &path = QString());

    // Drop stations not heard for a while.  Returns how many went.
    int expire();

    // Stations sorted by how recently they were heard, newest first.
    // Every station held, most recently heard first; a positive limit
    // truncates (the lists and the map pass none: the expiry in the
    // settings is the bound on what is held).
    QList<Station> listed(int limit = -1) const;
    QList<Station> withPosition() const;

private:
    Clock m_clock;
    QHash<QString, Station> m_stations;
};
