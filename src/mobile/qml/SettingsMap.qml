// SettingsMap.qml - the map's tiles and their cache.
//
// This file is part of AX25Chat.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    property var owner
    readonly property var cfg: owner ? owner.cfg : ({})
    spacing: 8
    Field { label: "Tiles"; ComboBox {
        width: parent.width
        readonly property var providers: app.tileProviders()
        model: providers
        textRole: "name"
        currentIndex: { if (!cfg.ui) return 0; for (var i = 0; i < providers.length; i++) if (providers[i].key === cfg.ui.map_provider) return i; return 0 }
        onActivated: { cfg.ui.map_provider = providers[currentIndex].key; owner.refresh() } } }
    Field { label: "Tile URL"; TextField { width: parent.width; enabled: cfg.ui ? cfg.ui.map_provider === "custom" : false; text: cfg.ui ? cfg.ui.map_custom_url : ""; placeholderText: "https://host/{z}/{x}/{y}.png"; inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase; onEditingFinished: cfg.ui.map_custom_url = text } }
    Note { text: "Tile servers that need no key or account (a server of your own must be https: Android refuses plain http). IGN's Plan and orthophotos cover France. Tiles are kept on disk once downloaded, so an area seen once stays available without a connection." }
    Field { label: "Sharp plain tiles"; Switch { checked: cfg.ui ? cfg.ui.map_sharp : true; onToggled: cfg.ui.map_sharp = checked } }
    Note { text: "On a high-density screen: the next zoom level is fetched and drawn at half size, crisp but with the map's own labels half as big, and four times the tiles." }
    Field { label: "Cache size (MB)"; SpinBox { from: 20; to: 5000; stepSize: 50; editable: true; value: cfg.ui ? cfg.ui.map_cache_mb : 200
        textFromValue: function(v) { return String(v) }; valueFromText: function(t) { return parseInt(t) || 200 }
        onValueModified: cfg.ui.map_cache_mb = value } }
    RowLayout {
        Layout.fillWidth: true
        Label { id: cacheLabel; text: "On disk: " + app.tileCacheMb().toFixed(1) + " MB"; font.pixelSize: 12; color: window.colourOf("dim"); Layout.fillWidth: true }
        Button { text: "Clear tiles"; Material.elevation: window.elev(2); onClicked: { app.clearTileCache(); cacheLabel.text = "On disk: 0.0 MB" } }
    }
}
