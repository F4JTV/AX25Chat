// SettingsConnection.qml - "APRS Connection": what Start brings up, and
// the way to each protocol's own settings (APRSdroid's Connection
// Preferences).
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
    Field { label: "Connection"; ComboBox {
        width: parent.width
        model: ["AFSK modem (the radio)", "APRS Internet System", "Both"]
        currentIndex: !cfg.modem ? 2 : (cfg.modem.enabled && cfg.aprsis.enabled) ? 2 : cfg.aprsis.enabled ? 1 : 0
        onActivated: {
            cfg.modem.enabled = currentIndex !== 1
            cfg.aprsis.enabled = currentIndex !== 0
            owner.refresh()
        } } }
    Note { text: "Start and Stop act on this: the modem through the sound card and the radio, the Internet connection, or both at once." }
    SubSection { title: "Modem (AFSK via audio)"; enabled: cfg.modem ? cfg.modem.enabled : true
                 summary: cfg.modem ? cfg.modem.speed + " baud, PTT " + ({ "none": "VOX", "cm108": "CM108 GPIO", "rts": "serial RTS", "dtr": "serial DTR" }[cfg.modem.ptt] || cfg.modem.ptt) : ""
                 onOpen: owner.openSection("modem", "Modem") }
    SubSection { title: "APRS Internet System"; enabled: cfg.aprsis ? cfg.aprsis.enabled : true
                 summary: cfg.aprsis ? (({ "rotate": "any server", "euro": "Europe and Africa", "noam": "North America", "soam": "South America", "asia": "Asia", "aunz": "Oceania", "custom": cfg.aprsis.custom_host }[cfg.aprsis.region] || cfg.aprsis.region)
                          + (cfg.aprsis.gate_rf ? ", upload" : "") + (cfg.aprsis.receive ? ", download " + cfg.aprsis.radius_km + " km" : "") + (cfg.aprsis.position_enabled ? ", my position" : "")) : ""
                 onOpen: owner.openSection("aprsis", "APRS Internet System") }
    Field { label: "Start at launch"; Switch { checked: cfg.modem ? cfg.modem.auto_start : true; onToggled: cfg.modem.auto_start = checked } }
    Field { label: "Digipeater path"; UpperField { text: cfg.station ? cfg.station.digipeaters : ""; placeholderText: "WIDE1-1,WIDE2-1"; onEditingFinished: cfg.station.digipeaters = text } }
    Note { text: "The path every frame sent on the air carries; empty for direct." }
}
