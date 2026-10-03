// SettingsDisplay.qml - "Display and Notifications": the screen, the
// theme, what the chat shows, and the way to the notifications, the map
// and the background behaviour.
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
    Field { label: "Keep screen awake"; Switch { checked: cfg.ui ? cfg.ui.keep_screen_on : true; onToggled: cfg.ui.keep_screen_on = checked } }
    Note { text: "No inactivity lock while the application is in front." }
    Field { label: "Theme"; ComboBox {
        width: parent.width
        model: ["Light", "Dark", "Red (night)", "Amber"]
        readonly property var keys: ["light", "dark", "red", "amber"]
        currentIndex: cfg.ui ? Math.max(0, keys.indexOf(cfg.ui.theme)) : 0
        onActivated: cfg.ui.theme = keys[currentIndex] } }
    Field { label: "Timestamps"; Switch { checked: cfg.ui ? cfg.ui.show_timestamps : true; onToggled: cfg.ui.show_timestamps = checked } }
    Field { label: "Monitor other traffic"; Switch { checked: cfg.ui ? cfg.ui.show_monitor : true; onToggled: cfg.ui.show_monitor = checked } }
    Note { text: "Monitor: frames that are not addressed to the chat or to you are shown too, with their path." }
    Field { label: "Show modem messages in the chat"; Switch { checked: cfg.modem ? cfg.modem.echo_log : false; onToggled: cfg.modem.echo_log = checked } }
    SubSection { title: "Notifications"
                 summary: cfg.notifications ? (cfg.notifications.enabled ? (cfg.notifications.sound ? "sound" : "silent") : "off") : ""
                 onOpen: owner.openSection("notifications", "Notifications") }
    SubSection { title: "Map"
                 summary: cfg.ui ? ({ "osm": "OpenStreetMap", "opentopo": "OpenTopoMap", "ign_plan": "IGN Plan", "ign_ortho": "IGN Orthophotos", "esri_topo": "Esri World Topo", "esri_imagery": "Esri World Imagery", "custom": "another tile server" }[cfg.ui.map_provider] || cfg.ui.map_provider) + ", " + cfg.ui.map_cache_mb + " MB of tiles" : ""
                 onOpen: owner.openSection("map", "Map") }

    Label { text: "In the background"; font.bold: true; font.pixelSize: 16; topPadding: 12; Layout.fillWidth: true }
    Field { label: "Keep running in the background"; Switch { checked: cfg.ui ? cfg.ui.keep_running : true; onToggled: cfg.ui.keep_running = checked } }
    Note { text: "A foreground service with a permanent notification keeps the modem, the APRS-IS connection and the position working while the application is off screen. Off: Android suspends them within minutes." }
    RowLayout {
        Layout.fillWidth: true
        Label { text: "Battery optimisation: " + (app.batteryExempt() ? "exempt" : "active, the network is cut when the phone is locked"); font.pixelSize: 12; color: window.colourOf("dim"); Layout.fillWidth: true; wrapMode: Text.Wrap }
        Button { text: "Request exemption"; Material.elevation: window.elev(2); visible: !app.batteryExempt(); onClicked: app.requestBatteryExemption() }
    }
}
