/*
    SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15

Item {
    id: overlay
    required property color color

    Rectangle {
        id: safezone
        objectName: "safezone"
        color: "transparent"
        border.color: overlay.color
        width: overlay.width * 0.9
        height: overlay.height * 0.9
        anchors.centerIn: overlay
        Rectangle {
            id: safetext
            objectName: "safetext"
            color: "transparent"
            border.color: overlay.color
            width: overlay.width * 0.8
            height: overlay.height * 0.8
            anchors.centerIn: safezone
        }
        Rectangle {
            color: overlay.color
            width: overlay.width / 20
            height: 1
            anchors.centerIn: safezone
        }
        Rectangle {
            color: overlay.color
            height: overlay.width / 20
            width: 1
            anchors.centerIn: safezone
        }
        Rectangle {
            color: overlay.color
            height: overlay.height / 11
            width: 1
            y: 0
            x: safezone.width / 2
        }
        Rectangle {
            color: overlay.color
            height: overlay.height / 11
            width: 1
            y: safezone.height -height
            x: safezone.width / 2
        }
        Rectangle {
            color: overlay.color
            width: overlay.width / 11
            height: 1
            y: safezone.height / 2
            x: 0
        }
        Rectangle {
            color: overlay.color
            width: overlay.width / 11
            height: 1
            y: safezone.height / 2
            x: safezone.width -width
        }
    }
}
