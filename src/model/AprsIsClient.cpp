/*
 * AprsIsClient.cpp - a client of the APRS Internet System.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "AprsIsClient.h"

#include "AprsEncoder.h"

#include <QDateTime>
#include <QTimer>

#include <cmath>

namespace {
constexpr int WatchdogMs = 150000;      // two minutes and a half without a byte
constexpr int KeepaliveMs = 60000;
constexpr int MaxReconnectDelayS = 120;
}

AprsIsClient::AprsIsClient(QObject *parent) : QObject(parent)
{
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &AprsIsClient::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &AprsIsClient::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &AprsIsClient::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &AprsIsClient::onError);

    m_watchdog = new QTimer(this);
    m_watchdog->setSingleShot(true);
    connect(m_watchdog, &QTimer::timeout, this, &AprsIsClient::onWatchdog);
    m_keepalive = new QTimer(this);
    connect(m_keepalive, &QTimer::timeout, this, &AprsIsClient::onKeepalive);
    m_reconnect = new QTimer(this);
    m_reconnect->setSingleShot(true);
    connect(m_reconnect, &QTimer::timeout, this, &AprsIsClient::connectToServer);
}

AprsIsClient::~AprsIsClient()
{
    stop();
}

void AprsIsClient::configure(const Settings &settings)
{
    const bool changed = settings.host != m_settings.host || settings.port != m_settings.port
                      || settings.callsign != m_settings.callsign || settings.passcode != m_settings.passcode;
    const bool radiusChanged = settings.radiusKm != m_settings.radiusKm;
    m_settings = settings;
    if (m_running && changed) {
        stop();
        start();
    } else if (m_running && radiusChanged && m_state == State::Connected) {
        m_filterSent.reset();
        const QString spec = filterSpec();
        // "filter default" is the server's own reset: back to messages for
        // us only, nothing around us.
        sendLine(spec.isEmpty() ? QStringLiteral("#filter default") : QStringLiteral("#filter %1").arg(spec));
        if (!spec.isEmpty()) {
            m_filterSent = m_position;
            m_filterSentAt = QDateTime::currentMSecsSinceEpoch();
        }
    }
}

QString AprsIsClient::statusText() const
{
    switch (m_state) {
        case State::Stopped: return QStringLiteral("APRS-IS: off");
        case State::Connecting: return QStringLiteral("APRS-IS: connecting to %1").arg(m_settings.host);
        case State::LoggingIn: return QStringLiteral("APRS-IS: logging in");
        case State::Connected: {
            QString note;
            if (!m_verified) {
                note = m_settings.passcode < 0 ? QStringLiteral(" (no passcode: download only)")
                                               : QStringLiteral(" (passcode refused: download only)");
            }
            return QStringLiteral("APRS-IS: %1%2, %3 in / %4 out")
                .arg(m_serverName.isEmpty() ? m_settings.host : m_serverName, note)
                .arg(m_received).arg(m_sent);
        }
    }
    return QString();
}

void AprsIsClient::start()
{
    if (m_running) return;
    m_running = true;
    m_reconnectDelayS = 5;
    connectToServer();
}

void AprsIsClient::stop()
{
    m_running = false;
    m_reconnect->stop();
    m_watchdog->stop();
    m_keepalive->stop();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) m_socket->abort();
    }
    m_buffer.clear();
    m_verified = false;
    m_serverName.clear();
    m_filterSent.reset();
    setState(State::Stopped);
}

void AprsIsClient::connectToServer()
{
    if (!m_running) return;
    m_buffer.clear();
    m_verified = false;
    m_warnedUnverified = false;
    m_serverName.clear();
    m_filterSent.reset();
    setState(State::Connecting);
    emit logMessage(QStringLiteral("info"), QStringLiteral("APRS-IS: connecting to %1:%2").arg(m_settings.host).arg(m_settings.port));
    m_socket->connectToHost(m_settings.host, static_cast<quint16>(m_settings.port));
    m_watchdog->start(WatchdogMs);
}

QString AprsIsClient::filterSpec() const
{
    if (!m_position || m_settings.radiusKm <= 0) return QString();
    return QStringLiteral("r/%1/%2/%3")
        .arg(m_position->first, 0, 'f', 3).arg(m_position->second, 0, 'f', 3).arg(m_settings.radiusKm);
}

void AprsIsClient::onConnected()
{
    // TCP keepalive probes under the comment lines, so a connection the
    // network dropped is noticed by the kernel too.
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    setState(State::LoggingIn);
    QString login = QStringLiteral("user %1 pass %2 vers %3")
        .arg(m_settings.callsign).arg(m_settings.passcode).arg(m_settings.software);
    if (!m_settings.version.isEmpty()) login += QLatin1Char(' ') + m_settings.version;
    const QString filter = filterSpec();
    if (!filter.isEmpty()) {
        login += QStringLiteral(" filter ") + filter;
        m_filterSent = m_position;
        m_filterSentAt = QDateTime::currentMSecsSinceEpoch();
    }
    sendLine(login);
    m_keepalive->start(KeepaliveMs);
}

void AprsIsClient::sendLine(const QString &line)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) return;
    m_socket->write(line.toUtf8() + "\r\n");
}

void AprsIsClient::onReadyRead()
{
    m_buffer += m_socket->readAll();
    m_watchdog->start(WatchdogMs);
    int at;
    while ((at = m_buffer.indexOf('\n')) >= 0) {
        QByteArray raw = m_buffer.left(at);
        m_buffer.remove(0, at + 1);
        while (raw.endsWith('\r')) raw.chop(1);
        if (raw.isEmpty()) continue;
        // Packets are 8-bit; the payload keeps whatever the sender put in.
        const QString line = QString::fromUtf8(raw);
        if (raw.startsWith('#')) {
            handleComment(line);
        } else {
            m_received++;
            emit packetReceived(line);
        }
    }
}

// "# logresp CALL verified, server T2NAME" tells the login result; the
// other comments are heartbeats and are only useful to the watchdog.
void AprsIsClient::handleComment(const QString &line)
{
    if (line.startsWith(QStringLiteral("# logresp"), Qt::CaseInsensitive)) {
        m_verified = line.contains(QStringLiteral(" verified"), Qt::CaseInsensitive)
                  && !line.contains(QStringLiteral("unverified"), Qt::CaseInsensitive);
        const int serverAt = line.indexOf(QStringLiteral("server "), 0, Qt::CaseInsensitive);
        if (serverAt >= 0) m_serverName = line.mid(serverAt + 7).trimmed();
        m_reconnectDelayS = 5;
        setState(State::Connected);
        // Three cases, worded apart: verified; no passcode was given, so the
        // server could not verify anything; a passcode was given and refused.
        QString outcome;
        if (m_verified) outcome = QStringLiteral("passcode accepted");
        else if (m_settings.passcode < 0) outcome = QStringLiteral("no passcode, download only");
        else outcome = QStringLiteral("the server refused the passcode, download only");
        emit logMessage(m_verified || m_settings.passcode < 0 ? QStringLiteral("info") : QStringLiteral("error"),
                        QStringLiteral("APRS-IS: logged in to %1, %2%3.")
                            .arg(m_serverName.isEmpty() ? m_settings.host : m_serverName, outcome,
                                 m_settings.radiusKm > 0 && m_position
                                     ? QStringLiteral(", traffic within %1 km").arg(m_settings.radiusKm)
                                     : QStringLiteral(", no range filter until a position is known")));
    }
}

void AprsIsClient::setFilterPosition(double latitude, double longitude)
{
    m_position = std::make_pair(latitude, longitude);
    if (m_state != State::Connected || m_settings.radiusKm <= 0) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_filterSent) {
        const double moved = aprs::distanceKm(m_filterSent->first, m_filterSent->second, latitude, longitude);
        const double threshold = std::max(1.0, m_settings.radiusKm / 10.0);
        if (moved < threshold || now - m_filterSentAt < 60000) return;
    }
    sendLine(QStringLiteral("#filter %1").arg(filterSpec()));
    m_filterSent = m_position;
    m_filterSentAt = now;
}

bool AprsIsClient::sendPacket(const QString &tnc2)
{
    if (m_state != State::Connected) return false;
    if (!m_verified) {
        if (!m_warnedUnverified) {
            m_warnedUnverified = true;
            emit logMessage(QStringLiteral("error"),
                            m_settings.passcode < 0
                                ? QStringLiteral("APRS-IS: nothing is uploaded without a passcode.")
                                : QStringLiteral("APRS-IS: nothing is uploaded, the server refused the passcode."));
        }
        return false;
    }
    sendLine(tnc2);
    m_sent++;
    return true;
}

void AprsIsClient::onKeepalive()
{
    // A comment line: the server ignores it, and the link stays alive.
    sendLine(QStringLiteral("# %1 keepalive").arg(m_settings.software));
}

void AprsIsClient::onWatchdog()
{
    if (!m_running) return;
    emit logMessage(QStringLiteral("error"), QStringLiteral("APRS-IS: no data from %1 for a while, reconnecting.").arg(m_settings.host));
    m_socket->abort();
    scheduleReconnect();
}

void AprsIsClient::onDisconnected()
{
    m_keepalive->stop();
    m_watchdog->stop();
    if (!m_running) return;
    emit logMessage(QStringLiteral("error"), QStringLiteral("APRS-IS: connection closed by %1.").arg(m_settings.host));
    scheduleReconnect();
}

void AprsIsClient::onError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    if (!m_running) return;
    m_keepalive->stop();
    m_watchdog->stop();
    emit logMessage(QStringLiteral("error"), QStringLiteral("APRS-IS: %1").arg(m_socket->errorString()));
    if (m_socket->state() != QAbstractSocket::UnconnectedState) m_socket->abort();
    scheduleReconnect();
}

void AprsIsClient::scheduleReconnect()
{
    if (!m_running || m_reconnect->isActive()) return;
    setState(State::Connecting);
    emit logMessage(QStringLiteral("info"), QStringLiteral("APRS-IS: retrying in %1 s.").arg(m_reconnectDelayS));
    m_reconnect->start(m_reconnectDelayS * 1000);
    m_reconnectDelayS = std::min(MaxReconnectDelayS, m_reconnectDelayS * 2);
}

void AprsIsClient::setState(State state)
{
    if (state == m_state) return;
    m_state = state;
    emit stateChanged(state);
}
