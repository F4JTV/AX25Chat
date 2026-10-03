// ChatPage.qml - the conversation, the status strip and the input row.
//
// This file is part of AX25Chat.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Page {
    id: page

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- status strip: a card under the header ---------------------------
        Pane {
            Layout.fillWidth: true
            Material.elevation: window.elev(2)
            Material.background: window.dark ? Qt.lighter(page.Material.background, 1.35) : page.Material.background
            padding: 8
            z: 2
            RowLayout {
                anchors.fill: parent
                spacing: 8
                Rectangle {
                    width: 12; height: 12; radius: 6
                    color: window.colourOf(app.session.channelColourKey)
                    // A soft ring around the state dot.
                    Rectangle { anchors.centerIn: parent; width: 20; height: 20; radius: 10; color: parent.color; opacity: 0.25; z: -1 }
                }
                Label {
                    text: app.session.channelStateLabel
                    font.pixelSize: 13
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Label {
                    text: app.session.modemStatusText
                    font.pixelSize: 12
                    color: window.colourOf("dim")
                    elide: Text.ElideRight
                    Layout.maximumWidth: parent.width * 0.5
                }
                Label {
                    text: "IS"
                    font.pixelSize: 12
                    font.bold: true
                    color: app.session.aprsIsConnected ? window.colourOf("clear") : window.colourOf("offline")
                    visible: app.session.aprsIsStatusText.length > 0
                }
                Label {
                    text: "Q " + app.session.queueCount
                    font.pixelSize: 12
                    color: window.colourOf("dim")
                    visible: app.session.queueCount > 0
                }
            }
        }

        // ---- conversation -----------------------------------------------------------
        ListView {
            id: logView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: app.log
            clip: true
            spacing: 4
            topMargin: 6
            bottomMargin: 6
            // The view follows new lines only while it is at the end; once
            // scrolled up to read, it stays put, and a button offers the way
            // back down with a count of what arrived meanwhile.
            property bool followTail: true
            property int unseen: 0
            onMovementEnded: { if (atYEnd) { followTail = true; unseen = 0 } else followTail = false }
            onFlickEnded: { if (atYEnd) { followTail = true; unseen = 0 } else followTail = false }
            Connections {
                target: app.log
                function onRowsAboutToBeInserted() { logView.followTail = logView.atYEnd || logView.count === 0 }
                function onRowsInserted() {
                    if (logView.followTail) Qt.callLater(function() { logView.positionViewAtEnd() })
                    else logView.unseen++
                }
                function onModelReset() { logView.followTail = true; logView.unseen = 0 }
            }
            RoundButton {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 12
                visible: !logView.followTail && logView.count > 0
                text: "\u2193" + (logView.unseen > 0 ? " " + logView.unseen : "")
                Material.elevation: window.elev(6)
                highlighted: true
                onClicked: { logView.positionViewAtEnd(); logView.followTail = true; logView.unseen = 0 }
            }
            // Traffic (sent, received, positions) sits on a card with an
            // accent bar in its colour; the system's own notes stay plain.
            delegate: Item {
                width: ListView.view.width
                implicitHeight: card.implicitHeight
                readonly property bool traffic: model.colour === "rx" || model.colour === "tx" || model.colour === "beacon" || model.colour === "monitor"
                Rectangle {
                    id: card
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    implicitHeight: row.implicitHeight + (traffic ? 14 : 6)
                    radius: 8
                    color: traffic ? (window.dark ? Qt.lighter(page.Material.background, 1.3) : Qt.darker(page.Material.background, 1.04)) : "transparent"
                    Rectangle {
                        visible: traffic
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: 1
                        width: 4
                        radius: 2
                        color: window.colourOf(model.colour)
                    }
                    RowLayout {
                        id: row
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: traffic ? 14 : 8
                        anchors.rightMargin: 8
                        spacing: 6
                        Label {
                            text: model.time
                            visible: model.time.length > 0
                            color: window.colourOf("dim")
                            font.pixelSize: 11
                            font.family: "monospace"
                            Layout.alignment: Qt.AlignTop
                        }
                        Label {
                            text: model.prefix
                            color: window.colourOf(model.colour)
                            font.bold: true
                            font.pixelSize: 14
                            Layout.alignment: Qt.AlignTop
                        }
                        Label {
                            text: model.message
                            wrapMode: Text.Wrap
                            font.pixelSize: 14
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }

        // ---- input: a raised card with a round send button ----------------------
        Pane {
            Layout.fillWidth: true
            Material.elevation: window.elev(4)
            Material.background: window.dark ? Qt.lighter(page.Material.background, 1.35) : page.Material.background
            padding: 8
            z: 2
            RowLayout {
                anchors.fill: parent
                spacing: 8
                TextField {
                    id: recipientField
                    Layout.preferredWidth: 110
                    placeholderText: "To (all)"
                    text: window.recipient
                    inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    onTextChanged: {
                        var up = text.toUpperCase()
                        if (up !== text) { var pos = cursorPosition; text = up; cursorPosition = pos }
                        window.recipient = text
                    }
                }
                TextField {
                    id: inputField
                    Layout.fillWidth: true
                    placeholderText: "Message"
                    onAccepted: page.send()
                }
                RoundButton {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    highlighted: true
                    Material.elevation: window.elev(6)
                    onClicked: page.send()
                    // A send arrow, drawn.
                    contentItem: Item {
                        Canvas {
                            anchors.fill: parent
                            onPaint: {
                                var ctx = getContext("2d"); ctx.reset()
                                var w = width, h = height
                                ctx.fillStyle = "white"
                                ctx.beginPath()
                                ctx.moveTo(w * 0.22, h * 0.25); ctx.lineTo(w * 0.80, h * 0.50); ctx.lineTo(w * 0.22, h * 0.75)
                                ctx.lineTo(w * 0.34, h * 0.50); ctx.closePath(); ctx.fill()
                            }
                            Component.onCompleted: requestPaint()
                        }
                    }
                }
            }
        }
    }

    function send() {
        if (app.send(recipientField.text, inputField.text)) inputField.text = ""
    }
}
