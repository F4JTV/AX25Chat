// Field.qml - a settings row: a label and one control.
//
// This file is part of AX25Chat.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    property alias label: l.text
    default property alias content: holder.data
    Layout.fillWidth: true
    Label { id: l; Layout.preferredWidth: 130; Layout.maximumWidth: 130; wrapMode: Text.Wrap }
    Item { id: holder; Layout.fillWidth: true; implicitHeight: childrenRect.height }
}
