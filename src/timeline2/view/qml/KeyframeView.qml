/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQml.Models 2.15
import com.enums 1.0

Rectangle
{
    id: keyframeContainer
    property int kfrCount : keyframes.count
    anchors.fill: parent
    color: Qt.rgba(1,1,0.8, 0.3)
    property int activeIndex
    property int inPoint
    property int outPoint
    property int clipId
    property int modelStart
    property bool selected
    property var kfrModel
    property int scrollStart
    property alias kfrCanvas: keyframecanvas
    signal seek(int position)

    onKfrCountChanged: {
        keyframecanvas.requestPaint()
    }

    onInPointChanged: {
        keyframecanvas.requestPaint()
    }

    onOutPointChanged: {
        keyframecanvas.requestPaint()
    }

    function resetSelection() {
        kfrModel.setActiveKeyframe(-1)
        keyframeContainer.activeIndex = -1
        keyframeContainer.focus = false
        kfrModel.setSelectedKeyframe(-1, false)
    }

    Keys.onShortcutOverride: {
        if (event.key === Qt.Key_Left) {
            if (event.modifiers & Qt.AltModifier) {
                kfrModel.setActiveKeyframe(Math.max(0, --activeIndex))
                seek(keyframes.itemAt(kfrModel.activeKeyframe).value + keyframeContainer.modelStart - keyframeContainer.inPoint)
                event.accepted = true
            } else {
                var oldFrame = keyframes.itemAt(kfrModel.activeKeyframe).value
                var newPos = Math.max(oldFrame - 1 - keyframeContainer.inPoint, 0)
                if (newPos != oldFrame) {
                    timeline.updateEffectKeyframe(clipId, oldFrame, newPos)
                    event.accepted = true
                }
            }
        }
        else if (event.key === Qt.Key_Right) {
            if (event.modifiers & Qt.AltModifier) {
                kfrModel.setActiveKeyframe(Math.min(keyframes.count - 1, ++activeIndex))
                seek(keyframes.itemAt(kfrModel.activeKeyframe()).value + keyframeContainer.modelStart - keyframeContainer.inPoint)
            } else {
                var oldFrame = keyframes.itemAt(kfrModel.activeKeyframe()).value
                var newPos = Math.min(oldFrame + 1 - keyframeContainer.inPoint, keyframeContainer.outPoint - keyframeContainer.inPoint)
                if (newPos != oldFrame) {
                    timeline.updateEffectKeyframe(clipId, oldFrame, newPos)
                }
            }
            event.accepted = true
        }
        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Escape) {
            keyframeContainer.focus = false
            event.accepted = true
        }
        if ((event.key === Qt.Key_Plus) && !(event.modifiers & Qt.ControlModifier)) {
            var newVal = Math.min(keyframes.itemAt(activeIndex).value / parent.height + .05, 1)
            kfrModel.updateKeyframe(kfrModel.activeKeyframe(), newVal)
            event.accepted = true
        }
        else if ((event.key === Qt.Key_Minus) && !(event.modifiers & Qt.ControlModifier)) {
            var newVal = Math.max(keyframes.itemAt(activeIndex).value / parent.height - .05, 0)
            kfrModel.updateKeyframe(kfrModel.activeKeyframe(), newVal)
            event.accepted = true
        } else {
            event.accepted = false
        }
    }
    Item {
        // Keyframes container
        anchors.fill: parent
        z: 5
        visible: keyframeContainer.selected && keyframeContainer.width > root.baseUnit * 3 && (kfrCount < (keyframeContainer.width / root.baseUnit)) && kfrCount > 1
        Repeater {
            id: keyframes
            model: kfrModel
            Rectangle {
                id: keyframe
                visible: root.activeTool === ProjectTool.SelectTool
                property int frame : model.frame
                property int frameType : model.type
                property string realValue: model.value
                x: (model.frame - keyframeContainer.inPoint) * timeScale
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
                onRealValueChanged: {
                    kf1MouseArea.movingVal = kfrModel.realValue(model.normalizedValue)
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
                    onReleased: mouse => {
                        root.autoScrolling = timeline.autoScroll
                        dragPos = -1
                        var newPos = Math.round(parent.x / timeScale) + keyframeContainer.inPoint
                        if (frame != keyframeContainer.inPoint && newPos != frame) {
                            if (mouse.modifiers & Qt.ShiftModifier) {
                                // offset all subsequent keyframes
                                // TODO: rewrite using timeline to ensure all kf parameters are updated
                                timeline.offsetKeyframes(clipId, frame, newPos)
                            } else {
                                timeline.updateEffectKeyframe(clipId, frame, newPos)
                            }
                        }
                    }
                    onPositionChanged: mouse => {
                        if (mouse.buttons === Qt.LeftButton) {
                            if (frame == keyframeContainer.inPoint) {
                                parent.x = keyframeContainer.inPoint * timeScale
                                return
                            }
                            var newPos = Math.min(Math.round(parent.x / timeScale), Math.round(keyframeContainer.width / timeScale) - 1)
                            if (newPos < 1) {
                                newPos = 1
                            }
                            if (newPos != dragPos && (newPos == 0 || !timeline.hasKeyframeAt(clipId, frame + newPos))) {
                                dragPos = newPos
                                parent.x = newPos * timeScale
                                keyframecanvas.requestPaint()
                            } else {
                                parent.x = dragPos * timeScale
                            }
                        }
                    }
                    onEntered: {
                        timeline.showKeyBinding(i18n("<b>Drag</b> to move selected keyframes position. <b>Shift drag</b> to move all keyframes after this one."))
                    }
                    onExited: {
                        timeline.showKeyBinding()
                    }
                }
                Rectangle {
                    id: keyframeVal
                    x: - root.baseUnit / 2
                    y: keyframeContainer.height - keyframe.value - root.baseUnit / 2
                    width: root.baseUnit
                    height: width
                    radius: width / 2
                    color: model.active ? 'red' : model.selected ? 'orange' : kf1MouseArea.containsMouse || kf1MouseArea.pressed ? root.textColor : root.videoColor
                    border.color: kf1MouseArea.containsMouse || kf1MouseArea.pressed ? activePalette.highlight : root.textColor

                    MouseArea {
                        id: kf1MouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: shiftPressed ? Qt.SizeVerCursor : Qt.PointingHandCursor
                        drag.target: parent
                        drag.smoothed: false
                        drag.threshold: 1
                        property string movingVal: kfrModel.realValue(model.normalizedValue)
                        property double newVal: NaN
                        property bool shiftPressed: false
                        onPressed: mouse => {
                            drag.axis = model.moveOnly ? Drag.XAxis : (mouse.modifiers & Qt.ShiftModifier) ? Drag.YAxis : Drag.XAndYAxis
                        }
                        onClicked: mouse => {
                            keyframeContainer.focus = true
                            if (mouse.modifiers & Qt.ControlModifier && model.selected) {
                                kfrModel.setActiveKeyframe(-1)
                                keyframeContainer.activeIndex = -1
                                kfrModel.setSelectedKeyframe(index, true)
                            } else {
                                kfrModel.setActiveKeyframe(index)
                                keyframeContainer.activeIndex = index
                                kfrModel.setSelectedKeyframe(index, mouse.modifiers & Qt.ControlModifier)
                            }
                            var ix = kfrModel.activeKeyframe()
                            if (ix > -1) {
                                seek(keyframes.itemAt(ix).frame + keyframeContainer.modelStart - keyframeContainer.inPoint)
                            }
                        }
                        onReleased: {
                            if (isNaN(newVal)) {
                                return
                            }
                            root.autoScrolling = timeline.autoScroll
                            var newPos = frame == keyframeContainer.inPoint ? keyframeContainer.inPoint : Math.round((keyframe.x + parent.x + root.baseUnit / 2) / timeScale) + keyframeContainer.inPoint
                            if (newPos === frame && keyframe.value == keyframe.height - parent.y - root.baseUnit / 2) {
                                var pos = keyframeContainer.modelStart + frame - keyframeContainer.inPoint
                                if (proxy.position != pos) {
                                    seek(pos)
                                }
                                return
                            }
                            if (newVal > 1.5 || newVal < -0.5) {
                                if (frame != keyframeContainer.inPoint) {
                                    keyframeContainer.resetSelection()
                                    timeline.removeEffectKeyframe(clipId, frame);
                                } else {
                                    if (newVal < 0) {
                                        newVal = 0;
                                    } else if (newVal > 1) {
                                        newVal = 1;
                                    }
                                    timeline.updateEffectKeyframe(clipId, frame, frame, newVal)
                                }
                            } else {
                                if (newVal < 0) {
                                    newVal = 0;
                                } else if (newVal > 1) {
                                    newVal = 1;
                                }
                                if (model.moveOnly) {
                                    timeline.updateEffectKeyframe(clipId, frame, newPos)
                                } else {
                                    timeline.updateEffectKeyframe(clipId, frame, frame == keyframeContainer.inPoint ? frame : newPos, newVal)
                                }
                            }
                        }
                        onPositionChanged: mouse => {
                            shiftPressed = (mouse.modifiers & Qt.ShiftModifier)
                            if (mouse.buttons === Qt.LeftButton) {
                                if (frame == keyframeContainer.inPoint) {
                                    parent.x = - root.baseUnit / 2
                                } else {
                                    var newPos = Math.min(Math.round((parent.x + root.baseUnit / 2) / timeScale), Math.round(keyframeContainer.width / timeScale) - frame + keyframeContainer.inPoint - 1)
                                    if (frame + newPos <= keyframeContainer.inPoint) {
                                        newPos = keyframeContainer.inPoint + 1 - frame
                                    }
                                    if (newPos != dragPos && (newPos == 0 || !timeline.hasKeyframeAt(clipId, frame + newPos))) {
                                        dragPos = newPos
                                        parent.x = newPos * timeScale - root.baseUnit / 2
                                        keyframecanvas.requestPaint()
                                    } else {
                                        parent.x = dragPos * timeScale - root.baseUnit / 2
                                    }
                                }
                                keyframecanvas.requestPaint()
                                newVal = (keyframeContainer.height - (parent.y + mouse.y)) / keyframeContainer.height
                                movingVal = kfrModel.realValue(Math.min(Math.max(newVal, 0), 1))
                            }
                        }
                        onDoubleClicked: {
                            keyframeContainer.resetSelection()
                            timeline.removeEffectKeyframe(clipId, frame);
                        }
                        onEntered: {
                            timeline.showKeyBinding(i18n("<b>Shift drag</b> to change value of selected keyframes, <b>Ctrl click</b> for multiple keyframe selection."))
                        }
                        onExited: {
                            timeline.showKeyBinding()
                        }
                        ToolTip.visible: (containsMouse || pressed) && movingVal != ""
                        ToolTip.text: movingVal
                    }
                }
            }
        }
    }
    Canvas {
        id: keyframecanvas
        contextType: "2d"
        renderStrategy: Canvas.Threaded
        property int offset: scrollStart < 0 || parent.width <= scrollView.width ? 0 : scrollStart
        anchors.left: parent.left
        anchors.leftMargin: offset
        width: kfrCount > 0 ? Math.min(parent.width, scrollView.width) : 0
        height: kfrCount > 0 ? parent.height : 0
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
            startX: 0
            startY: parent.height
        }

        onPaint: {
            if (kfrCount < 1) {
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
                if (i + 1 < keyframes.count) {
                    if (keyframes.itemAt(i + 1).tmpPos < offset) {
                        continue;
                    }
                }
                xpos = keyframes.itemAt(i).tmpPos - offset
                var type = i > 0 ? keyframes.itemAt(i-1).frameType : keyframes.itemAt(i).frameType
                switch (type) {
                    case 0:
                        // discrete
                        paths.push(compline.createObject(keyframecanvas, {"x": xpos, "y": ypos} ))
                        break;
                    case 2:
                        // curve
                        ypos = keyframes.itemAt(i).tmpVal
                        paths.push(comp.createObject(keyframecanvas, {"x": xpos, "y": ypos} ))
                        break;
                    default:
                        // linear of others
                        ypos = keyframes.itemAt(i).tmpVal
                        paths.push(compline.createObject(keyframecanvas, {"x": xpos, "y": ypos} ))
                        break;
                }
                if (xpos > scrollView.width) {
                    break;
                }
            }
            paths.push(compline.createObject(keyframecanvas, {"x": keyframecanvas.width, "y": ypos} ))
            paths.push(compline.createObject(keyframecanvas, {"x": keyframecanvas.width, "y": keyframecanvas.height} ))
            myPath.pathElements = paths
            ctx.clearRect(0,0, width, height);
            ctx.path = myPath;
            ctx.closePath()
            ctx.fill()
        }
    }
}
