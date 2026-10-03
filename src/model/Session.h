/*
 * Session.h - one running station, without a user interface.
 *
 * Everything the desktop window used to do that is not a widget lives here:
 * the modem, the channel manager, the message manager, the periodic
 * senders, SmartBeaconing, the GPS source, the stations heard, and the
 * receive and transmit paths between them. The Qt Widgets window and the
 * Qt Quick mobile interface are both views over one Session: they render
 * its log entries, its lists and its state, and call its slots.
 *
 * Log entries carry a prefix, a message and a colour key (tx, rx, beacon,
 * error, system, monitor); the view adds the timestamp and resolves the
 * colour against its own palette or theme.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "AX25Frame.h"
#include "AppConfig.h"
#include "AprsEncoder.h"
#include "AprsIsClient.h"
#include "AprsPacket.h"
#include "ChannelManager.h"
#include "DirewolfEngine.h"
#include "GpsProvider.h"
#include "MessageManager.h"
#include "PeriodicSender.h"
#include "SmartBeacon.h"
#include "StationRegistry.h"

#include <QObject>
#include <QString>

#include <optional>

class QTimer;

class Session : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString myCall READ myCall NOTIFY settingsApplied)
    Q_PROPERTY(bool modemRunning READ modemRunning NOTIFY modemStateChanged)
    // The station: the modem and the APRS-IS connection together.  Start
    // and Stop act on both; "on" is what the operator asked for, running
    // is what is actually up.
    Q_PROPERTY(bool stationOn READ stationOn NOTIFY stationChanged)
    Q_PROPERTY(bool stationBusy READ stationBusy NOTIFY stationChanged)
    Q_PROPERTY(int modemState READ modemStateValue NOTIFY modemStateChanged)
    Q_PROPERTY(QString modemStatusText READ modemStatusText NOTIFY modemStateChanged)
    Q_PROPERTY(int channelState READ channelStateValue NOTIFY channelStateChanged)
    Q_PROPERTY(QString channelStateLabel READ channelStateLabel NOTIFY channelStateChanged)
    Q_PROPERTY(QString channelColourKey READ channelColourKey NOTIFY channelStateChanged)
    Q_PROPERTY(int queueCount READ queueCount NOTIFY queueChanged)
    Q_PROPERTY(QString beaconCountdown READ beaconCountdown NOTIFY panelChanged)
    Q_PROPERTY(QString positionCountdown READ positionCountdown NOTIFY panelChanged)
    Q_PROPERTY(QString gpsStatusText READ gpsStatusText NOTIFY panelChanged)
    Q_PROPERTY(bool gpsOpen READ gpsOpen NOTIFY panelChanged)
    Q_PROPERTY(bool gpsHasFix READ gpsHasFix NOTIFY panelChanged)
    Q_PROPERTY(double gpsLatitude READ gpsLatitude NOTIFY panelChanged)
    Q_PROPERTY(double gpsLongitude READ gpsLongitude NOTIFY panelChanged)
    Q_PROPERTY(QString aprsIsStatusText READ aprsIsStatusText NOTIFY panelChanged)
    Q_PROPERTY(bool aprsIsConnected READ aprsIsConnected NOTIFY panelChanged)

public:
    explicit Session(const AppConfig &config, QObject *parent = nullptr);
    ~Session() override;

    static QString version();

    const AppConfig &config() const { return m_config; }
    QString myCall() const { return m_config.myCall(); }

    // ---- parts the views may talk to directly
    DirewolfEngine *modem() const { return m_modem; }
    ChannelManager *channel() const { return m_channel; }
    MessageManager *messages() const { return m_messages; }
    GpsProvider *gps() const { return m_gps; }
    StationRegistry &stations() { return m_stations; }
    const StationRegistry &stations() const { return m_stations; }
    SmartBeaconer &smart() { return m_smart; }

    // ---- state for the views
    bool modemRunning() const { return m_modem->isRunning(); }
    int modemStateValue() const { return static_cast<int>(m_modem->state()); }
    QString modemStatusText() const;
    int channelStateValue() const { return static_cast<int>(m_channel->state()); }
    QString channelStateLabel() const { return ChannelManager::stateLabel(m_channel->state()); }
    QString channelColourKey() const { return ChannelManager::stateColourKey(m_channel->state()); }
    int queueCount() const { return m_channel->pending(); }
    QString beaconCountdown() const;
    QString positionCountdown() const;
    QString gpsStatusText() const;
    bool gpsOpen() const { return m_gps->isOpen(); }
    bool gpsHasFix() const;
    double gpsLatitude() const;
    double gpsLongitude() const;
    AprsIsClient *aprsIs() const { return m_aprsIs; }
    QString aprsIsStatusText() const;
    bool aprsIsConnected() const { return m_aprsIs->state() == AprsIsClient::State::Connected; }
    QString lastModemFault() const { return m_lastModemFault; }

    // Our own position for distance and bearing: a live fix, else the
    // configured coordinates.
    std::optional<std::pair<double, double>> referencePosition() const;

    // ---- actions
    bool stationOn() const { return m_stationOn; }
    bool stationBusy() const { return m_modem->state() == DirewolfEngine::State::Stopping; }
    Q_INVOKABLE void startStation();
    Q_INVOKABLE void stopStation();
    Q_INVOKABLE void toggleStation();
    Q_INVOKABLE void startModem();
    Q_INVOKABLE void stopModem();
    Q_INVOKABLE void toggleModem();

    // Send what the operator typed: to everyone on the unproto destination
    // when recipient is empty, else an acknowledged APRS message to that
    // station. Returns false (and logs why) when nothing was queued.
    Q_INVOKABLE bool sendText(const QString &recipient, const QString &text);
    Q_INVOKABLE void beaconNow();
    Q_INVOKABLE void positionNow();
    Q_INVOKABLE int cancelQueue();
    Q_INVOKABLE void openGps();
    // Our own position to the APRS Internet System, now.
    Q_INVOKABLE bool sendPositionToAprsIs();
    Q_INVOKABLE void closeGps();

    void applySettings(const AppConfig &config);

    // Explain what a fresh installation configured for itself.
    void noteFirstRun(const QString &confPath);

    // Something the operator did in the view, worth a line in the log.
    void systemLine(const QString &message);
    void warn(const QString &message);

    static SmartBeaconSettings smartSettings(const AppConfig &config);
    static bool smartActive(const AppConfig &config);
    static bool networkPositionPeriodic(const AppConfig &config);

signals:
    // A line for the conversation window: prefix, message, colour key.
    void logEntry(const QString &prefix, const QString &message, const QString &colourKey);
    void modemStateChanged();
    void stationChanged();
    void modemStartFailed(const QString &reason);
    void channelStateChanged();
    void queueChanged(int count);
    void stationsChanged();
    void panelChanged();
    void settingsApplied();
    // The view should show the configuration (callsign still NOCALL).
    void configurationNeeded();

private:
    // modem
    void onModemStarted();
    void onModemStartFailed(const QString &reason);
    void onModemStopped(bool afterFault);
    void onModemLog(DirewolfEngine::LogLevel level, const QString &line);
    bool sendToModem(const QByteArray &raw);
    void pushChannelParams();

    // receive
    void onFrameReceived(const DwReceivedFrame &received);
    bool handleAprs(const AX25Frame &frame, const AprsPacket &packet);
    QString weatherSummary(const AprsPacket &packet) const;
    QString positionSummary(const AprsPacket &packet) const;

    // transmit
    void sendDirected(const QString &recipient, const QString &text);
    bool queueMessageFrame(const QByteArray &info, const QString &addressee,
                           const QString &kind, const QString &label);
    bool sendBeacon();
    bool sendPosition();
    std::optional<PositionReport> currentPosition();
    void smartTick();

    bool airWantsPosition() const;
    bool networkWantsPosition() const;

    // APRS Internet System
    void configureAprsIs();
    void gateToAprsIs(const AX25Frame &frame, const AprsPacket &packet);
    void onAprsIsPacket(const QString &line);
    void refreshAprsIsFilter();

    // feedback
    void onItemSent(const ChannelManager::OutgoingItem &item);
    void onItemDropped(const ChannelManager::OutgoingItem &item, const QString &reason);
    void onLog(const QString &level, const QString &message);
    void onMessageSent(const OutgoingMessage &message);
    void onMessageReceived(const QString &source, const AprsPacket &packet);

    void append(const QString &prefix, const QString &message, const QString &colourKey);

    AppConfig m_config;
    StationRegistry m_stations;
    DirewolfEngine *m_modem = nullptr;
    ChannelManager *m_channel = nullptr;
    PeriodicSender *m_beacon = nullptr;
    PeriodicSender *m_positionSender = nullptr;
    GpsProvider *m_gps = nullptr;
    MessageManager *m_messages = nullptr;
    AprsIsClient *m_aprsIs = nullptr;
    PeriodicSender *m_aprsIsPosition = nullptr;
    QHash<QString, qint64> m_gatedRecently;      // source:info -> when, against digipeated copies
    SmartBeaconer m_smart;
    QTimer *m_smartTimer = nullptr;
    QTimer *m_panelTimer = nullptr;
    bool m_restartModemWhenStopped = false;
    bool m_stationOn = false;
    // After a fault with the station on, the modem is started again on a
    // growing delay, reset once a start has held for a while.
    int m_faultRestartDelayS = 5;
    qint64 m_modemStartedAtMs = 0;
    QString m_lastModemFault;
    QString m_lastModemError;
    qint64 m_lastModemErrorAt = 0;
    int m_modemErrorRepeats = 0;
};
