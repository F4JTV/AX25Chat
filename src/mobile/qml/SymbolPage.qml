// SymbolPage.qml - the full APRS symbol picker: both tables, with a search,
// and an overlay character for the alternate table.
//
// This file is part of AX25Chat.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Page {
    id: page
    title: "Map symbol"
    property string pageName: "symbol"
    signal chosen(string table, string code)

    // The current choice: the base table ('/' or '\\') and code, and the
    // overlay character for the alternate table (empty for none).  The APRS
    // table identifier that is transmitted is the overlay character itself
    // when there is one.
    property string table: "/"
    property string code: "-"
    property string overlay: ""
    Component.onCompleted: {
        if (table !== "/" && table !== "\\") { overlay = table; table = "\\" }
    }
    readonly property string effectiveTable: (table === "\\" && overlay.length > 0) ? overlay : table
    readonly property var overlays: ["0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z"]

    readonly property var allSymbols: app.symbolTable("/").concat(app.symbolTable("\\"))
    property string filter: ""
    readonly property var shown: {
        var f = filter.trim().toLowerCase()
        if (f.length === 0) return allSymbols
        var out = []
        for (var i = 0; i < allSymbols.length; i++) {
            var e = allSymbols[i]
            if (e.description.toLowerCase().indexOf(f) >= 0 || (e.table + e.code) === f) out.push(e)
        }
        return out
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        // ---- the result, as it will be sent
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 10
            spacing: 10
            Image {
                source: app.symbolsAvailable ? app.symbolImage(page.effectiveTable, page.code) : ""
                sourceSize.width: 48; sourceSize.height: 48
                Layout.preferredWidth: 48; Layout.preferredHeight: 48
                visible: app.symbolsAvailable
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label { text: app.symbolDescription(page.effectiveTable, page.code); font.bold: true; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Label {
                    text: (page.table === "/" ? "Primary table" : "Alternate table") + ", code " + page.code
                          + (page.effectiveTable !== page.table ? ", overlay " + page.overlay : "")
                          + "  -  sent as " + page.effectiveTable + page.code
                    font.pixelSize: 12; color: window.colourOf("dim"); Layout.fillWidth: true; wrapMode: Text.Wrap
                }
            }
            Button { text: "Use"; highlighted: true; Material.elevation: window.elev(2); onClicked: page.chosen(page.effectiveTable, page.code) }
        }

        // ---- overlay, for the alternate table only
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            visible: page.table === "\\"
            Label { text: "Overlay character (alternate table only): a digit or a capital letter drawn over the symbol"; font.pixelSize: 12; color: window.colourOf("dim"); Layout.fillWidth: true; wrapMode: Text.Wrap }
            Flow {
                Layout.fillWidth: true
                spacing: 4
                Button {
                    text: "none"; flat: true; highlighted: page.overlay === ""
                    implicitWidth: 60; implicitHeight: 36
                    onClicked: page.overlay = ""
                }
                Repeater {
                    model: page.overlays
                    Button {
                        text: modelData; flat: true; highlighted: page.overlay === modelData
                        implicitWidth: 36; implicitHeight: 36
                        onClicked: page.overlay = modelData
                    }
                }
            }
        }

        TextField {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            placeholderText: "Search (name, or table and code such as /- or \\a)"
            onTextChanged: page.filter = text
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: page.shown
            section.property: "table"
            section.delegate: Label {
                width: ListView.view.width
                text: section === "/" ? "Primary table  /" : "Alternate table  \\  (overlay allowed)"
                font.bold: true
                padding: 8
                background: Rectangle { color: window.colourOf("dim"); opacity: 0.15 }
            }
            delegate: ItemDelegate {
                width: ListView.view.width
                highlighted: modelData.table === page.table && modelData.code === page.code
                contentItem: RowLayout {
                    spacing: 10
                    Image {
                        source: app.symbolsAvailable ? app.symbolImage(modelData.table, modelData.code) : ""
                        sourceSize.width: 32; sourceSize.height: 32
                        Layout.preferredWidth: 32; Layout.preferredHeight: 32
                        visible: app.symbolsAvailable
                    }
                    Label { text: modelData.description; Layout.fillWidth: true; elide: Text.ElideRight }
                    Label { text: modelData.table + modelData.code; font.family: "monospace"; color: window.colourOf("dim") }
                }
                onClicked: {
                    page.table = modelData.table
                    page.code = modelData.code
                    if (page.table === "/") page.overlay = ""
                }
            }
        }
    }
}
