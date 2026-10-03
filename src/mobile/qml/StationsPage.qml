// StationsPage.qml - the hub: stations heard, each on a card; tapping one
// addresses the next message.
//
// This file is part of AX25Chat.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Page {
    id: page
    signal stationChosen(string callsign)

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Label {
            Layout.fillWidth: true
            Layout.margins: 10
            text: (app.stations.count > 0 ? app.stations.count + " station" + (app.stations.count > 1 ? "s" : "") + "  -  " : "")
                  + (app.session.gpsStatusText.length > 0 ? app.session.gpsStatusText : app.session.positionCountdown)
            color: window.colourOf("dim")
            font.pixelSize: 12
            elide: Text.ElideRight
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: app.stations
            clip: true
            spacing: 6
            leftMargin: 8
            rightMargin: 8
            bottomMargin: 8
            delegate: Item {
                width: ListView.view.width - 16
                implicitHeight: card.implicitHeight
                Pane {
                    id: card
                    anchors.left: parent.left
                    anchors.right: parent.right
                    Material.elevation: window.elev(1)
                    Material.background: window.dark ? Qt.lighter(page.Material.background, 1.3) : page.Material.background
                    padding: 10
                    // A mouse area above the content takes the tap and the
                    // long press (the Pane consumes presses itself).
                    contentItem: Item {
                        implicitHeight: cardRow.implicitHeight
                        RowLayout {
                            id: cardRow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            spacing: 12
                            Image {
                                source: model.hasPosition && app.symbolsAvailable ? app.symbolImage(model.table, model.code) : ""
                                sourceSize.width: 36
                                sourceSize.height: 36
                                Layout.preferredWidth: 36; Layout.preferredHeight: 36
                                visible: status === Image.Ready
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                RowLayout {
                                    Label { text: model.callsign; font.bold: true; font.pixelSize: 16 }
                                    Label { text: "IS"; visible: model.internet; font.pixelSize: 11; font.bold: true; color: window.colourOf("monitor") }
                                }
                                Label {
                                    // Everything after the callsign: position, distance, age, comment.
                                    text: model.line.substring(model.callsign.length).replace("[IS]", "").trim()
                                    font.pixelSize: 12
                                    color: window.colourOf("dim")
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                            }
                            Label { text: "\u203A"; font.pixelSize: 24; color: window.colourOf("dim") }
                        }
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -10
                            onClicked: page.stationChosen(model.callsign)
                            onPressAndHold: { detail.text = model.tooltip; detail.open() }
                        }
                    }
                }
            }
            Label {
                anchors.centerIn: parent
                visible: parent.count === 0
                text: "Nothing heard yet"
                color: window.colourOf("dim")
            }
        }
    }

    Popup {
        id: detail
        property alias text: detailLabel.text
        modal: true
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 400)
        Material.elevation: window.elev(8)
        Label { id: detailLabel; width: parent.width; wrapMode: Text.Wrap; font.family: "monospace" }
    }
}
