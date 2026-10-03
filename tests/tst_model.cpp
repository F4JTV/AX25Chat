/*
 * tst_model.cpp - tests for AppConfig and ChannelManager.
 *
 * The channel manager is driven with an injected clock and a fake modem, so
 * every timing rule is checked without waiting for real time to pass.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtTest>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "AppConfig.h"
#include "ChannelManager.h"

namespace {

// A fake modem: records frames, answers the transmit-queue query.
struct FakeModem
{
    QList<QByteArray> sent;
    bool accept = true;
    int queued = 0;

    ChannelManager::SendFunction sender()
    {
        return [this](const QByteArray &raw) {
            if (!accept) return false;
            sent.append(raw);
            return true;
        };
    }
    ChannelManager::TxQueueFunction txQueue()
    {
        return [this] { return queued; };
    }
};

AX25Frame frameFrom(const QString &source, const char *text)
{
    auto frame = AX25Frame::ui(source, "CHAT", QByteArray(text));
    Q_ASSERT(frame);
    return *frame;
}

AppConfig quietConfig()
{
    AppConfig config;
    config.station.callsign = QStringLiteral("F1ABC");
    config.station.ssid = 7;
    config.channel.randomJitterMs = 0;       // deterministic
    config.channel.rxHoldOffMs = 1500;
    config.channel.interFrameGapMs = 700;
    config.channel.maxDeferSeconds = 120;
    return config;
}

} // namespace


class ModelTest : public QObject
{
    Q_OBJECT

private slots:

    // ---- AppConfig ---------------------------------------------------------------

    void configRoundTrip()
    {
        AppConfig config;
        config.station.callsign = QStringLiteral("F1ABC");
        config.station.ssid = 7;
        config.modem.configFile = QStringLiteral("/tmp/direwolf.conf");
        config.modem.channel = 1;
        config.modem.persistence = 100;
        config.channel.useDcd = false;
        config.position.latitude = 43.2965;
        config.position.smartTurnSlope = 30.5;
        config.notifications.onPosition = true;
        config.ui.qtStyle = QStringLiteral("Fusion");

        const AppConfig back = AppConfig::fromJson(config.toJson());
        QCOMPARE(back.station.callsign, QStringLiteral("F1ABC"));
        QCOMPARE(back.station.ssid, 7);
        QCOMPARE(back.modem.configFile, QStringLiteral("/tmp/direwolf.conf"));
        QCOMPARE(back.modem.channel, 1);
        QCOMPARE(back.modem.persistence, 100);
        QCOMPARE(back.channel.useDcd, false);
        QCOMPARE(back.position.latitude, 43.2965);
        QCOMPARE(back.position.smartTurnSlope, 30.5);
        QCOMPARE(back.notifications.onPosition, true);
        QCOMPARE(back.ui.qtStyle, QStringLiteral("Fusion"));
        // Something left at its default survives too.
        QCOMPARE(back.messaging.retrySeconds, 30);
        QCOMPARE(back.beacon.text, config.beacon.text);
    }

    void configMigratesPythonFile()
    {
        // Sections as written by the Python version, including the two that
        // no longer exist.
        const QByteArray json = R"({
            "station": {"callsign": "F1ABC", "ssid": 7, "destination": "CHAT"},
            "tnc": {"transport": "tcp", "host": "127.0.0.1", "port": 8001, "kiss_channel": 1,
                    "txdelay": 40, "persistence": 128, "slottime": 20, "txtail": 8,
                    "full_duplex": true, "send_kiss_params": false},
            "direwolf": {"auto_launch": false, "executable": "/usr/bin/direwolf",
                         "config_file": "/home/op/direwolf.conf", "extra_args": "-p",
                         "echo_output": false, "show_console": true},
            "channel": {"rx_hold_off_ms": 2000, "txbuf_poll_ms": 500},
            "unknown_section": {"x": 1}
        })";
        const AppConfig config = AppConfig::fromJson(QJsonDocument::fromJson(json).object());
        QCOMPARE(config.station.callsign, QStringLiteral("F1ABC"));
        QCOMPARE(config.modem.channel, 1);
        QCOMPARE(config.modem.txdelay, 40);
        QCOMPARE(config.modem.persistence, 128);
        QCOMPARE(config.modem.slottime, 20);
        QCOMPARE(config.modem.txtail, 8);
        QCOMPARE(config.modem.fullDuplex, true);
        QCOMPARE(config.modem.sendParams, false);
        QCOMPARE(config.modem.configFile, QStringLiteral("/home/op/direwolf.conf"));
        QCOMPARE(config.modem.autoStart, false);
        QCOMPARE(config.modem.echoLog, false);
        QCOMPARE(config.channel.rxHoldOffMs, 2000);
        QCOMPARE(config.channel.useDcd, true);       // new key, default
        // Saved again, the file carries the new section only.
        const QJsonObject saved = config.toJson();
        QVERIFY(saved.contains("modem"));
        QVERIFY(!saved.contains("tnc"));
        QVERIFY(!saved.contains("direwolf"));
    }

    void configCoercesAndIgnores()
    {
        const QByteArray json = R"({
            "station": {"callsign": "F1ABC", "ssid": "7", "max_info_len": "abc", "bogus": 1},
            "beacon": {"enabled": "true", "interval_minutes": 15.9},
            "position": {"latitude": "43.5", "smart_enabled": 1},
            "ui": {"font_size": null}
        })";
        const AppConfig config = AppConfig::fromJson(QJsonDocument::fromJson(json).object());
        QCOMPARE(config.station.ssid, 7);
        QCOMPARE(config.station.maxInfoLen, 200);            // unparsable, default kept
        QCOMPARE(config.beacon.enabled, true);
        QCOMPARE(config.beacon.intervalMinutes, 15);
        QCOMPARE(config.position.latitude, 43.5);
        QCOMPARE(config.position.smartEnabled, true);
        QCOMPARE(config.ui.fontSize, 11);
    }

    void configHelpers()
    {
        AppConfig config;
        config.station.callsign = QStringLiteral(" f1abc ");
        config.station.ssid = 7;
        QCOMPARE(config.myCall(), QStringLiteral("F1ABC-7"));
        config.station.ssid = 0;
        QCOMPARE(config.myCall(), QStringLiteral("F1ABC"));
        config.station.callsign.clear();
        QCOMPARE(config.myCall(), QStringLiteral("NOCALL"));

        config.station.digipeaters = QStringLiteral(" wide1-1 , WIDE2-1,, ");
        QCOMPARE(config.digipeaterList(), QStringList({"WIDE1-1", "WIDE2-1"}));
        QCOMPARE(config.digipeaterList("relay"), QStringList({"RELAY"}));
        QCOMPARE(config.digipeaterList("  "), QStringList({"WIDE1-1", "WIDE2-1"}));

        config.station.callsign = QStringLiteral("F1ABC");
        config.station.ssid = 7;
        QCOMPARE(config.beaconText(), QStringLiteral("AX25Chat station F1ABC-7 - QRV packet"));
        config.station.operatorName = QStringLiteral("Yo");
        config.station.locator = QStringLiteral("JN33RU");
        QCOMPARE(config.beaconText(), QStringLiteral("AX25Chat station F1ABC-7 Yo JN33RU - QRV packet"));
        config.beacon.text = QStringLiteral("{name} {loc}");
        config.station.operatorName.clear();
        config.station.locator.clear();
        QCOMPARE(config.beaconText(), QString());
    }

    void configSaveAndLoad()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("sub/config.json"));

        AppConfig config;
        config.station.callsign = QStringLiteral("F1ABC");
        config.modem.channel = 2;
        QString error;
        QCOMPARE(config.save(path, &error), path);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        const AppConfig back = AppConfig::load(path);
        QCOMPARE(back.station.callsign, QStringLiteral("F1ABC"));
        QCOMPARE(back.modem.channel, 2);

        // Absent or corrupt files fall back to defaults.
        QCOMPARE(AppConfig::load(dir.filePath("missing.json")).station.callsign, QStringLiteral("NOCALL"));
        QFile corrupt(dir.filePath("corrupt.json"));
        QVERIFY(corrupt.open(QIODevice::WriteOnly));
        corrupt.write("{not json");
        corrupt.close();
        QCOMPARE(AppConfig::load(corrupt.fileName()).station.callsign, QStringLiteral("NOCALL"));

        QVERIFY(AppConfig::configPath().endsWith(QStringLiteral("AX25Chat/config.json")));
    }

    // ---- ChannelManager -----------------------------------------------------------

    void offlineSendsNothing()
    {
        FakeModem modem;
        ChannelManager cm(quietConfig(), modem.sender(), modem.txQueue());
        qint64 clock = 1000;
        cm.setClock([&clock] { return clock; });

        cm.enqueue(frameFrom("F1ABC-7", "hello"), "hello");
        cm.tick();
        QCOMPARE(cm.state(), ChannelManager::State::Offline);
        QCOMPARE(modem.sent.size(), 0);
        QCOMPARE(cm.pending(), 1);
    }

    void clearChannelSendsAtOnce()
    {
        FakeModem modem;
        ChannelManager cm(quietConfig(), modem.sender(), modem.txQueue());
        qint64 clock = 1000;
        cm.setClock([&clock] { return clock; });
        QSignalSpy sent(&cm, &ChannelManager::itemSent);
        QSignalSpy complete(&cm, &ChannelManager::txComplete);

        cm.setOnline(true);
        cm.tick();
        QCOMPARE(cm.state(), ChannelManager::State::Clear);

        const auto item = cm.enqueue(frameFrom("F1ABC-7", "hello"), "hello", "chat", 1, 2);
        QCOMPARE(item.describe(), QStringLiteral("hello  [1/2]"));
        clock += 100;
        cm.tick();
        QCOMPARE(modem.sent.size(), 1);
        QCOMPARE(sent.count(), 1);
        QCOMPARE(cm.state(), ChannelManager::State::Transmitting);
        QCOMPARE(cm.pending(), 0);

        // Handed over but not keyed yet, then keyed, then unkeyed, then
        // the gap, then clear.
        clock += 100;
        cm.tick();
        QCOMPARE(cm.state(), ChannelManager::State::Transmitting);
        QVERIFY(cm.busyReason().contains(QStringLiteral("waiting for the transmitter")));
        cm.setPtt(true);
        clock += 100;
        cm.tick();
        QCOMPARE(cm.busyReason(), QStringLiteral("transmitting"));
        cm.setPtt(false);
        clock += 100;
        cm.tick();
        QCOMPARE(cm.state(), ChannelManager::State::Transmitting);   // inter-frame gap
        QCOMPARE(cm.busyReason(), QStringLiteral("inter-frame gap"));
        clock += 700;
        cm.tick();
        QCOMPARE(cm.state(), ChannelManager::State::Clear);
        QCOMPARE(complete.count(), 1);
    }

    // Two frames queued together must be two keyings, each with its own
    // TXDELAY: the second waits for the transmitter to key and unkey for
    // the first, even when the modem's own queue already looks empty.
    void secondFrameWaitsForTheFirstKeying()
    {
        FakeModem modem;
        ChannelManager cm(quietConfig(), modem.sender(), modem.txQueue());
        qint64 clock = 1000;
        cm.setClock([&clock] { return clock; });
        cm.setOnline(true);
        cm.tick();
        cm.enqueueMany({frameFrom("F1ABC-7", "one"), frameFrom("F1ABC-7", "two")}, {"one", "two"}, "chat");
        clock += 100;
        cm.tick();
        QCOMPARE(modem.sent.size(), 1);
        // The modem dequeued it at once; no PTT report yet.
        modem.queued = 0;
        for (int i = 0; i < 10; i++) { clock += 100; cm.tick(); }
        QCOMPARE(modem.sent.size(), 1);
        cm.setPtt(true);
        for (int i = 0; i < 20; i++) { clock += 100; cm.tick(); }
        QCOMPARE(modem.sent.size(), 1);
        cm.setPtt(false);
        clock += 100;
        cm.tick();
        QCOMPARE(modem.sent.size(), 1);                 // the gap
        clock += 700;
        cm.tick();
        QCOMPARE(modem.sent.size(), 2);
    }

    // A modem that never reports its PTT (no feedback at all) does not
    // hold the queue forever.
    void noPttFeedbackTimesOut()
    {
        FakeModem modem;
        ChannelManager cm(quietConfig(), modem.sender(), modem.txQueue());
        qint64 clock = 1000;
        cm.setClock([&clock] { return clock; });
        cm.setOnline(true);
        cm.tick();
        cm.enqueueMany({frameFrom("F1ABC-7", "one"), frameFrom("F1ABC-7", "two")}, {"one", "two"}, "chat");
        clock += 100;
        cm.tick();
        QCOMPARE(modem.sent.size(), 1);
        clock += 5100;
        cm.tick();
        QCOMPARE(modem.sent.size(), 2);
    }

    void carrierDefersUntilQuiet()
    {
        FakeModem modem;
        ChannelManager cm(quietConfig(), modem.sender(), modem.txQueue());
        qint64 clock = 1000;
        cm.setClock([&clock] { return clock; });
        cm.setOnline(true);

        cm.setDcd(true);
        cm.tick();
        QCOMPARE(cm.state(), ChannelManager::State::RxBusy);

        cm.enqueue(frameFrom("F1ABC-7", "hello"), "hello");
        cm.tick();
        QCOMPARE(cm.state(), ChannelManager::State::Deferred);
        QCOMPARE(cm.busyReason(), QStringLiteral("carrier detected"));
        QCOMPARE(modem.sent.size(), 0);

        // Carrier drops: the quiet period starts now.
        clock += 3000;
        cm.setDcd(false);
        cm.tick();
        QCOMPARE(cm.state(), ChannelManager::State::Deferred);
        QVERIFY(cm.busyReason().startsWith(QStringLiteral("another station heard")));
        clock += 1400;
        cm.tick();
        QCOMPARE(modem.sent.size(), 0);
        clock += 200;
        cm.tick();
        QCOMPARE(modem.sent.size(), 1);
    }

    void receivedFrameStartsHoldOff()
    {
        FakeModem modem;
        AppConfig config = quietConfig();
        ChannelManager cm(config, modem.sender(), modem.txQueue());
        qint64 clock = 1000;
        cm.setClock([&clock] { return clock; });
        cm.setOnline(true);

        // Our own echo is not activity...
        const AX25Frame ours = frameFrom("F1ABC-7", "echo");
        cm.noteRx(&ours);
        cm.enqueue(frameFrom("F1ABC-7", "hello"), "hello");
        cm.tick();
        QCOMPARE(modem.sent.size(), 1);

        // ...another station is.
        clock += 5000;
        modem.queued = 0;
        const AX25Frame theirs = frameFrom("F1XYZ", "hi");
        cm.noteRx(&theirs);
        cm.enqueue(frameFrom("F1ABC-7", "again"), "again");
        cm.tick();
        QCOMPARE(cm.state(), ChannelManager::State::Deferred);
        clock += 1500;
        cm.tick();
        QCOMPARE(modem.sent.size(), 2);

        // With treatOwnEchoAsBusy, our own echo counts too.
        config.channel.treatOwnEchoAsBusy = true;
        cm.reloadConfig(config);
        clock += 5000;
        cm.noteRx(&ours);
        cm.enqueue(frameFrom("F1ABC-7", "third"), "third");
        cm.tick();
        QCOMPARE(cm.state(), ChannelManager::State::Deferred);
    }

    void dcdCanBeIgnored()
    {
        FakeModem modem;
        AppConfig config = quietConfig();
        config.channel.useDcd = false;
        ChannelManager cm(config, modem.sender(), modem.txQueue());
        qint64 clock = 1000;
        cm.setClock([&clock] { return clock; });
        cm.setOnline(true);

        cm.setDcd(true);
        cm.enqueue(frameFrom("F1ABC-7", "hello"), "hello");
        cm.tick();
        QCOMPARE(modem.sent.size(), 1);
    }

    void pttCountsAsTransmitting()
    {
        FakeModem modem;
        ChannelManager cm(quietConfig(), modem.sender(), modem.txQueue());
        qint64 clock = 1000;
        cm.setClock([&clock] { return clock; });
        cm.setOnline(true);

        cm.setPtt(true);
        cm.tick();
        QCOMPARE(cm.state(), ChannelManager::State::Transmitting);
        cm.enqueue(frameFrom("F1ABC-7", "hello"), "hello");
        cm.tick();
        QCOMPARE(cm.state(), ChannelManager::State::Deferred);
        QCOMPARE(cm.busyReason(), QStringLiteral("transmitting"));
        cm.setPtt(false);
        clock += 700;                    // inter-frame gap after PTT release
        cm.tick();
        QCOMPARE(modem.sent.size(), 1);
    }

    void jitterBoundsTheRelease()
    {
        FakeModem modem;
        AppConfig config = quietConfig();
        config.channel.randomJitterMs = 400;
        ChannelManager cm(config, modem.sender(), modem.txQueue());
        qint64 clock = 1000;
        cm.setClock([&clock] { return clock; });
        cm.setOnline(true);

        cm.setDcd(true);
        cm.enqueue(frameFrom("F1ABC-7", "hello"), "hello");
        cm.tick();
        cm.setDcd(false);
        clock += 1500;                   // hold-off over, jitter starts here
        cm.tick();
        QVERIFY(modem.sent.size() <= 1);
        clock += 401;
        cm.tick();
        QCOMPARE(modem.sent.size(), 1);
    }

    void staleItemsAreAbandoned()
    {
        FakeModem modem;
        AppConfig config = quietConfig();
        config.channel.maxDeferSeconds = 10;
        ChannelManager cm(config, modem.sender(), modem.txQueue());
        qint64 clock = 1000;
        cm.setClock([&clock] { return clock; });
        QSignalSpy dropped(&cm, &ChannelManager::itemDropped);
        cm.setOnline(true);

        cm.setDcd(true);
        cm.enqueue(frameFrom("F1ABC-7", "hello"), "hello");
        clock += 9000;
        cm.tick();
        QCOMPARE(dropped.count(), 0);
        clock += 2000;
        cm.tick();
        QCOMPARE(dropped.count(), 1);
        QVERIFY(dropped.at(0).at(1).toString().contains(QStringLiteral("10s")));
        QCOMPARE(cm.pending(), 0);
        QCOMPARE(modem.sent.size(), 0);
    }

    void queueOperations()
    {
        FakeModem modem;
        ChannelManager cm(quietConfig(), modem.sender(), modem.txQueue());
        qint64 clock = 1000;
        cm.setClock([&clock] { return clock; });
        QSignalSpy changed(&cm, &ChannelManager::queueChanged);
        QSignalSpy dropped(&cm, &ChannelManager::itemDropped);

        cm.enqueueMany({frameFrom("F1ABC-7", "a"), frameFrom("F1ABC-7", "b")}, {"a", "b"}, "chat");
        cm.enqueue(frameFrom("F1ABC-7", "beacon"), "beacon", "beacon");
        cm.enqueue(frameFrom("F1ABC-7", "pos"), "pos", "position");
        QCOMPARE(cm.pending(), 4);
        QCOMPARE(cm.pendingItems().at(1).describe(), QStringLiteral("b  [2/2]"));

        QCOMPARE(cm.dropPending("beacon"), 1);
        QCOMPARE(cm.dropPending("beacon"), 0);
        QCOMPARE(cm.pending(), 3);
        QCOMPARE(dropped.count(), 0);              // silent

        QCOMPARE(cm.clearQueue(), 3);
        QCOMPARE(dropped.count(), 3);
        QCOMPARE(dropped.at(0).at(1).toString(), QStringLiteral("cancelled by operator"));
        QCOMPARE(cm.pending(), 0);
        QCOMPARE(changed.last().at(0).toInt(), 0);
    }

    void refusedFrameStaysQueued()
    {
        FakeModem modem;
        modem.accept = false;
        ChannelManager cm(quietConfig(), modem.sender(), modem.txQueue());
        qint64 clock = 1000;
        cm.setClock([&clock] { return clock; });
        QSignalSpy logged(&cm, &ChannelManager::logMessage);
        cm.setOnline(true);

        cm.enqueue(frameFrom("F1ABC-7", "hello"), "hello");
        cm.tick();
        QCOMPARE(cm.pending(), 1);
        QCOMPARE(cm.state(), ChannelManager::State::Deferred);
        QCOMPARE(logged.count(), 1);

        modem.accept = true;
        clock += 100;
        cm.tick();
        QCOMPARE(cm.pending(), 0);
        QCOMPARE(modem.sent.size(), 1);
    }

    void goingOfflineKeepsTheQueue()
    {
        FakeModem modem;
        ChannelManager cm(quietConfig(), modem.sender(), modem.txQueue());
        qint64 clock = 1000;
        cm.setClock([&clock] { return clock; });
        cm.setOnline(true);
        cm.setDcd(true);
        cm.enqueue(frameFrom("F1ABC-7", "hello"), "hello");
        cm.tick();
        cm.setOnline(false);
        QCOMPARE(cm.state(), ChannelManager::State::Offline);
        QCOMPARE(cm.pending(), 1);
        // Back online: the carrier state is forgotten with the modem, the
        // quiet period after the last activity still applies.
        cm.setOnline(true);
        cm.tick();
        QCOMPARE(cm.state(), ChannelManager::State::Deferred);
        QVERIFY(cm.busyReason().startsWith(QStringLiteral("another station heard")));
        clock += 1500;
        cm.tick();
        QCOMPARE(modem.sent.size(), 1);
    }

    void stateLabels()
    {
        QCOMPARE(ChannelManager::stateLabel(ChannelManager::State::Deferred), QStringLiteral("Waiting for clear channel"));
        QCOMPARE(ChannelManager::stateColourKey(ChannelManager::State::Deferred), QStringLiteral("busy"));
        QCOMPARE(ChannelManager::stateColourKey(ChannelManager::State::Offline), QStringLiteral("offline"));
    }
};

QTEST_GUILESS_MAIN(ModelTest)
#include "tst_model.moc"
