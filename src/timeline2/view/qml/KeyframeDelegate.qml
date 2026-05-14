/*
 *  SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
 *  SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick 2.15
import QtQuick.Controls 2.15

import org.kde.ki18n

import org.kde.kdenlive as Kdenlive

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
            timeline.showKeyBinding(KI18n.i18n("<b>Drag</b> to move selected keyframes position. <b>Shift drag</b> to move all keyframes after this one."))
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
                timeline.showKeyBinding(KI18n.i18n("<b>Shift drag</b> to change value of selected keyframes, <b>Ctrl click</b> for multiple keyframe selection."))
            }
            onExited: {
                timeline.showKeyBinding()
            }
            ToolTip.visible: (containsMouse || pressed) && movingVal != ""
            ToolTip.text: movingVal
        }
    }
}
