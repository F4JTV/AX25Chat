/*
 * PhoneLocation.cpp - the device's own location service as a GPS source.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "PhoneLocation.h"

#include <QElapsedTimer>
#include <QGeoCoordinate>
#include <QGeoPositionInfo>
#include <QGeoPositionInfoSource>

#include <cmath>

namespace {

double monotonicSeconds()
{
    static QElapsedTimer timer;
    if (!timer.isValid()) timer.start();
    return timer.elapsed() / 1000.0;
}

constexpr double MetresPerSecondToKnots = 1.943844;

} // namespace

PhoneLocation::PhoneLocation(QObject *parent) : GpsProvider(parent)
{
    qRegisterMetaType<GpsFix>("GpsFix");
}

PhoneLocation::~PhoneLocation()
{
    close();
}

bool PhoneLocation::available()
{
    return !QGeoPositionInfoSource::availableSources().isEmpty();
}

double PhoneLocation::now() const
{
    return monotonicSeconds();
}

double PhoneLocation::fixAgeSeconds() const
{
    return m_fix.ageSeconds(now());
}

QString PhoneLocation::statusText() const
{
    if (!m_running) return QStringLiteral("GPS: closed");
    if (!m_fix.valid) return QStringLiteral("GPS: waiting for a fix");
    return QStringLiteral("GPS: %1").arg(m_fix.describe(now()));
}

void PhoneLocation::setStatus(const QString &status)
{
    if (status != m_status) {
        m_status = status;
        emit statusChanged(status);
    }
}

bool PhoneLocation::open(const QString &port, int baud)
{
    Q_UNUSED(port);
    Q_UNUSED(baud);
    close();
    m_source = QGeoPositionInfoSource::createDefaultSource(this);
    if (!m_source) {
        emit logMessage(QStringLiteral("error"),
                        QStringLiteral("No location service is available on this device."));
        return false;
    }
    m_source->setUpdateInterval(1000);
    m_source->setPreferredPositioningMethods(QGeoPositionInfoSource::AllPositioningMethods);
    connect(m_source, &QGeoPositionInfoSource::positionUpdated, this, &PhoneLocation::onUpdate);
    connect(m_source, &QGeoPositionInfoSource::errorOccurred, this, [this](QGeoPositionInfoSource::Error) { onError(); });
    m_source->startUpdates();
    m_running = true;
    m_fix = GpsFix();
    setStatus(QStringLiteral("open"));
    emit logMessage(QStringLiteral("info"),
                    QStringLiteral("Location updates started (%1).").arg(m_source->sourceName()));
    return true;
}

void PhoneLocation::close()
{
    if (m_source) {
        m_source->stopUpdates();
        m_source->deleteLater();
        m_source = nullptr;
    }
    m_running = false;
    setStatus(QStringLiteral("closed"));
}

void PhoneLocation::onUpdate(const QGeoPositionInfo &info)
{
    if (!info.isValid()) {
        m_fix.valid = false;
        setStatus(QStringLiteral("nofix"));
        emit fixChanged(m_fix);
        return;
    }
    const QGeoCoordinate c = info.coordinate();
    m_fix.latitude = c.latitude();
    m_fix.longitude = c.longitude();
    if (c.type() == QGeoCoordinate::Coordinate3D) m_fix.altitudeM = c.altitude();
    if (info.hasAttribute(QGeoPositionInfo::GroundSpeed)) {
        const double knots = info.attribute(QGeoPositionInfo::GroundSpeed) * MetresPerSecondToKnots;
        m_fix.speedKnots = knots;
        // A stationary receiver reports a meaningless, drifting direction;
        // keep it only when actually moving, as the NMEA reader does.
        if (info.hasAttribute(QGeoPositionInfo::Direction) && knots >= 1.0) {
            m_fix.courseDeg = info.attribute(QGeoPositionInfo::Direction);
        } else if (knots < 1.0) {
            m_fix.courseDeg.reset();
        }
    }
    if (info.hasAttribute(QGeoPositionInfo::HorizontalAccuracy)) {
        // No HDOP from the platform; the accuracy in metres is the closest
        // thing and is shown in the same slot.
        m_fix.hdop = info.attribute(QGeoPositionInfo::HorizontalAccuracy) / 5.0;
    }
    m_fix.utc = info.timestamp().toUTC().toString(QStringLiteral("HHmmss"));
    m_fix.fixQuality = 1;
    m_fix.valid = true;
    m_fix.updatedAt = now();
    setStatus(QStringLiteral("fix"));
    emit fixChanged(m_fix);
}

void PhoneLocation::onError()
{
    const QString reason = m_source ? QString::number(static_cast<int>(m_source->error())) : QString();
    emit logMessage(QStringLiteral("error"),
                    QStringLiteral("Location service error %1. Is the location permission granted and "
                                   "location switched on?").arg(reason));
    m_fix.valid = false;
    setStatus(QStringLiteral("nofix"));
}

void PhoneLocation::onTimeout()
{
}
