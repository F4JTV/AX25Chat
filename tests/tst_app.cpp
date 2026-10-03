/*
 * tst_app.cpp - end-to-end test of the application window over the real
 * modem core.  The modem reads the audio produced by Direwolf's gen_packets
 * from standard input (conf/test-stdin.conf), so this test must be run as
 *
 *   tst_app < test.wav
 *
 * It checks that the window starts the modem, decodes the frames, lists the
 * stations, shows the chat line and can queue a transmission.  Skipped when
 * nothing arrives on stdin within the timeout (no audio piped in).
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtTest>
#include <QApplication>
#include <QLineEdit>
#include <QListWidget>
#include <QTextBrowser>

#include "AppConfig.h"
#include "MainWindow.h"

class AppTest : public QObject
{
    Q_OBJECT

private slots:

    void receivesAndLists()
    {
        const QString conf = QFileInfo(QStringLiteral(TEST_CONF_DIR "/test-stdin.conf")).absoluteFilePath();
        QVERIFY(QFileInfo(conf).isFile());

        AppConfig config;
        config.station.callsign = QStringLiteral("F4TST");
        config.station.ssid = 1;
        config.station.destination = QStringLiteral("CHAT");
        config.modem.configFile = conf;
        config.modem.autoStart = true;
        config.modem.echoLog = false;
        config.notifications.enabled = false;
        config.position.latitude = 43.2965;
        config.position.longitude = 5.3698;

        MainWindow window(config);
        window.show();

        auto *log = window.findChild<QTextBrowser *>();
        QVERIFY(log);
        auto *heard = window.findChild<QListWidget *>();
        QVERIFY(heard);

        // The modem starts after the 300 ms start-up delay, then decodes
        // the three frames of the test audio.
        bool decoded = false;
        for (int i = 0; i < 100 && !decoded; i++) {
            QTest::qWait(100);
            decoded = log->toPlainText().contains(QStringLiteral("Hello from the test generator"));
        }
        if (!decoded && !log->toPlainText().contains(QStringLiteral("Modem started"))) {
            QSKIP("The modem did not start; is the test audio piped on stdin?");
        }
        const QString text = log->toPlainText();
        QVERIFY2(decoded, qPrintable(text));
        // Chat on our destination, and monitor lines for the APRS traffic.
        QVERIFY(text.contains(QStringLiteral("<F1ABC-7>")));
        QVERIFY(text.contains(QStringLiteral("<F1XYZ>")));
        QVERIFY(text.contains(QStringLiteral("second station answering")));
        QVERIFY(text.contains(QStringLiteral("[F1ABC-7]")));              // position monitor line
        QVERIFY(text.contains(QStringLiteral("km")));                      // distance from our reference

        // The side panel lists what was heard, with the position station.
        QTest::qWait(1200);
        QStringList callsigns;
        for (int i = 0; i < heard->count(); i++) callsigns << heard->item(i)->text().section(' ', 0, 0);
        QVERIFY2(callsigns.contains(QStringLiteral("F1XYZ")), qPrintable(callsigns.join(",")));
        QVERIFY2(callsigns.contains(QStringLiteral("F1ABC-7")), qPrintable(callsigns.join(",")));

        // Typing a message queues a frame for the modem.
        QLineEdit *input = nullptr;
        const auto edits = window.findChildren<QLineEdit *>();
        for (QLineEdit *e : edits) if (e->placeholderText().startsWith(QStringLiteral("Type a message"))) input = e;
        QVERIFY(input);
        input->setText(QStringLiteral("hello from the test"));
        QTest::keyClick(input, Qt::Key_Return);
        bool sent = false;
        for (int i = 0; i < 50 && !sent; i++) {
            QTest::qWait(100);
            sent = log->toPlainText().contains(QStringLiteral("<F4TST-1> hello from the test"));
        }
        QVERIFY2(sent, qPrintable(log->toPlainText()));
        window.close();
    }
};

QTEST_MAIN(AppTest)
#include "tst_app.moc"
