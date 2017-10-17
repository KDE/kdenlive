/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

import QtQuick 2.6
import QtQuick.Controls 1.4
import QtQml.Models 2.2

Rectangle
{
    property alias kfrCount : keyframes.count
    anchors.fill: parent
    color: Qt.rgba(1,1,1, 0.6)

    onKfrCountChanged: {
        keyframecanvas.requestPaint()
    }
    Repeater {
        id: keyframes
        model: keyframeModel
        Rectangle {
            id: keyframe
            property int frame : model.frame
            property int frameType : model.type
            x: model.frame * timeScale
            height: parent.height * model.normalizedValue
            anchors.bottom: parent.bottom
            onFrameTypeChanged: {
                keyframecanvas.requestPaint()
            }
            onHeightChanged: {
                keyframecanvas.requestPaint()
            }
            width: Math.max(2, timeScale)
            color: 'darkred'
            border.color: 'red'
            MouseArea {
                id: kfMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                drag.target: parent
                drag.axis: Drag.XAxis
                onReleased: {
                    root.stopScrolling = false
                    var newPos = Math.round(parent.x / timeScale)
                    if (newPos != frame) {
                        keyframeModel.moveKeyframe(frame, newPos, true)
                    }
                }
                onPositionChanged: {
                    if (mouse.buttons === Qt.LeftButton) {
                        var newPos = Math.round(parent.x / timeScale)
                        parent.x = newPos * timeScale
                        keyframecanvas.requestPaint()
                    }
                }
                onDoubleClicked: {
                    keyframeModel.removeKeyframe(frame);
                }
            }
        }
    }
    Canvas {
        id: keyframecanvas
        anchors.fill: parent
        contextType: "2d"
        Component {
            id: comp
            PathCurve { }
        }
        Component {
            id: compline
            PathLine { }
        }
        property var paths : []
        Path {
            id: myPath
            startX: 0; startY: parent.height
        }

        onPaint: {
            if (keyframes.count == 0) {
                return
            }
            context.beginPath()
            context.fillStyle = Qt.rgba(0,0,0.8, 0.4);
            paths = []
            var xpos
            var ypos
            for(var i = 0; i < keyframes.count; i++)
            {
                var type = keyframes.itemAt(i).frameType
                xpos = keyframes.itemAt(i).x
                if (type == 1) {
                    // discrete
                    paths.push(compline.createObject(keyframecanvas, {"x": xpos, "y": ypos} ))
                }
                ypos = parent.height - keyframes.itemAt(i).height
                if (type < 2) {
                    // linear
                    paths.push(compline.createObject(keyframecanvas, {"x": xpos, "y": ypos} ))
                } else if (type == 2) {
                    // curve
                    paths.push(comp.createObject(keyframecanvas, {"x": xpos, "y": ypos} ))
                }
            }
            paths.push(compline.createObject(keyframecanvas, {"x": parent.width, "y": ypos} ))
            paths.push(compline.createObject(keyframecanvas, {"x": parent.width, "y": parent.height} ))
            myPath.pathElements = paths
            context.clearRect(0,0, width, height);
            context.path = myPath;
            context.closePath()
            context.fill()
        }
    }
}
