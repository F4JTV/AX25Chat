// SettingsNotifications.qml - what deserves a sound or a notification.
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
    Field { label: "Enabled"; Switch { checked: cfg.notifications ? cfg.notifications.enabled : true; onToggled: cfg.notifications.enabled = checked } }
    Field { label: "Sound"; Switch { checked: cfg.notifications ? cfg.notifications.sound : true; onToggled: cfg.notifications.sound = checked } }
    Field { label: "Messages to me"; Switch { checked: cfg.notifications ? cfg.notifications.on_message : true; onToggled: cfg.notifications.on_message = checked } }
    Field { label: "Chat"; Switch { checked: cfg.notifications ? cfg.notifications.on_chat : true; onToggled: cfg.notifications.on_chat = checked } }
    Field { label: "Positions"; Switch { checked: cfg.notifications ? cfg.notifications.on_position : false; onToggled: cfg.notifications.on_position = checked } }
    Note { text: "On a phone, traffic that arrives while the application is in the background also becomes a system notification." }
    Button { text: "Test sound"; Material.elevation: window.elev(2); onClicked: app.testSound() }
}
