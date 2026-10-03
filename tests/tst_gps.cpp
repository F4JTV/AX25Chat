/*
 * tst_gps.cpp - tests for the NMEA parser and the GPS receiver's line
 * handling.  Sentences are real-world shapes: GP and GN talkers, with and
 * without checksum, moving and stationary, fix lost.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtTest>
#include <QSignalSpy>

#include "GpsReceiver.h"

namespace {

QString withChecksum(const QString &body)
{
    int sum = 0;
    for (const QChar c : body.mid(1)) sum ^= c.unicode();
    return body + '*' + QString::number(sum, 16).toUpper().rightJustified(2, '0');
}

} // namespace

class GpsTest : public QObject
{
    Q_OBJECT

private slots:

    void checksums()
    {
        QVERIFY(gps::verifyChecksum("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"));
        QVERIFY(!gps::verifyChecksum("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*48"));
        QVERIFY(gps::verifyChecksum("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"));  // none
        QVERIFY(!gps::verifyChecksum("$GPGGA,1*4"));
        QVERIFY(!gps::verifyChecksum("$GPGGA,1*ZZ"));
    }

    void degrees()
    {
        QVERIFY(std::abs(*gps::nmeaDegrees("4807.038", "N") - 48.1173) < 1e-6);
        QVERIFY(std::abs(*gps::nmeaDegrees("01131.000", "E") - 11.516667) < 1e-6);
        QVERIFY(std::abs(*gps::nmeaDegrees("4807.038", "S") + 48.1173) < 1e-6);
        QVERIFY(std::abs(*gps::nmeaDegrees("00522.00", "W") + 5.366667) < 1e-6);
        QVERIFY(!gps::nmeaDegrees("", "N"));
        QVERIFY(!gps::nmeaDegrees("4807.038", ""));
        QVERIFY(!gps::nmeaDegrees("48", "N"));
        QVERIFY(!gps::nmeaDegrees("4x07.038", "N"));
    }

    void ggaAndRmc()
    {
        NmeaParser parser;
        double clock = 100.0;
        parser.setClock([&clock] { return clock; });

        // GGA: position, altitude, satellites, HDOP.
        auto fix = parser.feed("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47");
        QVERIFY(fix);
        QVERIFY(fix->valid);
        QVERIFY(std::abs(fix->latitude - 48.1173) < 1e-6);
        QVERIFY(std::abs(fix->longitude - 11.516667) < 1e-6);
        QCOMPARE(*fix->altitudeM, 545.4);
        QCOMPARE(fix->satellites, 8);
        QCOMPARE(*fix->hdop, 0.9);
        QCOMPARE(fix->fixQuality, 1);
        QCOMPARE(fix->utc, QStringLiteral("123519"));
        QCOMPARE(fix->updatedAt, 100.0);
        QVERIFY(!fix->speedKnots);
        QCOMPARE(parser.sentencesSeen, 1);
        QCOMPARE(parser.talkers(), QStringList{"GP"});

        // RMC from a multi-constellation receiver: speed and course.
        clock = 101.0;
        fix = parser.feed(withChecksum("$GNRMC,123520,A,4807.040,N,01131.010,E,022.4,084.4,230394,003.1,W"));
        QVERIFY(fix && fix->valid);
        QCOMPARE(*fix->speedKnots, 22.4);
        QCOMPARE(*fix->courseDeg, 84.4);
        QCOMPARE(*fix->altitudeM, 545.4);            // kept from GGA
        QCOMPARE(fix->updatedAt, 101.0);
        QCOMPARE(parser.talkers(), QStringList({"GN", "GP"}));

        // Stationary: the course is dropped.
        fix = parser.feed(withChecksum("$GNRMC,123521,A,4807.040,N,01131.010,E,000.3,215.0,230394,003.1,W"));
        QVERIFY(fix && fix->valid);
        QCOMPARE(*fix->speedKnots, 0.3);
        QVERIFY(!fix->courseDeg);

        // Fix lost (RMC status V): reported as invalid, position kept.
        fix = parser.feed(withChecksum("$GNRMC,123522,V,4807.040,N,01131.010,E,,,230394,003.1,W"));
        QVERIFY(fix);
        QVERIFY(!fix->valid);
        QCOMPARE(fix->describe(clock), QStringLiteral("no fix"));

        // GGA quality 0 also means no fix.
        fix = parser.feed(withChecksum("$GPGGA,123523,,,,,0,03,,,M,,M,,"));
        QVERIFY(fix);
        QVERIFY(!fix->valid);
        QCOMPARE(fix->satellites, 3);

        // Other sentences are counted but ignored; garbage is rejected.
        QVERIFY(!parser.feed(withChecksum("$GPGSV,3,1,11,03,03,111,00,04,15,270,00,06,01,010,00,13,06,292,00")));
        QVERIFY(!parser.feed("junk"));
        QVERIFY(!parser.feed("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*00"));
        QCOMPARE(parser.sentencesRejected, 1);
        QCOMPARE(parser.sentencesSeen, 6);
    }

    void fixAgeAndUsability()
    {
        GpsFix fix;
        fix.valid = true;
        fix.latitude = 43.2965;
        fix.longitude = 5.3698;
        fix.satellites = 7;
        fix.hdop = 1.24;
        fix.updatedAt = 100.0;
        QVERIFY(fix.isUsable(120.0));
        QVERIFY(!fix.isUsable(131.0));
        QVERIFY(fix.isUsable(170.0, 90.0));
        QCOMPARE(fix.describe(112.0), QStringLiteral("43.29650N, 5.36980E, 7 sats, HDOP 1.2, 12s ago"));
        fix.valid = false;
        QVERIFY(!fix.isUsable(100.0));
    }

    void receiverSplitsLines()
    {
        GpsReceiver receiver;
        QSignalSpy fixes(&receiver, &GpsReceiver::fixChanged);
        QSignalSpy status(&receiver, &GpsReceiver::statusChanged);

        QCOMPARE(receiver.statusText(), QStringLiteral("GPS: closed"));
        QVERIFY(!receiver.open(""));                         // no port selected
        QVERIFY(!receiver.open("/dev/does-not-exist-ax25chat", 4800));

        // Data arriving in fragments, with a partial line held back.
        receiver.feedText("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n$GNRMC,12");
        QCOMPARE(fixes.count(), 1);
        QCOMPARE(status.last().at(0).toString(), QStringLiteral("fix"));
        receiver.feedText(QString(withChecksum("$GNRMC,123520,A,4807.040,N,01131.010,E,022.4,084.4,230394,003.1,W")
                              .mid(9) + "\r\n").toLatin1());
        QCOMPARE(fixes.count(), 2);
        QCOMPARE(*fixes.last().at(0).value<GpsFix>().speedKnots, 22.4);

        receiver.feedText(QString(withChecksum("$GNRMC,123522,V,,,,,,,230394,,") + "\r\n").toLatin1());
        QCOMPARE(fixes.count(), 3);
        QCOMPARE(receiver.status(), QStringLiteral("nofix"));
    }
};

QTEST_GUILESS_MAIN(GpsTest)
#include "tst_gps.moc"
