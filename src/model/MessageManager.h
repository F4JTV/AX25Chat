/*
 * MessageManager.h - APRS messaging.
 *
 * Port of ax25chat/messaging.py.  A message is an ordinary UI frame whose
 * information field is
 *
 *     : ADDRESSEE : text {id
 *     ^ ^           ^     ^
 *     | |           |     `-- optional 1 to 5 character identifier
 *     | |           `-------- up to 67 characters of text
 *     | `-------------------- exactly 9 characters, space padded on the right
 *     `---------------------- data type identifier
 *
 * When an identifier is present the recipient must answer with
 * ":SENDER   :ack<id>".  This class handles both ends: it retries an
 * unanswered message on a widening interval, and it answers messages
 * addressed to us.
 *
 * Two behaviours look like bugs and are not.  Duplicate suppression: the
 * sender keeps retransmitting until an acknowledgement gets through, so the
 * same message often arrives several times; each repeat is acknowledged
 * again, because the sender is repeating precisely because our earlier
 * acknowledgement did not arrive, but it is shown to the operator once.
 * Acknowledgements are never retried: if ours is lost the sender repeats
 * the message and we answer again, which converges.
 *
 * Reply-ack (APRS 1.2 addendum): one transmission can carry both a new
 * message and the acknowledgement of the previous one, written {12}34.
 * Incoming reply-acks are honoured; outgoing messages use a plain
 * acknowledgement, which every client understands.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "AppConfig.h"
#include "AprsPacket.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMap>
#include <QMetaType>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

class QTimer;

namespace aprsmsg {

constexpr int MaxTextLen = 67;          // per the APRS specification
constexpr int AddresseeLen = 9;
constexpr double DuplicateMemoryS = 600; // how long a received (sender, id) pair is remembered

// Pad or truncate a callsign to the fixed 9 character field.
QString formatAddressee(const QString &callsign);

// '|' and '~' are reserved inside the information field, '{' would be read
// as the start of the identifier, a newline would end the frame early.
QString sanitiseText(const QString &text);

QByteArray encodeMessage(const QString &addressee, const QString &text, const QString &messageId = QString());
QByteArray encodeAck(const QString &addressee, const QString &messageId, bool reject = false);

} // namespace aprsmsg


// One message awaiting acknowledgement.
struct OutgoingMessage
{
    enum class State { Pending, Waiting, Acked, Rejected, Failed };

    QString addressee;
    QString text;
    QString messageId;              // empty: no acknowledgement expected
    int attempts = 0;
    int maxAttempts = 5;
    State state = State::Pending;
    double createdAt = 0.0;         // seconds
    double lastSentAt = 0.0;
    double nextRetryAt = 0.0;

    QString describe() const;
    static QString stateName(State state);
};
Q_DECLARE_METATYPE(OutgoingMessage)


class MessageManager : public QObject
{
    Q_OBJECT

public:
    // Puts a frame on the transmit queue: (information field, addressee,
    // kind "message" or "ack", label the operator sees when it goes out).
    using SendFunction = std::function<bool(const QByteArray &, const QString &, const QString &, const QString &)>;
    using Clock = std::function<double()>;   // seconds

    explicit MessageManager(const AppConfig &config, SendFunction send, QObject *parent = nullptr);

    void start();
    void stop();
    void reloadConfig(const AppConfig &config);
    void setClock(Clock clock);

    QList<OutgoingMessage> pending() const;

    // Queue a message.  Returns nullopt when there is nothing to send or the
    // transmit queue refused it.  Bulletins (BLN...) never ask for an ack.
    std::optional<OutgoingMessage> send(const QString &addressee, const QString &text, bool requestAck = true);

    bool cancel(const QString &messageId);
    int cancelAll();

    // Process a received packet.  Returns true when it was consumed here
    // and the caller need not display it itself.
    bool handleIncoming(const QString &source, const AprsPacket &packet);

    // One pass of the retry loop.  The timer calls it every two seconds.
    void tick();

signals:
    void messageSent(const OutgoingMessage &message);
    void acknowledged(const OutgoingMessage &message);
    void rejected(const OutgoingMessage &message);
    void failed(const OutgoingMessage &message);
    void incoming(const QString &source, const AprsPacket &packet);
    void logMessage(const QString &level, const QString &message);

private:
    QString makeId();
    bool transmit(OutgoingMessage &message);
    double delayAfter(const OutgoingMessage &message) const;
    bool resolve(const QString &messageId, OutgoingMessage::State state);
    void sendAck(const QString &addressee, const QString &messageId);
    bool isDuplicate(const QString &source, const AprsPacket &packet);

    AppConfig m_config;
    SendFunction m_send;
    Clock m_clock;
    QMap<QString, OutgoingMessage> m_outgoing;   // by identifier
    QHash<QString, double> m_seen;               // "SOURCE\x1Fid" -> when
    int m_nextId = 1;
    QTimer *m_timer = nullptr;
};
