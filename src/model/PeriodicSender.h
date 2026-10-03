/*
 * PeriodicSender.h - the text beacon and the APRS position report timer.
 *
 * Port of ax25chat/beacon.py.  Neither transmits by itself: both build a
 * frame and hand it to the ChannelManager queue, so they obey exactly the
 * same "wait for a clear frequency" rule as a typed message.  A transmission
 * still queued when the next interval elapses is replaced rather than
 * stacked (the caller drops the pending kind before enqueueing), so a long
 * busy period cannot produce a burst of stale beacons.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QString>

#include <functional>

class QTimer;

class PeriodicSender : public QObject
{
    Q_OBJECT

public:
    using SendFunction = std::function<bool()>;
    using Clock = std::function<double()>;   // seconds

    // label names the sender in log messages ("Beacon", "Position").
    PeriodicSender(const QString &label, SendFunction send, QObject *parent = nullptr);

    void setClock(Clock clock);

    // Apply settings; restarts the timer when asked.
    void configure(bool enabled, int intervalMinutes, bool restart = true);
    void start();
    void stop();

    bool running() const;
    int intervalMinutes() const { return m_intervalMinutes; }
    QString label() const { return m_label; }

    // Seconds until the next transmission, or -1 when disabled.
    int secondsRemaining() const;
    QString countdownText() const;

    // Send immediately and restart the interval.
    bool triggerNow();

signals:
    void fired();
    void logMessage(const QString &level, const QString &message);

private:
    bool fire();

    QString m_label;
    SendFunction m_send;
    Clock m_clock;
    int m_intervalMinutes = 30;
    double m_nextDue = 0.0;
    QTimer *m_timer = nullptr;
};
