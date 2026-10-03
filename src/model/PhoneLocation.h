/*
 * PhoneLocation.h - the device's own location service as a GPS source.
 *
 * Qt Positioning delivers positions from whatever the platform has: the
 * GPS of a phone, geoclue on a Linux desktop. The updates are merged into
 * the same GpsFix that the serial NMEA receiver produces, so the rest of
 * the program does not know the difference.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "GpsProvider.h"

class QGeoPositionInfoSource;
class QGeoPositionInfo;

class PhoneLocation : public GpsProvider
{
    Q_OBJECT

public:
    explicit PhoneLocation(QObject *parent = nullptr);
    ~PhoneLocation() override;

    // False when Qt has no position source on this machine.
    static bool available();

    QString kind() const override { return QStringLiteral("phone location"); }
    bool needsPort() const override { return false; }
    bool isOpen() const override { return m_running; }
    GpsFix fix() const override { return m_fix; }
    double fixAgeSeconds() const override;
    QString status() const override { return m_status; }
    QString statusText() const override;
    bool open(const QString &port, int baud) override;
    void close() override;

private:
    void onUpdate(const QGeoPositionInfo &info);
    void onError();
    void onTimeout();
    void setStatus(const QString &status);
    double now() const;

    QGeoPositionInfoSource *m_source = nullptr;
    GpsFix m_fix;
    QString m_status = QStringLiteral("closed");
    bool m_running = false;
};
