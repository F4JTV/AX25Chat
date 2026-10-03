/*
 * MobileApp.cpp - what the Qt Quick interface binds to.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MobileApp.h"

#include <algorithm>

#include "AndroidUi.h"
#include "AprsEncoder.h"
#include "ModemConfigFile.h"
#include "TileCache.h"
#include "TileProviders.h"
#include "Notifier.h"
#include "SymbolArt.h"
#include "SymbolPicker.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QUrl>
#include <QJsonObject>
#include <QTextStream>

namespace {

QString colourFor(const QString &key)
{
    if (key == QLatin1String("message")) return QStringLiteral("rx");
    if (key == QLatin1String("bulletin")) return QStringLiteral("beacon");
    if (key == QLatin1String("weather") || key == QLatin1String("position") || key == QLatin1String("status")) {
        return QStringLiteral("monitor");
    }
    return key;
}

QString categoryFor(const QString &key)
{
    if (key == QLatin1String("rx")) return QStringLiteral("chat");
    if (key == QLatin1String("message") || key == QLatin1String("bulletin") || key == QLatin1String("weather")
        || key == QLatin1String("position") || key == QLatin1String("status")) {
        return key;
    }
    return QString();
}

} // namespace


// ---- LogModel ---------------------------------------------------------------------

LogModel::LogModel(int maxLines, QObject *parent) : QAbstractListModel(parent), m_maxLines(qMax(50, maxLines)) {}

int LogModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size()) return {};
    const Entry &e = m_entries.at(index.row());
    switch (role) {
        case TimeRole: return e.time;
        case PrefixRole: return e.prefix;
        case MessageRole: return e.message;
        case ColourRole: return e.colour;
        default: return {};
    }
}

QHash<int, QByteArray> LogModel::roleNames() const
{
    return {{TimeRole, "time"}, {PrefixRole, "prefix"}, {MessageRole, "message"}, {ColourRole, "colour"}};
}

void LogModel::append(const QString &time, const QString &prefix, const QString &message, const QString &colour)
{
    if (m_entries.size() >= m_maxLines) {
        beginRemoveRows(QModelIndex(), 0, 0);
        m_entries.removeFirst();
        endRemoveRows();
    }
    beginInsertRows(QModelIndex(), m_entries.size(), m_entries.size());
    m_entries.append({time, prefix, message, colour});
    endInsertRows();
}

void LogModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

QString LogModel::plainText() const
{
    QString text;
    for (const Entry &e : m_entries) {
        text += QStringLiteral("%1 %2 %3\n").arg(e.time, e.prefix, e.message);
    }
    return text;
}


// ---- StationModel -----------------------------------------------------------------

StationModel::StationModel(QObject *parent) : QAbstractListModel(parent) {}

int StationModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_stations.size();
}

QVariant StationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_stations.size()) return {};
    const Station &s = m_stations.at(index.row());
    switch (role) {
        case CallsignRole: return s.callsign;
        case LineRole: return m_lines.at(index.row());
        case TableRole: return s.symbolTable;
        case CodeRole: return s.symbolCode;
        case HasPositionRole: return s.hasPosition();
        case TooltipRole: return m_tooltips.at(index.row());
        case LatitudeRole: return s.latitude ? *s.latitude : 0.0;
        case LongitudeRole: return s.longitude ? *s.longitude : 0.0;
        case InternetRole: return s.viaInternet;
        default: return {};
    }
}

QHash<int, QByteArray> StationModel::roleNames() const
{
    return {{CallsignRole, "callsign"}, {LineRole, "line"}, {TableRole, "table"}, {CodeRole, "code"},
            {HasPositionRole, "hasPosition"}, {TooltipRole, "tooltip"}, {LatitudeRole, "latitude"},
            {LongitudeRole, "longitude"}, {InternetRole, "internet"}};
}

// Called every second for the ages, and whenever a station is heard.  The
// list is alphabetical, and changes are applied in place - rows updated,
// a newcomer inserted where it sorts, an expired station removed - so the
// view keeps its scroll position whatever arrives.
void StationModel::refresh(const QList<Station> &stationsIn, double now, std::optional<std::pair<double, double>> reference)
{
    QList<Station> stations = stationsIn;
    std::sort(stations.begin(), stations.end(), [](const Station &a, const Station &b) {
        return a.callsign.compare(b.callsign, Qt::CaseInsensitive) < 0;
    });
    const int before = m_stations.size();
    int i = 0, j = 0;
    while (i < m_stations.size() || j < stations.size()) {
        const int order = (i >= m_stations.size()) ? 1 : (j >= stations.size()) ? -1
                        : m_stations.at(i).callsign.compare(stations.at(j).callsign, Qt::CaseInsensitive);
        if (order < 0) {
            // Gone from the registry: expired.
            beginRemoveRows(QModelIndex(), i, i);
            m_stations.removeAt(i);
            m_lines.removeAt(i);
            m_tooltips.removeAt(i);
            endRemoveRows();
        } else if (order > 0) {
            // A station heard for the first time, where it sorts.
            const Station &s = stations.at(j);
            beginInsertRows(QModelIndex(), i, i);
            m_stations.insert(i, s);
            m_lines.insert(i, s.describe(now, reference));
            m_tooltips.insert(i, s.tooltip());
            endInsertRows();
            i++;
            j++;
        } else {
            const Station &s = stations.at(j);
            m_stations[i] = s;
            m_lines[i] = s.describe(now, reference);
            m_tooltips[i] = s.tooltip();
            emit dataChanged(index(i), index(i));
            i++;
            j++;
        }
    }
    if (m_stations.size() != before) emit countChanged();
}


// ---- SymbolImageProvider -----------------------------------------------------------

SymbolImageProvider::SymbolImageProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

// The id is "<table char code>-<symbol char code>" in decimal: a symbol can
// be '/', '\\', '%', '#' or '?', none of which survives a URL.
QPixmap SymbolImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QString key = id.section('?', 0, 0);
    const int dash = key.indexOf('-', 1);
    bool okTable = false, okCode = false;
    const int tableValue = key.left(dash).toInt(&okTable);
    const int codeValue = key.mid(dash + 1).toInt(&okCode);
    if (dash < 1 || !okTable || !okCode || tableValue <= 0 || codeValue <= 0) {
        qWarning("symbols image provider: bad id '%s'", qPrintable(id));
        return QPixmap();
    }
    const int px = requestedSize.width() > 0 ? requestedSize.width() : 48;
    const QPixmap art = symbolart::pixmap(QString(QChar(tableValue)), QString(QChar(codeValue)), px);
    if (size) *size = art.size();
    return art;
}


// ---- MobileApp ------------------------------------------------------------------------

MobileApp::MobileApp(const AppConfig &config, QObject *parent)
    : QObject(parent), m_config(config)
{
    m_log = new LogModel(config.ui.maxLogLines, this);
    m_stations = new StationModel(this);
    m_notifier = new Notifier(config, nullptr, this);
    m_session = new Session(config, this);

    connect(m_session, &Session::logEntry, this, &MobileApp::onLogEntry);
    connect(m_session, &Session::stationsChanged, this, &MobileApp::refreshStations);
    connect(m_session, &Session::queueChanged, this, [this](int) { emit queueChanged(); });
    connect(m_session, &Session::panelChanged, this, [this] { emit queueChanged(); });
    connect(m_session, &Session::configurationNeeded, this, &MobileApp::showSettings);
    connect(m_session, &Session::settingsApplied, this, &MobileApp::configChanged);
    connect(m_notifier, &Notifier::logMessage, this, [this](const QString &level, const QString &message) {
        if (level == QLatin1String("error")) m_session->warn(message);
        else m_session->systemLine(message);
    });
}

bool MobileApp::flatRendering() const
{
    return qEnvironmentVariableIsSet("AX25CHAT_SCREENSHOT_DIR")
        || qgetenv("QT_QUICK_BACKEND").toLower() == "software";
}

QVariantMap MobileApp::configMap() const
{
    return m_config.toJson().toVariantMap();
}

QStringList MobileApp::queue() const
{
    QStringList lines;
    const auto items = m_session->channel()->pendingItems();
    for (const ChannelManager::OutgoingItem &item : items) {
        const qint64 waited = qMax<qint64>(0, (m_session->channel()->clockNow() - item.queuedAtMs) / 1000);
        lines << QStringLiteral("[%1] %2  (%3s)").arg(item.kind, item.describe()).arg(waited);
    }
    return lines;
}

bool MobileApp::symbolsAvailable() const
{
    return symbolart::available();
}

QString MobileApp::gpsKind() const
{
    return m_session->gps()->kind();
}

QString MobileApp::generatedConfPath() const
{
    return modemconf::userPath();
}

bool MobileApp::saveConfig(const QVariantMap &map)
{
    AppConfig config = AppConfig::fromJson(QJsonObject::fromVariantMap(map));
    const QString problem = config.validate();
    if (!problem.isEmpty()) {
        m_lastError = problem;
        emit lastErrorChanged();
        return false;
    }
    // The mobile interface owns the modem configuration file: regenerated
    // from the choices on every save, unless the operator pointed at a
    // file of their own.
    const QString generated = generatedConfPath();
    if (config.modem.configFile.isEmpty() || config.modem.configFile == generated) {
        QString error;
        const QString written = modemconf::writeGenerated(generated, config.myCall(), config.modem.speed,
                                                          config.modem.ptt, config.modem.gpio, &error);
        if (written.isEmpty()) {
            m_lastError = QStringLiteral("Could not write the modem configuration: %1").arg(error);
            emit lastErrorChanged();
            return false;
        }
        config.modem.configFile = written;
        // A regenerated file is a changed modem configuration: make the
        // session restart the modem by giving it a fresh path string.
    }
    QString saveError;
    if (config.save(QString(), &saveError).isEmpty()) {
        m_lastError = QStringLiteral("Could not save the settings: %1").arg(saveError);
        emit lastErrorChanged();
        return false;
    }
    m_lastError.clear();
    emit lastErrorChanged();
    const bool modemFileRewritten = config.modem.configFile == generated;
    m_config = config;
    m_notifier->reloadConfig(config);
    m_log->setMaxLines(config.ui.maxLogLines);
    if (modemFileRewritten && m_session->modemRunning()) {
        // The file's content changed even though its path did not.
        m_session->systemLine(QStringLiteral("Modem configuration regenerated, restarting the modem."));
        m_session->stopModem();
        QObject *ctx = new QObject(this);
        connect(m_session, &Session::modemStateChanged, ctx, [this, ctx] {
            if (!m_session->modemRunning() && m_session->modem()->state() == DirewolfEngine::State::Stopped) {
                ctx->deleteLater();
                m_session->startModem();
            }
        });
    }
    m_session->applySettings(config);
    if (config.ui.keepRunning) androidui::startKeepAlive();
    else androidui::stopKeepAlive();
    androidui::keepScreenOn(config.ui.keepScreenOn);
    emit configChanged();
    return true;
}

bool MobileApp::send(const QString &recipient, const QString &text)
{
    return m_session->sendText(recipient, text);
}

QString MobileApp::symbolDescription(const QString &table, const QString &code) const
{
    return SymbolPicker::describe(table, code);
}

QVariantList MobileApp::commonSymbols() const
{
    QVariantList list;
    for (const symbols::Common &c : symbols::commonSymbols()) {
        list << QVariantMap{{"table", c.table}, {"code", c.code}, {"description", c.description}};
    }
    return list;
}

// Every symbol of one table, in code order, for the full picker.
QVariantList MobileApp::symbolTable(const QString &table) const
{
    QVariantList list;
    for (const symbols::Entry &e : symbols::tableEntries(table)) {
        list << QVariantMap{{"table", table}, {"code", e.code}, {"description", e.description}};
    }
    return list;
}

QString MobileApp::symbolImage(const QString &table, const QString &code) const
{
    if (table.isEmpty() || code.isEmpty()) return QString();
    // Explicit integers: QString::arg() on a char16_t is resolved to the
    // QChar overload by newer Qt versions, which put the characters
    // themselves back into the URL.
    return QStringLiteral("image://symbols/") + QString::number(int(table.at(0).unicode()))
         + QLatin1Char('-') + QString::number(int(code.at(0).unicode()));
}

QVariantMap MobileApp::locatorToLatLon(const QString &locator) const
{
    double lat = 0, lon = 0;
    QString error;
    if (!aprs::locatorToLatLon(locator, lat, lon, &error)) return {{"error", error}};
    return {{"latitude", lat}, {"longitude", lon}};
}

int MobileApp::passcodeFor(const QString &callsign) const
{
    return aprs::passcode(callsign);
}

QVariantMap MobileApp::ownPosition() const
{
    if (const auto reference = m_session->referencePosition()) {
        return {{"latitude", reference->first}, {"longitude", reference->second}, {"valid", true},
                {"table", m_config.position.symbolTable}, {"code", m_config.position.symbolCode}};
    }
    return {{"valid", false}};
}

// Free tile servers that need no key.  Both ask for an identifiable
// User-Agent and a cache, which TileCache provides, and for their credit
// to be shown, which the map does.
QString MobileApp::tileUrlTemplate() const
{
    return tiles::current(m_config).urlTemplate;
}

QString MobileApp::tileAttribution() const
{
    return tiles::current(m_config).attribution;
}

int MobileApp::tileScale() const
{
    return tiles::scale(tiles::current(m_config));
}

int MobileApp::tileMaxZoom() const
{
    return tiles::current(m_config).maxZoom;
}

QVariantList MobileApp::tileProviders() const
{
    QVariantList list;
    for (const tiles::Provider &p : tiles::providers()) list << QVariantMap{{"key", p.key}, {"name", p.name}};
    list << QVariantMap{{"key", QStringLiteral("custom")}, {"name", QStringLiteral("Another tile server")}};
    return list;
}

// Dire Wolf looks for tocalls.yaml and symbols-new.txt in data/ under the
// working directory.  On a phone there is no folder next to the program,
// so the two files are compiled in and written to the application's data
// folder, which becomes the working directory.  Returns that folder, or
// an empty string when nothing could be written.
QString MobileApp::prepareDataFolder()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString dataDir = base + QStringLiteral("/data");
    if (!QDir().mkpath(dataDir)) return QString();
    for (const QString &name : {QStringLiteral("tocalls.yaml"), QStringLiteral("symbols-new.txt")}) {
        const QString target = dataDir + QLatin1Char('/') + name;
        QFile resource(QStringLiteral(":/data/") + name);
        if (!resource.exists()) continue;
        // Rewritten when the bundled copy differs (a new version of the
        // application carries a newer tocalls.yaml).
        QFile existing(target);
        if (existing.exists() && existing.size() == resource.size()) continue;
        QFile::remove(target);
        if (!resource.copy(target)) return QString();
        QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner);
    }
    return QFileInfo::exists(dataDir + QStringLiteral("/tocalls.yaml")) ? base : QString();
}

double MobileApp::tileCacheMb() const
{
    return TileCacheFactory::sizeOnDisk() / (1024.0 * 1024.0);
}

void MobileApp::clearTileCache()
{
    TileCacheFactory::clear();
    m_session->systemLine(QStringLiteral("Map tile cache cleared."));
}

void MobileApp::checkLocationService()
{
    if (m_config.position.source != QLatin1String("gps")) return;
    if (androidui::locationEnabled()) return;
    m_session->warn(QStringLiteral("Location is switched off on this device: no position until it is on."));
    emit locationOff();
}

void MobileApp::reportBackgroundState()
{
    if (!m_config.ui.keepRunning) return;
    const QString status = androidui::keepAliveStatus();
    if (status.startsWith(QStringLiteral("running"))) {
        m_session->systemLine(QStringLiteral("Background service %1.").arg(status));
    } else if (status.startsWith(QStringLiteral("failed"))) {
        m_session->warn(QStringLiteral("Background service %1 - the modem and APRS-IS will stop when the screen goes off.").arg(status));
    } else if (status != QLatin1String("not on Android")) {
        m_session->warn(QStringLiteral("Background service %1.").arg(status));
    }
    if (!androidui::ignoringBatteryOptimizations()) {
        m_session->warn(QStringLiteral("Battery optimisation is on for this application: Android will cut the network "
                                       "when the phone is locked. Settings -> Interface -> Request exemption."));
        if (!m_config.ui.batteryPromptDone) emit batteryPromptNeeded();
    }
}

bool MobileApp::batteryExempt() const
{
    return androidui::ignoringBatteryOptimizations();
}

// Asked once on our initiative, whatever the answer; afterwards only from
// Settings.
void MobileApp::noteBatteryPromptDeclined()
{
    if (m_config.ui.batteryPromptDone) return;
    m_config.ui.batteryPromptDone = true;
    QString error;
    m_config.save(QString(), &error);
    emit configChanged();
}

void MobileApp::requestBatteryExemption()
{
    noteBatteryPromptDeclined();
    androidui::requestIgnoreBatteryOptimizations();
}

void MobileApp::quit()
{
    m_session->stopStation();
    androidui::stopKeepAlive();
    QCoreApplication::quit();
}

void MobileApp::openLocationSettings()
{
    androidui::openLocationSettings();
}

void MobileApp::testSound()
{
    m_notifier->test();
}

void MobileApp::copyLog()
{
    QGuiApplication::clipboard()->setText(m_log->plainText());
    m_session->systemLine(QStringLiteral("Conversation copied to the clipboard (%1 lines).").arg(m_log->rowCount()));
}

void MobileApp::onLogEntry(const QString &prefix, const QString &message, const QString &colourKey)
{
    const QString category = categoryFor(colourKey);
    if (!category.isEmpty()) {
        m_notifier->notify(category, prefix, message);
        emit attention(category, prefix);
    }
    const QString time = m_config.ui.showTimestamps ? QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")) : QString();
    m_log->append(time, prefix, message, colourFor(colourKey));
    if (qEnvironmentVariableIsSet("AX25CHAT_DUMP_LOG")) {
        QTextStream(stderr) << time << ' ' << prefix << ' ' << message << '\n';
    }
}

void MobileApp::refreshStations()
{
    const StationRegistry &registry = m_session->stations();
    m_stations->refresh(registry.listed(), registry.now(), m_session->referencePosition());
}
