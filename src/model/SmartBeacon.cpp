/*
 * SmartBeacon.cpp - speed and turn adaptive position interval.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "SmartBeacon.h"

#include "AprsDecoder.h"
#include "AprsEncoder.h"

#include <QDateTime>

#include <cmath>
#include <limits>

namespace {

constexpr double KnotsToKmh = 1.852;

double wallClockSeconds()
{
    return QDateTime::currentMSecsSinceEpoch() / 1000.0;
}

} // namespace


SmartBeaconer::SmartBeaconer(const SmartBeaconSettings &settingsIn)
    : settings(settingsIn), m_clock(wallClockSeconds)
{
}

void SmartBeaconer::setClock(Clock clock)
{
    m_clock = clock ? std::move(clock) : Clock(wallClockSeconds);
}

void SmartBeaconer::reset()
{
    m_lastSent = 0.0;
    m_lastCourse.reset();
    m_lastReason.clear();
    m_lastSample = Sample();
}

// The course of the report that went out, or none when the station had no
// heading (parked): the next heading then counts as "started moving".
void SmartBeaconer::noteSent(std::optional<double> courseDeg)
{
    m_lastSent = m_clock();
    m_lastCourse = courseDeg;
}

double SmartBeaconer::secondsSinceSent() const
{
    if (m_lastSent == 0.0) return std::numeric_limits<double>::infinity();
    return m_clock() - m_lastSent;
}

double SmartBeaconer::intervalForSpeed(double speedKmh) const
{
    const SmartBeaconSettings &s = settings;
    if (speedKmh < s.lowSpeedKmh) return static_cast<double>(s.slowIntervalS);
    if (speedKmh >= s.highSpeedKmh) return static_cast<double>(s.fastIntervalS);
    // Inversely proportional, so the distance between reports stays similar.
    const double interval = s.fastIntervalS * s.highSpeedKmh / std::max(0.1, speedKmh);
    return std::min(interval, static_cast<double>(s.slowIntervalS));
}

// The reference formula (HamHUD, Kenwood): threshold = turn angle +
// turn slope / speed in mph, the slope being kept in tens of degrees.mph.
double SmartBeaconer::turnThreshold(double speedKmh) const
{
    if (speedKmh < 1.0) return 180.0;
    const double speedMph = speedKmh / 1.609344;
    return std::min(180.0, settings.turnMinDeg + settings.turnSlope * 10.0 / speedMph);
}

double SmartBeaconer::courseChange(double previous, double current)
{
    double difference = std::fmod(std::fabs(current - previous), 360.0);
    return difference > 180.0 ? 360.0 - difference : difference;
}

bool SmartBeaconer::evaluate(std::optional<double> speedKnots, std::optional<double> courseDeg,
                             QString *reason)
{
    const double speedKmh = speedKnots.value_or(0.0) * KnotsToKmh;
    const double elapsed = secondsSinceSent();

    auto answer = [&](bool decision, const QString &why) {
        m_lastReason = why;
        if (reason) *reason = why;
        return decision;
    };

    // Nothing has ever been sent: send the first one immediately so the
    // station appears on other maps without waiting half an hour.
    if (m_lastSent == 0.0) {
        return answer(true, QStringLiteral("first report"));
    }

    // Started moving: the last report had no heading (stopped), this fix has
    // one and the station is above the stopped speed.  Reported after the
    // turn time, as APRSdroid does, rather than after the slow interval.
    if (courseDeg && !m_lastCourse && speedKmh >= settings.lowSpeedKmh && elapsed >= settings.turnTimeS) {
        return answer(true, QStringLiteral("started moving, %1 km/h heading %2 deg")
                                .arg(std::lround(speedKmh)).arg(std::lround(*courseDeg)));
    }
    // Corner pegging is checked next: its whole purpose is to pre-empt the
    // interval.
    if (courseDeg && m_lastCourse) {
        const double change = courseChange(*m_lastCourse, *courseDeg);
        const double threshold = turnThreshold(speedKmh);
        if (change >= threshold && elapsed >= settings.turnTimeS) {
            return answer(true, QStringLiteral("turned %1 deg, threshold %2 deg at %3 km/h")
                                    .arg(std::lround(change)).arg(std::lround(threshold)).arg(std::lround(speedKmh)));
        }
    }

    const double interval = intervalForSpeed(speedKmh);
    if (elapsed >= interval) {
        return answer(true, QStringLiteral("%1 min since the last report, interval %2 min at %3 km/h")
                                .arg(elapsed / 60.0, 0, 'f', 1).arg(interval / 60.0, 0, 'f', 1).arg(std::lround(speedKmh)));
    }
    return answer(false, QStringLiteral("%1 s to go, interval %2 min at %3 km/h")
                             .arg(std::lround(interval - elapsed)).arg(interval / 60.0, 0, 'f', 1).arg(std::lround(speedKmh)));
}

bool SmartBeaconer::evaluateFix(double latitude, double longitude, std::optional<double> speedKnots,
                                std::optional<double> courseDeg, QString *reason)
{
    const double now = m_clock();
    std::optional<double> speed = speedKnots;
    std::optional<double> course = courseDeg;
    if (m_lastSample.valid) {
        const double dt = now - m_lastSample.at;
        if (dt >= 1.0) {
            const double km = aprs::distanceKm(m_lastSample.latitude, m_lastSample.longitude, latitude, longitude);
            const double derivedKnots = km / dt * 3600.0 / KnotsToKmh;
            // Position noise at a standstill looks like slow movement: the
            // derived speed only counts once the station moved a real distance.
            if (km >= 0.010) {
                if (!speed || derivedKnots > *speed) speed = derivedKnots;
                if (!course) course = AprsDecoder::bearingDeg(m_lastSample.latitude, m_lastSample.longitude, latitude, longitude);
            } else if (!speed) {
                speed = 0.0;
            }
        }
    }
    m_lastSample = {latitude, longitude, now, true};
    return evaluate(speed, course, reason);
}

QString SmartBeaconer::statusText(std::optional<double> speedKnots) const
{
    const double speedKmh = speedKnots.value_or(0.0) * KnotsToKmh;
    const double interval = intervalForSpeed(speedKmh);
    if (m_lastSent == 0.0) return QStringLiteral("SmartBeacon: waiting for the first fix");
    const int remaining = static_cast<int>(std::max(0.0, interval - secondsSinceSent()));
    return QStringLiteral("SmartBeacon: next in %1:%2 at %3 km/h")
        .arg(remaining / 60, 2, 10, QChar('0'))
        .arg(remaining % 60, 2, 10, QChar('0'))
        .arg(std::lround(speedKmh));
}

QString SmartBeaconer::preview() const
{
    QStringList rows;
    for (const int speed : {0, 5, 10, 30, 50, 90, 130}) {
        const double interval = intervalForSpeed(speed);
        if (interval >= 60.0) rows << QStringLiteral("%1 km/h: %2 min").arg(speed).arg(std::lround(interval / 60.0));
        else rows << QStringLiteral("%1 km/h: %2 s").arg(speed).arg(std::lround(interval));
    }
    return rows.join(QStringLiteral(", "));
}
