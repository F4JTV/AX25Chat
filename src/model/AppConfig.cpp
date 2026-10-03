/*
 * AppConfig.cpp - application configuration: value model plus JSON persistence.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "AppConfig.h"
#include "AX25Frame.h"
#include "AprsEncoder.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

// Readers that keep the default when the key is missing or of a type that
// cannot be coerced, which is what the Python version did with int(),
// float(), bool() and str() in try/except.

QString getString(const QJsonObject &o, const char *key, const QString &def)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isString()) return v.toString();
    if (v.isDouble()) return QString::number(v.toDouble());
    if (v.isBool()) return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    return def;
}

int getInt(const QJsonObject &o, const char *key, int def)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isDouble()) return static_cast<int>(v.toDouble());
    if (v.isBool()) return v.toBool() ? 1 : 0;
    if (v.isString()) {
        bool ok = false;
        const int n = v.toString().trimmed().toInt(&ok);
        if (ok) return n;
        const double d = v.toString().trimmed().toDouble(&ok);
        if (ok) return static_cast<int>(d);
    }
    return def;
}

double getDouble(const QJsonObject &o, const char *key, double def)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isDouble()) return v.toDouble();
    if (v.isBool()) return v.toBool() ? 1.0 : 0.0;
    if (v.isString()) {
        bool ok = false;
        const double d = v.toString().trimmed().toDouble(&ok);
        if (ok) return d;
    }
    return def;
}

bool getBool(const QJsonObject &o, const char *key, bool def)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isBool()) return v.toBool();
    if (v.isDouble()) return v.toDouble() != 0.0;
    if (v.isString()) {
        const QString s = v.toString().trimmed().toLower();
        if (s == QLatin1String("true") || s == QLatin1String("1") || s == QLatin1String("yes")) return true;
        if (s == QLatin1String("false") || s == QLatin1String("0") || s == QLatin1String("no") || s.isEmpty()) return false;
    }
    return def;
}

} // namespace


// ---- StationConfig ----------------------------------------------------------

QJsonObject StationConfig::toJson() const
{
    return {
        {"callsign", callsign},
        {"ssid", ssid},
        {"destination", destination},
        {"digipeaters", digipeaters},
        {"operator_name", operatorName},
        {"locator", locator},
        {"text_encoding", textEncoding},
        {"max_info_len", maxInfoLen},
    };
}

StationConfig StationConfig::fromJson(const QJsonObject &o)
{
    StationConfig c;
    c.callsign = getString(o, "callsign", c.callsign);
    c.ssid = getInt(o, "ssid", c.ssid);
    c.destination = getString(o, "destination", c.destination);
    c.digipeaters = getString(o, "digipeaters", c.digipeaters);
    c.operatorName = getString(o, "operator_name", c.operatorName);
    c.locator = getString(o, "locator", c.locator);
    c.textEncoding = getString(o, "text_encoding", c.textEncoding);
    c.maxInfoLen = getInt(o, "max_info_len", c.maxInfoLen);
    return c;
}


// ---- ModemConfig ------------------------------------------------------------

QJsonObject ModemConfig::toJson() const
{
    return {
        {"enabled", enabled},
        {"config_file", configFile},
        {"channel", channel},
        {"auto_start", autoStart},
        {"echo_log", echoLog},
        {"txdelay", txdelay},
        {"persistence", persistence},
        {"slottime", slottime},
        {"txtail", txtail},
        {"full_duplex", fullDuplex},
        {"send_params", sendParams},
        {"speed", speed},
        {"ptt", ptt},
        {"gpio", gpio},
    };
}

ModemConfig ModemConfig::fromJson(const QJsonObject &o)
{
    ModemConfig c;
    c.enabled = getBool(o, "enabled", c.enabled);
    c.configFile = getString(o, "config_file", c.configFile);
    c.channel = getInt(o, "channel", c.channel);
    c.autoStart = getBool(o, "auto_start", c.autoStart);
    c.echoLog = getBool(o, "echo_log", c.echoLog);
    c.txdelay = getInt(o, "txdelay", c.txdelay);
    c.persistence = getInt(o, "persistence", c.persistence);
    c.slottime = getInt(o, "slottime", c.slottime);
    c.txtail = getInt(o, "txtail", c.txtail);
    c.fullDuplex = getBool(o, "full_duplex", c.fullDuplex);
    c.sendParams = getBool(o, "send_params", c.sendParams);
    c.speed = getInt(o, "speed", c.speed);
    c.ptt = getString(o, "ptt", c.ptt);
    c.gpio = getInt(o, "gpio", c.gpio);
    return c;
}

ModemConfig ModemConfig::fromLegacy(const QJsonObject &tnc, const QJsonObject &direwolf)
{
    ModemConfig c;
    // "tnc": the KISS channel and the parameters pushed over KISS.
    c.channel = getInt(tnc, "kiss_channel", c.channel);
    c.txdelay = getInt(tnc, "txdelay", c.txdelay);
    c.persistence = getInt(tnc, "persistence", c.persistence);
    c.slottime = getInt(tnc, "slottime", c.slottime);
    c.txtail = getInt(tnc, "txtail", c.txtail);
    c.fullDuplex = getBool(tnc, "full_duplex", c.fullDuplex);
    c.sendParams = getBool(tnc, "send_kiss_params", c.sendParams);
    // "direwolf": only the configuration file and two switches survive; the
    // executable, arguments, timeouts and console options described an
    // external process that no longer exists.
    c.configFile = getString(direwolf, "config_file", c.configFile);
    c.autoStart = getBool(direwolf, "auto_launch", c.autoStart);
    c.echoLog = getBool(direwolf, "echo_output", c.echoLog);
    return c;
}


// ---- ChannelConfig ----------------------------------------------------------

QJsonObject ChannelConfig::toJson() const
{
    return {
        {"use_dcd", useDcd},
        {"rx_hold_off_ms", rxHoldOffMs},
        {"inter_frame_gap_ms", interFrameGapMs},
        {"random_jitter_ms", randomJitterMs},
        {"max_defer_seconds", maxDeferSeconds},
        {"wait_for_txbuf_empty", waitForTxbufEmpty},
        {"treat_own_echo_as_busy", treatOwnEchoAsBusy},
    };
}

ChannelConfig ChannelConfig::fromJson(const QJsonObject &o)
{
    ChannelConfig c;
    c.useDcd = getBool(o, "use_dcd", c.useDcd);
    c.rxHoldOffMs = getInt(o, "rx_hold_off_ms", c.rxHoldOffMs);
    c.interFrameGapMs = getInt(o, "inter_frame_gap_ms", c.interFrameGapMs);
    c.randomJitterMs = getInt(o, "random_jitter_ms", c.randomJitterMs);
    c.maxDeferSeconds = getInt(o, "max_defer_seconds", c.maxDeferSeconds);
    c.waitForTxbufEmpty = getBool(o, "wait_for_txbuf_empty", c.waitForTxbufEmpty);
    c.treatOwnEchoAsBusy = getBool(o, "treat_own_echo_as_busy", c.treatOwnEchoAsBusy);
    return c;
}


// ---- BeaconConfig -----------------------------------------------------------

QJsonObject BeaconConfig::toJson() const
{
    return {
        {"enabled", enabled},
        {"interval_minutes", intervalMinutes},
        {"text", text},
        {"send_on_connect", sendOnConnect},
        {"destination", destination},
        {"digipeaters", digipeaters},
    };
}

BeaconConfig BeaconConfig::fromJson(const QJsonObject &o)
{
    BeaconConfig c;
    c.enabled = getBool(o, "enabled", c.enabled);
    c.intervalMinutes = getInt(o, "interval_minutes", c.intervalMinutes);
    c.text = getString(o, "text", c.text);
    c.sendOnConnect = getBool(o, "send_on_connect", c.sendOnConnect);
    c.destination = getString(o, "destination", c.destination);
    c.digipeaters = getString(o, "digipeaters", c.digipeaters);
    return c;
}


// ---- NotificationConfig -----------------------------------------------------

QJsonObject NotificationConfig::toJson() const
{
    return {
        {"enabled", enabled},
        {"sound", sound},
        {"flash_window", flashWindow},
        {"sound_file", soundFile},
        {"volume", volume},
        {"min_interval_s", minIntervalS},
        {"on_message", onMessage},
        {"on_chat", onChat},
        {"on_bulletin", onBulletin},
        {"on_weather", onWeather},
        {"on_position", onPosition},
        {"on_status", onStatus},
    };
}

NotificationConfig NotificationConfig::fromJson(const QJsonObject &o)
{
    NotificationConfig c;
    c.enabled = getBool(o, "enabled", c.enabled);
    c.sound = getBool(o, "sound", c.sound);
    c.flashWindow = getBool(o, "flash_window", c.flashWindow);
    c.soundFile = getString(o, "sound_file", c.soundFile);
    c.volume = getInt(o, "volume", c.volume);
    c.minIntervalS = getInt(o, "min_interval_s", c.minIntervalS);
    c.onMessage = getBool(o, "on_message", c.onMessage);
    c.onChat = getBool(o, "on_chat", c.onChat);
    c.onBulletin = getBool(o, "on_bulletin", c.onBulletin);
    c.onWeather = getBool(o, "on_weather", c.onWeather);
    c.onPosition = getBool(o, "on_position", c.onPosition);
    c.onStatus = getBool(o, "on_status", c.onStatus);
    return c;
}


// ---- MessagingConfig --------------------------------------------------------

QJsonObject MessagingConfig::toJson() const
{
    return {
        {"auto_ack", autoAck},
        {"max_attempts", maxAttempts},
        {"retry_seconds", retrySeconds},
        {"notify_on_message", notifyOnMessage},
        {"track_stations", trackStations},
        {"station_expiry_minutes", stationExpiryMinutes},
    };
}

MessagingConfig MessagingConfig::fromJson(const QJsonObject &o)
{
    MessagingConfig c;
    c.autoAck = getBool(o, "auto_ack", c.autoAck);
    c.maxAttempts = getInt(o, "max_attempts", c.maxAttempts);
    c.retrySeconds = getInt(o, "retry_seconds", c.retrySeconds);
    c.notifyOnMessage = getBool(o, "notify_on_message", c.notifyOnMessage);
    c.trackStations = getBool(o, "track_stations", c.trackStations);
    c.stationExpiryMinutes = getInt(o, "station_expiry_minutes", c.stationExpiryMinutes);
    return c;
}


// ---- PositionConfig ---------------------------------------------------------

QJsonObject PositionConfig::toJson() const
{
    return {
        {"enabled", enabled},
        {"interval_minutes", intervalMinutes},
        {"source", source},
        {"latitude", latitude},
        {"longitude", longitude},
        {"altitude_m", altitudeM},
        {"gps_port", gpsPort},
        {"gps_baud", gpsBaud},
        {"gps_max_fix_age_s", gpsMaxFixAgeS},
        {"gps_send_course_speed", gpsSendCourseSpeed},
        {"gps_auto_open", gpsAutoOpen},
        {"smart_enabled", smartEnabled},
        {"smart_low_speed_kmh", smartLowSpeedKmh},
        {"smart_high_speed_kmh", smartHighSpeedKmh},
        {"smart_slow_interval_s", smartSlowIntervalS},
        {"smart_fast_interval_s", smartFastIntervalS},
        {"smart_turn_min_deg", smartTurnMinDeg},
        {"smart_turn_slope", smartTurnSlope},
        {"smart_turn_time_s", smartTurnTimeS},
        {"symbol_table", symbolTable},
        {"symbol_code", symbolCode},
        {"comment", comment},
        {"send_altitude", sendAltitude},
        {"messaging_capable", messagingCapable},
        {"destination", destination},
        {"digipeaters", digipeaters},
        {"send_on_connect", sendOnConnect},
    };
}

PositionConfig PositionConfig::fromJson(const QJsonObject &o)
{
    PositionConfig c;
    c.enabled = getBool(o, "enabled", c.enabled);
    c.intervalMinutes = getInt(o, "interval_minutes", c.intervalMinutes);
    c.source = getString(o, "source", c.source);
    c.latitude = getDouble(o, "latitude", c.latitude);
    c.longitude = getDouble(o, "longitude", c.longitude);
    c.altitudeM = getDouble(o, "altitude_m", c.altitudeM);
    c.gpsPort = getString(o, "gps_port", c.gpsPort);
    c.gpsBaud = getInt(o, "gps_baud", c.gpsBaud);
    c.gpsMaxFixAgeS = getInt(o, "gps_max_fix_age_s", c.gpsMaxFixAgeS);
    c.gpsSendCourseSpeed = getBool(o, "gps_send_course_speed", c.gpsSendCourseSpeed);
    c.gpsAutoOpen = getBool(o, "gps_auto_open", c.gpsAutoOpen);
    c.smartEnabled = getBool(o, "smart_enabled", c.smartEnabled);
    c.smartLowSpeedKmh = getDouble(o, "smart_low_speed_kmh", c.smartLowSpeedKmh);
    c.smartHighSpeedKmh = getDouble(o, "smart_high_speed_kmh", c.smartHighSpeedKmh);
    c.smartSlowIntervalS = getInt(o, "smart_slow_interval_s", c.smartSlowIntervalS);
    c.smartFastIntervalS = getInt(o, "smart_fast_interval_s", c.smartFastIntervalS);
    c.smartTurnMinDeg = getDouble(o, "smart_turn_min_deg", c.smartTurnMinDeg);
    c.smartTurnSlope = getDouble(o, "smart_turn_slope", c.smartTurnSlope);
    c.smartTurnTimeS = getInt(o, "smart_turn_time_s", c.smartTurnTimeS);
    c.symbolTable = getString(o, "symbol_table", c.symbolTable);
    c.symbolCode = getString(o, "symbol_code", c.symbolCode);
    c.comment = getString(o, "comment", c.comment);
    c.sendAltitude = getBool(o, "send_altitude", c.sendAltitude);
    c.messagingCapable = getBool(o, "messaging_capable", c.messagingCapable);
    c.destination = getString(o, "destination", c.destination);
    c.digipeaters = getString(o, "digipeaters", c.digipeaters);
    c.sendOnConnect = getBool(o, "send_on_connect", c.sendOnConnect);
    return c;
}


// ---- AprsIsConfig -----------------------------------------------------------

// The Tier 2 network's regional rotate addresses, port 14580 (the
// client-defined filter port) everywhere.
QString AprsIsConfig::host() const
{
    if (region == QLatin1String("euro")) return QStringLiteral("euro.aprs2.net");
    if (region == QLatin1String("noam")) return QStringLiteral("noam.aprs2.net");
    if (region == QLatin1String("soam")) return QStringLiteral("soam.aprs2.net");
    if (region == QLatin1String("asia")) return QStringLiteral("asia.aprs2.net");
    if (region == QLatin1String("aunz")) return QStringLiteral("aunz.aprs2.net");
    if (region == QLatin1String("custom")) return customHost.trimmed();
    return QStringLiteral("rotate.aprs2.net");
}

QJsonObject AprsIsConfig::toJson() const
{
    return {
        {"enabled", enabled},
        {"region", region},
        {"custom_host", customHost},
        {"port", port},
        {"passcode", passcode},
        {"gate_rf", gateRf},
        {"receive", receive},
        {"radius_km", radiusKm},
        {"show_in_chat", showInChat},
        {"position_enabled", positionEnabled},
        {"position_interval_minutes", positionIntervalMinutes},
        {"position_on_connect", positionOnConnect},
    };
}

AprsIsConfig AprsIsConfig::fromJson(const QJsonObject &o)
{
    AprsIsConfig c;
    c.enabled = getBool(o, "enabled", c.enabled);
    c.region = getString(o, "region", c.region);
    c.customHost = getString(o, "custom_host", c.customHost);
    c.port = getInt(o, "port", c.port);
    c.passcode = getInt(o, "passcode", c.passcode);
    c.gateRf = getBool(o, "gate_rf", c.gateRf);
    c.receive = getBool(o, "receive", c.receive);
    c.radiusKm = getInt(o, "radius_km", c.radiusKm);
    c.showInChat = getBool(o, "show_in_chat", c.showInChat);
    c.positionEnabled = getBool(o, "position_enabled", c.positionEnabled);
    c.positionIntervalMinutes = getInt(o, "position_interval_minutes", c.positionIntervalMinutes);
    c.positionOnConnect = getBool(o, "position_on_connect", c.positionOnConnect);
    return c;
}

// ---- UiConfig ---------------------------------------------------------------

QJsonObject UiConfig::toJson() const
{
    return {
        {"qt_style", qtStyle},
        {"theme", theme},
        {"map_provider", mapProvider},
        {"map_custom_url", mapCustomUrl},
        {"map_sharp", mapSharp},
        {"map_cache_mb", mapCacheMb},
        {"keep_running", keepRunning},
        {"keep_screen_on", keepScreenOn},
        {"battery_prompt_done", batteryPromptDone},
        {"show_timestamps", showTimestamps},
        {"show_monitor", showMonitor},
        {"font_size", fontSize},
        {"max_log_lines", maxLogLines},
    };
}

UiConfig UiConfig::fromJson(const QJsonObject &o)
{
    UiConfig c;
    c.qtStyle = getString(o, "qt_style", c.qtStyle);
    c.theme = getString(o, "theme", c.theme);
    c.mapProvider = getString(o, "map_provider", c.mapProvider);
    c.mapCustomUrl = getString(o, "map_custom_url", c.mapCustomUrl);
    c.mapSharp = getBool(o, "map_sharp", c.mapSharp);
    c.mapCacheMb = getInt(o, "map_cache_mb", c.mapCacheMb);
    c.keepRunning = getBool(o, "keep_running", c.keepRunning);
    c.keepScreenOn = getBool(o, "keep_screen_on", c.keepScreenOn);
    c.batteryPromptDone = getBool(o, "battery_prompt_done", c.batteryPromptDone);
    c.showTimestamps = getBool(o, "show_timestamps", c.showTimestamps);
    c.showMonitor = getBool(o, "show_monitor", c.showMonitor);
    c.fontSize = getInt(o, "font_size", c.fontSize);
    c.maxLogLines = getInt(o, "max_log_lines", c.maxLogLines);
    return c;
}


// ---- AppConfig --------------------------------------------------------------

QString AppConfig::myCall() const
{
    const QString base = station.callsign.trimmed().toUpper().isEmpty()
                             ? QStringLiteral("NOCALL") : station.callsign.trimmed().toUpper();
    return station.ssid ? QStringLiteral("%1-%2").arg(base).arg(station.ssid) : base;
}

QStringList AppConfig::digipeaterList(const QString &override) const
{
    const QString raw = override.trimmed().isEmpty() ? station.digipeaters : override.trimmed();
    QStringList out;
    for (const QString &part : raw.split(',')) {
        const QString p = part.trimmed().toUpper();
        if (!p.isEmpty()) out << p;
    }
    return out;
}

QString AppConfig::beaconText() const
{
    QString expanded = beacon.text;
    expanded.replace(QStringLiteral("{call}"), myCall());
    expanded.replace(QStringLiteral("{name}"), station.operatorName);
    expanded.replace(QStringLiteral("{loc}"), station.locator);
    QString collapsed = expanded.simplified();
    collapsed.replace(QStringLiteral(" -  "), QStringLiteral(" - "));
    // strip(" -")
    while (collapsed.startsWith(' ') || collapsed.startsWith('-')) collapsed.remove(0, 1);
    while (collapsed.endsWith(' ') || collapsed.endsWith('-')) collapsed.chop(1);
    return collapsed;
}

QString AppConfig::validate() const
{
    QString call, error;
    int ssid = 0;
    if (!ax25::splitCallsign(station.callsign, call, ssid, &error)) return error;
    if (call == QLatin1String("NOCALL")) return QStringLiteral("Please enter your own callsign. NOCALL must not be transmitted on the air.");
    if (station.ssid < 0 || station.ssid > 15) return QStringLiteral("The SSID must be between 0 and 15.");
    const QString destination = station.destination.trimmed().isEmpty() ? QStringLiteral("CHAT") : station.destination;
    if (!ax25::splitCallsign(destination, call, ssid, &error)) return QStringLiteral("Destination: %1").arg(error);

    auto checkPath = [&](const QString &text, const QString &what) -> QString {
        QStringList digis;
        for (const QString &d : text.split(',')) if (!d.trimmed().isEmpty()) digis << d.trimmed();
        if (digis.size() > 8) return QStringLiteral("At most 8 digipeaters are allowed in an AX.25 path.");
        for (QString digi : digis) {
            while (digi.endsWith('*')) digi.chop(1);
            if (!ax25::splitCallsign(digi, call, ssid, &error)) return QStringLiteral("%1: %2").arg(what, error);
        }
        return QString();
    };
    if (const QString e = checkPath(station.digipeaters, QStringLiteral("Digipeater path")); !e.isEmpty()) return e;

    if (!beacon.destination.trimmed().isEmpty() && !ax25::splitCallsign(beacon.destination, call, ssid, &error)) {
        return QStringLiteral("Beacon destination: %1").arg(error);
    }
    if (beacon.enabled && beacon.text.trimmed().isEmpty()) return QStringLiteral("The beacon is enabled but its text is empty.");
    if (const QString e = checkPath(beacon.digipeaters, QStringLiteral("Beacon path")); !e.isEmpty()) return e;

    if (position.enabled || !position.destination.trimmed().isEmpty()) {
        const QString dest = position.destination.trimmed().isEmpty() ? aprs::DefaultTocall : position.destination;
        if (!ax25::splitCallsign(dest, call, ssid, &error)) return QStringLiteral("APRS destination: %1").arg(error);
    }
    if (position.enabled && position.source != QLatin1String("gps")
        && position.latitude == 0.0 && position.longitude == 0.0) {
        return QStringLiteral("Periodic position reporting is enabled but the fixed coordinates are still 0, 0. "
                              "Enter them, or use Fill from locator.");
    }
    if (const QString e = checkPath(position.digipeaters, QStringLiteral("Position path")); !e.isEmpty()) return e;
    if (position.smartEnabled && position.source != QLatin1String("gps")) {
        return QStringLiteral("SmartBeaconing needs the GPS source: with fixed coordinates the speed is always zero, "
                              "so every interval would collapse to the slow one.");
    }
    if (position.smartEnabled && position.smartLowSpeedKmh >= position.smartHighSpeedKmh) {
        return QStringLiteral("SmartBeaconing: the stopped speed must be below the fast speed.");
    }
    if (!aprs::validateSymbol(position.symbolTable, position.symbolCode, &error)) return QStringLiteral("Map symbol: %1").arg(error);
    if (!station.locator.trimmed().isEmpty()) {
        double lat, lon;
        if (!aprs::locatorToLatLon(station.locator, lat, lon, &error)) return error;
    }
    if (modem.speed != 1200 && modem.speed != 9600) return QStringLiteral("The modem speed must be 1200 or 9600.");
    if (!modem.enabled && !aprsis.enabled) {
        return QStringLiteral("Nothing to connect: choose the modem, the APRS Internet System, or both.");
    }
    static const QStringList regions = {QStringLiteral("rotate"), QStringLiteral("euro"), QStringLiteral("noam"),
                                        QStringLiteral("soam"), QStringLiteral("asia"), QStringLiteral("aunz"),
                                        QStringLiteral("custom")};
    if (!regions.contains(aprsis.region)) return QStringLiteral("Unknown APRS-IS region '%1'.").arg(aprsis.region);
    if (aprsis.region == QLatin1String("custom") && aprsis.customHost.trimmed().isEmpty()) {
        return QStringLiteral("APRS-IS: enter the server's host name, or pick a region.");
    }
    if (aprsis.port < 1 || aprsis.port > 65535) return QStringLiteral("APRS-IS: the port must be between 1 and 65535.");
    if (aprsis.passcode < -1 || aprsis.passcode > 99999) return QStringLiteral("APRS-IS: the passcode is a number of up to five digits, or -1 for none.");
    if (aprsis.radiusKm < 1 || aprsis.radiusKm > 500) return QStringLiteral("APRS-IS: the radius must be between 1 and 500 km.");
    if (aprsis.enabled && aprsis.positionEnabled && aprsis.passcode < 0) {
        return QStringLiteral("APRS-IS: uploading your position needs the passcode for your callsign; enter it "
                              "(or compute it), or switch the position upload off.");
    }
    if (aprsis.enabled && aprsis.gateRf && aprsis.passcode < 0) {
        return QStringLiteral("APRS-IS: uploading what is heard needs the passcode for your callsign; enter it "
                              "(or compute it), or switch the upload off.");
    }
    static const QStringList pttMethods = {QStringLiteral("none"), QStringLiteral("cm108"), QStringLiteral("rts"), QStringLiteral("dtr")};
    if (!pttMethods.contains(modem.ptt)) return QStringLiteral("Unknown PTT method '%1'.").arg(modem.ptt);
    if (ui.mapProvider == QLatin1String("custom")
        && (!ui.mapCustomUrl.contains(QLatin1String("{z}")) || !ui.mapCustomUrl.contains(QLatin1String("{x}"))
            || !ui.mapCustomUrl.contains(QLatin1String("{y}")))) {
        return QStringLiteral("Map: a custom tile URL must contain {z}, {x} and {y}.");
    }
    if (ui.mapCacheMb < 20 || ui.mapCacheMb > 5000) return QStringLiteral("Map: the tile cache must be between 20 and 5000 MB.");
    return QString();
}

QString AppConfig::appName()
{
    return QStringLiteral("AX25Chat");
}

QString AppConfig::configDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (base.isEmpty()) {
        base = QDir::home().filePath(QStringLiteral(".config/") + appName());
    } else {
        QString trimmed = base;
        while (trimmed.endsWith('/') || trimmed.endsWith('\\')) trimmed.chop(1);
        if (!trimmed.endsWith(appName())) base = QDir(base).filePath(appName());
    }
    return base;
}

QString AppConfig::configPath()
{
    return QDir(configDir()).filePath(QStringLiteral("config.json"));
}

QJsonObject AppConfig::toJson() const
{
    return {
        {"station", station.toJson()},
        {"modem", modem.toJson()},
        {"channel", channel.toJson()},
        {"beacon", beacon.toJson()},
        {"position", position.toJson()},
        {"messaging", messaging.toJson()},
        {"notifications", notifications.toJson()},
        {"aprsis", aprsis.toJson()},
        {"ui", ui.toJson()},
    };
}

AppConfig AppConfig::fromJson(const QJsonObject &json)
{
    AppConfig c;
    auto section = [&json](const char *name) { return json.value(QLatin1String(name)).toObject(); };

    c.station = StationConfig::fromJson(section("station"));
    if (json.contains(QLatin1String("modem"))) {
        c.modem = ModemConfig::fromJson(section("modem"));
    } else if (json.contains(QLatin1String("tnc")) || json.contains(QLatin1String("direwolf"))) {
        c.modem = ModemConfig::fromLegacy(section("tnc"), section("direwolf"));
    }
    c.channel = ChannelConfig::fromJson(section("channel"));
    c.beacon = BeaconConfig::fromJson(section("beacon"));
    c.position = PositionConfig::fromJson(section("position"));
    c.messaging = MessagingConfig::fromJson(section("messaging"));
    c.notifications = NotificationConfig::fromJson(section("notifications"));
    c.aprsis = AprsIsConfig::fromJson(section("aprsis"));
    c.ui = UiConfig::fromJson(section("ui"));
    return c;
}

QString AppConfig::save(const QString &pathIn, QString *error) const
{
    const QString path = pathIn.isEmpty() ? configPath() : pathIn;
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error) *error = QStringLiteral("Cannot create %1").arg(info.absolutePath());
        return QString();
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return QString();
    }
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return QString();
    }
    return path;
}

AppConfig AppConfig::load(const QString &pathIn)
{
    const QString path = pathIn.isEmpty() ? configPath() : pathIn;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return AppConfig();
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) return AppConfig();
    return fromJson(doc.object());
}
