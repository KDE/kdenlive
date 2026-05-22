/*
    SPDX-FileCopyrightText: 2020 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Shapes 1.15

Shape {
    id: mixShape
    asynchronous: true
    opacity: 0.4

    ShapePath {
        fillColor: "#AAFFFFFF"
        strokeColor: "transparent"
        PathLine {x: 0; y: 0}
        PathLine {x: mixShape.width; y: mixShape.height}
        PathLine {x: 0; y: mixShape.height}
        PathLine {x: 0; y: 0}
    }
    /*ShapePath {
        fillColor: "#66FFFFFF"
        strokeColor: "transparent"
        PathLine {x: mixShape.width; y: 0}
        PathLine {x: mixShape.width; y: mixShape.height}
        PathLine {x: 0; y: mixShape.height}
        PathLine {x: mixShape.width; y: 0}
    }*/
}
