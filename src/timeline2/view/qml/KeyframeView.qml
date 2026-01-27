/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQml.Models 2.15

import org.kde.kdenlive as Kdenlive

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
        parent.hasKeyframes = kfrCount > 1
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
                visible: root.activeTool === Kdenlive.ToolType.SelectTool
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
                width: Math.max(1, timeScale / 2)
                color: kfMouseArea.containsMouse ? 'darkred' : 'transparent'
                MouseArea {
                    id: kfMouseArea
                    anchors.fill: parent
                    anchors.leftMargin: - root.baseUnit/3
                    anchors.rightMargin: - root.baseUnit/3
                    hoverEnabled: !root.isPanning
                    cursorShape: Qt.SizeHorCursor
                    enabled: !root.isPanning && parent.x > root.baseUnit / 2 && parent.x < keyframeContainer.width - root.baseUnit / 2
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
                        hoverEnabled: !root.isPanning
                        enabled: !root.isPanning
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
            id: quad
            PathQuad { }
        }
        Component {
            id: cubic
            PathCubic { }
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

            var xpos = keyframes.itemAt(0).tmpPos - offset
            var ypos = keyframes.itemAt(0).tmpVal
            // Add first curve point
            paths.push(compline.createObject(keyframecanvas, {"x": xpos, "y": ypos} ))

            for(var i = 1; i < keyframes.count; i++)
            {
                if (i + 1 < keyframes.count) {
                    if (keyframes.itemAt(i + 1).tmpPos < offset) {
                        continue;
                    }
                }
                xpos = keyframes.itemAt(i).tmpPos - offset
                var alpha = 0.5
                var type = keyframes.itemAt(i-1).frameType
                switch (type) {
                    case KeyframeType.Discrete:
                        // discrete
                        paths.push(compline.createObject(keyframecanvas, {"x": xpos, "y": ypos} ))
                        break;
                    case KeyframeType.Curve:
                    case KeyframeType.CurveSmooth:
                    {
                        // curve
                        console.log('control point:', i, ' is SMOOTH')
                        if (type == KeyframeType.CurveSmooth) {
                            alpha = 0.6
                        } else {
                            alpha = 1.
                        }
                        ypos = keyframes.itemAt(i).tmpVal
                        var nextxpos = 0
                        var nextypos = 0
                        if (i == 0) {
                            paths.push(compline.createObject(keyframecanvas, {"x": xpos, "y": ypos} ))
                            break;
                        }
                        var nextXOffset
                        var nextYOffset = 0
                        if (i < keyframes.count - 1) {
                            if (i == 1) {
                                nextXOffset = (keyframes.itemAt(i + 1).tmpPos - keyframes.itemAt(i - 1).tmpPos) / 3
                                nextYOffset = (keyframes.itemAt(i + 1).tmpVal - keyframes.itemAt(i - 1).tmpVal) / 3
                            } else {
                                nextXOffset = (keyframes.itemAt(i + 1).tmpPos - keyframes.itemAt(i - 1).tmpPos) / 6
                                nextYOffset = (keyframes.itemAt(i + 1).tmpVal - keyframes.itemAt(i - 1).tmpVal) / 6
                            }
                        } else {
                            // Last point in the curve
                            nextXOffset = (xpos - (keyframes.itemAt(i - 1).tmpPos - offset)) / 3
                            nextYOffset = (keyframes.itemAt(i).tmpVal - keyframes.itemAt(i - 1).tmpVal) / 3
                        }
                        nextxpos = xpos - nextXOffset * alpha
                        nextypos = ypos - nextYOffset * alpha
                        var prevXOffset
                        var prevYOffset = 0
                        if (i == 1) {
                            // First point, only one control point
                            paths.push(quad.createObject(keyframecanvas, {"x": xpos, "y": ypos, "controlX": nextxpos, "controlY": nextypos} ))
                            break;
                        } else {
                            prevXOffset = (keyframes.itemAt(i).tmpPos - keyframes.itemAt(i - 2).tmpPos) / 6
                            prevYOffset = (keyframes.itemAt(i).tmpVal - keyframes.itemAt(i - 2).tmpVal) / 6
                        }
                        var prevxpos = keyframes.itemAt(i - 1).tmpPos - offset + prevXOffset * alpha
                        var prevypos = keyframes.itemAt(i - 1).tmpVal + prevYOffset * alpha
                        if (i == keyframes.count - 1) {
                            // Last point
                            paths.push(quad.createObject(keyframecanvas, {"x": xpos, "y": ypos, "controlX": prevxpos, "controlY": prevypos} ))
                            break;
                        }
                        paths.push(cubic.createObject(keyframecanvas, {"x": xpos, "y": ypos, "control1X": prevxpos, "control1Y": prevypos, "control2X": nextxpos, "control2Y": nextypos} ))
                        break;
                    }
                    case KeyframeType.CubicIn:
                        // Simulate cubic with Bezier curve, based on empiric testing
                        ypos = keyframes.itemAt(i).tmpVal
                        var prevX = keyframes.itemAt(i-1).tmpPos - offset
                        var prevY = keyframes.itemAt(i-1).tmpVal
                        paths.push(quad.createObject(keyframecanvas, {"x": xpos, "y": ypos, "controlX": prevX + (xpos - prevX) *  0.75, "controlY": prevY} ))
                        break;
                    case KeyframeType.CubicOut:
                        // Simulate cubic with Bezier curve, based on empiric testing
                        ypos = keyframes.itemAt(i).tmpVal
                        var prevX = keyframes.itemAt(i-1).tmpPos - offset
                        var prevY = keyframes.itemAt(i-1).tmpVal
                        paths.push(quad.createObject(keyframecanvas, {"x": xpos, "y": ypos, "controlX": prevX, "controlY": prevY + (ypos - prevY) *  0.75} ))
                        break;
                    case KeyframeType.ExponentialIn:
                        // Simulate exponential with Bezier curve, based on empiric testing
                        ypos = keyframes.itemAt(i).tmpVal
                        var prevX = keyframes.itemAt(i-1).tmpPos - offset
                        var prevY = keyframes.itemAt(i-1).tmpVal
                        paths.push(cubic.createObject(keyframecanvas, {"x": xpos, "y": ypos, "control1X": prevX + (xpos - prevX) *  1, "control1Y": prevY, "control2X": xpos, "control2Y": ypos - (ypos - prevY) *  0.5}))
                        break;
                    case KeyframeType.ExponentialOut:
                        // Simulate exponential with Bezier curve, based on empiric testing
                        ypos = keyframes.itemAt(i).tmpVal
                        var prevX = keyframes.itemAt(i-1).tmpPos - offset
                        var prevY = keyframes.itemAt(i-1).tmpVal
                        paths.push(cubic.createObject(keyframecanvas, {"x": xpos, "y": ypos, "control1X": prevX, "control1Y": prevY + (ypos - prevY) *  1, "control2X": xpos - (xpos - prevX) *  0.5, "control2Y": ypos}))
                        break;
                    case KeyframeType.CircularIn:
                        // Simulate circular with Bezier curve
                        ypos = keyframes.itemAt(i).tmpVal
                        var prevX = keyframes.itemAt(i-1).tmpPos - offset
                        var prevY = keyframes.itemAt(i-1).tmpVal
                        paths.push(cubic.createObject(keyframecanvas, {"x": xpos, "y": ypos, "control1X": prevX + (xpos - prevX) *  0.5522, "control1Y": prevY, "control2X": xpos, "control2Y": ypos - (ypos - prevY) *  0.5522} ))
                        break;
                    case KeyframeType.CircularOut:
                        // Simulate circular with Bezier curve
                        ypos = keyframes.itemAt(i).tmpVal
                        var prevX = keyframes.itemAt(i-1).tmpPos - offset
                        var prevY = keyframes.itemAt(i-1).tmpVal
                        paths.push(cubic.createObject(keyframecanvas, {"x": xpos, "y": ypos, "control1X": prevX, "control1Y": prevY + (ypos - prevY) *  0.5522, "control2X": xpos - (xpos - prevX) *  0.5522, "control2Y": ypos} ))
                        break;
                    case KeyframeType.BounceIn:
                        // Simulate bounce with Bezier curve, based on empiric testing
                        // Add 3 control points based on i-1
                        ypos = keyframes.itemAt(i).tmpVal
                        var prevX = keyframes.itemAt(i-1).tmpPos - offset
                        var prevY = keyframes.itemAt(i-1).tmpVal
                        var step = (xpos - prevX) / 11.
                        var delta = keyframes.itemAt(i).tmpVal - prevY
                        // Bounce intervals are in steps, a step is the width / 11
                        // 1st touch down at 1 * step
                        // 2nd touch down at 3 * step
                        // 3rd touch down at 7 * step
                        // last touch is the end keyframe
                        var lastX = prevX + step / 4
                        var newX = prevX + step
                        var newY = prevY + delta / 27
                        paths.push(cubic.createObject(keyframecanvas, {"x": newX, "y": prevY, "control1X": lastX, "control1Y": prevY, "control2X": newX, "control2Y": newY} ))
                        // Second kf
                        lastX = newX
                        newX = prevX + 3 * step
                        newY = prevY + delta / 9
                        paths.push(cubic.createObject(keyframecanvas, {"x": newX, "y": prevY, "control1X": lastX, "control1Y": newY, "control2X": newX, "control2Y": newY} ))
                        // Third kf
                        lastX = newX
                        newX = prevX + 7 * step
                        newY = prevY + delta / 3
                        paths.push(cubic.createObject(keyframecanvas, {"x": newX, "y": prevY, "control1X": lastX, "control1Y": newY, "control2X": newX, "control2Y": newY} ))
                        // Last kf
                        lastX = newX
                        newX = xpos - step
                        newY = prevY + delta / 2
                        paths.push(cubic.createObject(keyframecanvas, {"x": xpos, "y": ypos, "control1X": lastX, "control1Y": newY, "control2X": newX, "control2Y": ypos} ))
                        break;
                    case KeyframeType.BounceOut:
                        // Simulate bounce with Bezier curve, based on empiric testing
                        // Add 3 control points based on i-1
                        ypos = keyframes.itemAt(i).tmpVal
                        var prevX = keyframes.itemAt(i-1).tmpPos - offset
                        var prevY = keyframes.itemAt(i-1).tmpVal
                        var step = (xpos - prevX) / 11.
                        var delta = prevY - keyframes.itemAt(i).tmpVal
                        // Bounce intervals are in steps, a step is the width / 11
                        // 1st touch down at 4 * step
                        // 2nd touch down at 8 * step
                        // 3rd touch down at 10 * step
                        // last touch is the end keyframe

                        var lastX = prevX + step
                        var newX = prevX + 4 * step
                        var newY = ypos + delta / 2
                        paths.push(cubic.createObject(keyframecanvas, {"x": newX, "y": ypos, "control1X": lastX, "control1Y": prevY, "control2X": newX, "control2Y": newY} ))
                        // Second kf
                        var lastX = newX
                        newX = prevX + 8 * step
                        newY = ypos + delta / 3
                        paths.push(cubic.createObject(keyframecanvas, {"x": newX, "y": ypos, "control1X": lastX, "control1Y": newY, "control2X": newX, "control2Y": newY} ))
                        // Third kf
                        lastX = newX
                        newX = prevX + 10 * step
                        newY = ypos + delta / 9
                        paths.push(cubic.createObject(keyframecanvas, {"x": newX, "y": ypos, "control1X": lastX, "control1Y": newY, "control2X": newX, "control2Y": newY} ))
                        // Last kf
                        lastX = newX
                        newX = xpos
                        newY = ypos + delta / 27
                        paths.push(cubic.createObject(keyframecanvas, {"x": xpos, "y": ypos, "control1X": lastX, "control1Y": newY, "control2X": xpos, "control2Y": newY} ))
                        break;
                    case KeyframeType.ElasticIn:
                        // Simulate elastic with Bezier curve, based on empiric testing
                        // Add 3 control points based on i-1
                        ypos = keyframes.itemAt(i).tmpVal
                        var prevX = keyframes.itemAt(i-1).tmpPos - offset
                        var prevY = keyframes.itemAt(i-1).tmpVal
                        var step = xpos - prevX
                        var delta = keyframes.itemAt(i).tmpVal - prevY
                        // first half is almost flat
                        var newX = prevX + step / 2.4
                        paths.push(compline.createObject(keyframecanvas, {"x": newX, "y": prevY} ))
                        // Second kf
                        var lastX = newX
                        var newX = newX + step / 6
                        paths.push(cubic.createObject(keyframecanvas, {"x": newX, "y": prevY, "control1X": lastX + step / 20, "control1Y": prevY - Math.cbrt(delta), "control2X": newX - step / 20, "control2Y": prevY - 2 * Math.cbrt(delta)} ))
                        // Third kf
                        lastX = newX
                        newX = newX + step / 6
                        var factor = delta < 0 ? -1 : 1
                        delta = Math.abs(delta)
                        var maximum = factor * Math.sqrt(delta / 2)
                        paths.push(cubic.createObject(keyframecanvas, {"x": newX, "y": prevY, "control1X": lastX + step / 18, "control1Y": prevY + maximum , "control2X": newX - step / 18, "control2Y": prevY + 3 * maximum} ))
                        // Last kf
                        lastX = newX
                        maximum = 6 * (ypos - prevY) * Math.pow(2, 10 * ((lastX - prevX) / step - 1))
                        paths.push(cubic.createObject(keyframecanvas, {"x": xpos, "y": ypos, "control1X": lastX + (xpos - lastX) * 0.6, "control1Y": prevY - maximum, "control2X": xpos - (xpos - lastX) / 6, "control2Y": ypos - maximum} ))
                        break;
                    case KeyframeType.ElasticOut:
                        // Simulate elastic with Bezier curve, based on empiric testing
                        // Add 3 control points based on i-1
                        ypos = keyframes.itemAt(i).tmpVal
                        var prevX = keyframes.itemAt(i-1).tmpPos - offset
                        var prevY = keyframes.itemAt(i-1).tmpVal
                        var step = xpos - prevX
                        var delta = keyframes.itemAt(i).tmpVal - prevY

                        // First kf
                        var lastX = prevX
                        var newX = prevX + step / 6
                        maximum = 6 * (prevY - ypos) * Math.pow(2, 10 * (.65 - 1))
                        paths.push(cubic.createObject(keyframecanvas, {"x": newX, "y": ypos, "control1X": lastX + (step / 6) / 6, "control1Y": prevY - maximum, "control2X": newX - (step / 6) * 0.6, "control2Y": ypos - maximum} ))

                        // Second kf
                        lastX = newX
                        newX = newX + step / 6
                        var factor = delta < 0 ? -1 : 1
                        var maximum = factor * Math.sqrt(Math.abs(delta) / 2)
                        paths.push(cubic.createObject(keyframecanvas, {"x": newX, "y": ypos, "control1X": lastX + step / 18, "control1Y": ypos - 3 * maximum , "control2X": newX - step / 18, "control2Y": ypos - maximum} ))

                        // Third kf
                        var lastX = newX
                        var newX = newX + step / 6
                        paths.push(cubic.createObject(keyframecanvas, {"x": newX, "y": ypos, "control1X": lastX + step / 20, "control1Y": ypos + 2 * Math.cbrt(delta), "control2X": newX - step / 20, "control2Y": ypos + Math.cbrt(delta)} ))

                        // Last half is almost flat
                        paths.push(compline.createObject(keyframecanvas, {"x": xpos, "y": ypos} ))
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


            // Draw CATMULL for reference

            /*ctx.beginPath()
            paths = []
            ctx.fillStyle = Qt.rgba(0.5,0,0.5, 0.5);
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
                    case KeyframeType.Discrete:
                        // discrete
                        paths.push(compline.createObject(keyframecanvas, {"x": xpos, "y": ypos} ))
                        break;
                    case KeyframeType.Curve:
                    case KeyframeType.CurveSmooth:
                    {
                        // curve
                        ypos = keyframes.itemAt(i).tmpVal
                        paths.push(comp.createObject(keyframecanvas, {"x": xpos, "y": ypos} ))
                        break;
                    }
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
            ctx.path = myPath;
            ctx.closePath()
            ctx.fill()
            */

        }
    }
}
