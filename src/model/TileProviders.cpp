/*
 * TileProviders.cpp - the map tile servers both interfaces draw from.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TileProviders.h"

#include "AppConfig.h"

#include <QUrl>

namespace tiles {

const QList<Provider> &providers()
{
    static const QList<Provider> list = {
        {QStringLiteral("osm"), QStringLiteral("OpenStreetMap"),
         QStringLiteral("https://tile.openstreetmap.org/{z}/{x}/{y}.png"),
         QStringLiteral("Map data and tiles: OpenStreetMap contributors (ODbL)"), 19},
        {QStringLiteral("opentopo"), QStringLiteral("OpenTopoMap"),
         QStringLiteral("https://tile.opentopomap.org/{z}/{x}/{y}.png"),
         QStringLiteral("Map data: OpenStreetMap contributors, SRTM | Map style: OpenTopoMap (CC-BY-SA)"), 17},
        {QStringLiteral("ign_plan"), QStringLiteral("IGN Plan (France)"),
         QStringLiteral("https://data.geopf.fr/wmts?SERVICE=WMTS&VERSION=1.0.0&REQUEST=GetTile"
                        "&LAYER=GEOGRAPHICALGRIDSYSTEMS.PLANIGNV2&STYLE=normal&FORMAT=image/png"
                        "&TILEMATRIXSET=PM&TILEMATRIX={z}&TILEROW={y}&TILECOL={x}"),
         QStringLiteral("Plan IGN v2 - IGN / Geoplateforme"), 19},
        {QStringLiteral("ign_ortho"), QStringLiteral("IGN Orthophotos (France)"),
         QStringLiteral("https://data.geopf.fr/wmts?SERVICE=WMTS&VERSION=1.0.0&REQUEST=GetTile"
                        "&LAYER=ORTHOIMAGERY.ORTHOPHOTOS&STYLE=normal&FORMAT=image/jpeg"
                        "&TILEMATRIXSET=PM&TILEMATRIX={z}&TILEROW={y}&TILECOL={x}"),
         QStringLiteral("Orthophotos - IGN / Geoplateforme"), 19},
        {QStringLiteral("esri_topo"), QStringLiteral("Esri World Topo"),
         QStringLiteral("https://server.arcgisonline.com/ArcGIS/rest/services/World_Topo_Map/MapServer/tile/{z}/{y}/{x}"),
         QStringLiteral("Tiles: Esri, HERE, Garmin, FAO, NOAA, USGS, OpenStreetMap contributors"), 18},
        {QStringLiteral("esri_imagery"), QStringLiteral("Esri World Imagery"),
         QStringLiteral("https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}"),
         QStringLiteral("Imagery: Esri, Maxar, Earthstar Geographics, GIS User Community"), 18},
    };
    return list;
}

Provider current(const AppConfig &config)
{
    const UiConfig &ui = config.ui;
    if (ui.mapProvider == QLatin1String("custom") && !ui.mapCustomUrl.trimmed().isEmpty()) {
        Provider p;
        p.key = QStringLiteral("custom");
        p.name = QStringLiteral("Another tile server");
        p.urlTemplate = ui.mapCustomUrl.trimmed();
        p.attribution = QStringLiteral("Tiles: %1").arg(QUrl(p.urlTemplate).host());
        p.maxZoom = 19;
        return p;
    }
    for (const Provider &p : providers()) {
        if (p.key == ui.mapProvider) return p;
    }
    return providers().first();
}

int scale(const Provider &provider)
{
    return provider.urlTemplate.contains(QLatin1String("@2x")) ? 2 : 1;
}

QString url(const Provider &provider, int z, int x, int y)
{
    QString u = provider.urlTemplate;
    u.replace(QLatin1String("{z}"), QString::number(z));
    u.replace(QLatin1String("{x}"), QString::number(x));
    u.replace(QLatin1String("{y}"), QString::number(y));
    return u;
}

} // namespace tiles
