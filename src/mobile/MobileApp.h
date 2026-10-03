/*
 * MobileApp.h - what the Qt Quick interface binds to.
 *
 * One object exposed to QML as "app": the Session, a model of the
 * conversation, a model of the stations heard, the configuration as a
 * JavaScript-friendly map, and the few actions the pages need. The
 * protocol logic stays in Session; this is glue.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "AppConfig.h"
#include "Session.h"
#include "StationRegistry.h"

#include <QAbstractListModel>
#include <QObject>
#include <QQuickImageProvider>
#include <QStringList>
#include <QVariantMap>

class Notifier;

// The conversation: one entry per line, newest last.
class LogModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles { TimeRole = Qt::UserRole + 1, PrefixRole, MessageRole, ColourRole };
    explicit LogModel(int maxLines, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void append(const QString &time, const QString &prefix, const QString &message, const QString &colour);
    Q_INVOKABLE void clear();
    Q_INVOKABLE QString plainText() const;
    void setMaxLines(int lines) { m_maxLines = lines; }

private:
    struct Entry { QString time, prefix, message, colour; };
    QList<Entry> m_entries;
    int m_maxLines;
};

// Stations heard, newest first.
class StationModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Roles { CallsignRole = Qt::UserRole + 1, LineRole, TableRole, CodeRole, HasPositionRole, TooltipRole,
                 LatitudeRole, LongitudeRole, InternetRole };
    explicit StationModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void refresh(const QList<Station> &stations, double now, std::optional<std::pair<double, double>> reference);

signals:
    void countChanged();

private:
    QList<Station> m_stations;
    QStringList m_lines;
    QStringList m_tooltips;
};

// image://symbols/<table><code> for the station list and the picker.
class SymbolImageProvider : public QQuickImageProvider
{
public:
    SymbolImageProvider();
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
};


class MobileApp : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Session *session READ session CONSTANT)
    Q_PROPERTY(LogModel *log READ log CONSTANT)
    Q_PROPERTY(StationModel *stations READ stations CONSTANT)
    Q_PROPERTY(QVariantMap config READ configMap NOTIFY configChanged)
    Q_PROPERTY(QStringList queue READ queue NOTIFY queueChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool symbolsAvailable READ symbolsAvailable CONSTANT)
    Q_PROPERTY(QString gpsKind READ gpsKind CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    // True when the scene graph cannot draw shadows (the software renderer
    // used for headless screenshots): the interface then stays flat.
    Q_PROPERTY(bool flatRendering READ flatRendering CONSTANT)

public:
    explicit MobileApp(const AppConfig &config, QObject *parent = nullptr);

    Session *session() const { return m_session; }
    LogModel *log() const { return m_log; }
    StationModel *stations() const { return m_stations; }
    QVariantMap configMap() const;
    QStringList queue() const;
    QString version() const { return Session::version(); }
    bool symbolsAvailable() const;
    QString gpsKind() const;
    QString lastError() const { return m_lastError; }
    bool flatRendering() const;

    // Validate, save to disk, regenerate direwolf.conf, apply.  False with
    // lastError set when the configuration was refused.
    Q_INVOKABLE bool saveConfig(const QVariantMap &map);
    Q_INVOKABLE bool send(const QString &recipient, const QString &text);
    Q_INVOKABLE QString symbolDescription(const QString &table, const QString &code) const;
    Q_INVOKABLE QVariantList commonSymbols() const;
    Q_INVOKABLE QVariantList symbolTable(const QString &table) const;
    // The image:// URL of a symbol for the views (see SymbolImageProvider).
    Q_INVOKABLE QString symbolImage(const QString &table, const QString &code) const;
    Q_INVOKABLE QVariantMap locatorToLatLon(const QString &locator) const;
    // The APRS-IS passcode for a callsign, -1 when it is not one.
    Q_INVOKABLE int passcodeFor(const QString &callsign) const;
    Q_INVOKABLE void testSound();
    // The map: our own position (latitude, longitude, valid), the tile URL
    // template for the configured provider, its attribution, the cache.
    Q_INVOKABLE QVariantMap ownPosition() const;
    Q_INVOKABLE QString tileUrlTemplate() const;
    Q_INVOKABLE QString tileAttribution() const;
    // 2 for providers whose tiles are 512 px for the same area (@2x), else 1.
    Q_INVOKABLE int tileScale() const;
    Q_INVOKABLE int tileMaxZoom() const;
    Q_INVOKABLE QVariantList tileProviders() const;
    Q_INVOKABLE double tileCacheMb() const;
    Q_INVOKABLE void clearTileCache();
    // Location switched off at device level while the position comes from
    // it: ask (locationOff signal) and open the settings on request.
    Q_INVOKABLE void checkLocationService();
    Q_INVOKABLE void openLocationSettings();
    // Background operation: the service's state a moment after its start,
    // and the Doze exemption (batteryPromptNeeded signal once; the request
    // opens the system's dialog).
    Q_INVOKABLE void reportBackgroundState();
    Q_INVOKABLE bool batteryExempt() const;
    Q_INVOKABLE void requestBatteryExemption();
    Q_INVOKABLE void noteBatteryPromptDeclined();
    // Stop the station and the background service, then leave.
    Q_INVOKABLE void quit();
    // The whole conversation to the clipboard, to paste into a report.
    Q_INVOKABLE void copyLog();
    Q_INVOKABLE void noteFirstRun(const QString &confPath) { m_session->noteFirstRun(confPath); }
    // The bundled Dire Wolf data files written to the application's data
    // folder; that folder, to be made the working directory.  Static: it
    // runs before the session, whose modem needs the files.
    static QString prepareDataFolder();

signals:
    void configChanged();
    void queueChanged();
    void lastErrorChanged();
    // Something arrived that deserves the operator's attention.
    void attention(const QString &category, const QString &prefix);
    void showSettings();
    void locationOff();
    void batteryPromptNeeded();

private:
    void onLogEntry(const QString &prefix, const QString &message, const QString &colourKey);
    void refreshStations();
    QString generatedConfPath() const;

    AppConfig m_config;
    Session *m_session = nullptr;
    LogModel *m_log = nullptr;
    StationModel *m_stations = nullptr;
    Notifier *m_notifier = nullptr;
    QString m_lastError;
};
