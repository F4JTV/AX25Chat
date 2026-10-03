/*
 * dwengine_demo - smallest Qt program driving DirewolfEngine.
 *
 *   dwengine_demo -c direwolf.conf [-t seconds] [-s "SRC>DST,PATH:text"]
 *
 * Prints every signal the engine emits, optionally queues one UI frame once
 * the core is up, stops after the given time and exits.  Exit status is 1
 * when the core could not start or faulted.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTextStream>
#include <QTimer>

#include "DirewolfEngine.h"

namespace {

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

// Minimal "SRC>DST,DIGI:text" to raw UI frame (PID 0xF0).  The real
// application will have a full AX.25 module; this is only for the demo.
bool encodeCall(const QString &text, QByteArray &out, bool last)
{
    const QStringList parts = text.split('-');
    const QString call = parts.value(0).toUpper();
    const int ssid = parts.size() > 1 ? parts.value(1).toInt() : 0;
    if (call.isEmpty() || call.size() > 6 || ssid < 0 || ssid > 15) return false;
    for (int i = 0; i < 6; i++) {
        const char c = i < call.size() ? call.at(i).toLatin1() : ' ';
        out.append(static_cast<char>(c << 1));
    }
    out.append(static_cast<char>(0x60 | (ssid << 1) | (last ? 1 : 0)));
    return true;
}

QByteArray buildUiFrame(const QString &spec)
{
    const int gt = spec.indexOf('>');
    const int colon = spec.indexOf(':', gt < 0 ? 0 : gt);
    if (gt < 0 || colon < 0) return {};

    const QString src = spec.left(gt);
    const QStringList path = spec.mid(gt + 1, colon - gt - 1).split(',');
    const QByteArray text = spec.mid(colon + 1).toUtf8();
    if (path.isEmpty() || path.size() > 9) return {};

    QByteArray frame;
    if (!encodeCall(path.at(0), frame, false)) return {};
    if (!encodeCall(src, frame, path.size() == 1)) return {};
    for (int i = 1; i < path.size(); i++) {
        if (!encodeCall(path.at(i), frame, i + 1 == path.size())) return {};
    }
    frame.append('\x03');   // UI
    frame.append('\xF0');   // no layer 3
    frame.append(text);
    return frame;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("dwengine_demo");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({"c", "Direwolf configuration file", "file"});
    parser.addOption({"t", "Seconds to run (default 10)", "seconds", "10"});
    parser.addOption({"s", "Queue one UI frame once started, \"SRC>DST,PATH:text\"", "spec"});
    parser.process(app);

    if (!parser.isSet("c")) {
        out() << "a configuration file is required (-c)\n";
        return 2;
    }

    DirewolfEngine engine;
    int exitCode = 0;

    out() << "embedded Dire Wolf " << DirewolfEngine::direwolfVersion() << Qt::endl;

    QObject::connect(&engine, &DirewolfEngine::logMessage, [](DirewolfEngine::LogLevel level, const QString &line) {
        static const char *names[] = {"INFO", "ERROR", "REC", "DECODED", "XMIT", "DEBUG"};
        out() << "[dw " << names[level] << "] " << line << Qt::endl;
    });
    QObject::connect(&engine, &DirewolfEngine::stateChanged, [](DirewolfEngine::State state) {
        out() << "STATE " << static_cast<int>(state) << Qt::endl;
    });
    QObject::connect(&engine, &DirewolfEngine::frameReceived, [](const DwReceivedFrame &f) {
        out() << "FRAME chan=" << f.chan << " level=" << f.audioLevel << " fec=" << f.fec
              << " len=" << f.frame.size() << "  " << DirewolfEngine::formatAddresses(f.frame) << Qt::endl;
    });
    QObject::connect(&engine, &DirewolfEngine::dcdChanged, [](int chan, bool active) {
        out() << "DCD chan=" << chan << (active ? " busy" : " clear") << Qt::endl;
    });
    QObject::connect(&engine, &DirewolfEngine::pttChanged, [](int chan, bool active) {
        out() << "PTT chan=" << chan << (active ? " on" : " off") << Qt::endl;
    });
    QObject::connect(&engine, &DirewolfEngine::faulted, [&](int status, const QString &reason) {
        out() << "FAULT status=" << status << ": " << reason << Qt::endl;
        exitCode = 1;
    });
    QObject::connect(&engine, &DirewolfEngine::startFailed, [&](const QString &reason) {
        out() << "START FAILED: " << reason << Qt::endl;
        exitCode = 1;
        app.quit();
    });
    QObject::connect(&engine, &DirewolfEngine::stopped, [&](bool afterFault) {
        out() << "STOPPED" << (afterFault ? " after fault" : "") << Qt::endl;
        app.quit();
    });

    const int seconds = parser.value("t").toInt();
    const QString spec = parser.value("s");

    QObject::connect(&engine, &DirewolfEngine::started, [&] {
        out() << "STARTED channel 0: " << engine.channelDescription(0)
              << ", MYCALL " << engine.channelMycall(0) << Qt::endl;
        if (!spec.isEmpty()) {
            const QByteArray frame = buildUiFrame(spec);
            if (frame.isEmpty() || !engine.transmit(0, frame)) {
                out() << "could not queue \"" << spec << "\"" << Qt::endl;
            } else {
                out() << "queued " << frame.size() << " octets, TXBUF=" << engine.txQueueBytes(0) << Qt::endl;
            }
        }
        QTimer::singleShot(seconds * 1000, &engine, &DirewolfEngine::stop);
    });

    engine.start(parser.value("c"));
    const int rc = app.exec();
    return rc != 0 ? rc : exitCode;
}
