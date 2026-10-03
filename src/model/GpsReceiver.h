/*
 * GpsReceiver.h - NMEA 0183 reader over a serial port.
 *
 * Port of ax25chat/gps.py.  GGA and RMC are both parsed because neither
 * alone suffices: GGA carries altitude and satellite count, RMC carries
 * speed and course.  Sentences are matched on the last three characters
 * of the header rather than a "$GP" prefix, because a multi-constellation
 * dongle emits $GNGGA and $GNRMC.  Checksums are verified; sentences
 * without one are accepted (a few cheap receivers omit it), a mismatched
 * one is rejected.
 *
 * Two refusals are deliberate: a stale fix is reported as such rather than
 * as current, and a course is dropped when the receiver reports a speed
 * under one knot, because a stationary GPS produces a meaningless drifting
 * course and APRS reads a course of zero as "unknown" anyway.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QList>
#include <QMetaType>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

class QSerialPort;
class QTimer;

namespace gps {

inline const QList<int> CommonBaudRates = {4800, 9600, 19200, 38400, 57600, 115200};
constexpr int DefaultBaud = 4800;

// XOR of everything between '$' and '*'.  True when absent.
bool verifyChecksum(const QString &sentence);

// NMEA ddmm.mmmm plus hemisphere to decimal degrees.
std::optional<double> nmeaDegrees(const QString &value, const QString &hemisphere);

// Serial ports on this machine as (name, description).
QList<std::pair<QString, QString>> availablePorts();

} // namespace gps


// The latest known position, merged from GGA and RMC.
struct GpsFix
{
    double latitude = 0.0;
    double longitude = 0.0;
    std::optional<double> altitudeM;
    std::optional<double> speedKnots;
    std::optional<double> courseDeg;
    int satellites = 0;
    std::optional<double> hdop;
    int fixQuality = 0;              // GGA field 6, 0 means no fix
    bool valid = false;              // RMC status A, or GGA quality > 0
    double updatedAt = 0.0;          // monotonic seconds
    QString utc;

    double ageSeconds(double now) const { return std::max(0.0, now - updatedAt); }
    bool isUsable(double now, double maxAgeS = 30.0) const { return valid && ageSeconds(now) <= maxAgeS; }
    QString describe(double now) const;
};
Q_DECLARE_METATYPE(GpsFix)


// Accumulates GGA and RMC into a single fix.
class NmeaParser
{
public:
    using Clock = std::function<double()>;

    NmeaParser();
    void setClock(Clock clock);

    GpsFix fix;
    int sentencesSeen = 0;
    int sentencesRejected = 0;
    QStringList talkers() const;

    // Parse one sentence.  Returns the fix when a position sentence was
    // processed (valid or not), nullopt for anything else.
    std::optional<GpsFix> feed(const QString &sentence);

private:
    std::optional<GpsFix> parseGga(const QStringList &f);
    std::optional<GpsFix> parseRmc(const QStringList &f);

    Clock m_clock;
    QSet<QString> m_talkers;
};


// Reads NMEA from a serial port and publishes the current fix.
class GpsReceiver : public QObject
{
    Q_OBJECT

public:
    // If nothing arrives at all, the port is probably wrong or the baud rate
    // does not match; say so instead of sitting silent for ever.
    static constexpr int SilenceTimeoutS = 12;

    explicit GpsReceiver(QObject *parent = nullptr);
    ~GpsReceiver() override;

    bool isOpen() const;
    GpsFix fix() const { return m_parser.fix; }
    double fixAgeSeconds() const;          // seconds since the fix was last updated
    QString status() const { return m_status; }   // closed, open, fix, nofix
    QString statusText() const;

    bool open(const QString &portName, int baud = gps::DefaultBaud);
    void close();

    // Feed text as if it came from the port (tests, replay).
    void feedText(const QByteArray &data);

signals:
    void fixChanged(const GpsFix &fix);
    void statusChanged(const QString &status);
    void logMessage(const QString &level, const QString &message);

private:
    void setStatus(const QString &status);
    void onReadyRead();
    void publish(const GpsFix &fix);
    void checkHealth();
    double now() const;

    QSerialPort *m_port = nullptr;
    QString m_buffer;
    NmeaParser m_parser;
    QString m_status = QStringLiteral("closed");
    double m_openedAt = 0.0;
    bool m_warnedSilent = false;
    bool m_warnedNoFix = false;
    QTimer *m_watchdog = nullptr;
};
