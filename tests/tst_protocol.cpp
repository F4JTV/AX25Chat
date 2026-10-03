/*
 * tst_protocol.cpp - tests for AX25Frame and AprsDecoder.
 *
 * Expected values come from two oracles: Direwolf's own ax25_pad.c (the
 * frames it builds from monitor text must be octet-for-octet what
 * AX25Frame::encode() builds) and the output of Direwolf's decode_aprs tool
 * and of the Python decoder this code replaces, run on the same packets.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtTest>

#include "AX25Frame.h"
#include "AprsDecoder.h"

#include "DirewolfHeaders.h"

namespace {

// What Direwolf builds for a monitor line, without FCS.
QByteArray direwolfFrame(const char *monitor)
{
    QByteArray text(monitor);
    packet_t pp = dw::fromText(text.data(), 1);
    if (!pp) return {};
    unsigned char buf[AX25_MAX_PACKET_LEN];
    const int len = ax25_pack(pp, buf);
    dw::del(pp);
    return QByteArray(reinterpret_cast<const char *>(buf), len);
}

// Not "near": windows.h, reached through direwolf.h, defines near and far
// as (empty) macros, the 16-bit keywords of old.
bool closeTo(std::optional<double> value, double expected, double tolerance)
{
    return value && std::abs(*value - expected) <= tolerance;
}

} // namespace


class ProtocolTest : public QObject
{
    Q_OBJECT

private slots:
    void aprsDecodeInternetAddresses()
    {
        // Sources that are not AX.25 addresses (a DMR hotspot's -D, a two
        // digit SSID) must decode all the same, quietly.
        const QByteArray info = "!4340.00N/00715.00E-Hotspot";
        for (const QString &source : {QStringLiteral("F4ABC-D"), QStringLiteral("IW1CGW-23"), QStringLiteral("IR1ZWA-S")}) {
            const AprsPacket packet = AprsDecoder::decode(info, QStringLiteral("APRS"), source);
            QCOMPARE(packet.kind, AprsPacket::Kind::Position);
            QVERIFY(packet.hasPosition());
            QVERIFY(packet.errors.isEmpty());
        }
    }

    // ---- AX.25 ---------------------------------------------------------------

    void fcs()
    {
        // CRC-16/X-25 check value for "123456789".
        QCOMPARE(ax25::fcsCalc(QByteArray("123456789")), quint16(0x906E));
        QVERIFY(ax25::fcsCheck(ax25::fcsAppend(QByteArray("hello"))));
        QByteArray damaged = ax25::fcsAppend(QByteArray("hello"));
        damaged[1] = 'a';
        QVERIFY(!ax25::fcsCheck(damaged));
    }

    void callsigns()
    {
        QString call; int ssid = -1; QString error;
        QVERIFY(ax25::splitCallsign(" f1abc-7 ", call, ssid, &error));
        QCOMPARE(call, QStringLiteral("F1ABC"));
        QCOMPARE(ssid, 7);
        QVERIFY(ax25::splitCallsign("CHAT", call, ssid, &error));
        QCOMPARE(ssid, 0);
        QVERIFY(!ax25::splitCallsign("TOOLONGCALL", call, ssid, &error));
        QVERIFY(!ax25::splitCallsign("F1ABC-16", call, ssid, &error));
        QVERIFY(!ax25::splitCallsign("F1ABC-x", call, ssid, &error));
        QVERIFY(!ax25::splitCallsign("", call, ssid, &error));
        QCOMPARE(ax25::formatCallsign("F1ABC", 0), QStringLiteral("F1ABC"));
        QCOMPARE(ax25::formatCallsign("F1ABC", 7), QStringLiteral("F1ABC-7"));
    }

    void addressRoundTrip()
    {
        auto addr = AX25Address::parse("wide1-1*");
        QVERIFY(addr);
        QCOMPARE(addr->call, QStringLiteral("WIDE1"));
        QCOMPARE(addr->ssid, 1);
        QVERIFY(addr->hBit);
        QCOMPARE(addr->display(), QStringLiteral("WIDE1-1*"));

        const QByteArray raw = addr->encode(true);
        QCOMPARE(raw.size(), 7);
        bool last = false;
        auto back = AX25Address::decode(raw, &last);
        QVERIFY(back);
        QVERIFY(last);
        QVERIFY(back->sameStation(*addr));
        QVERIFY(back->hBit);
        // reserved bits are 1 1
        QCOMPARE(static_cast<unsigned char>(raw.at(6)) & 0x60, 0x60);
    }

    void uiFrameMatchesDirewolf_data()
    {
        QTest::addColumn<QString>("source");
        QTest::addColumn<QString>("destination");
        QTest::addColumn<QStringList>("via");
        QTest::addColumn<QByteArray>("info");
        QTest::addColumn<QByteArray>("monitor");

        QTest::newRow("direct") << "F1ABC-7" << "CHAT" << QStringList{} << QByteArray("Hello")
                                << QByteArray("F1ABC-7>CHAT:Hello");
        QTest::newRow("one digi") << "F1ABC-7" << "CHAT" << QStringList{"WIDE1-1"} << QByteArray("Hello")
                                  << QByteArray("F1ABC-7>CHAT,WIDE1-1:Hello");
        QTest::newRow("two digis") << "F1XYZ" << "APZ25C" << QStringList{"WIDE1-1", "WIDE2-1"}
                                   << QByteArray("=4350.00N/00710.00E-test")
                                   << QByteArray("F1XYZ>APZ25C,WIDE1-1,WIDE2-1:=4350.00N/00710.00E-test");
    }

    void uiFrameMatchesDirewolf()
    {
        QFETCH(QString, source);
        QFETCH(QString, destination);
        QFETCH(QStringList, via);
        QFETCH(QByteArray, info);
        QFETCH(QByteArray, monitor);

        QString error;
        auto frame = AX25Frame::ui(source, destination, info, via, false, &error);
        QVERIFY2(frame, qPrintable(error));

        const QByteArray ours = frame->encode();
        QByteArray theirs = direwolfFrame(monitor.constData());
        QVERIFY(!theirs.isEmpty());
        QCOMPARE(ours.size(), theirs.size());

        // Direwolf's ax25_from_text() sets the C bit in both the destination
        // and the source ("cc=11", the pre-2.0 form); this application keeps
        // the AX.25 v2.2 command form, C=1 in the destination and C=0 in the
        // source, as the Python version did.  Receivers ignore the C bits of
        // UI frames.  Everything else must match octet for octet.
        QCOMPARE(static_cast<unsigned char>(ours.at(6)) & 0x80, 0x80);      // destination C=1
        QCOMPARE(static_cast<unsigned char>(ours.at(13)) & 0x80, 0x00);     // source C=0
        theirs[13] = static_cast<char>(static_cast<unsigned char>(theirs.at(13)) & ~0x80);
        QCOMPARE(ours.toHex(), theirs.toHex());
    }

    void decodeDirewolfFrame()
    {
        const QByteArray raw = direwolfFrame("F1ABC-7>CHAT,WIDE1-1*,WIDE2-1:Hello there");
        QString error;
        auto frame = AX25Frame::decode(raw, &error);
        QVERIFY2(frame, qPrintable(error));
        QCOMPARE(frame->source.toString(), QStringLiteral("F1ABC-7"));
        QCOMPARE(frame->destination.toString(), QStringLiteral("CHAT"));
        QCOMPARE(frame->digipeaters.size(), 2);
        QVERIFY(frame->digipeaters.at(0).hBit);
        QVERIFY(!frame->digipeaters.at(1).hBit);
        QVERIFY(frame->isUi());
        QVERIFY(frame->pid.has_value());
        QCOMPARE(*frame->pid, quint8(0xF0));
        QCOMPARE(frame->info, QByteArray("Hello there"));
        QCOMPARE(frame->path(), QStringLiteral("CHAT,WIDE1-1*,WIDE2-1"));
        QCOMPARE(frame->toTnc2(), QStringLiteral("F1ABC-7>CHAT,WIDE1-1*,WIDE2-1:Hello there"));
        QCOMPARE(frame->frameType(), QStringLiteral("UI"));

        // Our own frames round-trip exactly, command bits included.
        auto ours = AX25Frame::ui("F1ABC-7", "CHAT", "Hello there", {"WIDE1-1", "WIDE2-1"});
        QVERIFY(ours);
        auto back = AX25Frame::decode(ours->encode(), &error);
        QVERIFY2(back, qPrintable(error));
        QVERIFY(back->command);
        QCOMPARE(back->encode().toHex(), ours->encode().toHex());
    }

    void decodeRejectsGarbage()
    {
        QString error;
        QVERIFY(!AX25Frame::decode(QByteArray("short"), &error));
        QVERIFY(!error.isEmpty());
        // Twelve address slots and never a terminator.
        QByteArray endless(12 * 7 + 1, '\x40');
        QVERIFY(!AX25Frame::decode(endless, &error));
        // Three valid addresses, control missing.
        QByteArray noControl = AX25Address("CHAT", 0).encode(false) + AX25Address("F1ABC", 0).encode(false)
                             + AX25Address("WIDE1", 1).encode(true);
        QVERIFY(!AX25Frame::decode(noControl, &error));
        QCOMPARE(error, QStringLiteral("Missing control field"));
        // Two addresses and a control octet but no PID on a UI frame.
        QByteArray noPid = AX25Address("CHAT", 0).encode(false) + AX25Address("F1ABC", 0).encode(true) + '\x03';
        QVERIFY(!AX25Frame::decode(noPid, &error));
        QCOMPARE(error, QStringLiteral("Missing PID field"));
    }

    void frameTypes()
    {
        AX25Frame f;
        f.control = 0x03; QCOMPARE(f.frameType(), QStringLiteral("UI"));
        f.control = 0x13; QCOMPARE(f.frameType(), QStringLiteral("UI"));
        f.control = 0x3F; QCOMPARE(f.frameType(), QStringLiteral("SABM"));
        f.control = 0x53; QCOMPARE(f.frameType(), QStringLiteral("DISC"));
        f.control = 0x01; QCOMPARE(f.frameType(), QStringLiteral("RR nr=0"));
        f.control = 0x45; QCOMPARE(f.frameType(), QStringLiteral("RNR nr=2"));
        f.control = 0x00; QCOMPARE(f.frameType(), QStringLiteral("I ns=0 nr=0"));
        f.control = 0xA6; QCOMPARE(f.frameType(), QStringLiteral("I ns=3 nr=5"));
        f.control = 0xCB; QCOMPARE(f.frameType(), QStringLiteral("U 0xCB"));
    }

    void infoText()
    {
        AX25Frame f;
        f.info = QByteArray("Line one\r\nLine two\rtab\x01here ");
        QCOMPARE(f.text(), QStringLiteral("Line one\nLine two\ntab<01>here"));
        f.info = QByteArray("caf\xC3\xA9");
        QCOMPARE(f.text(), QStringLiteral("caf\u00E9"));
        QCOMPARE(f.text("ascii"), QStringLiteral("caf\uFFFD\uFFFD"));
    }

    void splitMessage()
    {
        QVERIFY(ax25::splitMessage("   ").isEmpty());
        QCOMPARE(ax25::splitMessage("short").size(), 1);

        // Long ASCII text: every chunk within the limit, cut on spaces,
        // nothing lost.
        QString words;
        for (int i = 0; i < 90; i++) words += QStringLiteral("word%1 ").arg(i);
        words = words.trimmed();
        const auto chunks = ax25::splitMessage(words, 100);
        QVERIFY(chunks.size() > 1);
        QString joined;
        for (const QByteArray &c : chunks) {
            QVERIFY(c.size() <= 100);
            QVERIFY(!c.startsWith(' ') && !c.endsWith(' '));
            joined += QString::fromUtf8(c) + ' ';
        }
        QCOMPARE(joined.trimmed(), words);

        // Multi-octet characters are never split in two.
        const QString accents(150, QChar(0x00E9));   // 300 octets in UTF-8
        const auto pieces = ax25::splitMessage(accents, 200);
        QCOMPARE(pieces.size(), 2);
        QCOMPARE(pieces.at(0).size(), 200);
        QCOMPARE(pieces.at(1).size(), 100);
        QCOMPARE(QString::fromUtf8(pieces.at(0) + pieces.at(1)), accents);

        // ASCII encoding replaces what it cannot carry.
        QCOMPARE(ax25::encodeText(QStringLiteral("caf\u00E9"), "ascii"), QByteArray("caf?"));
    }

    // ---- APRS ---------------------------------------------------------------

    void positionUncompressed()
    {
        const auto p = AprsDecoder::decode("!4903.50N/07201.75W-Test 001234", "APRS", "W1ABC");
        QCOMPARE(p.kind, AprsPacket::Kind::Position);
        QCOMPARE(p.description, QStringLiteral("Position"));
        QVERIFY(closeTo(p.latitude, 49.058333, 1e-5));
        QVERIFY(closeTo(p.longitude, -72.029167, 1e-5));
        QCOMPARE(p.symbol(), QStringLiteral("/-"));
        QCOMPARE(p.comment, QStringLiteral("Test 001234"));
        QVERIFY(!p.courseDeg && !p.speedKnots && !p.altitudeM);
        QVERIFY(p.errors.isEmpty());
    }

    void positionWithExtensions()
    {
        const auto p = AprsDecoder::decode("!4903.50N/07201.75W>088/036/A=001234 with altitude", "APRS", "W1ABC");
        QCOMPARE(p.kind, AprsPacket::Kind::Position);
        QCOMPARE(p.symbol(), QStringLiteral("/>"));
        QVERIFY(closeTo(p.courseDeg, 88.0, 0.01));
        QVERIFY(closeTo(p.speedKnots, 36.0, 0.1));
        QVERIFY(closeTo(p.altitudeM, 376.12, 0.1));
        QCOMPARE(p.comment, QStringLiteral("with altitude"));
    }

    void positionCompressed()
    {
        const auto p = AprsDecoder::decode("!/5L!!<*e7>7P[", "APRS", "W1ABC");
        QCOMPARE(p.kind, AprsPacket::Kind::Position);
        QVERIFY(closeTo(p.latitude, 49.5, 1e-4));
        QVERIFY(closeTo(p.longitude, -72.75, 1e-4));
        QCOMPARE(p.symbol(), QStringLiteral("/>"));
        QVERIFY(closeTo(p.courseDeg, 88.0, 0.01));
        QVERIFY(closeTo(p.speedKnots, 36.3, 0.5));
    }

    void positionWithTimestamp()
    {
        const auto p = AprsDecoder::decode("@092345z4903.50N/07201.75W>088/036 timestamped", "APRS", "W1ABC");
        QCOMPARE(p.kind, AprsPacket::Kind::Position);
        QCOMPARE(p.description, QStringLiteral("Position with timestamp, messaging"));
        QCOMPARE(p.timestamp, QStringLiteral("092345z"));
        QVERIFY(closeTo(p.latitude, 49.058333, 1e-5));
        QVERIFY(closeTo(p.courseDeg, 88.0, 0.01));
        QCOMPARE(p.comment, QStringLiteral("timestamped"));
    }

    void micE()
    {
        // Latitude lives in the destination address.
        const auto p = AprsDecoder::decode("`c_Vm6hk/`\"49}Jeff", "T2SP0W", "N1ZZN-9");
        QCOMPARE(p.kind, AprsPacket::Kind::Position);
        QCOMPARE(p.description, QStringLiteral("Mic-E"));
        QVERIFY(closeTo(p.latitude, 42.501167, 1e-5));
        QVERIFY(closeTo(p.longitude, -71.126333, 1e-5));
        QCOMPARE(p.symbol(), QStringLiteral("/k"));
        QVERIFY(closeTo(p.courseDeg, 276.0, 0.01));
        QVERIFY(closeTo(p.speedKnots, 12.0, 0.3));
        QCOMPARE(p.status, QStringLiteral("In Service"));
        QCOMPARE(p.comment, QStringLiteral("`\"49}Jeff"));
    }

    void weatherPositionless()
    {
        const auto p = AprsDecoder::decode("_10090556c220s004g005t077r000p000P000h50b09900wRSW", "APRS", "F1WX-1");
        QCOMPARE(p.kind, AprsPacket::Kind::Weather);
        QCOMPARE(p.timestamp, QStringLiteral("10090556"));
        QVERIFY(p.weather);
        QCOMPARE(*p.weather->windDirectionDeg, 220);
        QVERIFY(closeTo(p.weather->windSpeedKmh, 6.437, 0.01));
        QVERIFY(closeTo(p.weather->windGustKmh, 8.047, 0.01));
        QVERIFY(closeTo(p.weather->temperatureC, 25.0, 0.01));
        QVERIFY(closeTo(p.weather->rainHourMm, 0.0, 0.001));
        QCOMPARE(*p.weather->humidityPct, 50);
        QVERIFY(closeTo(p.weather->pressureHpa, 990.0, 0.01));
        QVERIFY(!p.weather->luminosityWm2);
        QCOMPARE(p.comment, QStringLiteral("wRSW"));
        QVERIFY(!p.hasPosition());
    }

    void weatherStationPosition()
    {
        // The leading 180/012 is wind, not course and speed.
        const auto p = AprsDecoder::decode("!4340.00N/00522.00E_180/012g020t041r005p010P015h72b10132Station meteo", "APRS", "F1WX-2");
        QCOMPARE(p.kind, AprsPacket::Kind::Position);
        QVERIFY(closeTo(p.latitude, 43.666667, 1e-5));
        QVERIFY(closeTo(p.longitude, 5.366667, 1e-5));
        QCOMPARE(p.symbol(), QStringLiteral("/_"));
        QVERIFY(!p.courseDeg && !p.speedKnots);
        QVERIFY(p.weather);
        QCOMPARE(*p.weather->windDirectionDeg, 180);
        QVERIFY(closeTo(p.weather->windSpeedKmh, 19.31, 0.01));
        QVERIFY(closeTo(p.weather->windGustKmh, 32.19, 0.01));
        QVERIFY(closeTo(p.weather->temperatureC, 5.0, 0.01));
        QVERIFY(closeTo(p.weather->rainHourMm, 1.27, 0.01));
        QVERIFY(closeTo(p.weather->rain24hMm, 2.54, 0.01));
        QVERIFY(closeTo(p.weather->rainMidnightMm, 3.81, 0.01));
        QCOMPARE(*p.weather->humidityPct, 72);
        QVERIFY(closeTo(p.weather->pressureHpa, 1013.2, 0.01));
        QCOMPARE(p.comment, QStringLiteral("Station meteo"));
        QVERIFY(p.weather->describe().contains(QStringLiteral("5.0 C")));
        QVERIFY(p.weather->describe().contains(QStringLiteral("from 180 S")));
    }

    void weatherMissingSensors()
    {
        // Dots mean "no such sensor", never zero.  Humidity 00 is 100 %.
        const auto p = AprsDecoder::decode("_10090556c...s...g...t077r000p...P...h00b.....", "APRS", "F1WX-1");
        QVERIFY(p.weather);
        QVERIFY(!p.weather->windDirectionDeg);
        QVERIFY(!p.weather->windSpeedKmh);
        QVERIFY(closeTo(p.weather->temperatureC, 25.0, 0.01));
        QCOMPARE(*p.weather->humidityPct, 100);
        QVERIFY(!p.weather->pressureHpa);
    }

    void objectReport()
    {
        const auto p = AprsDecoder::decode(";LEADER   *092345z4903.50N/07201.75W>088/036", "APRS", "W1ABC");
        QCOMPARE(p.kind, AprsPacket::Kind::Object);
        QCOMPARE(p.name, QStringLiteral("LEADER"));
        QVERIFY(p.alive);
        QCOMPARE(p.timestamp, QStringLiteral("092345z"));
        QVERIFY(closeTo(p.latitude, 49.058333, 1e-5));
        QVERIFY(closeTo(p.courseDeg, 88.0, 0.01));
        QVERIFY(closeTo(p.speedKnots, 36.0, 0.1));

        const auto killed = AprsDecoder::decode(";LEADER   _092345z4903.50N/07201.75W>088/036", "APRS", "W1ABC");
        QVERIFY(!killed.alive);
    }

    void itemReport()
    {
        const auto p = AprsDecoder::decode(")AID #2!4903.50N/07201.75W!", "APRS", "W1ABC");
        QCOMPARE(p.kind, AprsPacket::Kind::Item);
        QCOMPARE(p.name, QStringLiteral("AID #2"));
        QVERIFY(p.alive);
        QVERIFY(closeTo(p.latitude, 49.058333, 1e-5));
        QCOMPARE(p.symbol(), QStringLiteral("/!"));
    }

    void messages()
    {
        auto p = AprsDecoder::decode(":WA1XYX-15:Howdy y'all{12}34", "APRS", "W1ABC");
        QCOMPARE(p.kind, AprsPacket::Kind::Message);
        QCOMPARE(p.messageKind, AprsPacket::MessageKind::Message);
        QCOMPARE(p.addressee, QStringLiteral("WA1XYX-15"));
        QCOMPARE(p.text, QStringLiteral("Howdy y'all"));
        QCOMPARE(p.messageId, QStringLiteral("12"));
        QCOMPARE(p.ackOf, QStringLiteral("34"));
        QVERIFY(p.replyAckCapable);

        p = AprsDecoder::decode(":WA1XYX-15:Howdy{12}", "APRS", "W1ABC");
        QCOMPARE(p.messageId, QStringLiteral("12"));
        QVERIFY(p.replyAckCapable);
        QVERIFY(p.ackOf.isEmpty());

        p = AprsDecoder::decode(":WA1XYX-15:Hello{ab1", "APRS", "W1ABC");
        QCOMPARE(p.messageId, QStringLiteral("ab1"));
        QVERIFY(!p.replyAckCapable);
        QCOMPARE(p.text, QStringLiteral("Hello"));

        p = AprsDecoder::decode(":WA1XYX-15:ack12}", "APRS", "W1ABC");
        QCOMPARE(p.messageKind, AprsPacket::MessageKind::Ack);
        QCOMPARE(p.messageId, QStringLiteral("12"));
        QVERIFY(p.text.isEmpty());

        p = AprsDecoder::decode(":WA1XYX-15:rej7", "APRS", "W1ABC");
        QCOMPARE(p.messageKind, AprsPacket::MessageKind::Rej);

        p = AprsDecoder::decode(":BLN1     :Club meeting tonight", "APRS", "W1ABC");
        QCOMPARE(p.messageKind, AprsPacket::MessageKind::Bulletin);
        QCOMPARE(p.addressee, QStringLiteral("BLN1"));
        QCOMPARE(p.text, QStringLiteral("Club meeting tonight"));

        p = AprsDecoder::decode(":bad", "APRS", "W1ABC");
        QCOMPARE(p.messageKind, AprsPacket::MessageKind::Invalid);
        QVERIFY(!p.errors.isEmpty());
    }

    void statusAndOthers()
    {
        auto p = AprsDecoder::decode(">Net control station", "APRS", "W1ABC");
        QCOMPARE(p.kind, AprsPacket::Kind::Status);
        QCOMPARE(p.status, QStringLiteral("Net control station"));

        p = AprsDecoder::decode("T#005,199,000,255,073,123,01101001", "APRS", "W1ABC");
        QCOMPARE(p.kind, AprsPacket::Kind::Other);
        QCOMPARE(p.description, QStringLiteral("Telemetry"));

        p = AprsDecoder::decode("", "APRS", "W1ABC");
        QCOMPARE(p.kind, AprsPacket::Kind::Unknown);

        p = AprsDecoder::decode("%garbage", "APRS", "W1ABC");
        QCOMPARE(p.kind, AprsPacket::Kind::Unknown);
        QCOMPARE(p.comment, QStringLiteral("garbage"));

        // A malformed position must not stop the receive path.
        p = AprsDecoder::decode("!49XX.50N/07201.75W-", "APRS", "W1ABC");
        QCOMPARE(p.kind, AprsPacket::Kind::Position);
        QVERIFY(!p.errors.isEmpty());
    }

    void decodeFromFrame()
    {
        auto frame = AX25Frame::ui("N1ZZN-9", "T2SP0W", "`c_Vm6hk/`\"49}Jeff");
        QVERIFY(frame);
        const auto p = AprsDecoder::decode(*frame);
        QCOMPARE(p.kind, AprsPacket::Kind::Position);
        QVERIFY(closeTo(p.latitude, 42.501167, 1e-5));

        AX25Frame notAprs = *frame;
        notAprs.control = 0x3F;   // SABM
        QCOMPARE(AprsDecoder::decode(notAprs).kind, AprsPacket::Kind::Unknown);
    }

    void geometry()
    {
        // Marseille to Nice: about 158 km, bearing east-north-east.
        const double d = AprsDecoder::distanceKm(43.2965, 5.3698, 43.7102, 7.2620);
        QVERIFY(d > 157.0 && d < 160.0);
        const double b = AprsDecoder::bearingDeg(43.2965, 5.3698, 43.7102, 7.2620);
        QVERIFY(b > 70.0 && b < 76.0);
        QCOMPARE(AprsDecoder::compassPoint(b), QStringLiteral("ENE"));
        QCOMPARE(AprsDecoder::compassPoint(0), QStringLiteral("N"));
        QCOMPARE(AprsDecoder::compassPoint(359), QStringLiteral("N"));
        QCOMPARE(AprsDecoder::compassPoint(180), QStringLiteral("S"));
    }
};

QTEST_GUILESS_MAIN(ProtocolTest)
#include "tst_protocol.moc"
