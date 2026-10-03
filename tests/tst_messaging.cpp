/*
 * tst_messaging.cpp - tests for MessageManager, StationRegistry and
 * PeriodicSender.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtTest>
#include <QSignalSpy>

#include "AprsDecoder.h"
#include "MessageManager.h"
#include "PeriodicSender.h"
#include "StationRegistry.h"

namespace {

struct Sent
{
    QByteArray info;
    QString addressee;
    QString kind;
    QString label;
};

struct FakeQueue
{
    QList<Sent> sent;
    bool accept = true;

    MessageManager::SendFunction sender()
    {
        return [this](const QByteArray &info, const QString &addressee, const QString &kind, const QString &label) {
            if (!accept) return false;
            sent.append({info, addressee, kind, label});
            return true;
        };
    }
};

AppConfig config()
{
    AppConfig c;
    c.station.callsign = QStringLiteral("F1ABC");
    c.station.ssid = 7;
    c.messaging.maxAttempts = 5;
    c.messaging.retrySeconds = 30;
    return c;
}

AprsPacket packetFrom(const char *info)
{
    return AprsDecoder::decode(QByteArray(info), "APRS", "F1XYZ");
}

} // namespace


class MessagingTest : public QObject
{
    Q_OBJECT

private slots:

    void encoding()
    {
        QCOMPARE(aprsmsg::formatAddressee(" wa1xyx-15 "), QStringLiteral("WA1XYX-15"));
        QCOMPARE(aprsmsg::formatAddressee("F1ABC"), QStringLiteral("F1ABC    "));
        QCOMPARE(aprsmsg::formatAddressee("TOOLONGCALLSIGN"), QStringLiteral("TOOLONGCA"));
        QCOMPARE(aprsmsg::sanitiseText("  hello {world} | ~ caf\u00E9\nline  "), QStringLiteral("hello world}   caf line"));
        QCOMPARE(aprsmsg::sanitiseText(QString(80, 'x')).size(), 67);
        QCOMPARE(aprsmsg::encodeMessage("WA1XYX-15", "Howdy", "12"), QByteArray(":WA1XYX-15:Howdy{12"));
        QCOMPARE(aprsmsg::encodeMessage("BLN1", "Meeting"), QByteArray(":BLN1     :Meeting"));
        QCOMPARE(aprsmsg::encodeAck("WA1XYX-15", "12"), QByteArray(":WA1XYX-15:ack12"));
        QCOMPARE(aprsmsg::encodeAck("WA1XYX-15", "12", true), QByteArray(":WA1XYX-15:rej12"));
    }

    void identifiersAreBase36()
    {
        FakeQueue queue;
        MessageManager mm(config(), queue.sender());
        QStringList ids;
        for (int i = 0; i < 40; i++) {
            const auto m = mm.send("F1XYZ", QStringLiteral("msg %1").arg(i));
            QVERIFY(m);
            ids << m->messageId;
        }
        QCOMPARE(ids.at(0), QStringLiteral("1"));
        QCOMPARE(ids.at(8), QStringLiteral("9"));
        QCOMPARE(ids.at(9), QStringLiteral("A"));
        QCOMPARE(ids.at(34), QStringLiteral("Z"));
        QCOMPARE(ids.at(35), QStringLiteral("10"));
        QCOMPARE(QSet<QString>(ids.begin(), ids.end()).size(), 40);
    }

    void sendAndRetrySchedule()
    {
        FakeQueue queue;
        MessageManager mm(config(), queue.sender());
        double clock = 1000.0;
        mm.setClock([&clock] { return clock; });
        QSignalSpy sentSpy(&mm, &MessageManager::messageSent);
        QSignalSpy failedSpy(&mm, &MessageManager::failed);

        const auto m = mm.send("f1xyz", "  Hello there  ");
        QVERIFY(m);
        QCOMPARE(m->addressee, QStringLiteral("F1XYZ"));
        QCOMPARE(m->text, QStringLiteral("Hello there"));
        QCOMPARE(m->attempts, 1);
        QCOMPARE(m->state, OutgoingMessage::State::Waiting);
        QCOMPARE(m->describe(), QStringLiteral("to F1XYZ: Hello there"));
        QCOMPARE(queue.sent.size(), 1);
        QCOMPARE(queue.sent.at(0).info, QByteArray(":F1XYZ    :Hello there{1"));
        QCOMPARE(queue.sent.at(0).kind, QStringLiteral("message"));
        QCOMPARE(queue.sent.at(0).label, QStringLiteral("to F1XYZ: Hello there"));
        QCOMPARE(mm.pending().size(), 1);

        // Defaults: attempts at 0, 30, 90, 210, 450 s; failed at 480 s.
        const QList<double> expected = {30, 90, 210, 450};
        int attempts = 1;
        for (double at : expected) {
            clock = 1000.0 + at - 1;
            mm.tick();
            QCOMPARE(queue.sent.size(), attempts);
            clock = 1000.0 + at;
            mm.tick();
            attempts++;
            QCOMPARE(queue.sent.size(), attempts);
            QCOMPARE(sentSpy.last().at(0).value<OutgoingMessage>().attempts, attempts);
        }
        QCOMPARE(sentSpy.last().at(0).value<OutgoingMessage>().describe(), QStringLiteral("to F1XYZ: Hello there (try 5)"));
        clock = 1000.0 + 479;
        mm.tick();
        QCOMPARE(failedSpy.count(), 0);
        clock = 1000.0 + 480;
        mm.tick();
        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(failedSpy.at(0).at(0).value<OutgoingMessage>().state, OutgoingMessage::State::Failed);
        QCOMPARE(mm.pending().size(), 0);
    }

    void acknowledgementStopsRetries()
    {
        FakeQueue queue;
        MessageManager mm(config(), queue.sender());
        double clock = 1000.0;
        mm.setClock([&clock] { return clock; });
        QSignalSpy acked(&mm, &MessageManager::acknowledged);
        QSignalSpy rejected(&mm, &MessageManager::rejected);

        const auto m = mm.send("F1XYZ", "Hello");
        QVERIFY(m);

        // Someone else's ack: consumed, no effect.
        QVERIFY(mm.handleIncoming("F1XYZ", packetFrom(":F1OTHER  :ack1")));
        QCOMPARE(acked.count(), 0);

        QVERIFY(mm.handleIncoming("F1XYZ", packetFrom(":F1ABC-7  :ack1")));
        QCOMPARE(acked.count(), 1);
        QCOMPARE(acked.at(0).at(0).value<OutgoingMessage>().state, OutgoingMessage::State::Acked);
        QCOMPARE(mm.pending().size(), 0);
        clock += 1000;
        mm.tick();
        QCOMPARE(queue.sent.size(), 1);          // no retry after the ack

        // A late duplicate ack is harmless.
        QVERIFY(mm.handleIncoming("F1XYZ", packetFrom(":F1ABC-7  :ack1")));
        QCOMPARE(acked.count(), 1);

        // Rejection.
        const auto m2 = mm.send("F1XYZ", "Second");
        QVERIFY(mm.handleIncoming("F1XYZ", packetFrom(":F1ABC-7  :rej2")));
        QCOMPARE(rejected.count(), 1);
        QCOMPARE(mm.pending().size(), 0);
    }

    void replyAckIsHonoured()
    {
        FakeQueue queue;
        MessageManager mm(config(), queue.sender());
        QSignalSpy acked(&mm, &MessageManager::acknowledged);
        QSignalSpy incoming(&mm, &MessageManager::incoming);

        mm.send("F1XYZ", "Question?");
        // The answer carries the ack of our message 1, and its own id 7.
        QVERIFY(mm.handleIncoming("F1XYZ", packetFrom(":F1ABC-7  :Yes indeed{07}1")));
        QCOMPARE(acked.count(), 1);
        QCOMPARE(incoming.count(), 1);
        QCOMPARE(incoming.at(0).at(1).value<AprsPacket>().text, QStringLiteral("Yes indeed"));
        // ...and we acknowledged their message.
        QCOMPARE(queue.sent.size(), 2);
        QCOMPARE(queue.sent.at(1).kind, QStringLiteral("ack"));
        QCOMPARE(queue.sent.at(1).info, QByteArray(":F1XYZ    :ack07"));
    }

    void incomingMessagesAreAckedAndShownOnce()
    {
        FakeQueue queue;
        MessageManager mm(config(), queue.sender());
        double clock = 1000.0;
        mm.setClock([&clock] { return clock; });
        QSignalSpy incoming(&mm, &MessageManager::incoming);

        QVERIFY(mm.handleIncoming("F1XYZ", packetFrom(":F1ABC-7  :Hello{5")));
        QCOMPARE(incoming.count(), 1);
        QCOMPARE(queue.sent.size(), 1);
        QCOMPARE(queue.sent.at(0).info, QByteArray(":F1XYZ    :ack5"));
        QCOMPARE(queue.sent.at(0).label, QStringLiteral("ack 5 to F1XYZ"));

        // The repeat is acknowledged again but not shown again.
        clock += 30;
        QVERIFY(mm.handleIncoming("F1XYZ", packetFrom(":F1ABC-7  :Hello{5")));
        QCOMPARE(incoming.count(), 1);
        QCOMPARE(queue.sent.size(), 2);

        // Much later the same id is a new message.
        clock += 700;
        QVERIFY(mm.handleIncoming("F1XYZ", packetFrom(":F1ABC-7  :Hello{5")));
        QCOMPARE(incoming.count(), 2);

        // Without an identifier: no ack, duplicates keyed on the text.
        QVERIFY(mm.handleIncoming("F1XYZ", packetFrom(":F1ABC-7  :No id here")));
        QCOMPARE(incoming.count(), 3);
        QCOMPARE(queue.sent.size(), 3);
        QVERIFY(mm.handleIncoming("F1XYZ", packetFrom(":F1ABC-7  :No id here")));
        QCOMPARE(incoming.count(), 3);

        // Not for us, not a bulletin: left to the caller.
        QVERIFY(!mm.handleIncoming("F1XYZ", packetFrom(":F1OTHER  :Hi{9")));
        QCOMPARE(queue.sent.size(), 3);

        // Bulletins are shown, never acknowledged.
        QVERIFY(mm.handleIncoming("F1XYZ", packetFrom(":BLN1     :Club meeting")));
        QCOMPARE(incoming.count(), 4);
        QCOMPARE(queue.sent.size(), 3);

        // Invalid messages and non-messages are not consumed.
        QVERIFY(!mm.handleIncoming("F1XYZ", packetFrom(":bad")));
        QVERIFY(!mm.handleIncoming("F1XYZ", packetFrom("!4903.50N/07201.75W-")));
    }

    void autoAckCanBeTurnedOff()
    {
        FakeQueue queue;
        AppConfig c = config();
        c.messaging.autoAck = false;
        MessageManager mm(c, queue.sender());
        QVERIFY(mm.handleIncoming("F1XYZ", packetFrom(":F1ABC-7  :Hello{5")));
        QCOMPARE(queue.sent.size(), 0);
    }

    void bulletinsAndCancellation()
    {
        FakeQueue queue;
        MessageManager mm(config(), queue.sender());

        const auto b = mm.send("BLN1", "Meeting tonight");
        QVERIFY(b);
        QVERIFY(b->messageId.isEmpty());
        QCOMPARE(b->state, OutgoingMessage::State::Acked);
        QCOMPARE(queue.sent.at(0).info, QByteArray(":BLN1     :Meeting tonight"));
        QCOMPARE(mm.pending().size(), 0);

        const auto plain = mm.send("F1XYZ", "No ack wanted", false);
        QVERIFY(plain && plain->messageId.isEmpty());

        QVERIFY(!mm.send("F1XYZ", "   "));
        QVERIFY(!mm.send("", "text"));
        queue.accept = false;
        QVERIFY(!mm.send("F1XYZ", "refused"));
        queue.accept = true;

        const auto a = mm.send("F1XYZ", "one");
        const auto bb = mm.send("F1XYZ", "two");
        QCOMPARE(mm.pending().size(), 2);
        QVERIFY(mm.cancel(a->messageId));
        QVERIFY(!mm.cancel(a->messageId));
        QCOMPARE(mm.cancelAll(), 1);
        QCOMPARE(mm.pending().size(), 0);
        Q_UNUSED(bb);
    }

    // ---- StationRegistry -------------------------------------------------------------

    void stations()
    {
        StationRegistry registry(120);
        double clock = 1000.0;
        registry.setClock([&clock] { return clock; });

        registry.noteHeard("f1xyz", "CHAT,WIDE1-1*");
        registry.noteHeard("F1XYZ");
        auto s = registry.get("F1XYZ");
        QVERIFY(s);
        QCOMPARE(s->packets, 2);
        QCOMPARE(s->path, QStringLiteral("CHAT,WIDE1-1*"));
        QVERIFY(!s->hasPosition());
        QCOMPARE(s->describe(clock), QStringLiteral("F1XYZ   0s"));

        clock += 100;
        registry.applyPacket("F1XYZ", packetFrom("!4340.00N/00522.00E>090/030Mobile"));
        s = registry.get("F1XYZ");
        QVERIFY(s->hasPosition());
        QCOMPARE(s->symbol(), QStringLiteral("/>"));
        QVERIFY(std::abs(*s->speedKnots - 30.0) < 0.1);
        QCOMPARE(s->comment, QStringLiteral("Mobile"));
        // Reference: Marseille.  Station at 43.667N 5.367E: about 41 km NNE.
        const QString line = s->describe(clock, std::make_pair(43.2965, 5.3698));
        QVERIFY2(line.startsWith(QStringLiteral("F1XYZ   41 km N")), qPrintable(line));
        QVERIFY(line.endsWith(QStringLiteral("0s   30 kn")));
        QVERIFY(s->tooltip().contains(QStringLiteral("Course 90 deg, 30 kn")));
        QVERIFY(s->tooltip().contains(QStringLiteral("43.66667N  5.36667E")));

        // A report without course and speed clears them.
        registry.applyPacket("F1XYZ", packetFrom("!4340.00N/00522.00E>Parked"));
        s = registry.get("F1XYZ");
        QVERIFY(!s->speedKnots && !s->courseDeg);
        QCOMPARE(s->comment, QStringLiteral("Parked"));

        registry.applyPacket("F1XYZ", packetFrom(">Net control"));
        registry.applyPacket("F1XYZ", packetFrom(":F1ABC-7  :Hi{1"));
        registry.applyPacket("F1WX", packetFrom("_10090556c220s004g005t077r000p000P000h50b09900"));
        s = registry.get("F1XYZ");
        QCOMPARE(s->status, QStringLiteral("Net control"));
        QCOMPARE(s->lastMessage, QStringLiteral("Hi"));
        QVERIFY(registry.get("F1WX")->weather.contains(QStringLiteral("25.0 C")));

        // Ages and ordering.
        clock += 200;
        QCOMPARE(registry.get("F1XYZ")->ageText(clock), QStringLiteral("3m"));
        clock += 7001;
        QCOMPARE(registry.get("F1XYZ")->ageText(clock), QStringLiteral("2h"));
        registry.noteHeard("F1NEW");
        const auto listed = registry.listed();
        QCOMPARE(listed.size(), 3);
        QCOMPARE(listed.at(0).callsign, QStringLiteral("F1NEW"));
        QCOMPARE(registry.listed(1).size(), 1);
        QCOMPARE(registry.withPosition().size(), 1);

        // Expiry after 120 minutes: F1XYZ and F1WX were heard 7201 s ago.
        QCOMPARE(registry.expire(), 2);
        QCOMPARE(registry.size(), 1);
        registry.expiryMinutes = 0;
        clock += 1e6;
        QCOMPARE(registry.expire(), 0);
        registry.clear();
        QCOMPARE(registry.size(), 0);
    }

    // ---- PeriodicSender ------------------------------------------------------------

    void periodicSender()
    {
        int calls = 0;
        PeriodicSender sender("Beacon", [&calls] { calls++; return true; });
        double clock = 1000.0;
        sender.setClock([&clock] { return clock; });
        QSignalSpy fired(&sender, &PeriodicSender::fired);
        QSignalSpy logged(&sender, &PeriodicSender::logMessage);

        QVERIFY(!sender.running());
        QCOMPARE(sender.secondsRemaining(), -1);
        QCOMPARE(sender.countdownText(), QStringLiteral("Beacon: off"));

        sender.configure(true, 0);                 // clamped to 1 minute
        QCOMPARE(sender.intervalMinutes(), 1);
        QVERIFY(sender.running());
        QCOMPARE(sender.secondsRemaining(), 60);
        QCOMPARE(logged.last().at(1).toString(), QStringLiteral("Beacon enabled, every 1 min"));
        clock += 25;
        QCOMPARE(sender.countdownText(), QStringLiteral("Beacon: next in 00:35"));

        QVERIFY(sender.triggerNow());
        QCOMPARE(calls, 1);
        QCOMPARE(fired.count(), 1);
        QCOMPARE(sender.secondsRemaining(), 60);   // a full interval from now

        sender.configure(false, 15, false);        // settings only, no restart
        QVERIFY(sender.running());
        QCOMPARE(sender.intervalMinutes(), 15);
        sender.configure(false, 15);
        QVERIFY(!sender.running());
        QCOMPARE(logged.last().at(1).toString(), QStringLiteral("Beacon disabled"));

        // Trigger while disabled: sends, stays disabled.
        QVERIFY(sender.triggerNow());
        QCOMPARE(calls, 2);
        QVERIFY(!sender.running());
    }
};

QTEST_GUILESS_MAIN(MessagingTest)
#include "tst_messaging.moc"
