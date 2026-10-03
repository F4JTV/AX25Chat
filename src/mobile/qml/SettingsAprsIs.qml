// SettingsAprsIs.qml - the APRS Internet System connection.
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
    Note { text: "What is heard on the air goes to the network, where aprs.fi shows it; the traffic around your position comes back into the stations list." }
    Field { label: "Connect"; Switch { checked: cfg.aprsis ? cfg.aprsis.enabled : false; onToggled: cfg.aprsis.enabled = checked } }
    Field { label: "Server"; ComboBox {
        width: parent.width
        model: ["Any (rotate.aprs2.net)", "Europe and Africa", "North America", "South America", "Asia", "Oceania", "Another server"]
        readonly property var keys: ["rotate", "euro", "noam", "soam", "asia", "aunz", "custom"]
        currentIndex: cfg.aprsis ? Math.max(0, keys.indexOf(cfg.aprsis.region)) : 0
        onActivated: { cfg.aprsis.region = keys[currentIndex]; owner.refresh() } } }
    Field { label: "Host name"; TextField { width: parent.width; enabled: cfg.aprsis ? cfg.aprsis.region === "custom" : false; text: cfg.aprsis ? cfg.aprsis.custom_host : ""; placeholderText: "for Another server"; inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase; onEditingFinished: cfg.aprsis.custom_host = text } }
    Field { label: "Port"; SpinBox {
        from: 1; to: 65535; editable: true
        value: cfg.aprsis ? cfg.aprsis.port : 14580
        // A port number, not a quantity: no thousands separator.
        textFromValue: function(v) { return String(v) }
        valueFromText: function(t) { return parseInt(t) || 14580 }
        onValueModified: cfg.aprsis.port = value } }
    Field { label: "Passcode"; RowLayout {
        width: parent.width
        spacing: 8
        TextField {
            id: passcodeField
            Layout.fillWidth: true
            text: cfg.aprsis && cfg.aprsis.passcode >= 0 ? String(cfg.aprsis.passcode) : ""
            placeholderText: "for your callsign; empty = download only"
            inputMethodHints: Qt.ImhDigitsOnly
            onEditingFinished: cfg.aprsis.passcode = text.trim().length > 0 ? parseInt(text) : -1
        }
        Button {
            text: "Compute"
            Material.elevation: window.elev(2)
            onClicked: {
                var code = app.passcodeFor(cfg.station.callsign)
                if (code < 0) { owner.showError("Enter your callsign first, in Station."); return }
                cfg.aprsis.passcode = code; passcodeField.text = String(code); owner.showError("")
            }
        }
    } }
    Note { text: "The passcode is derived from your callsign (without SSID); Compute fills it in. Uploading needs it; downloading works without." }
    Field { label: "Upload what is heard on the air"; Switch { checked: cfg.aprsis ? cfg.aprsis.gate_rf : true; onToggled: cfg.aprsis.gate_rf = checked } }
    Note { text: "Packets heard on the air that decode as APRS are sent as a receive-only IGate does (qAO and your callsign). Chat traffic is never sent." }
    Field { label: "Download the traffic around me"; Switch { checked: cfg.aprsis ? cfg.aprsis.receive : true; onToggled: cfg.aprsis.receive = checked } }
    Field { label: "Radius (km)"; SpinBox { from: 1; to: 500; editable: true; stepSize: 10; value: cfg.aprsis ? cfg.aprsis.radius_km : 50; onValueModified: cfg.aprsis.radius_km = value } }
    Field { label: "Show Internet traffic in the chat"; Switch { checked: cfg.aprsis ? cfg.aprsis.show_in_chat : false; onToggled: cfg.aprsis.show_in_chat = checked } }
    Field { label: "Upload my position"; Switch { checked: cfg.aprsis ? cfg.aprsis.position_enabled : false; onToggled: cfg.aprsis.position_enabled = checked } }
    Field { label: "And right after the login"; Switch { checked: cfg.aprsis ? cfg.aprsis.position_on_connect : true; onToggled: cfg.aprsis.position_on_connect = checked } }
    Note { text: "The same report as on the air (source, symbol, comment, GPS or fixed coordinates), sent straight to the network with the TCPIP* path, on the schedule of Position Reports -> Location Settings: the periodic interval, or SmartBeaconing. Needs the passcode." }
    Note { text: app.session.aprsIsStatusText; visible: text.length > 0 }
}
