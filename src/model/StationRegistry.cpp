/*
 * StationRegistry.cpp - stations heard.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "StationRegistry.h"
#include "AprsDecoder.h"
#include "AprsEncoder.h"

#include <QDateTime>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace {

double wallClockSeconds()
{
    return QDateTime::currentMSecsSinceEpoch() / 1000.0;
}

} // namespace


QString Station::ageText(double now) const
{
    const int age = static_cast<int>(ageSeconds(now));
    if (age < 90) return QStringLiteral("%1s").arg(age);
    if (age < 5400) return QStringLiteral("%1m").arg(age / 60);
    return QStringLiteral("%1h").arg(age / 3600);
}

std::optional<std::pair<double, double>> Station::relativeTo(double lat, double lon) const
{
    if (!hasPosition()) return std::nullopt;
    return std::make_pair(aprs::distanceKm(lat, lon, *latitude, *longitude),
                          AprsDecoder::bearingDeg(lat, lon, *latitude, *longitude));
}

QString Station::describe(double now, std::optional<std::pair<double, double>> reference) const
{
    QStringList parts;
    parts << callsign;
    if (reference && hasPosition()) {
        const auto rel = relativeTo(reference->first, reference->second);
        const double distance = rel->first;
        const QString point = AprsDecoder::compassPoint(rel->second);
        if (distance < 10) parts << QStringLiteral("%1 km %2").arg(distance, 0, 'f', 1).arg(point);
        else parts << QStringLiteral("%1 km %2").arg(std::lround(distance)).arg(point);
    } else if (hasPosition()) {
        parts << QStringLiteral("%1,%2").arg(*latitude, 0, 'f', 3).arg(*longitude, 0, 'f', 3);
    }
    parts << ageText(now);
    if (speedKnots && *speedKnots >= 1) parts << QStringLiteral("%1 kn").arg(std::lround(*speedKnots));
    if (viaInternet) parts << QStringLiteral("[IS]");
    return parts.join(QStringLiteral("   "));
}

QString Station::tooltip() const
{
    QStringList lines;
    lines << QStringLiteral("%1   %2 packet(s)").arg(callsign).arg(packets);
    if (hasPosition()) {
        lines << QStringLiteral("%1%2  %3%4")
                     .arg(std::fabs(*latitude), 0, 'f', 5).arg(*latitude >= 0 ? QStringLiteral("N") : QStringLiteral("S"))
                     .arg(std::fabs(*longitude), 0, 'f', 5).arg(*longitude >= 0 ? QStringLiteral("E") : QStringLiteral("W"));
    }
    if (altitudeM) lines << QStringLiteral("Altitude %1 m").arg(std::lround(*altitudeM));
    if (courseDeg && speedKnots) {
        lines << QStringLiteral("Course %1 deg, %2 kn").arg(std::lround(*courseDeg)).arg(std::lround(*speedKnots));
    }
    if (!weather.isEmpty()) lines << QStringLiteral("Weather: %1").arg(weather);
    if (!status.isEmpty()) lines << QStringLiteral("Status: %1").arg(status);
    if (!comment.isEmpty()) lines << QStringLiteral("Comment: %1").arg(comment);
    if (!lastMessage.isEmpty()) lines << QStringLiteral("Last message: %1").arg(lastMessage);
    if (!path.isEmpty()) lines << QStringLiteral("Path: %1").arg(path);
    return lines.join('\n');
}


StationRegistry::StationRegistry(int expiryMinutesIn)
    : expiryMinutes(expiryMinutesIn), m_clock(wallClockSeconds)
{
}

void StationRegistry::setClock(Clock clock)
{
    m_clock = clock ? std::move(clock) : Clock(wallClockSeconds);
}

std::optional<Station> StationRegistry::get(const QString &callsign) const
{
    const auto it = m_stations.constFind(callsign.toUpper());
    if (it == m_stations.constEnd()) return std::nullopt;
    return it.value();
}

void StationRegistry::clear()
{
    m_stations.clear();
}

Station StationRegistry::noteHeard(const QString &callsign, const QString &path)
{
    const QString key = callsign.toUpper();
    const double now = m_clock();
    auto it = m_stations.find(key);
    if (it == m_stations.end()) {
        Station station;
        station.callsign = key;
        station.firstHeard = now;
        it = m_stations.insert(key, station);
    }
    Station &station = it.value();
    station.lastHeard = now;
    station.packets += 1;
    if (!path.isEmpty()) station.path = path;
    // A path beginning with the Internet marker (Session) tags the station;
    // hearing it on the air again clears the tag.
    station.viaInternet = path.startsWith(QStringLiteral("APRS-IS"));
    return station;
}

Station StationRegistry::applyPacket(const QString &callsign, const AprsPacket &packet, const QString &path)
{
    noteHeard(callsign, path);
    Station &station = m_stations[callsign.toUpper()];

    if (packet.hasPosition()) {
        station.latitude = packet.latitude;
        station.longitude = packet.longitude;
        station.positionAt = m_clock();
        station.symbolTable = QString(packet.symbolTable);
        station.symbolCode = QString(packet.symbolCode);
        // Course and speed are only meaningful with the fix they came with,
        // so clear them when a report omits them rather than leaving a stale
        // heading attached to a new position.
        station.courseDeg = packet.courseDeg;
        station.speedKnots = packet.speedKnots;
        if (packet.altitudeM) station.altitudeM = packet.altitudeM;
        if (!packet.comment.isEmpty()) station.comment = packet.comment;
    }
    if (packet.weather) station.weather = packet.weather->describe();
    if (!packet.status.isEmpty()) station.status = packet.status;
    if (packet.kind == AprsPacket::Kind::Message && !packet.text.isEmpty()) station.lastMessage = packet.text;
    return station;
}

int StationRegistry::expire()
{
    if (expiryMinutes <= 0) return 0;
    const double cutoff = m_clock() - expiryMinutes * 60.0;
    int removed = 0;
    for (auto it = m_stations.begin(); it != m_stations.end();) {
        if (it.value().lastHeard < cutoff) { it = m_stations.erase(it); removed++; }
        else ++it;
    }
    return removed;
}

QList<Station> StationRegistry::listed(int limit) const
{
    QList<Station> all = m_stations.values();
    std::sort(all.begin(), all.end(), [](const Station &a, const Station &b) { return a.lastHeard > b.lastHeard; });
    if (limit >= 0 && all.size() > limit) all = all.mid(0, limit);
    return all;
}

QList<Station> StationRegistry::withPosition() const
{
    QList<Station> out;
    for (const Station &s : m_stations) if (s.hasPosition()) out.append(s);
    return out;
}
