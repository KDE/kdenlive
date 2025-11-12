/*
    SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick

Item {
    id: overlay
    property color color

    Rectangle {
        id: safezone
        objectName: "safezone"
        color: "transparent"
        border.color: overlay.color
        width: parent.width * 0.9
        height: parent.height * 0.9
        anchors.centerIn: parent
        Rectangle {
            id: safetext
            objectName: "safetext"
            color: "transparent"
            border.color: overlay.color
            width: frame.width * 0.8
            height: frame.height * 0.8
            anchors.centerIn: parent
        }
        Rectangle {
            color: overlay.color
            height: frame.height / 11
            width: 1
            y: 0
            x: parent.width / 2
        }
        Rectangle {
            color: overlay.color
            height: frame.height / 11
            width: 1
            y: parent.height -height
            x: parent.width / 2
        }
        Rectangle {
            color: overlay.color
            width: frame.width / 11
            height: 1
            y: parent.height / 2
            x: 0
        }
        Rectangle {
            color: overlay.color
            width: frame.width / 11
            height: 1
            y: parent.height / 2
            x: parent.width -width
        }
    }
}
