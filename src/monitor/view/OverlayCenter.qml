/*
    SPDX-FileCopyrightText: 2020 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2018 Willian Pessoa
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick 2.11

Item {
    id: overlay
    Rectangle {
        color: root.overlayColor
        width: parent.width
        height: 1
        anchors.centerIn: parent
    }
    Rectangle {
        color: root.overlayColor
        height: parent.height
        width: 1
        anchors.centerIn: parent
    }
}
