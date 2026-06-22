/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15

import org.kde.kdenlive as K

Rectangle
{
    id: keyframeContainer
    property int kfrCount : keyframes.count
    anchors.fill: parent
    color: Qt.rgba(1,1,0.8, 0.3)
    property int activeIndex
    property int inPoint
    property int outPoint
    property int ownerType
    property int ownerId
    property int modelStart
    property bool selected
    property var kfrModel
    property int scrollStart
    property int containerWidth
    property bool isPanning
    property color textColor: activePalette.text
    property color kfColor: activePalette.highlight
    property alias kfrCanvas: keyframecanvas
    property double timescale
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

    Keys.onShortcutOverride: (event) => {
        if (event.key === Qt.Key_Left) {
            let kfr = keyframes.itemAt(kfrModel.activeKeyframe) as KeyframeDelegate
            if (event.modifiers & Qt.AltModifier) {
                kfrModel.setActiveKeyframe(Math.max(0, --activeIndex))
                let activeKeyframe = keyframes.itemAt(kfrModel.activeKeyframe) as KeyframeDelegate
                seek(activeKeyframe.value + keyframeContainer.modelStart - keyframeContainer.inPoint)
                event.accepted = true
            } else {
                let activeKeyframe = keyframes.itemAt(kfrModel.activeKeyframe) as KeyframeDelegate
                var oldFrame = activeKeyframe.value
                var newPos = Math.max(oldFrame - 1 - keyframeContainer.inPoint, 0)
                if (newPos != oldFrame) {
                    kfrModel.moveKeyframe(oldFrame, newPos, true)
                    event.accepted = true
                }
            }
        }
        else if (event.key === Qt.Key_Right) {
            if (event.modifiers & Qt.AltModifier) {
                kfrModel.setActiveKeyframe(Math.min(keyframes.count - 1, ++activeIndex))
                let activeKeyframe = keyframes.itemAt(kfrModel.activeKeyframe) as KeyframeDelegate
                seek(activeKeyframe.value + keyframeContainer.modelStart - keyframeContainer.inPoint)
            } else {
                let activeKeyframe = keyframes.itemAt(kfrModel.activeKeyframe) as KeyframeDelegate
                var oldFrame = activeKeyframe.value
                var newPos = Math.min(oldFrame + 1 - keyframeContainer.inPoint, keyframeContainer.outPoint - keyframeContainer.inPoint)
                if (newPos != oldFrame) {
                    kfrModel.moveKeyframe(oldFrame, newPos, true)
                }
            }
            event.accepted = true
        }
        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Escape) {
            keyframeContainer.focus = false
            event.accepted = true
        }
        if ((event.key === Qt.Key_Plus) && !(event.modifiers & Qt.ControlModifier)) {
            let activeKeyframe = keyframes.itemAt(activeIndex) as KeyframeDelegate
            var newVal = Math.min(activeKeyframe.value / parent.height + .05, 1)
            kfrModel.updateKeyframe(kfrModel.activeKeyframe(), newVal)
            event.accepted = true
        }
        else if ((event.key === Qt.Key_Minus) && !(event.modifiers & Qt.ControlModifier)) {
            let activeKeyframe = keyframes.itemAt(activeIndex) as KeyframeDelegate
            var newVal = Math.max(activeKeyframe.value / parent.height - .05, 0)
            kfrModel.updateKeyframe(kfrModel.activeKeyframe(), newVal)
            event.accepted = true
        } else {
            event.accepted = false
        }
    }
    MouseArea {
        // Keyframes container
        anchors.fill: parent
        z: 5
        visible: keyframeContainer.selected && keyframeContainer.width > K.UiUtils.baseSizeMedium * 3
                 && (keyframeContainer.kfrCount < (keyframeContainer.width / K.UiUtils.baseSizeMedium)) && keyframeContainer.kfrCount > 1
        onDoubleClicked: {
            var newVal = (height - mouseY) / height
            var newPos = Math.round(mouseX / keyframeContainer.timescale) + keyframeContainer.inPoint
            //root.timeline.addEffectKeyframe(dragProxy.draggedItem, newPos, newVal)
            keyframeContainer.kfrModel.addKeyframe(newPos, newVal);
        }
        Repeater {
            id: keyframes
            model: keyframeContainer.kfrModel
            KeyframeDelegate {
                timeScale: keyframeContainer.timescale
            }
        }
    }
    Canvas {
        id: keyframecanvas
        contextType: "2d"
        renderStrategy: Canvas.Threaded
        property int offset: keyframeContainer.scrollStart < 0 || parent.width <= keyframeContainer.containerWidth ? 0 : keyframeContainer.scrollStart
        property bool exitLoop: false
        anchors.left: parent.left
        anchors.leftMargin: offset
        anchors.right: parent.right
        //width: keyframeContainer.kfrCount > 0 ? Math.min(keyframeContainer.width, keyframeContainer.containerWidth) : 0
        height: keyframeContainer.kfrCount > 0 ? parent.height : 0
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
            if (keyframeContainer.kfrCount < 1) {
                console.log('KEYFRAME COUNT ERROR: ', keyframeContainer.kfrCount)
                return
            }
            console.log('PAINTING KEYFRMS: ', keyframeContainer.kfrCount, ' / KFRS: ', keyframes.count)
            var ctx = getContext("2d");
            ctx.beginPath()
            ctx.fillStyle = Qt.rgba(0,0,0.8, 0.5);
            paths = []

            var firstKeyframe = keyframes.itemAt(0) as KeyframeDelegate
            var xpos = firstKeyframe.tmpPos - keyframecanvas.offset
            var ypos = firstKeyframe.tmpVal
            // Add first curve point
            paths.push(compline.createObject(keyframecanvas, {"x": xpos, "y": ypos} ))
            exitLoop = false
            for(var i = 1; i < keyframes.count; i++)
            {
                var previousKeyframe = keyframes.itemAt(i - 1) as KeyframeDelegate
                var currentKeyframe = keyframes.itemAt(i) as KeyframeDelegate
                var nextKeyframe = keyframes.itemAt(i + 1) as KeyframeDelegate
                if (i + 1 < keyframes.count) {
                    if (nextKeyframe.tmpPos < keyframecanvas.offset) {
                        continue;
                    }
                }
                xpos = currentKeyframe.tmpPos - keyframecanvas.offset
                var alpha = 0.5
                var type = previousKeyframe.frameType
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
                        ypos = currentKeyframe.tmpVal
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
                                nextXOffset = (nextKeyframe.tmpPos - previousKeyframe.tmpPos) / 3
                                nextYOffset = (nextKeyframe.tmpVal - previousKeyframe.tmpVal) / 3
                            } else {
                                nextXOffset = (nextKeyframe.tmpPos - previousKeyframe.tmpPos) / 6
                                nextYOffset = (nextKeyframe.tmpVal - previousKeyframe.tmpVal) / 6
                            }
                        } else {
                            // Last point in the curve
                            nextXOffset = (xpos - (previousKeyframe.tmpPos - offset)) / 3
                            nextYOffset = (currentKeyframe.tmpVal - previousKeyframe.tmpVal) / 3
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
                            let beforePreviousKeframe = keyframes.itemAt(i - 2) as KeyframeDelegate
                            prevXOffset = (currentKeyframe.tmpPos - beforePreviousKeframe.tmpPos) / 6
                            prevYOffset = (currentKeyframe.tmpVal - beforePreviousKeframe.tmpVal) / 6
                        }
                        var prevxpos = previousKeyframe.tmpPos - keyframecanvas.offset + prevXOffset * alpha
                        var prevypos = previousKeyframe.tmpVal + prevYOffset * alpha
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
                        ypos = currentKeyframe.tmpVal
                        var prevX = previousKeyframe.tmpPos - keyframecanvas.offset
                        var prevY = previousKeyframe.tmpVal
                        paths.push(quad.createObject(keyframecanvas, {"x": xpos, "y": ypos, "controlX": prevX + (xpos - prevX) *  0.75, "controlY": prevY} ))
                        break;
                    case KeyframeType.CubicOut:
                        // Simulate cubic with Bezier curve, based on empiric testing
                        ypos = currentKeyframe.tmpVal
                        var prevX = previousKeyframe.tmpPos - keyframecanvas.offset
                        var prevY = previousKeyframe.tmpVal
                        paths.push(quad.createObject(keyframecanvas, {"x": xpos, "y": ypos, "controlX": prevX, "controlY": prevY + (ypos - prevY) *  0.75} ))
                        break;
                    case KeyframeType.ExponentialIn:
                        // Simulate exponential with Bezier curve, based on empiric testing
                        ypos = currentKeyframe.tmpVal
                        var prevX = previousKeyframe.tmpPos - keyframecanvas.offset
                        var prevY = previousKeyframe.tmpVal
                        paths.push(cubic.createObject(keyframecanvas, {"x": xpos, "y": ypos, "control1X": prevX + (xpos - prevX) *  1, "control1Y": prevY, "control2X": xpos, "control2Y": ypos - (ypos - prevY) *  0.5}))
                        break;
                    case KeyframeType.ExponentialOut:
                        // Simulate exponential with Bezier curve, based on empiric testing
                        ypos = currentKeyframe.tmpVal
                        var prevX = previousKeyframe.tmpPos - keyframecanvas.offset
                        var prevY = previousKeyframe.tmpVal
                        paths.push(cubic.createObject(keyframecanvas, {"x": xpos, "y": ypos, "control1X": prevX, "control1Y": prevY + (ypos - prevY) *  1, "control2X": xpos - (xpos - prevX) *  0.5, "control2Y": ypos}))
                        break;
                    case KeyframeType.CircularIn:
                        // Simulate circular with Bezier curve
                        ypos = currentKeyframe.tmpVal
                        var prevX = previousKeyframe.tmpPos - keyframecanvas.offset
                        var prevY = previousKeyframe.tmpVal
                        paths.push(cubic.createObject(keyframecanvas, {"x": xpos, "y": ypos, "control1X": prevX + (xpos - prevX) *  0.5522, "control1Y": prevY, "control2X": xpos, "control2Y": ypos - (ypos - prevY) *  0.5522} ))
                        break;
                    case KeyframeType.CircularOut:
                        // Simulate circular with Bezier curve
                        ypos = currentKeyframe.tmpVal
                        var prevX = previousKeyframe.tmpPos - keyframecanvas.offset
                        var prevY = previousKeyframe.tmpVal
                        paths.push(cubic.createObject(keyframecanvas, {"x": xpos, "y": ypos, "control1X": prevX, "control1Y": prevY + (ypos - prevY) *  0.5522, "control2X": xpos - (xpos - prevX) *  0.5522, "control2Y": ypos} ))
                        break;
                    case KeyframeType.BounceIn:
                        // Simulate bounce with Bezier curve, based on empiric testing
                        // Add 3 control points based on i-1
                        ypos = currentKeyframe.tmpVal
                        var prevX = previousKeyframe.tmpPos - keyframecanvas.offset
                        var prevY = previousKeyframe.tmpVal
                        var step = (xpos - prevX) / 11.
                        var delta = currentKeyframe.tmpVal - prevY
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
                        ypos = currentKeyframe.tmpVal
                        var prevX = previousKeyframe.tmpPos - keyframecanvas.offset
                        var prevY = previousKeyframe.tmpVal
                        var step = (xpos - prevX) / 11.
                        var delta = prevY - currentKeyframe.tmpVal
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
                        ypos = currentKeyframe.tmpVal
                        var prevX = previousKeyframe.tmpPos - keyframecanvas.offset
                        var prevY = previousKeyframe.tmpVal
                        var step = xpos - prevX
                        var delta = currentKeyframe.tmpVal - prevY
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
                        ypos = previousKeyframe.tmpVal
                        var prevX = previousKeyframe.tmpPos - keyframecanvas.offset
                        var prevY = previousKeyframe.tmpVal
                        var step = xpos - prevX
                        var delta = previousKeyframe.tmpVal - prevY

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
                        ypos = currentKeyframe.tmpVal
                        paths.push(compline.createObject(keyframecanvas, {"x": xpos, "y": ypos} ))
                        break;
                }
                if (xpos > keyframeContainer.containerWidth) {
                    if (exitLoop) {
                        break;
                    }
                    exitLoop = true
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
