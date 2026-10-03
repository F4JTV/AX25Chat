// UpperField.qml - a text field that keeps its content upper case whatever
// the keyboard does, for callsigns, paths and locators.
//
// This file is part of AX25Chat.
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls

TextField {
    width: parent.width
    inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
    onTextChanged: {
        var up = text.toUpperCase()
        if (up !== text) { var pos = cursorPosition; text = up; cursorPosition = pos }
    }
}
