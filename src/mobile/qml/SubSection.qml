// SubSection.qml - a row that opens a sub-section of the settings, drawn
// like a preference row: a title, a summary, a chevron.
//
// This file is part of AX25Chat.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: row
    property string title
    property string summary
    signal open()
    Layout.fillWidth: true
    Layout.topMargin: 4
    Layout.bottomMargin: 4
    Material.elevation: window.elev(1)
    Material.background: window.dark ? Qt.lighter(Material.background, 1.3) : Material.background
    padding: 10
    opacity: enabled ? 1.0 : 0.5
    contentItem: Item {
        implicitHeight: rowLayout.implicitHeight
        RowLayout {
            id: rowLayout
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 8
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label { text: row.title; font.pixelSize: 15; font.bold: true }
                Label { text: row.summary; visible: text.length > 0; font.pixelSize: 12; color: window.colourOf("dim"); Layout.fillWidth: true; elide: Text.ElideRight }
            }
            Label { text: "\u203A"; font.pixelSize: 24; color: window.colourOf("dim") }
        }
        // Above the content: the Pane consumes presses itself.
        MouseArea { anchors.fill: parent; anchors.margins: -10; enabled: row.enabled; onClicked: row.open() }
    }
}
