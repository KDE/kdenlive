/*
    SPDX-FileCopyrightText: 2022 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15


ToolButton {
    id: toolButton
    property string iconName
    property string toolTipText
    contentItem: Item {
        Image {
            visible: toolButton.iconName
            source: 'image://icon/' + toolButton.iconName
            anchors.centerIn: parent
            width: parent.width - 4
            height: width
        }
    }
    width: parent.width
    height: width
    focusPolicy: Qt.NoFocus
    onPressed: {
        root.captureRightClick = true
    }
    onReleased: {
        root.captureRightClick = false
    }
    ToolTip {
        visible: toolButton.hovered
        delay: Qt.styleHints.mousePressAndHoldInterval
        text: toolButton.toolTipText
        timeout: 3000
    }
}
