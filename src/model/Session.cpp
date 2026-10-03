/*
 * Session.cpp - one running station, without a user interface.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Session.h"

#include "AprsDecoder.h"
#include "ModemConfigFile.h"

#include <QDateTime>

#include <algorithm>
#include <QFileInfo>
#include <QTimer>

#include <cmath>

QString Session::version()
{
    return QStringLiteral(AX25CHAT_VERSION);
}

Session::Session(const AppConfig &config, QObject *parent)
    : QObject(parent), m_config(config), m_stations(config.messaging.stationExpiryMinutes),
      m_smart(smartSettings(config))
{
    m_modem = new DirewolfEngine(this);
    m_channel = new ChannelManager(config, [this](const QByteArray &raw) { return sendToModem(raw); },
                                   [this] { return m_modem->txQueueBytes(m_config.modem.channel); }, this);
    m_beacon = new PeriodicSender(QStringLiteral("Beacon"), [this] { return sendBeacon(); }, this);
    m_beacon->configure(config.beacon.enabled, config.beacon.intervalMinutes, false);
    m_positionSender = new PeriodicSender(QStringLiteral("Position"), [this] { return sendPosition(); }, this);
    m_positionSender->configure(config.position.enabled, config.position.intervalMinutes, false);
    m_gps = GpsProvider::create(this);
    m_messages = new MessageManager(config,
        [this](const QByteArray &info, const QString &addressee, const QString &kind, const QString &label) {
            return queueMessageFrame(info, addressee, kind, label);
        }, this);

    connect(m_modem, &DirewolfEngine::started, this, &Session::onModemStarted);
    connect(m_modem, &DirewolfEngine::startFailed, this, &Session::onModemStartFailed);
    connect(m_modem, &DirewolfEngine::stopped, this, &Session::onModemStopped);
    connect(m_modem, &DirewolfEngine::faulted, this, [this](int, const QString &reason) { m_lastModemFault = reason; });
    connect(m_modem, &DirewolfEngine::stateChanged, this, [this](DirewolfEngine::State) { emit modemStateChanged(); });
    connect(m_modem, &DirewolfEngine::logMessage, this, &Session::onModemLog);
    connect(m_modem, &DirewolfEngine::frameReceived, this, &Session::onFrameReceived);
    connect(m_modem, &DirewolfEngine::dcdChanged, this, [this](int chan, bool active) {
        if (chan == m_config.modem.channel) m_channel->setDcd(active);
    });
    connect(m_modem, &DirewolfEngine::pttChanged, this, [this](int chan, bool active) {
        if (chan == m_config.modem.channel) m_channel->setPtt(active);
    });

    connect(m_channel, &ChannelManager::stateChanged, this, [this](ChannelManager::State) { emit channelStateChanged(); });
    connect(m_channel, &ChannelManager::queueChanged, this, &Session::queueChanged);
    connect(m_channel, &ChannelManager::itemSent, this, &Session::onItemSent);
    connect(m_channel, &ChannelManager::itemDropped, this, &Session::onItemDropped);
    connect(m_channel, &ChannelManager::logMessage, this, &Session::onLog);
    connect(m_beacon, &PeriodicSender::logMessage, this, &Session::onLog);
    connect(m_positionSender, &PeriodicSender::logMessage, this, &Session::onLog);
    connect(m_gps, &GpsProvider::logMessage, this, &Session::onLog);
    connect(m_gps, &GpsProvider::statusChanged, this, [this](const QString &) { emit panelChanged(); });
    connect(m_messages, &MessageManager::logMessage, this, &Session::onLog);
    connect(m_messages, &MessageManager::messageSent, this, &Session::onMessageSent);
    connect(m_messages, &MessageManager::acknowledged, this, [this](const OutgoingMessage &m) {
        systemLine(QStringLiteral("%1 acknowledged: %2").arg(m.addressee, m.text));
    });
    connect(m_messages, &MessageManager::rejected, this, [this](const OutgoingMessage &m) {
        warn(QStringLiteral("%1 rejected: %2").arg(m.addressee, m.text));
    });
    connect(m_messages, &MessageManager::failed, this, [this](const OutgoingMessage &m) {
        warn(QStringLiteral("No acknowledgement from %1 after %2 attempts, giving up: %3")
                 .arg(m.addressee).arg(m.attempts).arg(m.text));
    });
    connect(m_messages, &MessageManager::incoming, this, &Session::onMessageReceived);

    m_aprsIs = new AprsIsClient(this);
    connect(m_aprsIs, &AprsIsClient::logMessage, this, &Session::onLog);
    connect(m_aprsIs, &AprsIsClient::packetReceived, this, &Session::onAprsIsPacket);
    // The network report keeps the schedule of the one on the air: the
    // periodic interval of the position reports, or SmartBeaconing.
    m_aprsIsPosition = new PeriodicSender(QStringLiteral("Internet position"), [this] { return sendPositionToAprsIs(); }, this);
    m_aprsIsPosition->configure(networkPositionPeriodic(config), config.position.intervalMinutes, false);
    connect(m_aprsIsPosition, &PeriodicSender::logMessage, this, &Session::onLog);
    connect(m_aprsIs, &AprsIsClient::stateChanged, this, [this](AprsIsClient::State state) {
        emit panelChanged();
        // Just logged in: our position, if wanted, a moment later (the
        // login response has to have been read first).
        if (state == AprsIsClient::State::Connected && m_config.aprsis.positionEnabled && m_config.aprsis.positionOnConnect) {
            QTimer::singleShot(2000, this, [this] { sendPositionToAprsIs(); });
        }
    });
    configureAprsIs();

    m_channel->start();
    m_messages->start();
    if (config.beacon.enabled) m_beacon->start();
    if (config.position.enabled) m_positionSender->start();
    if (config.position.source == QLatin1String("gps") && config.position.gpsAutoOpen) openGps();

    // SmartBeaconing decides its own moment, so it is evaluated often
    // rather than on a fixed interval.
    m_smartTimer = new QTimer(this);
    m_smartTimer->setInterval(2000);
    connect(m_smartTimer, &QTimer::timeout, this, &Session::smartTick);
    if (smartActive(config)) m_smartTimer->start();

    m_panelTimer = new QTimer(this);
    connect(m_panelTimer, &QTimer::timeout, this, [this] {
        m_stations.expire();
        refreshAprsIsFilter();
        emit panelChanged();
        emit stationsChanged();
    });
    m_panelTimer->start(1000);

    systemLine(QStringLiteral("AX25Chat %1 ready. Modem: Dire Wolf %2 (WB2OSZ), built in.")
                   .arg(version(), DirewolfEngine::direwolfVersion()));
    systemLine(QStringLiteral("Station: %1  ->  %2%3")
                   .arg(config.myCall(), config.station.destination,
                        config.station.digipeaters.isEmpty() ? QString()
                                                             : QStringLiteral(" via %1").arg(config.station.digipeaters)));
    if (config.station.callsign.toUpper() == QLatin1String("NOCALL")) {
        warn(QStringLiteral("Set your callsign in Configuration before transmitting."));
        QTimer::singleShot(0, this, &Session::configurationNeeded);
    } else if (config.modem.autoStart) {
        QTimer::singleShot(300, this, &Session::startStation);
    }
}

// =====================================================================
//  The station: modem and APRS-IS together
// =====================================================================

void Session::startStation()
{
    m_stationOn = true;
    emit stationChanged();
    if (m_config.modem.enabled) startModem();
    else systemLine(QStringLiteral("Modem left off: the station runs on the APRS Internet System alone."));
    configureAprsIs();
    if (networkPositionPeriodic(m_config) && !m_aprsIsPosition->running()) m_aprsIsPosition->start();
}

void Session::stopStation()
{
    m_stationOn = false;
    m_faultRestartDelayS = 5;
    emit stationChanged();
    stopModem();
    m_aprsIsPosition->stop();
    if (m_aprsIs->isRunning()) {
        m_aprsIs->stop();
        systemLine(QStringLiteral("APRS-IS: disconnected."));
    }
    emit panelChanged();
}

void Session::toggleStation()
{
    if (m_stationOn) stopStation();
    else startStation();
}

Session::~Session()
{
    m_aprsIsPosition->stop();
    m_aprsIs->stop();
    m_beacon->stop();
    m_messages->stop();
    m_smartTimer->stop();
    m_positionSender->stop();
    m_gps->close();
    m_channel->stop();
    m_modem->stop();      // the engine's destructor waits for the core thread
}

void Session::noteFirstRun(const QString &confPath)
{
    systemLine(QStringLiteral("First run: a starter modem configuration was written to %1.").arg(confPath));
    warn(QStringLiteral("Edit that file for your audio device and PTT wiring before transmitting. "
                        "Set your callsign in Configuration."));
}


// =====================================================================
//  Modem
// =====================================================================

QString Session::modemStatusText() const
{
    switch (m_modem->state()) {
        case DirewolfEngine::State::Starting: return QStringLiteral("Modem: starting");
        case DirewolfEngine::State::Stopping: return QStringLiteral("Modem: stopping");
        case DirewolfEngine::State::Running:
            return QStringLiteral("Modem: %1, ch %2")
                .arg(m_modem->channelDescription(m_config.modem.channel)).arg(m_config.modem.channel);
        case DirewolfEngine::State::Stopped:
        default:
            return m_lastModemFault.isEmpty() ? QStringLiteral("Modem: stopped") : QStringLiteral("Modem: stopped after a fault");
    }
}

void Session::startModem()
{
    if (m_modem->state() != DirewolfEngine::State::Stopped) return;
    QString conf = m_config.modem.configFile.trimmed();
    if (conf.isEmpty()) conf = modemconf::defaultFile();
    if (conf.isEmpty()) {
        warn(QStringLiteral("No direwolf.conf is configured and none was found in the usual places. "
                            "Use Configuration -> Modem -> Create to write a starter file."));
        return;
    }
    if (!QFileInfo(conf).isFile()) {
        warn(QStringLiteral("The modem configuration file does not exist: %1").arg(conf));
        return;
    }
    m_lastModemFault.clear();
    systemLine(QStringLiteral("Starting the modem with %1").arg(conf));
    m_modem->start(conf);
}

void Session::stopModem()
{
    if (m_modem->state() == DirewolfEngine::State::Stopped) return;
    m_modem->stop();
}

void Session::toggleModem()
{
    if (m_modem->state() == DirewolfEngine::State::Stopped) startModem();
    else stopModem();
}

void Session::pushChannelParams()
{
    const ModemConfig &m = m_config.modem;
    if (!m.sendParams) return;
    m_modem->setChannelParams(m.channel, m.txdelay, m.persistence, m.slottime, m.txtail, m.fullDuplex ? 1 : 0);
    systemLine(QStringLiteral("Channel access: TXDELAY %1 ms, persistence %2, slot %3 ms, TXtail %4 ms, %5 duplex.")
                   .arg(m.txdelay * 10).arg(m.persistence).arg(m.slottime * 10).arg(m.txtail * 10)
                   .arg(m.fullDuplex ? QStringLiteral("full") : QStringLiteral("half")));
    if (m.fullDuplex) {
        // Full duplex switches off the modem's carrier sense, the only layer
        // that can see the RF.
        warn(QStringLiteral("Full duplex is enabled, so the modem will not wait for the frequency to be clear. "
                            "On a simplex channel this will transmit over other stations. Turn it off unless "
                            "you are working through a duplex repeater."));
    }
}

void Session::onModemStarted()
{
    m_modemStartedAtMs = QDateTime::currentMSecsSinceEpoch();
    emit stationChanged();
    const int wanted = m_config.modem.channel;
    if (!m_modem->channelIsRadio(wanted)) {
        QStringList available;
        for (int c = 0; c < DirewolfEngine::maxChannels(); c++) {
            if (m_modem->channelIsRadio(c)) available << QString::number(c);
        }
        warn(QStringLiteral("AX25Chat is set to modem channel %1, but the configuration file only declares "
                            "channel %2. Transmission will be refused. Set the channel in Configuration, or "
                            "add a CHANNEL line to direwolf.conf.")
                 .arg(wanted).arg(available.isEmpty() ? QStringLiteral("none") : available.join(QStringLiteral(", "))));
    }
    const QString mycall = m_modem->channelMycall(wanted);
    systemLine(QStringLiteral("Modem started: %1 on channel %2%3.")
                   .arg(m_modem->channelDescription(wanted)).arg(wanted)
                   .arg(mycall.isEmpty() ? QString() : QStringLiteral(", MYCALL %1").arg(mycall)));
    pushChannelParams();
    m_channel->setOnline(true);
    emit modemStateChanged();

    if (m_config.beacon.enabled && m_config.beacon.sendOnConnect) {
        QTimer::singleShot(1500, this, [this] { sendBeacon(); });
    }
    if (m_config.position.enabled && m_config.position.sendOnConnect) {
        QTimer::singleShot(3000, this, [this] { sendPosition(); });
    }
}

void Session::onModemStartFailed(const QString &reason)
{
    warn(QStringLiteral("The modem could not start: %1").arg(reason));
    warn(QStringLiteral("Check the audio device and PTT lines in the modem configuration file "
                        "(Configuration -> Modem)."));
    emit modemStartFailed(reason);
    emit modemStateChanged();
}

void Session::onModemStopped(bool afterFault)
{
    m_channel->setOnline(false);
    emit stationChanged();
    if (afterFault) {
        warn(QStringLiteral("The modem stopped after a fault%1.")
                 .arg(m_lastModemFault.isEmpty() ? QString() : QStringLiteral(": %1").arg(m_lastModemFault)));
    } else {
        systemLine(QStringLiteral("Modem stopped."));
    }
    emit modemStateChanged();
    if (m_restartModemWhenStopped) {
        m_restartModemWhenStopped = false;
        QTimer::singleShot(200, this, &Session::startModem);
        return;
    }
    // The station is meant to be on: a fault (an audio device gone for a
    // moment, say) is not the operator's decision to stop.  Try again,
    // 5 s, then 10, 20, up to a minute; a start that held for two minutes
    // resets the delay.
    if (afterFault && m_stationOn) {
        if (m_modemStartedAtMs && QDateTime::currentMSecsSinceEpoch() - m_modemStartedAtMs > 120000) m_faultRestartDelayS = 5;
        const int delay = m_faultRestartDelayS;
        m_faultRestartDelayS = std::min(60, m_faultRestartDelayS * 2);
        systemLine(QStringLiteral("The station is on: the modem will be started again in %1 s.").arg(delay));
        QTimer::singleShot(delay * 1000, this, [this] { if (m_stationOn && m_config.modem.enabled) startModem(); });
    }
}

void Session::onModemLog(DirewolfEngine::LogLevel level, const QString &line)
{
    const QString text = line.trimmed();
    if (text.isEmpty()) return;
    if (level == DirewolfEngine::Error) {
        // Problems are shown even with the modem's chatter switched off,
        // but a flood of the same line (a diagnostic printed on every
        // frame, say) is folded into one entry and a count.
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (text == m_lastModemError && now - m_lastModemErrorAt < 30000) {
            m_modemErrorRepeats++;
            m_lastModemErrorAt = now;
            return;
        }
        if (m_modemErrorRepeats > 0) {
            systemLine(QStringLiteral("(the previous modem message was repeated %1 more times)").arg(m_modemErrorRepeats));
        }
        m_lastModemError = text;
        m_lastModemErrorAt = now;
        m_modemErrorRepeats = 0;
        warn(QStringLiteral("modem: %1").arg(text));
    } else if (m_config.modem.echoLog && level != DirewolfEngine::Debug) {
        append(QStringLiteral("modem"), text, QStringLiteral("system"));
    }
}

bool Session::sendToModem(const QByteArray &raw)
{
    return m_modem->transmit(m_config.modem.channel, raw);
}


// =====================================================================
//  Receive path
// =====================================================================

void Session::onFrameReceived(const DwReceivedFrame &received)
{
    if (received.chan != m_config.modem.channel) return;

    QString error;
    const auto decoded = AX25Frame::decode(received.frame, &error);
    if (!decoded) {
        warn(QStringLiteral("Undecodable frame (%1), %2 octets").arg(error).arg(received.frame.size()));
        return;
    }
    const AX25Frame &frame = *decoded;

    // The channel was occupied while this frame was being sent.
    m_channel->noteRx(&frame);

    const QString mine = m_config.myCall().toUpper();
    const QString source = frame.source.toString().toUpper();
    const QString destination = frame.destination.toString().toUpper();
    const QString wanted = m_config.station.destination.toUpper();

    if (source == mine) return;      // our own frame coming back through a digipeater

    const QString encoding = m_config.station.textEncoding;

    // Try APRS first.  A packet that decodes as APRS carries far more than
    // its text: a position, a symbol, a message with an identifier that
    // must be acknowledged.  Plain chat traffic falls through unharmed
    // because it does not begin with an APRS data type identifier.
    std::optional<AprsPacket> packet;
    if (frame.isUi() && !frame.info.isEmpty()) {
        AprsPacket p = AprsDecoder::decode(frame.info, frame.destination.toString(), frame.source.toString());
        if (p.kind != AprsPacket::Kind::Unknown) packet = p;
    }
    if (packet) {
        m_stations.applyPacket(frame.source.toString(), *packet, frame.path());
        emit stationsChanged();
        gateToAprsIs(frame, *packet);
        if (handleAprs(frame, *packet)) return;
    } else {
        m_stations.noteHeard(frame.source.toString(), frame.path());
        emit stationsChanged();
    }

    if (frame.isUi() && destination == wanted) {
        append(QStringLiteral("<%1>").arg(frame.source.toString()), frame.text(encoding), QStringLiteral("rx"));
    } else if (frame.isUi() && destination == mine) {
        append(QStringLiteral("<%1> (direct)").arg(frame.source.toString()), frame.text(encoding), QStringLiteral("rx"));
    } else if (m_config.ui.showMonitor) {
        append(QStringLiteral("monitor"),
               QStringLiteral("%1>%2 [%3] %4").arg(frame.source.toString(), frame.path(), frame.frameType(), frame.text(encoding)),
               QStringLiteral("monitor"));
    }
}

// Act on a decoded APRS packet.  True when it has been fully dealt with and
// must not also be shown as raw chat text.
bool Session::handleAprs(const AX25Frame &frame, const AprsPacket &packet)
{
    const QString source = frame.source.toString();

    if (packet.kind == AprsPacket::Kind::Message) {
        // The message manager acknowledges, suppresses duplicates and
        // matches acknowledgements against what we sent.
        return m_messages->handleIncoming(source, packet);
    }
    if (packet.kind == AprsPacket::Kind::Weather) {
        append(QStringLiteral("[%1] weather").arg(source), weatherSummary(packet), QStringLiteral("weather"));
        return true;
    }
    if (packet.kind == AprsPacket::Kind::Position || packet.kind == AprsPacket::Kind::Object
        || packet.kind == AprsPacket::Kind::Item) {
        if (!m_config.ui.showMonitor) return true;
        QString label = source;
        if (!packet.name.isEmpty()) label = QStringLiteral("%1 (%2)").arg(source, packet.name);
        // A weather station reports its readings inside a position, so the
        // category follows the content rather than the packet type.
        append(QStringLiteral("[%1]").arg(label), positionSummary(packet),
               packet.weather ? QStringLiteral("weather") : QStringLiteral("position"));
        return true;
    }
    if (packet.kind == AprsPacket::Kind::Status && !packet.status.isEmpty()) {
        append(QStringLiteral("[%1] status").arg(source), packet.status, QStringLiteral("status"));
        return true;
    }
    return false;
}

QString Session::weatherSummary(const AprsPacket &packet) const
{
    const QString readings = packet.weather ? packet.weather->describe() : QStringLiteral("no readings");
    return QStringLiteral("%1   %2").arg(readings, packet.comment).trimmed();
}

// One readable line describing a received position.
QString Session::positionSummary(const AprsPacket &packet) const
{
    QStringList parts;
    if (packet.hasPosition()) {
        parts << QStringLiteral("%1%2 %3%4")
                     .arg(std::fabs(*packet.latitude), 0, 'f', 4).arg(*packet.latitude >= 0 ? QStringLiteral("N") : QStringLiteral("S"))
                     .arg(std::fabs(*packet.longitude), 0, 'f', 4).arg(*packet.longitude >= 0 ? QStringLiteral("E") : QStringLiteral("W"));
        if (const auto reference = referencePosition()) {
            const double distance = aprs::distanceKm(reference->first, reference->second, *packet.latitude, *packet.longitude);
            const double bearing = AprsDecoder::bearingDeg(reference->first, reference->second, *packet.latitude, *packet.longitude);
            parts << QStringLiteral("%1 km %2").arg(distance, 0, 'f', 1).arg(AprsDecoder::compassPoint(bearing));
        }
    }
    if (packet.altitudeM) parts << QStringLiteral("%1 m").arg(std::lround(*packet.altitudeM));
    if (packet.speedKnots && *packet.speedKnots >= 1) {
        const QString course = packet.courseDeg ? QStringLiteral(" %1deg").arg(std::lround(*packet.courseDeg)) : QString();
        parts << QStringLiteral("%1 kn%2").arg(std::lround(*packet.speedKnots)).arg(course);
    }
    if (packet.weather) parts << packet.weather->describe();
    if (!packet.status.isEmpty()) parts << packet.status;
    if (!packet.comment.isEmpty()) parts << packet.comment;
    return parts.isEmpty() ? packet.description : parts.join(QStringLiteral("  "));
}

std::optional<std::pair<double, double>> Session::referencePosition() const
{
    if (m_gps->isOpen() && m_gps->fix().valid) {
        return std::make_pair(m_gps->fix().latitude, m_gps->fix().longitude);
    }
    const PositionConfig &settings = m_config.position;
    if (settings.latitude != 0.0 || settings.longitude != 0.0) {
        return std::make_pair(settings.latitude, settings.longitude);
    }
    return std::nullopt;
}


// =====================================================================
//  Transmit path
// =====================================================================

bool Session::sendText(const QString &recipientIn, const QString &textIn)
{
    const QString text = textIn.trimmed();
    if (text.isEmpty()) return false;
    if (!m_modem->isRunning()) {
        warn(QStringLiteral("The modem is not running, the message was not queued."));
        return false;
    }
    if (m_config.station.callsign.toUpper() == QLatin1String("NOCALL")) {
        warn(QStringLiteral("Set your callsign before transmitting."));
        return false;
    }
    const QString recipient = recipientIn.trimmed().toUpper();
    if (!recipient.isEmpty()) {
        sendDirected(recipient, text);
        return true;
    }

    const StationConfig &station = m_config.station;
    const QList<QByteArray> chunks = ax25::splitMessage(text, station.maxInfoLen, station.textEncoding);
    if (chunks.isEmpty()) return false;
    QList<AX25Frame> frames;
    QStringList labels;
    for (const QByteArray &chunk : chunks) {
        QString error;
        auto frame = AX25Frame::ui(m_config.myCall(), station.destination, chunk, m_config.digipeaterList(), false, &error);
        if (!frame) {
            warn(QStringLiteral("Cannot build the frame: %1").arg(error));
            return false;
        }
        frames.append(*frame);
        labels.append(ax25::decodeText(chunk, station.textEncoding));
    }
    m_channel->enqueueMany(frames, labels, QStringLiteral("chat"));
    const QString reason = m_channel->nextItemWaitReason();
    if (!reason.isEmpty()) {
        systemLine(QStringLiteral("Message held back, %1. It will go out as soon as the frequency is clear.").arg(reason));
    }
    return true;
}

// Send an APRS message to one station, acknowledged and retried.
void Session::sendDirected(const QString &recipient, const QString &text)
{
    const QString cleaned = aprsmsg::sanitiseText(text);
    if (cleaned.isEmpty()) {
        warn(QStringLiteral("Nothing left to send after removing reserved characters."));
        return;
    }
    if (text.size() > aprsmsg::MaxTextLen) {
        warn(QStringLiteral("An APRS message holds %1 characters; yours was %2 and has been shortened.")
                 .arg(aprsmsg::MaxTextLen).arg(text.size()));
    }
    if (!m_messages->send(recipient, cleaned)) {
        warn(QStringLiteral("The message could not be queued."));
    }
}

// Put a message or acknowledgement frame on the transmit queue.
bool Session::queueMessageFrame(const QByteArray &info, const QString &addressee, const QString &kind, const QString &label)
{
    Q_UNUSED(addressee);
    QString error;
    const QString destination = m_config.position.destination.isEmpty() ? aprs::DefaultTocall : m_config.position.destination;
    auto frame = AX25Frame::ui(m_config.myCall(), destination, info, m_config.digipeaterList(), false, &error);
    if (!frame) {
        warn(QStringLiteral("Cannot build the message frame: %1").arg(error));
        return false;
    }
    m_channel->enqueue(*frame, label.isEmpty() ? QString::fromLatin1(info) : label, kind);
    return true;
}

// The message has been queued, not yet transmitted.  Only retries are
// announced here; the first transmission is reported when the frame leaves.
void Session::onMessageSent(const OutgoingMessage &message)
{
    if (message.attempts > 1) {
        systemLine(QStringLiteral("Retrying message to %1 (attempt %2 of %3)")
                       .arg(message.addressee).arg(message.attempts).arg(message.maxAttempts));
    }
}

void Session::onMessageReceived(const QString &source, const AprsPacket &packet)
{
    if (packet.messageKind == AprsPacket::MessageKind::Bulletin) {
        // Weather bulletins are separated out because they arrive on a
        // schedule and an operator may want them silent while keeping
        // ordinary bulletins audible.
        const QString prefix = packet.addressee.toUpper().left(3);
        const bool weather = prefix == QLatin1String("NWS") || prefix == QLatin1String("SKY")
                          || prefix == QLatin1String("CWA") || prefix == QLatin1String("BOM");
        append(QStringLiteral("[bulletin %1 from %2]").arg(packet.addressee, source), packet.text,
               weather ? QStringLiteral("weather") : QStringLiteral("bulletin"));
    } else {
        append(QStringLiteral("<%1 to me>").arg(source), packet.text, QStringLiteral("message"));
    }
}

bool Session::sendBeacon()
{
    if (!m_modem->isRunning()) {
        warn(QStringLiteral("Beacon skipped, the modem is not running."));
        return false;
    }
    const QString text = m_config.beaconText();
    if (text.isEmpty()) return false;
    const StationConfig &station = m_config.station;
    const QList<QByteArray> chunks = ax25::splitMessage(text, station.maxInfoLen, station.textEncoding);
    if (chunks.isEmpty()) return false;
    // Never let beacons pile up behind a long busy period.
    if (m_channel->dropPending(QStringLiteral("beacon"))) {
        systemLine(QStringLiteral("Previous beacon still queued, it is replaced by the current one."));
    }
    const QString destination = m_config.beacon.destination.isEmpty() ? station.destination : m_config.beacon.destination;
    QList<AX25Frame> frames;
    QStringList labels;
    for (const QByteArray &chunk : chunks) {
        QString error;
        auto frame = AX25Frame::ui(m_config.myCall(), destination, chunk,
                                   m_config.digipeaterList(m_config.beacon.digipeaters), false, &error);
        if (!frame) {
            warn(QStringLiteral("Cannot build the beacon: %1").arg(error));
            return false;
        }
        frames.append(*frame);
        labels.append(ax25::decodeText(chunk, station.textEncoding));
    }
    m_channel->enqueueMany(frames, labels, QStringLiteral("beacon"));
    return true;
}

void Session::openGps()
{
    const PositionConfig &settings = m_config.position;
    if (m_gps->needsPort() && settings.gpsPort.isEmpty()) {
        warn(QStringLiteral("No GPS serial port is configured."));
        return;
    }
    m_gps->open(settings.gpsPort, settings.gpsBaud);
    emit panelChanged();
}

void Session::closeGps()
{
    m_gps->close();
    emit panelChanged();
}

// Build the report to send now, or nullopt with a reason logged.  This is
// where the fixed and GPS sources converge.
std::optional<PositionReport> Session::currentPosition()
{
    const PositionConfig &settings = m_config.position;
    PositionReport report;
    report.symbolTable = settings.symbolTable;
    report.symbolCode = settings.symbolCode;
    report.comment = settings.comment;
    report.messagingCapable = settings.messagingCapable;

    if (settings.source == QLatin1String("gps")) {
        if (!m_gps->isOpen()) {
            warn(QStringLiteral("Position not sent: the GPS source is not open."));
            return std::nullopt;
        }
        const GpsFix fix = m_gps->fix();
        if (!fix.valid) {
            warn(QStringLiteral("Position not sent: no GPS fix yet%1.")
                     .arg(fix.satellites ? QStringLiteral(" (%1 satellites)").arg(fix.satellites) : QString()));
            return std::nullopt;
        }
        const double fixAge = m_gps->fixAgeSeconds();
        if (fixAge > settings.gpsMaxFixAgeS) {
            // Reporting a position the receiver stopped confirming is worse
            // than reporting nothing: nobody downstream can tell it is stale.
            warn(QStringLiteral("Position not sent: the GPS fix is %1 s old, older than the %2 s limit.")
                     .arg(std::lround(fixAge)).arg(settings.gpsMaxFixAgeS));
            return std::nullopt;
        }
        report.latitude = fix.latitude;
        report.longitude = fix.longitude;
        if (settings.sendAltitude && fix.altitudeM) report.altitudeM = fix.altitudeM;
        if (settings.gpsSendCourseSpeed && fix.courseDeg && fix.speedKnots) {
            report.courseDeg = fix.courseDeg;
            report.speedKnots = fix.speedKnots;
        }
        return report;
    }

    if (settings.latitude == 0.0 && settings.longitude == 0.0) {
        warn(QStringLiteral("Position not sent: no fixed coordinates are set. Enter them in Configuration, "
                            "or fill them from your locator."));
        return std::nullopt;
    }
    report.latitude = settings.latitude;
    report.longitude = settings.longitude;
    if (settings.sendAltitude) report.altitudeM = settings.altitudeM;
    return report;
}

SmartBeaconSettings Session::smartSettings(const AppConfig &config)
{
    const PositionConfig &p = config.position;
    SmartBeaconSettings s;
    s.lowSpeedKmh = p.smartLowSpeedKmh;
    s.highSpeedKmh = p.smartHighSpeedKmh;
    s.slowIntervalS = p.smartSlowIntervalS;
    s.fastIntervalS = p.smartFastIntervalS;
    s.turnMinDeg = p.smartTurnMinDeg;
    s.turnSlope = p.smartTurnSlope;
    s.turnTimeS = p.smartTurnTimeS;
    return s;
}

// SmartBeaconing only makes sense with a live GPS: with fixed coordinates
// the speed is always zero and every interval collapses to the slow one.
bool Session::smartActive(const AppConfig &config)
{
    return config.position.smartEnabled && config.position.source == QLatin1String("gps");
}

// Where a position report can go right now: the air (modem running), the
// network (APRS-IS logged in with the passcode, position upload wanted).
bool Session::airWantsPosition() const
{
    return m_modem->isRunning();
}

// The network report on a fixed schedule: wanted, periodic reporting on,
// and not SmartBeaconing (which drives it itself).
bool Session::networkPositionPeriodic(const AppConfig &config)
{
    return config.aprsis.enabled && config.aprsis.positionEnabled && config.position.enabled && !smartActive(config);
}

bool Session::networkWantsPosition() const
{
    return m_config.aprsis.enabled && m_config.aprsis.positionEnabled
        && m_aprsIs->state() == AprsIsClient::State::Connected && m_aprsIs->verified();
}

// SmartBeaconing decides the moment; the report then goes wherever it can:
// the air, the network, or both.  It runs with the modem stopped too, for
// a phone that is only an Internet tracker.
void Session::smartTick()
{
    if (!smartActive(m_config)) return;
    if (!m_gps->isOpen()) return;
    if (!airWantsPosition() && !networkWantsPosition()) return;
    const GpsFix fix = m_gps->fix();
    if (!fix.valid || m_gps->fixAgeSeconds() > m_config.position.gpsMaxFixAgeS) return;
    QString reason;
    if (!m_smart.evaluateFix(fix.latitude, fix.longitude, fix.speedKnots, fix.courseDeg, &reason)) return;
    bool sent = false;
    if (airWantsPosition()) sent = sendPosition() || sent;
    if (networkWantsPosition()) sent = sendPositionToAprsIs() || sent;
    if (sent) {
        m_smart.noteSent(fix.courseDeg);
        systemLine(QStringLiteral("SmartBeacon: %1").arg(reason));
    }
}

bool Session::sendPosition()
{
    if (!m_modem->isRunning()) {
        warn(QStringLiteral("Position skipped, the modem is not running."));
        return false;
    }
    if (m_config.station.callsign.toUpper() == QLatin1String("NOCALL")) {
        warn(QStringLiteral("Set your callsign before transmitting a position."));
        return false;
    }
    const auto report = currentPosition();
    if (!report) return false;
    QString error;
    const QByteArray info = report->encode(&error);
    if (info.isEmpty()) {
        warn(QStringLiteral("Cannot build the position report: %1").arg(error));
        return false;
    }
    const PositionConfig &settings = m_config.position;
    // A queued position that never found a gap is stale; replace it rather
    // than transmitting a trail of old fixes when the channel clears.
    if (m_channel->dropPending(QStringLiteral("position"))) {
        systemLine(QStringLiteral("Previous position still queued, it is replaced by the current one."));
    }
    const QString destination = settings.destination.isEmpty() ? aprs::DefaultTocall : settings.destination;
    auto frame = AX25Frame::ui(m_config.myCall(), destination, info, m_config.digipeaterList(settings.digipeaters), false, &error);
    if (!frame) {
        warn(QStringLiteral("Cannot build the position frame: %1").arg(error));
        return false;
    }
    const QString source = settings.source == QLatin1String("gps") ? QStringLiteral("GPS") : QStringLiteral("fixed");
    m_channel->enqueue(*frame, QStringLiteral("%1  [%2]").arg(report->describe(), source), QStringLiteral("position"));
    return true;
}

// Send position now: to the air, and to the network when the upload is
// on.  A manual report counts as a report for SmartBeaconing, so it does
// not fire again a moment later for the same position.
void Session::positionNow()
{
    if (m_positionSender->running()) m_positionSender->triggerNow();
    else sendPosition();
    if (m_config.aprsis.enabled && m_config.aprsis.positionEnabled) sendPositionToAprsIs();
    if (smartActive(m_config)) {
        m_smart.noteSent(m_gps->isOpen() ? m_gps->fix().courseDeg : std::nullopt);
    }
}

void Session::beaconNow()
{
    if (m_beacon->running()) m_beacon->triggerNow();
    else sendBeacon();
}

int Session::cancelQueue()
{
    const int count = m_channel->clearQueue();
    if (count) systemLine(QStringLiteral("%1 pending frame(s) cancelled.").arg(count));
    return count;
}


// =====================================================================
//  APRS Internet System
// =====================================================================

void Session::configureAprsIs()
{
    const AprsIsConfig &c = m_config.aprsis;
    AprsIsClient::Settings settings;
    settings.host = c.host();
    settings.port = c.port;
    settings.callsign = m_config.myCall();
    settings.passcode = c.passcode;      // the upload switch decides what is sent, not the login
    settings.radiusKm = c.receive ? c.radiusKm : 0;
    settings.version = version();
    m_aprsIs->configure(settings);
    if (const auto reference = referencePosition()) {
        m_aprsIs->setFilterPosition(reference->first, reference->second);
    }
    const bool wanted = m_stationOn && c.enabled && m_config.station.callsign.toUpper() != QLatin1String("NOCALL");
    if (wanted && !m_aprsIs->isRunning()) m_aprsIs->start();
    else if (!wanted && m_aprsIs->isRunning()) m_aprsIs->stop();
}

QString Session::aprsIsStatusText() const
{
    if (!m_config.aprsis.enabled) return QString();
    QString text = m_aprsIs->statusText();
    if (m_config.aprsis.positionEnabled) {
        if (smartActive(m_config)) text += QStringLiteral(", position by SmartBeaconing");
        else if (m_aprsIsPosition->running()) text += QStringLiteral(", ") + m_aprsIsPosition->countdownText().toLower();
    }
    return text;
}

// Our own position straight to the network, as any Internet-only station
// sends it: source, a tocall, the TCPIP* path, the same report as on the
// air.  Needs a verified login.
bool Session::sendPositionToAprsIs()
{
    if (!m_config.aprsis.enabled) return false;
    if (m_config.station.callsign.toUpper() == QLatin1String("NOCALL")) return false;
    if (m_aprsIs->state() != AprsIsClient::State::Connected || !m_aprsIs->verified()) {
        systemLine(QStringLiteral("Position not sent to APRS-IS: not logged in with an accepted passcode."));
        return false;
    }
    const auto report = currentPosition();
    if (!report) return false;
    QString error;
    const QByteArray info = report->encode(&error);
    if (info.isEmpty()) {
        warn(QStringLiteral("Cannot build the position report: %1").arg(error));
        return false;
    }
    const QString destination = m_config.position.destination.isEmpty() ? aprs::DefaultTocall : m_config.position.destination;
    const QString line = QStringLiteral("%1>%2,TCPIP*:%3").arg(m_config.myCall(), destination, QString::fromLatin1(info));
    if (!m_aprsIs->sendPacket(line)) return false;
    append(QStringLiteral("position"), QStringLiteral("%1  [to APRS-IS]").arg(report->describe()), QStringLiteral("beacon"));
    return true;
}

// The range filter follows the station: a moving GPS position is re-sent
// by the client when it has moved far enough.
void Session::refreshAprsIsFilter()
{
    if (!m_aprsIs->isRunning() || !m_config.aprsis.receive) return;
    if (const auto reference = referencePosition()) {
        m_aprsIs->setFilterPosition(reference->first, reference->second);
    }
}

// What a receive-only IGate sends: the packet as heard, the q-construct
// qAO and our callsign appended to the path (aprs-is.net, "q Construct").
// Same rules as Dire Wolf's igate.c: nothing that already went through the
// Internet or asks not to (TCPIP, TCPXX, NOGATE, RFONLY in the path), no
// third-party packets, no generic queries, and the information part cut at
// the first CR or LF.  Only packets that decode as APRS are gated: the
// plain chat traffic of this program is not APRS and has no business on
// aprs.fi.  A digipeated copy heard again within a minute is not sent twice.
void Session::gateToAprsIs(const AX25Frame &frame, const AprsPacket &packet)
{
    Q_UNUSED(packet);
    if (!m_config.aprsis.enabled || !m_config.aprsis.gateRf) return;
    if (m_aprsIs->state() != AprsIsClient::State::Connected || !m_aprsIs->verified()) return;
    if (!frame.isUi() || frame.info.isEmpty()) return;
    for (const AX25Address &digi : frame.digipeaters) {
        const QString call = digi.call.toUpper();
        if (call == QLatin1String("TCPIP") || call == QLatin1String("TCPXX") || call == QLatin1String("NOGATE")
            || call == QLatin1String("RFONLY")) {
            return;
        }
    }
    const char dti = frame.info.at(0);
    if (dti == '}' || dti == '?') return;
    QByteArray info = frame.info;
    const int cut = std::min(info.indexOf('\r') < 0 ? info.size() : info.indexOf('\r'),
                             info.indexOf('\n') < 0 ? info.size() : info.indexOf('\n'));
    info.truncate(cut);
    if (info.isEmpty()) return;

    const QString key = frame.source.toString() + QLatin1Char(':') + QString::fromLatin1(info);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_gatedRecently.begin(); it != m_gatedRecently.end();) {
        if (now - it.value() > 60000) it = m_gatedRecently.erase(it);
        else ++it;
    }
    if (m_gatedRecently.contains(key)) return;
    m_gatedRecently.insert(key, now);

    const QString line = QStringLiteral("%1>%2,qAO,%3:%4")
        .arg(frame.source.toString(), frame.path(), m_config.myCall(), QString::fromLatin1(info));
    m_aprsIs->sendPacket(line);
}

// A packet from the server, TNC-2 form.  The stations list learns about it
// in every case (tagged [IS]); the chat shows it only when asked to, since
// the traffic within a few hundred kilometres can be relentless.  Messages
// addressed to us are always shown; they are not acknowledged, this is a
// receive-only connection.
void Session::onAprsIsPacket(const QString &line)
{
    const int colon = line.indexOf(':');
    const int gt = line.indexOf('>');
    if (colon < 0 || gt < 0 || gt > colon) return;
    const QString source = line.left(gt).trimmed();
    const QStringList path = line.mid(gt + 1, colon - gt - 1).split(',');
    if (source.isEmpty() || path.isEmpty()) return;
    const QString destination = path.first();
    const QByteArray info = line.mid(colon + 1).toUtf8();
    if (info.isEmpty()) return;
    // Our own packets come back through the network; nothing to learn.
    if (source.toUpper() == m_config.myCall().toUpper()) return;

    AprsPacket packet = AprsDecoder::decode(info, destination, source);
    if (packet.kind == AprsPacket::Kind::Unknown) return;
    m_stations.applyPacket(source, packet, QStringLiteral("APRS-IS ") + path.join(','));
    emit stationsChanged();

    if (packet.kind == AprsPacket::Kind::Message && packet.addressee.toUpper() == m_config.myCall().toUpper()) {
        append(QStringLiteral("<%1 to me via APRS-IS>").arg(source), packet.text, QStringLiteral("message"));
        return;
    }
    if (!m_config.aprsis.showInChat) return;
    if (packet.kind == AprsPacket::Kind::Position || packet.kind == AprsPacket::Kind::Object
        || packet.kind == AprsPacket::Kind::Item) {
        QString label = source;
        if (!packet.name.isEmpty()) label = QStringLiteral("%1 (%2)").arg(source, packet.name);
        append(QStringLiteral("[IS %1]").arg(label), positionSummary(packet), QStringLiteral("monitor"));
    } else if (packet.kind == AprsPacket::Kind::Weather) {
        append(QStringLiteral("[IS %1] weather").arg(source), weatherSummary(packet), QStringLiteral("monitor"));
    } else if (packet.kind == AprsPacket::Kind::Status && !packet.status.isEmpty()) {
        append(QStringLiteral("[IS %1] status").arg(source), packet.status, QStringLiteral("monitor"));
    } else if (packet.kind == AprsPacket::Kind::Message) {
        append(QStringLiteral("[IS %1 to %2]").arg(source, packet.addressee), packet.text, QStringLiteral("monitor"));
    }
}


// =====================================================================
//  Feedback and panel
// =====================================================================

void Session::onItemSent(const ChannelManager::OutgoingItem &item)
{
    if (item.kind == QLatin1String("beacon")) {
        append(QStringLiteral("beacon"), item.text, QStringLiteral("beacon"));
    } else if (item.kind == QLatin1String("position")) {
        append(QStringLiteral("position"), item.text, QStringLiteral("beacon"));
    } else if (item.kind == QLatin1String("message")) {
        // "to CALL: text" -> "<MYCALL to CALL>" + text
        const QString head = item.text.section(':', 0, 0);
        const QString body = item.text.contains(QStringLiteral(": ")) ? item.text.section(QStringLiteral(": "), 1) : item.text;
        append(QStringLiteral("<%1 %2>").arg(m_config.myCall(), head), body, QStringLiteral("tx"));
    } else if (item.kind == QLatin1String("ack")) {
        // Acknowledgements are protocol chatter, not conversation.
        if (m_config.ui.showMonitor) {
            QString text = item.text;
            if (text.startsWith(QStringLiteral("ack "))) text.remove(0, 4);
            append(QStringLiteral("ack"), text, QStringLiteral("monitor"));
        }
    } else {
        const QString suffix = item.total > 1 ? QStringLiteral("  [%1/%2]").arg(item.part).arg(item.total) : QString();
        append(QStringLiteral("<%1>").arg(m_config.myCall()), item.text + suffix, QStringLiteral("tx"));
    }
}

void Session::onItemDropped(const ChannelManager::OutgoingItem &item, const QString &reason)
{
    warn(QStringLiteral("Not sent (%1): %2").arg(reason, item.describe()));
}

void Session::onLog(const QString &level, const QString &message)
{
    if (level == QLatin1String("error")) warn(message);
    else if (level != QLatin1String("debug")) systemLine(message);
}

QString Session::beaconCountdown() const
{
    return m_beacon->countdownText();
}

QString Session::positionCountdown() const
{
    if (smartActive(m_config)) {
        return m_smart.statusText(m_gps->isOpen() ? m_gps->fix().speedKnots : std::nullopt);
    }
    return m_positionSender->countdownText();
}

double Session::gpsLatitude() const
{
    return m_gps->isOpen() && m_gps->fix().valid ? m_gps->fix().latitude : 0.0;
}

double Session::gpsLongitude() const
{
    return m_gps->isOpen() && m_gps->fix().valid ? m_gps->fix().longitude : 0.0;
}

bool Session::gpsHasFix() const
{
    return m_gps->isOpen() && m_gps->fix().valid;
}

QString Session::gpsStatusText() const
{
    if (m_gps->isOpen()) return m_gps->statusText();
    if (m_config.position.source == QLatin1String("gps")) return QStringLiteral("GPS: not open");
    return QString();
}


// =====================================================================
//  Settings
// =====================================================================

void Session::applySettings(const AppConfig &config)
{
    const bool modemChanged = config.modem.configFile != m_config.modem.configFile
                           || config.modem.channel != m_config.modem.channel;
    const bool beaconChanged = config.beacon.enabled != m_config.beacon.enabled
                            || config.beacon.intervalMinutes != m_config.beacon.intervalMinutes;
    const bool positionChanged = config.position.enabled != m_config.position.enabled
                              || config.position.intervalMinutes != m_config.position.intervalMinutes;
    const bool gpsChanged = config.position.source != m_config.position.source
                         || config.position.gpsPort != m_config.position.gpsPort
                         || config.position.gpsBaud != m_config.position.gpsBaud;
    const bool styleChanged = config.ui.qtStyle != m_config.ui.qtStyle;
    const bool isPositionChanged = networkPositionPeriodic(config) != networkPositionPeriodic(m_config)
                                || config.position.intervalMinutes != m_config.position.intervalMinutes;

    m_config = config;
    m_channel->reloadConfig(config);
    m_messages->reloadConfig(config);
    m_smart.settings = smartSettings(config);
    m_stations.expiryMinutes = config.messaging.stationExpiryMinutes;
    m_beacon->configure(config.beacon.enabled, config.beacon.intervalMinutes, beaconChanged);
    m_positionSender->configure(config.position.enabled, config.position.intervalMinutes, positionChanged);

    if (smartActive(config)) {
        if (!m_smartTimer->isActive()) {
            m_smart.reset();
            m_smartTimer->start();
        }
        // The fixed sender must not run alongside; two schedulers would both
        // queue positions and double the traffic.
        m_positionSender->stop();
    } else {
        m_smartTimer->stop();
    }

    configureAprsIs();
    // With SmartBeaconing the network report follows the movement too; the
    // fixed interval would only double the traffic.
    m_aprsIsPosition->configure(m_stationOn && networkPositionPeriodic(config), config.position.intervalMinutes, isPositionChanged);

    if (gpsChanged) m_gps->close();
    if (config.position.source == QLatin1String("gps") && config.position.gpsAutoOpen && !m_gps->isOpen()) {
        openGps();
    } else if (config.position.source != QLatin1String("gps") && m_gps->isOpen()) {
        m_gps->close();
    }

    systemLine(QStringLiteral("Configuration saved."));
    if (styleChanged) {
        systemLine(QStringLiteral("Qt style set to %1; restart AX25Chat for it to take effect.")
                       .arg(config.ui.qtStyle.isEmpty() ? QStringLiteral("the system default") : config.ui.qtStyle));
    }

    if (m_modem->isRunning()) {
        if (!config.modem.enabled) {
            systemLine(QStringLiteral("Modem taken out of the station, stopping it."));
            stopModem();
        } else if (modemChanged) {
            systemLine(QStringLiteral("Modem configuration changed, restarting the modem."));
            m_restartModemWhenStopped = true;
            stopModem();
        } else {
            pushChannelParams();
        }
    } else if (m_stationOn && config.modem.enabled && m_modem->state() == DirewolfEngine::State::Stopped) {
        systemLine(QStringLiteral("The station is on, starting the modem now."));
        QTimer::singleShot(200, this, &Session::startModem);
    }
    emit settingsApplied();
    emit modemStateChanged();
    emit panelChanged();
}


// =====================================================================
//  Log
// =====================================================================

void Session::append(const QString &prefix, const QString &message, const QString &colourKey)
{
    emit logEntry(prefix, message, colourKey);
}

void Session::systemLine(const QString &message)
{
    append(QStringLiteral("*"), message, QStringLiteral("system"));
}

void Session::warn(const QString &message)
{
    append(QStringLiteral("!"), message, QStringLiteral("error"));
}
