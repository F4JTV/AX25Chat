/*
 * tst_aprsis.cpp - the APRS-IS client against a server in this process.
 *
 * A QTcpServer plays a Tier 2 server: banner, login response, a packet,
 * and it checks what the client sends.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "AprsIsClient.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

class TestAprsIs : public QObject
{
    Q_OBJECT

private slots:
    void loginFilterAndTraffic()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));

        AprsIsClient client;
        AprsIsClient::Settings settings;
        settings.host = QStringLiteral("127.0.0.1");
        settings.port = server.serverPort();
        settings.callsign = QStringLiteral("F4TST-7");
        settings.passcode = 12345;
        settings.radiusKm = 100;
        settings.version = QStringLiteral("test");
        client.configure(settings);
        client.setFilterPosition(43.7, 7.25);
        QSignalSpy packets(&client, &AprsIsClient::packetReceived);
        QSignalSpy states(&client, &AprsIsClient::stateChanged);
        client.start();

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *peer = server.nextPendingConnection();
        QVERIFY(peer);
        peer->write("# javAPRSSrvr test\r\n");
        QTRY_VERIFY(peer->canReadLine());
        QString login = QString::fromUtf8(peer->readLine()).trimmed();
        QCOMPARE(login, QStringLiteral("user F4TST-7 pass 12345 vers AX25Chat test filter r/43.700/7.250/100"));

        peer->write("# logresp F4TST-7 verified, server T2TEST\r\n");
        peer->write("F1ABC-9>APRS,WIDE1-1,qAR,F5XYZ:!4340.00N/00715.00E>test\r\n");
        QTRY_COMPARE(packets.count(), 1);
        QCOMPARE(packets.first().first().toString(), QStringLiteral("F1ABC-9>APRS,WIDE1-1,qAR,F5XYZ:!4340.00N/00715.00E>test"));
        QTRY_COMPARE(client.state(), AprsIsClient::State::Connected);
        QVERIFY(client.verified());
        QCOMPARE(client.serverName(), QStringLiteral("T2TEST"));

        QVERIFY(client.sendPacket(QStringLiteral("F1DEF>APRS,qAO,F4TST-7:>hello")));
        QTRY_VERIFY(peer->canReadLine());
        QCOMPARE(QString::fromUtf8(peer->readLine()).trimmed(), QStringLiteral("F1DEF>APRS,qAO,F4TST-7:>hello"));

        // A small move keeps the filter (and the minute between updates has
        // not passed); a new radius re-sends it at once.
        client.setFilterPosition(43.71, 7.26);
        QTest::qWait(300);
        QVERIFY(!peer->canReadLine());
        settings.radiusKm = 200;
        client.configure(settings);
        QTRY_VERIFY(peer->canReadLine());
        QCOMPARE(QString::fromUtf8(peer->readLine()).trimmed(), QStringLiteral("#filter r/43.710/7.260/200"));

        client.stop();
        QCOMPARE(client.state(), AprsIsClient::State::Stopped);
    }

    void unverifiedSendsNothing()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        AprsIsClient client;
        AprsIsClient::Settings settings;
        settings.host = QStringLiteral("127.0.0.1");
        settings.port = server.serverPort();
        settings.callsign = QStringLiteral("F4TST");
        settings.passcode = -1;
        settings.radiusKm = 0;
        client.configure(settings);
        QSignalSpy logs(&client, &AprsIsClient::logMessage);
        client.start();
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *peer = server.nextPendingConnection();
        QTRY_VERIFY(peer->canReadLine());
        QCOMPARE(QString::fromUtf8(peer->readLine()).trimmed(), QStringLiteral("user F4TST pass -1 vers AX25Chat"));
        peer->write("# logresp F4TST unverified, server T2TEST\r\n");
        QTRY_COMPARE(client.state(), AprsIsClient::State::Connected);
        QVERIFY(!client.verified());
        QVERIFY(!client.sendPacket(QStringLiteral("X>Y:z")));
        QTest::qWait(300);
        QVERIFY(!peer->canReadLine());
        client.stop();
    }
};

QTEST_MAIN(TestAprsIs)
#include "tst_aprsis.moc"
