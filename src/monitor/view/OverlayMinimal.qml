/*
    SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15

Item {
    id: overlay
    property color color

    Rectangle {
        color: overlay.color
        width: frame.width / 20
        height: 1
        anchors.centerIn: parent
    }
    Rectangle {
        color: overlay.color
        height: frame.width / 20
        width: 1
        anchors.centerIn: parent
    }
}
