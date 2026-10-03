// SettingsSectionPage.qml - one section of the configuration, over the
// sections list. The section's fields come from a file of their own
// (SettingsStation.qml and so on); they edit the owner's working copy.
//
// This file is part of AX25Chat.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Page {
    id: page
    property var owner        // the SettingsPage: cfg, refresh(), save(), generation
    property string section
    property string pageName: "settings-section"
    readonly property var cfg: owner ? owner.cfg : ({})
    readonly property int generation: owner ? owner.generation : 0
    function refresh() { owner.refresh() }
    function showError(text) { errorLabel.text = text }
    // A sub-section over this one (Connection -> Modem, Position -> Location...);
    // the window holds the component, a page cannot instantiate its own type.
    function openSection(key, title) { window.openSettingsSection(owner, key, title) }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            contentHeight: loader.height + 32
            clip: true
            Loader {
                id: loader
                width: page.width - 24
                x: 12
                y: 16
                source: {
                    switch (page.section) {
                    case "station": return "SettingsStation.qml"
                    case "connection": return "SettingsConnection.qml"
                    case "modem": return "SettingsModem.qml"
                    case "aprsis": return "SettingsAprsIs.qml"
                    case "position": return "SettingsPosition.qml"
                    case "location": return "SettingsLocation.qml"
                    case "privacy": return "SettingsPrivacy.qml"
                    case "chat": return "SettingsChat.qml"
                    case "display": return "SettingsDisplay.qml"
                    case "notifications": return "SettingsNotifications.qml"
                    case "map": return "SettingsMap.qml"
                    }
                    return ""
                }
                onLoaded: item.owner = page
            }
        }
        Label { id: errorLabel; color: window.colourOf("error"); wrapMode: Text.Wrap; Layout.fillWidth: true; Layout.margins: 10; visible: text.length > 0 }
        Button {
            text: "Save configuration"
            highlighted: true
            Material.elevation: window.elev(2)
            Layout.fillWidth: true
            Layout.margins: 10
            onClicked: {
                if (owner.save()) page.StackView.view.pop()
                else errorLabel.text = app.lastError
            }
        }
    }
}
