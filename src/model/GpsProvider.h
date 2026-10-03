/*
 * GpsProvider.h - where the current position comes from.
 *
 * Two implementations share this interface: GpsReceiver, an NMEA receiver
 * on a serial port (desktop), and PhoneLocation, the device's own location
 * service through Qt Positioning (Android, or any machine where Qt has a
 * position source). The session and the user interfaces only see this.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "GpsReceiver.h"

#include <QObject>
#include <QString>

class GpsProvider : public QObject
{
    Q_OBJECT

public:
    explicit GpsProvider(QObject *parent = nullptr) : QObject(parent) {}

    // Human readable name of the source kind: "serial NMEA", "phone location".
    virtual QString kind() const = 0;
    // True when the source has no port to configure (phone location).
    virtual bool needsPort() const = 0;

    virtual bool isOpen() const = 0;
    virtual GpsFix fix() const = 0;
    virtual double fixAgeSeconds() const = 0;
    virtual QString status() const = 0;       // closed, open, fix, nofix
    virtual QString statusText() const = 0;

    // port and baud are ignored by sources that do not need them.
    virtual bool open(const QString &port, int baud) = 0;
    virtual void close() = 0;

    // The provider for this platform: PhoneLocation where Qt Positioning
    // is available and no serial port is, GpsReceiver otherwise.
    static GpsProvider *create(QObject *parent = nullptr);

signals:
    void fixChanged(const GpsFix &fix);
    void statusChanged(const QString &status);
    void logMessage(const QString &level, const QString &message);
};

// GpsReceiver behind the interface.
class SerialGpsProvider : public GpsProvider
{
    Q_OBJECT

public:
    explicit SerialGpsProvider(QObject *parent = nullptr);

    QString kind() const override { return QStringLiteral("serial NMEA"); }
    bool needsPort() const override { return true; }
    bool isOpen() const override { return m_receiver->isOpen(); }
    GpsFix fix() const override { return m_receiver->fix(); }
    double fixAgeSeconds() const override { return m_receiver->fixAgeSeconds(); }
    QString status() const override { return m_receiver->status(); }
    QString statusText() const override { return m_receiver->statusText(); }
    bool open(const QString &port, int baud) override { return m_receiver->open(port, baud); }
    void close() override { m_receiver->close(); }

    GpsReceiver *receiver() const { return m_receiver; }

private:
    GpsReceiver *m_receiver;
};
