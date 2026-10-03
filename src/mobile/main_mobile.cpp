/*
 * main_mobile.cpp - AX25Chat for phones and tablets: the Qt Quick interface.
 *
 * Same session, same modem, same settings file as the desktop program; a
 * touch interface on top. Also runs on the desktop, which is how it is
 * tested.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "AndroidNotify.h"
#include "AndroidPtt.h"
#include "AndroidUi.h"
#include "AppConfig.h"
#include "MobileApp.h"
#include "ModemConfigFile.h"
#include "TileCache.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSslSocket>
#include <QQuickWindow>
#include <QTimer>
#include <QImage>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QPermissions>
#endif

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("AX25Chat"));
    app.setApplicationVersion(Session::version());
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/ax25chat.png")));
    QQuickStyle::setStyle(QStringLiteral("Material"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("config"), QStringLiteral("path to config.json"), QStringLiteral("path")});
    parser.addOption({QStringLiteral("call"), QStringLiteral("override the callsign, e.g. MYCALL-7"), QStringLiteral("call")});
    parser.addOption({QStringLiteral("modem-conf"), QStringLiteral("path to direwolf.conf for the built-in modem"), QStringLiteral("path")});
    parser.addOption({QStringLiteral("no-modem"), QStringLiteral("do not start the modem even if configured to")});
    parser.process(app);

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
    else {
        // Nothing installed beside the program (a phone): the bundled copies.
        const QString dataFolder = MobileApp::prepareDataFolder();
        if (!dataFolder.isEmpty()) QDir::setCurrent(dataFolder);
    }

    AppConfig config = AppConfig::load(configPath);

    // A fresh installation: the generated modem configuration, VOX keying,
    // 1200 baud, default audio device.  The Configuration page rewrites it.
    QString starter;
    if (!QFileInfo::exists(AppConfig::configPath())) {
        // A phone's chat is not the place for the modem's start-up notes;
        // Settings -> Modem turns them back on.
        config.modem.echoLog = false;
    }
    if (!QFileInfo::exists(AppConfig::configPath()) && config.modem.configFile.isEmpty()) {
        QString error;
        starter = modemconf::writeGenerated(modemconf::userPath(), config.myCall(), config.modem.speed,
                                            config.modem.ptt, config.modem.gpio, &error);
        if (!starter.isEmpty()) config.modem.configFile = starter;
    }

    if (parser.isSet(QStringLiteral("call"))) {
        const QString call = parser.value(QStringLiteral("call")).toUpper();
        const int dash = call.indexOf('-');
        config.station.callsign = dash >= 0 ? call.left(dash) : call;
        config.station.ssid = dash >= 0 ? call.mid(dash + 1).toInt() : 0;
    }
    if (!modemConf.isEmpty()) config.modem.configFile = modemConf;
    if (parser.isSet(QStringLiteral("no-modem"))) config.modem.autoStart = false;
    // Screenshots show a conversation, not the modem's start-up notes.
    if (qEnvironmentVariableIsSet("AX25CHAT_SCREENSHOT_DIR")) config.modem.echoLog = false;

#ifdef Q_OS_ANDROID
    // The transmitter is keyed from Java (UsbPtt); the core learns about it
    // before the session starts the modem.
    androidptt::install();
    androidnotify::install();
    androidui::installDeviceServices();
    // No inactivity lock while the station is on screen; the service and
    // the stream recovery cover the rest.
    androidui::keepScreenOn(config.ui.keepScreenOn);
    // Full screen, as RemoteRig and FT891Remote: the bars are hidden now and
    // again each time the application comes back to the foreground.
    androidui::hideSystemBars();
#endif

    MobileApp mobile(config);

#ifdef Q_OS_ANDROID
    // Coming back to the foreground: the bars again, and a look at whether
    // location got switched on or off meanwhile.
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &mobile, [&mobile](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive) return;
        androidui::hideSystemBars();
        mobile.checkLocationService();
    });
    if (!QSslSocket::supportsSsl()) {
        mobile.session()->warn(QStringLiteral("TLS is not available in this build: https servers (the map tiles) cannot be reached."));
    }
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // The modem records audio and the position comes from the device; both
    // need the user's consent on a phone.  One request at a time: Android
    // shows one dialog, and a second request made while the first is
    // pending is dropped, which is why the location was never asked for.
    // A location source opened before the consent gets no fixes, so it is
    // reopened once the answer is in.
    // Notifications (Android 13+) last; Qt has no permission class for
    // them, so the Java side asks the activity directly.  Then the
    // foreground service that keeps everything running off screen: it is
    // started only now because its service type depends on the permissions
    // just granted (microphone, location).
    auto askNotifications = [&mobile] {
        androidnotify::requestPermission();
        if (mobile.session()->config().ui.keepRunning) {
            androidui::startKeepAlive();
            QTimer::singleShot(3000, &mobile, [&mobile] { mobile.reportBackgroundState(); });
        }
    };
    auto askLocation = [&app, &mobile, askNotifications] {
        QLocationPermission location;
        location.setAccuracy(QLocationPermission::Precise);
        if (app.checkPermission(location) == Qt::PermissionStatus::Granted) {
            mobile.checkLocationService();
            askNotifications();
            return;
        }
        app.requestPermission(location, &mobile, [&mobile, askNotifications](const QPermission &permission) {
            if (permission.status() != Qt::PermissionStatus::Granted) {
                mobile.session()->warn(QStringLiteral("Location permission refused: no position from this device."));
            } else if (mobile.session()->config().position.source == QLatin1String("gps")) {
                mobile.session()->closeGps();
                mobile.session()->openGps();
                mobile.checkLocationService();
            }
            askNotifications();
        });
    };
    if (app.checkPermission(QMicrophonePermission()) == Qt::PermissionStatus::Granted) {
        askLocation();
    } else {
        app.requestPermission(QMicrophonePermission(), &mobile, [askLocation, &mobile](const QPermission &permission) {
            if (permission.status() != Qt::PermissionStatus::Granted) {
                mobile.session()->warn(QStringLiteral("Microphone permission refused: the modem cannot receive."));
            }
            askLocation();
        });
    }
#endif
    QQmlApplicationEngine engine;
    // Map tiles on disk, served from there first (TileCache).
    engine.setNetworkAccessManagerFactory(new TileCacheFactory(qint64(config.ui.mapCacheMb) * 1024 * 1024));
    engine.addImageProvider(QStringLiteral("symbols"), new SymbolImageProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &mobile);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    // The module is compiled into the binary under /qt/qml; the URL form
    // works on every Qt 6, loadFromModule() only from 6.5.
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/AX25Chat/Mobile/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    if (!starter.isEmpty()) mobile.noteFirstRun(starter);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [] { androidui::stopKeepAlive(); });

    // AX25CHAT_SCREENSHOT_DIR=<folder>: render each page at phone size into
    // PNG files and quit.  Used with the offscreen platform to document the
    // interface without a device.
    const QString shotDir = qEnvironmentVariable("AX25CHAT_SCREENSHOT_DIR");
    if (!shotDir.isEmpty()) {
        QTimer::singleShot(6000, &app, [&engine, &mobile, shotDir] {
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            if (!window) { QCoreApplication::exit(2); return; }
            mobile.send(QString(), QStringLiteral("Hello from the phone, testing the mobile interface"));
            mobile.send(QStringLiteral("F1ABC-7"), QStringLiteral("Got your position, see you at the meeting"));
            // The three pages in the configured theme, then the chat page in
            // each of the other themes.
            struct Shot { QString name; QString page; QString theme; };
            const QList<Shot> shots = {
                {QStringLiteral("chat"), QStringLiteral("chat"), QString()},
                {QStringLiteral("stations"), QStringLiteral("stations"), QString()},
                {QStringLiteral("settings"), QStringLiteral("settings"), QString()},
                {QStringLiteral("settings-connection"), QStringLiteral("settings/connection"), QString()},
                {QStringLiteral("symbols"), QStringLiteral("symbol"), QString()},
                {QStringLiteral("map"), QStringLiteral("map"), QString()},
                {QStringLiteral("theme-dark"), QStringLiteral("chat"), QStringLiteral("dark")},
                {QStringLiteral("theme-red"), QStringLiteral("chat"), QStringLiteral("red")},
                {QStringLiteral("theme-amber"), QStringLiteral("chat"), QStringLiteral("amber")}};
            int index = 0;
            auto *stepper = new QTimer(window);
            QObject::connect(stepper, &QTimer::timeout, window, [=, &mobile]() mutable {
                if (index > 0) {
                    const QImage image = window->grabWindow();
                    image.save(QDir(shotDir).filePath(QStringLiteral("mobile-%1.png").arg(shots.at(index - 1).name)));
                }
                if (index >= shots.size()) { QCoreApplication::quit(); return; }
                QMetaObject::invokeMethod(window, "showPage", Q_ARG(QVariant, shots.at(index).page));
                if (!shots.at(index).theme.isEmpty()) {
                    QVariantMap map = mobile.configMap();
                    QVariantMap ui = map.value(QStringLiteral("ui")).toMap();
                    ui.insert(QStringLiteral("theme"), shots.at(index).theme);
                    map.insert(QStringLiteral("ui"), ui);
                    mobile.saveConfig(map);
                }
                index++;
            });
            stepper->start(1200);
        });
    }
    return app.exec();
}
