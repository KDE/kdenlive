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
    property point profile: root.profile
    onProfileChanged: {
        if (showSafeZone) {
            // Update safe zone in case the profile changed
            safeFrame.update()
        }
    }

    function update() {
        var source = showSafeZone ? (profile.x / profile.y < 0.7 ? "Safe916.qml" : "Safe169.qml") : "" ;
        loader.setSource(source)
    }
    onShowSafeZoneChanged: safeFrame.update()

    Loader {
        id: loader
        anchors.fill: parent
    }
}
