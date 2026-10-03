// SettingsPosition.qml - "Position Reports": the symbol, the comment, and
// the way to the location settings and the position privacy.
//
// This file is part of AX25Chat.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: root
    property var owner
    readonly property var cfg: owner ? owner.cfg : ({})
    readonly property int generation: owner ? owner.generation : 0
    spacing: 8
    Field { label: "APRS symbol"; RowLayout {
        width: parent.width
        spacing: 8
        Image {
            source: (app.symbolsAvailable && cfg.position) ? app.symbolImage(cfg.position.symbol_table, cfg.position.symbol_code) + "?" + root.generation : ""
            sourceSize.width: 32; sourceSize.height: 32
            Layout.preferredWidth: 32; Layout.preferredHeight: 32
            visible: app.symbolsAvailable
        }
        Label {
            text: cfg.position && root.generation >= 0 ? app.symbolDescription(cfg.position.symbol_table, cfg.position.symbol_code) + "  (" + cfg.position.symbol_table + cfg.position.symbol_code + ")" : ""
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        Button {
            text: "Choose"
            Material.elevation: window.elev(2)
            onClicked: window.openSymbolPicker(cfg.position.symbol_table, cfg.position.symbol_code,
                function(table, code) { cfg.position.symbol_table = table; cfg.position.symbol_code = code; owner.refresh() })
        }
    } }
    Field { label: "Comment field"; TextField { width: parent.width; text: cfg.position ? cfg.position.comment : ""; maximumLength: 43; onEditingFinished: cfg.position.comment = text } }
    Note { text: "Sent with every position report; what the others read next to your symbol." }
    SubSection { title: "Location Settings"
                 summary: cfg.position ? ((cfg.position.smart_enabled && cfg.position.source === "gps" ? "SmartBeaconing" : cfg.position.enabled ? "every " + cfg.position.interval_minutes + " min" : "off")
                          + ", " + (cfg.position.source === "gps" ? "from this device" : "manual position")) : ""
                 onOpen: owner.openSection("location", "Location Settings") }
    SubSection { title: "Position privacy"
                 summary: cfg.position ? ((cfg.position.send_altitude ? "altitude" : "no altitude") + ", " + (cfg.position.gps_send_course_speed ? "speed and bearing" : "no speed or bearing")) : ""
                 onOpen: owner.openSection("privacy", "Position privacy") }
    Field { label: "Send on start"; Switch { checked: cfg.position ? cfg.position.send_on_connect : false; onToggled: cfg.position.send_on_connect = checked } }
    Note { text: "One report right after the modem starts." }
}
