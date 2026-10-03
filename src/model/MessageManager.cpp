/*
 * MessageManager.cpp - APRS messaging.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MessageManager.h"

#include <QDateTime>
#include <QTimer>

#include <cmath>

namespace {

constexpr int TickMs = 2000;
const QString IdAlphabet = QStringLiteral("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");

double wallClockSeconds()
{
    return QDateTime::currentMSecsSinceEpoch() / 1000.0;
}

} // namespace


namespace aprsmsg {

QString formatAddressee(const QString &callsign)
{
    return callsign.trimmed().toUpper().left(AddresseeLen).leftJustified(AddresseeLen, ' ');
}

QString sanitiseText(const QString &textIn)
{
    QString text = textIn;
    text.replace('\r', ' ');
    text.replace('\n', ' ');
    QString cleaned;
    cleaned.reserve(text.size());
    for (const QChar c : text) {
        const ushort u = c.unicode();
        if (u == '|' || u == '~' || u == '{') continue;
        if (u < 0x20 || u > 0x7E) continue;
        cleaned.append(c);
    }
    return cleaned.trimmed().left(MaxTextLen);
}

QByteArray encodeMessage(const QString &addressee, const QString &text, const QString &messageId)
{
    QString body = QStringLiteral(":%1:%2").arg(formatAddressee(addressee), sanitiseText(text));
    if (!messageId.isEmpty()) body += '{' + messageId;
    return body.toLatin1();
}

QByteArray encodeAck(const QString &addressee, const QString &messageId, bool reject)
{
    return QStringLiteral(":%1:%2%3").arg(formatAddressee(addressee),
                                          reject ? QStringLiteral("rej") : QStringLiteral("ack"),
                                          messageId).toLatin1();
}

} // namespace aprsmsg


QString OutgoingMessage::describe() const
{
    const QString suffix = attempts > 1 ? QStringLiteral(" (try %1)").arg(attempts) : QString();
    return QStringLiteral("to %1: %2%3").arg(addressee, text, suffix);
}

QString OutgoingMessage::stateName(State state)
{
    switch (state) {
        case State::Pending:  return QStringLiteral("pending");
        case State::Waiting:  return QStringLiteral("waiting");
        case State::Acked:    return QStringLiteral("acked");
        case State::Rejected: return QStringLiteral("rejected");
        case State::Failed:   return QStringLiteral("failed");
    }
    return QString();
}


MessageManager::MessageManager(const AppConfig &config, SendFunction send, QObject *parent)
    : QObject(parent), m_config(config), m_send(std::move(send)), m_clock(wallClockSeconds)
{
    qRegisterMetaType<OutgoingMessage>("OutgoingMessage");
    qRegisterMetaType<AprsPacket>("AprsPacket");

    m_timer = new QTimer(this);
    m_timer->setInterval(TickMs);
    connect(m_timer, &QTimer::timeout, this, &MessageManager::tick);
}

void MessageManager::start() { m_timer->start(); }
void MessageManager::stop() { m_timer->stop(); }
void MessageManager::reloadConfig(const AppConfig &config) { m_config = config; }

void MessageManager::setClock(Clock clock)
{
    m_clock = clock ? std::move(clock) : Clock(wallClockSeconds);
}

QList<OutgoingMessage> MessageManager::pending() const
{
    QList<OutgoingMessage> out;
    for (const OutgoingMessage &m : m_outgoing) {
        if (m.state == OutgoingMessage::State::Pending || m.state == OutgoingMessage::State::Waiting) out.append(m);
    }
    return out;
}


// ---- sending -----------------------------------------------------------------------

// A short identifier, unique within this session.  Base 36 keeps it to one
// or two characters for the first thirteen hundred messages, which matters
// because the identifier eats into the 67 characters available for text.
QString MessageManager::makeId()
{
    int value = m_nextId++;
    QString text;
    while (true) {
        const int remainder = value % 36;
        value /= 36;
        text.prepend(IdAlphabet.at(remainder));
        if (value == 0) break;
    }
    return text.right(5);
}

std::optional<OutgoingMessage> MessageManager::send(const QString &addresseeIn, const QString &textIn, bool requestAck)
{
    const QString text = aprsmsg::sanitiseText(textIn);
    if (text.isEmpty()) return std::nullopt;
    const QString addressee = addresseeIn.trimmed().toUpper();
    if (addressee.isEmpty()) return std::nullopt;

    // Bulletins are broadcast to everyone and must not ask for an
    // acknowledgement; there is nobody in particular to send one.
    if (addressee.startsWith(QStringLiteral("BLN"))) requestAck = false;

    OutgoingMessage message;
    message.addressee = addressee;
    message.text = text;
    message.messageId = requestAck ? makeId() : QString();
    message.maxAttempts = qMax(1, m_config.messaging.maxAttempts);
    message.createdAt = m_clock();

    if (!transmit(message)) return std::nullopt;
    if (!message.messageId.isEmpty()) m_outgoing.insert(message.messageId, message);
    return message;
}

bool MessageManager::transmit(OutgoingMessage &message)
{
    const QByteArray info = aprsmsg::encodeMessage(message.addressee, message.text, message.messageId);
    const QString label = QStringLiteral("to %1: %2").arg(message.addressee, message.text);
    if (!m_send || !m_send(info, message.addressee, QStringLiteral("message"), label)) return false;

    message.attempts += 1;
    message.lastSentAt = m_clock();
    message.state = message.messageId.isEmpty() ? OutgoingMessage::State::Acked : OutgoingMessage::State::Waiting;
    if (!message.messageId.isEmpty()) message.nextRetryAt = message.lastSentAt + delayAfter(message);
    emit messageSent(message);
    return true;
}

// Between attempts the interval doubles, because a message that failed
// twice is usually blocked by something that will not clear in the next
// thirty seconds.  After the final attempt the wait is only the base
// interval: it is purely an acknowledgement window.  With the defaults:
// attempt 1 at 0s, 2 at 30s, 3 at 1m30, 4 at 3m30, 5 at 7m30, failed at 8m00.
double MessageManager::delayAfter(const OutgoingMessage &message) const
{
    const int base = qMax(5, m_config.messaging.retrySeconds);
    if (message.attempts >= message.maxAttempts) return base;
    return base * std::pow(2.0, message.attempts - 1);
}

bool MessageManager::cancel(const QString &messageId)
{
    return m_outgoing.remove(messageId) > 0;
}

int MessageManager::cancelAll()
{
    const int count = pending().size();
    m_outgoing.clear();
    return count;
}


// ---- incoming ------------------------------------------------------------------------

bool MessageManager::handleIncoming(const QString &sourceIn, const AprsPacket &packet)
{
    if (packet.kind != AprsPacket::Kind::Message) return false;

    const QString source = sourceIn.trimmed().toUpper();
    const QString mine = m_config.myCall().toUpper();
    const QString addressee = packet.addressee.trimmed().toUpper();

    // A reply-ack carries an acknowledgement inside an ordinary message.
    // Process it first, so a station that answers rather than acknowledging
    // still stops our retries.
    if (!packet.ackOf.isEmpty() && addressee == mine) {
        resolve(packet.ackOf, OutgoingMessage::State::Acked);
    }

    // Acknowledgement or rejection of something we sent.
    if (packet.messageKind == AprsPacket::MessageKind::Ack || packet.messageKind == AprsPacket::MessageKind::Rej) {
        if (addressee != mine) return true;     // someone else's acknowledgement
        resolve(packet.messageId, packet.messageKind == AprsPacket::MessageKind::Ack
                                      ? OutgoingMessage::State::Acked : OutgoingMessage::State::Rejected);
        return true;
    }

    if (packet.messageKind == AprsPacket::MessageKind::Invalid) return false;

    const bool isBulletin = packet.messageKind == AprsPacket::MessageKind::Bulletin;
    const bool forUs = addressee == mine;
    if (!forUs && !isBulletin) return false;    // addressed to someone else

    // Acknowledge before the duplicate check, because a repeat means our
    // previous acknowledgement did not get through.
    if (forUs && !packet.messageId.isEmpty() && m_config.messaging.autoAck) {
        sendAck(source, packet.messageId);
    }
    if (isDuplicate(source, packet)) return true;   // answered again, shown once

    emit incoming(source, packet);
    return true;
}

// A missing entry is not an error: a late acknowledgement for something we
// already gave up on, or a duplicate of one already processed.
bool MessageManager::resolve(const QString &messageId, OutgoingMessage::State state)
{
    auto it = m_outgoing.find(messageId);
    if (it == m_outgoing.end()) return false;
    OutgoingMessage message = it.value();
    m_outgoing.erase(it);
    message.state = state;
    if (state == OutgoingMessage::State::Acked) emit acknowledged(message);
    else emit rejected(message);
    return true;
}

void MessageManager::sendAck(const QString &addressee, const QString &messageId)
{
    const QByteArray info = aprsmsg::encodeAck(addressee, messageId);
    const QString label = QStringLiteral("ack %1 to %2").arg(messageId, addressee);
    if (!m_send || !m_send(info, addressee, QStringLiteral("ack"), label)) {
        emit logMessage(QStringLiteral("error"), QStringLiteral("Could not queue an ack for %1").arg(addressee));
    }
}

// Keyed on sender plus identifier when there is one.  Without an identifier
// there is nothing reliable to key on, so the text itself is used, which is
// what a retransmission would repeat anyway.
bool MessageManager::isDuplicate(const QString &source, const AprsPacket &packet)
{
    const QString key = source + QChar(0x1F)
                      + (packet.messageId.isEmpty() ? QStringLiteral("text:") + packet.text : packet.messageId);
    const double now = m_clock();
    const auto previous = m_seen.constFind(key);
    const bool duplicate = previous != m_seen.constEnd() && now - previous.value() < aprsmsg::DuplicateMemoryS;
    m_seen.insert(key, now);
    if (duplicate) return true;

    if (m_seen.size() > 500) {
        const double cutoff = now - aprsmsg::DuplicateMemoryS;
        for (auto it = m_seen.begin(); it != m_seen.end();) {
            if (it.value() < cutoff) it = m_seen.erase(it);
            else ++it;
        }
    }
    return false;
}


// ---- retrying ----------------------------------------------------------------------------

void MessageManager::tick()
{
    const double now = m_clock();
    const QStringList ids = m_outgoing.keys();
    for (const QString &id : ids) {
        auto it = m_outgoing.find(id);
        if (it == m_outgoing.end()) continue;
        OutgoingMessage &message = it.value();
        if (message.state != OutgoingMessage::State::Waiting) continue;
        if (now < message.nextRetryAt) continue;
        if (message.attempts >= message.maxAttempts) {
            OutgoingMessage done = message;
            m_outgoing.erase(it);
            done.state = OutgoingMessage::State::Failed;
            emit failed(done);
            continue;
        }
        transmit(message);
    }
}
