// SettingsLocation.qml - "Location Settings": where the position comes
// from, and when it is reported (APRSdroid's Location Source).
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
    Field { label: "Location source"; ComboBox {
        width: parent.width
        model: ["Manual position", "This device (" + app.gpsKind + ")"]
        currentIndex: cfg.position && cfg.position.source === "gps" ? 1 : 0
        onActivated: {
            cfg.position.source = currentIndex === 1 ? "gps" : "fixed"
            if (cfg.position.source !== "gps" && cfg.position.smart_enabled) { cfg.position.smart_enabled = false; cfg.position.enabled = true }
            owner.refresh()
        } } }
    Field { label: "Reporting"; ComboBox {
        id: modeCombo
        width: parent.width
        readonly property bool gps: cfg.position ? cfg.position.source === "gps" : false
        model: ["Off", "Periodic position", "SmartBeaconing position"]
        currentIndex: !cfg.position ? 0 : cfg.position.smart_enabled && gps ? 2 : cfg.position.enabled ? 1 : 0
        delegate: ItemDelegate {
            width: ListView.view.width
            text: modelData
            enabled: index !== 2 || modeCombo.gps
            highlighted: modeCombo.highlightedIndex === index
        }
        onActivated: {
            if (currentIndex === 2 && !gps) { currentIndex = cfg.position.enabled ? 1 : 0; return }
            cfg.position.smart_enabled = currentIndex === 2
            cfg.position.enabled = currentIndex !== 0
            owner.refresh()
        } } }
    Field { label: "Interval (min)"; SpinBox { from: 1; to: 1440; enabled: modeCombo.currentIndex === 1; value: cfg.position ? cfg.position.interval_minutes : 30; onValueModified: cfg.position.interval_minutes = value } }
    Note { text: "SmartBeaconing reports more often when moving fast, on a turn, and when setting off; it needs this device's location (greyed out with a manual position) and drives the APRS-IS position upload as well." }
    // With the device as the source the fields show its live fix, read
    // only; with a manual position they are the fixed coordinates.
    readonly property bool liveGps: cfg.position ? cfg.position.source === "gps" : false
    Field { label: "Latitude"; TextField { width: parent.width; readOnly: liveGps
        text: liveGps ? (app.session.gpsHasFix ? app.session.gpsLatitude.toFixed(5) : "waiting for a fix") : (cfg.position ? String(cfg.position.latitude) : "0")
        inputMethodHints: Qt.ImhFormattedNumbersOnly; onEditingFinished: if (!liveGps) cfg.position.latitude = parseFloat(text) || 0 } }
    Field { label: "Longitude"; TextField { width: parent.width; readOnly: liveGps
        text: liveGps ? (app.session.gpsHasFix ? app.session.gpsLongitude.toFixed(5) : "waiting for a fix") : (cfg.position ? String(cfg.position.longitude) : "0")
        inputMethodHints: Qt.ImhFormattedNumbersOnly; onEditingFinished: if (!liveGps) cfg.position.longitude = parseFloat(text) || 0 } }
    Button { text: "Fill from locator"; Material.elevation: window.elev(2); enabled: cfg.position ? cfg.position.source !== "gps" : true; onClicked: {
        var r = app.locatorToLatLon(cfg.station.locator)
        if (r.error) { owner.showError(r.error); return }
        cfg.position.latitude = r.latitude; cfg.position.longitude = r.longitude; owner.refresh() } }
    Field { label: "Open location at start"; Switch { enabled: cfg.position ? cfg.position.source === "gps" : false; checked: cfg.position ? cfg.position.gps_auto_open : true; onToggled: cfg.position.gps_auto_open = checked } }
}
