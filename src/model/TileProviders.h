/*
 * TileProviders.h - the map tile servers both interfaces draw from.
 *
 * Servers that need no key or account: OpenStreetMap, OpenTopoMap, IGN's
 * Plan and orthophotos (its open "découverte" services, France), Esri's
 * World Topo and World Imagery, or a {z}/{x}/{y} template of the user's
 * own. Each comes with the credit its terms require, its deepest zoom
 * level, and whether its tiles are drawn at twice the resolution (@2x).
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QList>
#include <QString>

struct AppConfig;

namespace tiles {

struct Provider {
    QString key;           // what the configuration stores
    QString name;          // what the settings show
    QString urlTemplate;   // with {z}, {x}, {y}
    QString attribution;
    int maxZoom = 19;
};

// The built-in providers, in the order the settings list them; "custom" is
// not among them.
const QList<Provider> &providers();

// The provider the configuration selects, the custom template filled in
// when that is the choice.
Provider current(const AppConfig &config);

// 2 when the template produces 512 px tiles (an "@2x" in it), else 1.
int scale(const Provider &provider);

// The tile URL for z/x/y.
QString url(const Provider &provider, int z, int x, int y);

} // namespace tiles
