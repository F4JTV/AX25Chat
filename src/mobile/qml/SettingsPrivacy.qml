// SettingsPrivacy.qml - "Position privacy": what a report leaves out.
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
    Field { label: "Send altitude"; Switch { checked: cfg.position ? cfg.position.send_altitude : true; onToggled: cfg.position.send_altitude = checked } }
    Field { label: "Send speed and bearing"; Switch { checked: cfg.position ? cfg.position.gps_send_course_speed : true; onToggled: cfg.position.gps_send_course_speed = checked } }
    Note { text: "Both come from this device's location; a manual position carries neither." }
    Field { label: "Messaging capable"; Switch { checked: cfg.position ? cfg.position.messaging_capable : true; onToggled: cfg.position.messaging_capable = checked } }
    Note { text: "Tells the others your station takes APRS messages." }
}
