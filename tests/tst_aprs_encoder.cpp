/*
 * tst_aprs_encoder.cpp - tests for AprsEncoder and SmartBeaconer.
 *
 * Expected values were produced by the Python modules this code replaces
 * (aprs.py, smartbeacon.py) and, for the position report, cross-checked by
 * decoding it again with AprsDecoder.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtTest>

#include "AprsDecoder.h"
#include "AprsEncoder.h"
#include "SmartBeacon.h"

class AprsEncoderTest : public QObject
{
    Q_OBJECT

private slots:
    void passcode()
    {
        // The reference values every client agrees on; the SSID is ignored.
        QCOMPARE(aprs::passcode(QStringLiteral("N0CALL")), 13023);
        QCOMPARE(aprs::passcode(QStringLiteral("n0call-9")), 13023);
        QCOMPARE(aprs::passcode(QStringLiteral("W1AW")), 25988);
        QCOMPARE(aprs::passcode(QStringLiteral("F4TST")), 13701);
        QCOMPARE(aprs::passcode(QStringLiteral("F1ABC-7")), 14225);
        QCOMPARE(aprs::passcode(QStringLiteral("")), -1);
        QCOMPARE(aprs::passcode(QStringLiteral("F4/TST")), -1);
    }

    void coordinates()
    {
        QCOMPARE(aprs::formatLatitude(49.058333), QStringLiteral("4903.50N"));
        QCOMPARE(aprs::formatLongitude(-72.029167), QStringLiteral("07201.75W"));
        // Rounding carries into the degrees instead of producing "4960.00".
        QCOMPARE(aprs::formatLatitude(49.999999), QStringLiteral("5000.00N"));
        QCOMPARE(aprs::formatLongitude(-179.999999), QStringLiteral("18000.00W"));
        QCOMPARE(aprs::formatLatitude(-0.5), QStringLiteral("0030.00S"));
        QCOMPARE(aprs::formatLongitude(0.0), QStringLiteral("00000.00E"));
        QString error;
        QVERIFY(aprs::formatLatitude(91.0, &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(aprs::formatLongitude(-181.0).isEmpty());
    }

    void extensions()
    {
        QCOMPARE(aprs::formatCourseSpeed(0, 12.4), QStringLiteral("360/012"));
        QCOMPARE(aprs::formatCourseSpeed(359.6, 1000), QStringLiteral("360/999"));
        QCOMPARE(aprs::formatCourseSpeed(88, 36), QStringLiteral("088/036"));
        QCOMPARE(aprs::formatAltitude(376.12), QStringLiteral("/A=001234"));
        QCOMPARE(aprs::formatAltitude(-5), QStringLiteral("/A=000000"));
    }

    void symbols()
    {
        QVERIFY(aprs::isOverlay("L"));
        QVERIFY(aprs::isOverlay("9"));
        QVERIFY(!aprs::isOverlay("/"));
        QVERIFY(!aprs::isOverlay("a"));
        QString error;
        QVERIFY(aprs::validateSymbol("/", "-", &error));
        QVERIFY(aprs::validateSymbol("\\", "!", &error));
        QVERIFY(aprs::validateSymbol("A", "#", &error));
        QVERIFY(!aprs::validateSymbol("a", "#", &error));
        QVERIFY(!aprs::validateSymbol("/", "", &error));
        QVERIFY(!aprs::validateSymbol("/", " ", &error));
        QVERIFY(!aprs::validateSymbol("//", "-", &error));
    }

    void positionReport()
    {
        PositionReport r;
        r.latitude = 43.2965;
        r.longitude = 5.3698;
        r.symbolTable = "/";
        r.symbolCode = ">";
        r.comment = QStringLiteral("Test | comment~ with\nnewline");
        r.altitudeM = 120.0;
        r.courseDeg = 88.0;
        r.speedKnots = 36.0;
        r.messagingCapable = true;

        QString error;
        const QByteArray info = r.encode(&error);
        QVERIFY2(!info.isEmpty(), qPrintable(error));
        QCOMPARE(info, QByteArray("=4317.79N/00522.19E>088/036/A=000394 Test  comment with newline"));
        QCOMPARE(r.describe(), QStringLiteral("43.29650N  5.36980E  JN23qh  120 m  36 kn / 88deg"));

        // What we send, we can decode.
        const AprsPacket p = AprsDecoder::decode(info, aprs::DefaultTocall, "F1ABC-7");
        QCOMPARE(p.kind, AprsPacket::Kind::Position);
        QVERIFY(std::abs(*p.latitude - 43.2965) < 0.0002);
        QVERIFY(std::abs(*p.longitude - 5.3698) < 0.0002);
        QCOMPARE(p.symbol(), QStringLiteral("/>"));
        QVERIFY(std::abs(*p.courseDeg - 88.0) < 0.01);
        QVERIFY(std::abs(*p.speedKnots - 36.0) < 0.1);
        QVERIFY(std::abs(*p.altitudeM - 120.0) < 0.5);
        QCOMPARE(p.comment, QStringLiteral("Test  comment with newline"));

        PositionReport minimal;
        minimal.latitude = 43.2965;
        minimal.longitude = 5.3698;
        minimal.symbolTable = "L";
        minimal.symbolCode = "#";
        QCOMPARE(minimal.encode(), QByteArray("!4317.79NL00522.19E#"));

        PositionReport bad = minimal;
        bad.symbolTable = "x";
        QVERIFY(bad.encode(&error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void maidenhead()
    {
        double lat = 0, lon = 0;
        QString error;
        QVERIFY(aprs::locatorToLatLon("JN33RU", lat, lon, &error));
        QVERIFY(std::abs(lat - 43.854167) < 1e-5);
        QVERIFY(std::abs(lon - 7.458333) < 1e-5);
        QVERIFY(aprs::locatorToLatLon(" jn33 ", lat, lon, &error));
        QCOMPARE(lat, 43.5);
        QCOMPARE(lon, 7.0);
        QVERIFY(!aprs::locatorToLatLon("JN3", lat, lon, &error));
        QVERIFY(!aprs::locatorToLatLon("ZZ00", lat, lon, &error));
        QVERIFY(!error.isEmpty());

        QCOMPARE(aprs::latLonToLocator(43.2965, 5.3698), QStringLiteral("JN23qh"));
        QCOMPARE(aprs::latLonToLocator(43.2965, 5.3698, 4), QStringLiteral("JN23"));
        QCOMPARE(aprs::latLonToLocator(-33.8688, 151.2093), QStringLiteral("QF56od"));
        QVERIFY(aprs::latLonToLocator(95.0, 0.0).isEmpty());
    }

    void comments()
    {
        const QString sixty(60, 'x');
        QCOMPARE(aprs::sanitiseComment(QStringLiteral("  a|b~c\x01" "d  ") + sixty),
                 QStringLiteral("abcd  ") + QString(37, 'x'));
        QCOMPARE(aprs::sanitiseComment(QStringLiteral("caf\u00E9")), QStringLiteral("caf"));
        QCOMPARE(aprs::distanceKm(43.2965, 5.3698, 43.2965, 5.3698), 0.0);
    }

    void smartBeaconIntervals()
    {
        SmartBeaconer sb;
        QCOMPARE(sb.intervalForSpeed(0), 1800.0);
        QCOMPARE(sb.intervalForSpeed(7.9), 1800.0);
        QCOMPARE(sb.intervalForSpeed(8.0), 1800.0);        // clamped, not 2160
        QCOMPARE(sb.intervalForSpeed(10), 1728.0);
        QCOMPARE(sb.intervalForSpeed(30), 576.0);
        QVERIFY(std::abs(sb.intervalForSpeed(50) - 345.6) < 1e-9);
        QCOMPARE(sb.intervalForSpeed(96), 180.0);
        QCOMPARE(sb.intervalForSpeed(130), 180.0);

        // 28 deg + 260 deg.mph / speed in mph, the Kenwood defaults.
        QCOMPARE(sb.turnThreshold(0.5), 180.0);
        QCOMPARE(sb.turnThreshold(1), 180.0);
        QVERIFY(std::abs(sb.turnThreshold(16.09344) - 54.0) < 1e-9);      // 10 mph: 28 + 26
        QVERIFY(std::abs(sb.turnThreshold(48.28032) - 36.6666666667) < 1e-6);   // 30 mph
        QVERIFY(std::abs(sb.turnThreshold(96.56064) - 32.3333333333) < 1e-6);   // 60 mph
        QCOMPARE(sb.turnThreshold(2), 180.0);                              // clamped at walking pace

        QCOMPARE(SmartBeaconer::courseChange(350, 10), 20.0);
        QCOMPARE(SmartBeaconer::courseChange(10, 200), 170.0);
        QCOMPARE(SmartBeaconer::courseChange(0, 180), 180.0);

        QCOMPARE(sb.preview(), QStringLiteral("0 km/h: 30 min, 5 km/h: 30 min, 10 km/h: 29 min, "
                                              "30 km/h: 10 min, 50 km/h: 6 min, 90 km/h: 3 min, 130 km/h: 3 min"));
    }

    void smartBeaconDecisions()
    {
        SmartBeaconer sb;
        double clock = 1000.0;
        sb.setClock([&clock] { return clock; });
        QString reason;

        // First fix goes out at once.
        QVERIFY(sb.evaluate(std::nullopt, std::nullopt, &reason));
        QCOMPARE(reason, QStringLiteral("first report"));
        QCOMPARE(sb.statusText(std::nullopt), QStringLiteral("SmartBeacon: waiting for the first fix"));
        sb.noteSent(90.0);

        // Parked: nothing for half an hour.
        clock += 1799;
        QVERIFY(!sb.evaluate(0.0, 90.0, &reason));
        QVERIFY(reason.startsWith(QStringLiteral("1 s to go")));
        clock += 1;
        QVERIFY(sb.evaluate(0.0, 90.0, &reason));
        QVERIFY(reason.contains(QStringLiteral("30.0 min since")));
        sb.noteSent(90.0);

        // Motorway speed: every three minutes.  100 kn = 185 km/h.
        clock += 179;
        QVERIFY(!sb.evaluate(100.0, 90.0, &reason));
        clock += 1;
        QVERIFY(sb.evaluate(100.0, 90.0, &reason));
        sb.noteSent(90.0);

        // A turn pre-empts the interval, but not within turnTime.
        clock += 10;
        QVERIFY(!sb.evaluate(30.0, 180.0, &reason));   // 90 deg turn at 56 km/h, too soon
        clock += 5;
        QVERIFY(sb.evaluate(30.0, 180.0, &reason));
        QVERIFY(reason.startsWith(QStringLiteral("turned 90 deg")));
        sb.noteSent(180.0);

        // A small heading wobble at walking pace is not a turn.
        clock += 100;
        QVERIFY(!sb.evaluate(2.0, 200.0, &reason));       // 20 deg at 3.7 km/h, threshold 141
        QCOMPARE(sb.statusText(2.0), QStringLiteral("SmartBeacon: next in 28:20 at 4 km/h"));

        // Parked (no heading) then moving again: reported after the turn
        // time, not after the slow interval.
        sb.noteSent(std::nullopt);
        clock += 10;
        QVERIFY(!sb.evaluate(20.0, 45.0, &reason));
        clock += 5;
        QVERIFY(sb.evaluate(20.0, 45.0, &reason));
        QVERIFY(reason.startsWith(QStringLiteral("started moving")));
    }

    void smartBeaconDerivedSpeed()
    {
        // A receiver that gives no speed: the movement between two fixes
        // one minute apart (about 1.5 km north) is 90 km/h, so the fast
        // interval applies, not the parked one.
        SmartBeaconer sb;
        double clock = 1000.0;
        sb.setClock([&clock] { return clock; });
        QString reason;
        QVERIFY(sb.evaluateFix(43.0, 7.0, std::nullopt, std::nullopt, &reason));   // first report
        sb.noteSent(std::nullopt);
        clock += 60;
        // The derived course (north) counts as started moving, the parked
        // report having had none.
        QVERIFY(sb.evaluateFix(43.0135, 7.0, std::nullopt, std::nullopt, &reason));
        QVERIFY2(reason.startsWith(QStringLiteral("started moving, 90 km/h")), qPrintable(reason));
        sb.noteSent(0.0);
        // Straight on at the same pace: the fast interval, three minutes,
        // has not passed.
        clock += 60;
        QVERIFY(!sb.evaluateFix(43.027, 7.0, std::nullopt, std::nullopt, &reason));
        QVERIFY2(reason.contains(QStringLiteral("at 90 km/h")), qPrintable(reason));

        sb.reset();
        QVERIFY(sb.evaluate(0.0, std::nullopt, &reason));
        QCOMPARE(reason, QStringLiteral("first report"));
    }
};

QTEST_GUILESS_MAIN(AprsEncoderTest)
#include "tst_aprs_encoder.moc"
