// SettingsChat.qml - the chat on the air: destination, text beacon, messages.
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
    Field { label: "Unproto destination"; UpperField { text: cfg.station ? cfg.station.destination : ""; onEditingFinished: cfg.station.destination = text } }
    Note { text: "The destination the chat frames carry; every station listening on it sees them." }

    Label { text: "Text beacon"; font.bold: true; font.pixelSize: 16; topPadding: 12; Layout.fillWidth: true }
    Field { label: "Periodic beacon"; Switch { checked: cfg.beacon ? cfg.beacon.enabled : false; onToggled: cfg.beacon.enabled = checked } }
    Field { label: "Interval (min)"; SpinBox { from: 1; to: 1440; value: cfg.beacon ? cfg.beacon.interval_minutes : 30; onValueModified: cfg.beacon.interval_minutes = value } }
    Field { label: "Text"; TextField { width: parent.width; text: cfg.beacon ? cfg.beacon.text : ""; onEditingFinished: cfg.beacon.text = text } }
    Note { text: "{call}, {name} and {loc} are replaced by the callsign, the operator name and the locator." }

    Label { text: "Messages"; font.bold: true; font.pixelSize: 16; topPadding: 12; Layout.fillWidth: true }
    Field { label: "Acknowledge messages"; Switch { checked: cfg.messaging ? cfg.messaging.auto_ack : true; onToggled: cfg.messaging.auto_ack = checked } }
    Field { label: "Attempts"; SpinBox { from: 1; to: 20; value: cfg.messaging ? cfg.messaging.max_attempts : 5; onValueModified: cfg.messaging.max_attempts = value } }
    Field { label: "First retry (s)"; SpinBox { from: 5; to: 600; value: cfg.messaging ? cfg.messaging.retry_seconds : 30; onValueModified: cfg.messaging.retry_seconds = value } }
    Note { text: "A message to a station is repeated with growing intervals until it is acknowledged or the attempts run out." }
}
