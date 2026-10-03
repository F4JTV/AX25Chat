// MapPage.qml - the stations heard, on a map.
//
// A slippy map drawn from standard {z}/{x}/{y} tiles (Web Mercator, 256 px):
// the visible tiles are placed around the centre, pan and pinch move and
// zoom, and each station with a position is a marker carrying its APRS
// symbol.  Tiles go through the engine's network manager, which keeps them
// on disk (TileCache.cpp), so an area seen once stays available offline.
//
// This file is part of AX25Chat.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

Page {
    id: page
    title: "Map"
    property string pageName: "map"
    signal stationChosen(string callsign)

    // ---- view state ------------------------------------------------------
    property real centerLat: 46.5
    property real centerLon: 2.5
    property int zoom: 6
    readonly property int minZoom: 2
    readonly property int maxZoom: Math.min(18, app.tileMaxZoom())
    readonly property int tileSize: 256
    readonly property string urlTemplate: app.tileUrlTemplate()

    // Sharpness on a high-density screen.  A 256 px tile drawn over 256
    // device-independent pixels is stretched across two or three physical
    // pixels each, and looks blurred.  Two remedies: providers whose tiles
    // are 512 px for the same area (@2x, tileScale 2) are drawn at 256 dp
    // and land crisp; for plain 256 px tiles, "sharp" fetches the next zoom
    // level and draws each tile at 128 dp (four times the tiles, the map's
    // own labels half the size).
    readonly property int tileScale: app.tileScale()
    readonly property real dpr: Screen.devicePixelRatio
    readonly property bool sharp: app.config.ui && app.config.ui.map_sharp !== undefined ? app.config.ui.map_sharp : true
    readonly property int overZoom: (sharp && tileScale === 1 && dpr >= 1.5 && zoom + 1 <= app.tileMaxZoom()) ? 1 : 0
    readonly property int tileZoom: zoom + overZoom
    readonly property real drawSize: tileSize / Math.pow(2, overZoom)

    // ---- Web Mercator, in tile units at the current zoom --------------------
    function lonToX(lon, z) { return (lon + 180) / 360 * Math.pow(2, z) }
    function latToY(lat, z) {
        var r = lat * Math.PI / 180
        return (1 - Math.log(Math.tan(r) + 1 / Math.cos(r)) / Math.PI) / 2 * Math.pow(2, z)
    }
    function xToLon(x, z) { return x / Math.pow(2, z) * 360 - 180 }
    function yToLat(y, z) {
        var n = Math.PI - 2 * Math.PI * y / Math.pow(2, z)
        return 180 / Math.PI * Math.atan(0.5 * (Math.exp(n) - Math.exp(-n)))
    }
    // Screen position of a coordinate, relative to the map area.
    function toScreenX(lon) { return (lonToX(lon, zoom) - lonToX(centerLon, zoom)) * tileSize + mapArea.width / 2 }
    function toScreenY(lat) { return (latToY(lat, zoom) - latToY(centerLat, zoom)) * tileSize + mapArea.height / 2 }

    function panBy(dx, dy) {
        var n = Math.pow(2, zoom)
        var cx = lonToX(centerLon, zoom) - dx / tileSize
        var cy = latToY(centerLat, zoom) - dy / tileSize
        cy = Math.max(0, Math.min(n, cy))
        centerLon = xToLon(((cx % n) + n) % n, zoom)
        centerLat = yToLat(cy, zoom)
    }
    // Zoom by a whole number of levels, keeping the point under (px, py) still.
    function zoomAt(delta, px, py) {
        var newZoom = Math.max(minZoom, Math.min(maxZoom, zoom + delta))
        if (newZoom === zoom) return
        var lon = xToLon(lonToX(centerLon, zoom) + (px - mapArea.width / 2) / tileSize, zoom)
        var lat = yToLat(latToY(centerLat, zoom) + (py - mapArea.height / 2) / tileSize, zoom)
        zoom = newZoom
        var cx = lonToX(lon, zoom) - (px - mapArea.width / 2) / tileSize
        var cy = latToY(lat, zoom) - (py - mapArea.height / 2) / tileSize
        centerLon = xToLon(cx, zoom)
        centerLat = yToLat(cy, zoom)
    }
    function centerOnMe() {
        var me = app.ownPosition()
        if (!me.valid) { app.session.warn("No position of our own to centre on: no GPS fix and no fixed coordinates."); return }
        centerLat = me.latitude; centerLon = me.longitude
        if (zoom < 10) zoom = 11
    }
    function centerOnStations() {
        // The stations with a position, and us: fit them all.
        var minLat = 90, maxLat = -90, minLon = 180, maxLon = -180, count = 0
        var me = app.ownPosition()
        if (me.valid) { minLat = maxLat = me.latitude; minLon = maxLon = me.longitude; count = 1 }
        for (var i = 0; i < stationMarkers.count; i++) {
            var m = stationMarkers.itemAt(i)
            if (!m || !m.valid) continue
            minLat = Math.min(minLat, m.lat); maxLat = Math.max(maxLat, m.lat)
            minLon = Math.min(minLon, m.lon); maxLon = Math.max(maxLon, m.lon); count++
        }
        if (count === 0) return
        centerLat = (minLat + maxLat) / 2; centerLon = (minLon + maxLon) / 2
        var z = maxZoom
        while (z > minZoom) {
            var w = (lonToX(maxLon, z) - lonToX(minLon, z)) * tileSize
            var h = (latToY(maxLat, z) - latToY(minLat, z)) * tileSize
            if (Math.abs(w) < mapArea.width * 0.8 && Math.abs(h) < mapArea.height * 0.8) break
            z--
        }
        zoom = Math.min(z, 15)
    }

    Component.onCompleted: {
        var me = app.ownPosition()
        if (me.valid) { centerLat = me.latitude; centerLon = me.longitude; zoom = 10 }
        else centerOnStations()
    }

    // ---- the visible tiles, at the tile zoom (see overZoom) -------------------
    readonly property var visibleTiles: {
        var n = Math.pow(2, tileZoom)
        var cx = lonToX(centerLon, tileZoom), cy = latToY(centerLat, tileZoom)
        var halfW = mapArea.width / 2 / drawSize, halfH = mapArea.height / 2 / drawSize
        var x0 = Math.floor(cx - halfW), x1 = Math.floor(cx + halfW)
        var y0 = Math.max(0, Math.floor(cy - halfH)), y1 = Math.min(n - 1, Math.floor(cy + halfH))
        var list = []
        for (var y = y0; y <= y1; y++) {
            for (var x = x0; x <= x1; x++) {
                var wx = ((x % n) + n) % n     // wrap around the date line
                list.push({ x: x, y: y, wx: wx, z: tileZoom })
            }
        }
        return list
    }
    function tileUrl(t) { return urlTemplate.replace("{z}", t.z).replace("{x}", t.wx).replace("{y}", t.y) }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            id: mapArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Rectangle { anchors.fill: parent; color: window.dark ? "#1b2126" : "#dfe6ea" }

            // Tiles: only the images move, the page's centre stays put.
            Item {
                id: tileLayer
                width: parent.width; height: parent.height
                transformOrigin: Item.TopLeft
                Repeater {
                    model: page.visibleTiles
                    Image {
                        x: (modelData.x - page.lonToX(page.centerLon, page.tileZoom)) * page.drawSize + mapArea.width / 2
                        y: (modelData.y - page.latToY(page.centerLat, page.tileZoom)) * page.drawSize + mapArea.height / 2
                        width: page.drawSize; height: page.drawSize
                        source: page.tileUrl(modelData)
                        asynchronous: true
                        cache: true
                        // Interpolated, never nearest-neighbour: the tile is
                        // resampled to the screen's density either way.
                        smooth: true
                        mipmap: page.tileScale > 1
                    }
                }
            }

            // Our own station.
            Item {
                id: ownMarker
                property var me: app.ownPosition()
                visible: me.valid
                x: visible ? page.toScreenX(me.longitude) : 0
                y: visible ? page.toScreenY(me.latitude) : 0
                Connections { target: app.session; function onPanelChanged() { ownMarker.me = app.ownPosition() } }
                Rectangle { x: -22; y: -22; width: 44; height: 44; radius: 22; color: window.colourOf("tx"); opacity: 0.25 }
                Image {
                    x: -16; y: -16; width: 32; height: 32
                    source: ownMarker.visible && app.symbolsAvailable ? app.symbolImage(ownMarker.me.table, ownMarker.me.code) : ""
                    sourceSize.width: 32; sourceSize.height: 32
                }
                Label { x: 18; y: -8; text: app.session.myCall; font.bold: true; font.pixelSize: 12; color: window.colourOf("tx");
                        background: Rectangle { color: page.Material.background; opacity: 0.7; radius: 3 } }
            }

            // The stations heard, with their symbol.
            Repeater {
                id: stationMarkers
                model: app.stations
                Item {
                    property bool valid: model.hasPosition
                    property real lat: model.latitude
                    property real lon: model.longitude
                    visible: valid
                    x: valid ? page.toScreenX(lon) : 0
                    y: valid ? page.toScreenY(lat) : 0
                    Image {
                        x: -14; y: -14; width: 28; height: 28
                        source: app.symbolsAvailable ? app.symbolImage(model.table, model.code) : ""
                        sourceSize.width: 28; sourceSize.height: 28
                    }
                    Rectangle { visible: !app.symbolsAvailable; x: -6; y: -6; width: 12; height: 12; radius: 6; color: window.colourOf("rx") }
                    Label {
                        x: 16; y: -8
                        text: model.callsign + (model.internet ? " [IS]" : "")
                        font.pixelSize: 11
                        color: model.internet ? window.colourOf("monitor") : window.colourOf("rx")
                        background: Rectangle { color: page.Material.background; opacity: 0.7; radius: 3 }
                    }
                    TapHandler {
                        onTapped: { detail.stationCall = model.callsign; detail.text = model.tooltip; detail.open() }
                    }
                }
            }

            // ---- interaction
            DragHandler {
                target: null
                onTranslationChanged: {
                    page.panBy(translation.x - lastX, translation.y - lastY)
                    lastX = translation.x; lastY = translation.y
                }
                onActiveChanged: { lastX = 0; lastY = 0 }
                property real lastX: 0
                property real lastY: 0
            }
            PinchHandler {
                id: pinch
                target: null
                onActiveChanged: {
                    if (active) return
                    // Fingers lifted: snap to whole levels around the pinch centre.
                    var levels = Math.round(Math.log(activeScale) / Math.LN2)
                    tileLayer.scale = 1; tileLayer.x = 0; tileLayer.y = 0
                    if (levels !== 0) page.zoomAt(levels, centroid.position.x, centroid.position.y)
                }
                onActiveScaleChanged: {
                    if (!active) return
                    // Live feedback while pinching: the tile layer scales
                    // about the centroid until the fingers lift.
                    var s = activeScale
                    tileLayer.scale = s
                    tileLayer.x = centroid.position.x * (1 - s)
                    tileLayer.y = centroid.position.y * (1 - s)
                }
            }
            WheelHandler {
                onWheel: function(event) { page.zoomAt(event.angleDelta.y > 0 ? 1 : -1, event.x, event.y) }
            }
            TapHandler {
                onDoubleTapped: function(point) { page.zoomAt(1, point.position.x, point.position.y) }
            }

            // ---- controls
            Column {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 8
                spacing: 6
                RoundButton { text: "+"; Material.elevation: window.elev(4); Material.background: page.Material.background; onClicked: page.zoomAt(1, mapArea.width / 2, mapArea.height / 2) }
                RoundButton { text: "\u2212"; Material.elevation: window.elev(4); Material.background: page.Material.background; onClicked: page.zoomAt(-1, mapArea.width / 2, mapArea.height / 2) }
                RoundButton { text: "\u25CE"; Material.elevation: window.elev(4); Material.background: page.Material.background; onClicked: page.centerOnMe(); ToolTip.text: "Centre on me"; ToolTip.visible: hovered }
                RoundButton { text: "\u2B1A"; Material.elevation: window.elev(4); Material.background: page.Material.background; onClicked: page.centerOnStations(); ToolTip.text: "Fit the stations"; ToolTip.visible: hovered }
            }
            Label {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 8
                text: "z" + page.zoom + "  " + page.centerLat.toFixed(3) + ", " + page.centerLon.toFixed(3)
                font.pixelSize: 11
                color: window.colourOf("dim")
                background: Rectangle { color: page.Material.background; opacity: 0.7; radius: 3 }
                padding: 3
            }
            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                text: app.tileAttribution()
                font.pixelSize: 10
                color: window.colourOf("dim")
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideLeft
                padding: 3
                background: Rectangle { color: page.Material.background; opacity: 0.7 }
            }
        }
    }

    Popup {
        id: detail
        property string stationCall: ""
        property alias text: detailLabel.text
        modal: true
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 420)
        ColumnLayout {
            width: parent.width
            Label { id: detailLabel; Layout.fillWidth: true; wrapMode: Text.Wrap; font.family: "monospace"; font.pixelSize: 12 }
            Button { text: "Message " + detail.stationCall; Material.elevation: window.elev(2); Layout.alignment: Qt.AlignRight
                     onClicked: { detail.close(); page.stationChosen(detail.stationCall) } }
        }
    }
}
