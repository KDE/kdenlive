/*
    SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Shapes
import org.kde.kdenlive as K

Item {
    id: overlay
    property color color: K.KdenliveSettings.safeColor

    Shape {
        id: framePath
        property int availableHeight: parent.height
        property int availableWidth: parent.width
        anchors.centerIn: parent
        height: availableHeight
        width: availableHeight * 9 / 16
        ShapePath {
            strokeWidth: 1
            strokeColor: overlay.color
            fillColor: "transparent"
            startX: framePath.availableHeight * 0.0625
            startY: framePath.availableHeight * 0.130
            PathLine { relativeX: framePath.availableHeight * 0.4375; relativeY: 0 }
            PathLine { relativeX: 0; relativeY: framePath.availableHeight * 0.24479 }
            PathLine { relativeX: framePath.availableHeight * -0.0375; relativeY: 0 }
            PathLine { relativeX: 0; relativeY: framePath.availableHeight * 0.427}
            PathLine { relativeX: framePath.availableHeight * -0.4; relativeY: 0 }
            PathLine { relativeX: 0; relativeY: framePath.availableHeight * -0.6718}
        }
    }
}
