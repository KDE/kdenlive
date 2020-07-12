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

import QtQuick 2.11
import QtQuick.Controls 1.4
import QtQml.Models 2.11

Rectangle
{
    property int kfrCount : keyframes.count
    anchors.fill: parent
    color: Qt.rgba(1,1,0.8, 0.3)
    id: keyframeContainer
    property int activeFrame
    property int activeIndex
    property int inPoint
    property int outPoint
    property bool selected
    property var masterObject
    property var kfrModel

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
                    timeline.updateEffectKeyframe(masterObject.clipId, oldFrame, activeFrame)
                }
            }
            event.accepted = true
        }
        else if (event.key == Qt.Key_Right) {
            if (event.modifiers & Qt.AltModifier) {
                activeFrame = keyframes.itemAt(Math.min(keyframes.count - 1, ++activeIndex)).frame
            } else {
                var oldFrame = activeFrame
                activeFrame += 1
                activeFrame = Math.min(activeFrame, parent.width / timeScale)
                if (activeFrame > oldFrame) {
                    timeline.updateEffectKeyframe(masterObject.clipId, oldFrame, activeFrame)
                }
            }
            event.accepted = true
        }
        else if (event.key == Qt.Key_Return || event.key == Qt.Key_Escape) {
            keyframeContainer.focus = false
            event.accepted = true
        }
        if ((event.key == Qt.Key_Plus) && !(event.modifiers & Qt.ControlModifier)) {
            var newVal = Math.min(keyframes.itemAt(activeIndex).value / parent.height + .05, 1)
            kfrModel.updateKeyframe(activeFrame, newVal)
            event.accepted = true
        }
        else if ((event.key == Qt.Key_Minus) && !(event.modifiers & Qt.ControlModifier)) {
            var newVal = Math.max(keyframes.itemAt(activeIndex).value / parent.height - .05, 0)
            kfrModel.updateKeyframe(activeFrame, newVal)
            event.accepted = true
        } else {
            event.accepted = false
        }
    }
    Item {
        // Keyframes container
        anchors.fill: parent
        z: 5
        visible: keyframeContainer.selected && keyframeContainer.width > root.baseUnit * 3 && (kfrCount < (keyframeContainer.width / root.baseUnit))
        Repeater {
            id: keyframes
            model: kfrModel
            Rectangle {
                id: keyframe
                property int frame : model.frame
                property int frameType : model.type
                x: (model.frame - inPoint) * timeScale
                height: parent.height
                property int value: parent.height * model.normalizedValue
                property int tmpVal : keyframeVal.y + root.baseUnit / 2
                property int tmpPos : x + keyframeVal.x + root.baseUnit / 2
                property int dragPos : -1
                anchors.bottom: parent.bottom
                onFrameTypeChanged: {
                    keyframecanvas.requestPaint()
                }
                onValueChanged: {
                    keyframecanvas.requestPaint()
                }
                width: Math.max(1, timeScale)
                color: kfMouseArea.containsMouse ? 'darkred' : 'transparent'
                MouseArea {
                    id: kfMouseArea
                    anchors.fill: parent
                    anchors.leftMargin: - root.baseUnit/3
                    anchors.rightMargin: - root.baseUnit/3
                    hoverEnabled: true
                    cursorShape: Qt.SizeHorCursor
                    enabled: parent.x > root.baseUnit / 2 && parent.x < keyframeContainer.width - root.baseUnit / 2
                    drag.target: parent
                    drag.smoothed: false
                    drag.axis: Drag.XAxis
                    onReleased: {
                        root.autoScrolling = timeline.autoScroll
                        dragPos = -1
                        var newPos = Math.round(parent.x / timeScale) + inPoint
                        if (frame != inPoint && newPos != frame) {
                            if (mouse.modifiers & Qt.ShiftModifier) {
                                // offset all subsequent keyframes
                                // TODO: rewrite using timeline to ensure all kf parameters are updated
                                timeline.offsetKeyframes(masterObject.clipId, frame, newPos)
                            } else {
                                timeline.updateEffectKeyframe(masterObject.clipId, frame, newPos)
                            }
                        }
                    }
                    onPositionChanged: {
                        if (mouse.buttons === Qt.LeftButton) {
                            if (frame == inPoint) {
                                parent.x = inPoint * timeScale
                                return
                            }
                            var newPos = Math.min(Math.round(parent.x / timeScale), Math.round(keyframeContainer.width / timeScale) - 1)
                            if (newPos < 1) {
                                newPos = 1
                            }
                            if (newPos != dragPos && (newPos == 0 || !timeline.hasKeyframeAt(masterObject.clipId, frame + newPos))) {
                                dragPos = newPos
                                parent.x = newPos * timeScale
                                keyframecanvas.requestPaint()
                            } else {
                                parent.x = dragPos * timeScale
                            }
                        }
                    }
                }
                Rectangle {
                    id: keyframeVal
                    x: - root.baseUnit / 2
                    y: keyframeContainer.height - keyframe.value - root.baseUnit / 2
                    width: root.baseUnit
                    height: width
                    radius: width / 2
                    color: keyframeContainer.activeFrame == keyframe.frame ? 'red' : kf1MouseArea.containsMouse || kf1MouseArea.pressed ? root.textColor : root.videoColor
                    border.color: kf1MouseArea.containsMouse || kf1MouseArea.pressed ? activePalette.highlight : root.textColor
                    MouseArea {
                        id: kf1MouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        drag.target: parent
                        drag.smoothed: false
                        onPressed: {
                            drag.axis = (mouse.modifiers & Qt.ShiftModifier) ? Drag.YAxis : Drag.XAndYAxis
                        }
                        onClicked: {
                            keyframeContainer.activeFrame = frame
                            keyframeContainer.activeIndex = index
                            keyframeContainer.focus = true
                        }
                        onReleased: {
                            root.autoScrolling = timeline.autoScroll
                            var newPos = frame == inPoint ? inPoint : Math.round((keyframe.x + parent.x + root.baseUnit / 2) / timeScale) + inPoint
                            if (newPos == frame && keyframe.value == keyframe.height - parent.y - root.baseUnit / 2) {
                                var pos = masterObject.modelStart + frame - inPoint
                                if (proxy.position != pos) {
                                    proxy.position = pos
                                }
                                return
                            }
                            var newVal = (keyframeContainer.height - (parent.y + mouse.y)) / keyframeContainer.height
                            if (newVal > 1.5 || newVal < -0.5) {
                                if (frame != inPoint) {
                                    timeline.removeEffectKeyframe(masterObject.clipId, frame);
                                } else {
                                    if (newVal < 0) {
                                        newVal = 0;
                                    } else if (newVal > 1) {
                                        newVal = 1;
                                    }
                                    timeline.updateEffectKeyframe(masterObject.clipId, frame, frame, newVal)
                                }
                            } else {
                                if (newVal < 0) {
                                    newVal = 0;
                                } else if (newVal > 1) {
                                    newVal = 1;
                                }
                                timeline.updateEffectKeyframe(masterObject.clipId, frame, frame == inPoint ? frame : newPos, newVal)
                            }
                        }
                        onPositionChanged: {
                            if (mouse.buttons === Qt.LeftButton) {
                                if (frame == inPoint) {
                                    parent.x = - root.baseUnit / 2
                                } else {
                                    var newPos = Math.min(Math.round((parent.x + root.baseUnit / 2) / timeScale), Math.round(keyframeContainer.width / timeScale) - frame + inPoint - 1)
                                    if (frame + newPos <= inPoint) {
                                        newPos = inPoint + 1 - frame
                                    }
                                    if (newPos != dragPos && (newPos == 0 || !timeline.hasKeyframeAt(masterObject.clipId, frame + newPos))) {
                                        dragPos = newPos
                                        parent.x = newPos * timeScale - root.baseUnit / 2
                                        keyframecanvas.requestPaint()
                                    } else {
                                        parent.x = dragPos * timeScale - root.baseUnit / 2
                                    }
                                }
                                keyframecanvas.requestPaint()
                            }
                        }
                        onDoubleClicked: {
                            timeline.removeEffectKeyframe(masterObject.clipId, frame);
                        }
                    }
                }
            }
        }
    }
    Canvas {
        id: keyframecanvas
        contextType: "2d"
        renderStrategy: Canvas.Threaded
        width: kfrCount > 1 ? parent.width : 0
        height: kfrCount > 1 ? parent.height : 0
        opacity: keyframeContainer.selected ? 1 : 0.5
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
            if (kfrCount < 2) {
                return
            }
            var ctx = getContext("2d");
            ctx.beginPath()
            ctx.fillStyle = Qt.rgba(0,0,0.8, 0.5);
            paths = []
            var xpos
            var ypos
            for(var i = 0; i < keyframes.count; i++)
            {
                var type = i > 0 ? keyframes.itemAt(i-1).frameType : keyframes.itemAt(i).frameType
                xpos = keyframes.itemAt(i).tmpPos
                if (type == 0) {
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
            ctx.clearRect(0,0, width, height);
            ctx.path = myPath;
            ctx.closePath()
            ctx.fill()
        }
    }
}
