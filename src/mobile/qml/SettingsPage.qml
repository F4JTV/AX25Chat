// SettingsPage.qml - the configuration as a list of sections; each opens a
// page of its own (SettingsSectionPage) over this one.
//
// One working copy of the configuration, a plain JavaScript object, is
// shared by every section page; Save, on any of them or here, validates
// and applies it as a whole.
//
// This file is part of AX25Chat.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Page {
    id: page
    signal saved()

    property var cfg: ({})
    property string savedSnapshot: ""
    function reload() { cfg = JSON.parse(JSON.stringify(app.config)); savedSnapshot = JSON.stringify(cfg); refresh() }
    property int generation: 0
    function refresh() { generation++ }
    function hasUnsavedChanges() { return JSON.stringify(cfg) !== savedSnapshot }
    function save() {
        if (app.saveConfig(cfg)) { errorLabel.text = ""; page.saved(); return true }
        errorLabel.text = app.lastError
        return false
    }
    function showError(text) { errorLabel.text = text }
    Component.onCompleted: reload()
    Connections { target: app; function onConfigChanged() { page.reload() } }

    // One line under each section name, what the section currently says.
    function summary(key) {
        if (!cfg.station) return ""
        var c = cfg
        switch (key) {
        case "station": return app.session.myCall + (c.station.operator_name ? ", " + c.station.operator_name : "") + (c.station.locator ? ", " + c.station.locator : "")
        case "connection": {
            var what = c.modem.enabled && c.aprsis.enabled ? "modem and APRS-IS" : c.aprsis.enabled ? "APRS-IS only" : "modem only"
            return what + (c.modem.enabled ? ", " + c.modem.speed + " baud, PTT " + ({ "none": "VOX", "cm108": "CM108", "rts": "RTS", "dtr": "DTR" }[c.modem.ptt] || c.modem.ptt) : "")
                        + (c.aprsis.enabled ? ", " + ({ "rotate": "any server", "euro": "Europe", "noam": "N. America", "soam": "S. America", "asia": "Asia", "aunz": "Oceania", "custom": c.aprsis.custom_host }[c.aprsis.region] || c.aprsis.region) : "")
                        + (c.modem.auto_start ? ", at launch" : "")
        }
        case "position": return (c.position.smart_enabled && c.position.source === "gps" ? "SmartBeaconing, " : c.position.enabled ? "every " + c.position.interval_minutes + " min, " : "off, ")
                               + (c.position.source === "gps" ? "from this device" : "manual position") + ", symbol " + c.position.symbol_table + c.position.symbol_code
        case "chat": return "to " + (c.station.destination || "CHAT") + (c.beacon.enabled ? ", beacon every " + c.beacon.interval_minutes + " min" : "") + (c.messaging.auto_ack ? ", acknowledges" : "")
        case "display": return ({ "light": "light", "dark": "dark", "red": "red (night)", "amber": "amber" }[c.ui.theme] || "light") + " theme"
                               + (c.ui.keep_screen_on ? ", screen awake" : "") + (c.ui.keep_running ? ", runs in the background" : "")
                               + (c.notifications.enabled ? "" : ", notifications off")
        }
        return ""
    }

    // APRSdroid's order: the station, its connection, its position reports,
    // then what is ours (the chat), then the display.
    readonly property var sections: [
        { key: "station", title: "APRS Settings" },
        { key: "connection", title: "APRS Connection" },
        { key: "position", title: "Position Reports" },
        { key: "chat", title: "Chat, beacon and messages" },
        { key: "display", title: "Display and Notifications" }
    ]

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: page.sections
            spacing: 8
            topMargin: 8
            leftMargin: 10
            rightMargin: 10
            delegate: Item {
                width: ListView.view.width - 20
                implicitHeight: card.implicitHeight
                Pane {
                    id: card
                    anchors.left: parent.left
                    anchors.right: parent.right
                    Material.elevation: window.elev(1)
                    Material.background: window.dark ? Qt.lighter(page.Material.background, 1.3) : page.Material.background
                    padding: 12
                    // The Pane consumes presses itself (that is what lets it
                    // block what lies under a popup), so the tap is taken by
                    // a mouse area above the content.
                    contentItem: Item {
                        implicitHeight: cardRow.implicitHeight
                        RowLayout {
                            id: cardRow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            spacing: 10
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                Label { text: modelData.title; font.pixelSize: 16; font.bold: true }
                                Label {
                                    text: page.generation >= 0 ? page.summary(modelData.key) : ""
                                    font.pixelSize: 12
                                    color: window.colourOf("dim")
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                }
                            }
                            Label { text: "\u203A"; font.pixelSize: 26; color: window.colourOf("dim") }
                        }
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -12
                            onClicked: window.openSettingsSection(page, modelData.key, modelData.title)
                        }
                    }
                }
            }
        }
        Label { id: errorLabel; color: window.colourOf("error"); wrapMode: Text.Wrap; Layout.fillWidth: true; Layout.margins: 10; visible: text.length > 0 }
        Button {
            text: page.hasUnsavedChanges() || page.generation >= 0 ? (page.hasUnsavedChanges() ? "Save changes" : "Saved") : "Saved"
            enabled: page.hasUnsavedChanges()
            highlighted: true
            Material.elevation: window.elev(2)
            Layout.fillWidth: true
            Layout.margins: 10
            onClicked: page.save()
        }
    }

}
