/*
 * main.cpp - AX25Chat entry point.
 *
 * Port of run.py.  Command line:
 *
 *   ax25chat [--config PATH] [--call CALL] [--modem-conf PATH] [--no-modem]
 *            [--style NAME] [--list-styles] [--version]
 *
 * The --host / --port / --connect / --direwolf options of the Python version
 * described a KISS link and an external process; the modem is built in now.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "AppConfig.h"
#include "MainWindow.h"
#include "ModemConfigFile.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QStyle>
#include <QStyleFactory>
#include <QTextStream>

#include <csignal>

namespace {

// Select a Qt style by name, case-insensitively.  An unknown name is
// reported and ignored rather than fatal: a saved setting naming a style
// that is missing on this machine must not stop the application.
void applyStyle(QApplication &app, const QString &requestedIn)
{
    const QString requested = requestedIn.trimmed();
    if (requested.isEmpty()) return;
    const QStringList available = QStyleFactory::keys();
    for (const QString &key : available) {
        if (key.compare(requested, Qt::CaseInsensitive) == 0) {
            app.setStyle(key);
            return;
        }
    }
    QTextStream(stderr) << "Unknown Qt style '" << requested << "'. Available here: "
                        << available.join(", ") << ". Using the platform default.\n";
}

// A fresh installation gets a starter direwolf.conf in the settings folder.
// Only when no configuration file exists yet, so it cannot overwrite a
// choice the operator has already made.  Returns the file written, or empty.
QString firstRunSetup(AppConfig &config)
{
    if (QFileInfo::exists(AppConfig::configPath())) return QString();
    if (!config.modem.configFile.isEmpty() || !modemconf::defaultFile().isEmpty()) return QString();
    QString error;
    const QString conf = modemconf::writeStarter(modemconf::userPath(), config.myCall(), &error);
    if (conf.isEmpty()) return QString();
    config.modem.configFile = conf;
    return conf;
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("AX25Chat"));
    app.setApplicationVersion(MainWindow::version());
    const QIcon icon(QStringLiteral(":/icons/ax25chat.png"));
    if (!icon.isNull()) app.setWindowIcon(icon);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("AX.25 packet radio chat with a built-in Dire Wolf modem"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("config"), QStringLiteral("path to config.json"), QStringLiteral("path")});
    parser.addOption({QStringLiteral("call"), QStringLiteral("override the callsign, e.g. MYCALL-7"), QStringLiteral("call")});
    parser.addOption({QStringLiteral("modem-conf"), QStringLiteral("path to direwolf.conf for the built-in modem"), QStringLiteral("path")});
    parser.addOption({QStringLiteral("no-modem"), QStringLiteral("do not start the modem even if configured to")});
    parser.addOption({QStringLiteral("style"), QStringLiteral("Qt style for this run, overriding the saved setting (try 'Fusion')"), QStringLiteral("name")});
    parser.addOption({QStringLiteral("list-styles"), QStringLiteral("print the Qt styles available here and exit")});
    parser.process(app);

    if (parser.isSet(QStringLiteral("list-styles"))) {
        QTextStream out(stdout);
        out << "Available Qt styles: " << QStyleFactory::keys().join(", ") << "\n";
        out << "Current: " << app.style()->objectName() << "\n";
        return 0;
    }

    // The modem's decoders look for data\tocalls.yaml and data\symbols-new.txt
    // relative to the working directory, which for a program started from
    // a shortcut or the shell is anywhere.  Paths given on the command line
    // are resolved first, then the working directory becomes the folder the
    // program was installed in, when the data files are there.
    QString configPath = parser.value(QStringLiteral("config"));
    if (!configPath.isEmpty()) configPath = QFileInfo(configPath).absoluteFilePath();
    QString modemConf = parser.value(QStringLiteral("modem-conf"));
    if (!modemConf.isEmpty()) modemConf = QFileInfo(modemConf).absoluteFilePath();
    const QString appDir = QCoreApplication::applicationDirPath();
    if (QFileInfo::exists(appDir + QStringLiteral("/data/tocalls.yaml"))) {
        QDir::setCurrent(appDir);
    }
#ifdef AX25CHAT_DATA_DIR
    else if (QFileInfo::exists(QStringLiteral(AX25CHAT_DATA_DIR "/data/tocalls.yaml"))) {
        QDir::setCurrent(QStringLiteral(AX25CHAT_DATA_DIR));
    }
#endif

    AppConfig config = AppConfig::load(configPath);
    const QString starter = firstRunSetup(config);

    // No stylesheet is installed, so the platform style is what makes the
    // application follow the desktop theme.  A style is only forced when
    // asked for: --style, then AX25CHAT_STYLE, then ui.qt_style.  This has
    // to happen before any widget exists.
    QString style = parser.value(QStringLiteral("style"));
    if (style.isEmpty()) style = qEnvironmentVariable("AX25CHAT_STYLE");
    if (style.isEmpty()) style = config.ui.qtStyle;
    applyStyle(app, style);

    if (parser.isSet(QStringLiteral("call"))) {
        const QString call = parser.value(QStringLiteral("call")).toUpper();
        const int dash = call.indexOf('-');
        config.station.callsign = dash >= 0 ? call.left(dash) : call;
        config.station.ssid = dash >= 0 ? call.mid(dash + 1).toInt() : 0;
    }
    if (!modemConf.isEmpty()) config.modem.configFile = modemConf;
    if (parser.isSet(QStringLiteral("no-modem"))) config.modem.autoStart = false;

    MainWindow window(config);
    window.show();
    if (!starter.isEmpty()) window.noteFirstRun(starter);

    // Let Ctrl+C in the terminal close the window.
    std::signal(SIGINT, [](int) { QApplication::quit(); });

    return app.exec();
}
