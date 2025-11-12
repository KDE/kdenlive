/*
    SPDX-FileCopyrightText: 2025 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import org.kde.kdenlive as K

Item {
    id: safeFrame

    property color color
    property bool showSafeZone: false

    function update() {
        console.log('UPDATING SAFE ZONES.....')
        var source = showSafeZone ? (safeFrame.width / safeFrame.height < 0.7 ? "Safe916.qml" : "Safe169.qml") : "" ;
        loader.setSource(source, { "color": safeFrame.color })
    }

    onWidthChanged: update()
    onHeightChanged: update()
    onShowSafeZoneChanged: safeFrame.update()
    onColorChanged: safeFrame.update()

    Loader {
        id: loader
        anchors.fill: parent
    }
}
