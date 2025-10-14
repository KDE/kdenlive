/*
    SPDX-FileCopyrightText: 2020 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Shapes 1.15

Shape {
    anchors.fill: parent
    asynchronous: true
    opacity: 0.4
    ShapePath {
        fillColor: "#AAFFFFFF"
        strokeColor: "transparent"
        PathLine {x: 0; y: 0}
        PathLine {x: mixBackground.width; y: mixBackground.height}
        PathLine {x: 0; y: mixBackground.height}
        PathLine {x: 0; y: 0}
    }
    /*ShapePath {
        fillColor: "#66FFFFFF"
        strokeColor: "transparent"
        PathLine {x: mixBackground.width; y: 0}
        PathLine {x: mixBackground.width; y: mixBackground.height}
        PathLine {x: 0; y: mixBackground.height}
        PathLine {x: mixBackground.width; y: 0}
    }*/
}
