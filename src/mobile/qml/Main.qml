// Main.qml - the mobile window: header with the menu, the chat, pages pushed over it.
//
// This file is part of AX25Chat.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: window
    visible: true
    width: 430
    height: 820
    title: "AX25Chat " + app.version
    // Full screen on the phone (the system bars are hidden by the C++ side
    // as well; Qt shows them again for a window that is not full screen).
    visibility: Qt.platform.os === "android" ? Window.FullScreen : Window.AutomaticVisibility
    // ---- themes -----------------------------------------------------------
    // Four looks, chosen in Settings: light and dark for daytime, red for
    // the night (every colour a shade of red, so the eyes keep their dark
    // adaptation), amber for dim light.  Each carries the Material palette
    // and the colours that mean something in the conversation.
    readonly property var themes: ({
        "light": { "material": Material.Light, "primary": "#3f51b5", "accent": "#e91e63",
                   "background": "#fafafa", "foreground": "#212121",
                   "colours": { "tx": "#1a7f37", "rx": "#0969da", "beacon": "#9a6700", "error": "#b32626",
                                "system": "#57606a", "monitor": "#7c3aed", "dim": "#6e7681", "clear": "#1a7f37",
                                "busy": "#9a6700", "transmitting": "#b32626", "offline": "#6e7681" } },
        "dark":  { "material": Material.Dark, "primary": "#263238", "accent": "#26c6da",
                   "background": "#121212", "foreground": "#e6edf3",
                   "colours": { "tx": "#3fb950", "rx": "#58a6ff", "beacon": "#d29922", "error": "#f85149",
                                "system": "#9198a1", "monitor": "#a371f7", "dim": "#8b949e", "clear": "#3fb950",
                                "busy": "#d29922", "transmitting": "#f85149", "offline": "#8b949e" } },
        "red":   { "material": Material.Dark, "primary": "#2b0000", "accent": "#ff2a2a",
                   "background": "#0a0000", "foreground": "#ff3b3b",
                   "colours": { "tx": "#ff7070", "rx": "#ff4d4d", "beacon": "#d63c3c", "error": "#ff1a1a",
                                "system": "#b03636", "monitor": "#8f2d2d", "dim": "#7a2424", "clear": "#ff5252",
                                "busy": "#c92e2e", "transmitting": "#ff1a1a", "offline": "#5c1f1f" } },
        "amber": { "material": Material.Dark, "primary": "#3d2a00", "accent": "#ffb000",
                   "background": "#100c00", "foreground": "#ffb84d",
                   "colours": { "tx": "#ffd166", "rx": "#ffb000", "beacon": "#e69500", "error": "#ff6a00",
                                "system": "#b88a2e", "monitor": "#a07a30", "dim": "#8a6a2a", "clear": "#ffd166",
                                "busy": "#e69500", "transmitting": "#ff6a00", "offline": "#5c4a1f" } }
    })
    readonly property string themeName: (app.config.ui && themes[app.config.ui.theme]) ? app.config.ui.theme : "light"
    readonly property var theme: themes[themeName]
    Material.theme: theme.material
    Material.primary: theme.primary
    Material.accent: theme.accent
    Material.background: theme.background
    Material.foreground: theme.foreground
    color: theme.background

    readonly property bool dark: theme.material === Material.Dark
    readonly property var colours: theme.colours
    function colourOf(key) { return colours[key] !== undefined ? colours[key] : colours["system"] }
    // Material's elevation (a drop shadow) where the renderer can draw it;
    // the software renderer behind headless screenshots cannot.
    function elev(n) { return app.flatRendering ? 0 : n }

    // The recipient of the next message, set from the stations page.
    property string recipient: ""

    // The chat is the screen; Stations and Settings are pages pushed over
    // it from the menu, with the back arrow (or the phone's back gesture)
    // returning to the chat.
    function showPage(name) {
        if (name === "chat") { stack.pop(null); return }
        if (stack.depth > 1 && stack.currentItem.pageName === name) return
        stack.pop(null)
        if (name === "symbol") { openSymbolPicker("E", "a", null); return }   // the picker over an overlaid alternate symbol
        if (name.indexOf("settings/") === 0) {
            var settings = stack.push(settingsComponent)
            var key = name.substring(9)
            var titles = { "station": "APRS Settings", "connection": "APRS Connection", "position": "Position Reports", "chat": "Chat, beacon and messages", "display": "Display and Notifications" }
            stack.push(sectionComponent, { owner: settings, section: key, title: titles[key] || key })
            return
        }
        stack.push(name === "stations" ? stationsComponent : name === "map" ? mapComponent : settingsComponent)
    }

    // A settings section page over the current one, for the sections list
    // and for a section opening one of its sub-sections.
    function openSettingsSection(owner, key, title) {
        stack.push(sectionComponent, { owner: owner, section: key, title: title })
    }

    // The full symbol picker over the current page; onChosen(table, code)
    // receives the choice, then the page is popped.
    function openSymbolPicker(table, code, onChosen) {
        var pageItem = stack.push(symbolComponent, { table: table, code: code })
        pageItem.chosen.connect(function(t, c) { if (onChosen) onChosen(t, c); stack.pop() })
    }

    header: ToolBar {
        Material.elevation: window.elev(4)
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            ToolButton {
                visible: stack.depth > 1
                onClicked: window.goBack()
                // A drawn arrow, for the same reason as the menu glyph.
                contentItem: Item {
                    implicitWidth: 24; implicitHeight: 24
                    Canvas {
                        anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d"); ctx.reset()
                            ctx.strokeStyle = Material.foreground; ctx.lineWidth = 2.5; ctx.lineCap = "round"; ctx.lineJoin = "round"
                            ctx.beginPath(); ctx.moveTo(20, 12); ctx.lineTo(5, 12); ctx.moveTo(11, 5); ctx.lineTo(4, 12); ctx.lineTo(11, 19); ctx.stroke()
                        }
                        Component.onCompleted: requestPaint()
                    }
                }
            }
            Label {
                text: stack.depth > 1 ? stack.currentItem.title : app.session.myCall
                font.bold: true
                font.pixelSize: 18
                leftPadding: stack.depth > 1 ? 0 : 8
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            ToolButton {
                // The station as a whole: the modem and the APRS-IS connection.
                text: app.session.stationOn ? "Stop" : "Start"
                enabled: !app.session.stationBusy
                onClicked: app.session.toggleStation()
            }
            ToolButton {
                // Drawn rather than the U+2630 glyph, which the phone's fonts
                // may not have.
                contentItem: Item {
                    implicitWidth: 24
                    implicitHeight: 24
                    Column {
                        anchors.centerIn: parent
                        spacing: 4
                        Repeater {
                            model: 3
                            Rectangle { width: 20; height: 2.5; radius: 1; color: Material.foreground }
                        }
                    }
                }
                onClicked: actionsMenu.open()
                // APRSdroid's menu, in its order: the views, the one-shot
                // actions, the log, then preferences, about and quit.
                Menu {
                    id: actionsMenu
                    MenuItem { text: "Show Map"; onTriggered: window.showPage("map") }
                    MenuItem { text: "Show Hub"; onTriggered: window.showPage("stations") }
                    MenuSeparator {}
                    MenuItem { text: "Send position"; onTriggered: app.session.positionNow() }
                    MenuItem { text: "Beacon now"; onTriggered: app.session.beaconNow() }
                    MenuItem { text: "Cancel queue"; enabled: app.session.queueCount > 0; onTriggered: app.session.cancelQueue() }
                    MenuSeparator {}
                    MenuItem { text: "Copy log"; onTriggered: app.copyLog() }
                    MenuItem { text: "Clear log"; onTriggered: app.log.clear() }
                    MenuSeparator {}
                    MenuItem { text: "Preferences"; onTriggered: window.showPage("settings") }
                    MenuItem { text: "About"; onTriggered: aboutDialog.open() }
                    MenuItem { text: "Quit"; onTriggered: app.quit() }
                }
            }
        }
    }

    StackView {
        id: stack
        objectName: "stack"
        anchors.fill: parent
        initialItem: ChatPage { property string pageName: "chat" }
    }

    Component {
        id: stationsComponent
        StationsPage {
            property string pageName: "stations"
            title: "Stations heard"
            onStationChosen: function(call) { window.recipient = call; stack.pop() }
        }
    }
    Component {
        id: symbolComponent
        SymbolPage {}
    }
    Component {
        id: mapComponent
        MapPage {
            onStationChosen: function(call) { window.recipient = call; stack.pop(null) }
        }
    }
    Component {
        id: sectionComponent
        SettingsSectionPage {}
    }
    Component {
        id: settingsComponent
        SettingsPage {
            property string pageName: "settings"
            title: "Settings"
            onSaved: stack.pop()
        }
    }

    // Back: one page, and the sections list of the settings asks about
    // edits that were not saved.
    function goBack() {
        var current = stack.currentItem
        if (current && current.pageName === "settings" && current.hasUnsavedChanges()) {
            unsavedDialog.open()
            return
        }
        stack.pop()
    }

    Dialog {
        id: unsavedDialog
        title: "Unsaved changes"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Save | Dialog.Discard | Dialog.Cancel
        Label { text: "The configuration was changed and not saved." }
        onAccepted: { if (stack.currentItem.save()) stack.pop() }
        onDiscarded: { close(); stack.currentItem.reload(); stack.pop() }
    }

    // The phone's back gesture leaves a page before it leaves the program.
    onClosing: function(close) {
        if (stack.depth > 1) { close.accepted = false; window.goBack() }
    }

    Connections {
        target: app
        function onShowSettings() { window.showPage("settings") }
        function onLocationOff() { locationDialog.open() }
        function onBatteryPromptNeeded() { batteryDialog.open() }
    }

    Dialog {
        id: aboutDialog
        title: "AX25Chat " + app.version
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok
        contentItem: Item {
            implicitWidth: Math.min(window.width - 80, 360)
            implicitHeight: aboutText.implicitHeight
            Label {
                id: aboutText
                width: parent.width
                wrapMode: Text.Wrap
                text: "Packet radio chat and APRS over AX.25.\n\nModem: Dire Wolf by John Langner, WB2OSZ, compiled in.\nMap tiles: OpenStreetMap contributors and the other providers named on the map.\nSymbol artwork: aprs.fi.\n\nGNU General Public License, version 2 or later."
            }
        }
    }

    Dialog {
        id: batteryDialog
        title: "Keep running when locked"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Item {
            implicitWidth: Math.min(window.width - 80, 360)
            implicitHeight: batteryText.implicitHeight
            Label {
                id: batteryText
                width: parent.width
                wrapMode: Text.Wrap
                text: "With the phone locked, Android cuts the network of applications under battery optimisation, so the APRS-IS connection drops. Ask Android to exempt AX25Chat? A system dialog follows."
            }
        }
        onAccepted: app.requestBatteryExemption()
        onRejected: app.noteBatteryPromptDeclined()
    }

    Dialog {
        id: locationDialog
        title: "Location is off"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        // The label fixes its own width: a label sized from the dialog and a
        // dialog sized from the label would loop.
        contentItem: Item {
            implicitWidth: Math.min(window.width - 80, 360)
            implicitHeight: locationText.implicitHeight
            Label {
                id: locationText
                width: parent.width
                wrapMode: Text.Wrap
                text: "The position is set to come from this device, but location is switched off in its settings. Open the location settings to turn it on?"
            }
        }
        onAccepted: app.openLocationSettings()
    }
}
