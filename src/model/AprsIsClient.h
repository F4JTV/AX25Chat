/*
 * AprsIsClient.h - a client of the APRS Internet System.
 *
 * Connects to a Tier 2 server on the client-defined filter port (14580),
 * logs in with the station's callsign and passcode, subscribes to the
 * traffic around a position with a range filter (r/lat/lon/km), and
 * forwards packets both ways: lines from the server are handed to the
 * session, packets the session heard on the air are sent to the server
 * as a receive-only IGate would (q-construct qAO).
 *
 * Protocol, from aprs-is.net: one line per packet in TNC-2 form
 * (SRC>DEST,PATH:info); lines beginning with '#' are server comments,
 * including the login response "# logresp CALL verified|unverified,
 * server NAME"; the filter is given on the login line and may be changed
 * with a "#filter ..." line; with passcode -1 the login is unverified and
 * nothing the client sends is accepted. Servers emit a comment every 20 s
 * or so, so silence for two minutes means the connection is gone.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QString>
#include <QTcpSocket>

#include <optional>

class QTimer;

class AprsIsClient : public QObject
{
    Q_OBJECT

public:
    enum class State { Stopped, Connecting, LoggingIn, Connected };

    struct Settings {
        QString host = QStringLiteral("rotate.aprs2.net");
        int port = 14580;
        QString callsign;                 // with SSID, e.g. MYCALL-7
        int passcode = -1;                // -1: receive only
        int radiusKm = 50;                // 0: no range filter
        QString software = QStringLiteral("AX25Chat");
        QString version;
    };

    explicit AprsIsClient(QObject *parent = nullptr);
    ~AprsIsClient() override;

    void configure(const Settings &settings);
    const Settings &settings() const { return m_settings; }

    void start();
    void stop();
    bool isRunning() const { return m_running; }
    State state() const { return m_state; }
    bool verified() const { return m_verified; }
    QString serverName() const { return m_serverName; }
    QString statusText() const;
    int packetsReceived() const { return m_received; }
    int packetsSent() const { return m_sent; }

    // The centre of the range filter; re-sent to the server when it moves
    // by more than a tenth of the radius, at most once a minute.
    void setFilterPosition(double latitude, double longitude);

    // Send one packet in TNC-2 form, without line ending.  Ignored (and
    // logged once) when the login is not verified.
    bool sendPacket(const QString &tnc2);

signals:
    void stateChanged(AprsIsClient::State state);
    // One packet from the server, TNC-2 form, comments already removed.
    void packetReceived(const QString &line);
    void logMessage(const QString &level, const QString &message);

private:
    void connectToServer();
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onWatchdog();
    void onKeepalive();
    void handleComment(const QString &line);
    void setState(State state);
    QString filterSpec() const;
    void sendLine(const QString &line);
    void scheduleReconnect();

    Settings m_settings;
    QTcpSocket *m_socket = nullptr;
    QTimer *m_watchdog = nullptr;
    QTimer *m_keepalive = nullptr;
    QTimer *m_reconnect = nullptr;
    State m_state = State::Stopped;
    bool m_running = false;
    bool m_verified = false;
    bool m_warnedUnverified = false;
    QString m_serverName;
    QByteArray m_buffer;
    std::optional<std::pair<double, double>> m_position;
    std::optional<std::pair<double, double>> m_filterSent;
    qint64 m_filterSentAt = 0;
    int m_received = 0;
    int m_sent = 0;
    int m_reconnectDelayS = 5;
};
