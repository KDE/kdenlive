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
    id: keyframeContainer
    property int activeFrame
    property int activeIndex
    property int inPoint
    property int outPoint
    property bool selected

    onKfrCountChanged: {
        keyframecanvas.requestPaint()
    }
    Keys.onShortcutOverride: {
        if (event.key == Qt.Key_Left) {
            if (event.modifiers & Qt.AltModifier) {
                activeFrame = keyframes.itemAt(Math.max(0, --activeIndex)).frame
            } else {
                var oldFrame = activeFrame
                activeFrame -= 1
                if (activeFrame < 0) {
                    activeFrame = 0
                } else {
                    keyframeModel.moveKeyframe(oldFrame, activeFrame, true)
                }
            }
            event.accepted = true
        }
        if (event.key == Qt.Key_Right) {
            if (event.modifiers & Qt.AltModifier) {
                activeFrame = keyframes.itemAt(Math.min(keyframes.count - 1, ++activeIndex)).frame
            } else {
                var oldFrame = activeFrame
                activeFrame += 1
                keyframeModel.moveKeyframe(oldFrame, activeFrame, true)
            }
            event.accepted = true
        }
        if (event.key == Qt.Key_Return || event.key == Qt.Key_Escape) {
            focus = false
            event.accepted = true
        }
        if (event.key == Qt.Key_Tab) {
            activeFrame = keyframes.itemAt(++activeIndex).frame
            console.log('------------------------- TAB: ', activeFrame)
            event.accepted = true
            focus = true
        }
    }
    Repeater {
        id: keyframes
        model: keyframeModel
        Rectangle {
            id: keyframe
            property int frame : model.frame
            property int frameType : model.type
            x: (model.frame - inPoint) * timeScale
            height: parent.height // * model.normalizedValue
            property int value: parent.height * model.normalizedValue
            property int tmpVal : keyframeVal.y + root.baseUnit / 2
            property int tmpPos : x + keyframeVal.x + root.baseUnit / 2
            anchors.bottom: parent.bottom
            onFrameTypeChanged: {
                keyframecanvas.requestPaint()
            }
            onValueChanged: {
                keyframecanvas.requestPaint()
            }
            width: Math.max(1, timeScale)
            color: kfMouseArea.containsMouse ? 'darkred' : 'transparent'
            visible: keyframeContainer.selected
            MouseArea {
                id: kfMouseArea
                anchors.fill: parent
                anchors.leftMargin: -2
                anchors.rightMargin: -2
                hoverEnabled: true
                cursorShape: Qt.SizeHorCursor
                drag.target: parent
                drag.smoothed: false
                drag.axis: Drag.XAxis
                onReleased: {
                    root.stopScrolling = false
                    var newPos = Math.round(parent.x / timeScale)
                    if (newPos != frame) {
                        if (mouse.modifiers & Qt.ShiftModifier) {
                            // offset all subsequent keyframes
                            keyframeModel.offsetKeyframes(frame, newPos, true)
                        } else {
                            keyframeModel.moveKeyframe(frame, newPos, true)
                        }
                    }
                }
                onPositionChanged: {
                    if (mouse.buttons === Qt.LeftButton) {
                        var newPos = Math.round(parent.x / timeScale)
                        parent.x = newPos * timeScale
                        keyframecanvas.requestPaint()
                    }
                }
            }
            Rectangle {
                id: keyframeVal
                x: - root.baseUnit / 2
                y: parent.height - keyframe.value - root.baseUnit / 2
                width: root.baseUnit
                height: width
                radius: width / 2
                color: Qt.rgba(1,0,0, 0.4)
                border.color: keyframeContainer.activeFrame == keyframe.frame ? 'black' : kf1MouseArea.containsMouse ? 'blue' : 'transparent'
                MouseArea {
                    id: kf1MouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    drag.target: parent
                    drag.smoothed: false
                    onClicked: {
                        keyframeContainer.activeFrame = frame
                        keyframeContainer.activeIndex = index
                        keyframeContainer.focus = true
                    }
                    onReleased: {
                        root.stopScrolling = false
                        var newPos = Math.round((keyframe.x + parent.x + root.baseUnit / 2) / timeScale)
                        var newVal = (keyframeContainer.height - (parent.y + mouse.y)) / keyframeContainer.height
                        if (newVal > 1.5 || newVal < -0.5) {
                            keyframeModel.removeKeyframe(frame);
                        } else {
                            newVal = newVal < 0 ? 0 : newVal > 1 ? 1 : newVal
                            keyframeModel.moveKeyframe(frame, newPos, newVal)
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
                xpos = keyframes.itemAt(i).tmpPos
                if (type == 1) {
                    // discrete
                    paths.push(compline.createObject(keyframecanvas, {"x": xpos, "y": ypos} ))
                }
                ypos = keyframes.itemAt(i).tmpVal
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
