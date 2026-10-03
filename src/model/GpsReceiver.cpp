/*
 * GpsReceiver.cpp - NMEA 0183 reader over a serial port.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "GpsReceiver.h"

#include <QElapsedTimer>
#include <QTimer>

// Qt Serial Port does not exist on Android: the parser and the fix model
// stay, the port itself becomes a stub that says so.
#ifdef AX25CHAT_HAVE_SERIALPORT
#include <QSerialPort>
#include <QSerialPortInfo>
#endif

#include <cmath>

namespace {

double monotonicSeconds()
{
    static QElapsedTimer timer;
    if (!timer.isValid()) timer.start();
    return timer.elapsed() / 1000.0;
}

std::optional<double> toDouble(const QString &value)
{
    bool ok = false;
    const double d = value.trimmed().toDouble(&ok);
    if (!ok) return std::nullopt;
    return d;
}

int toInt(const QString &value)
{
    bool ok = false;
    const int n = value.trimmed().toInt(&ok);
    return ok ? n : 0;
}

} // namespace


namespace gps {

bool verifyChecksum(const QString &sentence)
{
    const int star = sentence.indexOf('*');
    if (star < 0) return true;
    QString body = sentence.left(star);
    while (body.startsWith('$')) body.remove(0, 1);
    const QString given = sentence.mid(star + 1).trimmed().left(2);
    if (given.size() != 2) return false;
    int checksum = 0;
    for (const QChar c : body) checksum ^= c.unicode();
    bool ok = false;
    const int expected = given.toInt(&ok, 16);
    return ok && checksum == expected;
}

std::optional<double> nmeaDegrees(const QString &value, const QString &hemisphere)
{
    if (value.isEmpty() || hemisphere.isEmpty()) return std::nullopt;
    const int dot = value.indexOf('.');
    if (dot < 3) return std::nullopt;
    bool okDeg = false, okMin = false;
    const int degrees = value.left(dot - 2).toInt(&okDeg);
    const double minutes = value.mid(dot - 2).toDouble(&okMin);
    if (!okDeg || !okMin) return std::nullopt;
    double result = degrees + minutes / 60.0;
    const QString h = hemisphere.toUpper();
    if (h == QLatin1String("S") || h == QLatin1String("W")) result = -result;
    return result;
}

QList<std::pair<QString, QString>> availablePorts()
{
    QList<std::pair<QString, QString>> ports;
#ifdef AX25CHAT_HAVE_SERIALPORT
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        QStringList label;
        if (!info.description().isEmpty()) label << info.description();
        if (!info.manufacturer().isEmpty()) label << info.manufacturer();
        ports.append({info.portName(), label.isEmpty() ? QStringLiteral("serial port") : label.join(' ')});
    }
#endif
    return ports;
}

} // namespace gps


QString GpsFix::describe(double now) const
{
    if (!valid) return QStringLiteral("no fix");
    QStringList parts;
    parts << QStringLiteral("%1%2").arg(std::fabs(latitude), 0, 'f', 5).arg(latitude >= 0 ? QStringLiteral("N") : QStringLiteral("S"));
    parts << QStringLiteral("%1%2").arg(std::fabs(longitude), 0, 'f', 5).arg(longitude >= 0 ? QStringLiteral("E") : QStringLiteral("W"));
    if (satellites) parts << QStringLiteral("%1 sats").arg(satellites);
    if (hdop) parts << QStringLiteral("HDOP %1").arg(*hdop, 0, 'f', 1);
    parts << QStringLiteral("%1s ago").arg(std::lround(ageSeconds(now)));
    return parts.join(QStringLiteral(", "));
}


// ---- NmeaParser ---------------------------------------------------------------------

NmeaParser::NmeaParser() : m_clock(monotonicSeconds) {}

void NmeaParser::setClock(Clock clock)
{
    m_clock = clock ? std::move(clock) : Clock(monotonicSeconds);
}

QStringList NmeaParser::talkers() const
{
    QStringList list(m_talkers.begin(), m_talkers.end());
    list.sort();
    return list;
}

std::optional<GpsFix> NmeaParser::feed(const QString &sentenceIn)
{
    const QString sentence = sentenceIn.trimmed();
    if (!sentence.startsWith('$') || sentence.size() < 9) return std::nullopt;
    if (!gps::verifyChecksum(sentence)) {
        sentencesRejected++;
        return std::nullopt;
    }
    sentencesSeen++;
    const QString body = sentence.section('*', 0, 0);
    const QStringList fields = body.split(',');
    QString header = fields.at(0);
    while (header.startsWith('$')) header.remove(0, 1);
    if (header.size() < 5) return std::nullopt;
    m_talkers.insert(header.left(2));
    const QString kind = header.mid(2, 3).toUpper();
    if (kind == QLatin1String("GGA")) return parseGga(fields);
    if (kind == QLatin1String("RMC")) return parseRmc(fields);
    return std::nullopt;
}

std::optional<GpsFix> NmeaParser::parseGga(const QStringList &f)
{
    if (f.size() < 10) return std::nullopt;
    const int quality = toInt(f.at(6));
    const auto latitude = gps::nmeaDegrees(f.at(2), f.at(3));
    const auto longitude = gps::nmeaDegrees(f.at(4), f.at(5));
    fix.fixQuality = quality;
    fix.satellites = toInt(f.at(7));
    fix.hdop = toDouble(f.at(8));
    fix.utc = f.at(1);
    if (quality > 0 && latitude && longitude) {
        fix.latitude = *latitude;
        fix.longitude = *longitude;
        if (const auto altitude = toDouble(f.at(9))) fix.altitudeM = altitude;
        fix.valid = true;
        fix.updatedAt = m_clock();
        return fix;
    }
    // Quality 0 means the receiver has lost the fix; say so rather than
    // continuing to report the last known position as if it were current.
    if (quality == 0) fix.valid = false;
    return fix;
}

std::optional<GpsFix> NmeaParser::parseRmc(const QStringList &f)
{
    if (f.size() < 9) return std::nullopt;
    const QString status = f.at(2).toUpper();
    const auto latitude = gps::nmeaDegrees(f.at(3), f.at(4));
    const auto longitude = gps::nmeaDegrees(f.at(5), f.at(6));
    if (status != QLatin1String("A") || !latitude || !longitude) {
        fix.valid = false;
        return fix;
    }
    fix.latitude = *latitude;
    fix.longitude = *longitude;
    const auto speed = toDouble(f.at(7));
    const auto course = toDouble(f.at(8));
    if (speed) fix.speedKnots = speed;
    // A stationary receiver reports a meaningless, drifting course; APRS
    // treats course 0 as unknown, so only keep it when actually moving.
    if (course && speed && *speed >= 1.0) fix.courseDeg = course;
    else if (speed && *speed < 1.0) fix.courseDeg.reset();
    fix.utc = f.at(1);
    fix.valid = true;
    fix.updatedAt = m_clock();
    return fix;
}


// ---- GpsReceiver -----------------------------------------------------------------

GpsReceiver::GpsReceiver(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<GpsFix>("GpsFix");
    m_watchdog = new QTimer(this);
    m_watchdog->setInterval(2000);
    connect(m_watchdog, &QTimer::timeout, this, &GpsReceiver::checkHealth);
}

GpsReceiver::~GpsReceiver()
{
    close();
}

double GpsReceiver::now() const
{
    return monotonicSeconds();
}

double GpsReceiver::fixAgeSeconds() const
{
    return m_parser.fix.ageSeconds(now());
}

bool GpsReceiver::isOpen() const
{
#ifdef AX25CHAT_HAVE_SERIALPORT
    return m_port && m_port->isOpen();
#else
    return false;
#endif
}

QString GpsReceiver::statusText() const
{
    if (!isOpen()) return QStringLiteral("GPS: closed");
    if (!m_parser.sentencesSeen) return QStringLiteral("GPS: open, no data yet");
    const GpsFix &fix = m_parser.fix;
    if (!fix.valid) return QStringLiteral("GPS: no fix (%1 sats)").arg(fix.satellites);
    return QStringLiteral("GPS: %1").arg(fix.describe(now()));
}

void GpsReceiver::setStatus(const QString &status)
{
    if (status != m_status) {
        m_status = status;
        emit statusChanged(status);
    }
}

bool GpsReceiver::open(const QString &portName, int baud)
{
    close();
    if (portName.isEmpty()) {
        emit logMessage(QStringLiteral("error"), QStringLiteral("No GPS serial port selected."));
        return false;
    }
#ifndef AX25CHAT_HAVE_SERIALPORT
    Q_UNUSED(baud);
    emit logMessage(QStringLiteral("error"), QStringLiteral("Serial ports are not available on this platform."));
    return false;
#else
    auto *port = new QSerialPort(this);
    port->setPortName(portName);
    port->setBaudRate(baud);
    port->setDataBits(QSerialPort::Data8);
    port->setParity(QSerialPort::NoParity);
    port->setStopBits(QSerialPort::OneStop);
    port->setFlowControl(QSerialPort::NoFlowControl);
    if (!port->open(QIODevice::ReadOnly)) {
        emit logMessage(QStringLiteral("error"),
                        QStringLiteral("Cannot open GPS port %1: %2. Another program may be holding it.")
                            .arg(portName, port->errorString()));
        port->deleteLater();
        return false;
    }
    connect(port, &QSerialPort::readyRead, this, &GpsReceiver::onReadyRead);
    connect(port, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::NoError) return;
        const QString message = m_port ? m_port->errorString() : QString::number(error);
        // A dongle unplugged mid-session lands here; close cleanly rather
        // than leaving a dead port object that never delivers again.
        emit logMessage(QStringLiteral("error"), QStringLiteral("GPS serial error: %1").arg(message));
        close();
    });
    m_port = port;
    m_buffer.clear();
    m_parser = NmeaParser();
    m_openedAt = now();
    m_warnedSilent = false;
    m_warnedNoFix = false;
    m_watchdog->start();
    setStatus(QStringLiteral("open"));
    emit logMessage(QStringLiteral("info"), QStringLiteral("GPS port %1 open at %2 baud.").arg(portName).arg(baud));
    return true;
#endif
}

void GpsReceiver::close()
{
    m_watchdog->stop();
#ifdef AX25CHAT_HAVE_SERIALPORT
    QSerialPort *port = m_port;
    m_port = nullptr;
    if (port) {
        port->disconnect(this);
        if (port->isOpen()) port->close();
        port->deleteLater();
    }
#endif
    setStatus(QStringLiteral("closed"));
}

void GpsReceiver::onReadyRead()
{
#ifdef AX25CHAT_HAVE_SERIALPORT
    if (!m_port) return;
    feedText(m_port->readAll());
#endif
}

void GpsReceiver::feedText(const QByteArray &data)
{
    // NMEA is ASCII; a stray byte from line noise must not kill the reader.
    m_buffer += QString::fromLatin1(data);
    if (m_buffer.size() > 8192) m_buffer = m_buffer.right(2048);
    int newline;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const QString line = m_buffer.left(newline);
        m_buffer.remove(0, newline + 1);
        if (const auto fix = m_parser.feed(line)) publish(*fix);
    }
}

void GpsReceiver::publish(const GpsFix &fix)
{
    setStatus(fix.valid ? QStringLiteral("fix") : QStringLiteral("nofix"));
    if (fix.valid) m_warnedNoFix = false;
    emit fixChanged(fix);
}

// Warn once about the two failures that look identical from outside.
void GpsReceiver::checkHealth()
{
    if (!isOpen()) return;
    const double elapsed = now() - m_openedAt;
    if (!m_parser.sentencesSeen && elapsed > SilenceTimeoutS) {
        if (!m_warnedSilent) {
            m_warnedSilent = true;
            emit logMessage(QStringLiteral("error"),
                            QStringLiteral("No NMEA data on the GPS port after %1 seconds. Check the port and "
                                           "the baud rate; most dongles use 4800 or 9600.").arg(SilenceTimeoutS));
        }
        return;
    }
    if (m_parser.sentencesSeen && !m_parser.fix.valid && elapsed > 60 && !m_warnedNoFix) {
        m_warnedNoFix = true;
        emit logMessage(QStringLiteral("info"),
                        QStringLiteral("GPS is receiving data but has no fix yet (%1 satellites). A cold start "
                                       "outdoors can take several minutes.").arg(m_parser.fix.satellites));
    }
}
