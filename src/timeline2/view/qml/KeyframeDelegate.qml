/*
 *  SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
 *  SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick 2.15
import QtQuick.Controls 2.15

import org.kde.ki18n

import org.kde.kdenlive as K

Rectangle {
    id: keyframe
    visible: K.Core.activeTool === K.ToolType.SelectTool
    required property var model
    required property int index
    property int frame : model.frame
    property int frameType : model.type
    property string realValue: model.value
    x: (model.frame - keyframeContainer.inPoint) * timeScale
    height: parent.height
    property int value: parent.height * model.normalizedValue
    property int tmpVal : keyframeVal.y + K.UiUtils.baseSizeMedium / 2
    property int tmpPos : x + keyframeVal.x + K.UiUtils.baseSizeMedium / 2
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
        anchors.leftMargin: - K.UiUtils.baseSizeMedium / 3
        anchors.rightMargin: - K.UiUtils.baseSizeMedium / 3
        hoverEnabled: !root.isPanning
        cursorShape: Qt.SizeHorCursor
        enabled: !root.isPanning && parent.x > K.UiUtils.baseSizeMedium / 2 && parent.x < keyframeContainer.width - K.UiUtils.baseSizeMedium / 2
        drag.target: parent
        drag.smoothed: false
        drag.axis: Drag.XAxis
        onReleased: mouse => {
            root.autoScrolling = timeline.autoScroll
            keyframe.dragPos = -1
            var newPos = Math.round(parent.x / timeScale) + keyframeContainer.inPoint
            if (keyframe.frame != keyframeContainer.inPoint && newPos != keyframe.frame) {
                if (mouse.modifiers & Qt.ShiftModifier) {
                    // offset all subsequent keyframes
                    // TODO: rewrite using timeline to ensure all kf parameters are updated
                    timeline.offsetKeyframes(clipId, keyframe.frame, newPos)
                } else {
                    timeline.updateEffectKeyframe(clipId, keyframe.frame, newPos)
                }
            }
        }
        onPositionChanged: mouse => {
            if (mouse.buttons === Qt.LeftButton) {
                if (keyframe.frame == keyframeContainer.inPoint) {
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
                    parent.x = keyframe.dragPos * timeScale
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
        x: - K.UiUtils.baseSizeMedium / 2
        y: keyframeContainer.height - keyframe.value - K.UiUtils.baseSizeMedium / 2
        width: K.UiUtils.baseSizeMedium
        height: width
        radius: width / 2
        color: keyframe.model.active ? 'red' : keyframe.model.selected ? 'orange' : (kf1MouseArea.containsMouse || kf1MouseArea.pressed) ? root.textColor : root.videoColor
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
            property string movingVal: kfrModel.realValue(keyframe.model.normalizedValue)
            property double newVal: NaN
            property bool shiftPressed: false
            onPressed: mouse => {
                drag.axis = keyframe.model.moveOnly ? Drag.XAxis : (mouse.modifiers & Qt.ShiftModifier) ? Drag.YAxis : Drag.XAndYAxis
            }
            onClicked: mouse => {
                keyframeContainer.focus = true
                if (mouse.modifiers & Qt.ControlModifier && keyframe.model.selected) {
                    kfrModel.setActiveKeyframe(-1)
                    keyframeContainer.activeIndex = -1
                    kfrModel.setSelectedKeyframe(keyframe.index, true)
                } else {
                    kfrModel.setActiveKeyframe(keyframe.index)
                    keyframeContainer.activeIndex = keyframe.index
                    kfrModel.setSelectedKeyframe(keyframe.index, mouse.modifiers & Qt.ControlModifier)
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
                var newPos = frame == keyframeContainer.inPoint ? keyframeContainer.inPoint : Math.round((keyframe.x + parent.x + K.UiUtils.baseSizeMedium / 2) / timeScale) + keyframeContainer.inPoint
                if (newPos === frame && keyframe.value == keyframe.height - parent.y - K.UiUtils.baseSizeMedium / 2) {
                    var pos = keyframeContainer.modelStart + frame - keyframeContainer.inPoint
                    if (proxy.position != pos) {
                        seek(pos)
                    }
                    return
                }
                if (newVal > 1.5 || newVal < -0.5) {
                    if (keyframe.frame != keyframeContainer.inPoint) {
                        keyframeContainer.resetSelection()
                        timeline.removeEffectKeyframe(clipId, keyframe.frame);
                    } else {
                        if (newVal < 0) {
                            newVal = 0;
                        } else if (newVal > 1) {
                            newVal = 1;
                        }
                        timeline.updateEffectKeyframe(clipId, keyframe.frame, keyframe.frame, newVal)
                    }
                } else {
                    if (newVal < 0) {
                        newVal = 0;
                    } else if (newVal > 1) {
                        newVal = 1;
                    }
                    if (keyframe.model.moveOnly) {
                        timeline.updateEffectKeyframe(clipId, keyframe.frame, newPos)
                    } else {
                        timeline.updateEffectKeyframe(clipId, keyframe.frame, keyframe.frame == keyframeContainer.inPoint ? frame : newPos, newVal)
                    }
                }
            }
            onPositionChanged: mouse => {
                shiftPressed = (mouse.modifiers & Qt.ShiftModifier)
                if (mouse.buttons === Qt.LeftButton) {
                    if (keyframe.frame == keyframeContainer.inPoint) {
                        parent.x = - K.UiUtils.baseSizeMedium / 2
                    } else {
                        var newPos = Math.min(Math.round((parent.x + K.UiUtils.baseSizeMedium / 2) / timeScale), Math.round(keyframeContainer.width / timeScale) - keyframe.frame + keyframeContainer.inPoint - 1)
                        if (keyframe.frame + newPos <= keyframeContainer.inPoint) {
                            newPos = keyframeContainer.inPoint + 1 - keyframe.frame
                        }
                        if (newPos != keyframe.dragPos && (newPos == 0 || !timeline.hasKeyframeAt(clipId, keyframe.frame + newPos))) {
                            keyframe.dragPos = newPos
                            parent.x = newPos * timeScale - K.UiUtils.baseSizeMedium / 2
                            keyframecanvas.requestPaint()
                        } else {
                            parent.x = keyframe.dragPos * timeScale - K.UiUtils.baseSizeMedium / 2
                        }
                    }
                    keyframecanvas.requestPaint()
                    newVal = (keyframeContainer.height - (parent.y + mouse.y)) / keyframeContainer.height
                    movingVal = kfrModel.realValue(Math.min(Math.max(newVal, 0), 1))
                }
            }
            onDoubleClicked: {
                keyframeContainer.resetSelection()
                timeline.removeEffectKeyframe(clipId, keyframe.frame);
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
