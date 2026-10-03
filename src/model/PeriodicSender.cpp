/*
 * PeriodicSender.cpp - the text beacon and the APRS position report timer.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "PeriodicSender.h"

#include <QDateTime>
#include <QTimer>

namespace {

double wallClockSeconds()
{
    return QDateTime::currentMSecsSinceEpoch() / 1000.0;
}

} // namespace

PeriodicSender::PeriodicSender(const QString &label, SendFunction send, QObject *parent)
    : QObject(parent), m_label(label), m_send(std::move(send)), m_clock(wallClockSeconds)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this] { fire(); });
}

void PeriodicSender::setClock(Clock clock)
{
    m_clock = clock ? std::move(clock) : Clock(wallClockSeconds);
}

void PeriodicSender::configure(bool enabled, int intervalMinutes, bool restart)
{
    m_intervalMinutes = qMax(1, intervalMinutes);
    if (!restart) return;
    stop();
    if (enabled) start();
}

void PeriodicSender::start()
{
    const int minutes = m_intervalMinutes;
    m_timer->start(minutes * 60 * 1000);
    m_nextDue = m_clock() + minutes * 60.0;
    emit logMessage(QStringLiteral("info"), QStringLiteral("%1 enabled, every %2 min").arg(m_label).arg(minutes));
}

void PeriodicSender::stop()
{
    if (m_timer->isActive()) {
        m_timer->stop();
        emit logMessage(QStringLiteral("info"), QStringLiteral("%1 disabled").arg(m_label));
    }
    m_nextDue = 0.0;
}

bool PeriodicSender::running() const
{
    return m_timer->isActive();
}

int PeriodicSender::secondsRemaining() const
{
    if (!m_timer->isActive()) return -1;
    return qMax(0, static_cast<int>(m_nextDue - m_clock()));
}

QString PeriodicSender::countdownText() const
{
    const int remaining = secondsRemaining();
    if (remaining < 0) return QStringLiteral("%1: off").arg(m_label);
    return QStringLiteral("%1: next in %2:%3").arg(m_label)
        .arg(remaining / 60, 2, 10, QChar('0')).arg(remaining % 60, 2, 10, QChar('0'));
}

bool PeriodicSender::triggerNow()
{
    const bool result = fire();
    if (m_timer->isActive()) start();     // a full interval from now, not from the last
    return result;
}

bool PeriodicSender::fire()
{
    m_nextDue = m_clock() + m_intervalMinutes * 60.0;
    emit fired();
    if (!m_send) return false;
    return m_send();
}
