// SettingsStation.qml - "APRS Settings": who the station is.
//
// This file is part of AX25Chat.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    property var owner
    readonly property var cfg: owner ? owner.cfg : ({})
    spacing: 8
    Field { label: "Callsign"; UpperField { text: cfg.station ? cfg.station.callsign : ""; onEditingFinished: cfg.station.callsign = text } }
    Field { label: "SSID"; SpinBox { from: 0; to: 15; value: cfg.station ? cfg.station.ssid : 0; onValueModified: cfg.station.ssid = value } }
    Note { text: "The SSID tells your stations apart: 7 for a handheld, 9 for a car, 5 for a phone are the customary ones." }
    Field { label: "Operator name"; TextField { width: parent.width; text: cfg.station ? cfg.station.operator_name : ""; onEditingFinished: cfg.station.operator_name = text } }
    Field { label: "Locator"; UpperField { text: cfg.station ? cfg.station.locator : ""; placeholderText: "JN23AB"; onEditingFinished: cfg.station.locator = text } }
    Note { text: "The locator can fill the fixed coordinates of the position reports." }
}
