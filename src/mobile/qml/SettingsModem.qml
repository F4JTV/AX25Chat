// SettingsModem.qml - the generated modem configuration and channel access.
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
    Note { text: "The modem configuration is generated from these choices (default audio device, channel 0)." }
    Field { label: "Speed"; ComboBox { width: parent.width; model: ["1200", "9600"]; currentIndex: cfg.modem && cfg.modem.speed === 9600 ? 1 : 0; onActivated: cfg.modem.speed = parseInt(currentText) } }
    Field { label: "PTT"; ComboBox {
        width: parent.width
        model: ["VOX on the radio", "USB sound card GPIO (CM108)", "USB serial RTS", "USB serial DTR"]
        readonly property var keys: ["none", "cm108", "rts", "dtr"]
        currentIndex: cfg.modem ? Math.max(0, keys.indexOf(cfg.modem.ptt)) : 0
        onActivated: cfg.modem.ptt = keys[currentIndex] } }
    Field { label: "CM108 GPIO"; SpinBox { from: 1; to: 8; value: cfg.modem ? cfg.modem.gpio : 3; onValueModified: cfg.modem.gpio = value } }
    Field { label: "TXDELAY (x10 ms)"; SpinBox { from: 0; to: 255; value: cfg.modem ? cfg.modem.txdelay : 30; onValueModified: cfg.modem.txdelay = value } }
    Field { label: "Persistence"; SpinBox { from: 0; to: 255; value: cfg.modem ? cfg.modem.persistence : 63; onValueModified: cfg.modem.persistence = value } }
    Field { label: "Slot time (x10 ms)"; SpinBox { from: 0; to: 255; value: cfg.modem ? cfg.modem.slottime : 10; onValueModified: cfg.modem.slottime = value } }
}
