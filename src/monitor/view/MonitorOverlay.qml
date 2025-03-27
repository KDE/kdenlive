/*
    SPDX-FileCopyrightText: 2025 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15

Item {
    id: overlayFrame

    property color color
    property int overlayType: 0

    function update() {
        var source = "";
         switch (overlayFrame.overlayType) {
              case 1:
                source = "OverlayStandard.qml";
                break;
              case 2:
                source = "OverlayMinimal.qml";
                break;
              case 3:
                source = "OverlayCenter.qml";
                break;
              case 4:
                source = "OverlayCenterDiagonal.qml";
                break;
              case 5:
                source = "OverlayThirds.qml";
                break;
              case 0:
              default:
                 source = ''
        }
        loader.setSource(source, { "color": overlayFrame.color })
    }

    onOverlayTypeChanged: overlayFrame.update()
    onColorChanged: overlayFrame.update()

    Loader {
        id: loader
        anchors.fill: parent
    }
}
