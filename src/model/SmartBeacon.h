/*
 * SmartBeacon.h - speed and turn adaptive position interval.
 *
 * Port of ax25chat/smartbeacon.py.  Algorithm by Tony Arnerich KD7TA and
 * Steve Bragg KA9MVA, written for the HamHUD and since adopted by Xastir,
 * APRSdroid, the Kenwood mobiles and most other trackers.  It replaces a
 * fixed position interval with one derived from how fast the station moves
 * and whether it has just turned.
 *
 * Rate by speed: below lowSpeed the station counts as stopped and beacons at
 * the slow interval; above highSpeed at the fast interval; between the two
 * the interval is inversely proportional to speed, so the distance travelled
 * between reports stays roughly constant.  The computed interval is clamped
 * to the slow interval, because the plain formula is discontinuous at the
 * low threshold (36 minutes just above walking pace with the classic
 * parameters, slower than the 30 minutes used when stopped); Xastir applies
 * the same clamp.
 *
 * Corner pegging: a heading change beyond a threshold reports immediately.
 * The threshold widens as speed falls, because at walking pace a GPS heading
 * wanders by tens of degrees on its own.  turnTime is a floor between two
 * corner-pegged reports so a roundabout does not produce a burst.
 *
 * Speeds are in km/h; published parameter sets are usually quoted in mph.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QString>

#include <functional>
#include <optional>

struct SmartBeaconSettings
{
    double lowSpeedKmh = 8.0;      // below this the station counts as stopped
    double highSpeedKmh = 96.0;    // above this the fast interval applies
    int slowIntervalS = 1800;      // interval when stopped
    int fastIntervalS = 180;       // interval at or above highSpeed
    double turnMinDeg = 28.0;      // smallest heading change worth reporting
    double turnSlope = 26.0;       // widens the threshold at low speed: x10 degrees.mph, as on a Kenwood
    int turnTimeS = 15;            // floor between corner-pegged reports
};

class SmartBeaconer
{
public:
    // Seconds since some epoch; replaceable for tests.
    using Clock = std::function<double()>;

    explicit SmartBeaconer(const SmartBeaconSettings &settings = SmartBeaconSettings());

    SmartBeaconSettings settings;

    void setClock(Clock clock);

    void reset();

    // Record that a position went out, with the heading it carried.
    void noteSent(std::optional<double> courseDeg = std::nullopt);

    double secondsSinceSent() const;       // infinity before the first report
    QString lastReason() const { return m_lastReason; }

    // The interval this speed calls for, in seconds.
    double intervalForSpeed(double speedKmh) const;

    // Heading change worth a report at this speed, degrees, capped at 180.
    double turnThreshold(double speedKmh) const;

    // Smallest angle between two headings, 0 to 180 degrees.
    static double courseChange(double previous, double current);

    // Should a position go out now?  A missing speed is treated as
    // stationary: a GPS with a fix but no speed is parked.
    bool evaluate(std::optional<double> speedKnots, std::optional<double> courseDeg, QString *reason = nullptr);

    // The same decision from a fix with coordinates: the speed and course a
    // receiver leaves out are derived from the movement since the previous
    // sample, and the larger of the fix's speed and the derived one is
    // used, as APRSdroid does.  Call once per fix.
    bool evaluateFix(double latitude, double longitude, std::optional<double> speedKnots,
                     std::optional<double> courseDeg, QString *reason = nullptr);

    // One line for the side panel.
    QString statusText(std::optional<double> speedKnots) const;

    // The resulting timetable, for the configuration page.
    QString preview() const;

private:
    Clock m_clock;
    struct Sample { double latitude = 0, longitude = 0, at = 0; bool valid = false; };
    Sample m_lastSample;
    double m_lastSent = 0.0;
    std::optional<double> m_lastCourse;
    QString m_lastReason;
};
