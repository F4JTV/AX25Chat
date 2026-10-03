/*
 * AppConfig.h - application configuration: value model plus JSON persistence.
 *
 * Port of ax25chat/config.py.  The file keeps the same location and the
 * same keys as the Python version, so a config.json written by it loads
 * unchanged:
 *
 *     Linux    ~/.config/AX25Chat/config.json
 *     Windows  %APPDATA%/AX25Chat/config.json
 *     macOS    ~/Library/Preferences/AX25Chat/config.json
 *
 * One section changed shape.  The Python version reached the modem through
 * a KISS link and supervised an external Direwolf, which took two sections,
 * "tnc" and "direwolf".  The modem is now inside the program, so those two
 * collapse into "modem": the direwolf.conf to load, the radio channel, and
 * the channel access parameters.  A file still carrying the old sections is
 * migrated on load (see ModemConfig::fromLegacy).
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

// Local station identity and the unproto path used for chat.
struct StationConfig
{
    QString callsign = QStringLiteral("NOCALL");
    int ssid = 0;
    QString destination = QStringLiteral("CHAT");   // unproto destination, e.g. CHAT, APRS, CQ
    QString digipeaters;                             // comma separated, e.g. "WIDE1-1,WIDE2-1"
    QString operatorName;
    QString locator;                                 // Maidenhead, informational only
    QString textEncoding = QStringLiteral("utf-8");
    int maxInfoLen = 200;                            // octets per frame before fragmentation

    QJsonObject toJson() const;
    static StationConfig fromJson(const QJsonObject &json);
};

// The embedded modem: which direwolf.conf, which channel, channel access.
struct ModemConfig
{
    bool enabled = true;             // part of the station; off = the APRS Internet System alone
    QString configFile;              // direwolf.conf; blank = the one in the settings folder
    int channel = 0;                 // CHANNEL in direwolf.conf we transmit and listen on
    bool autoStart = true;           // start the modem when the application opens
    bool echoLog = true;             // show the modem's own messages in the chat log

    // Pushed to the modem at start, same units as the KISS commands 1 to 5.
    int txdelay = 30;                // * 10 ms
    int persistence = 63;            // transmit probability (p + 1) / 256
    int slottime = 10;               // * 10 ms
    int txtail = 5;                  // * 10 ms
    bool fullDuplex = false;
    bool sendParams = true;          // false = keep what direwolf.conf says

    // Choices the mobile interface turns into a generated direwolf.conf
    // (ModemConfigFile::writeGenerated); the desktop edits the file itself.
    int speed = 1200;                // MODEM: 1200 or 9600
    QString ptt = QStringLiteral("none");   // none (VOX), cm108, rts, dtr
    int gpio = 3;                    // CM108 GPIO pin

    QJsonObject toJson() const;
    static ModemConfig fromJson(const QJsonObject &json);

    // Build from the "tnc" and "direwolf" sections of a file written by the
    // Python version.
    static ModemConfig fromLegacy(const QJsonObject &tnc, const QJsonObject &direwolf);
};

// Frequency-busy logic used before releasing a frame to the modem.
struct ChannelConfig
{
    bool useDcd = true;              // the modem's carrier detect counts as busy
    int rxHoldOffMs = 1500;          // quiet time required after the last activity
    int interFrameGapMs = 700;       // spacing between our own frames
    int randomJitterMs = 400;        // extra random delay, avoids collisions
    int maxDeferSeconds = 120;       // give up after this long, 0 = never
    bool waitForTxbufEmpty = true;   // wait for the modem's transmit queue to drain
    bool treatOwnEchoAsBusy = false;

    QJsonObject toJson() const;
    static ChannelConfig fromJson(const QJsonObject &json);
};

// Periodic unattended identification / presence beacon.
struct BeaconConfig
{
    bool enabled = false;
    int intervalMinutes = 30;
    QString text = QStringLiteral("AX25Chat station {call} {name} {loc} - QRV packet");
    bool sendOnConnect = false;
    QString destination;             // blank = use the station destination
    QString digipeaters;             // blank = use the station path

    QJsonObject toJson() const;
    static BeaconConfig fromJson(const QJsonObject &json);
};

// Audible and visual alerts for received traffic.  Positions and status
// reports default to off: on a busy APRS frequency they arrive every few
// seconds, and an application that beeped at each one gets muted
// altogether, taking the message alerts with it.
struct NotificationConfig
{
    bool enabled = true;
    bool sound = true;
    bool flashWindow = true;
    QString soundFile;               // blank = the platform's alert sound
    int volume = 80;                 // percent, custom sound only
    int minIntervalS = 5;            // rate limit, per category

    bool onMessage = true;
    bool onChat = true;
    bool onBulletin = true;
    bool onWeather = true;
    bool onPosition = false;
    bool onStatus = false;

    QJsonObject toJson() const;
    static NotificationConfig fromJson(const QJsonObject &json);
};

// APRS messaging: acknowledgements, retries and the station list.
struct MessagingConfig
{
    bool autoAck = true;             // answer messages addressed to us
    int maxAttempts = 5;             // transmissions before giving up
    int retrySeconds = 30;           // first retry delay, doubling after
    bool notifyOnMessage = true;
    bool trackStations = true;       // remember positions of stations heard
    int stationExpiryMinutes = 120;  // drop a station not heard for this long

    QJsonObject toJson() const;
    static MessagingConfig fromJson(const QJsonObject &json);
};

// APRS position reporting, fixed coordinates or from a GPS receiver.
struct PositionConfig
{
    bool enabled = false;            // periodic reporting
    int intervalMinutes = 30;
    QString source = QStringLiteral("fixed");   // "fixed" or "gps"

    // Fixed position
    double latitude = 0.0;
    double longitude = 0.0;
    double altitudeM = 0.0;

    // GPS receiver
    QString gpsPort;                 // COM3, ttyUSB0, ...
    int gpsBaud = 4800;
    int gpsMaxFixAgeS = 60;          // refuse to report a stale fix
    bool gpsSendCourseSpeed = true;
    bool gpsAutoOpen = true;         // open the port at start-up

    // SmartBeaconing: speed-adaptive interval, GPS source only.  Speeds in
    // km/h; published parameter sets are usually quoted in mph.
    bool smartEnabled = false;
    double smartLowSpeedKmh = 8.0;
    double smartHighSpeedKmh = 96.0;
    int smartSlowIntervalS = 1800;
    int smartFastIntervalS = 180;
    double smartTurnMinDeg = 28.0;
    double smartTurnSlope = 26.0;
    int smartTurnTimeS = 15;

    // Report content
    QString symbolTable = QStringLiteral("/");
    QString symbolCode = QStringLiteral("-");
    QString comment;
    bool sendAltitude = false;
    bool messagingCapable = true;    // '=' rather than '!'

    // Addressing.  A software identifier, not a real station.
    QString destination = QStringLiteral("APZ25C");
    QString digipeaters;             // blank = the station path
    bool sendOnConnect = false;

    QJsonObject toJson() const;
    static PositionConfig fromJson(const QJsonObject &json);
};

// APRS Internet System: what is heard on the air goes to the network (and
// so to aprs.fi), and the traffic around the station comes back.
struct AprsIsConfig
{
    bool enabled = false;
    QString region = QStringLiteral("rotate");  // rotate, euro, noam, soam, asia, aunz, custom
    QString customHost;
    int port = 14580;
    int passcode = -1;                          // -1: none, receive only
    bool gateRf = true;                         // send what is heard on the air
    bool receive = true;                        // ask for the traffic around the station
    int radiusKm = 50;                          // 1..500
    bool showInChat = false;                    // Internet traffic in the chat, not only the stations list
    bool positionEnabled = false;               // our own position to the network, periodically
    int positionIntervalMinutes = 10;
    bool positionOnConnect = true;              // and once right after the login

    QString host() const;
    QJsonObject toJson() const;
    static AprsIsConfig fromJson(const QJsonObject &o);
};

struct UiConfig
{
    QString qtStyle;                 // blank = the platform's own style
    QString theme = QStringLiteral("light");   // mobile interface: light, dark, red, amber
    QString mapProvider = QStringLiteral("osm");   // osm, opentopo, ign_plan, ign_ortho, esri_topo, esri_imagery, custom
    QString mapCustomUrl;                          // {z}/{x}/{y} template for custom
    bool mapSharp = true;                          // over-zoom plain tiles on high-density screens
    int mapCacheMb = 200;
    bool keepRunning = true;                       // mobile: foreground service while off screen
    bool keepScreenOn = true;                      // mobile: no inactivity lock while in front
    bool batteryPromptDone = false;                // mobile: the Doze exemption was offered once
    bool showTimestamps = true;
    bool showMonitor = true;         // log non-chat frames too
    int fontSize = 11;
    int maxLogLines = 2000;

    QJsonObject toJson() const;
    static UiConfig fromJson(const QJsonObject &json);
};


class AppConfig
{
public:
    StationConfig station;
    ModemConfig modem;
    ChannelConfig channel;
    BeaconConfig beacon;
    PositionConfig position;
    MessagingConfig messaging;
    NotificationConfig notifications;
    AprsIsConfig aprsis;
    UiConfig ui;

    // ---- helpers ------------------------------------------------------------

    // Full source address, e.g. "MYCALL-7".
    QString myCall() const;

    // The digipeater path as a list, from an override or the station path.
    QStringList digipeaterList(const QString &override = QString()) const;

    // The beacon template with {call}, {name} and {loc} expanded.  Empty
    // placeholders leave gaps, so whitespace runs are collapsed and a
    // dangling separator trimmed.
    QString beaconText() const;

    // What would stop this configuration from working on the air: an
    // empty string when all is well, else the first problem, worded for
    // the operator.  Both user interfaces run it before saving.
    QString validate() const;

    // ---- persistence ---------------------------------------------------------

    static QString appName();
    static QString configDir();
    static QString configPath();

    QJsonObject toJson() const;

    // Build from JSON, ignoring unknown keys and keeping defaults for
    // anything missing or of the wrong type.  Migrates the "tnc" and
    // "direwolf" sections of the Python version.
    static AppConfig fromJson(const QJsonObject &json);

    // Write atomically so a crash cannot leave a truncated file.  Returns
    // the path written, or an empty string with error set.
    QString save(const QString &path = QString(), QString *error = nullptr) const;

    // Load, falling back to defaults if the file is absent or corrupt.
    static AppConfig load(const QString &path = QString());
};
