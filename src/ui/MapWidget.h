/*
 * MapWidget.h - the stations heard, on a map, for the desktop.
 *
 * A slippy map drawn from standard {z}/{x}/{y} tiles (Web Mercator,
 * 256 px): the visible tiles are fetched through the disk-cached network
 * manager (TileCache) and painted around the centre; the mouse drags and
 * the wheel zooms; each station with a position is a marker carrying its
 * APRS symbol, our own station has a halo. The same providers and the same
 * cache as the mobile map (TileProviders).
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "AppConfig.h"
#include "StationRegistry.h"
#include "TileProviders.h"

#include <QHash>
#include <QPixmap>
#include <QPointer>
#include <QSet>
#include <QWidget>

#include <optional>

class CachingNetworkAccessManager;
class QNetworkReply;
class QToolButton;

class MapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MapWidget(const AppConfig &config, QWidget *parent = nullptr);

    void reloadConfig(const AppConfig &config);

    // What to draw: the stations (with their positions) and our own.
    void setStations(const QList<Station> &stations);
    void setOwnPosition(std::optional<std::pair<double, double>> position, const QString &callsign,
                        const QString &symbolTable, const QString &symbolCode);

    void centerOn(double latitude, double longitude, int zoom = -1);
    void centerOnOwn();
    void fitStations();
    void zoomBy(int levels);

signals:
    // The user clicked a station's marker.
    void stationClicked(const QString &callsign);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool event(QEvent *event) override;

private:
    struct Tile { int z, x, y; bool operator==(const Tile &o) const { return z == o.z && x == o.x && y == o.y; } };
    friend size_t qHash(const Tile &t, size_t seed) { return qHash(QStringLiteral("%1/%2/%3").arg(t.z).arg(t.x).arg(t.y), seed); }

    // Web Mercator in tile units
    static double lonToX(double lon, int z);
    static double latToY(double lat, int z);
    static double xToLon(double x, int z);
    static double yToLat(double y, int z);
    QPointF toScreen(double latitude, double longitude) const;
    void zoomAt(int levels, const QPointF &at);
    void panBy(const QPointF &delta);
    void requestTile(const Tile &tile);
    void onTileReady(const Tile &tile, QNetworkReply *reply);
    void placeButtons();
    int tileZoom() const;
    double drawSize() const;
    QString providerKey() const;

    AppConfig m_config;
    tiles::Provider m_provider;
    CachingNetworkAccessManager *m_network = nullptr;
    QHash<Tile, QPixmap> m_tiles;              // loaded
    QSet<Tile> m_pending;                      // requested, not yet here
    QList<Tile> m_order;                       // for eviction, oldest first
    double m_centerLat = 46.5;
    double m_centerLon = 2.5;
    int m_zoom = 6;
    bool m_dragging = false;
    QPointF m_dragLast;
    QPointF m_pressAt;

    QList<Station> m_stations;
    std::optional<std::pair<double, double>> m_own;
    QString m_ownCall, m_ownTable, m_ownCode;
    QToolButton *m_zoomIn = nullptr;
    QToolButton *m_zoomOut = nullptr;
    QToolButton *m_centerButton = nullptr;
    QToolButton *m_fitButton = nullptr;
};
