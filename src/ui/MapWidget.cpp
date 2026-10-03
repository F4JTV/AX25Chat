/*
 * MapWidget.cpp - the stations heard, on a map, for the desktop.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MapWidget.h"

#include "Style.h"
#include "SymbolArt.h"
#include "TileCache.h"

#include <QHelpEvent>
#include <QMouseEvent>
#include <QNetworkReply>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>
#include <QToolTip>
#include <QWheelEvent>

#include <cmath>

namespace {
// MSVC's <cmath> has no Pi without _USE_MATH_DEFINES; a constant of our
// own costs nothing and builds everywhere.
constexpr double Pi = 3.14159265358979323846;
constexpr int TileSize = 256;
constexpr int MinZoom = 2;
constexpr int MaxZoom = 18;
constexpr int MaxLoadedTiles = 600;
}

MapWidget::MapWidget(const AppConfig &config, QWidget *parent)
    : QWidget(parent), m_config(config), m_provider(tiles::current(config))
{
    setMouseTracking(true);
    setMinimumSize(200, 200);
    m_network = new CachingNetworkAccessManager(qint64(config.ui.mapCacheMb) * 1024 * 1024, this);

    auto button = [this](const QString &text, const QString &tip, auto slot) {
        auto *b = new QToolButton(this);
        b->setText(text);
        b->setToolTip(tip);
        b->setFixedSize(30, 30);
        b->setAutoRaise(false);
        connect(b, &QToolButton::clicked, this, slot);
        return b;
    };
    m_zoomIn = button(QStringLiteral("+"), QStringLiteral("Zoom in"), [this] { zoomBy(1); });
    m_zoomOut = button(QStringLiteral("\u2212"), QStringLiteral("Zoom out"), [this] { zoomBy(-1); });
    m_centerButton = button(QStringLiteral("\u25CE"), QStringLiteral("Centre on my station"), [this] { centerOnOwn(); });
    m_fitButton = button(QStringLiteral("\u2B1A"), QStringLiteral("Fit every station"), [this] { fitStations(); });
    placeButtons();
}

void MapWidget::reloadConfig(const AppConfig &config)
{
    const bool providerChanged = tiles::current(config).urlTemplate != m_provider.urlTemplate;
    m_config = config;
    m_provider = tiles::current(config);
    if (providerChanged) {
        m_tiles.clear();
        m_pending.clear();
        m_order.clear();
    }
    update();
}

// ---- projection ----------------------------------------------------------------------

double MapWidget::lonToX(double lon, int z) { return (lon + 180.0) / 360.0 * std::pow(2.0, z); }
double MapWidget::latToY(double lat, int z)
{
    const double r = lat * Pi / 180.0;
    return (1.0 - std::log(std::tan(r) + 1.0 / std::cos(r)) / Pi) / 2.0 * std::pow(2.0, z);
}
double MapWidget::xToLon(double x, int z) { return x / std::pow(2.0, z) * 360.0 - 180.0; }
double MapWidget::yToLat(double y, int z)
{
    const double n = Pi - 2.0 * Pi * y / std::pow(2.0, z);
    return 180.0 / Pi * std::atan(0.5 * (std::exp(n) - std::exp(-n)));
}

// Sharpness on a high-density screen: plain tiles are fetched one level
// deeper and drawn at half size (the same choice as the mobile map).
int MapWidget::tileZoom() const
{
    const int scale = tiles::scale(m_provider);
    const bool sharp = m_config.ui.mapSharp && scale == 1 && devicePixelRatioF() >= 1.5 && m_zoom + 1 <= m_provider.maxZoom;
    return sharp ? m_zoom + 1 : m_zoom;
}

double MapWidget::drawSize() const
{
    return TileSize / std::pow(2.0, tileZoom() - m_zoom);
}

QPointF MapWidget::toScreen(double latitude, double longitude) const
{
    const double sx = (lonToX(longitude, m_zoom) - lonToX(m_centerLon, m_zoom)) * TileSize + width() / 2.0;
    const double sy = (latToY(latitude, m_zoom) - latToY(m_centerLat, m_zoom)) * TileSize + height() / 2.0;
    return {sx, sy};
}

// ---- view --------------------------------------------------------------------------------

void MapWidget::panBy(const QPointF &delta)
{
    const double n = std::pow(2.0, m_zoom);
    double cx = lonToX(m_centerLon, m_zoom) - delta.x() / TileSize;
    double cy = latToY(m_centerLat, m_zoom) - delta.y() / TileSize;
    cy = std::max(0.0, std::min(n, cy));
    cx = std::fmod(std::fmod(cx, n) + n, n);
    m_centerLon = xToLon(cx, m_zoom);
    m_centerLat = yToLat(cy, m_zoom);
    update();
}

// Zoom by whole levels, the point under `at` staying put.
void MapWidget::zoomAt(int levels, const QPointF &at)
{
    const int maxZoom = std::min(MaxZoom, m_provider.maxZoom);
    const int newZoom = std::max(MinZoom, std::min(maxZoom, m_zoom + levels));
    if (newZoom == m_zoom) return;
    const double lon = xToLon(lonToX(m_centerLon, m_zoom) + (at.x() - width() / 2.0) / TileSize, m_zoom);
    const double lat = yToLat(latToY(m_centerLat, m_zoom) + (at.y() - height() / 2.0) / TileSize, m_zoom);
    m_zoom = newZoom;
    const double cx = lonToX(lon, m_zoom) - (at.x() - width() / 2.0) / TileSize;
    const double cy = latToY(lat, m_zoom) - (at.y() - height() / 2.0) / TileSize;
    m_centerLon = xToLon(cx, m_zoom);
    m_centerLat = yToLat(cy, m_zoom);
    update();
}

void MapWidget::zoomBy(int levels)
{
    zoomAt(levels, QPointF(width() / 2.0, height() / 2.0));
}

void MapWidget::centerOn(double latitude, double longitude, int zoom)
{
    m_centerLat = latitude;
    m_centerLon = longitude;
    if (zoom >= 0) m_zoom = std::max(MinZoom, std::min(std::min(MaxZoom, m_provider.maxZoom), zoom));
    update();
}

void MapWidget::centerOnOwn()
{
    if (!m_own) return;
    centerOn(m_own->first, m_own->second, m_zoom < 10 ? 11 : -1);
}

// Every station with a position, and us: all in view.
void MapWidget::fitStations()
{
    double minLat = 90, maxLat = -90, minLon = 180, maxLon = -180;
    int count = 0;
    auto take = [&](double lat, double lon) {
        minLat = std::min(minLat, lat); maxLat = std::max(maxLat, lat);
        minLon = std::min(minLon, lon); maxLon = std::max(maxLon, lon);
        count++;
    };
    if (m_own) take(m_own->first, m_own->second);
    for (const Station &s : m_stations) if (s.hasPosition()) take(*s.latitude, *s.longitude);
    if (count == 0) return;
    m_centerLat = (minLat + maxLat) / 2.0;
    m_centerLon = (minLon + maxLon) / 2.0;
    int z = std::min(MaxZoom, m_provider.maxZoom);
    while (z > MinZoom) {
        const double w = std::fabs(lonToX(maxLon, z) - lonToX(minLon, z)) * TileSize;
        const double h = std::fabs(latToY(maxLat, z) - latToY(minLat, z)) * TileSize;
        if (w < width() * 0.8 && h < height() * 0.8) break;
        z--;
    }
    m_zoom = std::min(z, 15);
    update();
}

// ---- data ---------------------------------------------------------------------------------

void MapWidget::setStations(const QList<Station> &stations)
{
    m_stations = stations;
    update();
}

void MapWidget::setOwnPosition(std::optional<std::pair<double, double>> position, const QString &callsign,
                               const QString &symbolTable, const QString &symbolCode)
{
    const bool first = !m_own && position;
    m_own = position;
    m_ownCall = callsign;
    m_ownTable = symbolTable;
    m_ownCode = symbolCode;
    if (first) centerOn(position->first, position->second, 10);
    update();
}

// ---- tiles --------------------------------------------------------------------------------

void MapWidget::requestTile(const Tile &tile)
{
    if (m_tiles.contains(tile) || m_pending.contains(tile)) return;
    m_pending.insert(tile);
    QNetworkReply *reply = m_network->get(QNetworkRequest(QUrl(tiles::url(m_provider, tile.z, tile.x, tile.y))));
    connect(reply, &QNetworkReply::finished, this, [this, tile, reply] { onTileReady(tile, reply); });
}

void MapWidget::onTileReady(const Tile &tile, QNetworkReply *reply)
{
    reply->deleteLater();
    m_pending.remove(tile);
    if (reply->error() != QNetworkReply::NoError) return;
    QPixmap pixmap;
    if (!pixmap.loadFromData(reply->readAll())) return;
    m_tiles.insert(tile, pixmap);
    m_order.append(tile);
    while (m_order.size() > MaxLoadedTiles) {
        m_tiles.remove(m_order.takeFirst());
    }
    update();
}

// ---- painting -----------------------------------------------------------------------------

void MapWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), palette().color(QPalette::Base));
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // Tiles at the tile zoom, around the centre.
    const int tz = tileZoom();
    const double size = drawSize();
    const double n = std::pow(2.0, tz);
    const double cx = lonToX(m_centerLon, tz), cy = latToY(m_centerLat, tz);
    const double halfW = width() / 2.0 / size, halfH = height() / 2.0 / size;
    const int x0 = static_cast<int>(std::floor(cx - halfW)), x1 = static_cast<int>(std::floor(cx + halfW));
    const int y0 = std::max(0, static_cast<int>(std::floor(cy - halfH)));
    const int y1 = std::min(static_cast<int>(n) - 1, static_cast<int>(std::floor(cy + halfH)));
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            const int wx = static_cast<int>(std::fmod(std::fmod(x, n) + n, n));
            const Tile tile{tz, wx, y};
            const QRectF target((x - cx) * size + width() / 2.0, (y - cy) * size + height() / 2.0, size, size);
            auto it = m_tiles.constFind(tile);
            if (it != m_tiles.constEnd()) {
                p.drawPixmap(target, it.value(), it.value().rect());
            } else {
                requestTile(tile);
            }
        }
    }

    const QHash<QString, QString> colours = style::colours(palette());
    QFont smallFont = font();     // not "small": a macro in Windows' RPC headers
    smallFont.setPointSizeF(std::max(7.0, font().pointSizeF() - 1));
    p.setFont(smallFont);

    auto label = [&](const QPointF &at, const QString &text, const QColor &colour) {
        const QRectF box = p.fontMetrics().boundingRect(text).adjusted(-3, -1, 3, 1).translated(at.x() + 16, at.y() - p.fontMetrics().height() / 2.0);
        QColor back = palette().color(QPalette::Base);
        back.setAlpha(190);
        p.setPen(Qt::NoPen);
        p.setBrush(back);
        p.drawRoundedRect(box, 3, 3);
        p.setPen(colour);
        p.drawText(box, Qt::AlignCenter, text);
    };

    // The stations heard.
    for (const Station &s : m_stations) {
        if (!s.hasPosition()) continue;
        const QPointF at = toScreen(*s.latitude, *s.longitude);
        if (!rect().adjusted(-40, -40, 120, 40).contains(at.toPoint())) continue;
        const QPixmap art = symbolart::available() ? symbolart::pixmap(s.symbolTable, s.symbolCode, 28) : QPixmap();
        if (!art.isNull()) {
            p.drawPixmap(QRectF(at.x() - 14, at.y() - 14, 28, 28), art, art.rect());
        } else {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(colours.value(QStringLiteral("rx"))));
            p.drawEllipse(at, 6, 6);
        }
        label(at, s.callsign + (s.viaInternet ? QStringLiteral(" [IS]") : QString()),
              QColor(colours.value(s.viaInternet ? QStringLiteral("monitor") : QStringLiteral("rx"))));
    }

    // Our own station, with a halo.
    if (m_own) {
        const QPointF at = toScreen(m_own->first, m_own->second);
        QColor halo(colours.value(QStringLiteral("tx")));
        halo.setAlpha(60);
        p.setPen(Qt::NoPen);
        p.setBrush(halo);
        p.drawEllipse(at, 22, 22);
        const QPixmap art = symbolart::available() ? symbolart::pixmap(m_ownTable, m_ownCode, 32) : QPixmap();
        if (!art.isNull()) {
            p.drawPixmap(QRectF(at.x() - 16, at.y() - 16, 32, 32), art, art.rect());
        } else {
            p.setBrush(QColor(colours.value(QStringLiteral("tx"))));
            p.drawEllipse(at, 7, 7);
        }
        label(at, m_ownCall, QColor(colours.value(QStringLiteral("tx"))));
    }

    // Zoom and centre, top left; the provider's credit, bottom right.
    p.setPen(palette().color(QPalette::Text));
    const QString where = QStringLiteral("z%1  %2, %3").arg(m_zoom).arg(m_centerLat, 0, 'f', 3).arg(m_centerLon, 0, 'f', 3);
    QColor back = palette().color(QPalette::Base);
    back.setAlpha(190);
    auto box = [&](const QString &text, Qt::Alignment where) {
        QRectF r = p.fontMetrics().boundingRect(text).adjusted(-4, -2, 4, 2);
        if (where & Qt::AlignLeft) r.moveTopLeft(QPointF(6, 6));
        else r.moveBottomRight(QPointF(width() - 6, height() - 6));
        p.setPen(Qt::NoPen);
        p.setBrush(back);
        p.drawRoundedRect(r, 3, 3);
        p.setPen(palette().color(QPalette::Text));
        p.drawText(r, Qt::AlignCenter, text);
    };
    box(where, Qt::AlignLeft);
    box(m_provider.attribution, Qt::AlignRight);
}

// ---- interaction ------------------------------------------------------------------------

void MapWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragLast = event->position();
        m_pressAt = event->position();
        setCursor(Qt::ClosedHandCursor);
    }
}

void MapWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging) return;
    panBy(event->position() - m_dragLast);
    m_dragLast = event->position();
}

void MapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    m_dragging = false;
    setCursor(Qt::ArrowCursor);
    // A click (no drag) on a marker.
    if ((event->position() - m_pressAt).manhattanLength() < 4) {
        for (const Station &s : m_stations) {
            if (!s.hasPosition()) continue;
            if ((toScreen(*s.latitude, *s.longitude) - event->position()).manhattanLength() < 18) {
                emit stationClicked(s.callsign);
                return;
            }
        }
    }
}

void MapWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    zoomAt(1, event->position());
}

void MapWidget::wheelEvent(QWheelEvent *event)
{
    zoomAt(event->angleDelta().y() > 0 ? 1 : -1, event->position());
}

void MapWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    placeButtons();
}

void MapWidget::placeButtons()
{
    int y = 8;
    for (QToolButton *b : {m_zoomIn, m_zoomOut, m_centerButton, m_fitButton}) {
        b->move(width() - b->width() - 8, y);
        y += b->height() + 6;
    }
}

// A tooltip with the station's details under the cursor.
bool MapWidget::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        for (const Station &s : m_stations) {
            if (!s.hasPosition()) continue;
            if ((toScreen(*s.latitude, *s.longitude) - QPointF(help->pos())).manhattanLength() < 18) {
                QToolTip::showText(help->globalPos(), s.tooltip(), this);
                return true;
            }
        }
        QToolTip::hideText();
        return true;
    }
    return QWidget::event(event);
}
